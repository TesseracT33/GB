#include "Emulator.h"

Emulator::Emulator()
{
	ConnectSystemComponents();
	SetupSerializableComponents();
}


Emulator::~Emulator()
{
	if (emu_is_running)
		cartridge.EjectCartridge();
}


void Emulator::ConnectSystemComponents()
{
	// connect all core components...
	apu.bus = &bus;

	cpu.bus = &bus;

	dma.bus = &bus;
	dma.cpu = &cpu;

	ppu.cpu = &cpu;
	ppu.dma = &dma;
	ppu.bus = &bus;

	joypad.cpu = &cpu;
	joypad.bus = &bus;

	bus.apu = &apu;
	bus.cartridge = &cartridge;
	bus.cpu = &cpu;
	bus.dma = &dma;
	bus.joypad = &joypad;
	bus.ppu = &ppu;
	bus.serial = &serial;
	bus.timer = &timer;

	serial.bus = &bus;
	serial.cpu = &cpu;

	timer.apu = &apu;
	timer.bus = &bus;
	timer.cpu = &cpu;

	// components with pointers to other components are supplied with these here...
	apu.Initialize();
	cpu.Initialize();
	joypad.Initialize();
	ppu.Initialize();
	serial.Initialize();
	timer.Initialize();
}


void Emulator::SetupSerializableComponents()
{
	serializable_components.push_back(&apu);
	serializable_components.push_back(&bus);
	serializable_components.push_back(&cartridge);
	serializable_components.push_back(&cpu);
	serializable_components.push_back(&dma);
	serializable_components.push_back(&ppu);
	serializable_components.push_back(&serial);
	serializable_components.push_back(&timer);
	serializable_components.push_back(this);
}


void Emulator::StartGame(wxString rom_path)
{
	if (emu_is_running)
	{
		cartridge.EjectCartridge();
	}

	cartridge.Reset();
	System::mode = cartridge.LoadCartridge(rom_path);
	if (System::mode == System::Mode::NONE) return;
	this->rom_path = rom_path;

	bool boot_rom_loaded = false;
	if (load_boot_rom)
	{
		std::string boot_path;
		switch (System::mode)
		{
		case System::Mode::DMG: boot_path = dmg_boot_path; break;
		case System::Mode::CGB: boot_path = cgb_boot_path; break;
		}

		boot_rom_loaded = bus.LoadBootRom(boot_path); // returns true only on success
	}

	// should always be reset before all other components below (e.g. ppu.reset() depends on bus being reset)
	// todo: fix so that this isn't the case?
	bus.Reset(boot_rom_loaded);

	apu.Reset();
	cpu.Reset(boot_rom_loaded);
	dma.Reset();
	joypad.Reset();
	ppu.Reset();
	serial.Reset();
	timer.Reset();

	MainLoop();
}


void Emulator::MainLoop()
{
	emu_is_running = true;
	emu_is_paused = false;

	long long microseconds_since_fps_update = 0; // how many microseconds since the fps window label was updated
	int frames_since_fps_update = 0; // same as above but no. of frames

	while (emu_is_running && !emu_is_paused)
	{
		auto frame_start_t = std::chrono::steady_clock::now();

		m_cycle_counter = 0;
		while (m_cycle_counter++ < m_cycles_per_frame)
		{
			// apu, ppu and timer is updated each t-cycle (see e.g. ppu.Update()), rest each m-cycle
			apu.Update();
			cpu.Update();
			dma.Update();
			ppu.Update();
			serial.Update();
			timer.Update();
		}
		joypad.PollInput();

		if (load_state_on_next_cycle)
			LoadState();
		else if (save_state_on_next_cycle)
			SaveState();

		auto frame_end_t = std::chrono::steady_clock::now();
		long long microseconds_elapsed_this_frame = std::chrono::duration_cast<std::chrono::microseconds>(frame_end_t - frame_start_t).count();
		
		if (!emulation_speed_uncapped)
		{
			// put thread to sleep if more than 2 ms remain of the frame. sleep until 2 ms remain
			//if (microseconds_elapsed_this_frame < microseconds_per_frame - 2000)
				//std::this_thread::sleep_for(std::chrono::microseconds(microseconds_per_frame - microseconds_elapsed_this_frame - 2000));
			// for the rest of the frame, do busy waiting
			while (microseconds_elapsed_this_frame < microseconds_per_frame)
			{
				joypad.PollInput();
				microseconds_elapsed_this_frame = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - frame_start_t).count();
			}
		}

		// update fps on window title if it is time to do so (updated once every second)
		frame_end_t = std::chrono::steady_clock::now();
		microseconds_elapsed_this_frame = std::chrono::duration_cast<std::chrono::microseconds>(frame_end_t - frame_start_t).count();
		frames_since_fps_update++;
		microseconds_since_fps_update += microseconds_elapsed_this_frame;
		if (microseconds_since_fps_update >= 1000000 && emu_is_running)
		{
			gui->UpdateFPSLabel(frames_since_fps_update);
			frames_since_fps_update = 0;
			microseconds_since_fps_update -= 1000000;
		}
	}
}


void Emulator::Pause()
{
	emu_is_paused = true;
}


void Emulator::Reset()
{
	emu_is_running = false;
	StartGame(this->rom_path);
}


void Emulator::Resume()
{
	if (emu_is_running)
		MainLoop();
}


void Emulator::Stop()
{
	if (emu_is_running)
		cartridge.EjectCartridge();
	emu_is_running = false;
}


void Emulator::LoadState()
{
	if (!load_state_on_next_cycle)
	{
		load_state_on_next_cycle = true;
		return;
	}

	std::ifstream ifs(save_state_path, std::ifstream::in);
	if (!ifs)
	{
		wxMessageBox("Save state does not exist or could not be opened.");
		load_state_on_next_cycle = false;
		return;
	}

	for (Serializable* serializable : serializable_components)
		serializable->Deserialize(ifs);

	ifs.close();
	load_state_on_next_cycle = false;
}


void Emulator::SaveState()
{
	if (!save_state_on_next_cycle)
	{
		save_state_on_next_cycle = true;
		return;
	}

	std::ofstream ofs(save_state_path, std::ofstream::out | std::fstream::trunc);
	if (!ofs)
	{
		wxMessageBox("Save state could not be created.");
		save_state_on_next_cycle = false;
		return;
	}

	for (Serializable* serializable : serializable_components)
		serializable->Serialize(ofs);

	ofs.close();
	save_state_on_next_cycle = false;
}

bool Emulator::SetupSDLVideo(const void* window_handle)
{
	bool success = ppu.CreateRenderer(window_handle);
	return success;
}


void Emulator::SetWindowSize(unsigned width, unsigned height)
{
	if (width > 0 && height > 0)
	{
		ppu.SetWindowSize(width, height);
	}
}


void Emulator::SetWindowSize(wxSize size)
{
	SetWindowSize(size.GetWidth(), size.GetHeight());
}


void Emulator::SetColour(int id, SDL_Color colour)
{
	if (id >= 0 || id <= 3)
	{
		ppu.SetColour(id, colour);
	}
}


void Emulator::SetColour(int id, wxColour colour)
{
	if (id >= 0 || id <= 3)
	{
		SDL_Color col = SDL_Color();
		col.r = colour.Red();
		col.g = colour.Green();
		col.b = colour.Blue();
		col.a = colour.Alpha();
		ppu.SetColour(id, col);
	}
}


void Emulator::SetEmulationSpeed(unsigned speed)
{
	// speed is given in percentages, 100 is normal speed
	if (speed > 0)
	{
		this->emulation_speed = speed;
		microseconds_per_frame = std::ceil(100.0 / speed * microseconds_per_frame_DMG);
	}
}


wxSize Emulator::GetWindowSize()
{
	return wxSize(ppu.GetWindowWidth(), ppu.GetWindowHeight());
}


void Emulator::LoadConfig(std::ifstream& ifs)
{
	ifs.read((char*)&load_boot_rom, sizeof(bool));
	ifs.read((char*)&render_graphics_every_system_frame, sizeof(bool));
	ifs.read((char*)&emulation_speed_uncapped, sizeof(bool));
	ifs.read((char*)&emulation_speed, sizeof(unsigned));
	ifs.read((char*)&target_fps, sizeof(unsigned));

	Util::DeserializeSTDString(dmg_boot_path, ifs);
	Util::DeserializeSTDString(cgb_boot_path, ifs);

	SetEmulationSpeed(emulation_speed);
}


void Emulator::SaveConfig(std::ofstream& ofs)
{
	ofs.write((char*)&load_boot_rom, sizeof(bool));
	ofs.write((char*)&render_graphics_every_system_frame, sizeof(bool));
	ofs.write((char*)&emulation_speed_uncapped, sizeof(bool));
	ofs.write((char*)&emulation_speed, sizeof(unsigned));
	ofs.write((char*)&target_fps, sizeof(unsigned));

	Util::SerializeSTDString(dmg_boot_path, ofs);
	Util::SerializeSTDString(cgb_boot_path, ofs);
}


void Emulator::SetDefaultConfig()
{
	load_boot_rom = render_graphics_every_system_frame = true;
	emulation_speed_uncapped = false;
	emulation_speed = 100;
	target_fps = 60;
}


void Emulator::Deserialize(std::ifstream& ifs)
{
	ifs.read((char*)&m_cycle_counter, sizeof(unsigned));
}


void Emulator::Serialize(std::ofstream& ofs)
{
	ofs.write((char*)&m_cycle_counter, sizeof(unsigned));
}