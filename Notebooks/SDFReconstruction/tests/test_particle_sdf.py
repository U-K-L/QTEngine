"""Focused scientific/behavioral checks; no long research training run."""
import json
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
import numpy as np
import torch
from particle_sdf import (
    Transform2D, SDFShape, ParticleSet, ParticleSystem, SamplingConfig, ShapeObject,
    ExampleGenerator, TrainingExample, ParticleSDF, Trainer, box, capsule, circle,
    polygon, primitive_shapes, collate_examples, predict_local, predict_object,
    evaluate_resolutions, seed_everything,
)


class GeometryAndSamplingTests(unittest.TestCase):
    def test_world_transform_roundtrip_and_distance_scale(self):
        transform = Transform2D((3, -2), 0.7, 2.5)
        points = np.array([[0, 0], [1, 0], [2, 0], [-0.3, 1.2]])
        np.testing.assert_allclose(transform.to_local(transform.to_world(points)), points, atol=3e-7)
        obj = ShapeObject("circle", circle(1), transform)
        np.testing.assert_allclose(obj.world_distance(transform.to_world(points[:3])), [-2.5, 0, 2.5], atol=1e-6)
        with self.assertRaises(ValueError):
            Transform2D(scale=(2, 1))

    def test_concave_polygon_has_correct_sign_and_edge_distance(self):
        shape = polygon([(0, 0), (2, 0), (2, 1), (1, 1), (1, 2), (0, 2)])
        np.testing.assert_allclose(shape.distance([[1.5, 1.5], [0.5, 1.5], [0, 1]]), [0.5, -0.5, 0])
        reversed_shape = polygon([(0, 2), (1, 2), (1, 1), (2, 1), (2, 0), (0, 0)])
        np.testing.assert_allclose(shape.distance([[1.5, 1.5], [0.5, 1.5]]),
                                   reversed_shape.distance([[1.5, 1.5], [0.5, 1.5]]))

    def test_all_primitives_sample_inside_with_finite_distance_labels(self):
        system = ParticleSystem()
        for shape in primitive_shapes():
            with self.subTest(shape=shape.name):
                particles = system.sample(shape, SamplingConfig((21, 17)))
                self.assertGreater(len(particles), 0)
                self.assertTrue(np.all(shape.distance(particles.positions) <= 0))
        np.testing.assert_allclose(capsule((0, 0), (0, 0), 0.5).distance([[0, 0], [1, 0]]), [-0.5, 0.5])

    def test_resolution_spacing_and_independent_objects(self):
        system = ParticleSystem()
        shape = box((1, 1))
        a = system.create_object(shape, sampling=SamplingConfig((9, 5)))
        b = system.create_object(shape, sampling=SamplingConfig(7))
        original_b = b.particles.positions.copy()
        self.assertEqual(len(a.particles), 45)
        system.resample(a, SamplingConfig(spacing=0.5))
        self.assertEqual(len(a.particles), 25)
        np.testing.assert_array_equal(b.particles.positions, original_b)
        system.resample(a, SamplingConfig(13))
        self.assertEqual(len(a.particles), 169)

    def test_manual_world_positions_and_integration(self):
        system = ParticleSystem()
        obj = system.create_object(circle(), transform=Transform2D((1, 2), np.pi / 2, 2))
        world = np.array([[1.0, 2.0], [1.0, 3.0]])
        velocity = np.array([[0.2, 0.0], [0.2, 0.0]])
        system.set_positions(obj, world, world_space=True, velocities=velocity)
        np.testing.assert_allclose(obj.world_particles, world, atol=1e-6)
        system.step(0.5)
        np.testing.assert_allclose(obj.world_particles, world + 0.5 * velocity, atol=1e-6)

    def test_empty_particles_and_invalid_inputs_report_errors(self):
        system = ParticleSystem()
        obj = system.create_object(circle(), sampling=SamplingConfig(2))
        self.assertEqual(len(obj.particles), 0)
        with self.assertRaisesRegex(ValueError, "at least one particle"):
            ExampleGenerator([obj]).sample()
        for resolution in [1, 3.5, (4, 1)]:
            with self.assertRaises(ValueError):
                SamplingConfig(resolution)
        with self.assertRaises(ValueError):
            SamplingConfig(spacing=0)

    def test_examples_use_custom_shape_and_do_not_change_source_particles(self):
        ring = SDFShape("ring", lambda p: np.abs(np.linalg.norm(p, axis=-1) - 0.6) - 0.15,
                        ((-0.75, -0.75), (0.75, 0.75)))
        obj = ParticleSystem().create_object(ring, sampling=SamplingConfig(19))
        original = obj.particles.positions.copy()
        left = ExampleGenerator([obj], sampling_choices=[SamplingConfig(13, jitter=0.1), SamplingConfig(29)], seed=14)
        right = ExampleGenerator([obj], sampling_choices=[SamplingConfig(13, jitter=0.1), SamplingConfig(29)], seed=14)
        for _ in range(3):
            a, b = left.sample(), right.sample()
            np.testing.assert_array_equal(a.particle_positions, b.particle_positions)
            np.testing.assert_array_equal(a.query_points, b.query_points)
            np.testing.assert_allclose(a.target_distances[:, 0], ring.distance(a.query_points))
        np.testing.assert_array_equal(obj.particles.positions, original)
        manual = ExampleGenerator([obj], sampling_choices=None).sample()
        np.testing.assert_array_equal(manual.particle_positions, original)


class ModelTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        torch.set_num_threads(2)

    def test_padding_permutation_and_particle_sensitivity(self):
        seed_everything(5)
        model = ParticleSDF()
        system = ParticleSystem()
        a = ExampleGenerator([system.create_object(circle(), sampling=SamplingConfig(5))], query_count=17).sample()
        b = ExampleGenerator([system.create_object(box(), sampling=SamplingConfig(16))], query_count=31).sample()
        p, q, target, mask, qmask = collate_examples([a, b])
        with torch.no_grad():
            batched = model(p, q, mask)
            isolated = model(torch.tensor(a.particle_positions)[None], torch.tensor(a.query_points)[None])
            torch.testing.assert_close(batched[0, :17], isolated[0], atol=1e-6, rtol=1e-5)
            p[~mask] = 9999  # Padding content must have no influence.
            torch.testing.assert_close(model(p, q, mask), batched)
            reversed_result = model(torch.tensor(a.particle_positions[::-1].copy())[None], torch.tensor(a.query_points)[None])
            torch.testing.assert_close(reversed_result, isolated)
        collapsed = np.zeros_like(a.particle_positions)
        self.assertGreater(np.abs(predict_local(model, collapsed, a.query_points) - isolated[0, :, 0].detach().numpy()).max(), 1e-5)
        self.assertEqual(qmask.sum().item(), 48)

    def test_transform_inference_scales_predictions_without_retraining(self):
        seed_everything(6)
        obj = ParticleSystem().create_object(box(), transform=Transform2D((-2, 3), 1.1, 1.7))
        model = ParticleSDF()
        query = np.array([[0.1, 0.2], [0.7, -0.3]], dtype=np.float32)
        local = predict_local(model, obj.particles, query, chunk_size=1)
        world = predict_object(model, obj, obj.transform.to_world(query))
        np.testing.assert_allclose(world, local * 1.7, atol=2e-7)

    def test_training_improves_fresh_queries_with_mixed_particle_counts(self):
        seed_everything(10)
        system = ParticleSystem()
        objects = [system.create_object(circle(0.7)), system.create_object(box((0.8, 0.4)))]
        examples = ExampleGenerator(objects, sampling_choices=[SamplingConfig(9), SamplingConfig(17), SamplingConfig(25)], query_count=128, seed=10)
        validation = [ExampleGenerator([o], query_count=256, seed=100 + i).sample() for i, o in enumerate(objects)]
        model = ParticleSDF()
        def error():
            return np.mean([np.mean((predict_local(model, e.particle_positions, e.query_points) - e.target_distances[:, 0])**2) for e in validation])
        initial = error()
        trainer = Trainer(model, device="cpu")
        trainer.fit(examples, steps=160, batch_size=2, log_every=0)
        final = error()
        self.assertLess(final, initial * 0.3, (initial, final))
        self.assertTrue(np.isfinite(trainer.history).all())
        trainer.fit(lambda: validation[0], steps=1, batch_size=1, log_every=0)
        self.assertEqual(len(trainer.history), 161)
        rows = evaluate_resolutions(model, objects[0], [9, 16, 27], query_count=128)
        self.assertEqual(len(rows), 3)
        self.assertTrue(all(np.isfinite(row["mae"]) for row in rows))

    def test_notebook_components_match_library_and_cells_compile(self):
        notebook = json.loads((ROOT / "3ParticleSDF.ipynb").read_text(encoding="utf-8"))
        sections = [(part.split("\n", 1)[1].strip() + "\n")
                    for part in (ROOT / "particle_sdf.py").read_text(encoding="utf-8").split("# %% ")[1:]]
        sources = ["".join(cell["source"]) for cell in notebook["cells"] if cell["cell_type"] == "code"]
        for section in sections:
            self.assertIn(section, sources)
        for i, source in enumerate(sources):
            compile(source, f"notebook-cell-{i}", "exec")


if __name__ == "__main__":
    unittest.main()
