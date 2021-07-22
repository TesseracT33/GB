#pragma once

#include "wx/wx.h"
#include <wx/colordlg.h>
#include <wx/dir.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include "SDL.h"

#include "Config.h"
#include "Emulator.h"
#include "InputBindingsWindow.h"
#include "Joypad.h"
#include "Observer.h"
#include "Themes.h"

class GUI : public wxFrame, public Configurable, public Observer
{
public:
	GUI();
	~GUI();

private:
	// IDs for all buttons in the menubar
	enum MenuBarID
	{
		open_rom = 11000,
		set_game_dir,
		set_dmg_boot_rom_path,
		set_cgb_boot_rom_path,
		save_state,
		load_state,
		quit,
		pause_play,
		reset,
		stop,
		size_1x,
		size_2x,
		size_4x,
		size_6x,
		size_8x,
		size_10x,
		size_12x,
		size_15x,
		size_custom,
		size_maximized,
		size_fullscreen,
		speed_50,
		speed_75,
		speed_100,
		speed_125,
		speed_150,
		speed_200,
		speed_300,
		speed_500,
		speed_uncapped,
		speed_custom,
		sound_off,
		input,
		toggle_boot_rom,
		toggle_filter_gb_gc_files,
		toggle_render_on_every_system_frame,
		reset_settings,
		github_link,
		listBoxDoubleClick,
		colour00,
		colour01,
		colour10,
		colour11,
		colour_generate_random_palette,
		colour_generate_random_palette_on_every_boot,
		colour_palette_base // should always be the last entry in the enum, as various menubar items have IDs offset with this value
	};

	static const wxString emulator_name;

	static const int maximum_rom_path_length = 4096;
	static const int frameID = 10000;
	static const int listBoxID = 10001;
	static const int SDLWindowID = 10002;
	static const int inputFrameID = 10003;

	// Prespecified hotkeys for certain actions
	// todo: currently, menubar labels for the below actions use prespecified strings (e.g. 'Save state (F5)), see GUI::CreateMenuBar()
	// find a way to use and convert the below definitions instead, so that the menubar labels also change if the below definitions change
	static const int wx_keycode_save_state = WXK_F5;
	static const int wx_keycode_load_state = WXK_F8;
	static const int wx_keycode_fullscreen = WXK_F11;

	// note: definitions of the below six members may need to be altered if MenuBarID is altered
	// however, no other code in GUI.h or GUI.cpp will need to be changed
	const int size_id_min = MenuBarID::size_1x;
	const int size_id_max = MenuBarID::size_15x;
	const int speed_id_min = MenuBarID::speed_50;
	const int speed_id_max = MenuBarID::speed_500;
	int GetSizeFromMenuBarID(int id) const;
	int GetSpeedFromMenuBarID(int id) const;

	const wxString empty_listbox_item = wxString("Double click this text to choose a game directory");
	const wxSize default_window_size = wxSize(500, 500);
	wxSize default_client_size; // The area of the main window that is left outside of the menubar. Computed at runtime

	bool SDL_initialized = false;
	bool game_view_active = false;
	wxString active_rom_path; // path to currently or last played rom file
	wxString rom_folder_path; // selected rom directory
	wxArrayString rom_folder_name_arr; // array of names of all roms displayed in the list of games
	wxArrayString rom_folder_path_arr; // array of paths of all roms displayed

	Config config;
	Emulator emulator;

	// configuration-related variables
	const bool default_display_only_gb_gbc_files = true;
	const bool default_generate_random_colour_palette_on_every_boot = false;
	const wxString default_rom_folder_path = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
	bool display_only_gb_gbc_files = default_display_only_gb_gbc_files;
	bool generate_random_colour_palette_on_every_boot = default_generate_random_colour_palette_on_every_boot;

	// as per the wxwidgets documentation, all menus should be created on the heap
	// however, there is no need to manually use delete on any of these pointers
	// https://docs.wxwidgets.org/3.0/classwx_menu.html
	wxMenuBar* menu_bar = new wxMenuBar();
	wxMenu* menu_file = new wxMenu();
	wxMenu* menu_emulation = new wxMenu();
	wxMenu* menu_settings = new wxMenu();
	wxMenu* menu_info = new wxMenu();
	wxMenu* menu_size = new wxMenu();
	wxMenu* menu_speed = new wxMenu();
	wxMenu* menu_colour = new wxMenu();
	wxMenu* menu_colour_palette = new wxMenu();
	wxMenu* menu_input = new wxMenu();

	wxListBox* game_list_box = nullptr; // list of selectable roms in current directory 
	wxPanel* SDL_window_panel = nullptr; // "holds" the SDL window

	InputBindingsWindow* input_bindings_window = nullptr;
	bool input_window_active = false;

	bool full_screen_active = false;

	int fps = 0; // current fps that is displayed in the window title

	void LoadConfig(std::ifstream& ifs) override;
	void SaveConfig(std::ofstream& ofs) override;
	void SetDefaultConfig() override;
	void UpdateFPSLabel(int fps) override;

	void ApplyGUISettings();
	void ChooseGameDirDialog();
	void CreateMenuBar();
	wxString format_size_menubar_label(int scale) const;
	wxString format_speed_menubar_label(int speed) const;
	wxString format_custom_size_menubar_label(int scale) const;
	wxString format_custom_speed_menubar_label(int speed) const;
	void GenerateRandomColourPalette();
	int get_id_of_size_menubar_item(int scale) const;
	int get_id_of_speed_menubar_item(int speed) const;
	void LaunchGame();
	void LocateAndOpenRomFromListBoxSelection(wxString selection);
	void Quit();
	void QuitGame();
	void SetupConfig();
	void SetupGameList();
	void SetupInitialMenuView();
	bool SetupSDL();
	void SwitchToMenuView();
	void SwitchToGameView();
	void ToggleFullScreen();
	void UpdateViewSizing();
	void UpdateWindowLabel(bool gameIsRunning);

	/// Event handling functions ///
	void OnMenuOpen(wxCommandEvent& event);
	void OnMenuSetGameDir(wxCommandEvent& event);
	void OnMenuSetDMGBootRom(wxCommandEvent& event);
	void OnMenuSetCGBBootRom(wxCommandEvent& event);
	void OnMenuSaveState(wxCommandEvent& event);
	void OnMenuLoadState(wxCommandEvent& event);
	void OnMenuQuit(wxCommandEvent& event);
	void OnMenuPausePlay(wxCommandEvent& event);
	void OnMenuReset(wxCommandEvent& event);
	void OnMenuStop(wxCommandEvent& event);
	void OnMenuSize(wxCommandEvent& event);
	void OnMenuSpeed(wxCommandEvent& event);
	void OnMenuColour(wxCommandEvent& event);
	void OnMenuColourSetRandomPalette(wxCommandEvent& event);
	void OnMenuColourToggleRandomPalette(wxCommandEvent& event);
	void OnMenuInput(wxCommandEvent& event);
	void OnMenuToggleBootRom(wxCommandEvent& event);
	void OnMenuToggleFilterFiles(wxCommandEvent& event);
	void OnMenuToggleRenderEveryFrame(wxCommandEvent& event);
	void OnMenuResetSettings(wxCommandEvent& event);
	void OnMenuGitHubLink(wxCommandEvent& event);
	void OnListBoxGameSelection(wxCommandEvent& event);
	void OnWindowSizeChanged(wxSizeEvent& event);
	void OnClose(wxCloseEvent& event);
	void OnKeyDown(wxKeyEvent& event);

	wxDECLARE_EVENT_TABLE();
};