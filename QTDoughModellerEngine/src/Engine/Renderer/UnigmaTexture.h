#pragma once
#include <string>

struct UnigmaTexture {
	const uint32_t WIDTH;
	const uint32_t HEIGHT;
	std::string TEXTURE_PATH;

    // Constructor to initialize const members
    UnigmaTexture(uint32_t width, uint32_t height, const std::string& texturePath)
        : WIDTH(width), HEIGHT(height), TEXTURE_PATH(texturePath) {}

    UnigmaTexture() : WIDTH(0), HEIGHT(0), TEXTURE_PATH("") {}

    // Overload the assignment operator
    UnigmaTexture& operator=(const UnigmaTexture& other) {
        if (this != &other) { 
            TEXTURE_PATH = other.TEXTURE_PATH;
        }
        return *this;
    }
};