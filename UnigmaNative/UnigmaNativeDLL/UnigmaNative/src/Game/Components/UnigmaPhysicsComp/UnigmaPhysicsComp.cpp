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

void UnigmaPhysicsComp::InitializeData(nlohmann::json& componentData)
{
	std::cout << "Initializing Physics Component" << std::endl;
	UnigmaGameObject* gobj = &GameObjects[GID];
	// Initialize the component data from JSON
	if (componentData.contains("GeometryType"))
	{
		std::string geometryTypeStr = componentData["GeometryType"];
		if (geometryTypeStr == "Box")
			geometryType = Box;
		else if (geometryTypeStr == "Sphere")
			geometryType = Sphere;
		else if (geometryTypeStr == "Capsule")
			geometryType = Capsule;
		else if (geometryTypeStr == "Cylinder")
			geometryType = Cylinder;
		else if (geometryTypeStr == "Plane")
			geometryType = Plane;
		else if (geometryTypeStr == "TriangleMesh")
			geometryType = TriangleMesh;
		else if (geometryTypeStr == "Terrain")
			geometryType = Terrain;
		else if (geometryTypeStr == "ConvexMesh")
			geometryType = ConvexMesh;
	}

	if (componentData.contains("BodyType"))
	{
		std::string bodyTypeStr = componentData["BodyType"];
		if (bodyTypeStr == "Dynamic")
			bodyType = Dynamic;
		else if (bodyTypeStr == "Static")
			bodyType = Static;
		else if (bodyTypeStr == "Kinematic")
			bodyType = Kinematic;
		else if (bodyTypeStr == "Trigger")
			bodyType = Trigger;
		else if (bodyTypeStr == "Soft")
			bodyType = Soft;
	}

	if (componentData.contains("Bounds"))
	{
		bounds.x = componentData["Bounds"][0];
		bounds.y = componentData["Bounds"][1];
		bounds.z = componentData["Bounds"][2];
	}

	PxQuat qx(gobj->transform.rotation.x, PxVec3(1, 0, 0));
	PxQuat qy(gobj->transform.rotation.y, PxVec3(0, 1, 0));
	PxQuat qz(gobj->transform.rotation.z, PxVec3(0, 0, 1));
	PxQuat rot = qz * qy * qx;
	PxVec3 pos = PxVec3(gobj->transform.position.x, gobj->transform.position.y, gobj->transform.position.z);
	PxTransform t(pos, rot);
	transform = t;
	Init();

	UnigmaSceneManager* sceneManager = UnigmaSceneManager::instance;
	sceneManager->GetCurrentScene()->physicsScene->addActor(*actor);
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
	UnigmaGameObject* gobj = &GameObjects[GID];
	PxRigidDynamic* physicsCube = static_cast<PxRigidDynamic*>(actor);
	if (physicsCube)
	{
		PxTransform pose = physicsCube->getGlobalPose();
		gobj->transform.position.x = pose.p.x;
		gobj->transform.position.y = pose.p.y;
		gobj->transform.position.z = pose.p.z;

		glm::quat q = glm::quat(pose.q.w, pose.q.x, pose.q.y, pose.q.z); // (w, x, y, z)
		glm::vec3 euler = glm::eulerAngles(q);
		gobj->transform.rotation.x = euler.x;
		gobj->transform.rotation.y = euler.y;
		gobj->transform.rotation.z = euler.z;

		gobj->transform.UpdateTransform();

	}
}