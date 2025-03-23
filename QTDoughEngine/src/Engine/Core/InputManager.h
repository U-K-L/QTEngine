#pragma once
#include <cstdint>
#include <SDL2\SDL.h>
#include <glm/glm.hpp>

struct UnigmaInputStruct
{
	uint16_t port;
	bool inputReceived;
	bool orthoButtonDown;
	bool perspectiveButtonDown;
	bool cameraZoom;
	glm::vec2 wheel;
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

void cameraInputButtons(SDL_Event& inputEvent, UnigmaInputStruct* input);
void cameraProjectionButtons(SDL_Event& inputEvent, UnigmaInputStruct* input);

extern bool INPUTPROGRAMEND;
extern bool inputFramebufferResized;