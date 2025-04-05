#include "UnigmaPhysicsManager.h"

PxDefaultAllocator gPhysicsAllocator;
PxDefaultErrorCallback gPhysicsErrorCallback;
PxFoundation* gPhysicsFoundation = nullptr;
PxPhysics* gPhysics = nullptr;
PxDefaultCpuDispatcher* gPhysicsDispatcher = nullptr;
PxPvd* gPhysicsPvd = nullptr;
PxReal gPhysicsStackZ = 10.0f;

void PhysicsInitialize()
{
	std::cout << "Initializing Physics" << std::endl;
	gPhysicsFoundation = PxCreateFoundation(PX_PHYSICS_VERSION, gPhysicsAllocator, gPhysicsErrorCallback);
	if (!gPhysicsFoundation)
	{
		std::cerr << "PxCreateFoundation failed!" << std::endl;
		return;
	}

	gPhysics = PxCreatePhysics(PX_PHYSICS_VERSION, *gPhysicsFoundation, PxTolerancesScale(), true);

	if (!gPhysics)
	{
		std::cerr << "PxCreatePhysics failed!" << std::endl;
		return;
	}

	gPhysicsDispatcher = PxDefaultCpuDispatcherCreate(0);

	if (!gPhysicsDispatcher)
	{
		std::cerr << "PxDefaultCpuDispatcherCreate failed!" << std::endl;
		return;
	}
}