export module Debug;

import CPU;

import NumericalTypes;

import <array>;
import <cassert>;
import <format>;
import <fstream>;
import <string>;
import <string_view>;
import <vector>;

namespace Debug
{
	export
	{
		std::string Disassemble(u16 pc, u16* new_pc = nullptr);
		std::vector<std::string> Disassemble(u16 pc, size_t num_instructions, u16* new_pc = nullptr);
		void LogDma(u16 src_addr);
		void LogHdma(std::string_view type, uint dst_addr, uint src_addr, uint len);
		void LogInstr(u8 opcode, u16 AF, u16 BC, u16 DE, u16 HL, u16 PC, u16 SP, u8 IE, u8 IF);
		void LogInterrupt(CPU::Interrupt interrupt);
		void LogIoRead(u16 addr, u8 value);
		void LogIoWrite(u16 addr, u8 value);
		void SetLogPath(const std::string& path);

		constexpr bool logging_enabled = false;
		constexpr bool log_instr = logging_enabled && true;
		constexpr bool log_dma = logging_enabled && true;
		constexpr bool log_interrupts = logging_enabled && true;
		constexpr bool log_io = logging_enabled && true;
	}

	constexpr std::array<u8, 256> instr_len = {
		1,3,1,1,1,1,2,1,3,1,1,1,1,1,2,1,
		1,3,1,1,1,1,2,1,2,1,1,1,1,1,2,1,
		2,3,1,1,1,1,2,1,2,1,1,1,1,1,2,1,
		2,3,1,1,1,1,2,1,2,1,1,1,1,1,2,1,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
		1,1,3,3,3,1,2,1,1,1,3,2,3,3,2,1,
		1,1,3,1,3,1,2,1,1,1,3,1,3,1,2,1,
		2,1,1,1,1,1,2,1,2,1,3,1,1,1,2,1,
		2,1,1,1,1,1,2,1,2,1,3,1,1,1,2,1
	};

	bool logging_disabled = true;

	std::ofstream log;
}