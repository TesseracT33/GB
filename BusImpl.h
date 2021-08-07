#pragma once
#include "APU.h"
#include "Bus.h"
#include "Cartridge.h"
#include "CPU.h"
#include "DMA.h"
#include "Joypad.h"
#include "PPU.h"
#include "Serial.h"
#include "Timer.h"
#include "Utils.h"

class BusImpl final : public Bus, public Component
{
public:
	APU* apu;
	Cartridge* cartridge;
	CPU* cpu;
	class DMA* dma;
	Joypad* joypad;
	PPU* ppu;
	Serial* serial;
	Timer* timer;

	bool LoadBootRom(const std::string& boot_path);
	void Write(u16 addr, u8 data, bool ppu_access = false, bool apu_access = false);
	u8   Read(u16 addr, bool ppu_access = false, bool apu_access = false);
	u8*  ReadIOPointer(u16 addr);
	void Reset(bool execute_boot_rom);

	void InitiateSpeedSwitch();
	void ExitSpeedSwitch();

	void ResetDIV();

	u16 GetCurrentRomBank();

	void State(Serialization::BaseFunctor& functor) override;

private:
	struct Memory
	{
		u8 boot_rom[0x900]{}; // only 0x100 bytes large in DMG mode
		u8 vram[0x4000]{}; // 2 * 0x2000 bytes (2 banks)
		u8 wram[0x8000]{}; // 8 * 0x1000 bytes (8 banks)
		u8 oam[0xA0]{};
		u8 unused[0x60]{};
		u8 io[0x80]{};
		u8 hram[0x80]{};
		u8 ie{};
	} memory;

	u8 current_VRAM_bank = 0;
	u8 current_WRAM_bank = 1;

	bool boot_rom_mapped = true;
};

