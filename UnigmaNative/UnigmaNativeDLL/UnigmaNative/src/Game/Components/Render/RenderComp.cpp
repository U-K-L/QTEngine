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
        std::cout << "Game Object ID: " << GID << "Blend value: " << blend << std::endl;
        SetValue<float>("Blend", "float", blend);
    }


}