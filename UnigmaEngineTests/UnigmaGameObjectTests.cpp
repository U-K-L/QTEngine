#include "pch.h"
#include "UnigmaGameObjectTests.h"
#include "CppUnitTest.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
bool UnigmaGameObjectTests::TestTypeToMatch()
{
	UnigmaGameObject gObj = UnigmaGameObject{};
	Value v = Value{};

	v.type = ValueType::INT32;
	v.data.i32 = 0;

	if (!gObj.TypeMatches<int>(v))
	{
		Logger::WriteMessage("EXCEPTION: TYPE INT INVALID MATCH.");
		return false;
	}

	v.type = ValueType::FLOAT32;
	v.data.f32 = 0.0f;

	if (!gObj.TypeMatches<float>(v))
	{
		Logger::WriteMessage("EXCEPTION: TYPE FLOAT INVALID MATCH.");
		return false;
	}

	v.type = ValueType::BOOL;
	v.data.b = 1;

	if (!gObj.TypeMatches<bool>(v))
	{
		Logger::WriteMessage("EXCEPTION: TYPE BOOL INVALID MATCH.");
		return false;
	}

	v.type = ValueType::CHAR;
	v.data.c = 'A';

	if (!gObj.TypeMatches<char>(v))
	{
		Logger::WriteMessage("EXCEPTION: TYPE CHAR INVALID MATCH.");
		return false;
	}

	v.type = ValueType::FIXEDSTRING;
	v.data.fstr = {"Unigma"};

	if (!gObj.TypeMatches<std::string>(v))
	{
		Logger::WriteMessage("EXCEPTION: TYPE FIXED STRING INVALID MATCH.");
		return false;
	}
}