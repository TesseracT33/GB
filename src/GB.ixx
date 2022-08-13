export module GB;

import APU;
import Bus;
import Cartridge;
import CPU;
import DMA;
import Joypad;
import PPU;
import Serial;
import System;
import Timer;

import NumericalTypes;
import SerializationStream;

import <string>;

export namespace GB
{
	void ApplyNewSampleRate()
	{
		APU::ApplyNewSampleRate();
	}


	bool AssociatesWithRomExtension(const std::string& ext)
	{
		return ext.compare("gb") == 0 || ext.compare("gbc") == 0
			|| ext.compare("GB") == 0 || ext.compare("GBC") == 0;
	}


	void Detach()
	{
		Cartridge::Eject();
	}


	void DisableAudio()
	{
		// TODO
	}


	void EnableAudio()
	{
		// TODO
	}


	uint GetNumberOfInputs()
	{
		return 8;
	}


	void Initialize()
	{
		APU::Initialize();
		Bus::Initialize();
		Cartridge::Initialize();
		CPU::Initialize(false);
		DMA::Initialize();
		Joypad::Initialize();
		PPU::Initialize();
		Serial::Initialize();
		System::Initialize();
		Timer::Initialize();
	}


	bool LoadBios(const std::string& path)
	{
		return Bus::LoadBootRom(path);
	}


	bool LoadRom(const std::string& path)
	{
		return Cartridge::LoadRom(path);
	}


	void NotifyNewAxisValue(uint player_index, uint input_action_index, int axis_value)
	{
		/* no axes */
	}


	void NotifyButtonPressed(uint player_index, uint input_action_index)
	{
		if (player_index == 0) {
			Joypad::NotifyButtonPressed(input_action_index);
		}
	}


	void NotifyButtonReleased(uint player_index, uint input_action_index)
	{
		if (player_index == 0) {
			Joypad::NotifyButtonReleased(input_action_index);
		}
	}


	void Reset()
	{ // TODO: reset vs initialize
		APU::Initialize();
		Bus::Initialize();
		Cartridge::Initialize();
		CPU::Initialize(false);
		DMA::Initialize();
		Joypad::Initialize();
		PPU::Initialize();
		Serial::Initialize();
		System::Initialize();
		Timer::Initialize();
	}


	void Run()
	{
		CPU::Run();
	}


	void StreamState(SerializationStream& stream)
	{
		APU::StreamState(stream);
		Bus::StreamState(stream);
		Cartridge::StreamState(stream);
		CPU::StreamState(stream);
		DMA::StreamState(stream);
		Joypad::StreamState(stream);
		PPU::StreamState(stream);
		Serial::StreamState(stream);
		System::StreamState(stream);
		Timer::StreamState(stream);
	}
}