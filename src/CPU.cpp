module CPU;

import Bus;
import Debug;
import DMA;
import System;
import Timer;
import UserMessage;

namespace CPU
{
	bool IsHalted()
	{
		return in_halt_mode;
	}


	bool IsStopped()
	{
		return in_stop_mode;
	}


	void Initialize(bool hle_boot_rom)
	{
		if (hle_boot_rom) {
			switch (System::mode) {
			case System::Mode::DMG:
				A = 0x01;
				B = 0x00;
				C = 0x13;
				D = 0x00;
				E = 0xD8;
				H = 0x01;
				L = 0x4D;
				F.neg = F.half = F.carry = 0;
				F.zero = 1;
				pc = 0x0100;
				sp = 0xFFFE;
				break;

			case System::Mode::CGB:
				A = 0x11;
				B = 0x00;
				C = 0x00;
				D = 0xFF;
				E = 0x56;
				H = 0x00;
				L = 0x0D;
				F.neg = F.half = F.carry = 0;
				F.zero = 1;
				pc = 0x0100;
				sp = 0xFFFE;
				break;

			default:
				assert(false);
			}
		}
		else {
			sp = pc = 0;
			A = B = C = D = E = H = L = 0;
			std::memset(&F, 0, sizeof(F));
		}
		halt_bug = ime = in_halt_mode = in_stop_mode = ei_executed = false;
		speed_switch_is_active = false;
	}


	u8 ReadCycle(u16 addr)
	{
		u8 data = Bus::Read(addr);
		System::StepAllComponentsButCpu();
		return data;
	}


	u8 ReadCyclePageFF(u8 offset)
	{
		u8 data = Bus::ReadPageFF(offset);
		System::StepAllComponentsButCpu();
		return data;
	}


	u8 ReadCyclePC()
	{
		u8 data = Bus::ReadPC(pc++);
		System::StepAllComponentsButCpu();
		return data;
	}


	void WaitCycle()
	{
		System::StepAllComponentsButCpu();
	}


	void WriteCycle(u16 addr, u8 data)
	{
		Bus::Write(addr, data);
		System::StepAllComponentsButCpu();
	}


	void WriteCyclePageFF(u8 offset, u8 data)
	{
		Bus::WritePageFF(offset, data);
		System::StepAllComponentsButCpu();
	}


	void InitiateSpeedSwitch()
	{
		speed_switch_is_active = true;
		speed_switch_m_cycles_remaining = speed_switch_m_cycle_length;
	}


	void ExitSpeedSwitch()
	{
		speed_switch_is_active = false;
		System::EndSpeedSwitchInitialization();
	}


	template<Condition cond>
	bool EvalCond()
	{
		using enum Condition;
		if constexpr (cond == Carry)  return F.carry;
		if constexpr (cond == NCarry) return !F.carry;
		if constexpr (cond == Zero)   return F.zero;
		if constexpr (cond == NZero)  return !F.zero;
		if constexpr (cond == True)   return true;
	}


	u8 GetReg8(uint index)
	{
		if (index == 6) {
			read_hl = ReadCycle(H << 8 | L);
		}
		return *reg8_table[index];
	}


	void SetReg8(uint index, u8 value)
	{
		if (index == 6) {
			WriteCycle(H << 8 | L, value);
		}
		else {
			*reg8_table[index] = value;
		}
	}


	template<Reg16 reg>
	u16 GetReg16()
	{
		if constexpr (reg == Reg16::AF) return A << 8 | std::bit_cast<u8, decltype(F)>(F);
		if constexpr (reg == Reg16::BC) return B << 8 | C;
		if constexpr (reg == Reg16::DE) return D << 8 | E;
		if constexpr (reg == Reg16::HL) return H << 8 | L;
		if constexpr (reg == Reg16::PC) return pc;
		if constexpr (reg == Reg16::SP) return sp;
	}


	template<Reg16 reg>
	void SetReg16(const u16 value)
	{
		if constexpr (reg == Reg16::AF) {
			A = value >> 8 & 0xFF;
			F = std::bit_cast<decltype(F), u8>(u8(value & 0xFF));
		}
		if constexpr (reg == Reg16::BC) {
			B = value >> 8 & 0xFF;
			C = value & 0xFF;
		}
		if constexpr (reg == Reg16::DE) {
			D = value >> 8 & 0xFF;
			E = value & 0xFF;
		}
		if constexpr (reg == Reg16::HL) {
			H = value >> 8 & 0xFF;
			L = value & 0xFF;
		}
		if constexpr (reg == Reg16::PC) {
			pc = value;
		}
		if constexpr (reg == Reg16::SP) {
			sp = value;
		}
	}


	u8 Read8()
	{
		return ReadCyclePC();
	}


	u16 Read16()
	{
		u8 low = ReadCyclePC();
		u8 high = ReadCyclePC();
		return low | high << 8;
	}


	void Write8(u8 value)
	{
		WriteCycle(pc++, value);
	}


	void Write16(u16 value)
	{
		WriteCycle(pc++, value & 0xFF);
		WriteCycle(pc++, value >> 8 & 0xFF);
	}


	void Run()
	{
		// run the cpu for 5000 instructions. Each frame is 17556 m-cycles in single-speed mode.
		// This is not terribly important, as audio and video synchronization is handled elsewhere.
		uint instruction_counter = 0;
		while (instruction_counter++ < 3000) {
			if (DMA::CgbDmaCurrentlyCopyingData()) {
				WaitCycle();
				continue;
			}
			if (speed_switch_is_active) {
				WaitCycle();
				if (--speed_switch_m_cycles_remaining == 0) {
					ExitSpeedSwitch();
				}
				continue;
			}
			if (in_halt_mode) {
				CheckInterrupts();
				continue;
			}
			CheckInterrupts();
			opcode = ReadCyclePC();

			if constexpr (Debug::log_instr) {
				Debug::LogInstr(
					opcode,
					A << 8 | std::bit_cast<u8, Status>(F),
					B << 8 | C,
					D << 8 | E,
					H << 8 | L,
					pc - 1,
					sp,
					IE,
					IF
				);
			}

			// If the previous instruction was HALT, there is a hardware bug in which PC is not incremented after the current instruction
			if (halt_bug) {
				halt_bug = false;
				pc--;
			}
			instr_table[opcode]();
			// If the previous instruction was EI, the ime flag is set only after the instruction after the EI has been executed
			if (ei_executed) {
				if (instr_executed_after_ei_executed) {
					ime = 1;
					ei_executed = false;
				}
				else {
					instr_executed_after_ei_executed = true;
				}
			}
		}
	}


	void CheckInterrupts()
	{
		// This function does not only dispatch interrupts if necessary,
		// it also checks if HALT mode should be exited if the CPU is in it

		/* Behaviour for when  (IF & IE & 0x1F) != 0  :
		*  HALT,  ime : jump to interrupt vector, clear IF flag
		* !HALT,  ime : same as above
		*  HALT, !ime : don't jump to interrupt vector, don't clear IF flag
		* !HALT, !ime : do nothing
		* In addition: clear HALT flag in each case
		*/
		if (ime) {
			// fetch bits 0-4 of the IF IE registers
			u8 AND = IF & IE & 0x1F;
			if (AND) { // if any enabled interrupt is requested
				WaitCycle();
				WaitCycle();
				PushPC();
				for (int i = 0; i <= 4; ++i) {
					if (AND & 1 << i) {
						pc = 0x40 + 8 * i;
						WaitCycle();
						IF &= ~(1 << i); // todo: when does this happen exactly?
						if constexpr (Debug::log_interrupts) {
							Debug::LogInterrupt(static_cast<Interrupt>(i));
						}
					}
				}
				if (in_halt_mode) {
					WaitCycle();
				}
				ime = in_halt_mode = false;
				return;
			}
		}
		if (in_halt_mode) {
			// check if HALT mode should be exited, which is when enabled interrupt is requested
			if (IF & IE & 0x1F) {
				in_halt_mode = false;
			}
			WaitCycle();
		}
	}


	void PushPC()
	{
		WriteCycle(--sp, pc >> 8 & 0xFF);
		WriteCycle(--sp, pc & 0xFF);
	}


	void PopPC()
	{
		u8 low = ReadCycle(sp++);
		u8 high = ReadCycle(sp++);
		pc = high << 8 | low;
	}


	void RequestInterrupt(Interrupt interrupt)
	{
		IF |= std::to_underlying(interrupt);
	}


	u8 ReadIE()
	{
		return IE;
	}


	void WriteIE(u8 data)
	{
		IE = data | 0xE0;
	}


	u8 ReadIF()
	{
		return IF;
	}


	void WriteIF(u8 data)
	{
		IF = data | 0xE0;
	}


	// Load the value of the second r8 into the first r8
	void LD_r8_r8() // LD r8, r8    len: 4t if the first and second r8 != (HL), else 8t
	{
		auto dst_reg_idx = (opcode - 0x40) >> 3;
		auto src_reg_idx = opcode & 7;
		SetReg8(dst_reg_idx, GetReg8(src_reg_idx));
	}


	// Load u8 into r8
	void LD_r8_u8() // LD r8, u8    len: 8t if r8 != (HL), else 12t
	{
		auto dst_reg_idx = opcode >> 3;
		u8 immediate = Read8();
		SetReg8(dst_reg_idx, immediate);
	}


	// Load u16 into r16
	template<Reg16 r16>
	void LD_r16_u16() // LD r16, u16    len: 12t
	{
		u16 immediate = Read16();
		SetReg16<r16>(immediate);
	}


	// Load HL into SP
	void LD_SP_HL() // LD SP, HL    len: 8t
	{
		sp = H << 8 | L;
		WaitCycle();
	}


	// Load A into the byte at address r16
	template<Reg16 r16>
	void LD_r16_A() // LD (r16), A    len: 8t
	{
		WriteCycle(GetReg16<r16>(), A);
	}


	// Load the byte at address r16 into A
	template<Reg16 r16>
	void LD_A_r16() // LD A, (r16)    len: 8t
	{
		A = ReadCycle(GetReg16<r16>());
	}


	// Load A into the byte at address pointed to by u16
	void LD_u16_A() // LD (u16), A    len: 16t
	{
		u16 immediate = Read16();
		WriteCycle(immediate, A);
	}


	// Load the byte at address u16 into A
	void LD_A_u16() // LD A, (u16)    len: 16t
	{
		u16 immediate = Read16();
		A = ReadCycle(immediate);
	}


	// Load SP & 0xFF into the byte at address u16 and SP >> 8 into the byte at address u16 + 1
	void LD_u16_SP() // LD (u16), SP    len: 20t
	{
		u16 immediate = Read16();
		WriteCycle(immediate, sp & 0xFF);
		WriteCycle(immediate + 1, sp >> 8);
	}


	// Load SP+s8 into HL
	void LD_HL_SP_s8() // LD HL, SP + s8    len: 12t
	{
		s8 offset = Read8();
		F.half = (sp & 0xF) + (offset & 0xF) > 0xF; // check if overflow from bit 3
		F.carry = (sp & 0xFF) + (offset & 0xFF) > 0xFF; // check if overflow from bit 7
		u16 result = sp + offset;
		H = result >> 8;
		L = result & 0xFF;
		F.zero = F.neg = 0;
		WaitCycle();
	}


	// Load A into the byte at address HL, and thereafter increment HL by 1
	void LD_HLp_A() // LD (HL+), A    len: 8t
	{
		WriteCycle(H << 8 | L, A);
		if (++L == 0) {
			++H;
		}
	}


	// Load A into the byte at address HL, and thereafter decrement HL by 1
	void LD_HLm_A() // LD (HL-), A    len: 8t
	{
		WriteCycle(H << 8 | L, A);
		if (--L == 0xFF) {
			--H;
		}
	}


	// Load the byte at address HL into A, and thereafter increment HL by 1
	void LD_A_HLp() // LD (HL+), A    len: 8t
	{
		A = ReadCycle(H << 8 | L);
		if (++L == 0) {
			++H;
		}
	}


	// Load the byte at address HL into A, and thereafter decrement HL by 1
	void LD_A_HLm() // LD (HL-), A    len: 8t
	{
		A = ReadCycle(H << 8 | L);
		if (--L == 0xFF) {
			--H;
		}
	}


	// Load A into the byte at address FF00+u8
	void LDH_u8_A() // LD (FF00+u8), A    len: 12t
	{
		WriteCyclePageFF(Read8(), A);
	}


	// Load the byte at address FF00+u8 into A
	void LDH_A_u8() // LD A, (FF00+u8)    len: 12t
	{
		A = ReadCyclePageFF(Read8());
	}


	// Load A into the byte at address FF00+C
	void LDH_C_A() // LD (FF00+C), A    len: 8t
	{
		WriteCyclePageFF(C, A);
	}


	// Load the byte at address FF00+C into A
	void LDH_A_C() // LD A, (FF00+C)    len: 8t
	{
		A = ReadCyclePageFF(C);
	}


	// Add r8 and carry flag to register A
	void ADC_r8() // ADC A, r8    len: 4t if r8 != (HL), otherwise 8t
	{
		auto src_reg_idx = opcode & 7;
		ADC(GetReg8(src_reg_idx));
	}


	// Add u8 and carry flag to register A
	void ADC_u8() // ADC A, u8    len: 8t
	{
		ADC(Read8());
	}


	// Add r8 to register A
	void ADD_r8() // ADD A, r8    len: 4t if r8 != (HL), otherwise 8t
	{
		auto src_reg_idx = opcode & 7;
		ADD(GetReg8(src_reg_idx));
	}


	// Add u8 to register A
	void ADD_u8() // ADD A, u8    len: 8t
	{
		ADD(Read8());
	}


	// Add r16 to register HL
	template<Reg16 r16>
	void ADD_HL_r16() // ADD HL, r16    len: 8t
	{
		u16 reg = GetReg16<r16>();
		u16 HL = H << 8 | L;
		F.neg = 0;
		F.half = (HL & 0xFFF) + (reg & 0xFFF) > 0xFFF; // check if overflow from bit 11
		F.carry = (HL + reg > 0xFFFF); // check if overflow from bit 15
		u16 result = HL + reg;
		H = result >> 8;
		L = result & 0xFF;
		WaitCycle();
	}


	// Add signed value to SP
	void ADD_SP() // ADD SP, s8   len: 16t
	{
		s8 offset = Read8();
		F.half = (sp & 0xF) + (offset & 0xF) > 0xF; // check if overflow from bit 3
		F.carry = (sp & 0xFF) + (offset & 0xFF) > 0xFF; // check if overflow from bit 7
		sp += offset;
		F.zero = F.neg = 0;
		WaitCycle();
		WaitCycle();
	}


	// Store bitwise AND between r8 and A in A
	void AND_r8() // AND A, r8    len: 4t if r8 != (HL), otherwise 8t
	{
		auto src_reg_idx = opcode & 7;
		AND(GetReg8(src_reg_idx));
	}


	// Store bitwise AND between u8 and A in A
	void AND_u8() // AND A, u8    len: 8t
	{
		AND(Read8());
	}


	// Perform subtraction of r8 from A, but don't store the result
	void CP_r8() // CP A, r8    len: 4t if r8 != (HL), otherwise 8t
	{
		auto src_reg_idx = opcode & 7;
		CP(GetReg8(src_reg_idx));
	}


	// Perform subtraction of u8 from A, but don't store the result
	void CP_u8() // CP A, u8    len: 8t
	{
		CP(Read8());
	}


	// Decrement 8-bit register by 1
	void DEC_r8() // DEC r8    len: 4t if r8 != (HL), otherwise 12t
	{
		auto reg_idx = opcode >> 3;
		u8 reg = GetReg8(reg_idx);
		F.neg = 1;
		F.half = (reg & 0xF) == 0; // check if borrow from bit 4
		--reg;
		F.zero = reg == 0;
		SetReg8(reg_idx, reg);
	}


	// Decrement 16-bit register by 1
	template<Reg16 r16>
	void DEC_r16() // DEC r16    len: 8t
	{
		if constexpr (r16 == Reg16::BC) { /* DEC BC */
			if (--C == 0xFF) {
				--B;
			}
		}
		else if constexpr (r16 == Reg16::DE) { /* DEC DE */
			if (--E == 0xFF) {
				--D;
			}
		}
		else if constexpr (r16 == Reg16::HL) { /* DEC HL */
			if (--L == 0xFF) {
				--H;
			}
		}
		else if constexpr (r16 == Reg16::SP) { /* DEC SP */
			--sp;
		}
		else {
			static_assert(AlwaysFalse<r16>);
		}
		WaitCycle();
	}


	// Increment 8-bit register by 1
	void INC_r8() // INC r8    len: 4t if r8 != (HL), otherwise 12t
	{
		auto reg_idx = opcode >> 3;
		u8 reg = GetReg8(reg_idx);
		F.half = (reg & 0xF) == 0xF; // check if overflow from bit 3
		++reg;
		F.neg = 0;
		F.zero = reg == 0;
		SetReg8(reg_idx, reg);
	}


	// Increment 16-bit register by 1
	template<Reg16 r16>
	void INC_r16() // INC r16    len: 8t
	{
		if constexpr (r16 == Reg16::BC) { /* INC BC */
			if (++C == 0) {
				++B;
			}
		}
		else if constexpr (r16 == Reg16::DE) { /* INC DE */
			if (++E == 0) {
				++D;
			}
		}
		else if constexpr (r16 == Reg16::HL) { /* INC HL */
			if (++L == 0) {
				++H;
			}
		}
		else if constexpr (r16 == Reg16::SP) { /* INC SP */
			++sp;
		}
		else {
			static_assert(AlwaysFalse<r16>);
		}
		WaitCycle();
	}


	// Perform bitwise OR between A and r8, and store the result in A
	void OR_r8() // OR A, r8    len: 4t if r8 != (HL), otherwise 8t
	{
		auto src_reg_idx = opcode & 7;
		OR(GetReg8(src_reg_idx));
	}


	// Perform bitwise OR between A and u8, and store the result in A
	void OR_u8() // OR A, u8    len: 8t
	{
		OR(Read8());
	}


	// Subtract r8 and carry flag from A
	void SBC_r8() // SBC A, r8    len: 4t if r8 != (HL), otherwise 8t
	{
		auto src_reg_idx = opcode & 7;
		SBC(GetReg8(src_reg_idx));
	}


	// Subtract u8 and carry flag from A
	void SBC_u8() // SBC A, u8    len: 8t
	{
		SBC(Read8());
	}


	// Subtract r8 from A
	void SUB_r8() // SUB A, r8    len: 4t if r8 != (HL), otherwise 8t
	{
		auto src_reg_idx = opcode & 7;
		SUB(GetReg8(src_reg_idx));
	}


	// Subtract u8 from A
	void SUB_u8() // SUB A, u8    len: 8t
	{
		SUB(Read8());
	}


	// Perform bitwise XOR between A and r8, and store the result in A
	void XOR_r8() // XOR A, r8    len: 4t if r8 != (HL), otherwise 8t
	{
		auto src_reg_idx = opcode & 7;
		XOR(GetReg8(src_reg_idx));
	}


	// Perform bitwise XOR between A and u8, and store the result in A
	void XOR_u8() // XOR A, u8    len: 8t
	{
		XOR(Read8());
	}


	void ADC(const u8 op)
	{
		u16 delta = op + F.carry;
		F.neg = 0;
		F.half = (A & 0xF) + (op & 0xF) + F.carry > 0xF; // check if overflow from bit 3
		F.carry = A + delta > 0xFF; // check if overflow from bit 7
		A += delta;
		F.zero = A == 0;
	}


	void ADD(const u8 op)
	{
		F.neg = 0;
		F.half = ((A & 0xF) + (op & 0xF) > 0xF); // check if overflow from bit 3
		F.carry = A + op > 0xFF; // check if overflow from bit 7
		A += op;
		F.zero = A == 0;
	}


	void AND(const u8 op)
	{
		A &= op;
		F.zero = A == 0;
		F.neg = 0;
		F.half = 1;
		F.carry = 0;
	}


	void CP(const u8 op)
	{
		F.zero = A - op == 0;
		F.neg = 1;
		F.half = (op & 0xF) > (A & 0xF); // check if borrow from bit 4
		F.carry = op > A; // check if borrow
	}


	void OR(const u8 op)
	{
		A |= op;
		F.zero = A == 0;
		F.neg = F.half = F.carry = 0;
	}


	void SBC(const u8 op)
	{
		u16 delta = op + F.carry;
		F.neg = 1;
		F.half = (op & 0xF) + F.carry > (A & 0xF); // check if borrow from bit 4
		F.carry = delta > A; // check if borrow
		A -= delta;
		F.zero = A == 0;
	}


	void SUB(const u8 op)
	{
		F.neg = 1;
		F.half = (op & 0xF) > (A & 0xF); // check if borrow from bit 4
		F.carry = op > A; // check if borrow
		A -= op;
		F.zero = A == 0;
	}


	void XOR(const u8 op)
	{
		A ^= op;
		F.zero = A == 0;
		F.neg = F.half = F.carry = 0;
	}


	// Rotate register r8 left through carry.
	void RL() // RL r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
	{
		auto reg_idx = opcode & 7;
		u8 reg = GetReg8(reg_idx);
		bool prev_bit7 = reg >> 7;
		reg = reg << 1 | F.carry;
		F.carry = prev_bit7;
		F.zero = reg == 0;
		F.neg = F.half = 0;
		SetReg8(reg_idx, reg);
	}


	// Rotate A left through carry (and clear the zero flag)
	void RLA() // RLA    len: 4t
	{
		bool prev_bit7 = A >> 7;
		A = A << 1 | F.carry;
		F.carry = prev_bit7;
		F.zero = F.neg = F.half = 0;
	}


	// Rotate register r8 left
	void RLC() // RLC r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
	{
		auto reg_idx = opcode & 7;
		u8 reg = GetReg8(reg_idx);
		F.carry = reg >> 7;
		reg = std::rotl(reg, 1);
		F.zero = reg == 0;
		F.neg = F.half = 0;
		SetReg8(reg_idx, reg);
	}


	// Rotate A left (and clear the zero flag)
	void RLCA() // RLCA    len: 4t
	{
		F.carry = A >> 7;
		A = std::rotl(A, 1);
		F.zero = F.neg = F.half = 0;
	}


	// Rotate register r8 right through carry.
	void RR() // RR r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
	{
		auto reg_idx = opcode & 7;
		u8 reg = GetReg8(reg_idx);
		bool prev_bit0 = reg & 1;
		reg = reg >> 1 | F.carry << 7;
		F.carry = prev_bit0;
		F.zero = reg == 0;
		F.neg = F.half = 0;
		SetReg8(reg_idx, reg);
	}


	// Rotate A right through carry (and clear the zero flag)
	void RRA() // RRA    len: 4t
	{
		bool prev_bit0 = A & 1;
		A = A >> 1 | F.carry << 7;
		F.carry = prev_bit0;
		F.zero = F.neg = F.half = 0;
	}


	// Rotate register r8 right
	void RRC() // RRC r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
	{
		auto reg_idx = opcode & 7;
		u8 reg = GetReg8(reg_idx);
		F.carry = reg & 1;
		reg = std::rotr(reg, 1);
		F.zero = reg == 0;
		F.neg = F.half = 0;
		SetReg8(reg_idx, reg);
	}


	// Rotate A right (and clear the zero flag)
	void RRCA() // RRCA    len: 4t
	{
		F.carry = A & 1;
		A = std::rotr(A, 1);
		F.zero = F.neg = F.half = 0;
	}


	// Shift arithmetic register r8 left
	void SLA() // SLA r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
	{
		auto reg_idx = opcode & 7;
		u8 reg = GetReg8(reg_idx);
		F.carry = reg >> 7;
		reg <<= 1;
		F.zero = reg == 0;
		F.neg = F.half = 0;
		SetReg8(reg_idx, reg);
	}


	// Shift arithmetic register r8 right
	void SRA() // SRA r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
	{
		auto reg_idx = opcode & 7;
		u8 reg = GetReg8(reg_idx);
		F.carry = reg & 1;
		reg = reg >> 1 | reg & 0x80;
		F.zero = reg == 0;
		F.neg = F.half = 0;
		SetReg8(reg_idx, reg);
	}


	// Shift logic register r8 right
	void SRL() // SRL r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
	{
		auto reg_idx = opcode & 7;
		u8 reg = GetReg8(reg_idx);
		F.carry = reg & 1;
		reg >>= 1;
		F.zero = reg == 0;
		F.neg = F.half = 0;
		SetReg8(reg_idx, reg);
	}


	// Set the zero flag according to whether a bit with position pos in register r8 is set or not
	void BIT() // BIT pos, r8    len: 8t if r8 != (HL), otherwise 12t (prefixed instruction)
	{
		auto reg_idx = opcode & 7;
		u8 reg = GetReg8(reg_idx);
		auto pos = (opcode - 0x40) >> 3;
		F.zero = (reg & 1 << pos) == 0;
		F.neg = 0;
		F.half = 1;
	}


	// Clear bit number pos in register r8
	void RES() // RES pos, r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
	{
		auto reg_idx = opcode & 7;
		u8 reg = GetReg8(reg_idx);
		auto pos = (opcode - 0x80) >> 3;
		reg &= ~(1 << pos);
		SetReg8(reg_idx, reg);
	}


	// Set bit number pos in register r8
	void SET() // SET pos, r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
	{
		auto reg_idx = opcode & 7;
		u8 reg = GetReg8(reg_idx);
		auto pos = (opcode - 0xC0) >> 3;
		reg |= 1 << pos;
		SetReg8(reg_idx, reg);
	}


	// Swap upper 4 bits in register r8 and the lower 4 ones. 
	void SWAP() // SWAP r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
	{
		auto reg_idx = opcode & 7;
		u8 reg = GetReg8(reg_idx);
		reg = std::rotl(reg, 4);
		F.zero = reg == 0;
		F.neg = F.half = F.carry = 0;
		SetReg8(reg_idx, reg);
	}


	// Prefixed instructions
	void CB() // CB <instr>
	{
		opcode = Read8();
		switch (opcode >> 3) {
		case 0: RLC(); break;
		case 1: RRC(); break;
		case 2: RL(); break;
		case 3: RR(); break;
		case 4: SLA(); break;
		case 5: SRA(); break;
		case 6: SWAP(); break;
		case 7: SRL(); break;
		case 8: case 9: case 10: case 11: case 12: case 13: case 14: case 15: BIT(); break;
		case 16: case 17: case 18: case 19: case 20: case 21: case 22: case 23: RES(); break;
		case 24: case 25: case 26: case 27: case 28: case 29: case 30: case 31: SET(); break;
		default: std::unreachable();
		}
	}


	// Jump to subroutine; set PC to u16 if condition holds
	template<Condition cond>
	void CALL() // CALL cc, u16    len: 24t if branch is successful, else 12t
	{
		u16 target = Read16();
		if (EvalCond<cond>()) {
			WaitCycle();
			PushPC();
			pc = target;
		}
	}


	// Return from subroutine; set PC by popping the stack if condition holds
	template<Condition cond>
	void RET() // RET cc    len: 16t if opcode == $C9 (RET). Otherwise: 20t if branch is successful, else 8t
	{
		if constexpr (cond == Condition::True) {
			PopPC();
			WaitCycle();
		}
		else {
			WaitCycle();
			if (EvalCond<cond>()) {
				PopPC();
				WaitCycle();
			}
		}
	}


	// Return from subroutine and enable interrupts
	void RETI() // RETI    len: 16t
	{
		PopPC();
		WaitCycle();
		ime = 1;
	}


	// Relative jump; add a signed 8-bit integer to the address of the instruction following the JR if the condition holds
	template<Condition cond>
	void JR() // JR cc, s8    len: 12t if the branch is successful, else 8t
	{
		s8 offset = Read8();
		if (EvalCond<cond>()) {
			pc += offset;
			WaitCycle();
		}
	}


	// Absolute jump; jump to the address specified by u16 if the condition holds
	template<Condition cond>
	void JP_u16() // JP cc, u16    len: 16t if the branch is successful, else 12t
	{
		u16 target = Read16();
		if (EvalCond<cond>()) {
			pc = target;
			WaitCycle();
		}
	}


	// Absolute jump; jump to the address specified by HL
	void JP_HL() // JP HL    len: 4t
	{
		pc = H << 8 | L;
	}


	// Jump to subroutine at specific addresses
	void RST() // RST vec    len: 16t
	{
		WaitCycle();
		PushPC();
		pc = opcode - 0xC7;
	}


	// Toggle the carry flag
	void CCF() // CCF    len: 4t
	{
		F.carry = !F.carry;
		F.neg = F.half = 0;
	}


	// Invert all bits in A
	void CPL() // CPL    len: 4t
	{
		A = ~A;
		F.neg = F.half = 1;
	}


	void DAA() // DAA    len: 4t
	{
		// https://forums.nesdev.com/viewtopic.php?t=15944
		if (F.neg) {
			if (F.carry) {
				A -= 0x60;
			}
			if (F.half) {
				A -= 0x6;
			}
		}
		else {
			if (F.carry || A > 0x99) {
				A += 0x60;
				F.carry = 1;
			}
			if (F.half || (A & 0xF) > 0x9) {
				A += 0x6;
			}
		}
		F.zero = A == 0;
		F.half = 0;
	}


	// Disable interrupts
	void DI() // DI    len: 4t
	{
		ime = 0;
	}


	// Enable interrupts (delayed by one instruction)
	void EI() // EI    len: 4t
	{
		ei_executed = true;
		instr_executed_after_ei_executed = false;
	}


	// (Attempt to) enter HALT mode
	void HALT() // HALT    len: 4t
	{
		if (ime || (IF & IE & 0x1F) == 0) {
			in_halt_mode = true;
		}
		else {
			// HALT is not entered, causing a bug where the CPU fails to increase PC when executing the next instruction
			halt_bug = true;
		}
	}


	// Illegal opcode
	void Illegal()
	{
		UserMessage::Show(std::format("Illegal opcode ${:02X} encountered. Stopping emulation.", opcode),
			UserMessage::Type::Error);
	}


	// No operation
	void NOP() // NOP    len: 4t
	{

	}


	// Pop a word off of the stack and put into a 16-bit register
	template<Reg16 r16>
	void POP() // POP r16    len: 12t
	{
		if constexpr (r16 == Reg16::AF) { /* POP AF */
			F = std::bit_cast<Status, u8>(u8(ReadCycle(sp++) & 0xF0));
			A = ReadCycle(sp++);
		}
		else if constexpr (r16 == Reg16::BC) { /* POP BC */
			C = ReadCycle(sp++);
			B = ReadCycle(sp++);
		}
		else if constexpr (r16 == Reg16::DE) { /* POP DE */
			E = ReadCycle(sp++);
			D = ReadCycle(sp++);
		}
		else if constexpr (r16 == Reg16::HL) { /* POP HL */
			L = ReadCycle(sp++);
			H = ReadCycle(sp++);
		}
		else {
			static_assert(AlwaysFalse<r16>);
		}
	}


	// Push a 16-bit register onto the stack
	template<Reg16 r16>
	void PUSH() // PUSH r16    len: 16t
	{
		WaitCycle();
		if constexpr (r16 == Reg16::AF) { /* PUSH AF */
			WriteCycle(--sp, A);
			WriteCycle(--sp, std::bit_cast<u8, Status>(F));
		}
		else if constexpr (r16 == Reg16::BC) { /* PUSH BC */
			WriteCycle(--sp, B);
			WriteCycle(--sp, C);
		}
		else if constexpr (r16 == Reg16::DE) { /* PUSH DE */
			WriteCycle(--sp, D);
			WriteCycle(--sp, E);
		}
		else if constexpr (r16 == Reg16::HL) { /* PUSH HL */
			WriteCycle(--sp, H);
			WriteCycle(--sp, L);
		}
		else {
			static_assert(AlwaysFalse<r16>);
		}
	}


	// Set the carry flag
	void SCF() // SCF    len: 4t
	{
		F.carry = 1;
		F.neg = F.half = 0;
	}


	// Enter stop mode on DMG, switch between single and double speed modes on CGB
	void STOP() // STOP    len: 4t
	{
		// This instruction is very glitchy, see https://pbs.twimg.com/media/E5jlgW9XIAEKj0t.png:large
		// On DMG, enter low power mode. On the CGB, switch between normal and double speed (if KEY1.0 has been set).

		// TODO: Test if a button is being held and selected in JOYP
		// TODO: enter power mode on CGB if key1 == 0?
		bool interrupt_pending = IE & IF & 0x1F;
		if (System::SpeedSwitchPrepared()) { // If a speed switch was requested (and in CGB mode)
			if (interrupt_pending) {
				if (ime) {
					// Glitch non-deterministically.
					// For now, emulated as successfully performing a speed switch.
					InitiateSpeedSwitch();
				}
				else {
					// Successfully perform a speed switch.
					InitiateSpeedSwitch();
				}
			}
			else {
				// Successfully perform a speed switch with STOP as a 2-byte instruction. Also, enter HALT.
				++pc;
				HALT();
				InitiateSpeedSwitch();
			}
		}
		else {
			// DIV is reset and STOP mode is reset. STOP is a 2-byte opcode if an interrupt is not pending
			if (!interrupt_pending) {
				++pc;
			}
			in_stop_mode = true;
		}
		Timer::WriteDIV(0);
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamPrimitive(ei_executed);
		stream.StreamPrimitive(halt_bug);
		stream.StreamPrimitive(ime);
		stream.StreamPrimitive(in_halt_mode);
		stream.StreamPrimitive(in_stop_mode);
		stream.StreamPrimitive(instr_executed_after_ei_executed);
		stream.StreamPrimitive(speed_switch_is_active);
		stream.StreamPrimitive(speed_switch_m_cycles_remaining);
		stream.StreamPrimitive(opcode);
		stream.StreamPrimitive(A);
		stream.StreamPrimitive(B);
		stream.StreamPrimitive(C);
		stream.StreamPrimitive(D);
		stream.StreamPrimitive(E);
		stream.StreamPrimitive(H);
		stream.StreamPrimitive(L);
		stream.StreamPrimitive(F);
		stream.StreamPrimitive(pc);
		stream.StreamPrimitive(sp);
		stream.StreamPrimitive(IE);
		stream.StreamPrimitive(IF);
		stream.StreamPrimitive(read_hl);
	}


	template void CALL< Condition::Carry>();
	template void CALL< Condition::Zero>();
	template void CALL< Condition::NCarry>();
	template void CALL< Condition::NZero>();
	template void CALL< Condition::True>();
	template void JP_u16< Condition::Carry>();
	template void JP_u16< Condition::Zero>();
	template void JP_u16< Condition::NCarry>();
	template void JP_u16< Condition::NZero>();
	template void JP_u16< Condition::True>();
	template void JR< Condition::Carry>();
	template void JR< Condition::Zero>();
	template void JR< Condition::NCarry>();
	template void JR< Condition::NZero>();
	template void JR< Condition::True>();
	template void RET< Condition::Carry>();
	template void RET< Condition::Zero>();
	template void RET< Condition::NCarry>();
	template void RET< Condition::NZero>();
	template void RET< Condition::True>();

	template void ADD_HL_r16<Reg16::BC>();
	template void ADD_HL_r16<Reg16::DE>();
	template void ADD_HL_r16<Reg16::HL>();
	template void ADD_HL_r16<Reg16::SP>();
	template void DEC_r16<Reg16::BC>();
	template void DEC_r16<Reg16::DE>();
	template void DEC_r16<Reg16::HL>();
	template void DEC_r16<Reg16::SP>();
	template void INC_r16<Reg16::BC>();
	template void INC_r16<Reg16::DE>();
	template void INC_r16<Reg16::HL>();
	template void INC_r16<Reg16::SP>();
	template void LD_A_r16<Reg16::BC>();
	template void LD_A_r16<Reg16::DE>();
	template void LD_r16_u16<Reg16::BC>();
	template void LD_r16_u16<Reg16::DE>();
	template void LD_r16_u16<Reg16::HL>();
	template void LD_r16_u16<Reg16::SP>();
	template void LD_r16_A<Reg16::BC>();
	template void LD_r16_A<Reg16::DE>();
	template void LD_r16_A<Reg16::HL>();
	template void LD_r16_A<Reg16::SP>();
	template void POP<Reg16::BC>();
	template void POP<Reg16::DE>();
	template void POP<Reg16::HL>();
	template void POP<Reg16::AF>();
	template void PUSH<Reg16::BC>();
	template void PUSH<Reg16::DE>();
	template void PUSH<Reg16::HL>();
	template void PUSH<Reg16::AF>();
}