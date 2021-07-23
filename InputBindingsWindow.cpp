#include "InputBindingsWindow.h"

#define SDL_KEYCODE_NOT_FOUND -1

wxBEGIN_EVENT_TABLE(InputBindingsWindow, wxFrame)
	EVT_BUTTON(ID_RESET_KEYBOARD , InputBindingsWindow::OnResetKeyboard)
	EVT_BUTTON(ID_RESET_JOYPAD   , InputBindingsWindow::OnResetJoypad)
	EVT_BUTTON(ID_CANCEL_AND_EXIT, InputBindingsWindow::OnCancelAndExit)
	EVT_BUTTON(ID_SAVE_AND_EXIT  , InputBindingsWindow::OnSaveAndExit)
	EVT_CLOSE(InputBindingsWindow::OnCloseWindow)
wxEND_EVENT_TABLE()


InputBindingsWindow::InputBindingsWindow(wxWindow* parent, Config* config, Joypad* joypad, bool* window_active) :
	wxFrame(parent, wxID_ANY, "Input binding configuration", wxDefaultPosition, wxDefaultSize,
		wxDEFAULT_FRAME_STYLE & ~(wxRESIZE_BORDER | wxMAXIMIZE_BOX))
{
	this->config = config;
	this->joypad = joypad;
	this->window_active = window_active;

	// determine and set size of window
	int sizeX = padding + label_size.x + 2 * button_size.x + image_width;
	int sizeY = std::max(2 * padding + label_size.y + 8 * button_size.y, (int)image_height);
	SetClientSize(sizeX, sizeY);

	// create and layout all labels and buttons
	static_text_control  = new wxStaticText(this, wxID_ANY, "Control" , wxPoint(padding                                       , 0), label_size);
	static_text_keyboard = new wxStaticText(this, wxID_ANY, "Keyboard", wxPoint(padding + label_size.x                        , 0), label_size);
	static_text_joypad   = new wxStaticText(this, wxID_ANY, "Joypad"  , wxPoint(padding + label_size.x + button_size.x        , 0), label_size);
	static_text_special  = new wxStaticText(this, wxID_ANY, "Special" , wxPoint(3 * padding + 2 * label_size.x + button_size.x, 0), label_size);

	for (int i = 0; i < NUM_INPUT_KEYS; i++)
	{
		static_text_buttons[i] = new wxStaticText(this, wxID_ANY            , button_labels[i], wxPoint(padding                               , label_size.y + label_size.y  * i), label_size );
		buttons_keyboard   [i] = new wxButton    (this, ID_KEYBOARD_BASE + i, button_labels[i], wxPoint(padding + label_size.x                , label_size.y + button_size.y * i), button_size);
		buttons_joypad     [i] = new wxButton    (this, ID_JOYPAD_BASE   + i, button_labels[i], wxPoint(padding + label_size.x + button_size.x, label_size.y + button_size.y * i), button_size);
	}

	unsigned end_of_input_buttons_y = padding + label_size.y + std::max(label_size.y * NUM_INPUT_KEYS, button_size.y * NUM_INPUT_KEYS);
	button_reset_keyboard  = new wxButton(this, ID_RESET_KEYBOARD , "Reset keyboard" , wxPoint(padding                        , end_of_input_buttons_y                        ), button_options_size);
	button_reset_joypad    = new wxButton(this, ID_RESET_JOYPAD   , "Reset joypad"   , wxPoint(padding + button_options_size.x, end_of_input_buttons_y                        ), button_options_size);
	button_cancel_and_exit = new wxButton(this, ID_CANCEL_AND_EXIT, "Cancel and exit", wxPoint(padding                        , end_of_input_buttons_y + button_options_size.y), button_options_size);
	button_save_and_exit   = new wxButton(this, ID_SAVE_AND_EXIT  , "Save and exit"  , wxPoint(padding + button_options_size.x, end_of_input_buttons_y + button_options_size.y), button_options_size);

	// setup image
	wxImage::AddHandler(new wxPNGHandler());
	image = new wxStaticBitmap(this, wxID_ANY, wxBitmap("Gameboy_front.png", wxBITMAP_TYPE_PNG), wxPoint(padding + label_size.x + 2 * button_size.x, 0));

	GetButtonLabels();

	SetBackgroundColour(*wxWHITE);

	// setup event bindings
	for (int i = 0; i < NUM_INPUT_KEYS; i++)
	{
		// https://wiki.wxwidgets.org/Catching_key_events_globally
		buttons_keyboard[i]->Bind(wxEVT_CHAR_HOOK, &InputBindingsWindow::OnKeyDown, this);
		buttons_joypad  [i]->Bind(wxEVT_JOY_BUTTON_DOWN, &InputBindingsWindow::OnJoyDown, this);

		buttons_keyboard[i]->Bind(wxEVT_BUTTON, &InputBindingsWindow::OnKeyboardButton, this);
		buttons_joypad  [i]->Bind(wxEVT_BUTTON, &InputBindingsWindow::OnJoypadButton, this);

		buttons_keyboard[i]->Bind(wxEVT_KILL_FOCUS, &InputBindingsWindow::OnButtonLostFocus, this);
		buttons_joypad  [i]->Bind(wxEVT_KILL_FOCUS, &InputBindingsWindow::OnButtonLostFocus, this);
	}

	*window_active = true;
}


void InputBindingsWindow::OnKeyboardButton(wxCommandEvent& event)
{
	int button_index = event.GetId() - ID_KEYBOARD_BASE;
 	prev_input_button_label = buttons_keyboard[button_index]->GetLabel();
	buttons_keyboard[button_index]->SetLabel("...");
	index_of_awaited_input_button = button_index;
	awaiting_keyboard_input = true;
}


void InputBindingsWindow::OnJoypadButton(wxCommandEvent& event)
{
	int button_index = event.GetId() - ID_JOYPAD_BASE;
	prev_input_button_label = buttons_joypad[button_index]->GetLabel();
	buttons_joypad[button_index]->SetLabel("...");
	index_of_awaited_input_button = button_index;
	awaiting_controller_input = true;
}


void InputBindingsWindow::OnKeyDown(wxKeyEvent& event)
{
	if (awaiting_keyboard_input)
	{
		awaiting_keyboard_input = false;

		int keycode = event.GetKeyCode();
		if (keycode != WXK_NONE)
		{
			SDL_Keycode sdl_keycode = Convert_WX_Keycode_To_SDL_Keycode(keycode);
			if (sdl_keycode != SDL_KEYCODE_NOT_FOUND)
			{
				const char* name = SDL_GetKeyName(sdl_keycode);
				buttons_keyboard[index_of_awaited_input_button]->SetLabel(name);
				joypad->UpdateBinding(static_cast<Joypad::Button>(index_of_awaited_input_button), sdl_keycode);
				return;
			}
		}

		buttons_keyboard[index_of_awaited_input_button]->SetLabel(prev_input_button_label);
	}
}


void InputBindingsWindow::OnJoyDown(wxJoystickEvent& event)
{
	if (awaiting_controller_input)
	{
		awaiting_controller_input = false;

		int keycode = 0; // todo
		if (keycode != WXK_NONE)
		{
			SDL_Keycode sdl_keycode = Convert_WX_Keycode_To_SDL_Keycode(keycode);
			if (sdl_keycode != SDL_KEYCODE_NOT_FOUND)
			{
				const char* name = SDL_GetKeyName(sdl_keycode);
				buttons_keyboard[index_of_awaited_input_button]->SetLabel(name);
				joypad->UpdateBinding(static_cast<Joypad::Button>(index_of_awaited_input_button), sdl_keycode);
				return;
			}
		}

		buttons_joypad[index_of_awaited_input_button]->SetLabel(prev_input_button_label);
	}
}


void InputBindingsWindow::OnButtonLostFocus(wxFocusEvent& event)
{
	if (awaiting_keyboard_input)
	{
		buttons_keyboard[index_of_awaited_input_button]->SetLabel(prev_input_button_label);
		awaiting_keyboard_input = false;
	}
	else if (awaiting_controller_input)
	{
		buttons_joypad[index_of_awaited_input_button]->SetLabel(prev_input_button_label);
		awaiting_controller_input = false;
	}
}


void InputBindingsWindow::OnResetKeyboard(wxCommandEvent& event)
{
	joypad->ResetBindings(Joypad::InputMethod::KEYBOARD);
	GetButtonLabels();
}


void InputBindingsWindow::OnResetJoypad(wxCommandEvent& event)
{
	joypad->ResetBindings(Joypad::InputMethod::JOYPAD);
	GetButtonLabels();
}


void InputBindingsWindow::OnCancelAndExit(wxCommandEvent& event)
{
	joypad->RevertBindingChanges();
	Close();
	*window_active = false;
}


void InputBindingsWindow::OnSaveAndExit(wxCommandEvent& event)
{
	joypad->SaveBindings();
	config->Save();
	Close();
	*window_active = false;
}


void InputBindingsWindow::OnCloseWindow(wxCloseEvent& event)
{
	joypad->RevertBindingChanges();
	event.Skip();
	*window_active = false;
}


void InputBindingsWindow::GetButtonLabels()
{
	buttons_keyboard[0]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::A, Joypad::InputMethod::KEYBOARD));
	buttons_keyboard[1]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::B, Joypad::InputMethod::KEYBOARD));
	buttons_keyboard[2]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::SELECT, Joypad::InputMethod::KEYBOARD));
	buttons_keyboard[3]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::START, Joypad::InputMethod::KEYBOARD));
	buttons_keyboard[4]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::RIGHT, Joypad::InputMethod::KEYBOARD));
	buttons_keyboard[5]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::LEFT, Joypad::InputMethod::KEYBOARD));
	buttons_keyboard[6]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::UP, Joypad::InputMethod::KEYBOARD));
	buttons_keyboard[7]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::DOWN, Joypad::InputMethod::KEYBOARD));

	buttons_joypad[0]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::A, Joypad::InputMethod::JOYPAD));
	buttons_joypad[1]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::B, Joypad::InputMethod::JOYPAD));
	buttons_joypad[2]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::SELECT, Joypad::InputMethod::JOYPAD));
	buttons_joypad[3]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::START, Joypad::InputMethod::JOYPAD));
	buttons_joypad[4]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::RIGHT, Joypad::InputMethod::JOYPAD));
	buttons_joypad[5]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::LEFT, Joypad::InputMethod::JOYPAD));
	buttons_joypad[6]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::UP, Joypad::InputMethod::JOYPAD));
	buttons_joypad[7]->SetLabel(joypad->GetCurrentBindingString(Joypad::Button::DOWN, Joypad::InputMethod::JOYPAD));
}


SDL_Keycode InputBindingsWindow::Convert_WX_Keycode_To_SDL_Keycode(int wx_keycode)
{
	// ASCII range. From what I have tested so far, the wxWidgets keycodes enum correspond to the SDL keycodes enum exactly in this range
	// One difference is that SDL does not define keycodes for uppercase letters, which is what wxwidgets produces by default.
	// If 'A' is pressed, then wx_keycode == 65. However, SDLK_a == 97, so we need to translate such key presses.
	if (wx_keycode >= 0 && wx_keycode <= 127)
	{
		if (wx_keycode >= 65 && wx_keycode <= 90)
			return (SDL_Keycode)(wx_keycode + 32);
		return (SDL_Keycode)wx_keycode;
	}
	// these keycodes in the wxWidgets keycode enum do not all have the same value as in the SDL keycode enum
	else
	{
		switch (wx_keycode)
		{
		case WXK_SHIFT: return SDLK_LSHIFT;
		case WXK_ALT: return SDLK_LALT;
		case WXK_CONTROL: return SDLK_LCTRL;
		case WXK_LEFT: return SDLK_LEFT;
		case WXK_UP: return SDLK_UP;
		case WXK_RIGHT: return SDLK_RIGHT;
		case WXK_DOWN: return SDLK_DOWN;

		case WXK_NUMPAD0: return SDLK_KP_0;
		case WXK_NUMPAD1: return SDLK_KP_1;
		case WXK_NUMPAD2: return SDLK_KP_2;
		case WXK_NUMPAD3: return SDLK_KP_3;
		case WXK_NUMPAD4: return SDLK_KP_4;
		case WXK_NUMPAD5: return SDLK_KP_5;
		case WXK_NUMPAD6: return SDLK_KP_6;
		case WXK_NUMPAD7: return SDLK_KP_7;
		case WXK_NUMPAD8: return SDLK_KP_8;
		case WXK_NUMPAD9: return SDLK_KP_9;
		case WXK_NUMPAD_ADD: return SDLK_KP_PLUS;
		case WXK_NUMPAD_SUBTRACT: return SDLK_KP_MINUS;
		case WXK_NUMPAD_MULTIPLY: return SDLK_KP_MULTIPLY;
		case WXK_NUMPAD_DIVIDE: return SDLK_KP_DIVIDE;
		case WXK_NUMPAD_DECIMAL: return SDLK_KP_DECIMAL;
		case WXK_NUMPAD_ENTER: return SDLK_KP_ENTER;

		case WXK_F1: return SDLK_F1;
		case WXK_F2: return SDLK_F2;
		case WXK_F3: return SDLK_F3;
		case WXK_F4: return SDLK_F4;
		case WXK_F5: return SDLK_F5;
		case WXK_F6: return SDLK_F6;
		case WXK_F7: return SDLK_F7;
		case WXK_F8: return SDLK_F8;
		case WXK_F9: return SDLK_F9;
		case WXK_F10: return SDLK_F10;
		case WXK_F11: return SDLK_F11;
		case WXK_F12: return SDLK_F12;

		default: return SDL_KEYCODE_NOT_FOUND;
		}
	}
}


u8 Convert_WX_Joybutton_To_SDL_Joybutton(int wx_joybutton)
{
	// todo
	return 0;
}