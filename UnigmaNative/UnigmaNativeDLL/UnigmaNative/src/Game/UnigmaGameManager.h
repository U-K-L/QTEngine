#pragma once
#include <iostream>
#include "UnigmaGameObject.h"
#include "../Core/UnigmaNative.h"
#include "../Game/Components/ComponentsHeaders.h"
#include "UnigmaSceneManager.h"
#include "../Rendering/UnigmaRenderingManager.h"
#include "../Physics/UnigmaPhysicsManager.h"
#include <algorithm>
#include <SDL.h>
#include "../Core/InputHander.h"

// Function declarations.
void AddGameObject(UnigmaGameObject gameObject);
void RemoveGameObject(uint32_t ID);

//Typedef for callback register loadScene function
typedef void (*LoadSceneCallbackType)(const char* sceneName);
typedef UnigmaInputStruct (*LoadInputCallbackType)(int flag);

//pointer to the callback function
extern LoadSceneCallbackType LoadSceneCallbackPointer;
extern LoadInputCallbackType LoadInputCallbackPointer;

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
    void AddComponent(UnigmaGameObject& gObj, std::string compName);

    template<typename T> 
    T* GetObjectComponent(UnigmaGameObjectClass& gObjClass)
    {
        if (gObjClass.components.contains(T::TypeName)) {

            auto index = gObjClass.components[T::TypeName];
            auto globalId = gObjClass.gameObject->components[index];

            Component* comp = Components[globalId];

            UnigmaPhysicsComp* physicsComp = static_cast<T*>(comp);

            return static_cast<T*>(comp);
        }
        return nullptr;
    }

    UnigmaRenderingManager* RenderingManager;
    UnigmaSceneManager* SceneManager;
    UnigmaGameManager();

    ~UnigmaGameManager();

    std::vector<Component*> Components;
    bool IsCreated;

    //Add to input manager in the future.
    UnigmaInputStruct Controller0;
    SDL_Event inputEvent;
    float deltaTime;
    float currentTime;
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

    //Get Light.
    UNIGMANATIVE_API UnigmaLight* GetLight(uint32_t ID);

    //Size of Lights.
    UNIGMANATIVE_API uint32_t GetLightsSize();

    //Register loading scene callback
    extern UNIGMANATIVE_API void RegisterLoadSceneCallback(LoadSceneCallbackType callback);

    //Register loading input callback
    extern UNIGMANATIVE_API void RegisterLoadInputCallback(LoadInputCallbackType callback);
}
