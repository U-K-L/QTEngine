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
    for (auto key : componentData)
    {
        //std::string type = componentData[key].get<std::string>();

        //std::cout << key << std::endl;
        //std::cout << type << std::endl;

    }


    if (componentData.contains("Blend"))
    {
        blend = componentData["Blend"];
        //componentAttributes["Blend"] = blend;
            //Store component data.
        Value v;
        v.type = ValueType::FLOAT32;
        v.data.f32 = 100;
        componentAttributes["Blend"] = v;
    }


}