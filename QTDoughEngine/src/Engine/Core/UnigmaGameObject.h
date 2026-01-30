#pragma once
#include <iostream>
#include <vector>
#include "../Core/UnigmaTransform.h"

#define MAX_NUM_COMPONENTS 32

struct FixedString
{
    char fstr[8];
};

enum class ValueType : uint8_t
{
    INT32,
    FLOAT32,
    BOOL,
    CHAR,
    FIXEDSTRING
};

struct Value
{
    ValueType type; //Tag for what type of data, how to interpret.
    union {

        int i32;
        float f32;
        bool b;
        char c;
        FixedString fstr;
    } data; //The actual data itself.
};


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

    template <typename T>
    constexpr ValueType TypeToValueType()
    {
        //Compile time if brnach.
        if constexpr (std::is_same_v<T, int32_t>) return ValueType::INT32;
    }

    template <typename T>
    bool TypeMatches(const Value& v)
    {
        return v.type == TypeToValueType<T>();
    }

    template <typename T>
    T GetComponentAttr(char* componentName, char* componentAttr)
    {
        //Get the hash of the name so it can cross DLL boundaries.
        size_t hashNameSizeT = std::hash<std::string_view>{}(componentName);
        uint64_t componentHash = static_cast<uint64_t>(hashNameSizeT);

        size_t hashAttrSizeT = std::hash<std::string_view>{}(componentAttr);
        uint64_t componentAttrHash = static_cast<uint64_t>(hashAttrSizeT);

        std::cout << "Getting Component Attr: " << componentHash << " for GameObject ID: " << ID << std::endl;
        std::cout << "Attribute Hash: " << componentAttrHash << std::endl;

        //Assume we call some function from dll: GetComponentAttribute(uint32_t gameObjectID, uint64_t componentHash, uint64_t componentAttrHash)
        //And that function returned a Value struct.
        Value val;
        //val = getComponentAttribute(ID, componentHash, componentAttrHash);
        val = Value{}; // Dummy initialization
        val.type = ValueType::FLOAT32;
        val.data.i32 = 42; // Dummy value

        if(TypeMatches<int>(val))
            std::cout << "Type Matches!" << std::endl;

        //Check if val matches type T.


        return T{};
    }
};


struct UnigmaGameObjectDummy
{
    UnigmaTransform transform;
    //char _pad[4];
};
