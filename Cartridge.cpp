#include "Cartridge.h"

using namespace Util;

// used for reading to the bus for addresses in the ranges 0x0000-0x7FFF, 0xA000-0xBFFF
u8 Cartridge::Read(u16 address)
{
	if (address <= 0x3FFF)
	{
		// in MBC1, this "bank 0 area" may be mapped to bank 0, 0x20, 0x40 or 0x60.
		// The bank number is equal to bits 0-1 that have last been written to 0x4000-0x5FFF, shifted 5 bits to the left
		if (mbc == MBC::MBC1 && ROM_RAM_mode_select == 1 && num_ROM_banks > 0x20)
			return cartridge[address + ((current_ROM_bank & 3 << 5) % num_ROM_banks) * 0x4000];
		return cartridge[address];
	}

	// 4000-7FFF - ROM Bank 01-7F (Read Only)
	else if (address <= 0x7FFF)
	{
		return cartridge[address + current_ROM_bank * 0x4000 - 0x4000];
	}

	else if (address >= 0xA000 && address <= 0xBFFF)
	{
		if (RAM_enabled && (!RAM_bank_is_2KB || RAM_bank_is_2KB && address <= 0xA7FF))
		{
			switch (mbc)
			{
			case MBC::None:
				// this cart type can only have up to one RAM bank
				return RAM_banks[address - 0xA000];

			case MBC::MBC1:
				// if ROM mode is selected, only RAM bank 0 is accessible
				if (ROM_RAM_mode_select == 0) // meaning ROM mode
					return RAM_banks[address - 0xA000];
				return RAM_banks[address + current_RAM_bank * 0x2000 - 0xA000];

			case MBC::MBC2:
				address = 0xA000 + address % 0x200; // wrap the address to fit inside 0xA000-0xA1FF
				return 0xF0 | MBC2_RAM[address - 0xA000]; // todo: should upper four bits always return 1?

			case MBC::MBC3:
				if (RAM_RTC_mode_select == 0 && (!RAM_bank_is_2KB || RAM_bank_is_2KB && address <= 0xA7FF))
					return RAM_banks[address + current_RAM_bank * 0x2000 - 0xA000];
				else if (RAM_RTC_mode_select == 1 && RTC_enabled)
					return RTC_memory[RTC_register_select];
				return 0xFF;

			case MBC::MBC5:
				return RAM_banks[address + current_RAM_bank * 0x2000 - 0xA000];
			}
		}
	}
	return 0xFF;
}


// used for writing to the bus for addresses in the ranges 0x0000-0x7FFF, 0xA000-0xBFFF
void Cartridge::Write(u16 address, u8 data)
{
	if (mbc == MBC::None)
	{
		if (address <= 0x1FFF)
		{
			RAM_enabled = external_RAM_available && (data & 0xF) == 0xA;
		}
		else if (address >= 0xA000 && address <= 0xBFFF)
		{
			// this cart type can only have up to one RAM bank
			if (RAM_enabled && (!RAM_bank_is_2KB || RAM_bank_is_2KB && address <= 0xA7FF))
				RAM_banks[address - 0xA000] = data;
		}
	}

	else if (mbc == MBC::MBC1)
	{
		if (address <= 0x1FFF)
		{
			RAM_enabled = external_RAM_available && (data & 0xF) == 0xA;
		}
		else if (address <= 0x3FFF)
		{
			// select bits 0-4 of ROM bank number. attempting to write 0x00 to bits 0-4 will write 0x01 instead, meaning that bank numbers 0, 0x20, 0x40, 0x60 cannot be chosen
			current_ROM_bank = std::max(1, data & 0x1F) | (current_ROM_bank & 3 << 5);

			// mask the selected bank number with the total number of banks available.  
			current_ROM_bank %= num_ROM_banks;

			// Note: the masking may result in current_ROM_bank == 0
			// However, it appears that this is intended behaviour, and we should not set current_ROM_bank = 1
			// (some moon-eye MBC1 tests such as 'rom_1Mb.gb' fail if we do so) 
		}
		else if (address <= 0x5FFF)
		{
			// select bits 5-6 of ROM bank number
			current_ROM_bank = (current_ROM_bank & 0x1F) | (data & 3) << 5;
			current_ROM_bank %= num_ROM_banks;
			
			// select bits 0-1 of RAM bank number
			if (num_RAM_banks > 1)
				current_RAM_bank = (data & 3) % num_RAM_banks;
		}
		else if (address <= 0x7FFF)
		{
			ROM_RAM_mode_select = data & 1;
		}
		else if (address >= 0xA000 && address <= 0xBFFF)
		{
			if (RAM_enabled && (!RAM_bank_is_2KB || RAM_bank_is_2KB && address <= 0xA7FF))
			{
				// if ROM mode is selected, only RAM bank 0 is accessible
				if (ROM_RAM_mode_select == 0) // meaning ROM mode
					RAM_banks[address - 0xA000] = data;
				else
					RAM_banks[address + current_RAM_bank * 0x2000 - 0xA000] = data;
			}
		}
	}

	else if (mbc == MBC::MBC2)
	{
		if (address <= 0x3FFF)
		{
			// The least significant bit of the upper address byte must be '0' to enable/disable cart RAM.
			// If it is '1', then bits 0-3 of the data written control the ROM bank number mapped to 0x4000-0x7FFF (max no. of rom banks is 16)
			if (CheckBit(address, 8) == 0)
				RAM_enabled = external_RAM_available && (data & 0xF) == 0xA;
			else
			{
				// if bank 0 is selected, choose bank 1 instead
				if ((data & 0xF) != 0)
					current_ROM_bank = (data & 0xF) % num_ROM_banks;
				else
					current_ROM_bank = 0x01;

				// Note: masking 'current_ROM_bank' may result in current_ROM_bank == 0
				// However, it appears that this is intended behaviour, and we should not set current_ROM_bank = 1
				// (some moon-eye MBC2 tests such as 'rom_1Mb.gb' fail if we do so) 
			}
		}

		// chip ram. only the four lower bits of the "bytes" in this memory area are used
		// A200-BFFF are 15 "echoes of A000-A1FF
		else if (address >= 0xA000 && address <= 0xBFFF)
		{
			if (RAM_enabled && (!RAM_bank_is_2KB || RAM_bank_is_2KB && address <= 0xA7FF))
			{
				address = 0xA000 + (address % 0x200);
				MBC2_RAM[address - 0xA000] = data;
			}
		}
	}

	else if (mbc == MBC::MBC3)
	{
		if (address <= 0x1FFF)
		{
			RAM_enabled = external_RAM_available && (data & 0xF) == 0xA;
			RTC_enabled = external_clock_available && (data & 0xF) == 0xA;
		}
		else if (address <= 0x3FFF)
		{
			// select lower 7 bits of ROM bank number. writing 0x00 will select 0x01 instead
			current_ROM_bank = std::max(1, data & 0x7F) % num_ROM_banks;
		}
		else if (address <= 0x5FFF)
		{
			// select ram bank (0x00-0x03), unless 'data' in below interval, then select RTC register
			// depending on what was written, 'RAM_RTC_mode_select' is changed, which controls whether ram or rtc is read/written to when accessing 0xA000-0xBFFF
			if (data >= 0x08 && data <= 0x0C)
				RTC_register_select = data - 0x08;
			else
				current_RAM_bank = (data & 3) % num_RAM_banks;

			RAM_RTC_mode_select = (data >= 0x08 && data <= 0x0C);
		}
		else if (address <= 0x7FFF)
		{
			if (RTC_0_written && data == 1)
			{
				// todo
				std::time_t t = std::time(0);   // get time now
				std::tm* time_now = std::localtime(&t);
				RTC_memory[0] = time_now->tm_sec;
				RTC_memory[1] = time_now->tm_min;
				RTC_memory[2] = time_now->tm_hour;

				RTC_0_written = false;
			}
			RTC_0_written = data == 0;
		}
		else if (address >= 0xA000 && address <= 0xBFFF)
		{
			if (RAM_RTC_mode_select == 0 && RAM_enabled && (!RAM_bank_is_2KB || RAM_bank_is_2KB && address <= 0xA7FF))
				RAM_banks[address + current_RAM_bank * 0x2000 - 0xA000] = data;
			else if (RAM_RTC_mode_select == 1 && RTC_enabled)
				RTC_memory[RTC_register_select] = data;
		}
	}

	else if (mbc == MBC::MBC5)
	{
		if (address <= 0x1FFF)
		{
			RAM_enabled = external_RAM_available && (data & 0xF) == 0xA;
		}
		else if (address <= 0x2FFF)
		{
			// select lower 8 bits of ROM bank number. In total, it is only 9 bits. bank 0 is selectable.
			current_ROM_bank = (current_ROM_bank & 0x100) | data;
			current_ROM_bank %= num_ROM_banks;
		}
		else if (address <= 0x3FFF)
		{
			// select the 9th bit of the ROM bank number
			current_ROM_bank = (data & 1) << 8 | (current_ROM_bank & 0xFF);
			current_ROM_bank %= num_ROM_banks;
		}
		else if (address <= 0x5FFF)
		{
			// select RAM bank in range 0x0-0xF
			current_RAM_bank = (data & 0xF) % num_RAM_banks;
		}
		else if (address >= 0xA000 && address <= 0xBFFF)
		{
			if (RAM_enabled && (!RAM_bank_is_2KB || RAM_bank_is_2KB && address <= 0xA7FF))
			{
				RAM_banks[address + current_RAM_bank * 0x2000 - 0xA000] = data;
			}
		}
	}
}


void Cartridge::Reset()
{
	current_ROM_bank = 1;
	current_RAM_bank = 0;
	RAM_enabled = false;
	RAM_RTC_mode_select = 0;
	RTC_register_select = 0;
	RTC_0_written = false;
	RTC_enabled = false;

	for (int i = 0; i < std::size(cartridge); i++)
		cartridge[i] = 0;
	for (int i = 0; i < std::size(RAM_banks); i++)
		RAM_banks[i] = 0;
	for (int i = 0; i < std::size(MBC2_RAM); i++)
		MBC2_RAM[i] = 0;
	for (int i = 0; i < std::size(RTC_memory); i++)
		RTC_memory[i] = 0;
}


/// <summary>
/// Loads a .gb or .gbc rom file and returns an enum value signifying if the cart is an original game boy cart or a game boy colour one.
/// </summary>
/// <param name="path">The path to a .gb or .gbc file.</param>
/// <returns>Mode::DMG is the cart is a Game Boy cart, Mode::CGB if it is a GBC cart, or Mode::None on failure.</returns>
System::Mode Cartridge::LoadCartridge(const char* path)
{
	this->Reset();

	// open the file
	FILE* rom = fopen(path, "rb");
	if (rom == NULL)
	{
		wxMessageBox("Failed to open rom file.");
		return System::Mode::NONE;
	}

	// get the file's size and check that it's no more than 0x800000 bytes (8 MiB) in size
	fseek(rom, 0, SEEK_END);
	rom_size = ftell(rom);
	if (rom_size > size_cartridge)
	{
		wxMessageBox("Unsupported ROM size detected; max size is 8 MiB.");
		return System::Mode::NONE;
	}

	if (rom_size % 0x4000 != 0)
	{
		wxMessageBox("Invalid cartridge detected: its size must be a multiple of 16 kiB.");
		return System::Mode::NONE;
	}
	num_ROM_banks = rom_size / 0x4000;

	// read the file and put into cartridge[]
	rewind(rom);
	fread(cartridge, 1, rom_size, rom);
	fclose(rom);

	bool supported_cartridge_detected = DetectCartridgeType();
	if (!supported_cartridge_detected) return System::Mode::NONE;

	bool supported_ram_size_detected = DetectRAMSize();
	if (!supported_ram_size_detected) return System::Mode::NONE;

	bool valid_rom_size_detected = DetectROMSize();
	if (!valid_rom_size_detected) return System::Mode::NONE;

	this->cart_name = wxFileName(path).GetName();
	ReadCartridgeRAMFromDisk();

	bool is_SGB_Cart = DetectSGBFunctions();
	System::SGB_available = is_SGB_Cart;

	bool is_GBC_Cart = DetectGBCFunctions();
	if (is_GBC_Cart) return System::Mode::CGB;

	return System::Mode::DMG;
}


void Cartridge::EjectCartridge()
{
	WriteCartridgeRAMToDisk();
}


bool Cartridge::DetectCartridgeType()
{
	// note: by default, external_RAM_available, external_battery_available and external_timer_available are all false
	// note 2: MBC1 multi-carts, MBC6, MBC7 and MMM01 carts are currently not recognized
	u8 code = cartridge[Bus::Addr::CART_TYPE];
	switch (code)
	{
	case 0x00:
		mbc = MBC::None;
		return true;

	case 0x01:
	case 0x02:
	case 0x03:
		mbc = MBC::MBC1;
		external_RAM_available = code != 0x01;
		external_battery_available = code == 0x03;
		return true;

	case 0x05:
	case 0x06:
		mbc = MBC::MBC2;
		external_RAM_available = external_battery_available = code == 0x06;
		return true;

	case 0x08:
	case 0x09:
		mbc = MBC::None;
		external_RAM_available = true;
		external_battery_available = code == 0x09;
		return true;

	case 0x0F:
	case 0x10:
	case 0x11:
	case 0x12:
	case 0x13:
		mbc = MBC::MBC3;
		external_RAM_available = code == 0x10 || code == 0x12 || code == 0x13;
		external_battery_available = code == 0x0F || code == 0x10 || code == 0x13;
		external_clock_available = code == 0x0F || code == 0x10;
		return true;

	case 0x19:
	case 0x1A:
	case 0x1B:
	case 0x1C:
	case 0x1D:
	case 0x1E:
		mbc = MBC::MBC5;
		external_RAM_available = code != 0x19 && code != 0x1C;
		external_battery_available = code == 0x1B || code == 0x1E;
		return true;

	default:
		wxMessageBox(wxString::Format("Unsupported or invalid cartridge with code %i detected.", code));
		return false;
	}
}


bool Cartridge::DetectROMSize()
{
	u8 code = cartridge[Bus::Addr::ROM_SIZE];
	if (code > 0x08)
	{
		wxMessageBox("Unsupported ROM size detected; max size is 8 MiB.");
		return false;
	}
	u32 rom_size_cart = 0x8000 << code;

	// 'this->rom_size' refers to the rom size that was detected when reading the rom file into the 'cartridge' array (simply the size of the rom file)
	if (this->rom_size != rom_size_cart)
	{
		wxMessageBox(wxString::Format(
			"Error: ROM size mismatch; size was supposed to be %i bytes (code %i), but was %i bytes instead",
			rom_size_cart, code, this->rom_size));
		return false;
	}

	return true;
}


bool Cartridge::DetectRAMSize()
{
	u8 code = cartridge[Bus::Addr::RAM_SIZE];
	switch (code)
	{
	case 0x00: num_RAM_banks = 0; return true;
	case 0x01: num_RAM_banks = 1; RAM_bank_is_2KB = true; return true;
	case 0x02: num_RAM_banks = 1; RAM_bank_is_2KB = false; return true;
	case 0x03: num_RAM_banks = 4; return true;
	case 0x04: num_RAM_banks = 16; return true;
	case 0x05: num_RAM_banks = 8; return true;
	default:
		wxMessageBox(wxString::Format("Unsupported RAM size type %i detected.", code));
		return false;
	}
}


bool Cartridge::DetectGBCFunctions()
{
	u8 code = cartridge[Bus::Addr::GBC_FLAG];
	//return false;
	return code == 0xC0 || code == 0x80;
}


bool Cartridge::DetectSGBFunctions()
{
	u8 code = cartridge[Bus::Addr::SGB_FLAG];
	return code == 0x03;
}


void Cartridge::WriteCartridgeRAMToDisk()
{
	if (external_RAM_available && external_battery_available)
	{
		std::ofstream ofs(cart_name + ".sav", std::ofstream::out | std::fstream::trunc);
		if (!ofs) return;

		size_t ram_size;
		if (RAM_bank_is_2KB)
			ram_size = 0x800;
		else
			ram_size = 0x2000 * num_RAM_banks;

		ofs.write((char*)&RAM_banks, ram_size);
		ofs.close();
	}
}


void Cartridge::ReadCartridgeRAMFromDisk()
{
	if (external_RAM_available && external_battery_available)
	{
		std::ifstream ifs(cart_name + ".sav", std::ifstream::in);
		if (!ifs) return;

		size_t ram_size;
		if (RAM_bank_is_2KB)
			ram_size = 0x800;
		else
			ram_size = 0x2000 * num_RAM_banks;

		ifs.read((char*)&RAM_banks, ram_size);
		ifs.close();
	}
}


void Cartridge::Deserialize(std::ifstream& ifs)
{
	// todo: redo; new variables
	ifs.read((char*)cartridge, sizeof(u8) * size_cartridge);
	ifs.read((char*)RAM_banks, sizeof(u8) * size_RAM_banks);
	ifs.read((char*)MBC2_RAM, sizeof(u8) * size_MBC2_RAM);
	ifs.read((char*)RTC_memory, sizeof(u8) * 5);

	ifs.read((char*)&ROM_RAM_mode_select, sizeof(bool));
	ifs.read((char*)&RAM_enabled, sizeof(bool));
	ifs.read((char*)&current_RAM_bank, sizeof(u8));
	ifs.read((char*)&current_ROM_bank, sizeof(u16));
	ifs.read((char*)&RAM_RTC_mode_select, sizeof(bool));
	ifs.read((char*)&RTC_enabled, sizeof(bool));
	ifs.read((char*)&RTC_0_written, sizeof(bool));
	ifs.read((char*)&RTC_register_select, sizeof(u8));
}


void Cartridge::Serialize(std::ofstream& ofs)
{
	ofs.write((char*)cartridge, sizeof(u8) * size_cartridge);
	ofs.write((char*)RAM_banks, sizeof(u8) * size_RAM_banks);
	ofs.write((char*)MBC2_RAM, sizeof(u8) * size_MBC2_RAM);
	ofs.write((char*)RTC_memory, sizeof(u8) * 5);

	ofs.write((char*)&ROM_RAM_mode_select, sizeof(bool));
	ofs.write((char*)&RAM_enabled, sizeof(bool));
	ofs.write((char*)&current_RAM_bank, sizeof(u8));
	ofs.write((char*)&current_ROM_bank, sizeof(u16));
	ofs.write((char*)&RAM_RTC_mode_select, sizeof(bool));
	ofs.write((char*)&RTC_enabled, sizeof(bool));
	ofs.write((char*)&RTC_0_written, sizeof(bool));
	ofs.write((char*)&RTC_register_select, sizeof(u8));
}


u16 Cartridge::GetCurrentRomBank()
{
	return current_ROM_bank;
}