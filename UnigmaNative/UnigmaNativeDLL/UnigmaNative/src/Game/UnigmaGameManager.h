#pragma once
#include <iostream>
#include "UnigmaGameObject.h"
#include "../Core/UnigmaNative.h"
#include "../Game/Components/ComponentsHeaders.h"
#include "UnigmaSceneManager.h"
#include "../Rendering/UnigmaRenderingManager.h"
#include <algorithm>
#include <SDL.h>

// Function declarations.
void AddGameObject(UnigmaGameObject gameObject);
void RemoveGameObject(uint32_t ID);

//Typedef for callback register loadScene function
typedef void (*LoadSceneCallbackType)(const char* sceneName);

//pointer to the callback function
extern LoadSceneCallbackType LoadSceneCallbackPointer;

class UnigmaGameManager
{
public:
    static UnigmaGameManager* instance;

    static void SetInstance(UnigmaGameManager* app)
    {
        std::cout << "Setting Game Manager instance" << std::endl;
        instance = app;
    }

    // Delete copy constructor and assignment operator to prevent copies.
    UnigmaGameManager(const UnigmaGameManager&) = delete;
    UnigmaGameManager& operator=(const UnigmaGameManager&) = delete;

    void Update();
    void Start();
    void Create();
    void LoadScene(const char* sceneName);

    void SortComponents();
    void AddComponent(UnigmaGameObject gObj, Component component);

    UnigmaRenderingManager* RenderingManager;
    UnigmaSceneManager* SceneManager;
    UnigmaGameManager();

    ~UnigmaGameManager();

    std::vector<Component*> Components;
    bool IsCreated;

    //Add to input manager in the future.
    SDL_Event inputEvent;
    float deltaTime;
    std::chrono::high_resolution_clock::time_point lastTime;
    std::chrono::high_resolution_clock::time_point inputTime;

};

extern "C" {
    UNIGMANATIVE_API UnigmaGameObject* GetGameObject(uint32_t ID);

    // Function to get the size of the RenderingObjects vector
    UNIGMANATIVE_API uint32_t GetRenderObjectsSize();

    // Function to get an element from the RenderingObjects vector by index
    UNIGMANATIVE_API UnigmaRenderingStruct* GetRenderObjectAt(uint32_t index);

    //Get cameras.
    UNIGMANATIVE_API UnigmaCameraStruct* GetCamera(uint32_t ID);

    //Size of Camera.
    UNIGMANATIVE_API uint32_t GetCamerasSize();

    //Register loading scene callback
    extern UNIGMANATIVE_API void RegisterLoadSceneCallback(LoadSceneCallbackType callback);

}