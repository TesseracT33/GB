#include <wx/msgdlg.h>

#include "CPU.h"

using namespace Util;

#define  F (F_Z << 7 | F_N << 6 | F_H << 5 | F_C << 4)
#define AF (A << 8 | F)
#define BC (B << 8 | C)
#define DE (D << 8 | E)
#define HL (H << 8 | L)


void CPU::Initialize()
{
	IE = bus->ReadIOPointer(Bus::Addr::IE);
	IF = bus->ReadIOPointer(Bus::Addr::IF);
}


void CPU::Reset(bool execute_boot_rom)
{
	if (execute_boot_rom)
	{
		SP = PC = 0;
		A = B = C = D = E = H = L = 0;
		F_Z = F_N = F_H = F_C = 0;
	}
	else
	{
		switch (System::mode)
		{
		case System::Mode::DMG:
			A = 0x01;
			B = 0x00; C = 0x13;
			D = 0x00; E = 0xD8;
			H = 0x01; L = 0x4D;
			F_Z = F_H = F_C = 1; F_N = 0;
			PC = 0x0100;
			SP = 0xFFFE;
			break;

		case System::Mode::CGB:
			A = 0x11;
			B = 0x00; C = 0x00;
			D = 0x00; E = 0x08;
			H = 0x00; L = 0x7C;
			F_Z = 1; F_N = F_H = F_C = 0;
			// todo: set PC, SP
			break;
		}
	}
	halt_bug = IME = in_halt_mode = in_stop_mode = EI_called = false;
	speed_switch_active = HDMA_transfer_active = false;
}


void CPU::InitiateSpeedSwitch()
{
	speed_switch_active = true;
	speed_switch_m_cycles_remaining = speed_switch_m_cycle_count;
	bus->InitiateSpeedSwitch();
}


void CPU::ExitSpeedSwitch()
{
	speed_switch_active = false;
	bus->ExitSpeedSwitch();
}


bool CPU::GetCond()
{
	// 28, C8, CA, CC:  Z
	// 38, D8, DA, DC:  C
	// 20, C0, C2, C4: NZ
	// 30, D0, D2, D4: NC
	// 18, C3, C9, CD:  1
	switch (opcode)
	{
	case 0x28: case 0xC8: case 0xCA: case 0xCC: return F_Z;
	case 0x38: case 0xD8: case 0xDA: case 0xDC: return F_C;
	case 0x20: case 0xC0: case 0xC2: case 0xC4: return !F_Z;
	case 0x30: case 0xD0: case 0xD2: case 0xD4: return !F_C;
	default: return true;
	}
}


u8 CPU::GetOpMod()
{
	HL_set = false;
	switch (opcode % 8)
	{
	case 0: return B;
	case 1: return C;
	case 2: return D;
	case 3: return E;
	case 4: return H;
	case 5: return L;
	case 6: HL_set = true; return bus->ReadCycle(HL);
	case 7: return A;
	}
}


u8 CPU::GetOpDiv(u8 offset)
{
	HL_set = false;
	switch ((opcode - offset) / 8)
	{
	case 0: return B;
	case 1: return C;
	case 2: return D;
	case 3: return E;
	case 4: return H;
	case 5: return L;
	case 6: HL_set = true; return bus->ReadCycle(HL);
	case 7: return A;
	}
}


u8* CPU::GetOpModPointer()
{
	HL_set = false;
	switch (opcode % 8)
	{
	case 0: return &B;
	case 1: return &C;
	case 2: return &D;
	case 3: return &E;
	case 4: return &H;
	case 5: return &L;
	case 6: HL_set = true; read_HL = bus->ReadCycle(HL); return &read_HL;
	case 7: return &A;
	}
}

u8* CPU::GetOpDivPointer(u8 offset)
{
	HL_set = false;
	switch ((opcode - offset) / 8)
	{
	case 0: return &B;
	case 1: return &C;
	case 2: return &D;
	case 3: return &E;
	case 4: return &H;
	case 5: return &L;
	case 6: HL_set = true; read_HL = bus->ReadCycle(HL); return &read_HL;
	case 7: return &A;
	}
}


// Load the value of the second r8 into the first r8 (the first operand is not (HL))
void CPU::LD_r8_r8() // LD r8, r8    len: 4t if the second r8 != (HL), else 8t
{
	u8* op1 = GetOpDivPointer(0x40);
	u8 op2 = GetOpMod();
	*op1 = op2;
}


// Load the value of r8 into the memory location pointed to by HL
void CPU::LD_HL_r8() // LD (HL), r8    len: 8t
{
	u8 op = GetOpMod();
	bus->WriteCycle(HL, op);
}


// Load u8 into r8 (r8 is not (HL))
void CPU::LD_r8_u8() // LD r8, u8    len: 8t
{
	u8* op1 = GetOpDivPointer(0);
	u8 op2 = Read_u8();
	*op1 = op2;
}


// Load u8 into (HL)
void CPU::LD_HL_u8() // LD (HL), u8    len: 12t
{
	u8 op = Read_u8();
	bus->WriteCycle(HL, op);
}


// Load u16 into r16
void CPU::LD_r16_u16() // LD r16, u16    len: 12t
{
	u16 op = Read_u16();
	switch (opcode)
	{
	case 0x01: B = op >> 8; C = op & 0xFF; return;
	case 0x11: D = op >> 8; E = op & 0xFF; return;
	case 0x21: H = op >> 8; L = op & 0xFF; return;
	case 0x31: SP = op; return;
	}
}


// Load HL into SP
void CPU::LD_SP_HL() // LD SP, HL    len: 8t
{
	SP = HL;
	WaitCycle();
}


// Load A into the byte at address r16
void CPU::LD_r16_A() // LD (r16), A    len: 8t
{
	if (opcode == 0x02)
		bus->WriteCycle(BC, A);
	else // 0x12
		bus->WriteCycle(DE, A);
}


// Load the byte at address r16 into A
void CPU::LD_A_r16() // LD A, (r16)    len: 8t
{
	if (opcode == 0x0A)
		A = bus->ReadCycle(BC);
	else // 0x1A
		A = bus->ReadCycle(DE);
}


// Load A into the byte at address pointed to by u16
void CPU::LD_u16_A() // LD (u16), A    len: 16t
{
	u16 addr = Read_u16();
	bus->WriteCycle(addr, A);
}


// Load the byte at address u16 into A
void CPU::LD_A_u16() // LD A, (u16)    len: 16t
{
	u16 addr = Read_u16();
	A = bus->ReadCycle(addr);
}


// Load SP & 0xFF into the byte at address u16 and SP >> 8 into the byte at address u16 + 1
void CPU::LD_u16_SP() // LD (u16), SP    len: 20t
{
	u16 addr = Read_u16();
	bus->WriteCycle(addr, SP & 0xFF);
	bus->WriteCycle(addr + 1, SP >> 8);
}


// Load SP+s8 into HL
void CPU::LD_HL_SP_s8() // LD HL, SP + s8    len: 12t
{
	s8 offset = Read_s8();
	F_H = (SP & 0xF) + (offset & 0xF) > 0xF; // check if overflow from bit 3
	F_C = (SP & 0xFF) + (offset & 0xFF) > 0xFF; // check if overflow from bit 7
	u16 result = SP + offset;
	H = result >> 8;
	L = result & 0xFF;
	F_Z = F_N = 0;
	WaitCycle();
}


// Load A into the byte at address HL, and thereafter increment HL by 1
void CPU::LD_HLp_A() // LD (HL+), A    len: 8t
{
	bus->WriteCycle(HL, A);
	if (++L == 0)
		++H;
}


// Load A into the byte at address HL, and thereafter decrement HL by 1
void CPU::LD_HLm_A() // LD (HL-), A    len: 8t
{
	bus->WriteCycle(HL, A);
	if (--L == 0xFF)
		--H;
}


// Load the byte at address HL into A, and thereafter increment HL by 1
void CPU::LD_A_HLp() // LD (HL+), A    len: 8t
{
	A = bus->ReadCycle(HL);
	if (++L == 0)
		++H;
}


// Load the byte at address HL into A, and thereafter decrement HL by 1
void CPU::LD_A_HLm() // LD (HL-), A    len: 8t
{
	A = bus->ReadCycle(HL);
	if (--L == 0xFF)
		--H;
}


// Load A into the byte at address FF00+u8
void CPU::LDH_u8_A() // LD (FF00+u8), A    len: 12t
{
	bus->WriteCycle(0xFF00 | Read_u8(), A);
}


// Load the byte at address FF00+u8 into A
void CPU::LDH_A_u8() // LD A, (FF00+u8)    len: 12t
{
	A = bus->ReadCycle(0xFF00 | Read_u8());
}


// Load A into the byte at address FF00+C
void CPU::LDH_C_A() // LD (FF00+C), A    len: 8t
{
	bus->WriteCycle(0xFF00 | C, A);
}


// Load the byte at address FF00+C into A
void CPU::LDH_A_C() // LD A, (FF00+C)    len: 8t
{
	A = bus->ReadCycle(0xFF00 | C);
}


// Add r8 and carry flag to register A
void CPU::ADC_r8() // ADC A, r8    len: 4t if r8 != (HL), otherwise 8t
{
	u8 op = GetOpMod();
	u16 delta = op + F_C;
	F_N = 0;
	F_H = (A & 0xF) + (op & 0xF) + F_C > 0xF; // check if overflow from bit 3
	F_C = (A + delta > 0xFF); // check if overflow from bit 7
	A += delta;
	F_Z = A == 0;
}


// Add u8 and carry flag to register A
void CPU::ADC_u8() // ADC A, u8    len: 8t
{
	u8 op = Read_u8();
	u16 delta = op + F_C;
	F_N = 0;
	F_H = (A & 0xF) + (op & 0xF) + F_C > 0xF; // check if overflow from bit 3
	F_C = (A + delta > 0xFF); // check if overflow from bit 7
	A += delta;
	F_Z = A == 0;
}


// Add r8 to register A
void CPU::ADD_A_r8() // ADD A, r8    len: 4t if r8 != (HL), otherwise 8t
{
	u8 op = GetOpMod();
	F_N = 0;
	F_H = ((A & 0xF) + (op & 0xF) > 0xF); // check if overflow from bit 3
	F_C = (A + op > 0xFF); // check if overflow from bit 7
	A += op;
	F_Z = A == 0;
}


// Add u8 to register A
void CPU::ADD_A_u8() // ADD A, u8    len: 8t
{
	u8 op = Read_u8();
	F_N = 0;
	F_H = ((A & 0xF) + (op & 0xF) > 0xF); // check if overflow from bit 3
	F_C = (A + op > 0xFF); // check if overflow from bit 7
	A += op;
	F_Z = A == 0;
}


// Add r16 to register HL
void CPU::ADD_HL() // ADD HL, r16    len: 8t
{
	u16 op;
	switch (opcode & 0xF0)
	{
	case 0x00: op = BC; break;
	case 0x10: op = DE; break;
	case 0x20: op = HL; break;
	case 0x30: op = SP; break;
	}
	F_N = 0;
	F_H = (HL & 0xFFF) + (op & 0xFFF) > 0xFFF; // check if overflow from bit 11
	F_C = (HL + op > 0xFFFF); // check if overflow from bit 15
	u16 result = HL + op;
	H = result >> 8;
	L = result & 0xFF;
	WaitCycle();
}


// Add signed value to SP
void CPU::ADD_SP() // ADD SP, s8   len: 16t
{
	s8 offset = Read_s8();
	F_H = (SP & 0xF) + (offset & 0xF) > 0xF; // check if overflow from bit 3
	F_C = (SP & 0xFF) + (offset & 0xFF) > 0xFF; // check if overflow from bit 7
	SP += offset;
	F_Z = F_N = 0;
	WaitCycle(2);
}


// Store bitwise AND between r8 and A in A
void CPU::AND_r8() // AND A, r8    len: 4t if r8 != (HL), otherwise 8t
{
	u8 op = GetOpMod();
	A &= op;
	F_Z = A == 0;
	F_N = 0;
	F_H = 1;
	F_C = 0;
}


// Store bitwise AND between u8 and A in A
void CPU::AND_u8() // AND A, u8    len: 8t
{
	u8 op = Read_u8();
	A &= op;
	F_Z = A == 0;
	F_N = 0;
	F_H = 1;
	F_C = 0;
}


// Perform subtraction of r8 from A, but don't store the result
void CPU::CP_r8() // CP A, r8    len: 4t if r8 != (HL), otherwise 8t
{
	u8 op = GetOpMod();
	F_Z = A - op == 0;
	F_N = 1;
	F_H = (op & 0xF) > (A & 0xF); // check if borrow from bit 4
	F_C = op > A; // check if borrow
}


// Perform subtraction of u8 from A, but don't store the result
void CPU::CP_u8() // CP A, u8    len: 8t
{
	u8 op = Read_u8();
	F_Z = A - op == 0;
	F_N = 1;
	F_H = (op & 0xF) > (A & 0xF); // check if borrow from bit 4
	F_C = op > A; // check if borrow
}


// Decrement 8-bit register by 1
void CPU::DEC_r8() // DEC r8    len: 4t if r8 != (HL), otherwise 12t
{
	u8* op = GetOpDivPointer(0);
	F_N = 1;
	F_H = (*op & 0xF) == 0; // check if borrow from bit 4
	(*op)--;
	F_Z = *op == 0;

	if (HL_set)
		bus->WriteCycle(HL, *op);
}


// Decrement 16-bit register by 1
void CPU::DEC_r16() // DEC r16    len: 8t
{
	switch (opcode)
	{
	case 0x0B: if (--C == 0xFF) --B; break; // DEC BC
	case 0x1B: if (--E == 0xFF) --D; break; // DEC DE
	case 0x2B: if (--L == 0xFF) --H; break; // DEC HL
	case 0x3B: SP--; break; // DEC SP
	}
	WaitCycle();
}


// Increment 8-bit register by 1
void CPU::INC_r8() // INC r8    len: 4t if r8 != (HL), otherwise 12t
{
	u8* op = GetOpDivPointer(0);
	F_N = 0;
	F_H = (*op & 0xF) == 0xF; // check if overflow from bit 3
	(*op)++;
	F_Z = *op == 0;

	if (HL_set)
		bus->WriteCycle(HL, *op);
}


// Increment 16-bit register by 1
void CPU::INC_r16() // INC r16    len: 8t
{
	switch (opcode)
	{
	case 0x03: if (++C == 0) ++B; break; // INC BC
	case 0x13: if (++E == 0) ++D; break; // INC DE
	case 0x23: if (++L == 0) ++H; break; // INC HL
	case 0x33: SP++; break; // INC SP
	}
	WaitCycle();
}


// Perform bitwise OR between A and r8, and store the result in A
void CPU::OR_r8() // OR A, r8    len: 4t if r8 != (HL), otherwise 8t
{
	u8 op = GetOpMod();
	A |= op;
	F_Z = A == 0;
	F_N = F_H = F_C = 0;
}


// Perform bitwise OR between A and u8, and store the result in A
void CPU::OR_u8() // OR A, u8    len: 8t
{
	u8 op = Read_u8();
	A |= op;
	F_Z = A == 0;
	F_N = F_H = F_C = 0;
}


// Subtract r8 and carry flag from A
void CPU::SBC_r8() // SBC A, r8    len: 4t if r8 != (HL), otherwise 8t
{
	u8 op = GetOpMod();
	u16 delta = op + F_C;
	F_N = 1;
	F_H = (op & 0xF) + F_C > (A & 0xF); // check if borrow from bit 4
	F_C = delta > A; // check if borrow
	A -= delta;
	F_Z = A == 0;
}


// Subtract u8 and carry flag from A
void CPU::SBC_u8() // SBC A, u8    len: 8t
{
	u8 op = Read_u8();
	u16 delta = op + F_C;
	F_N = 1;
	F_H = (op & 0xF) + F_C > (A & 0xF); // check if borrow from bit 4
	F_C = delta > A; // check if borrow
	A -= delta;
	F_Z = A == 0;
}


// Subtract r8 from A
void CPU::SUB_r8() // SUB A, r8    len: 4t if r8 != (HL), otherwise 8t
{
	u8 op = GetOpMod();
	F_N = 1;
	F_H = (op & 0xF) > (A & 0xF); // check if borrow from bit 4
	F_C = op > A; // check if borrow
	A -= op;
	F_Z = A == 0;
}


// Subtract u8 from A
void CPU::SUB_u8() // SUB A, u8    len: 8t
{
	u8 op = Read_u8();
	F_N = 1;
	F_H = (op & 0xF) > (A & 0xF); // check if borrow from bit 4
	F_C = op > A; // check if borrow
	A -= op;
	F_Z = A == 0;
}


// Perform bitwise XOR between A and r8, and store the result in A
void CPU::XOR_r8() // XOR A, r8    len: 4t if r8 != (HL), otherwise 8t
{
	u8 op = GetOpMod();
	A ^= op;
	F_Z = A == 0;
	F_N = F_H = F_C = 0;
}


// Perform bitwise XOR between A and u8, and store the result in A
void CPU::XOR_u8() // XOR A, u8    len: 8t
{
	u8 op = Read_u8();
	A ^= op;
	F_Z = A == 0;
	F_N = F_H = F_C = 0;
}


// Rotate register r8 left through carry.
void CPU::RL() // RL r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
{
	u8* reg = GetOpModPointer();
	bool bit7 = *reg >> 7;
	*reg = (*reg & 0x7F) << 1 | F_C;
	F_C = bit7;
	F_Z = *reg == 0;
	F_N = F_H = 0;
	if (HL_set)
		bus->WriteCycle(HL, *reg);
}


// Rotate A left through carry (and clear the zero flag)
void CPU::RLA() // RLA    len: 4t
{
	bool bit7 = A >> 7;
	A = (A & 0x7F) << 1 | F_C;
	F_C = bit7;
	F_Z = F_N = F_H = 0;
}


// Rotate register r8 left
void CPU::RLC() // RLC r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
{
	u8* reg = GetOpModPointer();
	F_C = *reg >> 7;
	*reg = _rotl8(*reg, 1); // rotate A left one bit.
	F_Z = *reg == 0;
	F_N = F_H = 0;
	if (HL_set)
		bus->WriteCycle(HL, *reg);
}


// Rotate A left (and clear the zero flag)
void CPU::RLCA() // RLCA    len: 4t
{
	F_C = A >> 7;
	A = _rotl8(A, 1); // rotate A left one bit.
	F_Z = F_N = F_H = 0;
}


// Rotate register r8 right through carry.
void CPU::RR() // RR r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
{
	u8* reg = GetOpModPointer();
	bool bit0 = *reg & 1;
	*reg = *reg >> 1 | F_C << 7;
	F_C = bit0;
	F_Z = *reg == 0;
	F_N = F_H = 0;
	if (HL_set)
		bus->WriteCycle(HL, *reg);
}


// Rotate A right through carry (and clear the zero flag)
void CPU::RRA() // RRA    len: 4t
{
	bool bit0 = A & 1;
	A = A >> 1 | F_C << 7;
	F_C = bit0;
	F_Z = F_N = F_H = 0;
}


// Rotate register r8 right
void CPU::RRC() // RRC r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
{
	u8* reg = GetOpModPointer();
	F_C = *reg & 1;
	*reg = _rotr8(*reg, 1); // rotate A right one bit.
	F_Z = *reg == 0;
	F_N = F_H = 0;
	if (HL_set)
		bus->WriteCycle(HL, *reg);
}


// Rotate A right (and clear the zero flag)
void CPU::RRCA() // RRCA    len: 4t
{
	F_C = A & 1;
	A = _rotr8(A, 1); // rotate A right one bit.
	F_Z = F_N = F_H = 0;
}


// Shift arithmetic register r8 left
void CPU::SLA() // SLA r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
{
	u8* reg = GetOpModPointer();
	F_C = *reg >> 7;
	*reg = *reg << 1 & 0xFF;
	F_Z = *reg == 0;
	F_N = F_H = 0;
	if (HL_set)
		bus->WriteCycle(HL, *reg);
}


// Shift arithmetic register r8 right
void CPU::SRA() // SRA r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
{
	u8* reg = GetOpModPointer();
	F_C = *reg & 1;
	*reg = *reg >> 1 | *reg & 0x80;
	F_Z = *reg == 0;
	F_N = F_H = 0;
	if (HL_set)
		bus->WriteCycle(HL, *reg);
}


// Shift logic register r8 right
void CPU::SRL() // SRL r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
{
	u8* reg = GetOpModPointer();
	F_C = *reg & 1;
	*reg >>= 1;
	F_Z = *reg == 0;
	F_N = F_H = 0;
	if (HL_set)
		bus->WriteCycle(HL, *reg);
}


// Set the zero flag according to whether a bit with position pos in register r8 is set or not
void CPU::BIT() // BIT pos, r8    len: 8t if r8 != (HL), otherwise 12t (prefixed instruction)
{
	u8 pos = (opcode - 0x40) / 8;
	u8 reg = GetOpMod();
	F_Z = CheckBit(reg, pos) == 0;
	F_N = 0;
	F_H = 1;
}


// Clear bit number pos in register r8
void CPU::RES() // RES pos, r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
{
	u8 pos = (opcode - 0x80) / 8;
	u8* reg = GetOpModPointer();
	ClearBit(reg, pos);
	if (HL_set)
		bus->WriteCycle(HL, *reg);
}


// Set bit number pos in register r8
void CPU::SET() // SET pos, r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
{
	u8 pos = (opcode - 0xC0) / 8;
	u8* reg = GetOpModPointer();
	SetBit(reg, pos);
	if (HL_set)
		bus->WriteCycle(HL, *reg);
}


// Swap upper 4 bits in register r8 and the lower 4 ones. 
void CPU::SWAP() // SWAP r8    len: 8t if r8 != (HL), otherwise 16t (prefixed instruction)
{
	u8* reg = GetOpModPointer();
	*reg = _rotl8(*reg, 4);
	F_Z = *reg == 0;
	F_N = F_H = F_C = 0;
	if (HL_set)
		bus->WriteCycle(HL, *reg);
}


// Prefixed instructions
void CPU::CB() // CB <instr>
{
	opcode = Read_u8();

	#ifdef DEBUG
		char buf[200]{};
		sprintf(buf, "#%i\tPC: %04X\tOP: ~%02X\tSP: %04X\tAF: %04X\tBC: %04X\tDE: %04X\tHL: %04X\tIME: %i\tIE: %02X\tIF: %02X\tLCDC: %02X\tSTAT: %02X\tLY: %02X\tROM: %04X",
			instruction_counter++, (int)PC, (int)opcode, (int)SP, (int)AF, (int)BC, (int)DE, (int)HL, (int)IME,
			(int)bus->Read(Bus::Addr::IE), (int)bus->Read(Bus::Addr::IF), (int)bus->Read(Bus::Addr::LCDC), (int)bus->Read(Bus::Addr::STAT), (int)bus->Read(Bus::Addr::LY), (int)bus->GetCurrentRomBank());
		ofs << buf << std::endl;
	#endif

	switch (opcode / 8)
	{
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
	default: SET(); break;
	}
}


// Jump to subroutine; set PC to u16 if condition holds
void CPU::CALL() // CALL cc, u16    len: 24t if branch is successful, else 12t
{
	u16 newPC = Read_u16();
	if (GetCond())
	{
		WaitCycle();
		PushPC();
		PC = newPC;
	}
}


// Return from subroutine; set PC by popping the stack if condition holds
void CPU::RET() // RET cc    len: 16t if opcode == C9 (RET). Otherwise: 20t if branch is successful, else 8t
{
	if (opcode == 0xC9)
	{
		PopPC();
		WaitCycle();
	}
	else
	{
		WaitCycle();
		if (GetCond())
		{
			PopPC();
			WaitCycle();
		}
	}
}


// Return from subroutine and enable interrupts
void CPU::RETI() // RETI    len: 16t
{
	PopPC();
	WaitCycle();
	IME = 1;
}


// Relative jump; add a signed 8-bit integer to the address of the instruction following the JR if the condition holds
void CPU::JR() // JR cc, s8    len: 12t if the branch is successful, else 8t
{
	s8 offset = Read_s8();
	if (GetCond())
	{
		PC += offset; // todo: someone said to "sign extend by casting u8 to s8 to u16 before adding to PC"
		WaitCycle();
	}
}


// Absolute jump; jump to the address specified by u16 if the condition holds
void CPU::JP_u16() // JP cc, u16    len: 16t if the branch is successful, else 12t
{
	u16 word = Read_u16();
	if (GetCond())
	{
		PC = word;
		WaitCycle();
	}
}


// Absolute jump; jump to the address specified by HL
void CPU::JP_HL() // JP HL    len: 4t
{
	PC = HL;
}


// Jump to subroutine at specific addresses
void CPU::RST() // RST vec    len: 16t
{
	WaitCycle();
	PushPC();
	PC = 8 * ((opcode - 0xC0) / 8);
}


// Toggle the carry flag
void CPU::CCF() // CCF    len: 4t
{
	F_C = !F_C;
	F_N = F_H = 0;
}


// Invert all bits in A
void CPU::CPL() // CPL    len: 4t
{
	A = ~A;
	F_N = F_H = 1;
}


// 
void CPU::DAA() // DAA    len: 4t
{
	// https://forums.nesdev.com/viewtopic.php?t=15944
	if (F_N)
	{
		if (F_C) A -= 0x60;
		if (F_H) A -= 0x6;
	}
	else
	{
		if (F_C || A > 0x99)
		{
			A += 0x60;
			F_C = 1;
		}

		if (F_H || (A & 0xF) > 0x9)
			A += 0x6;
	}

	F_Z = A == 0;
	F_H = 0;
}


// Disable interrupts
void CPU::DI() // DI    len: 4t
{
	IME = 0;
}


// Enable interrupts (delayed by one instruction)
void CPU::EI() // EI    len: 4t
{
	EI_called = true;
	instr_executed_after_EI_called = false;
}


// (Attempt to) enter HALT mode
void CPU::HALT() // HALT    len: 4t
{
	if (IME || (*IF & *IE & 0x1F) == 0)
	{
		in_halt_mode = true;
	}
	else
	{ // HALT is not entered, causing a bug where the CPU fails to increase PC when executing the next instruction
		halt_bug = true;
	}
}


// Illegal opcode
void CPU::Illegal()
{
	wxMessageBox(wxString::Format("Illegal opcode 0x%02X encountered.", opcode));
}


// No operation
void CPU::NOP() // NOP    len: 4t
{

}


// Pop a word off of the stack and put into a 16-bit register
void CPU::POP() // POP r16    len: 12t
{
	switch (opcode)
	{
	case 0xC1: C = bus->ReadCycle(SP++); B = bus->ReadCycle(SP++); break;
	case 0xD1: E = bus->ReadCycle(SP++); D = bus->ReadCycle(SP++); break;
	case 0xE1: L = bus->ReadCycle(SP++); H = bus->ReadCycle(SP++); break;
	case 0xF1:
	{
		u8 flags = bus->ReadCycle(SP++);
		F_Z = flags & 0x80;
		F_N = flags & 0x40;
		F_H = flags & 0x20;
		F_C = flags & 0x10;
		A = bus->ReadCycle(SP++);
		break;
	}
	}
}


// Push a 16-bit register onto the stack
void CPU::PUSH() // PUSH r16    len: 16t
{
	WaitCycle();
	switch (opcode)
	{
	case 0xC5: bus->WriteCycle(--SP, B); bus->WriteCycle(--SP, C); break;
	case 0xD5: bus->WriteCycle(--SP, D); bus->WriteCycle(--SP, E); break;
	case 0xE5: bus->WriteCycle(--SP, H); bus->WriteCycle(--SP, L); break;
	case 0xF5: bus->WriteCycle(--SP, A); bus->WriteCycle(--SP, F); break;
	}
}


// Set the carry flag
void CPU::SCF() // SCF    len: 4t
{
	F_C = 1;
	F_N = F_H = 0;
}


// Enter stop mode on DMG, switch between single and double speed modes on CGB
void CPU::STOP() // STOP    len: 4t
{
	// This instruction is very glitchy, see https://pbs.twimg.com/media/E5jlgW9XIAEKj0t.png:large
	// On DMG, enter low power mode. On the CGB, switch between normal and double speed (if KEY1.0 has been set).
	
	// TODO: Test if a button is being held and selected in JOYP
	
	u8 KEY1 = bus->Read(Bus::Addr::KEY1);
	if ((KEY1 & 0x01) && System::mode == System::Mode::CGB) // If a speed switch was requested
	{
		if (*IE & *IF & 0x1F) // If an interrupt is pending
		{
			if (IME)
			{
				// Glitch non-deterministically.
				// For now, emulated as successfully performing a speed switch.
				bus->ResetDIV();
				InitiateSpeedSwitch();
			}
			else
			{
				// Successfully perform a speed switch.
				bus->ResetDIV();
				InitiateSpeedSwitch();
			}
		}
		else
		{
			// Successfully perform a speed switch with STOP as a 2-byte instruction. Also, enter HALT.
			PC++;
			HALT();
			bus->ResetDIV();
			InitiateSpeedSwitch();
		}
	}
	else
	{
		// DIV is reset and STOP mode is reset. STOP is a 2-byte opcode if an interrupt is not pending
		if ((*IE & *IF & 0x1F) == 0)
			PC++;
		bus->ResetDIV();
		in_stop_mode = true;
	}
}


void CPU::Run()
{
	bus->m_cycle_counter = 0;

	// run the cpu for a whole frame
	while (bus->m_cycle_counter < System::m_cycles_per_frame)
	{
		if (HDMA_transfer_active)
		{
			WaitCycle();
			continue;
		}
		if (speed_switch_active)
		{
			WaitCycle();
			if (--speed_switch_m_cycles_remaining == 0)
				ExitSpeedSwitch();
			continue;
		}
		if (in_halt_mode)
		{
			CheckInterrupts();
			continue;
		}

		CheckInterrupts();

		opcode = bus->ReadCycle(PC);

		#ifdef DEBUG
			char buf[200]{};
			sprintf(buf, "#%i\tPC: %04X\tOP: %02X\tSP: %04X\tAF: %04X\tBC: %04X\tDE: %04X\tHL: %04X\tIME: %i\tIE: %02X\tIF: %02X\tLCDC: %02X\tSTAT: %02X\tLY: %02X\tROM: %04X",
				instruction_counter++, (int)PC, (int)opcode, (int)SP, (int)AF, (int)BC, (int)DE, (int)HL, (int)IME,
				(int)bus->Read(Bus::Addr::IE), (int)bus->Read(Bus::Addr::IF), (int)bus->Read(Bus::Addr::LCDC), (int)bus->Read(Bus::Addr::STAT), (int)bus->Read(Bus::Addr::LY), (int)bus->GetCurrentRomBank());
			ofs << buf << std::endl;
		#endif

		// If the previous instruction was HALT, there is a hardware bug in which PC is not incremented after the current instruction
		if (!halt_bug)
			PC++;
		else
			halt_bug = false;

		instr_t instr = instr_table[opcode];
		std::invoke(instr, this);

		// If the previous instruction was EI, the IME flag is set only after the instruction after the EI has been executed
		if (EI_called)
		{
			if (instr_executed_after_EI_called)
			{
				IME = 1;
				EI_called = false;
			}
			else
				instr_executed_after_EI_called = true;
		}
	}
}


void CPU::CheckInterrupts()
{
	// This function does not only dispatch interrupts if necessary,
	// it also checks if HALT mode should be exited if the CPU is in it

	/* Behaviour for when  (*IF & *IE & 0x1F) != 0  :
	*  HALT,  IME : jump to interrupt vector, clear IF flag
	* !HALT,  IME : same as above
	*  HALT, !IME : don't jump to interrupt vector, don't clear IF flag
	* !HALT, !IME : do nothing
	* In addition: clear HALT flag in each case
	*/
	if (IME)
	{
		// fetch bits 0-4 of the IF IE registers
		u8 AND = *IF & *IE & 0x1F;
		if (AND) // if any enabled interrupt is requested
		{
			WaitCycle(2);
			PushPC();

			for (int i = 0; i <= 4; i++)
			{
				if (CheckBit(AND, i) == 1)
				{
					PC = interrupt_vector[i];
					WaitCycle();
					ClearBit(IF, i); // todo: when does this happen exactly?
				}
			}

			if (in_halt_mode)
				WaitCycle();
			IME = in_halt_mode = false;
			return;
		}
	}
	if (in_halt_mode)
	{ // check if HALT mode should be exited, which is when enabled interrupt is requested
		if (*IF & *IE & 0x1F)
		{
			in_halt_mode = false;
		}
		WaitCycle();
	}
}


void CPU::PushPC()
{
	bus->WriteCycle(--SP, PC >> 8);
	bus->WriteCycle(--SP, PC & 0xFF);
}


void CPU::PopPC()
{
	u8 low = bus->ReadCycle(SP++);
	u8 high = bus->ReadCycle(SP++);
	PC = high << 8 | low;
}


void CPU::RequestInterrupt(CPU::Interrupt interrupt)
{
	SetBit(IF, interrupt);
}


void CPU::State(Serialization::BaseFunctor& functor)
{
	functor.fun(&in_halt_mode, sizeof(bool));
	functor.fun(&HDMA_transfer_active, sizeof(bool));
	functor.fun(&speed_switch_active, sizeof(bool));
	functor.fun(&in_stop_mode, sizeof(bool));

	functor.fun(&A, sizeof(u8));
	functor.fun(&B, sizeof(u8));
	functor.fun(&C, sizeof(u8));
	functor.fun(&D, sizeof(u8));
	functor.fun(&E, sizeof(u8));
	functor.fun(&H, sizeof(u8));
	functor.fun(&L, sizeof(u8));

	functor.fun(&F_Z, sizeof(bool));
	functor.fun(&F_N, sizeof(bool));
	functor.fun(&F_H, sizeof(bool));
	functor.fun(&F_C, sizeof(bool));

	functor.fun(&SP, sizeof(u16));
	functor.fun(&PC, sizeof(u16));

	functor.fun(&EI_called, sizeof(bool));
	functor.fun(&halt_bug, sizeof(bool));
	functor.fun(&IME, sizeof(bool));
	functor.fun(&instr_executed_after_EI_called, sizeof(bool));

	functor.fun(&speed_switch_m_cycles_remaining, sizeof(unsigned));
}