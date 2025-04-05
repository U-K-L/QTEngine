#pragma once
#include <iostream>
#include <ctype.h>
#include "PxPhysicsAPI.h"

using namespace physx;

extern PxDefaultAllocator gPhysicsAllocator;
extern PxDefaultErrorCallback gPhysicsErrorCallback;
extern PxFoundation* gPhysicsFoundation;
extern PxPhysics* gPhysics;
extern PxDefaultCpuDispatcher* gPhysicsDispatcher;
extern PxPvd* gPhysicsPvd;
extern PxReal gPhysicsStackZ;

void PhysicsInitialize();
