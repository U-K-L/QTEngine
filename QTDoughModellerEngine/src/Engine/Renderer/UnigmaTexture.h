#pragma once
#include <string>
#include <vulkan/vulkan.h>
#include <SDL2/SDL.h>

#include <SDL2/SDL_vulkan.h>
#include <SDL2/SDL_system.h>
#include <SDL2/SDL_syswm.h>
struct UnigmaTexture {
	uint32_t WIDTH;
	uint32_t HEIGHT;
	std::string TEXTURE_PATH;
    VkImage u_image;
    VkDeviceMemory u_imageMemory;
    VkImageView u_imageView;
    VkSampler u_sampler; // Optional if samplers are separate

    // Constructor to initialize const members
    UnigmaTexture(uint32_t width, uint32_t height, std::string& texturePath)
        : WIDTH(width), HEIGHT(height), TEXTURE_PATH(texturePath) {}

    UnigmaTexture() : WIDTH(0), HEIGHT(0), TEXTURE_PATH(""), u_image(), u_imageMemory(), u_sampler()  {}

    UnigmaTexture(const std::string& texturePath) : WIDTH(0), HEIGHT(0), TEXTURE_PATH(texturePath), u_image(), u_imageMemory(), u_sampler(), u_imageView() {}

    // Overload the assignment operator
    UnigmaTexture& operator=(const UnigmaTexture& other) {
        if (this != &other) { 
            TEXTURE_PATH = other.TEXTURE_PATH;
        }
        return *this;
    }
};