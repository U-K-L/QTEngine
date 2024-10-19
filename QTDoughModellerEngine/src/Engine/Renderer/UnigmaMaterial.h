#pragma once
#include "UnigmaTexture.h"

#define MAX_NUM_TEXTURES 8

struct UnigmaMaterial
{
    UnigmaTexture textures[MAX_NUM_TEXTURES];

    UnigmaMaterial() {
        for (int i = 0; i < MAX_NUM_TEXTURES; ++i) {
            textures[i] = UnigmaTexture();
        }
    }

    UnigmaMaterial& operator=(const UnigmaMaterial& other) {
        if (this != &other) {
            for (int i = 0; i < MAX_NUM_TEXTURES; ++i) {
                textures[i] = other.textures[i];
            }
        }
        return *this;
    }
};