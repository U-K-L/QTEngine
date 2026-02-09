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
	uint8_t mouseMiddle, mouseRight, mouseLeft, pad1, pad2, pad3, pad4;
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