# Particle-to-SDF experiments

Open **3ParticleSDF.ipynb** for the organized workbench. The original `2DeepSDF.ipynb`
and its Python export remain reference experiments.

The new notebook is self-contained for Colab. Locally, install `requirements.txt`
in a virtual environment and select that environment's Python as your notebook kernel.
CPU and CUDA execution are supported; the local verification environment uses CPU PyTorch.
Run cells in order. The default 500 training steps are a modest experiment, not a quality guarantee.

## Components

| Component | Responsibility |
|---|---|
| `SDFShape` | Local reference distance function and sampling bounds |
| `Transform2D` | Translation, rotation in radians, positive uniform scale |
| `ParticleSet` | Independent local positions and velocities |
| `ParticleSystem` | Object creation, lattice sampling, resampling, manual assignment, integration |
| `ShapeObject` | One shape, one transform, and its own particles |
| `ExampleGenerator` | Object selection, sampling settings, and labeled query generation |
| `ParticleSDF` | Shared particle encoder, masked max pooling, distance decoder |
| `Trainer` | Ragged batches, optimizer, training history |

`particle_sdf.py` is importable without running training. Its definitions are embedded
into the notebook by `build_notebook.py`. After changing the library, run:

```powershell
python build_notebook.py
```

This regenerates `3ParticleSDF.ipynb`, including example cells, and clears outputs.
Save exploratory changes to a copy before regenerating it. Edit `build_notebook.py`
to retain changes to the example/control cells in future generated versions.

## Control particle resolution independently

```python
from particle_sdf import *

system = ParticleSystem(seed=7)
obj = system.create_object(
    box((0.8, 0.3)),
    transform=Transform2D(position=(1, -0.2), rotation=0.4, scale=1.5),
    sampling=SamplingConfig(resolution=(40, 18)),
)
system.resample(obj, SamplingConfig(spacing=0.05))
system.set_positions(obj, obj.world_particles[::2], world_space=True)

examples = ExampleGenerator([obj], query_count=512, seed=7)  # Use existing particles.
trainer = Trainer(ParticleSDF())
trainer.fit(examples, steps=500, batch_size=4)
```

Resolution means lattice nodes per axis, before filtering to the shape's interior.
Any integer >= 2 or integer pair is accepted, subject to available memory. Explicit
spacing overrides resolution and uses local units. Sampling an empty interior is valid;
training/inference requires a nonempty particle set and gives a clear error otherwise.
Changing particle count never requires changing the model architecture.

To sample multiple resolutions during training:

```python
examples = ExampleGenerator(
    system.objects,
    sampling_choices=[SamplingConfig(r, jitter=0.1) for r in [12, 23, 36, 51]],
    query_count=1024,
    seed=7,
)
```

Choose `sampling_choices=None` to use the exact particles already assigned to objects.
Generating examples does not resample or mutate the source objects.

## Custom shapes and ground truth

Use `polygon(vertices)` for simple convex or concave polygons with ordered boundary
vertices. Or supply `SDFShape(name, callable, bounds)` with a vectorized signed distance
function: negative inside, positive outside, output shape equal to the input shape without
its final coordinate axis. Bounds must enclose the object's interior. The built-in polygon
helper assumes a simple boundary without self-intersection.

For arbitrary externally labeled data, pass a function returning `TrainingExample`
to `Trainer.fit`. Its arrays are particle positions `(N, 2)`, queries `(Q, 2)`, and
matching signed distances `(Q,)` or `(Q, 1)` in the same local frame.

Particles alone do not define a unique correct surface. If a particle simulation changes
the object's geometry, update its reference shape or provide new distance labels.
The example `step` method integrates velocity; it is not the engine's material solver.

Transforms are handled analytically, including distance scaling. The model learns local
geometry, so moving/rotating/uniformly scaling an object does not require retraining.
Nonuniform scale is intentionally not accepted as an exact SDF transform; change the
shape's dimensions or polygon vertices instead.

## Research checks

- Training loss is not a held-out metric. Resolution sweeps use identical fresh query
  points to compare sampling changes. Metrics are reported in local distance units.
- Held-out geometry is demonstrated separately from resolution changes to training objects.
- Gaussian splats are labeled density, not signed distance.
- Max pooling is insensitive to duplicated points and does not encode particle mass/density.
- Distance MSE alone does not enforce an exact distance field or guarantee generalization.

Run the focused checks with `python -m unittest discover -s tests -v`. They cover
geometric labels, transforms, resolution changes, ragged batching, input sensitivity,
training on custom examples, and learning on independently sampled queries.
