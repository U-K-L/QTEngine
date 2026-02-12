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
		SetMovement(inputEvent, input);
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

	if (inputEvent.type == SDL_MOUSEBUTTONDOWN)
	{
		if (inputEvent.button.button == SDL_BUTTON_MIDDLE)
		{
			printf("Middle mouse button pressed\n");
			input->mouseMiddle = 1;
		}

		if (inputEvent.button.button == SDL_BUTTON_RIGHT)
		{
			SDL_Keymod mods = SDL_GetModState();
			if (mods & KMOD_SHIFT)
			{
				printf("Shift + Right Mouse Button pressed\n");
				input->mouseMiddle = 1;
			}
		}

		if (inputEvent.button.button == SDL_BUTTON_LEFT)
		{
			SDL_Keymod mods = SDL_GetModState();
			if (mods & KMOD_SHIFT)
			{
				printf("Shift + Left Mouse Button pressed\n");
				input->mouseLeft = 1;
			}
		}
	}

	if (inputEvent.type == SDL_MOUSEBUTTONUP)
	{
		if (inputEvent.button.button == SDL_BUTTON_MIDDLE)
		{
			input->mouseMiddle = 2;
		}

		if (inputEvent.button.button == SDL_BUTTON_RIGHT)
		{
			input->mouseMiddle = 2;
		}

		if (inputEvent.button.button == SDL_BUTTON_LEFT)
		{
			input->mouseLeft = 2;
		}
	}

}

void SetMovement(SDL_Event& inputEvent, UnigmaInputStruct* input)
{

	//Keyboard input arrow keys.
	if (inputEvent.type == SDL_KEYDOWN || inputEvent.type == SDL_KEYUP)
	{

		bool isDown = inputEvent.type == SDL_KEYDOWN;
		switch (inputEvent.key.keysym.sym)
		{
		case SDLK_LEFT:  input->movement.x = isDown ? -1.0f : 0.0f; break;
		case SDLK_RIGHT: input->movement.x = isDown ? 1.0f : 0.0f; break;
		case SDLK_UP:    input->movement.y = isDown ? -1.0f : 0.0f; break;
		case SDLK_DOWN:  input->movement.y = isDown ? 1.0f : 0.0f; break;
		default: break;
		}
	}
}