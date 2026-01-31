#pragma once
#include <iostream>
#include <vector>
#include "../Core/UnigmaTransform.h"
#include <cstdint>

#define MAX_NUM_COMPONENTS 32

struct FixedString { char fstr[8]; };

enum class ValueType : uint8_t { INT32, FLOAT32, BOOL, CHAR, FIXEDSTRING };

struct Value
{
    ValueType type;
    union {
        int32_t  i32;
        float    f32;
        uint8_t  b;   // store bool as byte for ABI stability
        char     c;
        FixedString fstr;
    } data;
};


typedef Value(*FnGetComponentAttribute)(uint32_t GID, const char* componentName, const char* componentAttr);
extern FnGetComponentAttribute UNGetComponentAttribute;

template <typename T>
Value GetComponentAttribute(uint32_t ID, const char* componentName, const char* componentAttr)
{

    Value v = UNGetComponentAttribute(ID, componentName, componentAttr);

    std::cout << "Value value: " << v.data.f32 << std::endl;

    return v;
}

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
        using U = std::remove_cv_t<std::remove_reference_t<T>>;

        //Compile time if brnach.
        if constexpr (std::is_same_v<U, int32_t>) return ValueType::INT32;
        else if constexpr (std::is_same_v<U, float>) return ValueType::FLOAT32;
        else if constexpr (std::is_same_v<U, bool>) return ValueType::BOOL;
        else if constexpr (std::is_same_v<U, char>) return ValueType::CHAR;
        else if constexpr (std::is_same_v<U, std::string>) return ValueType::FIXEDSTRING;
        else
            static_assert(!sizeof(T), "Unsupported type in TypeToValueType");
    }

    template <typename T>
    bool TypeMatches(const Value& v)
    {
        return v.type == TypeToValueType<T>();
    }

    template <typename T>
    T GetComponentAttr(const char* componentName, const char* componentAttr)
    {
        Value val;
        val = GetComponentAttribute<T>(ID, componentName, componentAttr);

        if (!TypeMatches<T>(val)) {
            throw std::runtime_error("Type mismatch");
        }

        switch (val.type)
        {
        case ValueType::INT32:
            return static_cast<T>(val.data.i32);

        case ValueType::FLOAT32:
            return static_cast<T>(val.data.f32);

        case ValueType::BOOL:
            return static_cast<T>(val.data.b);

        case ValueType::CHAR:
            return static_cast<T>(val.data.c);

        case ValueType::FIXEDSTRING:
        {
            auto begin = val.data.fstr.fstr;
            auto end = std::find(begin, begin + 8, '\0');
            std::string s(begin, end);

            if constexpr (std::is_same_v<T, std::string>)
                return reinterpret_cast<T>(s);
        }

        default:
            throw std::runtime_error("Unknown ValueType");
        }
    }
};

typedef UnigmaGameObject* (*FnGetGameObject)(uint32_t ID);
extern FnGetGameObject UNGetGameObject;


struct UnigmaGameObjectDummy
{
    UnigmaTransform transform;
    //char _pad[4];
};
