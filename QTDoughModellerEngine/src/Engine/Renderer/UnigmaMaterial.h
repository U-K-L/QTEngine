#pragma once
#include "UnigmaTexture.h"
#include "UnigmaShader.h"
#include <vector>

#define MAX_NUM_TEXTURES 8

struct UnigmaMaterial
{
    std::vector<UnigmaTexture> textures;
    UnigmaShader shader;

    uint32_t textureIDs[MAX_NUM_TEXTURES];

    UnigmaMaterial() {}

    UnigmaMaterial& operator=(const UnigmaMaterial& other) {
        if (this != &other) {
            for (int i = 0; i < textures.size(); ++i) {
                textures[i] = other.textures[i];
            }
        }
        return *this;
    }
};