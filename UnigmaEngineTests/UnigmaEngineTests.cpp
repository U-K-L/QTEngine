#include "pch.h"
#include "CppUnitTest.h"
#include "UnigmaGameObjectTests.h"

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
			auto gObjTests = make_unique<UnigmaGameObjectTests>();
			Assert::IsTrue(gObjTests->TestTypeToMatch());
		}
	};
}
