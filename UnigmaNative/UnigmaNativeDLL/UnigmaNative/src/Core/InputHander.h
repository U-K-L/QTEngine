#pragma once
#include <SDL.h>

//Orthographic Checker.
extern bool orthoButtonDown;
extern bool perspectiveButtonDown;

//Set button inputs.
void SetButtonInputs();

void cameraProjectionButtons(SDL_Event& inputEvent);