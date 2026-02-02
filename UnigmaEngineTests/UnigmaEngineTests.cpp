#include "pch.h"
#include "CppUnitTest.h"
#include "UnigmaGameObjectTests.h"
#include "UnigmaNative/UnigmaNative.h"
#include "Loader.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;
using namespace std;

namespace UnigmaEngineTests
{
	TEST_CLASS(UnigmaEngineTests)
	{
	public:
		
		TEST_METHOD(TestMethod1)
		{
		}

		TEST_METHOD(TestGameObject)
		{
			unigmaNative = LoadDLL(L"UnigmaNative.dll");
			LoadUnigmaNativeFunctions();

			UNStartProgram();

			auto gObjTests = make_unique<UnigmaGameObjectTests>();
			Assert::IsTrue(gObjTests->TestTypeToMatch());

			//Get Attribute testing.

		}
	};
}
