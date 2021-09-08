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

	void ExitSpeedSwitch();
	u16  GetCurrentRomBank();
	void InitiateSpeedSwitch();
	bool LoadBootRom(const std::string& boot_path);
	void Write(u16 addr, u8 data, bool ppu_access = false, bool apu_access = false);
	u8   Read(u16 addr, bool ppu_access = false, bool apu_access = false);
	u8*  ReadIOPointer(u16 addr);
	void Reset(bool execute_boot_rom);
	void ResetDIV();

	u8 ReadCycle(u16 addr);
	void WriteCycle(u16 addr, u8 data);
	void WaitCycle(const unsigned cycles = 1);

	void State(Serialization::BaseFunctor& functor) override;

private:
	/* General Memory Map
	* $0000-$3FFF -- ROM0          -- Non-switchable ROM Bank
	* $4000-$7FFF -- ROMX          -- Switchable ROM bank.
	* $8000-$9FFF -- VRAM          -- Video RAM, switchable (0-1) in GBC mode.
	* $A000-$BFFF -- SRAM          -- External RAM in cartridge, often battery buffered.
	* $C000-$CFFF -- WRAM0         -- Work RAM.
	* $D000-$DFFF -- WRAMX         -- Work RAM, switchable (1-7) in GBC mode
	* $E000-$FDFF -- ECHO          -- A mirror of C000-DDFF
	* $FE00-$FE9F -- OAM           -- (Object Attribute Table) Sprite information table.
	* $FEA0-$FEFF -- UNUSED        -- Not Usable
	* $FF00-$FF7F -- I/O Registers -- I/O registers are mapped here.
	* $FF80-$FFFE -- HRAM          -- Internal CPU RAM
	* $FFFF       -- IE Register   -- Interrupt enable flags.
	*/
	struct Memory
	{
		u8 boot_rom[0x900]{}; // $0-$100 on DMG, $0-$8FF on CGB
		u8 vram[0x4000]{};    // $8000-$9FFF (2 banks)
		u8 wram[0x8000]{};    // $C000-$DFFF (8 banks)
		u8 oam[0xA0]{};       // $FE00-$FE9F
		u8 unused[0x60]{};    // $FEA0-$FEFF
		u8 io[0x80]{};        // $FF00-$FF7F
		u8 hram[0x80]{};      // $FF80-$FFFE
		u8 ie{};              // $FFFF
	} memory;

	u8 current_VRAM_bank = 0;
	u8 current_WRAM_bank = 1;

	bool boot_rom_mapped = true;
};

