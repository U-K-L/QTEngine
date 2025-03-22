#pragma once
#include <cstdint>
#include <SDL2\SDL.h>

struct UnigmaInputStruct
{
	uint16_t port;
	bool inputReceived;
	bool orthoButtonDown;
	bool perspectiveButtonDown;

	UnigmaInputStruct()
		: port(0),
		inputReceived(false),
		orthoButtonDown(false),
		perspectiveButtonDown(false)
	{}
};


UnigmaInputStruct GetInput(int flag);

//Set button inputs.
void SetButtonInputs(UnigmaInputStruct* input);

void cameraProjectionButtons(SDL_Event& inputEvent, UnigmaInputStruct* input);

extern bool INPUTPROGRAMEND;
extern bool inputFramebufferResized;