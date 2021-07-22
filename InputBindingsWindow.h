#pragma once

#include <wx/wx.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/combobox.h>

#include "Config.h"
#include "Emulator.h"

#define SDL_KEYCODE_NOT_FOUND -1

class InputBindingsWindow : public wxFrame
{
public:
	InputBindingsWindow(wxWindow* parent, Config* config, Joypad* joypad, bool* window_active);
	// note: no destructor where all heap-allocated objects are deleted is needed. These are automatically destroyed when the window is destroyed

private:
	Config* config = nullptr;
	Joypad* joypad = nullptr;

	bool* window_active = nullptr;

	const wxSize button_size         = wxSize(70, 25);
	const wxSize button_options_size = wxSize(90, 25);
	const wxSize label_size          = wxSize(50, 25);
	static const int padding = 10; // space (pixels) between controls

	static const int NUM_INPUT_KEYS         = 8;
	static const int ID_KEYBOARD_BASE       = 20000;
	static const int ID_JOYPAD_BASE         = ID_KEYBOARD_BASE  + NUM_INPUT_KEYS;
	static const int ID_RESET_KEYBOARD      = ID_JOYPAD_BASE    + NUM_INPUT_KEYS;
	static const int ID_RESET_JOYPAD        = ID_RESET_KEYBOARD + 1;
	static const int ID_CANCEL_AND_EXIT     = ID_RESET_KEYBOARD + 2;
	static const int ID_SAVE_AND_EXIT       = ID_RESET_KEYBOARD + 3;

	const char* button_labels[NUM_INPUT_KEYS] = { "A", "B", "SELECT", "START", "RIGHT", "LEFT", "UP", "DOWN" };

	wxButton* buttons_keyboard[NUM_INPUT_KEYS]{};
	wxButton* buttons_joypad[NUM_INPUT_KEYS]{};
	wxButton* button_reset_keyboard = nullptr;
	wxButton* button_reset_joypad = nullptr;
	wxButton* button_save_and_exit = nullptr;
	wxButton* button_cancel_and_exit = nullptr;

	wxStaticText* static_text_buttons[NUM_INPUT_KEYS]{};
	wxStaticText* static_text_control = nullptr;
	wxStaticText* static_text_keyboard = nullptr;
	wxStaticText* static_text_joypad = nullptr;
	wxStaticText* static_text_special = nullptr;

	/// gameboy unit front image ///
	const unsigned image_width = 230;
	const unsigned image_height = 377;
	wxStaticBitmap* image = nullptr;

	int index_of_awaited_input_button;
	bool awaiting_controller_input = false;
	bool awaiting_keyboard_input = false;
	wxString prev_input_button_label;

	// event methods
	void OnKeyboardButton(wxCommandEvent& event);
	void OnJoypadButton(wxCommandEvent& event);
	void OnResetKeyboard(wxCommandEvent& event);
	void OnResetJoypad(wxCommandEvent& event);
	void OnCancelAndExit(wxCommandEvent& event);
	void OnSaveAndExit(wxCommandEvent& event);
	void OnCloseWindow(wxCloseEvent& event);
	void OnKeyDown(wxKeyEvent& event);
	void OnJoyDown(wxJoystickEvent& event);
	void OnButtonLostFocus(wxFocusEvent& event);

	void GetButtonLabels();
	SDL_Keycode Convert_WX_Keycode_To_SDL_Keycode(int wx_keycode);
	u8 Convert_WX_Joybutton_To_SDL_Joybutton(int wx_joybutton);

	wxDECLARE_EVENT_TABLE();
};