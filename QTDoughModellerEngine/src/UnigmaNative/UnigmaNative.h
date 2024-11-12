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
    std::atomic<bool> shouldTerminate{ false }; // Flag to signal thread termination
	UnigmaThread(void (*func)())
	{
		thread = std::thread(func);
	}

    UnigmaThread(std::function<void()> func) {
        thread = std::thread([this, func]() {
            while (!shouldTerminate) {
                func(); // Run the provided function
                std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Example sleep to avoid tight loop
            }
            });
    }

    ~UnigmaThread() {
        shouldTerminate = true; // Signal the thread to terminate
        if (thread.joinable()) {
            thread.join(); // Ensure the thread finishes before destruction
        }
    }
};

