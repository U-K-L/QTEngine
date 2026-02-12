#include "CameraComp.h"
//Add game manager
#include "../../UnigmaGameManager.h"

//SDL
#include <SDL.h>
#include "../../../Core/InputHander.h"

CameraComp::CameraComp()
{
}

CameraComp::~CameraComp()
{
}

void CameraComp::GetInputs()
{
    UnigmaInputStruct* Controller0 = &UnigmaGameManager::instance->Controller0;
    if (Controller0->mouseMiddle == 1)
    {
        AddBrushCallbackPointer(1, 0, 0, 0, 1, 1, 1, 1,0.023f, 0.1f, 0, 1, 1);
        rotationHeld = true;
    }
	else if (Controller0->mouseMiddle == 2)
	{
		rotationHeld = false;
	}

    if (Controller0->mouseLeft == 1)
    {
        panHeld = true;
    }
    else if (Controller0->mouseLeft == 2)
    {
        panHeld = false;
    }
}

void CameraComp::Update()
{
    GetInputs();
    UnigmaInputStruct *Controller0 = &UnigmaGameManager::instance->Controller0;

    /*
    // Get the input event
    SDL_Event inputEvent = UnigmaGameManager::instance->inputEvent;

    // Handle panning with Shift + Left Mouse Button
    static bool isPanning = false;            // Tracks if panning is active
    static bool isOrbiting = false;           // Tracks if orbiting is active
    static int lastMouseX = 0, lastMouseY = 0; // Tracks the last mouse position

    if (inputEvent.type == SDL_MOUSEBUTTONDOWN && inputEvent.button.button == SDL_BUTTON_LEFT)
    {
        // Check if Shift is held while left-clicking for panning
        const Uint8* keyState = SDL_GetKeyboardState(NULL);
        if (keyState[SDL_SCANCODE_LSHIFT] || keyState[SDL_SCANCODE_RSHIFT])
        {
            isPanning = true;
        }

        SDL_GetMouseState(&lastMouseX, &lastMouseY); // Store the initial mouse position
    }

    if (inputEvent.type == SDL_MOUSEBUTTONDOWN && inputEvent.button.button == SDL_BUTTON_RIGHT)
    {
        // Check if Shift is held while left-clicking for panning
        const Uint8* keyState = SDL_GetKeyboardState(NULL);
        if (keyState[SDL_SCANCODE_LSHIFT] || keyState[SDL_SCANCODE_RSHIFT])
        {
            isOrbiting = true; 
        }
    }

    if (inputEvent.type == SDL_MOUSEMOTION)
    {
        int currentMouseX, currentMouseY;
        SDL_GetMouseState(&currentMouseX, &currentMouseY); // Get current mouse position
        float deltaX = static_cast<float>(currentMouseX - lastMouseX);
        float deltaY = static_cast<float>(currentMouseY - lastMouseY);

        lastMouseX = currentMouseX; // Update last mouse position
        lastMouseY = currentMouseY;

        if (isPanning)
        {
            glm::vec3 cameraRight = camera->_transform.right();
            glm::vec3 cameraUp = camera->_transform.up();
            glm::vec3 worldDelta = cameraRight * deltaX + cameraUp * -deltaY;

            float panSpeed = 0.01f;
            glm::vec3 newCameraPosition = camera->position() + worldDelta * panSpeed;
            camera->setPosition(newCameraPosition);
        }

        if (isOrbiting)
        {
            // Get rotation rate from the camera
            float rotationRate = 0.004f;

            // Calculate angles in radians
            float angleX = deltaX * rotationRate;
            float angleY = deltaY * rotationRate;

            // Orbit horizontally around the Y-axis
            camera->rotateAroundPoint(glm::vec3(0.0f), angleX, glm::vec3(0.0f, 0.0f, 1.0f));

            // Orbit vertically around the X-axis (optional, limited to avoid flipping)
            glm::vec3 cameraForward = camera->forward();
            glm::vec3 forwardProjection = glm::vec3(cameraForward.x, 0.0f, cameraForward.z); // Forward projected to XZ plane
            if (glm::length(forwardProjection) > 0.01f) // Prevent flipping at zenith
            {
                camera->rotateAroundPoint(glm::vec3(0.0f), angleY, camera->_transform.right());
            }
        }
    }

    if (inputEvent.type == SDL_MOUSEBUTTONUP)
    {
        isPanning = false;
        isOrbiting = false;
    }
    */
    // Zoom camera via mousewheel
    if (Controller0->cameraZoom == true)
    {
        int scrollY = Controller0->wheel.y;
        camera->Zoom(scrollY);
        Controller0->cameraZoom = false;
    }

    //If keyboard P button press change to perspective or orthogonal. depending on current.
    if (Controller0->perspectiveButtonDown)
	{
		//camera->isOrthogonal = !camera->isOrthogonal;
        float transitionSpeed = 1.0f;

        camera->isOrthogonal -= transitionSpeed * UnigmaGameManager::instance->deltaTime;
        camera->isOrthogonal = std::fmax(0, camera->isOrthogonal);
	}

    //ooposite if O is pressed
    if (Controller0->orthoButtonDown)
    {
        float transitionSpeed = 1.0f;

		camera->isOrthogonal += transitionSpeed * UnigmaGameManager::instance->deltaTime;
        camera->isOrthogonal = std::fmin(1, camera->isOrthogonal);
	}

    camera->farClip = glm::mix(1000.0f, 1000.0f, camera->isOrthogonal);

    //std::cout << "Clip far plane: " << camera->farClip << std::endl;


    // MMB orbit (Blender-style)
    if (rotationHeld)
    {
        int dx, dy;
        SDL_GetRelativeMouseState(&dx, &dy);

        float rotationRate = 0.005f;
        float yaw   = -static_cast<float>(dx) * rotationRate;
        float pitch  = -static_cast<float>(dy) * rotationRate;

        // Yaw: orbit around pivot on world Z axis
        camera->rotateAroundPoint(orbitPivot, yaw, glm::vec3(0.0f, 0.0f, 1.0f));

        // Pitch: clamp elevation angle to prevent flipping past poles
        glm::vec3 toCamera = camera->position() - orbitPivot;
        float dist = glm::length(toCamera);
        if (dist > 0.001f)
        {
            float currentElevation = asinf(glm::clamp(toCamera.z / dist, -1.0f, 1.0f));
            float maxElev = glm::radians(89.0f);

            float newElevation = currentElevation + pitch;
            if (newElevation > maxElev)  pitch = maxElev - currentElevation;
            if (newElevation < -maxElev) pitch = -maxElev - currentElevation;

            if (fabsf(pitch) > 0.0001f)
            {
                camera->rotateAroundPoint(orbitPivot, pitch, camera->_transform.right());
            }
        }
    }
    else if (panHeld)
    {
        // Shift+LMB pan (Blender-style)
        int dx, dy;
        SDL_GetRelativeMouseState(&dx, &dy);

        float panSpeed = 0.01f;
        glm::vec3 cameraRight = camera->_transform.right();
        glm::vec3 cameraUp = camera->_transform.up();
        glm::vec3 offset = cameraRight * (-static_cast<float>(dx) * panSpeed)
                         + cameraUp * (static_cast<float>(dy) * panSpeed);
        camera->setPosition(camera->position() + offset);
        orbitPivot += offset;
    }
    else
    {
        // Drain accumulated relative state so it doesn't spike on next press
        SDL_GetRelativeMouseState(nullptr, nullptr);
    }
}


void CameraComp::Start()
{
	//Add this camera to global objects.

    /*
    camera->farClip = 15.0f;
    camera->nearClip = 0.1f;
    camera->isOrthogonal = 0.0f;
    */
}

void CameraComp::InitializeData(nlohmann::json& componentData)
{
	// Initialize camera properties from JSON data
    UnigmaCameraStruct cameraTemp = UnigmaCameraStruct();
    Cameras.push_back(cameraTemp);
    CameraID = Cameras.size() - 1;

    camera = &Cameras[CameraID];
    std::cout << componentData["CameraType"] << std::endl;
    //update transform;
    UnigmaGameObject* gobj = &GameObjects[GID];
    //position
    camera->setPosition(gobj->transform.position);
    //rotation
    camera->setForward(gobj->transform.forward());
	if (componentData.contains("FarClip"))
	{
        camera->farClip = componentData["FarClip"];
	}
	if (componentData.contains("NearClip"))
	{
		camera->nearClip = componentData["NearClip"];
	}
	if (componentData.contains("CameraType"))
	{
        if(componentData["CameraType"] == "Perspective")
			camera->isOrthogonal = 0.0f;
		else if(componentData["CameraType"] == "Orthogonal")
			camera->isOrthogonal = 1.0f;
	}
    if(componentData.contains("OrthographicSize"))
	{
		camera->orthoWidth = componentData["OrthographicSize"];
	}
}