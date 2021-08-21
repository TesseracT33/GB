#include "BusImpl.h"

using namespace Util;

#define IO(addr) memory.io[addr - 0xFF00]

bool BusImpl::LoadBootRom(const std::string& boot_path)
{
	// open the file
	FILE* boot_rom = fopen(boot_path.c_str(), "rb");
	if (boot_rom == NULL)
	{
		return false;
	}

	// get the file's size and check that it's equal to 256 bytes
	fseek(boot_rom, 0, SEEK_END);
	long rom_size = ftell(boot_rom);

	if (System::mode == System::Mode::CGB)
	{
		if (rom_size != 0x900)
		{
			wxMessageBox("Boot rom is not equal to the required 2 304 bytes.");
			return false;
		}
	}
	else
	{
		if (rom_size != 0x100)
		{
			wxMessageBox("Boot rom is not equal to the required 256 bytes.");
			return false;
		}
	}

	// read the file and put it in memory at 0000-00FF, or 0000-08FF in GBC mode
	rewind(boot_rom);
	fread(memory.boot_rom, 1, rom_size, boot_rom);
	fclose(boot_rom);

	return true;
}


void BusImpl::Write(u16 addr, u8 data, bool ppu_access, bool apu_access)
{
	// 0000h-7FFFh
	if (addr <= 0x7FFF)
	{
		// writing to this area is disallowed, except that we can write to
		// certain areas of memory for things such as switching RAM/ROM banks
		cartridge->Write(addr, data);
	}

	// 8000-9FFF - VRAM
	else if (addr <= 0x9FFF)
	{
		// in LCD mode 3, VRAM cannot be accessed, except by the ppu
		if (ppu_access || (IO(STAT) & 3) != 3)
		{
			memory.vram[addr - 0x8000 + current_VRAM_bank * 0x2000] = data;
		}
	}

	// A000-BFFF - RAM Bank 00-03, if any (Read/Write)
	else if (addr <= 0xBFFF)
	{
		cartridge->Write(addr, data);
	}

	// C0000-CFFF - WRAM bank 0
	else if (addr <= 0xCFFF)
	{
		memory.wram[addr - 0xC000] = data;
	}

	// D0000-DFFF - WRAM bank 1-7 (switchable in GBC mode only; in DMG mode always 1)
	else if (addr <= 0xDFFF)
	{
		memory.wram[addr - 0xD000 + current_WRAM_bank * 0x1000] = data;
	}

	// E000h-FDFFh -- ECHO
	else if (addr <= 0xFDFF)
	{
		memory.wram[addr - 0xE000] = data;  // redirect write to WRAM0
	}

	// FE00h-FE9Fh -- OAM
	else if (addr <= 0xFE9F)
	{
		// OAM can only be accessed in LCD mode 0 and 1, or by the ppu
		u8 LCDMode = IO(STAT) & 3;
		if (LCDMode == 0 || LCDMode == 1 || ppu_access)
			memory.oam[addr - 0xFE00] = data;
	}

	// FEA0h-FEFFh -- "Unused Memory Area"
	else if (addr <= 0xFEFF)
	{
		// In DMG mode, writing is ignored. In CGB mode, see section 2.10 in TCAGBD.pdf
		if (System::mode == System::Mode::CGB && (IO(STAT) & 3) != 3)
		{
			if (addr <= 0xFEBF)
				memory.unused[addr - 0xFEA0] = data;
			else
			{
				memory.unused[(0xFEC0 | addr & 0xF) - 0xFEA0] = data;
				memory.unused[(0xFED0 | addr & 0xF) - 0xFEA0] = data;
				memory.unused[(0xFEE0 | addr & 0xF) - 0xFEA0] = data;
				memory.unused[(0xFEF0 | addr & 0xF) - 0xFEA0] = data;
			}
		}
	}

	// FF00h-FF7Fh -- I/O Registers
	else if (addr <= 0xFF7F)
	{
		switch (addr)
		{
		case Addr::P1: // 0xFF00
		{
			u8 prev_select = IO(P1) & 3 << 4;
			u8 new_select = data & 3 << 4;

			if (prev_select == new_select) return;

			// only bits 4-5 are writable
			IO(P1) = (IO(P1) & 0xF) | new_select;

			// update bits 0-3 of P1. These are not writeable, but can be changed if bits 4-5 are changed, if a button is also currently held
			joypad->UpdateP1OutputLines();

			break;
		}

		case Addr::SC: // 0xFF02
			IO(SC) = data;
			if (data == 0x81)
				serial->TriggerTransfer();
			break;

		case Addr::DIV: // 0xFF04
			// reset DIV no matter what is written.
			timer->ResetDIV();
			break;

		case Addr::TAC: // 0xFF07
			IO(TAC) = data;
			timer->SetTIMAFreq(data & 3);
			timer->TIMA_enabled = CheckBit(data, 2);
			break;

		case Addr::NR10:
		case Addr::NR11:
		case Addr::NR12:
		case Addr::NR13:
		case Addr::NR14:
		case Addr::NR21:
		case Addr::NR22:
		case Addr::NR23:
		case Addr::NR24:
		case Addr::NR30:
		case Addr::NR31:
		case Addr::NR32:
		case Addr::NR33:
		case Addr::NR34:
		case Addr::NR42:
		case Addr::NR43:
		case Addr::NR44:
		case Addr::NR50:
		case Addr::NR51:
			if (!apu->enabled) break;
			IO(addr) = data;
			apu->WriteToAudioReg(addr, data);
			break;

		case Addr::NR41:
			// while the apu is off, writes can still be made to NR41. source: blargg test rom 'dmg_sound' -- '11.regs after power'
			IO(addr) = data;
			apu->WriteToAudioReg(addr, data);
			break;

		case Addr::NR52:
			// bits 0-3 are read-only. bit 7 is r/w. the rest are not used
			IO(NR52) = IO(NR52) & 0x7F | data & 0x80;
			apu->WriteToAudioReg(addr, data);
			break;

		case 0xFF30: // wave ram (0xFF30-0xFF3E)
		case 0xFF31:
		case 0xFF32:
		case 0xFF33:
		case 0xFF34:
		case 0xFF35:
		case 0xFF36:
		case 0xFF37:
		case 0xFF38:
		case 0xFF39:
		case 0xFF3A:
		case 0xFF3B:
		case 0xFF3C:
		case 0xFF3D:
		case 0xFF3E:
		case 0xFF3F:
			if (!apu_access)
				apu->WriteToWaveRam(addr, data);
			else
				IO(addr) = data;
			break;

		case Addr::LCDC: // 0xFF40
		{
			IO(LCDC) = data;
			bool enable_LCD = CheckBit(data, 7);
			if (enable_LCD && !ppu->LCD_enabled)
				ppu->EnableLCD();
			else if (!enable_LCD && ppu->LCD_enabled)
				ppu->DisableLCD();
			ppu->SetLCDCFlags();
			break;
		}

		case Addr::STAT: // 0xFF41
			// only bits 3-6 are writable
			IO(STAT) &= ~(0xF << 3);
			IO(STAT) |= data & 0xF << 3;
			ppu->CheckSTATInterrupt();
			break;

		case Addr::LY: // 0xFF44
			// reset the current scanline if the game tries to write to LY
			IO(LY) = 0;
			ppu->CheckSTATInterrupt();
			ppu->WY_equals_LY_in_current_frame |= IO(WY) == 0;
			break;

		case Addr::LYC: // 0xFF45
			IO(LYC) = data;
			ppu->CheckSTATInterrupt();
			break;

		case Addr::DMA: // 0xFF46
			dma->InitiateDMATransfer(data);
			break;

		case Addr::WY: // 0xFF4A
			IO(WY) = data;
			ppu->WY_equals_LY_in_current_frame |= IO(LY) == IO(WY);
			break;

		case Addr::KEY1: // 0xFF4D
			// bit 7 is read only. also, only bits 0 and 7 are used
			if (System::mode == System::Mode::CGB)
			{
				IO(KEY1) &= 0xFE;
				IO(KEY1) |= data & 1;
			}
			break;

		case Addr::VBK: // 0xFF4F
			if (System::mode == System::Mode::CGB)
			{
				// only the lowest bit is writeable; all others return 1 (handled in read function)
				IO(VBK) = data;
				current_VRAM_bank = data & 1;
			}
			break;

		case Addr::BOOT: // 0xFF50
			// unmap the boot rom whenever any value is written to 0xFF50
			boot_rom_mapped = false;
			break;

		case Addr::HDMA1: // 0xFF51
			IO(HDMA1) = data;
			dma->HDMA_source_addr = IO(HDMA1) << 8 | IO(HDMA2);
			// Selecting an address in the range E000h-FFFFh will read addresses from A000h-BFFFh
			if (dma->HDMA_source_addr >= 0xE000)
				dma->HDMA_source_addr -= 0x4000;
			break;

		case Addr::HDMA2: // 0xFF52
			// bits 0-3 are ignored (they are always set to 0)
			IO(HDMA2) = data & 0xF0;
			dma->HDMA_source_addr = IO(HDMA1) << 8 | IO(HDMA2);
			if (dma->HDMA_source_addr >= 0xE000)
				dma->HDMA_source_addr -= 0x4000;
			break;

		case Addr::HDMA3: // 0xFF53
			// bits 5-7 are ignored. Dest is always 0x8000-0x9FF0
			IO(HDMA3) = data & 0x1F; // 0x1FF0
			dma->HDMA_dest_addr = 0x8000 + (IO(HDMA3) << 8 | IO(HDMA4));
			break;

		case Addr::HDMA4: // 0xFF54
			// bits 0-3 are ignored (they are always set to 0)
			IO(HDMA4) = data & 0xF0;
			dma->HDMA_dest_addr = 0x8000 + (IO(HDMA3) << 8 | IO(HDMA4));
			break;

		case Addr::HDMA5: // 0xFF55
			if (System::mode == System::Mode::CGB)
			{
				IO(HDMA5) = data;
				bool transferMode = data >> 7; // 0=GDMA, 1=HDMA
				if (transferMode == 0)
				{
					// Stop HDMA copy if it is active. Do not start GDMA transfer at this time.
					if (dma->HDMA_transfer_active)
					{
						dma->HDMA_transfer_active = false;
						IO(HDMA5) = data | 0x80; // according to pandocs
					}
					else
					{
						dma->InitiateGDMATransfer(data);
					}
				}
				else if (!cpu->in_halt_mode && !cpu->in_stop_mode) // do not start HDMA if CPU is in HALT or STOP mode
				{
					IO(HDMA5) &= 0x7F; // clear bit 7
					dma->InitiateHDMATransfer(data);
				}
			}
			break;

		case Addr::BCPD: // 0xFF69
			if (System::mode != System::Mode::CGB) break;

			if ((IO(STAT) & 3) != 3)
				ppu->Set_CGB_Colour(IO(BCPS) & 0x3F, data, true);

			// if bit 7 of BCPS is set, increment the BCPS index (bits 0-5 of BCPS) (even if in lcd mode 3)
			if (CheckBit(IO(BCPS), 7) == 1)
			{
				IO(BCPS) &= ~(1 << 6);
				IO(BCPS)++;
			}

			break;

		case Addr::OCPD: // 0xFF6B
			if (System::mode != System::Mode::CGB) break;

			if ((IO(STAT) & 3) != 3)
				ppu->Set_CGB_Colour(IO(OCPS) & 0x3F, data, false);

			// if bit 7 of OCPS is set, increment the OCPS index (bits 0-5 of OCPS) (even if in lcd mode 3)
			if (CheckBit(IO(OCPS), 7) == 1)
			{
				IO(OCPS) &= ~(1 << 6);
				IO(OCPS)++;
			}

			break;

		case Addr::OPRI: // 0xFF6C
			IO(OPRI) = data;
			ppu->obj_priority_mode = static_cast<PPU::OBJ_Priority_Mode> (data & 1);
			// todo: OPRI has an effect if a PGB value (0xX8, 0xXC) is written to KEY0 but STOP hasn’t been executed yet, and the write takes effect instantly.
			break;

		case Addr::SVBK: // 0xFF70
			if (System::mode == System::Mode::CGB)
			{
				IO(SVBK) = data;
				current_WRAM_bank = std::max(1, data & 7); // selecting bank 0 will select bank 1
			}
			break;

		default:
			// writing is in general freely allowed
			IO(addr) = data;
			break;
		}
	}

	// FF80h-FFFEh -- HRAM
	else if (addr <= 0xFFFE)
	{
		memory.hram[addr - 0xFF80] = data;
	}

	// FFFF -- IE
	else
	{
		memory.ie = data;
	}
}


u8 BusImpl::Read(u16 addr, bool ppu_access, bool apu_access)
{
	if (addr <= 0xFF || System::mode == System::Mode::CGB && addr <= 0x8FF)
	{
		if (boot_rom_mapped)
			return memory.boot_rom[addr];
		return cartridge->Read(addr);
	}

	else if (addr <= 0x7FFF)
	{
		return cartridge->Read(addr);
	}

	// 8000-9FFF - VRAM
	else if (addr <= 0x9FFF)
	{
		// in LCD mode 3, VRAM cannot be accessed, except by the ppu
		if (ppu_access || (IO(STAT) & 3) != 3)
			return memory.vram[addr - 0x8000 + current_VRAM_bank * 0x2000];
		return 0xFF;
	}

	// A000-BFFF - RAM Bank 00-03, if any (Read/Write)
	else if (addr <= 0xBFFF)
	{
		return cartridge->Read(addr);
	}

	// C000-CFFF - WRAM bank 0
	else if (addr <= 0xCFFF)
	{
		return memory.wram[addr - 0xC000];
	}

	// D0000-DFFF - WRAM bank 1-7 (switchable in GBC mode only; in DMG mode always 1)
	else if (addr <= 0xDFFF)
	{
		return memory.wram[addr - 0xD000 + current_WRAM_bank * 0x1000];
	}

	// E000-FDFF -- ECHO
	else if (addr <= 0xFDFF)
	{
		return memory.wram[addr - 0xE000];  // redirect read to WRAM0
	}

	// FE00h-FE9Fh -- OAM
	else if (addr <= 0xFE9F)
	{
		// OAM can only be accessed in LCD mode 0 and 1, or by the ppu
		u8 LCDMode = IO(STAT) & 3;
		if (LCDMode == 0 || LCDMode == 1 || ppu_access)
			return memory.oam[addr - 0xFE00];
		return 0xFF;
	}

	// FEA0-FEFF -- "Unused Memory Area"
	else if (addr <= 0xFEFF)
	{
		// In DMG mode, reading returns 0. In CGB mode, see section 2.10 in TCAGBD.pdf
		if (System::mode == System::Mode::CGB)
		{
			if ((IO(STAT) & 3) == 3) return 0xFF;
			return memory.unused[addr - 0xFEA0];
		}
		return 0;
	}

	// FF00-FF7F -- IO
	else if (addr <= 0xFF7F)
	{
		switch (addr)
		{
		case Addr::IF: // 0xFF0F
			return IO(IF) | 7 << 5; // bits 5-7 always return 1

		case Addr::NR10: // 0xFF10
			return IO(NR10) | 0x80; // bit 7 always returns 1

		case Addr::NR11: // 0xFF11
			return IO(NR11) | 0x3F; // bits 0-5 always return 1

		case Addr::NR13: // 0xFF13
			return 0xFF; // always returns 0xFF

		case Addr::NR14: // 0xFF14
			return IO(NR14) | 0xBF; // bits 0-5 and 7 always return 1

		case Addr::NR20: // 0xFF15
			return 0xFF; // does not exist

		case Addr::NR21: // 0xFF16
			return IO(NR21) | 0x3F; // bits 0-5 always return 1

		case Addr::NR23: // 0xFF18
			return 0xFF; // always returns 0xFF

		case Addr::NR24: // 0xFF19
			return IO(NR24) | 0xBF; // bits 0-5 and 7 always return 1

		case Addr::NR30: // 0xFF1A
			return IO(NR30) | 0x7F; // bits 0-6 always return 1

		case Addr::NR31: // 0xFF1B
			return 0xFF; // always returns 0xFF

		case Addr::NR32: // 0xFF1C
			return IO(NR32) | 0x9F; // bits 0-4 and 7 always return 1

		case Addr::NR33: // 0xFF1D
			return 0xFF; // always returns 0xFF

		case Addr::NR34: // 0xFF1E
			return IO(NR34) | 0xBF; // bits 0-5 and 7 always return 1

		case Addr::NR40: // 0xFF1F
			return 0xFF; // does not exist

		case Addr::NR41: // 0xFF20
			return 0xFF; // always returns 0xFF

		case Addr::NR44: // 0xFF23
			return IO(NR44) | 0xBF; // bits 0-5 and 7 always return 1

		case Addr::NR52: // 0xFF26
			return IO(NR52) | 0x70; // bits 4-6 always return 1

		case 0xFF27:
		case 0xFF28:
		case 0xFF29:
		case 0xFF2A:
		case 0xFF2B:
		case 0xFF2C:
		case 0xFF2D:
		case 0xFF2E:
		case 0xFF2F:
			return 0xFF; // does not exist

		case 0xFF30: // wave ram (0xFF30-0xFF3E)
		case 0xFF31:
		case 0xFF32:
		case 0xFF33:
		case 0xFF34:
		case 0xFF35:
		case 0xFF36:
		case 0xFF37:
		case 0xFF38:
		case 0xFF39:
		case 0xFF3A:
		case 0xFF3B:
		case 0xFF3C:
		case 0xFF3D:
		case 0xFF3E:
		case 0xFF3F:
			if (!apu_access)
				return apu->ReadFromWaveRam(addr);
			return IO(addr);

		case Addr::STAT: // 0xFF41
			return IO(STAT) | 0x80; // bit 7 always returns 1. 

		case Addr::KEY1: // 0xFF4D
			// bits 1-6 always return 1. in DMG mode, always return 0xFF
			if (System::mode == System::Mode::DMG) return 0xFF;
			return IO(KEY1) | 0x3F << 1;

		case Addr::VBK: // 0xFF4F
			// select the VRAM bank mapped to 8000h - 9FFFh
			if (System::mode == System::Mode::CGB)
				return 0xFE | current_VRAM_bank; // only the lower bit is R/W, the rest return 1
			return 0xFF;

		case Addr::HDMA1: // 0xFF51
		case Addr::HDMA2: // 0xFF52
		case Addr::HDMA3: // 0xFF53
		case Addr::HDMA4: // 0xFF54
			return 0xFF; // always returns 0xFF when read

		case Addr::HDMA5: // 0xFF55
			if (System::mode == System::Mode::CGB)
			{
				// Reading from HDMA5 register will return the number of remaining blocks to copy
				if (dma->HDMA_transfer_active)
					return dma->HDMA_written_blocks_total - dma->HDMA_written_blocks;
			}
			return 0xFF;

		case Addr::RP: // 0xFF56
			if (System::mode == System::Mode::DMG)
				return 0xFF;
			return 0xFF; // todo

		case Addr::BCPD: // 0xFF69
			// (CGB mode only) in LCD mode 3, this area can't be accessed
			if (System::mode == System::Mode::CGB && (IO(STAT) & 3) != 3)
				return ppu->Get_CGB_Colour(IO(BCPS) & 0x3F, true);
			return 0xFF;

		case Addr::OCPD: // 0xFF6B
			// (CGB mode only) in LCD mode 3, this area can't be accessed
			if (System::mode == System::Mode::CGB && (IO(STAT) & 3) != 3)
				return ppu->Get_CGB_Colour(IO(OCPS) & 0x3F, false);
			return 0xFF;

		case Addr::SVBK: // 0xFF70
			if (System::mode == System::Mode::CGB)
				return IO(SVBK) | 0x1F << 3; // bits 3-7 return 1
			return 0xFF;

		default:
			// Wave ram (0xFF30-0xFF3F)
			if (addr >= 0xFF30 && addr <= 0xFF3F)
				return apu->ReadFromWaveRam(addr);
			return IO(addr);
		}
	}

	// FF80-FFFE -- HRAM
	else if (addr <= 0xFFFE)
	{
		return memory.hram[addr - 0xFF80];
	}

	// FFFF -- IE
	else
	{
		return memory.ie;
	}
}


u8* BusImpl::ReadIOPointer(u16 addr)
{
	assert(addr >= 0xFF00 && addr <= 0xFF7F || addr == 0xFFFF);
	if (addr == 0xFFFF)
		return &memory.ie;
	return &memory.io[addr - 0xFF00];
}


void BusImpl::Reset(bool execute_boot_rom)
{
	boot_rom_mapped = execute_boot_rom;

	// set all bytes in memory to 0, except for the boot rom area
	u8 boot_rom_tmp[0x900]{};
	memcpy(boot_rom_tmp, memory.boot_rom, sizeof(u8) * 0x900);
	memory = {};
	memcpy(memory.boot_rom, boot_rom_tmp, sizeof(u8) * 0x900);

	IO(P1) = 0xFF;
	current_VRAM_bank = 0;
	current_WRAM_bank = 1;

	if (!execute_boot_rom)
	{
		// https://gbdev.io/pandocs/Power_Up_Sequence.html
		// todo: change these to Write(addr) = data
		IO(NR10) = 0x80; IO(NR11) = 0xBF; IO(NR12) = 0xF3; IO(NR14) = 0xBF;
		IO(NR21) = 0x3F; IO(NR24) = 0xBF; IO(NR30) = 0x7F; IO(NR31) = 0xFF;
		IO(NR32) = 0x9F; IO(NR34) = 0xBF; IO(NR41) = 0xFF; IO(NR44) = 0xBF;
		IO(NR50) = 0x77; IO(NR51) = 0xF3; IO(NR52) = 0xF1;

		IO(LCDC) = 0x91; IO(BGP) = 0xFC; IO(OBP0) = 0xFF; IO(OBP1) = 0xFF;
		IO(STAT) = 0x85; IO(IF) = 0xE1;
	}

	m_cycle_counter = 0;
}


u8 BusImpl::ReadCycle(u16 addr)
{
	u8 data = Read(addr);
	// apu, ppu and timer is updated each t-cycle (see e.g. ppu.Update()), the rest each m-cycle
	apu->Update();
	dma->Update();
	ppu->Update();
	serial->Update();
	timer->Update();

	m_cycle_counter++;
	return data;
}


void BusImpl::WriteCycle(u16 addr, u8 data)
{
	Write(addr, data);

	apu->Update();
	dma->Update();
	ppu->Update();
	serial->Update();
	timer->Update();

	m_cycle_counter++;
}


void BusImpl::WaitCycle(const unsigned cycles)
{
	for (int i = 0; i < cycles; i++)
	{
		apu->Update();
		dma->Update();
		ppu->Update();
		serial->Update();
		timer->Update();
	}
	m_cycle_counter += cycles;
}


void BusImpl::InitiateSpeedSwitch()
{
	timer->DIV_enabled = false;

	// VRAM/OAM/… locking is "frozen", yielding different results depending on the STAT mode it's started in:
	u8 LCD_mode = IO(STAT) & 3;
	if (LCD_mode < 2)
		ppu->can_access_VRAM = false;
	else if (LCD_mode == 2)
		ppu->can_access_OAM = false;
}


void BusImpl::ExitSpeedSwitch()
{
	IO(KEY1) &= 0xFE;
	timer->DIV_enabled = true;
	ppu->can_access_VRAM = ppu->can_access_OAM = true;

	System::ToggleSpeedMode();
}


void BusImpl::ResetDIV()
{
	timer->ResetDIV();
}


u16 BusImpl::GetCurrentRomBank()
{
	return cartridge->GetCurrentRomBank();
}


void BusImpl::State(Serialization::BaseFunctor& functor)
{
	functor.fun(&boot_rom_mapped, sizeof(bool));
	functor.fun(&memory, sizeof(Memory));
	functor.fun(&current_VRAM_bank, sizeof(u8));
	functor.fun(&current_WRAM_bank, sizeof(u8));

	functor.fun(&m_cycle_counter, sizeof(unsigned));
}