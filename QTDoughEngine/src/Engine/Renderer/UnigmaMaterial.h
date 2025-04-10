#pragma once
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include "UnigmaTexture.h"
#include "UnigmaShader.h"
#include <vector>
#include <unordered_map>
#include <iostream>

#define MAX_NUM_TEXTURES 16

struct UnigmaMaterial
{
    std::vector<UnigmaTexture> textures;
    std::unordered_map<std::string, glm::vec4> vectorProperties;
    UnigmaShader shader;

    uint32_t textureIDs[MAX_NUM_TEXTURES];

    std::string textureNames[MAX_NUM_TEXTURES];

    UnigmaMaterial() {}

    UnigmaMaterial& operator=(const UnigmaMaterial& other) {
        if (this != &other) {
            textures.clear();
            for (int i = 0; i < other.textures.size(); ++i) {
                textures.push_back(other.textures[i]);
            }

            //set texture IDs
            for (int i = 0; i < MAX_NUM_TEXTURES; i++)
			{
				textureIDs[i] = other.textureIDs[i];
				textureNames[i] = other.textureNames[i];
			}

            //names
            for (auto& [key, value] : other.vectorProperties) {
				vectorProperties[key] = value;
			}

            shader = other.shader;


            //vector properties
            for (auto& [key, value] : other.vectorProperties) {
                vectorProperties[key] = value;
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
    }

    void Clean()
    {
        textures.clear();

        for (int i = 0; i < MAX_NUM_TEXTURES; i++)
        {
            textureIDs[i] = 0;
            textureNames[i] = "Default";
        }
    }
};