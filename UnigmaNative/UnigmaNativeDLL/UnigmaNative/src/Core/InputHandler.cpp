#include "InputHander.h"
#include <iostream>

UnigmaInputStruct GetInput(int flag)
{
	UnigmaInputStruct input;
	SetButtonInputs(&input);
	input.port = 0;
	return input;
}

bool orthoButtonDown = false;
bool perspectiveButtonDown = false;

void SetButtonInputs(UnigmaInputStruct* input)
{
	SDL_Event inputEvent;
	while (SDL_PollEvent(&inputEvent))
	{
		cameraProjectionButtons(inputEvent, input);
		SetMovement(inputEvent, input);
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