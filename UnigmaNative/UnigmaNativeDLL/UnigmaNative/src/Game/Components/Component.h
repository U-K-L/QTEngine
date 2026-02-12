#pragma once
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#include <iostream>
#include <string>
#include <vector>
#include <../glm//glm.hpp>
#include "../GlobalObjects.h"
#include "../json/json.hpp"
#include "../../Game/UnigmaSceneManager.h"
#include "../../Game/UnigmaGameObject.h"



class Component
{
	public:
	Component();
	~Component();

	template <typename T>
	Value TypeToValue(const char* type, T value);

	Value GetAttribute(const char* componentAttribute);

	template <typename T>
    void SetValue(const char* componentAttribute, const char* type, T value);

	virtual void InitializeData(nlohmann::json& componentData);
	virtual void Update();
	virtual void Start();

	static constexpr const char* TypeName = "ComponentNameComp";
	uint32_t GID; //ID of the gameObject this component is tied to.
	uint32_t CID; //ID of the component. For now this is set inside the code for each component, later we'll use a table.
	uint32_t GlobalIndexID; //Global index ID for the component.
	std::string Name;
	UnigmaGameObjectClass* gameObjectClass; //Pointer to the game object class this component is tied to.
	bool IsActive;
	bool IsCreated;
	bool IsStarted;

	//Attributes.
	std::unordered_map<std::string, Value> componentAttributes;
};

template <typename T>
Value Component::TypeToValue(const char* type, T value)
{
    Value v{};

    if (strcmp(type, "int") == 0 || strcmp(type, "int32") == 0)
    {
        v.type = ValueType::INT32;
        v.data.i32 = (int)value;
    }
    else if (strcmp(type, "float") == 0 || strcmp(type, "float32") == 0)
    {
        v.type = ValueType::FLOAT32;
        v.data.f32 = (float)value;
    }
    else if (strcmp(type, "bool") == 0)
    {
        v.type = ValueType::BOOL;
        v.data.b = (bool)value;
    }
    else if (strcmp(type, "char") == 0)
    {
        v.type = ValueType::CHAR;
        v.data.c = (char)value;
    }
    return v;
};

template <typename T>
void Component::SetValue(const char* componentAttribute, const char* type, T value)
{
    Value v = TypeToValue<T>(type, value);
    componentAttributes[std::string(componentAttribute)] = v;
}

