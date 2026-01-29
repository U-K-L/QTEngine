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
        std::cout << key << std::endl;
    }
}