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


void CameraComp::Update()
{
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

    // Zoom camera via mousewheel
    if (inputEvent.type == SDL_MOUSEWHEEL)
    {
        int scrollY = inputEvent.wheel.y;
        camera->Zoom(scrollY);
    }
    */
    //If keyboard P button press change to perspective or orthogonal. depending on current.
    if (perspectiveButtonDown)
	{
		//camera->isOrthogonal = !camera->isOrthogonal;
        float transitionSpeed = 1.0f;

        camera->isOrthogonal -= transitionSpeed * UnigmaGameManager::instance->deltaTime;
	}

    //ooposite if O is pressed
    if (orthoButtonDown)
    {
        float transitionSpeed = 1.01f;

		camera->isOrthogonal += transitionSpeed * UnigmaGameManager::instance->deltaTime;
	}

    //rotate camera.
    glm::vec3 targetPoint = glm::vec3(0, 0, 0);
    float angle = sin(UnigmaGameManager::instance->currentTime * UnigmaGameManager::instance->deltaTime*0.000000005) * 360;
    camera->rotateAroundPoint(targetPoint, angle, glm::vec3(0, 0, 1));

    //std::cout << "Delta time: " << UnigmaGameManager::instance->deltaTime << std::endl;
}


void CameraComp::Start()
{
	std::cout << "Camera component started" << std::endl;
	//Add this camera to global objects.
	UnigmaCameraStruct cameraTemp = UnigmaCameraStruct();
	Cameras.push_back(cameraTemp);
	CameraID = Cameras.size() - 1;

	camera = &Cameras[CameraID];

    camera->farClip = 15.0f;
    camera->nearClip = 0.1f;
    camera->isOrthogonal = 0.0f;
}