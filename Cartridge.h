#pragma once

#include <wx/filename.h>
#include <wx/msgdlg.h>

#include <ctime>
#include <time.h>

#include "Bus.h"
#include "Types.h"
#include "Utils.h"

class Cartridge final : public Serializable
{
public:
	u8   Read(u16 addr);
	void Write(u16 addr, u8 data);
	void Reset();
	System::Mode LoadCartridge(const char* path);
	void EjectCartridge();

	// helper function for debugging
	u16 GetCurrentRomBank();

	void Serialize(std::ofstream& ofs) override;
	void Deserialize(std::ifstream& ifs) override;

private:
	enum class MBC { None, MBC1, MBC2, MBC3, MBC5 } mbc;

	// general
	static const size_t size_cartridge = 0x800000; // the biggest known GBC game is 8 MiB
	static const size_t size_RAM_banks = 16 * 0x2000; // at most 16 * 8 kiB = 128 kiB

	std::string cart_name;

	bool DetectCartridgeType();
	bool DetectROMSize();
	bool DetectRAMSize();
	bool DetectGBCFunctions();
	bool DetectSGBFunctions();

	void WriteCartridgeRAMToDisk();
	void ReadCartridgeRAMFromDisk();

	// TODO: test implementation of saving external ram to disk

	bool external_battery_available = false;
	bool external_RAM_available = false;
	bool RAM_bank_is_2KB = false; // if the rom size code is 1 (@ cart addr 0x148), there is one RAM bank of size 2 KB. Otherwise, all RAM banks are of size 8 KB.
	bool RAM_enabled = false;
	bool ROM_RAM_mode_select = 0; // 0 = ROM, 1 = RAM
	u8 cartridge[size_cartridge]{};
	u8 current_RAM_bank = 0;
	u8 num_RAM_banks = 0;
	u8 RAM_banks[size_RAM_banks]{};
	u16 current_ROM_bank = 1;
	u16 num_ROM_banks; // = rom_size / 0x4000 and >= 2 (each bank is 16 kiB), always a power of two for supported roms
	u32 rom_size; // size of cartridge in bytes

	// MBC2-specific
	static const size_t size_MBC2_RAM = 0x200;
	u8 MBC2_RAM[size_MBC2_RAM]{};

	// MBC3-specific
	bool external_clock_available = false;
	bool RAM_RTC_mode_select = false; // 0 = RAM, 1 = RTC
	bool RTC_0_written = false; // signifies if 0 has been written to the addr range 0x6000-0x7FFF
	bool RTC_enabled = false;
	static const size_t size_RTC_RAM = 5;
	u8 RTC_memory[size_RTC_RAM]{};
	u8 RTC_register_select = 0;
};