#pragma once
#include "RenderPassObject.h"
class BackgroundPass : public RenderPassObject
{
public:
    // Constructor
    BackgroundPass();

    // Destructor
    ~BackgroundPass();

    void CreateMaterials() override;
};