#include "InputHander.h"

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