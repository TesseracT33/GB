export module Cartridge;

import System;

import NumericalTypes;
import SerializationStream;

import <algorithm>;
import <array>;
import <cassert>;
import <filesystem>;
import <format>;
import <optional>;
import <string>;
import <vector>;

namespace Cartridge
{
	export
	{
		void Eject();
		void Initialize();
		bool LoadRom(const std::string& path);
		u8 ReadRam(u16 addr);
		u8 ReadRom(u16 addr);
		void StreamState(SerializationStream& stream);
		void WriteRam(u16 addr, u8 data);
		void WriteRom(u16 addr, u8 data);
	}

	enum class CartType {
		NoMBC, MBC1, MBC2, MBC3, MBC5
	} cart_type;

	constexpr uint ram_bank_size = 0x2000;
	constexpr uint rom_bank_size = 0x4000;

	bool DetectCartridgeType();
	bool DetectGbcFunctions();
	bool DetectSgbFunctions();
	bool DetectRamSize();
	bool DetectRomSize();
	void ReadCartridgeRAMFromDisk();
	void WriteCartridgeRAMToDisk();

	bool has_battery;
	bool has_clock;
	bool has_ram;
	bool ram_bank_is_2KB; // if the rom size code is 1 (@ cart addr 0x148), there is one RAM bank of size 2 KB. Otherwise, all RAM banks are of size 8 KB.
	bool ram_enabled;
	bool ram_rtc_mode_select; // 0 = RAM, 1 = RTC
	bool rom_ram_mode_select; // 0 = ROM, 1 = RAM
	bool rtc_0_written; // signifies if 0 has been written to the addr range 0x6000-0x7FFF
	bool rtc_enabled;

	uint current_ram_bank;
	uint current_rom_bank;
	uint num_ram_banks;
	uint num_rom_banks; // = rom_size / 0x4000 and >= 2 (each bank is 16 kiB), always a power of two for supported roms
	uint ram_size;
	uint rtc_register_select;

	std::string cart_name;

	std::array<u8, 0x200> mbc2_ram;
	std::array<u8, 5> rtc_ram{};

	std::vector<u8> rom;
	std::vector<u8> ram;
}