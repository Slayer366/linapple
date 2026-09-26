#ifndef OSK_H
#define OSK_H

#ifdef SDL2
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#else
#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#endif

bool OSK_IsVisible();
void OSK_Toggle();
void OSK_Show();
void OSK_Hide();
void OSK_Cleanup();

bool JoyIsJoystick0Enabled();
SDL_Joystick *JoyGetJoystick0();
unsigned int JoyGetJoystick0Index();

bool OSK_HandleEvent(SDL_Event* event);

void OSK_Draw(SDL_Surface* surface);

#endif // OSK_H