#pragma once
#include <iostream>
#include <vector>
#include "../Core/UnigmaTransform.h"
#include <unordered_map>
#include <string>


#define MAX_NUM_COMPONENTS 32
#pragma pack(push, 1) // Sets 1-byte alignment
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
        : ID(0), RenderID(0), isActive(false), isCreated(false), JID(-1)
    {
        // Initialize 'name' to an empty string
        memset(name, 0, sizeof(name));

        // Initialize 'components' array to zero
        //memset(components, 0, sizeof(components));
    }

    //set name.
    void SetName(const char* newName)
	{
		strcpy(name, newName);
	}
};
#pragma pack(pop) // Restores the previous alignment

class UnigmaGameObjectClass
{
    public:
        UnigmaGameObjectClass()
        {
            components = std::unordered_map<std::string, uint16_t>();
		}
        ~UnigmaGameObjectClass()
        {
		}

        void AddComponent(std::string name, uint16_t ID)
		{
			components[name] = ID;
        }
        /*
        template<typename T>
        T* GetComponent()
        {

            if (components.contains(typeid(T).name()))
            {
                auto index = components[typeid(T).name()];

                Component* comp = UnigmaGameManager::instance->Components[index];

                return reinterpret_cast<T*>(it->second);
            }
			return nullptr;
        }
        */
        void RemoveComponent(std::string name)
        {
            components.erase(name);
        }
        void ClearComponents()
		{
			components.clear();
		}

        UnigmaGameObject* gameObject;
        std::unordered_map<std::string, uint16_t> components;
};
