export module Bus;

import NumericalTypes;
import SerializationStream;

import <array>;
import <format>;
import <optional>;
import <string>;
import <string_view>;

/* Memory Map
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

namespace Bus
{
	export
	{
		enum Addr : u16
		{
			P1    = 0xFF00,
			SB    = 0xFF01,
			SC    = 0xFF02,
			DIV   = 0xFF04,
			TIMA  = 0xFF05,
			TMA   = 0xFF06,
			TAC   = 0xFF07,
			IF    = 0xFF0F,
			NR10  = 0xFF10,
			NR11  = 0xFF11,
			NR12  = 0xFF12,
			NR13  = 0xFF13,
			NR14  = 0xFF14,
			NR21  = 0xFF16,
			NR22  = 0xFF17,
			NR23  = 0xFF18,
			NR24  = 0xFF19,
			NR30  = 0xFF1A,
			NR31  = 0xFF1B,
			NR32  = 0xFF1C,
			NR33  = 0xFF1D,
			NR34  = 0xFF1E,
			NR41  = 0xFF20,
			NR42  = 0xFF21,
			NR43  = 0xFF22,
			NR44  = 0xFF23,
			NR50  = 0xFF24,
			NR51  = 0xFF25,
			NR52  = 0xFF26,
			LCDC  = 0xFF40,
			STAT  = 0xFF41,
			SCY   = 0xFF42,
			SCX   = 0xFF43,
			LY    = 0xFF44,
			LYC   = 0xFF45,
			DMA   = 0xFF46,
			BGP   = 0xFF47,
			OBP0  = 0xFF48,
			OBP1  = 0xFF49,
			WY    = 0xFF4A,
			WX    = 0xFF4B,
			KEY0  = 0xFF4C,
			KEY1  = 0xFF4D,
			VBK   = 0xFF4F,
			BOOT  = 0xFF50,
			HDMA1 = 0xFF51,
			HDMA2 = 0xFF52,
			HDMA3 = 0xFF53,
			HDMA4 = 0xFF54,
			HDMA5 = 0xFF55,
			RP    = 0xFF56,
			BCPS  = 0xFF68,
			BCPD  = 0xFF69,
			OCPS  = 0xFF6A,
			OCPD  = 0xFF6B,
			OPRI  = 0xFF6C,
			SVBK  = 0xFF70,
			PCM12 = 0xFF76,
			PCM34 = 0xFF77,
			IE    = 0xFFFF
		};

		void Initialize();
		constexpr std::string_view IoAddrToString(u16 addr);
		bool LoadBootRom(const std::string& path);
		u8 Peek(u16 addr);
		u8 Read(u16 addr);
		u8 ReadPageFF(u8 offset);
		u8 ReadPC(u16 addr);
		void StreamState(SerializationStream& stream);
		void Write(u16 addr, u8 data);
		void WritePageFF(u8 offset, u8 data);
	}

	u8 ReadIO(u16 addr);
	void WriteIO(u16 addr, u8 data);

	constexpr uint wram_bank_size = 0x1000;

	bool boot_rom_mapped = false;

	uint current_wram_bank = 1;

	std::array<u8, 0x8000> wram;
	std::array<u8, 0x60>   unused_memory_area;
	std::array<u8, 0x80>   hram;
}