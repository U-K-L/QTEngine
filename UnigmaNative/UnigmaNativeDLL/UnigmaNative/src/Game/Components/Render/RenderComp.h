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

	float blend = 0.0225f;
};
#pragma once
