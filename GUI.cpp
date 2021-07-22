#include "GUI.h"


wxBEGIN_EVENT_TABLE(GUI, wxFrame)
	EVT_MENU(MenuBarID::open_rom, GUI::OnMenuOpen)
	EVT_MENU(MenuBarID::set_game_dir, GUI::OnMenuSetGameDir)
	EVT_MENU(MenuBarID::set_dmg_boot_rom_path, GUI::OnMenuSetDMGBootRom)
	EVT_MENU(MenuBarID::set_cgb_boot_rom_path, GUI::OnMenuSetCGBBootRom)
	EVT_MENU(MenuBarID::save_state, GUI::OnMenuSaveState)
	EVT_MENU(MenuBarID::load_state, GUI::OnMenuLoadState)
	EVT_MENU(MenuBarID::quit, GUI::OnMenuQuit)
	EVT_MENU(MenuBarID::pause_play, GUI::OnMenuPausePlay)
	EVT_MENU(MenuBarID::reset, GUI::OnMenuReset)
	EVT_MENU(MenuBarID::stop, GUI::OnMenuStop)
	EVT_MENU(MenuBarID::size_1x, GUI::OnMenuSize)
	EVT_MENU(MenuBarID::size_2x, GUI::OnMenuSize)
	EVT_MENU(MenuBarID::size_4x, GUI::OnMenuSize)
	EVT_MENU(MenuBarID::size_6x, GUI::OnMenuSize)
	EVT_MENU(MenuBarID::size_8x, GUI::OnMenuSize)
	EVT_MENU(MenuBarID::size_10x, GUI::OnMenuSize)
	EVT_MENU(MenuBarID::size_12x, GUI::OnMenuSize)
	EVT_MENU(MenuBarID::size_15x, GUI::OnMenuSize)
	EVT_MENU(MenuBarID::size_custom, GUI::OnMenuSize)
	EVT_MENU(MenuBarID::size_maximized, GUI::OnMenuSize)
	EVT_MENU(MenuBarID::size_fullscreen, GUI::OnMenuSize)
	EVT_MENU(MenuBarID::speed_50, GUI::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_75, GUI::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_100, GUI::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_125, GUI::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_150, GUI::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_200, GUI::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_300, GUI::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_500, GUI::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_uncapped, GUI::OnMenuSpeed)
	EVT_MENU(MenuBarID::speed_custom, GUI::OnMenuSpeed)
	EVT_MENU(MenuBarID::colour00, GUI::OnMenuColour)
	EVT_MENU(MenuBarID::colour01, GUI::OnMenuColour)
	EVT_MENU(MenuBarID::colour10, GUI::OnMenuColour)
	EVT_MENU(MenuBarID::colour11, GUI::OnMenuColour)
	EVT_MENU(MenuBarID::colour_generate_random_palette, GUI::OnMenuColourSetRandomPalette)
	EVT_MENU(MenuBarID::colour_generate_random_palette_on_every_boot, GUI::OnMenuColourToggleRandomPalette)
	EVT_MENU(MenuBarID::input, GUI::OnMenuInput)
	EVT_MENU(MenuBarID::toggle_boot_rom, GUI::OnMenuToggleBootRom)
	EVT_MENU(MenuBarID::toggle_filter_gb_gc_files, GUI::OnMenuToggleFilterFiles)
	EVT_MENU(MenuBarID::toggle_render_on_every_system_frame, GUI::OnMenuToggleRenderEveryFrame)
	EVT_MENU(MenuBarID::reset_settings, GUI::OnMenuResetSettings)
	EVT_MENU(MenuBarID::github_link, GUI::OnMenuGitHubLink)

	EVT_LISTBOX_DCLICK(listBoxID, GUI::OnListBoxGameSelection)
	EVT_SIZE(GUI::OnWindowSizeChanged)
	EVT_KEY_DOWN(GUI::OnKeyDown)
	EVT_CLOSE(GUI::OnClose)
wxEND_EVENT_TABLE()


#define PRESPECIFIED_SPEED_NOT_FOUND -1
#define PRESPECIFIED_SIZE_NOT_FOUND -1


const wxString GUI::emulator_name("gb-chan");


GUI::GUI() : wxFrame(nullptr, frameID, emulator_name, wxDefaultPosition, wxDefaultSize)
{
	SetupInitialMenuView();
	SwitchToMenuView();

	SetupConfig();
	ApplyGUISettings();

	emulator.gui = this;

	SetupSDL();
}


GUI::~GUI()
{
	delete game_list_box;
	delete SDL_window_panel;
}


void GUI::SetupInitialMenuView()
{
	SetClientSize(default_window_size);
	CreateMenuBar();
	default_client_size = GetClientSize(); // what remains of the window outside of the menubar

	game_list_box = new wxListBox(this, listBoxID, wxPoint(0, 0), default_client_size);
	SDL_window_panel = new wxPanel(this, SDLWindowID, wxPoint(0, 0), default_client_size);

	game_list_box->Bind(wxEVT_KEY_DOWN, &GUI::OnKeyDown, this);
	SDL_window_panel->Bind(wxEVT_KEY_DOWN, &GUI::OnKeyDown, this);
}


void GUI::SwitchToMenuView()
{
	SDL_window_panel->Hide();
	game_list_box->Show();
	SetClientSize(default_client_size);
	UpdateWindowLabel(false);
	menu_emulation->SetLabel(MenuBarID::pause_play, wxT("&Pause"));
	game_view_active = false;
}


void GUI::SwitchToGameView()
{
	game_list_box->Hide();
	SDL_window_panel->Show();
	SDL_window_panel->SetFocus();
	wxSize SDL_window_size = emulator.GetWindowSize();
	SDL_window_panel->SetSize(SDL_window_size);
	SetClientSize(SDL_window_size);
	UpdateWindowLabel(true);
	game_view_active = true;
}


void GUI::SetupConfig()
{
	config.AddConfigurable(this);
	config.AddConfigurable(&emulator);
	config.AddConfigurable(&emulator.joypad);
	config.AddConfigurable(&emulator.ppu);

	if (config.ConfigFileExists())
		config.Load();
	else
		config.SetDefaults();
}


void GUI::LaunchGame()
{
	if (!wxFileExists(active_rom_path))
	{
		wxMessageBox(wxString::Format("Error opening game; file \"%s\" does not exist.", active_rom_path));
		return;
	}

	if (!SDL_initialized)
	{
		bool success = SetupSDL();
		SDL_initialized = success;
		if (!success)
			return;
	}

	if (generate_random_colour_palette_on_every_boot)
		GenerateRandomColourPalette();

	SwitchToGameView();
	emulator.StartGame(active_rom_path);
}


void GUI::QuitGame()
{
	emulator.Stop();
	SwitchToMenuView();
}


void GUI::LoadConfig(std::ifstream& ifs)
{
	// read wxString 'rom_folder_path' from config.bin by first reading its length and then its actual data
	// todo: I would like to use char16_t instead of wchar_t here, but not sure how to safely convert between wxString and char16_t
	size_t str_len = 0; // length in wide characters, not bytes
	ifs.read((char*)&str_len, sizeof(str_len));
	if (str_len > maximum_rom_path_length)
	{
		wxMessageBox(wxString::Format("Error: cannot read config file; rom directory is longer than the maximum allowed %i characters.", maximum_rom_path_length));
		rom_folder_path = default_rom_folder_path;
		return;
	}
	wchar_t str[maximum_rom_path_length]{};
	ifs.read((char*)str, str_len * sizeof(wchar_t));
	rom_folder_path = wxString(str);

	ifs.read((char*)&display_only_gb_gbc_files, sizeof(bool));
	ifs.read((char*)&generate_random_colour_palette_on_every_boot, sizeof(bool));
}


void GUI::SaveConfig(std::ofstream& ofs)
{
	const wchar_t* str = rom_folder_path.wc_str();
	size_t str_len = std::wcslen(str);
	ofs.write((char*)&str_len, sizeof(str_len));
	ofs.write((char*)str, str_len * sizeof(wchar_t));

	ofs.write((char*)&display_only_gb_gbc_files, sizeof(bool));
	ofs.write((char*)&generate_random_colour_palette_on_every_boot, sizeof(bool));
}


void GUI::SetDefaultConfig()
{
	rom_folder_path = default_rom_folder_path;
	display_only_gb_gbc_files = default_display_only_gb_gbc_files;
	generate_random_colour_palette_on_every_boot = default_generate_random_colour_palette_on_every_boot;
}


// returns true if successful
bool GUI::SetupSDL()
{
	if (SDL_Init(SDL_INIT_EVERYTHING) == -1)
	{
		wxMessageBox("Could not initialise SDL!");
		return false;
	}

	const void* window_handle = (void*)SDL_window_panel->GetHandle();
	bool success = emulator.SetupSDLVideo(window_handle);
	SDL_initialized = success;
	return success;
}


void GUI::CreateMenuBar()
{
	menu_bar->Append(menu_file, wxT("&File"));
	menu_bar->Append(menu_emulation, wxT("&Emulation"));
	menu_bar->Append(menu_settings, wxT("&Settings"));
	menu_bar->Append(menu_info, wxT("&Info"));

	menu_file->Append(MenuBarID::open_rom, wxT("&Open rom"));
	menu_file->Append(MenuBarID::set_game_dir, wxT("&Set game directory"));
	menu_file->Append(MenuBarID::set_dmg_boot_rom_path, wxT("&Set GB boot rom path"));
	menu_file->Append(MenuBarID::set_cgb_boot_rom_path, wxT("&Set GBC boot rom path"));
	menu_file->AppendSeparator();
	menu_file->Append(MenuBarID::save_state, wxT("&Save state (F5)"));
	menu_file->Append(MenuBarID::load_state, wxT("&Load state (F8)"));
	menu_file->AppendSeparator();
	menu_file->Append(MenuBarID::quit, wxT("&Quit"));

	menu_emulation->Append(MenuBarID::pause_play, wxT("&Pause"));
	menu_emulation->Append(MenuBarID::reset, wxT("&Reset"));
	menu_emulation->Append(MenuBarID::stop, wxT("&Stop"));

	menu_size->AppendRadioItem(MenuBarID::size_1x, format_size_menubar_label(1));
	menu_size->AppendRadioItem(MenuBarID::size_2x, format_size_menubar_label(2));
	menu_size->AppendRadioItem(MenuBarID::size_4x, format_size_menubar_label(4));
	menu_size->AppendRadioItem(MenuBarID::size_6x, format_size_menubar_label(6));
	menu_size->AppendRadioItem(MenuBarID::size_8x, format_size_menubar_label(8));
	menu_size->AppendRadioItem(MenuBarID::size_10x, format_size_menubar_label(10));
	menu_size->AppendRadioItem(MenuBarID::size_12x, format_size_menubar_label(12));
	menu_size->AppendRadioItem(MenuBarID::size_15x, format_size_menubar_label(15));
	menu_size->AppendRadioItem(MenuBarID::size_custom, wxT("&Custom"));
	menu_size->Append(MenuBarID::size_maximized, wxT("&Toggle maximized window"));
	menu_size->AppendSeparator();
	menu_size->Append(MenuBarID::size_fullscreen, wxT("&Toggle fullscreen (F11)"));
	menu_settings->AppendSubMenu(menu_size, wxT("&Window size"));

	menu_speed->AppendRadioItem(MenuBarID::speed_50, format_speed_menubar_label(50));
	menu_speed->AppendRadioItem(MenuBarID::speed_75, format_speed_menubar_label(75));
	menu_speed->AppendRadioItem(MenuBarID::speed_100, format_speed_menubar_label(100));
	menu_speed->AppendRadioItem(MenuBarID::speed_125, format_speed_menubar_label(125));
	menu_speed->AppendRadioItem(MenuBarID::speed_150, format_speed_menubar_label(150));
	menu_speed->AppendRadioItem(MenuBarID::speed_200, format_speed_menubar_label(200));
	menu_speed->AppendRadioItem(MenuBarID::speed_300, format_speed_menubar_label(300));
	menu_speed->AppendRadioItem(MenuBarID::speed_500, format_speed_menubar_label(500));
	menu_speed->AppendRadioItem(MenuBarID::speed_uncapped, wxT("&Uncapped"));
	menu_speed->AppendRadioItem(MenuBarID::speed_custom, wxT("&Custom"));
	menu_settings->AppendSubMenu(menu_speed, wxT("&Emulation speed"));

	menu_colour->Append(MenuBarID::colour00, wxT("&Set colour (00)"));
	menu_colour->Append(MenuBarID::colour01, wxT("&Set colour (01)"));
	menu_colour->Append(MenuBarID::colour10, wxT("&Set colour (10)"));
	menu_colour->Append(MenuBarID::colour11, wxT("&Set colour (11)"));

	// append menu items for the palettes listed in Themes.h
	int i = 0;
	for (const Themes::DMG_Palette& palette : Themes::palette_vec)
	{
		wxString palette_name = palette.first;
		menu_colour_palette->Append(MenuBarID::colour_palette_base + i, palette_name);
		Bind(wxEVT_MENU, &GUI::OnMenuColour, this, MenuBarID::colour_palette_base + i);
		i++;
	}

	menu_colour->AppendSubMenu(menu_colour_palette, wxT("&Palettes"));
	menu_colour->AppendSeparator();
	menu_colour->Append(MenuBarID::colour_generate_random_palette, wxT("&Generate random palette"));
	menu_colour->AppendCheckItem(MenuBarID::colour_generate_random_palette_on_every_boot, wxT("&Generate random palette on every game boot"));
	menu_settings->AppendSubMenu(menu_colour, wxT("&Colour scheme"));

	menu_settings->Append(MenuBarID::input, wxT("&Configure input bindings"));

	menu_settings->AppendSeparator();
	menu_settings->AppendCheckItem(MenuBarID::toggle_boot_rom, wxT("&Play boot rom before starting game"));
	menu_settings->AppendCheckItem(MenuBarID::toggle_filter_gb_gc_files, wxT("&Filter game list to .gb and .gbc files only"));
	menu_settings->AppendCheckItem(MenuBarID::toggle_render_on_every_system_frame, wxT("&Render graphics on every system frame"));

	menu_settings->AppendSeparator();
	menu_settings->Append(MenuBarID::reset_settings, wxT("&Reset settings"));

	menu_info->Append(MenuBarID::github_link, wxT("&GitHub link"));

	SetMenuBar(menu_bar);
}


// fill the game list with the games in specified rom directory
void GUI::SetupGameList()
{
	rom_folder_name_arr.Clear();
	rom_folder_path_arr.Clear();
	game_list_box->Clear();

	size_t num_files = wxDir::GetAllFiles(rom_folder_path, &rom_folder_path_arr, wxEmptyString, wxDIR_FILES);
	
	// If the option to filter out all files that do not have extensions .gb or .gbc have been enabled, remove these files from 'rom_folder_path_arr'
	// The below way of doing this is not particularly efficient. It is possible to specify to wxDir::GetAllFiles() that only files of a certain 
	// extension to be collected. However, we need two extensions (.gb, .gbc), which does not seem possible to specify.
	if (menu_settings->IsChecked(MenuBarID::toggle_filter_gb_gc_files))
	{
		wxArrayString rom_folder_path_arr_temp = wxArrayString();
		for (size_t i = 0; i < num_files; i++)
		{
			wxString file_ext = wxFileName(rom_folder_path_arr[i]).GetExt();
			if (file_ext.IsSameAs("gb") || file_ext.IsSameAs("gbc"))
				rom_folder_path_arr_temp.Add(rom_folder_path_arr[i]);
		}
		rom_folder_path_arr = rom_folder_path_arr_temp;
		num_files = rom_folder_path_arr.GetCount();
	}
	
	if (num_files == 0)
	{
		game_list_box->InsertItems(1, &empty_listbox_item, 0);
		return;
	}

	for (size_t i = 0; i < num_files; i++)
		rom_folder_name_arr.Add(wxFileName(rom_folder_path_arr[i]).GetFullName());
	game_list_box->InsertItems(rom_folder_name_arr, 0);
}


void GUI::ApplyGUISettings()
{
	menu_settings->Check(MenuBarID::toggle_boot_rom, emulator.load_boot_rom);
	menu_settings->Check(MenuBarID::toggle_filter_gb_gc_files, display_only_gb_gbc_files);
	menu_settings->Check(MenuBarID::toggle_render_on_every_system_frame, emulator.render_graphics_every_system_frame);
	menu_colour->Check(MenuBarID::colour_generate_random_palette_on_every_boot, generate_random_colour_palette_on_every_boot);

	SetupGameList();

	unsigned scale = emulator.ppu.scale;
	int menu_id = get_id_of_size_menubar_item(scale);
	if (menu_id == wxNOT_FOUND)
	{
		menu_size->Check(MenuBarID::size_custom, true);
		menu_size->SetLabel(MenuBarID::size_custom, format_custom_size_menubar_label(scale));
	}
	else
	{
		menu_size->Check(menu_id, true);
	}

	if (emulator.emulation_speed_uncapped)
	{
		menu_speed->Check(MenuBarID::speed_uncapped, true);
	}
	else
	{
		unsigned speed = emulator.emulation_speed;
		menu_id = get_id_of_speed_menubar_item(speed);
		if (menu_id == wxNOT_FOUND)
		{
			menu_speed->Check(MenuBarID::speed_custom, true);
			menu_speed->SetLabel(MenuBarID::speed_custom, format_custom_speed_menubar_label(speed));
		}
		else
		{
			menu_speed->Check(menu_id, true);
		}
	}
}


void GUI::Quit()
{
	QuitGame();
	SDL_Quit();
	Close();
}


void GUI::UpdateWindowLabel(bool gameIsRunning)
{
	wxSize window_size = GetClientSize();
	int scale = std::min(window_size.GetWidth() / 160, window_size.GetHeight() / 144);

	if (gameIsRunning)
		this->SetLabel(wxString::Format("%s | %s | %ix%i | FPS: %i",
			emulator_name, wxFileName(active_rom_path).GetName(), 160 * scale, 144 * scale, fps));
	else
		this->SetLabel(emulator_name);
}


void GUI::GenerateRandomColourPalette()
{
	for (int i = 0; i < 4; i++)
	{
		SDL_Color col = { rand() & 0xFF, rand() & 0xFF, rand() & 0xFF, 0 };
		emulator.SetColour(i, col);
	}
	config.Save();
}


void GUI::OnMenuOpen(wxCommandEvent& event)
{
	// Creates an "open file" dialog
	wxFileDialog* fileDialog = new wxFileDialog(
		this, "Choose a file to open", wxEmptyString,
		wxEmptyString, "GB and GBC files (*.gb;*.gbc)|*.gb;*.gbc|All files (*.*)|*.*",
		wxFD_OPEN | wxFD_FILE_MUST_EXIST, wxDefaultPosition);

	int buttonPressed = fileDialog->ShowModal(); // show the dialog
	wxString selectedPath = fileDialog->GetPath();
	fileDialog->Destroy();

	// if the user clicks "Open" instead of "Cancel"
	if (buttonPressed == wxID_OK)
	{
		active_rom_path = selectedPath;
		LaunchGame();
	}
}


void GUI::OnMenuSetGameDir(wxCommandEvent& event)
{
	ChooseGameDirDialog();
}


void GUI::OnMenuSetDMGBootRom(wxCommandEvent& event)
{
	wxFileDialog* fileDialog = new wxFileDialog(
		this, "Choose a file to open", wxEmptyString,
		wxEmptyString, "GB files (*.gb)|*.gb|All files (*.*)|*.*",
		wxFD_OPEN | wxFD_FILE_MUST_EXIST, wxDefaultPosition);

	int buttonPressed = fileDialog->ShowModal();
	wxString selectedPath = fileDialog->GetPath();
	fileDialog->Destroy();

	if (buttonPressed == wxID_OK)
	{
		emulator.dmg_boot_path = selectedPath;
		config.Save();
	}
}


void GUI::OnMenuSetCGBBootRom(wxCommandEvent& event)
{
	wxFileDialog* fileDialog = new wxFileDialog(
		this, "Choose a file to open", wxEmptyString,
		wxEmptyString, "GBC files (*.gbc)|*.gbc|All files (*.*)|*.*",
		wxFD_OPEN | wxFD_FILE_MUST_EXIST, wxDefaultPosition);

	int buttonPressed = fileDialog->ShowModal();
	wxString selectedPath = fileDialog->GetPath();
	fileDialog->Destroy();

	if (buttonPressed == wxID_OK)
	{
		emulator.cgb_boot_path = selectedPath;
		config.Save();
	}
}


void GUI::ChooseGameDirDialog()
{
	// Creates a "select directory" dialog
	wxDirDialog* dirDialog = new wxDirDialog(this, "Choose a directory",
		wxEmptyString, wxDD_DIR_MUST_EXIST, wxDefaultPosition);

	int buttonPressed = dirDialog->ShowModal();
	wxString selectedPath = dirDialog->GetPath();
	dirDialog->Destroy();

	if (buttonPressed == wxID_OK)
	{
		// set the chosen directory
		rom_folder_path = selectedPath;
		SetupGameList();
		config.Save();
	}
}


void GUI::OnMenuSaveState(wxCommandEvent& event)
{
	if (emulator.emu_is_running)
		emulator.SaveState();
	else
		wxMessageBox("No game is loaded. Cannot save state.");
}


void GUI::OnMenuLoadState(wxCommandEvent& event)
{
	if (emulator.emu_is_running)
		emulator.LoadState();
	else
		wxMessageBox("No game is loaded. Cannot load a state.");
}


void GUI::OnMenuQuit(wxCommandEvent& event)
{
	Quit();
}


void GUI::OnMenuPausePlay(wxCommandEvent& event)
{
	if (emulator.emu_is_paused)
	{
		menu_emulation->SetLabel(MenuBarID::pause_play, wxT("&Pause"));
		emulator.Resume();
	}
	else if (emulator.emu_is_running)
	{
		menu_emulation->SetLabel(MenuBarID::pause_play, wxT("&Resume"));
		emulator.Pause();
	}
}


void GUI::OnMenuReset(wxCommandEvent& event)
{
	if (emulator.emu_is_running)
	{
		menu_emulation->SetLabel(MenuBarID::pause_play, wxT("&Pause"));
		emulator.Reset();
	}
}


void GUI::OnMenuStop(wxCommandEvent& event)
{
	QuitGame();
}


void GUI::OnMenuSize(wxCommandEvent& event)
{
	int id = event.GetId();
	int scale = GetSizeFromMenuBarID(id);
	
	if (scale != PRESPECIFIED_SIZE_NOT_FOUND)
	{
		if (menu_size->IsEnabled(MenuBarID::size_custom))
			menu_size->SetLabel(MenuBarID::size_custom, "Custom");
	}
	else if (id == MenuBarID::size_custom)
	{
		wxTextEntryDialog* textEntryDialog = new wxTextEntryDialog(this,
			"The resolution will be 160s x 144s, where s is the scale.", "Enter a scale (positive integer).",
			wxEmptyString, wxTextEntryDialogStyle, wxDefaultPosition);
		textEntryDialog->SetTextValidator(wxFILTER_DIGITS);

		int buttonPressed = textEntryDialog->ShowModal();
		wxString input = textEntryDialog->GetValue();
		textEntryDialog->Destroy();

		if (buttonPressed == wxID_CANCEL)
		{
			// at this time, the 'custom' option has been checked in the menubar
			// calling the below function restores the menu to what is was before the event was propogated
			ApplyGUISettings(); 
			return;
		}

		scale = wxAtoi(input); // string -> int
		if (scale <= 0) // input converted to int
		{
			wxMessageBox("Please enter a valid scale value (> 0).");
			ApplyGUISettings(); 
			return;
		}
		menu_size->SetLabel(MenuBarID::size_custom, format_custom_size_menubar_label(scale));
	}
	else if (id == MenuBarID::size_maximized)
	{
		if (this->IsMaximized())
			this->Restore();
		else
			this->Maximize(true);
		return;
	}
	else // fullscreen
	{
		ToggleFullScreen();
		return;
	}

	emulator.ppu.scale = scale;
	config.Save();

	if (emulator.emu_is_running)
		SetClientSize(emulator.GetWindowSize());
}


void GUI::OnMenuSpeed(wxCommandEvent& event)
{
	// Reset the label of the 'custom' speed option
	if (menu_speed->IsEnabled(MenuBarID::speed_custom))
		menu_speed->SetLabel(MenuBarID::speed_custom, "Custom");

	int id = event.GetId();
	int speed = GetSpeedFromMenuBarID(id);

	if (speed != PRESPECIFIED_SPEED_NOT_FOUND)
	{
		emulator.SetEmulationSpeed(speed);
		emulator.emulation_speed_uncapped = false;
	}
	else if (id == MenuBarID::speed_uncapped)
	{
		emulator.emulation_speed_uncapped = true;
	}
	else // custom speed
	{
		wxTextEntryDialog* textEntryDialog = new wxTextEntryDialog(this,
			"This will be the emulation speed in %.", "Enter a positive integer.",
			wxEmptyString, wxTextEntryDialogStyle, wxDefaultPosition);
		textEntryDialog->SetTextValidator(wxFILTER_DIGITS);

		int buttonPressed = textEntryDialog->ShowModal();
		wxString input = textEntryDialog->GetValue();
		textEntryDialog->Destroy();

		if (buttonPressed == wxID_CANCEL)
		{
			// at this time, the 'custom' option has been checked in the menubar
			// calling the below function restores the menu to what is was before the event was propogated
			ApplyGUISettings();
			return;
		}

		speed = wxAtoi(input);
		if (speed <= 0)
		{
			wxMessageBox("Please enter a valid speed value (> 0).");
			ApplyGUISettings();
			return;
		}
		emulator.SetEmulationSpeed(speed);
		emulator.emulation_speed_uncapped = false;
		menu_speed->SetLabel(MenuBarID::speed_custom, format_custom_speed_menubar_label(speed));
	}

	config.Save();
}


void GUI::OnMenuColour(wxCommandEvent& event)
{
	int id = event.GetId();
	if (id == MenuBarID::colour00 || id == MenuBarID::colour01 ||
		id == MenuBarID::colour10 || id == MenuBarID::colour11)
	{
		wxColourDialog* colourDialog = new wxColourDialog(this, NULL);
		int buttonPressed = colourDialog->ShowModal();
		wxColourData colourData = colourDialog->GetColourData();
		colourDialog->Destroy();

		if (buttonPressed == wxID_CANCEL)
			return;

		wxColour colour = colourData.GetColour();
		id -= MenuBarID::colour00;
		emulator.SetColour(id, colour);
	}
	else // predefined color palette
	{
		id -= MenuBarID::colour_palette_base;
		std::array<SDL_Color, 4> chosen_palette = Themes::palette_vec[id].second;
		for (int i = 0; i < 4; i++)
			emulator.SetColour(i, chosen_palette[i]);
	}
	config.Save();
}


void GUI::OnMenuColourSetRandomPalette(wxCommandEvent& event)
{
	GenerateRandomColourPalette();
}


void GUI::OnMenuColourToggleRandomPalette(wxCommandEvent& event)
{
	generate_random_colour_palette_on_every_boot = menu_colour->IsChecked(MenuBarID::colour_generate_random_palette_on_every_boot);
	config.Save();
}


void GUI::OnMenuInput(wxCommandEvent& event)
{
	if (!input_window_active)
		input_bindings_window = new InputBindingsWindow(this, &config, &emulator.joypad, &input_window_active);
	input_bindings_window->Show();
}

void GUI::OnMenuToggleBootRom(wxCommandEvent& event)
{
	emulator.load_boot_rom = menu_settings->IsChecked(MenuBarID::toggle_boot_rom);
	config.Save();
}


void GUI::OnMenuToggleFilterFiles(wxCommandEvent& event)
{
	SetupGameList();
	display_only_gb_gbc_files = menu_settings->IsChecked(MenuBarID::toggle_filter_gb_gc_files);;
	config.Save();
}


void GUI::OnMenuToggleRenderEveryFrame(wxCommandEvent& event)
{
	emulator.render_graphics_every_system_frame = menu_settings->IsChecked(MenuBarID::toggle_render_on_every_system_frame);
	config.Save();
}


void GUI::OnMenuResetSettings(wxCommandEvent& event)
{
	config.SetDefaults();
	ApplyGUISettings();
	if (emulator.emu_is_running)
		SetClientSize(emulator.GetWindowSize());
}


void GUI::OnMenuGitHubLink(wxCommandEvent& event)
{
	wxLaunchDefaultBrowser("https://github.com/TesseracT33/gb-chan", wxBROWSER_NEW_WINDOW);
}


void GUI::OnListBoxGameSelection(wxCommandEvent& event)
{
	LocateAndOpenRomFromListBoxSelection(event.GetString());
}


void GUI::LocateAndOpenRomFromListBoxSelection(wxString selection)
{
	if (rom_folder_path_arr.GetCount() == 0) // when the only item in the listbox is the 'empty_listbox_item'
	{
		ChooseGameDirDialog();
		return;
	}

	// from the selection (rom name; name.ext), get the full path of the file from array `romFolderGamePaths'
	for (const wxString& rom_path : rom_folder_path_arr)
	{
		if (wxFileNameFromPath(rom_path).IsSameAs(selection))
		{
			active_rom_path = rom_path;
			LaunchGame();
			return;
		}
	}

	wxMessageBox(wxString::Format("Error opening game; could not locate file \"%s\".", selection));
}


void GUI::OnWindowSizeChanged(wxSizeEvent& event)
{
	UpdateViewSizing();
}


void GUI::OnClose(wxCloseEvent& event)
{
	QuitGame();
	SDL_Quit();
	Destroy();
}


void GUI::OnKeyDown(wxKeyEvent& event)
{
	int keycode = event.GetKeyCode();

	// key presses are handled differently depending on if we are in the game list box selection view or playing a game
	if (game_list_box->HasFocus())
	{
		switch (keycode)
		{
		case WXK_RETURN:
		{
			wxString selection = game_list_box->GetStringSelection();
			if (selection != wxEmptyString)
				LocateAndOpenRomFromListBoxSelection(selection);
			break;
		}

		case WXK_DOWN:
		{
			int selection = game_list_box->GetSelection();
			game_list_box->SetSelection(std::min(selection + 1, (int)game_list_box->GetCount()));
			break;
		}

		case WXK_UP:
		{
			int selection = game_list_box->GetSelection();
			game_list_box->SetSelection(std::max(selection - 1, 0));
			break;
		}
		}
	}

	else if (emulator.emu_is_running)
	{
		switch (keycode)
		{
		case wx_keycode_save_state:
			emulator.SaveState();
			break;

		case wx_keycode_load_state:
			emulator.LoadState();
			break;

		case wx_keycode_fullscreen:
			ToggleFullScreen();
			break;
		}
	}
}


void GUI::ToggleFullScreen()
{
	if (!emulator.emu_is_running) return;

	this->ShowFullScreen(!full_screen_active, wxFULLSCREEN_ALL);
	full_screen_active = !full_screen_active;
}


// called when the window is resized by the user, to update the sizes of all UI elements
void GUI::UpdateViewSizing()
{
	wxSize size = GetClientSize();
	int width = size.GetWidth();
	int height = size.GetHeight();

	if (game_view_active)
	{
		SDL_window_panel->SetSize(size);
		emulator.SetWindowSize(size);
	}
	else
	{
		// The reason for the below nullptr check is that this function will get called automatically by wxwidgets before some child elements have been initialized
		// However, they will be initialized if game_view_active == true
		if (game_list_box != nullptr)
			game_list_box->SetSize(size);
	}
}


void GUI::UpdateFPSLabel(int fps)
{
	this->fps = fps;
	UpdateWindowLabel(true);
}


int GUI::get_id_of_size_menubar_item(int scale) const
{
	wxString item_string = format_size_menubar_label(scale);
	int menu_id = menu_size->FindItem(item_string);
	return menu_id;
}


int GUI::get_id_of_speed_menubar_item(int speed) const
{
	wxString item_string = format_speed_menubar_label(speed);
	int menu_id = menu_speed->FindItem(item_string);
	return menu_id;
}


wxString GUI::format_size_menubar_label(int scale) const
{
	// format is '160x144 (1x)'
	int width = scale * 160, height = scale * 144;
	return wxString::Format("%ix%i (%ix)", width, height, scale);
}


wxString GUI::format_speed_menubar_label(int speed) const
{
	// format is '100 %'
	return wxString::Format("%i %%", speed);
}


wxString GUI::format_custom_size_menubar_label(int scale) const
{
	// format is 'Custom (1x)'
	return wxString::Format("Custom (%ix)", scale);
}


wxString GUI::format_custom_speed_menubar_label(int speed) const
{
	// format is 'Custom (100 %)'
	return wxString::Format("Custom (%i %%)", speed);
}


int GUI::GetSizeFromMenuBarID(int id) const
{
	switch (id)
	{
	case MenuBarID::size_1x : return 1;
	case MenuBarID::size_2x : return 2;
	case MenuBarID::size_4x : return 4;
	case MenuBarID::size_6x : return 6;
	case MenuBarID::size_8x : return 8;
	case MenuBarID::size_10x: return 10;
	case MenuBarID::size_12x: return 12;
	case MenuBarID::size_15x: return 15;
	default: return PRESPECIFIED_SIZE_NOT_FOUND;
	}
}


int GUI::GetSpeedFromMenuBarID(int id) const
{
	switch (id)
	{
	case MenuBarID::speed_50:  return 50;
	case MenuBarID::speed_75:  return 75;
	case MenuBarID::speed_100: return 100;
	case MenuBarID::speed_125: return 125;
	case MenuBarID::speed_150: return 150;
	case MenuBarID::speed_200: return 200;
	case MenuBarID::speed_300: return 300;
	case MenuBarID::speed_500: return 500;
	default: return PRESPECIFIED_SPEED_NOT_FOUND;
	}
}