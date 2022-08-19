export module CPU;

import Util;

import <array>;
import <bit>;
import <cassert>;
import <cstring>;
import <format>;
import <utility>;

namespace CPU
{
	export
	{
		enum class Interrupt : u8 {
			VBlank = 1 << 0,
			Stat = 1 << 1,
			Timer = 1 << 2,
			Serial = 1 << 3,
			Joypad = 1 << 4
		};

		void Initialize(bool hle_boot_rom);
		bool IsHalted();
		bool IsStopped();
		u8 ReadIE();
		u8 ReadIF();
		void RequestInterrupt(Interrupt interrupt);
		void Run();
		void StreamState(SerializationStream& stream);
		void WriteIE(u8 data);
		void WriteIF(u8 data);
	}

	enum class Condition {
		Carry, Zero, NCarry, NZero, True
	};

	enum class Reg16 {
		AF, BC, DE, HL, PC, SP
	};

	template<Condition> bool EvalCond();

	template<Reg16> u16 GetReg16();

	template<Reg16> void SetReg16(u16 value);

	void CheckInterrupts();
	void ExitSpeedSwitch();
	u8 GetReg8(uint index);
	void InitiateSpeedSwitch();
	void PopPC();
	void PushPC();
	u8 Read8();
	u16 Read16();
	u8 ReadCycle(u16 addr);
	u8 ReadCyclePageFF(u8 offset);
	u8 ReadCyclePC();
	void SetReg8(uint index, u8 value);
	void WaitCycle();
	void Write8(u8 value);
	void Write16(u16 value);
	void WriteCycle(u16 addr, u8 data);
	void WriteCyclePageFF(u8 offset, u8 data);

	/* load instructions */
	template<Reg16> void LD_r16_u16();
	template<Reg16> void LD_r16_A();
	template<Reg16> void LD_A_r16();
	void LD_r8_r8();
	void LD_r8_u8();
	void LD_SP_HL();
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

	/* arithmetic and bitwise instructions */
	template<Reg16> void ADD_HL_r16();
	template<Reg16> void DEC_r16();
	template<Reg16> void INC_r16();
	void ADC_r8();
	void ADC_u8();
	void ADD_r8();
	void ADD_SP();
	void ADD_u8();
	void AND_r8();
	void AND_u8();
	void CP_r8();
	void CP_u8();
	void DEC_r8();
	void INC_r8();
	void OR_r8();
	void OR_u8();
	void SBC_r8();
	void SBC_u8();
	void SUB_r8();
	void SUB_u8();
	void XOR_r8();
	void XOR_u8();

	/* arithmetic and bitwise instructions generalized to both r8 and u8 operands */
	void ADC(u8 op);
	void ADD(u8 op);
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

	/* rotate instructions */
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

	/* jump/branch instructions */
	template<Condition> void CALL();
	template<Condition> void JP_u16();
	template<Condition> void JR();
	template<Condition> void RET();
	void JP_HL();
	void RETI();
	void RST();

	/* prefixed instructions */
	void CB();

	/* misc. instructions */
	template<Reg16> void POP();
	template<Reg16> void PUSH();
	void CCF();
	void CPL();
	void DAA();
	void DI();
	void EI();
	void HALT();
	void Illegal();
	void NOP();
	void SCF();
	void STOP();

	/* Make this constexpr, and MSVC will freak out */
	std::array instr_table = [] {
		using enum Condition;
		using enum Reg16;
		constexpr std::array table = {
			NOP       , LD_r16_u16<BC>, LD_r16_A<BC>, INC_r16<BC>, INC_r8, DEC_r8, LD_r8_u8, RLCA, /* 0x */
			LD_u16_SP , ADD_HL_r16<BC>, LD_A_r16<BC>, DEC_r16<BC>, INC_r8, DEC_r8, LD_r8_u8, RRCA, /* 0x */
			STOP      , LD_r16_u16<DE>, LD_r16_A<DE>, INC_r16<DE>, INC_r8, DEC_r8, LD_r8_u8, RLA , /* 1x */
			JR<True>  , ADD_HL_r16<DE>, LD_A_r16<DE>, DEC_r16<DE>, INC_r8, DEC_r8, LD_r8_u8, RRA , /* 1x */
			JR<NZero> , LD_r16_u16<HL>, LD_HLp_A    , INC_r16<HL>, INC_r8, DEC_r8, LD_r8_u8, DAA , /* 2x */
			JR<Zero>  , ADD_HL_r16<HL>, LD_A_HLp    , DEC_r16<HL>, INC_r8, DEC_r8, LD_r8_u8, CPL , /* 2x */
			JR<NCarry>, LD_r16_u16<SP>, LD_HLm_A    , INC_r16<SP>, INC_r8, DEC_r8, LD_r8_u8, SCF , /* 3x */
			JR<Carry> , ADD_HL_r16<SP>, LD_A_HLm    , DEC_r16<SP>, INC_r8, DEC_r8, LD_r8_u8, CCF , /* 3x */

			LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, /* 4x */
			LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, /* 4x */
			LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, /* 5x */
			LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, /* 5x */
			LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, /* 6x */
			LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, /* 6x */
			LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, HALT    , LD_r8_r8, /* 7x */
			LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, LD_r8_r8, /* 7x */

			ADD_r8, ADD_r8, ADD_r8, ADD_r8, ADD_r8, ADD_r8, ADD_r8, ADD_r8, /* 8x */
			ADC_r8, ADC_r8, ADC_r8, ADC_r8, ADC_r8, ADC_r8, ADC_r8, ADC_r8, /* 8x */
			SUB_r8, SUB_r8, SUB_r8, SUB_r8, SUB_r8, SUB_r8, SUB_r8, SUB_r8, /* 9x */
			SBC_r8, SBC_r8, SBC_r8, SBC_r8, SBC_r8, SBC_r8, SBC_r8, SBC_r8, /* 9x */
			AND_r8, AND_r8, AND_r8, AND_r8, AND_r8, AND_r8, AND_r8, AND_r8, /* Ax */
			XOR_r8, XOR_r8, XOR_r8, XOR_r8, XOR_r8, XOR_r8, XOR_r8, XOR_r8, /* Ax */
			OR_r8 , OR_r8 , OR_r8 , OR_r8 , OR_r8 , OR_r8 , OR_r8 , OR_r8 , /* Bx */
			CP_r8 , CP_r8 , CP_r8 , CP_r8 , CP_r8 , CP_r8 , CP_r8 , CP_r8 , /* Bx */

			RET<NZero> , POP<BC>  , JP_u16<NZero> , JP_u16<True> , CALL<NZero> , PUSH<BC>  , ADD_u8, RST, /* Cx */
			RET<Zero>  , RET<True>, JP_u16<Zero>  , CB           , CALL<Zero>  , CALL<True>, ADC_u8, RST, /* Cx */
			RET<NCarry>, POP<DE>  , JP_u16<NCarry>, Illegal      , CALL<NCarry>, PUSH<DE>  , SUB_u8, RST, /* Dx */
			RET<Carry> , RETI     , JP_u16<Carry> , Illegal      , CALL<Carry> , Illegal   , SBC_u8, RST, /* Dx */
			LDH_u8_A   , POP<HL>  , LDH_C_A       , Illegal      , Illegal     , PUSH<HL>  , AND_u8, RST, /* Ex */
			ADD_SP     , JP_HL    , LD_u16_A      , Illegal      , Illegal     , Illegal   , XOR_u8, RST, /* Ex */
			LDH_A_u8   , POP<AF>  , LDH_A_C       , DI           , Illegal     , PUSH<AF>  , OR_u8 , RST, /* Fx */
			LD_HL_SP_s8, LD_SP_HL , LD_A_u16      , EI           , Illegal     , Illegal   , CP_u8 , RST  /* Fx */
		};
		return table;
	}();

	constexpr uint speed_switch_m_cycle_length = 2050;

	bool ei_executed = false;
	bool halt_bug = false;
	bool ime = false;
	bool in_halt_mode = false;
	bool in_stop_mode = false;
	bool instr_executed_after_ei_executed = false;
	bool speed_switch_is_active = false;

	uint speed_switch_m_cycles_remaining;

	/* opcode of instruction currently being executed */
	u8 opcode;

	/* 8-bit registers */
	u8 A, B, C, D, E, H, L;
	/* status register */
	struct Status
	{
		u8 : 4;
		u8 carry : 1;
		u8 half : 1;
		u8 neg : 1;
		u8 zero : 1;
	} F{};
	/* program counter */
	u16 pc;
	/* stack pointer */
	u16 sp;

	u8 IE, IF;

	/* last value read at address HL */
	u8 read_hl;

	constexpr std::array reg8_table = {
		&B, &C, &D, &E, &H, &L, &read_hl /* (HL) */, &A
	};
}