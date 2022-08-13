module Cartridge;

import System;

import Util.Files;

import UserMessage;

namespace Cartridge
{
	bool DetectCartridgeType()
	{
		// MBC1 multi-carts, MBC6, MBC7 and MMM01 carts are currently not recognized
		assert(rom.size() != 0);
		static constexpr u16 cart_type_rom_addr = 0x0147;
		u8 code = rom[cart_type_rom_addr];
		bool recognized_code = true;
		cart_type = [&] {
			switch (code) {
			case 0x00:
				has_battery = has_clock = has_ram = false;
				return CartType::NoMBC;

			case 0x01: case 0x02: case 0x03:
				has_battery = code == 0x03;
				has_clock = false;
				has_ram = code != 0x01;
				return CartType::MBC1;

			case 0x05: case 0x06:
				has_battery = code == 0x06;
				has_clock = false;
				has_ram = true;
				return CartType::MBC2;

			case 0x08: case 0x09:
				has_battery = code == 0x09;
				has_clock = false;
				has_ram = true;
				return CartType::NoMBC;

			case 0x0F: case 0x10: case 0x11: case 0x12: case 0x13:
				has_battery = code == 0x0F || code == 0x10 || code == 0x13;
				has_clock = code == 0x0F || code == 0x10;
				has_ram = code == 0x10 || code == 0x12 || code == 0x13;
				return CartType::MBC3;

			case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1E:
				has_battery = code == 0x1B || code == 0x1E;
				has_clock = false;
				has_ram = code != 0x19 && code != 0x1C;
				return CartType::MBC5;

			default:
				recognized_code = false;
				UserMessage::Show(std::format("Unrecognizable cartridge code ${:X} detected.", code), 
					UserMessage::Type::Error);
				return CartType::NoMBC;
			}
		}();
		return recognized_code;
	}


	bool DetectRamSize()
	{
		assert(rom.size() != 0);
		static constexpr u16 ram_size_rom_addr = 0x0149;
		u8 code = rom[ram_size_rom_addr];
		bool recognized_code = true;
		num_ram_banks = [&] {
			switch (code) {
			case 0x00:
				ram_bank_is_2KB = false;
				return 0;

			case 0x01:
				ram_bank_is_2KB = true;
				return 1;

			case 0x02:
				ram_bank_is_2KB = false;
				return 1;

			case 0x03:
				ram_bank_is_2KB = false;
				return 4;

			case 0x04:
				ram_bank_is_2KB = false;
				return 16;

			case 0x05:
				ram_bank_is_2KB = false;
				return 8;

			default:
				recognized_code = false;
				UserMessage::Show(std::format("Unrecognizable ram size code ${:X} detected.", code), 
					UserMessage::Type::Error);
				return 0;
			}
		}();
		ram_size = ram_bank_is_2KB ? 0x800 : num_ram_banks * ram_bank_size;
		return recognized_code;
	}


	bool DetectRomSize()
	{
		assert(rom.size() != 0);
		static constexpr u16 rom_size_rom_addr = 0x0148;
		u32 cart_claimed_size = 0x8000 << rom[rom_size_rom_addr];
		if (rom.size() != cart_claimed_size) {
			UserMessage::Show(std::format("ROM size mismatch; the cartridge claims it to be {} bytes, "
				"but the actual rom file is {} bytes.", cart_claimed_size, rom.size()),
				UserMessage::Type::Error);
			return false;
		}
		return true;
	}


	bool DetectGbcFunctions()
	{
		assert(rom.size() != 0);
		static constexpr u16 gbc_flag_rom_addr = 0x0143;
		u8 code = rom[gbc_flag_rom_addr];
		return code == 0xC0 || code == 0x80;
	}


	bool DetectSgbFunctions()
	{
		assert(rom.size() != 0);
		static constexpr u16 sgb_flag_rom_addr = 0x0146;
		u8 code = rom[sgb_flag_rom_addr];
		return code == 0x03;
	}


	void Initialize()
	{
		current_rom_bank = 1;
		current_ram_bank = 0;
		rom_ram_mode_select = 0;
		ram_rtc_mode_select = 0;
		rtc_register_select = 0;
		rtc_0_written = false;
		ram_enabled = rtc_enabled = false;
		std::fill(ram.begin(), ram.end(), 0);
		std::fill(mbc2_ram.begin(), mbc2_ram.end(), 0);
		std::fill(rtc_ram.begin(), rtc_ram.end(), 0);
	}


	bool LoadRom(const std::string& path)
	{
		Initialize();

		std::optional<std::vector<u8>> opt_rom = Util::Files::LoadBinaryFileVec(path);
		if (!opt_rom.has_value()) {
			UserMessage::Show(std::format("Could not open file at {}", path), UserMessage::Type::Error);
			return false;
		}
		rom = opt_rom.value();
		if (rom.size() & 0x3FFF) {
			UserMessage::Show(std::format("Rom is {} bytes large, but must be a multiple of 16 KiB.", rom.size()), UserMessage::Type::Error);
			return false;
		}
		num_rom_banks = uint(rom.size()) / rom_bank_size;

		bool supported_cartridge_detected = DetectCartridgeType();
		if (!supported_cartridge_detected) {
			return false;
		}
		bool supported_ram_size_detected = DetectRamSize();
		if (!supported_ram_size_detected) {
			return false;
		}
		// if a cartridge claims to have external ram, but its size is 0, do not let it use external ram. 
		// MBC2 always has ram, built-into the chip, but does not support external RAM.
		if (has_ram && num_ram_banks == 0 && cart_type != CartType::MBC2) {
			has_ram = false;
		}
		if (has_ram) {
			ram.resize(ram_size);
		}
		bool valid_rom_size_detected = DetectRomSize();
		if (!valid_rom_size_detected) {
			return false;
		}
		bool is_gbc_cart = DetectGbcFunctions();
		System::mode = is_gbc_cart ? System::Mode::CGB : System::Mode::DMG;

		//cart_name = std::filesystem::path::filename(path);

		ReadCartridgeRAMFromDisk();

		return true;
	}


	u8 ReadRom(const u16 addr)
	{
		if (addr <= 0x3FFF) { // $0000-$3FFF -- ROM bank 0
			// in MBC1, this "bank 0 area" may be mapped to bank 0, 0x20, 0x40 or 0x60.
			// The bank number is equal to bits 0-1 that have last been written to 0x4000-0x5FFF, shifted 5 bits to the left
			if (cart_type == CartType::MBC1 && rom_ram_mode_select == 1 && num_rom_banks > 0x20) {
				return rom[addr + ((current_rom_bank & 3 << 5) % num_rom_banks) * rom_bank_size];
			}
			else {
				return rom[addr];
			}
		}
		else { // $4000-$7FFF -- ROM Bank 1-127
			return rom[addr + current_rom_bank * rom_bank_size - rom_bank_size];
		}
	}


	u8 ReadRam(u16 addr)
	{
		// $A000-$BFFF -- RAM bank 0-3
		if (ram_enabled && (!ram_bank_is_2KB || addr <= 0xA7FF)) {
			addr &= 0x1FFF;
			switch (cart_type) {
			case CartType::NoMBC:
				return ram[addr]; // this cart type can only have up to one RAM bank

			case CartType::MBC1:
				// if ROM mode is selected, only RAM bank 0 is accessible
				if (rom_ram_mode_select == 0) { // ROM mode
					return ram[addr];
				}
				else {
					return ram[addr + current_ram_bank * ram_bank_size];
				}

			case CartType::MBC2:
				// todo: should upper four bits always return 1?
				return mbc2_ram[addr & 0x1FF] | 0xF0;

			case CartType::MBC3:
				if (ram_rtc_mode_select == 0 && (!ram_bank_is_2KB || ram_bank_is_2KB && addr <= 0xA7FF)) {
					return ram[addr + current_ram_bank * ram_bank_size];
				}
				else if (ram_rtc_mode_select == 1 && rtc_enabled) {
					return rtc_ram[rtc_register_select];
				}
				else {
					return 0xFF;
				}

			case CartType::MBC5:
				return ram[addr + current_ram_bank * ram_bank_size];

			default:
				std::unreachable();
			}
		}
		else {
			return 0xFF;
		}
	}


	void WriteRom(const u16 addr, const u8 data)
	{
		switch (cart_type) {
		case CartType::NoMBC:
			if (addr <= 0x1FFF) {
				ram_enabled = has_ram && (data & 0xF) == 0xA;
			}
			break;

		case CartType::MBC1:
			if (addr <= 0x1FFF) {
				ram_enabled = has_ram && (data & 0xF) == 0xA;
			}
			else if (addr <= 0x3FFF) {
				// select bits 0-4 of ROM bank number. attempting to write 0x00 to bits 0-4 will write 0x01 instead, meaning that bank numbers 0, 0x20, 0x40, 0x60 cannot be chosen
				current_rom_bank = std::max(1, data & 0x1F) | (current_rom_bank & 3 << 5);
				// mask the selected bank number with the total number of banks available.  
				current_rom_bank %= num_rom_banks;
				// Note: the masking may result in current_rom_bank == 0
				// However, it appears that this is intended behaviour, and we should not set current_rom_bank = 1
				// (some moon-eye MBC1 tests such as 'rom_1Mb.gb' fail if we do so) 
			}
			else if (addr <= 0x5FFF) {
				// select bits 5-6 of ROM bank number
				current_rom_bank = (current_rom_bank & 0x1F) | (data & 3) << 5;
				current_rom_bank %= num_rom_banks;
				// select bits 0-1 of RAM bank number
				if (num_ram_banks > 1) {
					current_ram_bank = (data & 3) % num_ram_banks;
				}
			}
			else {
				rom_ram_mode_select = data & 1;
			}
			break;

		case CartType::MBC2:
			if (addr <= 0x3FFF) {
				// The least significant bit of the upper addr byte must be '0' to enable/disable cart RAM.
				// If it is '1', then bits 0-3 of the data written control the ROM bank number mapped to 0x4000-0x7FFF (max no. of rom banks is 16)
				if (addr & 0x100) {
					// if bank 0 is selected, choose bank 1 instead
					if (data & 0xF) {
						current_rom_bank = (data & 0xF) % num_rom_banks;
					}
					else {
						current_rom_bank = 0x01;
					}
					// Note: masking 'current_rom_bank' may result in current_rom_bank == 0
					// However, it appears that this is intended behaviour, and we should not set current_rom_bank = 1
					// (some moon-eye MBC2 tests such as 'rom_1Mb.gb' fail if we do so) 
				}
				else {
					ram_enabled = (data & 0xF) == 0xA;
				}
			}
			break;

		case CartType::MBC3:
			if (addr <= 0x1FFF) {
				ram_enabled = has_ram && (data & 0xF) == 0xA;
				rtc_enabled = has_clock && (data & 0xF) == 0xA;
			}
			else if (addr <= 0x3FFF) {
				// select lower 7 bits of ROM bank number. writing 0x00 will select 0x01 instead
				current_rom_bank = std::max(1, data & 0x7F) % num_rom_banks;
			}
			else if (addr <= 0x5FFF) {
				// select ram bank (0x00-0x03), unless 'data' in below interval, then select RTC register
				// depending on what was written, 'RAM_RTC_mode_select' is changed, which controls whether ram or rtc is read/written to when accessing 0xA000-0xBFFF
				if (data >= 0x08 && data <= 0x0C) {
					rtc_register_select = data - 0x08;
				}
				else {
					current_ram_bank = (data & 3) % num_ram_banks;
				}
				ram_rtc_mode_select = (data >= 0x08 && data <= 0x0C);
			}
			else {
				if (rtc_0_written && data == 1) {
					// todo
					//std::time_t t = std::time(0);   // get time now
					//std::tm* time_now = std::localtime(&t);
					//rtc_ram[0] = time_now->tm_sec;
					//rtc_ram[1] = time_now->tm_min;
					//rtc_ram[2] = time_now->tm_hour;
					rtc_0_written = false;
				}
				rtc_0_written = data == 0;
			}
			break;

		case CartType::MBC5:
			if (addr <= 0x1FFF) {
				ram_enabled = has_ram && (data & 0xF) == 0xA;
			}
			else if (addr <= 0x2FFF) {
				// select lower 8 bits of ROM bank number. In total, it is only 9 bits. bank 0 is selectable.
				current_rom_bank = (current_rom_bank & 0x100) | data;
				current_rom_bank %= num_rom_banks;
			}
			else if (addr <= 0x3FFF) {
				// select the 8th bit of the ROM bank number
				current_rom_bank = (data & 1) << 8 | (current_rom_bank & 0xFF);
				current_rom_bank %= num_rom_banks;
			}
			else if (addr <= 0x5FFF) {
				// select RAM bank in range 0x0-0xF
				current_ram_bank = (data & 0xF) % num_ram_banks;
			}
			break;

		default:
			std::unreachable();
		}
	}


	void WriteRam(u16 addr, const u8 data)
	{
		addr &= 0x1FFF;
		switch (cart_type) {
		case CartType::NoMBC:
			// this cart type can only have up to one RAM bank
			if (ram_enabled && (!ram_bank_is_2KB || addr <= 0x7FF)) {
				ram[addr] = data;
			}
			break;

		case CartType::MBC1:
			if (ram_enabled && (!ram_bank_is_2KB || addr <= 0x7FF)) {
				// if ROM mode is selected, only RAM bank 0 is accessible
				if (rom_ram_mode_select == 0) { // meaning ROM mode
					ram[addr] = data;
				}
				else {
					ram[addr + current_ram_bank * ram_bank_size] = data;
				}
			}
			break;

		case CartType::MBC2:
			// Only the four lower bits of the "bytes" in this memory area are used
			// A200-BFFF are 15 "echoes" of A000-A1FF
			if (ram_enabled && (!ram_bank_is_2KB || addr <= 0x7FF)) {
				mbc2_ram[addr & 0x1FF] = data;
			}
			break;

		case CartType::MBC3:
			if (ram_rtc_mode_select == 0 && ram_enabled && (!ram_bank_is_2KB || addr <= 0x7FF)) {
				ram[addr + current_ram_bank * ram_bank_size] = data;
			}
			else if (ram_rtc_mode_select == 1 && rtc_enabled) {
				rtc_ram[rtc_register_select] = data;
			}
			break;

		case CartType::MBC5:
			if (ram_enabled && (!ram_bank_is_2KB || addr <= 0x7FF)) {
				ram[addr + current_ram_bank * ram_bank_size] = data;
			}
			break;

		default:
			std::unreachable();
		}
	}


	void Eject()
	{
		WriteCartridgeRAMToDisk();
		ram.clear();
		rom.clear();
	}


	void WriteCartridgeRAMToDisk()
	{
		/* TODO: rewrite
		if (has_ram && has_battery) {
			std::ofstream ofs(cart_name + ".sav", std::ofstream::out | std::fstream::trunc);
			if (!ofs) return;

			size_t ram_size;
			if (ram_bank_is_2KB)
				ram_size = 0x800;
			else
				ram_size = ram_bank_size * num_ram_banks;

			ofs.write((char*)&ram, ram_size);
			ofs.close();
		}
		*/
	}


	void ReadCartridgeRAMFromDisk()
	{
		/* TODO: rewrite
		if (has_ram && has_battery)
		{
			std::ifstream ifs(cart_name + ".sav", std::ifstream::in);
			if (!ifs) return;

			std::size_t ram_size;
			if (ram_bank_is_2KB)
				ram_size = 0x800;
			else
				ram_size = ram_bank_size * num_ram_banks;

			ifs.read((char*)&ram, ram_size);
			ifs.close();
		}
		*/
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamPrimitive(ram_enabled);
		stream.StreamPrimitive(ram_rtc_mode_select);
		stream.StreamPrimitive(rom_ram_mode_select);
		stream.StreamPrimitive(rtc_0_written);
		stream.StreamPrimitive(rtc_enabled);
		stream.StreamPrimitive(current_ram_bank);
		stream.StreamPrimitive(current_rom_bank);
		stream.StreamPrimitive(rtc_register_select);
		stream.StreamArray(mbc2_ram);
		stream.StreamArray(rtc_ram);
		stream.StreamVector(ram);
	}
}