#ifndef OSK_H
#define OSK_H

#include <SDL/SDL.h>

bool OSK_IsVisible();
void OSK_Toggle();
void OSK_Show();
void OSK_Hide();

bool JoyIsJoystick0Enabled();
SDL_Joystick *JoyGetJoystick0();
unsigned int JoyGetJoystick0Index();

bool OSK_HandleEvent(SDL_Event* event);

void OSK_Draw(SDL_Surface* surface);

#endif // OSK_H