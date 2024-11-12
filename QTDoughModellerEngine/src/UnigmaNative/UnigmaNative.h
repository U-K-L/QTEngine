#pragma once
#include <windows.h>

// Function pointer types for the functions exported from the DLL
typedef int (*FnUnigmaNative)();
typedef void (*FnStartProgram)();
typedef void (*FnEndProgram)();
FnStartProgram UNStartProgram;
FnEndProgram UNEndProgram;
HMODULE unigmaNative;

void LoadUnigmaNativeFunctions()
{
	UNStartProgram = (FnStartProgram)GetProcAddress(unigmaNative, "StartProgram");
	UNEndProgram = (FnEndProgram)GetProcAddress(unigmaNative, "EndProgram");

}

