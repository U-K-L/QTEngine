#include "UnigmaPhysicsComp.h"

UnigmaPhysicsComp::UnigmaPhysicsComp()
{
}

UnigmaPhysicsComp::~UnigmaPhysicsComp()
{
}

void UnigmaPhysicsComp::InitializeData(nlohmann::json& componentData)
{
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

	if (componentData.contains("UseGravity"))
	{
		useGravity = componentData["UseGravity"] == true;
	}
}

void UnigmaPhysicsComp::Start()
{
}

void UnigmaPhysicsComp::Init()
{
}

void UnigmaPhysicsComp::Update()
{
}
