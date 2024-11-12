#pragma once
#include <windows.h>
#include <mutex> 
#include <thread>
#include <chrono>

// Function pointer types for the functions exported from the DLL
typedef int (*FnUnigmaNative)();
typedef void (*FnStartProgram)();
typedef void (*FnEndProgram)();
FnStartProgram UNStartProgram;
FnEndProgram UNEndProgram;
HMODULE unigmaNative;

using namespace std;

void LoadUnigmaNativeFunctions()
{
	UNStartProgram = (FnStartProgram)GetProcAddress(unigmaNative, "StartProgram");
	UNEndProgram = (FnEndProgram)GetProcAddress(unigmaNative, "EndProgram");

}

class UnigmaThread
{
public:
	std::thread thread;
	condition_variable cv;
	mutex mtx;
	atomic<bool> isSleeping();
	UnigmaThread(void (*func)())
	{
		thread = std::thread(func);
	}

};

