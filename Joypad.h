#pragma once

#include "wx/wx.h"
#include "SDL.h"
#include "SDL_gamecontroller.h"

#include "Bus.h"
#include "Configurable.h"
#include "CPU.h"
#include "Utils.h"

class Joypad final : public Component
{
public:
	Bus* bus;
	CPU* cpu;

	enum class Button { A, B, SELECT, START, RIGHT, LEFT, UP, DOWN };
	enum class InputMethod { JOYPAD, KEYBOARD };
	enum InputEvent { RELEASE = 0, PRESS = 1 };

	const char* GetCurrentBindingString(Button button, InputMethod input_method);
	void Initialize();
	void Reset();
	void OpenGameControllers();
	void PollInput();
	void RevertBindingChanges();
	void ResetBindings(InputMethod input_method);
	void SaveBindings();
	void UpdateBinding(Button button, SDL_GameControllerButton bind);
	void UpdateBinding(Button button, SDL_Keycode key);
	void UpdateP1OutputLines();

	void Configure(Serialization::BaseFunctor& functor) override;
	void SetDefaultConfig() override;

private:
	u8* P1;

	SDL_GameController* controller;
	SDL_Joystick* joystick;
	SDL_Event event;

	template <typename T>
	struct InputSet { T A, B, SELECT, START, RIGHT, LEFT, UP, DOWN; };

	InputSet<SDL_Keycode> keyboard_bindings, new_keyboard_bindings;
	const InputSet<SDL_Keycode> default_keyboard_bindings
		{ SDLK_SPACE, SDLK_LCTRL, SDLK_BACKSPACE, SDLK_RETURN, SDLK_RIGHT, SDLK_LEFT, SDLK_UP, SDLK_DOWN };

	InputSet<SDL_GameControllerButton> joypad_bindings, new_JoypadBindings;
	const InputSet<SDL_GameControllerButton> default_joypad_bindings
		{ SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START,
		SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN };

	InputSet<bool> buttons_currently_held{ 0, 0, 0, 0, 0, 0, 0, 0 };

	void MatchInputToBindings(s32 button, InputEvent input_event, InputMethod input_method);
};