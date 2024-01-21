#pragma once

#ifdef QTD_EXPORTS
#define QTD_API __declspec(dllexport)
#else
#define QTD_API __declspec(dllimport)
#endif

extern "C" QTD_API int getRandomNumber();
extern "C" QTD_API bool isThisCpp();
extern "C" QTD_API int sumNumbers(int a, int b);
extern "C" QTD_API int* getPointer();
extern "C" QTD_API void FillString(char* myString, int length);



extern "C"
{
	typedef int(*CalledFromCSharp)(int a, float b);
	QTD_API void Init(int(*csharpFunctionPtr)(int, float));
	QTD_API int Foo();

	static CalledFromCSharp callBackInstance = nullptr;

	// C++ function that C# calls
	// Takes the function pointer for the C# function that C++ can call
	void Init(int(*csharpFunctionPtr)(int, float))
	{
		callBackInstance = csharpFunctionPtr;
	}

	// Example function that calls into C#
	int  Foo()
	{
		// It's just a function call like normal!
		int retVal = 10;
		if(callBackInstance != nullptr)
			retVal = callBackInstance(2, 3.14f);
		return retVal;
	}
}