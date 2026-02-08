#pragma once
#include <SDL.h>
#include <cstdint>
#include <glm.hpp>

struct UnigmaInputStruct
{
	uint16_t port;
	bool inputReceived;
	bool orthoButtonDown;
	bool perspectiveButtonDown;
	bool cameraZoom;
	bool mouseMiddle, mouseRight;
	glm::vec2 wheel;
	glm::vec2 movement;
	UnigmaInputStruct()
		: port(0),
		inputReceived(false),
		orthoButtonDown(false),
		perspectiveButtonDown(false),
		cameraZoom(false)
	{}
};



UnigmaInputStruct GetInput(int flag);

//Set button inputs.
void SetButtonInputs(UnigmaInputStruct* input);

void cameraProjectionButtons(SDL_Event& inputEvent, UnigmaInputStruct* input);

void SetMovement(SDL_Event& inputEvent, UnigmaInputStruct* input);