#include "UnigmaPhysicsComp.h"
#include "../../../Physics/UnigmaPhysicsManager.h"


UnigmaPhysicsComp::UnigmaPhysicsComp()
{
}

UnigmaPhysicsComp::~UnigmaPhysicsComp()
{
	// Clean up the actor if it exists
	if (actor)
	{
		actor->release();
	}
}

void UnigmaPhysicsComp::Start()
{

}

void UnigmaPhysicsComp::Init()
{
	PxMaterial* material = gPhysics->createMaterial(staticFriction, dynamicFriction, restitution);

	switch (geometryType)
	{
	case Box:
		geometry = new PxBoxGeometry(bounds.x, bounds.y, bounds.z);
		break;
	case Sphere:
		geometry = new PxSphereGeometry(bounds.x);
		break;
	case Capsule:
		geometry = new PxCapsuleGeometry(bounds.x, bounds.y);
		break;
	case Cylinder:
		geometry = new PxCapsuleGeometry(bounds.x, bounds.y);
		break;
	case Plane:
		break;
	case TriangleMesh:
		break;
	case Terrain:
		break;
	case ConvexMesh:
		break;

	};

	PxShape* shape = gPhysics->createShape(*geometry, *material);

	switch (bodyType)
	{
	case Dynamic:
		actor = PxCreateDynamic(*gPhysics, transform, *geometry, *material, denisty);
		static_cast<PxRigidDynamic*>(actor)->attachShape(*shape);
		break;
	case Static:
		actor = gPhysics->createRigidStatic(transform);
		static_cast<PxRigidStatic*>(actor)->attachShape(*shape);
		break;
	case Kinematic:
		break;
	case Trigger:
		break;
	case Soft:
		break;

	}
}

void UnigmaPhysicsComp::Update()
{
	// Update logic for the physics component
}