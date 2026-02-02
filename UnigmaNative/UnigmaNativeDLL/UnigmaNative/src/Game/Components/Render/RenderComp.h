#pragma once
#include "../Component.h"


class RenderComp : public Component
{
public:
	RenderComp();
	~RenderComp();

	void Update() override;
	void Start() override;
	void InitializeData(nlohmann::json& componentData) override;

	static constexpr const char* TypeName = "RenderComp";

	int primType = 0;
	int opeartion = 0;
	int resolution = 0;
	int density = 0;
	float blend = 0.0225f;
	float smoothness = 0.1f;

	enum ResolutionType
	{
		LowRes,
		MediumRes,
		HighRes
	};

	enum PrimitiveType
	{
		Mesh,
		Sphere,
	};

	enum ParticleDensity
	{
		LowDens,
		MediumDens,
		HighDens
	};
};
#pragma once
