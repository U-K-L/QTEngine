#pragma once
#include <mutex> 
#include <thread>
#include <chrono>
#include <stdio.h>
#include <iostream>
#include <functional>

using namespace std;
class UnigmaThread
{
public:
    std::thread thread;
    condition_variable maincv;
    condition_variable workcv;
    mutex mainmtx;
    mutex workmtx;
    atomic<bool> requestSignal{ false };     // Main -> Worker
    atomic<bool> workFinished{ false };      // Worker -> Main
    atomic<bool> continueSignal{ false };    // Main -> Worker (resume)

    atomic<bool> isSleeping;
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

    void AskRequest() {
        requestSignal.store(true, std::memory_order_release);
    }

    void NotifyMain() {
		maincv.notify_one();
	}

    void NotifyWorker() {
		workcv.notify_one();
	}
};