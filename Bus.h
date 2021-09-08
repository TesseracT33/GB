#pragma once

#include "Component.h"
#include "Serialization.h"
#include "System.h"
#include "Types.h"

/* General Memory Map
* 0000h-3FFFh -- ROM0          -- Non-switchable ROM Bank
* 4000h-7FFFh -- ROMX          -- Switchable ROM bank.
* 8000h-9FFFh -- VRAM          -- Video RAM, switchable (0-1) in GBC mode.
* A000h-BFFFh -- SRAM          -- External RAM in cartridge, often battery buffered.
* C000h-CFFFh -- WRAM0         -- Work RAM.
* D000h-DFFFh -- WRAMX         -- Work RAM, switchable (1-7) in GBC mode
* E000h-FDFFh -- ECHO          -- A mirror of C000-DDFF
* FE00h-FE9Fh -- OAM           -- (Object Attribute Table) Sprite information table.
* FEA0h-FEFFh -- UNUSED        -- Not Usable
* FF00h-FF7Fh -- I/O Registers -- I/O registers are mapped here.
* FF80h-FFFEh -- HRAM          -- Internal CPU RAM
* FFFFh       -- IE Register   -- Interrupt enable flags.
*/

class Bus
{
public:
	enum Addr : u16
	{
		GBC_FLAG  = 0x0143,
		SGB_FLAG  = 0x0146,
		CART_TYPE = 0x0147,
		ROM_SIZE  = 0x0148,
		RAM_SIZE  = 0x0149,

		OAM_START = 0xFE00,

		P1        = 0xFF00,
		SB        = 0xFF01,
		SC        = 0xFF02,
		DIV       = 0xFF04,
		TIMA      = 0xFF05,
		TMA       = 0xFF06,
		TAC       = 0xFF07,
		IF        = 0xFF0F,
		NR10      = 0xFF10,
		NR11      = 0xFF11,
		NR12      = 0xFF12,
		NR13      = 0xFF13,
		NR14      = 0xFF14,
		NR20      = 0xFF15,
		NR21      = 0xFF16,
		NR22      = 0xFF17,
		NR23      = 0xFF18,
		NR24      = 0xFF19,
		NR30      = 0xFF1A,
		NR31      = 0xFF1B,
		NR32      = 0xFF1C,
		NR33      = 0xFF1D,
		NR34      = 0xFF1E,
		NR40      = 0xFF1F,
		NR41      = 0xFF20,
		NR42      = 0xFF21,
		NR43      = 0xFF22,
		NR44      = 0xFF23,
		NR50      = 0xFF24,
		NR51      = 0xFF25,
		NR52      = 0xFF26,
		WAV_START = 0xFF30,
		LCDC      = 0xFF40,
		STAT      = 0xFF41,
		SCY       = 0xFF42,
		SCX       = 0xFF43,
		LY        = 0xFF44,
		LYC       = 0xFF45,
		DMA       = 0xFF46,
		BGP       = 0xFF47,
		OBP0      = 0xFF48,
		OBP1      = 0xFF49,
		BOOT      = 0xFF50,
		WY        = 0xFF4A,
		WX        = 0xFF4B,
		KEY1      = 0xFF4D,
		VBK       = 0xFF4F,
		HDMA1     = 0xFF51,
		HDMA2     = 0xFF52,
		HDMA3     = 0xFF53,
		HDMA4     = 0xFF54,
		HDMA5     = 0xFF55,
		RP        = 0xFF56,
		BCPS      = 0xFF68,
		BCPD      = 0xFF69,
		OCPS      = 0xFF6A,
		OCPD      = 0xFF6B,
		OPRI      = 0xFF6C,
		SVBK      = 0xFF70,
		PCM12     = 0xFF76,
		PCM34     = 0xFF77,
		IE        = 0xFFFF
	};

	virtual void ExitSpeedSwitch() = 0;
	virtual u16  GetCurrentRomBank() = 0; // helper function for debugging
	virtual void InitiateSpeedSwitch() = 0;
	virtual bool LoadBootRom(const std::string& boot_path) = 0;
	virtual void Write(u16 addr, u8 data, bool ppu_access = false, bool apu_access = false) = 0;
	virtual u8   Read(u16 addr, bool ppu_access = false, bool apu_access = false) = 0;
	virtual u8*  ReadIOPointer(u16 addr) = 0;
	virtual void Reset(bool execute_boot_rom) = 0;
	virtual void ResetDIV() = 0; // out-of-place, but it is to get around circular dependencies between CPU and Timer

	// Read/write and advance the gb state machine by any one or more cycles. Only the CPU uses these functions
	virtual u8 ReadCycle(u16 addr) = 0;
	virtual void WriteCycle(u16 addr, u8 data) = 0;
	virtual void WaitCycle(const unsigned cycles = 1) = 0;

	unsigned m_cycle_counter = 0;
};

