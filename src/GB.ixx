export module GB;

import APU;
import Bus;
import Cartridge;
import Core;
import CPU;
import DMA;
import Joypad;
import PPU;
import Serial;
import System;
import Timer;
import Util;

import <string>;
import <string_view>;
import <vector>;

export struct GB : Core
{
	void ApplyNewSampleRate() override
	{
		APU::ApplyNewSampleRate();
	}


	void Detach() override
	{
		Cartridge::Eject();
	}


	void DisableAudio() override
	{
		// TODO
	}


	void EnableAudio() override
	{
		// TODO
	}


	std::vector<std::string_view> GetActionNames() override
	{
		std::vector<std::string_view> names{};
		names.emplace_back("A");
		names.emplace_back("B");
		names.emplace_back("Select");
		names.emplace_back("Start");
		names.emplace_back("Right");
		names.emplace_back("Left");
		names.emplace_back("Up");
		names.emplace_back("Down");
		return names;
	}


	uint GetNumberOfInputs() override
	{
		return 8;
	}


	void Initialize() override
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


	bool LoadBios(const std::string& path) override
	{
		return Bus::LoadBootRom(path);
	}


	bool LoadRom(const std::string& path) override
	{
		return Cartridge::LoadRom(path);
	}


	void NotifyNewAxisValue(uint player_index, uint input_action_index, int axis_value) override
	{
		/* no axes */
	}


	void NotifyButtonPressed(uint player_index, uint input_action_index) override
	{
		if (player_index == 0) {
			Joypad::NotifyButtonPressed(input_action_index);
		}
	}


	void NotifyButtonReleased(uint player_index, uint input_action_index) override
	{
		if (player_index == 0) {
			Joypad::NotifyButtonReleased(input_action_index);
		}
	}


	void Reset() override
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


	void Run() override
	{
		CPU::Run();
	}


	void StreamState(SerializationStream& stream) override
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
};