#pragma once
#include <iostream>
#include <vector>
#include "../Core/UnigmaTransform.h"

#define MAX_NUM_COMPONENTS 32
struct UnigmaGameObject
{
	UnigmaTransform transform;
    char _pad[4]; // Padding to align the next member on a 4-byte boundary
	char name[32];
	uint32_t ID;
	uint32_t RenderID;
    uint32_t JID; //Idea for data structure.
	bool isActive;
	bool isCreated;
    char _pad2[6];
	uint16_t components[MAX_NUM_COMPONENTS];

    // Default constructor
    UnigmaGameObject()
        : ID(0), RenderID(0), isActive(false), isCreated(false)
    {
        // Initialize 'name' to an empty string
        memset(name, 0, sizeof(name));

        // Initialize 'components' array to zero
        //memset(components, 0, sizeof(components));
    }

    // print name.
    void PrintName()
	{
        //char array print.
        std::cout << "Name: " << name << std::endl;
	}
};


struct UnigmaGameObjectDummy
{
    UnigmaTransform transform;
    //char _pad[4];
};