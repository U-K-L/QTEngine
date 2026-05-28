#pragma once
#include "../Component.h"

class UnigmaPhysicsComp : public Component
{
public:
	UnigmaPhysicsComp();
	~UnigmaPhysicsComp();

	void InitializeData(nlohmann::json& componentData) override;
	void Update() override;
	void Start() override;
	void Init();

	static constexpr const char* TypeName = "UnigmaPhysicsComp";
	enum BodyType
	{
		Dynamic,
		Static,
		Kinematic,
		Trigger,
		Soft
	};

	enum GeometryType
	{
		Box,
		Sphere,
		Capsule,
		Cylinder,
		Plane,
		TriangleMesh,
		Terrain,
		ConvexMesh
	};

	GeometryType geometryType;
	BodyType bodyType;

	//Varibles for the physics component
	glm::vec3 bounds;
	float staticFriction = 0.5f;
	float dynamicFriction = 0.5f;
	float restitution = 0.6f;
	float denisty = 1.0f;
	bool useGravity = true;
};
