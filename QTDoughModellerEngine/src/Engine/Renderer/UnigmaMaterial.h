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

    std::string textureNames[MAX_NUM_TEXTURES];

    UnigmaMaterial() {}

    UnigmaMaterial& operator=(const UnigmaMaterial& other) {
        if (this != &other) {
            for (int i = 0; i < textures.size(); ++i) {
                textures[i] = other.textures[i];
            }
        }
        return *this;
    }

    void push_back_texture(UnigmaTexture texture, std::string path) {
        if (textures.size() >= MAX_NUM_TEXTURES - 1)
        {
            return;
        }
        textures.push_back(texture);
        textureNames[textures.size()] = path;
    }
};