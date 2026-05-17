#pragma once
#include <mutex> 
#include <thread>
#include <chrono>
#include <stdio.h>
#include <iostream>
#include <functional>

struct SharedState {
    std::mutex mtx;
    std::condition_variable cvMain;
    std::condition_variable cvWorker;
    bool requestSignal = false;
    bool workFinished = false;
    bool continueSignal = false;
};

using namespace std;
class UnigmaThread
{
public:
    std::thread thread;
    SharedState shared;

    std::atomic<bool> shouldTerminate{ false };

    UnigmaThread(void (*func)())
    {
        thread = std::thread(func);
    }

    UnigmaThread(std::function<void()> func) {
        thread = std::thread([this, func]() {
            while (!shouldTerminate) {
                func();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            });
    }

    ~UnigmaThread() {
        shouldTerminate = true;
        shared.cvWorker.notify_one();
        if (thread.joinable()) {
            thread.join();
        }
    }
};