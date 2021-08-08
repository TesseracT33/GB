#pragma once

#include <fstream>

#include "Bus.h"
#include "System.h"
#include "Types.h"
#include "Utils.h"

//#define DEBUG
#define DEBUG_LOG_PATH "F:\\debug.txt"

class CPU final : public Component
{

#ifdef DEBUG
	std::ofstream ofs = std::ofstream(DEBUG_LOG_PATH, std::ofstream::out);
	int instruction_counter = 1;
#endif

public:
	enum Interrupt : u8 { VBlank, STAT, Timer, Serial, Joypad };

	Bus* bus;

	bool HALT = false;
	bool HDMA_transfer_active = false; // refers to either HDMA or GDMA. During this time, the CPU is stopped
	bool speed_switch_active = false;
	bool STOP = false;

	void Initialize();
	void Reset(bool execute_boot_rom);
	void Update();

	void InitiateSpeedSwitch();
	void ExitSpeedSwitch();
	void RequestInterrupt(CPU::Interrupt interrupt);

	void State(Serialization::BaseFunctor& functor) override;

private:
	// registers
	u8 A, B, C, D, E, H, L;

	// bits 7-4 of register F
	bool F_Z, F_N, F_H, F_C;

	u16 SP, PC;

	u8* IE{}, *IF{};

	// whether a jump has been performed when executing the current instruction, via e.g. CALL, RET, JP, etc.
	// if true, this increases the number of m-cycles that the instruction takes
	bool action_taken = false; 
	bool EI_called = false;
	bool HALT_bug = false;
	bool IME = false;
	bool interrupt_dispatched_on_last_update = false;

	const u16 interrupt_vector[5] = { 0x0040, 0x0048, 0x0050, 0x0058, 0x0060 };

	unsigned m_cycles_instr = 0; // how many cycles the current instruction takes to execute
	unsigned m_cycles_until_next_instr = 1;
	unsigned speed_switch_m_cycles_remaining = 0;
	unsigned instr_until_set_IME = 0;

	const unsigned speed_switch_m_cycle_count = 2050;

	const unsigned opcode_m_cycle_len[0x100] =
	{//0x 1x 2x 3x 4x 5x 6x 7x 8x 9x Ax Bx Cx Dx Ex Fx
		1, 3, 2, 2, 1, 1, 2, 1, 5, 2, 2, 2, 1, 1, 2, 1, // x0
		0, 3, 2, 2, 1, 1, 2, 1, 3, 2, 2, 2, 1, 1, 2, 1, // x1   note: cycle count for instr. 10h is set to 0. it is added manually in the function EnterSTOP()
		2, 3, 2, 2, 1, 1, 2, 1, 2, 2, 2, 2, 1, 1, 2, 1, // x2
		2, 3, 2, 2, 3, 3, 3, 1, 2, 2, 2, 2, 1, 1, 2, 1, // x3

		1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, // x4
		1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, // x5
		1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, // x6
		1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, // x7

		1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, // x8
		1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, // x9
		1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, // xA
		1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 1, 1, 1, 2, // xB

		2, 3, 3, 4, 3, 4, 2, 4, 2, 4, 3, 0, 3, 6, 2, 4, // xC
		2, 3, 3, 0, 3, 4, 2, 4, 2, 4, 3, 0, 3, 0, 2, 4, // xD
		3, 3, 2, 0, 0, 4, 2, 4, 4, 1, 4, 0, 0, 0, 2, 4, // xE
		3, 3, 2, 1, 0, 4, 2, 4, 3, 2, 4, 1, 0, 0, 2, 4  // xF
	};

	// m-cycles that are added to the length of an instruction when a conditional jump has been performed, see the 'action_taken' variable
	const unsigned opcode_m_cycle_len_jump[0x100] =
	{//0x 1x 2x 3x 4x 5x 6x 7x 8x 9x Ax Bx Cx Dx Ex Fx
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // x0
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // x1
		1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, // x2
		1, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, // x3

		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // x4
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // x5
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // x6
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // x7

		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // x8
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // x9
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // xA
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // xB

		3, 0, 1, 0, 3, 0, 0, 0, 3, 0, 1, 0, 3, 0, 0, 0, // xC
		3, 0, 1, 0, 3, 0, 0, 0, 3, 0, 1, 0, 3, 0, 0, 0, // xD
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, // xE
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0  // xF
	};

	inline unsigned opcode_m_cycle_len_CB(u8 opcode) // todo: double check CB opcode lengths
	{
		// all CB opcodes of the form x6 or xE take 4 m-cycles, all others take 2 m-cycles,
		// except if x=4,5,6,7, those take 3 m-cycles
		// source: blargg instr_timing test
		if ((opcode & 0xF) == 0x6 || (opcode & 0xF) == 0xE)
			return ((opcode & 0xF0) >= 4 && (opcode & 0xF0) <= 7) ? 3 : 4;
		return 2;
	};

	void ExecuteInstruction(u8 opcode);
	unsigned OneInstruction();
	unsigned CheckInterrupts();

	inline u8  Read_u8()  { return bus->Read(PC++); };
	inline u16 Read_u16() { PC += 2; return bus->Read(PC - 2) | bus->Read(PC - 1) << 8; };
	inline s8  Read_s8()  { return (s8)bus->Read(PC++); };

	// arithmetic and logic instructions
	void ADC(u8 op);
	void ADD8(u8 op);
	void ADD16(u16 op);
	void AND(u8 op);
	void CP(u8 op);
	void DEC(u8& op);
	void INC(u8& op);
	void OR(u8 op);
	void SBC(u8 op);
	void SUB(u8 op);
	void XOR(u8 op);

	// bit operations instructions
	void BIT(u8 pos, u8* reg);
	void RES(u8 pos, u8* reg);
	void SET(u8 pos, u8* reg);
	void SWAP(u8* reg);

	// bit shift instructions
	void RL(u8* reg);
	void RLC(u8* reg);
	void RR(u8* reg);
	void RRC(u8* reg);
	void SLA(u8* reg);
	void SRA(u8* reg);
	void SRL(u8* reg);

	void CALL(bool cond);
	void RET(bool cond);
	void JR(bool cond);
	void JP(bool cond);
	void RST(u16 addr);
	void PushPC();
	void PopPC();

	// prefixed instructions
	void CB(u8 opcode);

	void DAA();

	void IllegalOpcode(u8 opcode);

	void Stop();
	void Halt();
};