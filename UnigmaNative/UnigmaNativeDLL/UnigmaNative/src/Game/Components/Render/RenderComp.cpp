#include "RenderComp.h"
//Add game manager
#include "../../UnigmaGameManager.h"

//SDL
#include <SDL.h>
#include "../../../Core/InputHander.h"

RenderComp::RenderComp()
{
}

RenderComp::~RenderComp()
{
}


void RenderComp::Update()
{
}


void RenderComp::Start()
{

}

void RenderComp::InitializeData(nlohmann::json& componentData)
{
    if (componentData.contains("Blend"))
    {
        blend = componentData["Blend"];
        SetValue<float>("Blend", "float", blend);
    }

    if (componentData.contains("Smoothness"))
    {
        smoothness = componentData["Smoothness"];
        SetValue<float>("Smoothness", "float", smoothness);
    }

    if (componentData.contains("Resolution"))
    {
        std::string resTypeStr = componentData["Resolution"];
        if (resTypeStr == "Low")
            resolution = Low;
        else if (resTypeStr == "Medium")
            resolution = Medium;
        else if (resTypeStr == "High")
            resolution = High;

        SetValue<int>("Resolution", "int", resolution);
    }


    if (componentData.contains("Type"))
    {
        std::string primTypeString = componentData["Type"];
        if (primTypeString == "Mesh")
            primType = Mesh;
        else if (primTypeString == "Sphere")
            primType = Sphere;

        SetValue<int>("Type", "int", primType);
    }

}