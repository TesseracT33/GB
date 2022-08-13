module Bus;

import APU;
import Boot;
import Cartridge;
import CPU;
import Debug;
import DMA;
import Joypad;
import PPU;
import Serial;
import System;
import Timer;

import Util.Files;

import UserMessage;

namespace Bus
{
	void Initialize()
	{
		wram.fill(0);
		unused_memory_area.fill(0);
		hram.fill(0);
		current_wram_bank = 1;
		boot_rom_mapped = true;
	}


	constexpr std::string_view IoAddrToString(u16 addr)
	{
		switch (addr) {
		case P1: return "P1";
		case SB: return "SB";
		case SC: return "SC";
		case DIV: return "DIV";
		case TIMA: return "TIMA";
		case TMA: return "TMA";
		case TAC: return "TAC";
		case IF: return "IF";
		case NR10: return "NR10";
		case NR11: return "NR11";
		case NR12: return "NR12";
		case NR13: return "NR13";
		case NR14: return "NR14";
		case NR21: return "NR21";
		case NR22: return "NR22";
		case NR23: return "NR23";
		case NR24: return "NR24";
		case NR30: return "NR30";
		case NR31: return "NR31";
		case NR32: return "NR32";
		case NR33: return "NR33";
		case NR34: return "NR34";
		case NR41: return "NR41";
		case NR42: return "NR42";
		case NR43: return "NR43";
		case NR44: return "NR44";
		case NR50: return "NR50";
		case NR51: return "NR51";
		case NR52: return "NR52";
		case LCDC: return "LCDC";
		case STAT: return "STAT";
		case SCY: return "SCY";
		case SCX: return "SCX";
		case LY: return "LY";
		case LYC: return "LYC";
		case DMA: return "DMA";
		case BGP: return "BGP";
		case OBP0: return "OBP0";
		case OBP1: return "OBP1";
		case WY: return "WY";
		case WX: return "WX";
		case KEY0: return "KEY0";
		case KEY1: return "KEY1";
		case VBK: return "VBK";
		case BOOT: return "BOOT";
		case HDMA1: return "HDMA1";
		case HDMA2: return "HDMA2";
		case HDMA3: return "HDMA3";
		case HDMA4: return "HDMA4";
		case HDMA5: return "HDMA5";
		case RP: return "RP";
		case BCPS: return "BCPS";
		case BCPD: return "BCPD";
		case OCPS: return "OCPS";
		case OCPD: return "OCPD";
		case OPRI: return "OPRI";
		case SVBK: return "SVBK";
		case PCM12: return "PCM12";
		case PCM34: return "PCM34";
		case IE: return "IE";
		default: return {};
		}
	}


	bool LoadBootRom(const std::string& path)
	{
		//std::optional<std::vector<u8>> opt_vec = Util::Files::LoadBinaryFileVec(path);
		//if (!opt_vec.has_value()) {
		//	UserMessage::Show(std::format("Could not load boot rom at path {}", path), 
		//		UserMessage::Type::Warning);
		//	boot_rom_mapped = false;
		//	return false;
		//}
		//cgb_boot_rom = opt_vec.value();
		//boot_rom_mapped = true;
		return true;
	}


	u8 Peek(u16 addr)
	{
		/* No read in my emulator has side-effects. */
		return Read(addr);
	}


	u8 Read(u16 addr)
	{
		switch (addr >> 12) {
		case 0: /* $0000-$0FFF -- Cartridge ROM / boot ROM (0-FF DMG / 0-8FF CGB) */
			if (boot_rom_mapped) {
				if (System::mode == System::Mode::DMG && addr < Boot::dmg_boot_rom.size()) {
					return Boot::dmg_boot_rom[addr];
				}
				else if (System::mode == System::Mode::CGB && addr < Boot::cgb_boot_rom.size()) {
					return Boot::cgb_boot_rom[addr];
				}
				else {
					return Cartridge::ReadRom(addr);
				}
			}
			else {
				return Cartridge::ReadRom(addr);
			}

		case 1: case 2: case 3: case 4: case 5: case 6: case 7: /* $1000-$7FFF -- Cartridge ROM */
			return Cartridge::ReadRom(addr);

		case 8: case 9: /* $8000-$9FFF -- VRAM */
			return PPU::ReadVramCpu(addr);

		case 0xA: case 0xB: /* $A000-$BFFF -- Cartridge RAM */
			return Cartridge::ReadRam(addr);

		case 0xC: /* $C000-$CFFF -- WRAM bank 0 */
			return wram[addr - 0xC000];

		case 0xD: /* $D000-$DFFF -- WRAM bank 1-7 (switchable in GBC mode only; in DMG mode always 1) */
			return wram[addr - 0xD000 + current_wram_bank * wram_bank_size];

		case 0xE: /* $E000-$EFFF -- ECHO; mirror of $C000-$CFFF */
			return wram[addr - 0xE000];

		case 0xF:
			if (addr <= 0xFDFF) { /* $F000-$FDFF -- ECHO; mirror of $D000-$DDFF */
				return wram[addr - 0xF000 + current_wram_bank * wram_bank_size];
			}
			else if (addr <= 0xFE9F) { /* $FE00-$FE9F -- OAM */
				return PPU::ReadOamCpu(addr - 0xFE00);
			}
			else if (addr <= 0xFEFF) { /* $FEA0-$FEFF -- "Unused" */
				// In DMG mode, reading returns 0. In CGB mode, see section 2.10 in TCAGBD.pdf
				if (System::mode == System::Mode::CGB) {
					return (PPU::ReadLCDC() & 3) == 3
						? 0xFF
						: unused_memory_area[addr - 0xFEA0];
				}
				else {
					return 0;
				}
			}
			else if (addr <= 0xFF7F) { /* $FF00-$FF7F -- I/O */
				u8 value = ReadIO(addr);
				if constexpr (Debug::log_io) {
					Debug::LogIoRead(addr, value);
				}
				return value;
			}
			else if (addr <= 0xFFFE) { /* $FF80-$FFFE -- HRAM */
				return hram[addr - 0xFF80];
			}
			else { /* $FFFF -- IE */
				u8 value = CPU::ReadIE();
				if constexpr (Debug::log_io) {
					Debug::LogIoRead(addr, value);
				}
				return value;
			}
		}
		return 0xFF;
	}


	u8 ReadIO(u16 addr)
	{
		switch (addr) {
		case Addr::P1: return Joypad::ReadP1();
		case Addr::SB: return Serial::ReadSB();
		case Addr::SC: return Serial::ReadSC();
		case Addr::DIV: return Timer::ReadDIV();
		case Addr::TIMA: return Timer::ReadTIMA();
		case Addr::TMA: return Timer::ReadTMA();
		case Addr::TAC: return Timer::ReadTAC();
		case Addr::IF: return CPU::ReadIF();
		case Addr::NR10: return APU::ReadReg<APU::Reg::NR10>();
		case Addr::NR11: return APU::ReadReg<APU::Reg::NR11>();
		case Addr::NR12: return APU::ReadReg<APU::Reg::NR12>();
		case Addr::NR13: return APU::ReadReg<APU::Reg::NR13>();
		case Addr::NR14: return APU::ReadReg<APU::Reg::NR14>();
		case Addr::NR21: return APU::ReadReg<APU::Reg::NR21>();
		case Addr::NR22: return APU::ReadReg<APU::Reg::NR22>();
		case Addr::NR23: return APU::ReadReg<APU::Reg::NR23>();
		case Addr::NR24: return APU::ReadReg<APU::Reg::NR24>();
		case Addr::NR30: return APU::ReadReg<APU::Reg::NR30>();
		case Addr::NR31: return APU::ReadReg<APU::Reg::NR31>();
		case Addr::NR32: return APU::ReadReg<APU::Reg::NR32>();
		case Addr::NR33: return APU::ReadReg<APU::Reg::NR33>();
		case Addr::NR34: return APU::ReadReg<APU::Reg::NR34>();
		case Addr::NR41: return APU::ReadReg<APU::Reg::NR41>();
		case Addr::NR42: return APU::ReadReg<APU::Reg::NR42>();
		case Addr::NR43: return APU::ReadReg<APU::Reg::NR43>();
		case Addr::NR44: return APU::ReadReg<APU::Reg::NR44>();
		case Addr::NR50: return APU::ReadReg<APU::Reg::NR50>();
		case Addr::NR51: return APU::ReadReg<APU::Reg::NR51>();
		case Addr::NR52: return APU::ReadReg<APU::Reg::NR52>();
		case Addr::LCDC: return PPU::ReadLCDC();
		case Addr::STAT: return PPU::ReadSTAT();
		case Addr::SCY: return PPU::ReadSCY();
		case Addr::SCX: return PPU::ReadSCX();
		case Addr::LY: return PPU::ReadLY();
		case Addr::LYC: return PPU::ReadLYC();
		case Addr::DMA: return DMA::ReadReg<DMA::Reg::DMA>();
		case Addr::BGP: return PPU::ReadBGP();
		case Addr::OBP0: return PPU::ReadOBP0();
		case Addr::OBP1: return PPU::ReadOBP1();
		case Addr::WY: return PPU::ReadWY();
		case Addr::WX: return PPU::ReadWX();
		case Addr::KEY1: return System::ReadKey1();
		case Addr::VBK: return PPU::ReadVBK();
		case Addr::HDMA1: return DMA::ReadReg<DMA::Reg::HDMA1>();
		case Addr::HDMA2: return DMA::ReadReg<DMA::Reg::HDMA2>();
		case Addr::HDMA3: return DMA::ReadReg<DMA::Reg::HDMA3>();
		case Addr::HDMA4: return DMA::ReadReg<DMA::Reg::HDMA4>();
		case Addr::HDMA5: return DMA::ReadReg<DMA::Reg::HDMA5>();
		case Addr::RP: return 0xFF; // todo
		case Addr::BCPS: return PPU::ReadBCPS();
		case Addr::BCPD: return PPU::ReadBCPD();
		case Addr::OCPS: return PPU::ReadOCPS();
		case Addr::OCPD: return PPU::ReadOCPD();
		case Addr::OPRI: return PPU::ReadOPRI();
		case Addr::PCM12: return APU::ReadReg<APU::Reg::PCM12>();
		case Addr::PCM34: return APU::ReadReg<APU::Reg::PCM34>();

		case Addr::SVBK:
			return System::mode == System::Mode::CGB
				? current_wram_bank | 0xF8
				: 0xFF;

		case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
		case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
		case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
		case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
			return APU::ReadWaveRamCpu(addr);

		default:
			return 0xFF;
		}
	}


	u8 ReadPageFF(u8 offset)
	{
		u16 addr = 0xFF00 | offset;
		if (addr <= 0xFF7F) { /* $FF00-$FF7F -- I/O */
			u8 value = ReadIO(addr);
			if constexpr (Debug::log_io) {
				Debug::LogIoRead(addr, value);
			}
			return value;
		}
		else if (addr <= 0xFFFE) { /* $FF80-$FFFE -- HRAM */
			return hram[addr - 0xFF80];
		}
		else { /* $FFFF -- IE */
			u8 value = CPU::ReadIE();
			if constexpr (Debug::log_io) {
				Debug::LogIoRead(addr, value);
			}
			return value;
		}
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamPrimitive(boot_rom_mapped);
		stream.StreamPrimitive(current_wram_bank);
		stream.StreamArray(wram);
		stream.StreamArray(unused_memory_area);
		stream.StreamArray(hram);
	}


	void Write(const u16 addr, const u8 data)
	{
		switch (addr >> 12) {
		case 0: /* $0000-$0FFF -- Cartridge ROM / boot ROM */
			Cartridge::WriteRom(addr, data);
			break;

		case 1: case 2: case 3: case 4: case 5: case 6: case 7: /* $1000-$7FFF -- Cartridge ROM */
			Cartridge::WriteRom(addr, data);
			break;

		case 8: case 9: /* $8000-$9FFF -- VRAM */
			PPU::WriteVramCpu(addr, data);
			break;

		case 0xA: case 0xB: /* $A000-$BFFF -- Cartridge RAM */
			Cartridge::WriteRam(addr, data);
			break;

		case 0xC: /* $C000-$CFFF -- WRAM bank 0 */
			wram[addr - 0xC000] = data;;
			break;

		case 0xD: /* $D000-$DFFF -- WRAM bank 1-7 (switchable in GBC mode only; in DMG mode always 1) */
			wram[addr - 0xD000 + current_wram_bank * wram_bank_size] = data;
			break;

		case 0xE: /* $E000-$EFFF -- ECHO; mirror of $C000-$CFFF */
			wram[addr - 0xE000] = data;
			break;

		case 0xF:
			if (addr <= 0xFDFF) { /* $E000-$FDFF -- ECHO; mirror of $D000-$DDFF */
				wram[addr - 0xF000 + current_wram_bank * wram_bank_size] = data;
			}
			else if (addr <= 0xFE9F) { /* $FE00-$FE9F -- OAM */
				PPU::WriteOamCpu(addr - 0xFE00, data);
			}
			else if (addr <= 0xFEFF) { /* $FEA0-$FEFF -- "Unused" */
				// In DMG mode, writing is ignored. In CGB mode, see section 2.10 in TCAGBD.pdf
				if (System::mode == System::Mode::CGB && (PPU::ReadLCDC() & 3) != 3) {
					if (addr <= 0xFEBF) {
						unused_memory_area[addr - 0xFEA0] = data;
					}
					else {
						unused_memory_area[addr & 0xF | 0x20] = data;
						unused_memory_area[addr & 0xF | 0x30] = data;
						unused_memory_area[addr & 0xF | 0x40] = data;
						unused_memory_area[addr & 0xF | 0x50] = data;
					}
				}
			}
			else if (addr <= 0xFF7F) { /* $FF00-$FF7F -- I/O */
				if constexpr (Debug::log_io) {
					Debug::LogIoWrite(addr, data);
				}
				WriteIO(addr, data);
			}
			else if (addr <= 0xFFFE) { /* $FF80-$FFFE -- HRAM */
				hram[addr - 0xFF80] = data;
			}
			else { /* $FFFF -- IE */
				if constexpr (Debug::log_io) {
					Debug::LogIoWrite(addr, data);
				}
				CPU::WriteIE(data);
			}
		}
	}


	void WriteIO(u16 addr, u8 data)
	{
		switch (addr) {
		case Addr::P1: Joypad::WriteP1(data); break;
		case Addr::SB: Serial::WriteSB(data); break;
		case Addr::SC: Serial::WriteSC(data); break;
		case Addr::DIV: Timer::WriteDIV(data); break;
		case Addr::TIMA: Timer::WriteTIMA(data); break;
		case Addr::TMA: Timer::WriteTMA(data); break;
		case Addr::TAC: Timer::WriteTAC(data); break;
		case Addr::IF: CPU::WriteIF(data); break;
		case Addr::NR10: APU::WriteReg<APU::Reg::NR10>(data); break;
		case Addr::NR11: APU::WriteReg<APU::Reg::NR11>(data); break;
		case Addr::NR12: APU::WriteReg<APU::Reg::NR12>(data); break;
		case Addr::NR13: APU::WriteReg<APU::Reg::NR13>(data); break;
		case Addr::NR14: APU::WriteReg<APU::Reg::NR14>(data); break;
		case Addr::NR21: APU::WriteReg<APU::Reg::NR21>(data); break;
		case Addr::NR22: APU::WriteReg<APU::Reg::NR22>(data); break;
		case Addr::NR23: APU::WriteReg<APU::Reg::NR23>(data); break;
		case Addr::NR24: APU::WriteReg<APU::Reg::NR24>(data); break;
		case Addr::NR30: APU::WriteReg<APU::Reg::NR30>(data); break;
		case Addr::NR31: APU::WriteReg<APU::Reg::NR31>(data); break;
		case Addr::NR32: APU::WriteReg<APU::Reg::NR32>(data); break;
		case Addr::NR33: APU::WriteReg<APU::Reg::NR33>(data); break;
		case Addr::NR34: APU::WriteReg<APU::Reg::NR34>(data); break;
		case Addr::NR41: APU::WriteReg<APU::Reg::NR41>(data); break;
		case Addr::NR42: APU::WriteReg<APU::Reg::NR42>(data); break;
		case Addr::NR43: APU::WriteReg<APU::Reg::NR43>(data); break;
		case Addr::NR44: APU::WriteReg<APU::Reg::NR44>(data); break;
		case Addr::NR50: APU::WriteReg<APU::Reg::NR50>(data); break;
		case Addr::NR51: APU::WriteReg<APU::Reg::NR51>(data); break;
		case Addr::NR52: APU::WriteReg<APU::Reg::NR52>(data); break;
		case Addr::LCDC: PPU::WriteLCDC(data); break;
		case Addr::STAT: PPU::WriteSTAT(data); break;
		case Addr::SCY: PPU::WriteSCY(data); break;
		case Addr::SCX: PPU::WriteSCX(data); break;
		case Addr::LY: PPU::WriteLY(data); break;
		case Addr::LYC: PPU::WriteLYC(data); break;
		case Addr::DMA: DMA::WriteReg<DMA::Reg::DMA>(data); break;
		case Addr::BGP: PPU::WriteBGP(data); break;
		case Addr::OBP0: PPU::WriteOBP0(data); break;
		case Addr::OBP1: PPU::WriteOBP1(data); break;
		case Addr::WY: PPU::WriteWY(data); break;
		case Addr::WX: PPU::WriteWX(data); break;
		case Addr::KEY1: System::WriteKey1(data); break;
		case Addr::VBK: PPU::WriteVBK(data); break;
		case Addr::BOOT: boot_rom_mapped = false; break;
		case Addr::HDMA1: DMA::WriteReg<DMA::Reg::HDMA1>(data); break;
		case Addr::HDMA2: DMA::WriteReg<DMA::Reg::HDMA2>(data); break;
		case Addr::HDMA3: DMA::WriteReg<DMA::Reg::HDMA3>(data); break;
		case Addr::HDMA4: DMA::WriteReg<DMA::Reg::HDMA4>(data); break;
		case Addr::HDMA5: DMA::WriteReg<DMA::Reg::HDMA5>(data); break;
		case Addr::BCPS: PPU::WriteBCPS(data); break;
		case Addr::BCPD: PPU::WriteBCPD(data); break;
		case Addr::OCPS: PPU::WriteOCPS(data); break;
		case Addr::OCPD: PPU::WriteOCPD(data); break;
		case Addr::OPRI: PPU::WriteOPRI(data); break;
		case Addr::PCM12: APU::WriteReg<APU::Reg::PCM12>(data); break;
		case Addr::PCM34: APU::WriteReg<APU::Reg::PCM34>(data); break;

		case Addr::SVBK: // 0xFF70
			if (System::mode == System::Mode::CGB) {
				current_wram_bank = std::min(1, data & 7); // selecting bank 0 will select bank 1
			}
			break;

		case 0xFF30: case 0xFF31: case 0xFF32: case 0xFF33:
		case 0xFF34: case 0xFF35: case 0xFF36: case 0xFF37:
		case 0xFF38: case 0xFF39: case 0xFF3A: case 0xFF3B:
		case 0xFF3C: case 0xFF3D: case 0xFF3E: case 0xFF3F:
			APU::WriteWaveRamCpu(addr, data);
			break;

		default:
			break;
		}
	}


	void WritePageFF(u8 offset, u8 data)
	{
		u16 addr = 0xFF00 | offset;
		if (addr <= 0xFF7F) { /* $FF00-$FF7F -- I/O */
			if constexpr (Debug::log_io) {
				Debug::LogIoWrite(addr, data);
			}
			WriteIO(addr, data);
		}
		else if (addr <= 0xFFFE) { /* $FF80-$FFFE -- HRAM */
			hram[addr - 0xFF80] = data;
		}
		else { /* $FFFF -- IE */
			if constexpr (Debug::log_io) {
				Debug::LogIoWrite(addr, data);
			}
			CPU::WriteIE(data);
		}
	}
}