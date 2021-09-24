#pragma once

#include <fstream>

#include "Bus.h"
#include "System.h"
#include "Types.h"
#include "Utils.h"

//#define DEBUG
#define DEBUG_LOG_PATH "F:\\gb_trace_log.txt"

class CPU final : public Component
{

#ifdef DEBUG
	std::ofstream ofs{ DEBUG_LOG_PATH, std::ofstream::out };
	int instruction_counter = 1;
#endif

public:
	enum Interrupt : u8 { VBlank, STAT, Timer, Serial, Joypad };

	Bus* bus;

	bool in_halt_mode = false;
	bool in_stop_mode = false;
	bool HDMA_transfer_active = false; // refers to either HDMA or GDMA. During this time, the CPU is stopped
	bool speed_switch_active = false;

	void Initialize();
	void Reset(bool execute_boot_rom);
	void Run();

	void InitiateSpeedSwitch();
	void ExitSpeedSwitch();
	void RequestInterrupt(CPU::Interrupt interrupt);

	void State(Serialization::BaseFunctor& functor) override;

private:
	const unsigned speed_switch_m_cycle_count = 2050;
	const u16 interrupt_vector[5] = { 0x0040, 0x0048, 0x0050, 0x0058, 0x0060 };

	// registers
	u8 A, B, C, D, E, H, L;

	// bits 7-4 of register F
	bool F_Z, F_N, F_H, F_C;

	u16 SP, PC;

	u8* IE, *IF;

	bool EI_called = false;
	bool halt_bug = false;
	bool IME = false;
	bool instr_executed_after_EI_called = false;

	unsigned speed_switch_m_cycles_remaining = 0;

	u8 opcode; // opcode of instruction currently being executed

	void CheckInterrupts();

	u8  Read8()  { return bus->ReadCycle(PC++); };
	u16 Read16() { PC += 2; return bus->ReadCycle(PC - 2) | bus->ReadCycle(PC - 1) << 8; };

	void WaitCycle(const unsigned cycles = 1) { bus->WaitCycle(cycles); };

	// Used by instructions to determine register and flag operands, given the opcode
	bool GetCond();
	u8 GetRegMod();
	u8 GetRegDiv(u8 offset);
	void SetRegMod(u8 op);
	void SetRegDiv(u8 op, u8 offset);

	// load instructions
	void LD_r8_r8();
	void LD_r8_u8();
	void LD_r16_u16();
	void LD_SP_HL();
	void LD_r16_A();
	void LD_A_r16();
	void LD_u16_A();
	void LD_A_u16();
	void LD_u16_SP();
	void LD_HL_SP_s8();
	void LD_HLp_A();
	void LD_HLm_A();
	void LD_A_HLp();
	void LD_A_HLm();
	void LDH_u8_A();
	void LDH_A_u8();
	void LDH_C_A();
	void LDH_A_C();

	// arithmetic and logic instructions
	void ADC_r8();
	void ADC_u8();
	void ADD_A_r8();
	void ADD_A_u8();
	void ADD_HL();
	void ADD_SP();
	void AND_r8();
	void AND_u8();
	void CP_r8();
	void CP_u8();
	void DEC_r8();
	void DEC_r16();
	void INC_r8();
	void INC_r16();
	void OR_r8();
	void OR_u8();
	void SBC_r8();
	void SBC_u8();
	void SUB_r8();
	void SUB_u8();
	void XOR_r8();
	void XOR_u8();

	// arithmeticand logic instructions generalized to both r8 and u8 (op)
	void ADC(u8 op);
	void ADD_A(u8 op);
	void AND(u8 op);
	void CP(u8 op);
	void OR(u8 op);
	void SBC(u8 op);
	void SUB(u8 op);
	void XOR(u8 op);

	// bit operation instructions
	void BIT();
	void RES();
	void SET();
	void SWAP();

	// register rotation instructions
	void RL();
	void RLA();
	void RLC();
	void RLCA();
	void RR();
	void RRA();
	void RRC();
	void RRCA();
	void SLA();
	void SRA();
	void SRL();

	// branching instructions
	void CALL();
	void RET();
	void RETI();
	void JR();
	void JP_u16();
	void JP_HL();
	void RST();

	// prefixed instructions
	void CB();

	// misc instructions
	void CCF();
	void CPL();
	void DAA();
	void DI();
	void EI();
	void HALT();
	void Illegal();
	void NOP();
	void POP();
	void PUSH();
	void SCF();
	void STOP();

	// helper functions
	void PushPC();
	void PopPC();

	typedef void (CPU::* instr_t)();

	const instr_t instr_table[0x100] =
	{
		&CPU::NOP      , &CPU::LD_r16_u16, &CPU::LD_r16_A, &CPU::INC_r16, &CPU::INC_r8, &CPU::DEC_r8, &CPU::LD_r8_u8, &CPU::RLCA,
		&CPU::LD_u16_SP, &CPU::ADD_HL    , &CPU::LD_A_r16, &CPU::DEC_r16, &CPU::INC_r8, &CPU::DEC_r8, &CPU::LD_r8_u8, &CPU::RRCA,
		&CPU::STOP     , &CPU::LD_r16_u16, &CPU::LD_r16_A, &CPU::INC_r16, &CPU::INC_r8, &CPU::DEC_r8, &CPU::LD_r8_u8, &CPU::RLA ,
		&CPU::JR       , &CPU::ADD_HL    , &CPU::LD_A_r16, &CPU::DEC_r16, &CPU::INC_r8, &CPU::DEC_r8, &CPU::LD_r8_u8, &CPU::RRA ,
		&CPU::JR       , &CPU::LD_r16_u16, &CPU::LD_HLp_A, &CPU::INC_r16, &CPU::INC_r8, &CPU::DEC_r8, &CPU::LD_r8_u8, &CPU::DAA ,
		&CPU::JR       , &CPU::ADD_HL    , &CPU::LD_A_HLp, &CPU::DEC_r16, &CPU::INC_r8, &CPU::DEC_r8, &CPU::LD_r8_u8, &CPU::CPL ,
		&CPU::JR       , &CPU::LD_r16_u16, &CPU::LD_HLm_A, &CPU::INC_r16, &CPU::INC_r8, &CPU::DEC_r8, &CPU::LD_r8_u8, &CPU::SCF ,
		&CPU::JR       , &CPU::ADD_HL    , &CPU::LD_A_HLm, &CPU::DEC_r16, &CPU::INC_r8, &CPU::DEC_r8, &CPU::LD_r8_u8, &CPU::CCF ,

		&CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8,
		&CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8,
		&CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8,
		&CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8,
		&CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8,
		&CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8,
		&CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::HALT    , &CPU::LD_r8_r8,
		&CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8, &CPU::LD_r8_r8,

		&CPU::ADD_A_r8, &CPU::ADD_A_r8, &CPU::ADD_A_r8, &CPU::ADD_A_r8, &CPU::ADD_A_r8, &CPU::ADD_A_r8, &CPU::ADD_A_r8, &CPU::ADD_A_r8,
		&CPU::ADC_r8  , &CPU::ADC_r8  , &CPU::ADC_r8  , &CPU::ADC_r8  , &CPU::ADC_r8  , &CPU::ADC_r8  , &CPU::ADC_r8  , &CPU::ADC_r8  ,
		&CPU::SUB_r8  , &CPU::SUB_r8  , &CPU::SUB_r8  , &CPU::SUB_r8  , &CPU::SUB_r8  , &CPU::SUB_r8  , &CPU::SUB_r8  , &CPU::SUB_r8  ,
		&CPU::SBC_r8  , &CPU::SBC_r8  , &CPU::SBC_r8  , &CPU::SBC_r8  , &CPU::SBC_r8  , &CPU::SBC_r8  , &CPU::SBC_r8  , &CPU::SBC_r8  ,
		&CPU::AND_r8  , &CPU::AND_r8  , &CPU::AND_r8  , &CPU::AND_r8  , &CPU::AND_r8  , &CPU::AND_r8  , &CPU::AND_r8  , &CPU::AND_r8  ,
		&CPU::XOR_r8  , &CPU::XOR_r8  , &CPU::XOR_r8  , &CPU::XOR_r8  , &CPU::XOR_r8  , &CPU::XOR_r8  , &CPU::XOR_r8  , &CPU::XOR_r8  ,
		&CPU::OR_r8   , &CPU::OR_r8   , &CPU::OR_r8   , &CPU::OR_r8   , &CPU::OR_r8   , &CPU::OR_r8   , &CPU::OR_r8   , &CPU::OR_r8   ,
		&CPU::CP_r8   , &CPU::CP_r8   , &CPU::CP_r8   , &CPU::CP_r8   , &CPU::CP_r8   , &CPU::CP_r8   , &CPU::CP_r8   , &CPU::CP_r8   ,

		&CPU::RET        , &CPU::POP     , &CPU::JP_u16  , &CPU::JP_u16 , &CPU::CALL   , &CPU::PUSH   , &CPU::ADD_A_u8, &CPU::RST,
		&CPU::RET        , &CPU::RET     , &CPU::JP_u16  , &CPU::CB     , &CPU::CALL   , &CPU::CALL   , &CPU::ADC_u8  , &CPU::RST,
		&CPU::RET        , &CPU::POP     , &CPU::JP_u16  , &CPU::Illegal, &CPU::CALL   , &CPU::PUSH   , &CPU::SUB_u8  , &CPU::RST,
		&CPU::RET        , &CPU::RETI    , &CPU::JP_u16  , &CPU::Illegal, &CPU::CALL   , &CPU::Illegal, &CPU::SBC_u8  , &CPU::RST,
		&CPU::LDH_u8_A   , &CPU::POP     , &CPU::LDH_C_A , &CPU::Illegal, &CPU::Illegal, &CPU::PUSH   , &CPU::AND_u8  , &CPU::RST,
		&CPU::ADD_SP     , &CPU::JP_HL   , &CPU::LD_u16_A, &CPU::Illegal, &CPU::Illegal, &CPU::Illegal, &CPU::XOR_u8  , &CPU::RST,
		&CPU::LDH_A_u8   , &CPU::POP     , &CPU::LDH_A_C , &CPU::DI     , &CPU::Illegal, &CPU::PUSH   , &CPU::OR_u8   , &CPU::RST,
		&CPU::LD_HL_SP_s8, &CPU::LD_SP_HL, &CPU::LD_A_u16, &CPU::EI     , &CPU::Illegal, &CPU::Illegal, &CPU::CP_u8   , &CPU::RST
	};
};