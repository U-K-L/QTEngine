#include "InputHander.h"
#include <iostream>

bool orthoButtonDown = false;
bool perspectiveButtonDown = false;

void SetButtonInputs()
{
	SDL_Event inputEvent;
	while(SDL_PollEvent(&inputEvent))
	{
		cameraProjectionButtons(inputEvent);
	}
}

void cameraProjectionButtons(SDL_Event& inputEvent)
{
	if (inputEvent.type == SDL_KEYDOWN)
	{

		if (inputEvent.key.keysym.sym == SDLK_o)
		{
			orthoButtonDown = true;
		}
		if (inputEvent.key.keysym.sym == SDLK_p)
		{
			perspectiveButtonDown = true;
		}
	}
	if (inputEvent.type == SDL_KEYUP)
	{
		if (inputEvent.key.keysym.sym == SDLK_o)
		{
			orthoButtonDown = false;
		}
		if (inputEvent.key.keysym.sym == SDLK_p)
		{
			perspectiveButtonDown = false;
		}
	}
}