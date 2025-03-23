#include "InputManager.h"
#include <iostream>

bool INPUTPROGRAMEND = false;
bool inputFramebufferResized = false;
UnigmaInputStruct GetInput(int flag)
{
	UnigmaInputStruct input;
	SetButtonInputs(&input);
	input.port = 0;
	
	return input;
}

void SetButtonInputs(UnigmaInputStruct* input)
{
	inputFramebufferResized = false;
	SDL_Event inputEvent;
	while (SDL_PollEvent(&inputEvent)) {
		if (inputEvent.type == SDL_QUIT) {
			std::cout << "Quitting program." << std::endl;
			INPUTPROGRAMEND = true;
		}
		else if (inputEvent.type == SDL_WINDOWEVENT) {
			if (inputEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || inputEvent.window.event == SDL_WINDOWEVENT_RESIZED) {
				inputFramebufferResized = true;
			}
		}
		//ImGui_ImplSDL2_ProcessEvent(&e);
		cameraProjectionButtons(inputEvent, input);
		cameraInputButtons(inputEvent, input);
		input->inputReceived = true;
	}
}

void cameraProjectionButtons(SDL_Event& inputEvent, UnigmaInputStruct* input)
{
	if (inputEvent.type == SDL_KEYDOWN)
	{

		if (inputEvent.key.keysym.sym == SDLK_o)
		{
			input->orthoButtonDown = true;
		}
		if (inputEvent.key.keysym.sym == SDLK_p)
		{
			input->perspectiveButtonDown = true;
		}
	}
	if (inputEvent.type == SDL_KEYUP)
	{
		if (inputEvent.key.keysym.sym == SDLK_o)
		{
			input->orthoButtonDown = false;
		}
		if (inputEvent.key.keysym.sym == SDLK_p)
		{
			input->perspectiveButtonDown = false;
		}
	}
}

void cameraInputButtons(SDL_Event& inputEvent, UnigmaInputStruct* input)
{
	input->cameraZoom = false;
	if (inputEvent.type == SDL_MOUSEWHEEL)
	{
		glm::vec2 wheel = glm::vec2(inputEvent.wheel.x, inputEvent.wheel.y);
		input->wheel = wheel;
		input->cameraZoom = true;
	}
}