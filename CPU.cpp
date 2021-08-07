#include <wx/msgdlg.h>

#include "CPU.h"

using namespace Util;

#define  F (F_Z << 7 | F_N << 6 | F_H << 5 | F_C << 4 | 0xF)
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
	action_taken = HALT_bug = IME = HALT = STOP = EI_called = false;
	speed_switch_active = HDMA_transfer_active = interrupt_dispatched_on_last_update = false;
	m_cycles_instr = 0;
	m_cycles_until_next_instr = 1; // 1 since it gets decremented by one before being checked to be 0 in Update()
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


void CPU::ExecuteInstruction(u8 opcode)
{
	switch (opcode)
	{
		/* d8  : immediate unsigned 8-bit data
		* d16  : immediate unsigned 16-bit data
		* a8   : 8-bit address (unsigned)
		* a16  : 16-bit address (unsigned)
		* r8   : immediate signed 8-bit data
		* (a16): the byte at address a16 in RAM
		*/
	case 0x00: // NOP
		break;

	case 0x01: // LD BC, d16
		C = Read_u8();
		B = Read_u8();
		break;

	case 0x02: // LD (BC), A
		bus->Write(BC, A);
		break;

	case 0x03: // INC BC
		if (++C == 0)
			B++;
		break;

	case 0x04: // INC B
		INC(B);
		break;

	case 0x05: // DEC B
		DEC(B);
		break;

	case 0x06: // LD B, d8
		B = Read_u8();
		break;

	case 0x07: // RLCA
		RLC(&A);
		F_Z = 0;
		break;

	case 0x08: // LD (a16), SP
	{
		u16 addr = Read_u16();
		bus->Write(addr, SP & 0xFF);
		bus->Write(addr + 1, SP >> 8);
		break;
	}

	case 0x09: // ADD HL, BC
		ADD16(BC);
		break;

	case 0x0A: // LD A, (BC)
		A = bus->Read(BC);
		break;

	case 0x0B: // DEC BC
		if (--C == 0xFF)
			B--;
		break;

	case 0x0C: // INC C
		INC(C);
		break;

	case 0x0D: // DEC C
		DEC(C);
		break;

	case 0x0E: // LD C, d8
		C = Read_u8();
		break;

	case 0x0F: // RRCA
		RRC(&A);
		F_Z = 0;
		break;

	case 0x10: // STOP
		EnterSTOP();
		break;

	case 0x11: // LD DE, d16
		E = Read_u8();
		D = Read_u8();
		break;

	case 0x12: // LD (DE), A
		bus->Write(DE, A);
		break;

	case 0x13: // INC DE
		if (++E == 0)
			D++;
		break;

	case 0x14: // INC D
		INC(D);
		break;

	case 0x15: // DEC D
		DEC(D);
		break;

	case 0x16: // LD D, d8
		D = Read_u8();
		break;

	case 0x17: // RLA
		RL(&A);
		F_Z = 0;
		break;

	case 0x18: // JR r8
		JR(true);
		break;

	case 0x19: // ADD HL, DE
		ADD16(DE);
		break;

	case 0x1A: // LD A, (DE)
		A = bus->Read(DE);
		break;

	case 0x1B: // DEC DE 
		if (--E == 0xFF)
			D--;
		break;

	case 0x1C: // INC E
		INC(E);
		break;

	case 0x1D: // DEC E
		DEC(E);
		break;

	case 0x1E: // LD E, d8
		E = Read_u8();
		break;

	case 0x1F: // RRA
		RR(&A);
		F_Z = 0;
		break;

	case 0x20: // JR NZ, r8
		JR(!F_Z);
		break;

	case 0x21: // LD HL, d16
		L = Read_u8();
		H = Read_u8();
		break;

	case 0x22: // LD (HL+), A
		bus->Write(HL, A);
		if (++L == 0)
			H++;
		break;

	case 0x23: // INC HL
		if (++L == 0)
			H++;
		break;

	case 0x24: // INC H
		INC(H);
		break;

	case 0x25: // DEC H
		DEC(H);
		break;

	case 0x26: // LD H, d8
		H = Read_u8();
		break;

	case 0x27: // DAA
		DAA();
		break;

	case 0x28: // JR Z, r8
		JR(F_Z);
		break;

	case 0x29: // ADD HL, HL
		ADD16(HL);
		break;

	case 0x2A: // LD A, (HL+)
		A = bus->Read(HL);
		if (++L == 0)
			H++;
		break;

	case 0x2B: // DEC HL
		if (--L == 0xFF)
			H--;
		break;

	case 0x2C: // INC L
		INC(L);
		break;

	case 0x2D: // DEC L
		DEC(L);
		break;

	case 0x2E: // LD L, d8
		L = Read_u8();
		break;

	case 0x2F: // CPL
		A = ~A;
		F_N = F_H = 1;
		break;

	case 0x30: // JR NC, r8
		JR(!F_C);
		break;

	case 0x31: // LD SP, d16
		SP = Read_u16();
		break;

	case 0x32: // LD (HL-), A
		bus->Write(HL, A);
		if (--L == 0xFF)
			H--;
		break;

	case 0x33: // INC SP
		SP++;
		break;

	case 0x34: // INC (HL)
	{
		u8 op = bus->Read(HL);
		F_N = 0;
		F_H = (op & 0xF) == 0xF; // check if overflow from bit 3
		op++;
		F_Z = op == 0;
		bus->Write(HL, op);
		break;
	}

	case 0x35: // DEC (HL)
	{
		u8 op = bus->Read(HL);
		F_N = 1;
		F_H = (op & 0xF) == 0; // check if borrow from bit 4
		op--;
		F_Z = op == 0;
		bus->Write(HL, op);
		break;
	}

	case 0x36: // LD (HL), d8
		bus->Write(HL, Read_u8());
		break;

	case 0x37: // SCF
		F_C = 1;
		F_N = F_H = 0;
		break;

	case 0x38: // JR C, r8
		JR(F_C);
		break;

	case 0x39: // ADD HL, SP
		ADD16(SP);
		break;

	case 0x3A: // LD A, (HL-)
		A = bus->Read(HL);
		if (--L == 0xFF)
			H--;
		break;

	case 0x3B: // DEC SP
		SP--;
		break;

	case 0x3C: // INC A
		INC(A);
		break;

	case 0x3D: // DEC A
		DEC(A);
		break;

	case 0x3E: // LD A, d8
		A = Read_u8();
		break;

	case 0x3F: // CCF
		F_C = !F_C;
		F_N = F_H = 0;
		break;

	case 0x40: // LD  B, B
		break;

	case 0x41: // LD  B, C
		B = C;
		break;

	case 0x42: // LD  B, D
		B = D;
		break;

	case 0x43: // LD  B, E
		B = E;
		break;

	case 0x44: // LD  B, H
		B = H;
		break;

	case 0x45: // LD  B, L
		B = L;
		break;

	case 0x46: // LD  B (HL)
		B = bus->Read(HL);
		break;

	case 0x47: // LD  B, A
		B = A;
		break;

	case 0x48: // LD  C, B
		C = B;
		break;

	case 0x49: // LD  C, C
		break;

	case 0x4A: // LD  C, D
		C = D;
		break;

	case 0x4B: // LD  C, E
		C = E;
		break;

	case 0x4C: // LD  C, H
		C = H;
		break;

	case 0x4D: // LD  C, L
		C = L;
		break;

	case 0x4E: // LD  C, (HL)
		C = bus->Read(HL);
		break;

	case 0x4F: // LD  C, A
		C = A;
		break;

	case 0x50: // LD  D, B
		D = B;
		break;

	case 0x51: // LD  D, C
		D = C;
		break;

	case 0x52: // LD  D, D
		break;

	case 0x53: // LD  D, E
		D = E;
		break;

	case 0x54: // LD  D, H
		D = H;
		break;

	case 0x55: // LD  D, L
		D = L;
		break;

	case 0x56: // LD  D (HL)
		D = bus->Read(HL);
		break;

	case 0x57: // LD  D, A
		D = A;
		break;

	case 0x58: // LD  E, B
		E = B;
		break;

	case 0x59: // LD  E, C
		E = C;
		break;

	case 0x5A: // LD  E, D
		E = D;
		break;

	case 0x5B: // LD  E, E
		break;

	case 0x5C: // LD  E, H
		E = H;
		break;

	case 0x5D: // LD  E, L
		E = L;
		break;

	case 0x5E: // LD  E, (HL)
		E = bus->Read(HL);
		break;

	case 0x5F: // LD  E, A
		E = A;
		break;

	case 0x60: // LD  H, B
		H = B;
		break;

	case 0x61: // LD  H, C
		H = C;
		break;

	case 0x62: // LD  H, D
		H = D;
		break;

	case 0x63: // LD  H, E
		H = E;
		break;

	case 0x64: // LD  H, H
		break;

	case 0x65: // LD  H, L
		H = L;
		break;

	case 0x66: // LD  H (HL)
		H = bus->Read(HL);
		break;

	case 0x67: // LD  H, A
		H = A;
		break;

	case 0x68: // LD  L, B
		L = B;
		break;

	case 0x69: // LD  L, C
		L = C;
		break;

	case 0x6A: // LD  L, D
		L = D;
		break;

	case 0x6B: // LD  L, E
		L = E;
		break;

	case 0x6C: // LD  L, H
		L = H;
		break;

	case 0x6D: // LD  L, L
		break;

	case 0x6E: // LD  L, (HL)
		L = bus->Read(HL);
		break;

	case 0x6F: // LD  L, A
		L = A;
		break;

	case 0x70: // LD  (HL), B
		bus->Write(HL, B);
		break;

	case 0x71: // LD  (HL), C
		bus->Write(HL, C);
		break;

	case 0x72: // LD  (HL), D
		bus->Write(HL, D);
		break;

	case 0x73: // LD  (HL), E
		bus->Write(HL, E);
		break;

	case 0x74: // LD  (HL), H
		bus->Write(HL, H);
		break;

	case 0x75: // LD  (HL), L
		bus->Write(HL, L);
		break;

	case 0x76: // HALT
		EnterHALT();
		break;

	case 0x77: // LD  (HL), A
		bus->Write(HL, A);
		break;

	case 0x78: // LD  A, B
		A = B;
		break;

	case 0x79: // LD  A, C
		A = C;
		break;

	case 0x7A: // LD  A, D
		A = D;
		break;

	case 0x7B: // LD  A, E
		A = E;
		break;

	case 0x7C: // LD  A, H
		A = H;
		break;

	case 0x7D: // LD  A, L
		A = L;
		break;

	case 0x7E: // LD  A, (HL)
		A = bus->Read(HL);
		break;

	case 0x7F: // LD  A, A
		break;

	case 0x80: // ADD A, B
		ADD8(B);
		break;

	case 0x81: // ADD A, C
		ADD8(C);
		break;

	case 0x82: // ADD A, D
		ADD8(D);
		break;

	case 0x83: // ADD A, E
		ADD8(E);
		break;

	case 0x84: // ADD A, H
		ADD8(H);
		break;

	case 0x85: // ADD A, L
		ADD8(L);
		break;

	case 0x86: // ADD A, (HL)
		ADD8(bus->Read(HL));
		break;

	case 0x87: // ADD A, A
		ADD8(A);
		break;

	case 0x88: // ADC A, B
		ADC(B);
		break;

	case 0x89: // ADC A, C
		ADC(C);
		break;

	case 0x8A: // ADC A, D
		ADC(D);
		break;

	case 0x8B: // ADC A, E
		ADC(E);
		break;

	case 0x8C: // ADC A, H
		ADC(H);
		break;

	case 0x8D: // ADC A, L
		ADC(L);
		break;

	case 0x8E: // ADC A, (HL)
		ADC(bus->Read(HL));
		break;

	case 0x8F: // ADC A, A
		ADC(A);
		break;

	case 0x90: // SUB A, B
		SUB(B);
		break;

	case 0x91: // SUB A, C
		SUB(C);
		break;

	case 0x92: // SUB A, D
		SUB(D);
		break;

	case 0x93: // SUB A, E
		SUB(E);
		break;

	case 0x94: // SUB A, H
		SUB(H);
		break;

	case 0x95: // SUB A, L
		SUB(L);
		break;

	case 0x96: // SUB A, (HL)
		SUB(bus->Read(HL));
		break;

	case 0x97: // SUB A, A
		SUB(A);
		break;

	case 0x98: // SBC A, B
		SBC(B);
		break;

	case 0x99: // SBC A, C
		SBC(C);
		break;

	case 0x9A: // SBC A, D
		SBC(D);
		break;

	case 0x9B: // SBC A, E
		SBC(E);
		break;

	case 0x9C: // SBC A, H
		SBC(H);
		break;

	case 0x9D: // SBC A, L
		SBC(L);
		break;

	case 0x9E: // SBC A, (HL)
		SBC(bus->Read(HL));
		break;

	case 0x9F: // SBC A, A
		SBC(A);
		break;

	case 0xA0: // AND A, B
		AND(B);
		break;

	case 0xA1: // AND A, C
		AND(C);
		break;

	case 0xA2: // AND A, D
		AND(D);
		break;

	case 0xA3: // AND A, E
		AND(E);
		break;

	case 0xA4: // AND A, H
		AND(H);
		break;

	case 0xA5: // AND A, L
		AND(L);
		break;

	case 0xA6: // AND A, (HL)
		AND(bus->Read(HL));
		break;

	case 0xA7: // AND A, A
		AND(A);
		break;

	case 0xA8: // XOR A, B
		XOR(B);
		break;

	case 0xA9: // XOR A, C
		XOR(C);
		break;

	case 0xAA: // XOR A, D
		XOR(D);
		break;

	case 0xAB: // XOR A, E
		XOR(E);
		break;

	case 0xAC: // XOR A, H
		XOR(H);
		break;

	case 0xAD: // XOR A, L
		XOR(L);
		break;

	case 0xAE: // XOR A, (HL)
		XOR(bus->Read(HL));
		break;

	case 0xAF: // XOR A, A
		XOR(A);
		break;

	case 0xB0: // OR A, B
		OR(B);
		break;

	case 0xB1: // OR A, C
		OR(C);
		break;

	case 0xB2: // OR A, D
		OR(D);
		break;

	case 0xB3: // OR A, E
		OR(E);
		break;

	case 0xB4: // OR A, H
		OR(H);
		break;

	case 0xB5: // OR A, L
		OR(L);
		break;

	case 0xB6: // OR A, (HL)
		OR(bus->Read(HL));
		break;

	case 0xB7: // OR A, A
		OR(A);
		break;

	case 0xB8: // CP A, B
		CP(B);
		break;

	case 0xB9: // CP A, C
		CP(C);
		break;

	case 0xBA: // CP A, D
		CP(D);
		break;

	case 0xBB: // CP A, E
		CP(E);
		break;

	case 0xBC: // CP A, H
		CP(H);
		break;

	case 0xBD: // CP A, L
		CP(L);
		break;

	case 0xBE: // CP A, (HL)
		CP(bus->Read(HL));
		break;

	case 0xBF: // CP A, A
		CP(A);
		break;

	case 0xC0: // RET NZ
		RET(!F_Z);
		break;

	case 0xC1: // POP BC
		C = bus->Read(SP++);
		B = bus->Read(SP++);
		break;

	case 0xC2: // JP NZ a16
		JP(!F_Z);
		break;

	case 0xC3: // JP a16
		JP(true);
		break;

	case 0xC4: // CALL NZ, a16
		CALL(!F_Z);
		break;

	case 0xC5: // PUSH BC
		bus->Write(--SP, B);
		bus->Write(--SP, C);
		break;

	case 0xC6: // ADD A, d8
		ADD8(Read_u8());
		break;

	case 0xC7: // RST 00H
		RST(0x0000);
		break;

	case 0xC8: // RET Z
		RET(F_Z);
		break;

	case 0xC9: // RET
		RET(true);
		break;

	case 0xCA: // JP Z, a16
		JP(F_Z);
		break;

	case 0xCB: // PREFIX
	{
		u8 op = Read_u8();
		CB(op);
		m_cycles_instr += opcode_m_cycle_len_CB(op);
		break;
	}

	case 0xCC: // CALL Z, a16
		CALL(F_Z);
		break;

	case 0xCD: // CALL a16
		CALL(true);
		break;

	case 0xCE: // ADC A, d8
		ADC(Read_u8());
		break;

	case 0xCF: // RST 08H
		RST(0x0008);
		break;

	case 0xD0: // RET NC
		RET(!F_C);
		break;

	case 0xD1: // POP DE
		E = bus->Read(SP++);
		D = bus->Read(SP++);
		break;

	case 0xD2: // JP NC, a16
		JP(!F_C);
		break;

	case 0xD3: // -- no instruction
		IllegalOpcode(0xD3);
		break;

	case 0xD4: // CALL NC, a16
		CALL(!F_C);
		break;

	case 0xD5: // PUSH DE
		bus->Write(--SP, D);
		bus->Write(--SP, E);
		break;

	case 0xD6: // SUB A, d8
		SUB(Read_u8());
		break;

	case 0xD7: // RST 10H
		RST(0x0010);
		break;

	case 0xD8: // RET C
		RET(F_C);
		break;

	case 0xD9: // RETI
		IME = 1;
		RET(true);
		break;

	case 0xDA: // JP C, a16
		JP(F_C);
		break;

	case 0xDB: // -- no instruction
		IllegalOpcode(0xDB);
		break;

	case 0xDC: // CALL C, a16
		CALL(F_C);
		break;

	case 0xDD: // -- no instruction
		IllegalOpcode(0xDD);
		break;

	case 0xDE: // SBC A, d8
		SBC(Read_u8());
		break;

	case 0xDF: // RST 18H
		RST(0x0018);
		break;

	case 0xE0: // LDH (a8), A
		bus->Write(0xFF00 | Read_u8(), A);
		break;

	case 0xE1: // POP HL
		L = bus->Read(SP++);
		H = bus->Read(SP++);
		break;

	case 0xE2: // LD (C), A
		bus->Write(0xFF00 | C, A);
		break;

	case 0xE3: // -- no instruction
	case 0xE4: // -- no instruction
		IllegalOpcode(opcode);
		break;

	case 0xE5: // PUSH HL
		bus->Write(--SP, H);
		bus->Write(--SP, L);
		break;

	case 0xE6: // AND A, d8
		AND(Read_u8());
		break;

	case 0xE7: // RST 20H
		RST(0x0020);
		break;

	case 0xE8: // ADD SP, r8
	{
		s8 offset = Read_s8();
		F_H = (SP & 0xF) + (offset & 0xF) > 0xF; // check if overflow from bit 3
		F_C = (SP & 0xFF) + (offset & 0xFF) > 0xFF; // check if overflow from bit 7
		SP += offset;
		F_Z = F_N = 0;
		break;
	}

	case 0xE9: // JP HL
		PC = HL;
		break;

	case 0xEA: // LD (a16) A
		bus->Write(Read_u16(), A);
		break;

	case 0xEB: // -- no instruction
	case 0xEC: // -- no instruction
	case 0xED: // -- no instruction
		IllegalOpcode(opcode);
		break;

	case 0xEE: // XOR d8
		XOR(Read_u8());
		break;

	case 0xEF: // RST 28H
		RST(0x0028);
		break;

	case 0xF0: // LD A, (a8)
		A = bus->Read(0xFF00 | Read_u8());
		break;

	case 0xF1: // POP AF
	{
		u8 flags = bus->Read(SP++);
		F_Z = CheckBit(flags, 7);
		F_N = CheckBit(flags, 6);
		F_H = CheckBit(flags, 5);
		F_C = CheckBit(flags, 4);
		A = bus->Read(SP++);
		break;
	}

	case 0xF2: // LDH A, (C)
		A = bus->Read(0xFF00 | C);
		break;

	case 0xF3: // DI
		IME = 0;
		break;

	case 0xF4: // -- no instruction
		IllegalOpcode(0xF4);
		break;

	case 0xF5: // PUSH AF
		bus->Write(--SP, A);
		bus->Write(--SP, F);
		break;

	case 0xF6: // OR A, d8
		OR(Read_u8());
		break;

	case 0xF7: // RST 30H
		RST(0x0030);
		break;

	case 0xF8: // LD HL, SP + r8
	{
		s16 offset = Read_s8();
		F_H = (SP & 0xF) + (offset & 0xF) > 0xF; // check if overflow from bit 3
		F_C = (SP & 0xFF) + (offset & 0xFF) > 0xFF; // check if overflow from bit 7
		u16 result = SP + offset;
		H = result >> 8;
		L = result & 0xFF;
		F_Z = F_N = 0;
		break;
	}

	case 0xF9: // LD SP, HL
		SP = HL;
		break;

	case 0xFA: // LD A, (a16)
		A = bus->Read(Read_u16());
		break;

	case 0xFB: // EI
		// The effect of EI is delayed by one instruction. This means that EI followed immediately by DI does not allow interrupts between the EI and the DI.
		EI_called = true;
		instr_until_set_IME = 1;
		break;

	case 0xFC: // -- no instruction
	case 0xFD: // -- no instruction
		IllegalOpcode(opcode);
		break;

	case 0xFE: // CP d8
		CP(Read_u8());
		break;

	case 0xFF: // RST 38H
		RST(0x0038);
		break;
	}
}


// Add op and carry flag to register A
void CPU::ADC(u8 op)
{
	u16 delta = op + F_C;
	F_N = 0;
	F_H = (A & 0xF) + (op & 0xF) + F_C > 0xF; // check if overflow from bit 3
	F_C = (A + delta > 0xFF); // check if overflow from bit 7
	A += delta;
	F_Z = A == 0;
}


// Add op to register A
void CPU::ADD8(u8 op)
{
	F_N = 0;
	F_H = ((A & 0xF) + (op & 0xF) > 0xF); // check if overflow from bit 3
	F_C = (A + op > 0xFF); // check if overflow from bit 7
	A += op;
	F_Z = A == 0;
}


// Add op to register HL
void CPU::ADD16(u16 op)
{
	F_N = 0;
	F_H = (HL & 0xFFF) + (op & 0xFFF) > 0xFFF; // check if overflow from bit 11
	F_C = (HL + op > 0xFFFF); // check if overflow from bit 15
	u16 result = HL + op;
	H = result >> 8;
	L = result & 0xFF;
}


// Store bitwise AND between op and A in A
void CPU::AND(u8 op)
{
	A &= op;
	F_Z = A == 0;
	F_N = 0;
	F_H = 1;
	F_C = 0;
}


void CPU::CP(u8 op)
{
	F_Z = A - op == 0;
	F_N = 1;
	F_H = (op & 0xF) > (A & 0xF); // check if borrow from bit 4
	F_C = op > A; // check if borrow
}


void CPU::DEC(u8& op)
{
	F_N = 1;
	F_H = (op & 0xF) == 0; // check if borrow from bit 4
	op--;
	F_Z = op == 0;
}


void CPU::INC(u8& op)
{
	F_N = 0;
	F_H = (op & 0xF) == 0xF; // check if overflow from bit 3
	op++;
	F_Z = op == 0;
}


void CPU::OR(u8 op)
{
	A |= op;
	F_Z = A == 0;
	F_N = F_H = F_C = 0;
}


void CPU::SBC(u8 op)
{
	u16 delta = op + F_C;
	F_N = 1;
	F_H = (op & 0xF) + F_C > (A & 0xF); // check if borrow from bit 4
	F_C = delta > A; // check if borrow
	A -= delta;
	F_Z = A == 0;
}


void CPU::SUB(u8 op)
{
	F_N = 1;
	F_H = (op & 0xF) > (A & 0xF); // check if borrow from bit 4
	F_C = op > A; // check if borrow
	A -= op;
	F_Z = A == 0;
}


void CPU::XOR(u8 op)
{
	A ^= op;
	F_Z = A == 0;
	F_N = F_H = F_C = 0;
}


// Rotate bits in register reg left through carry.
void CPU::RL(u8* reg)
{
	bool bit7 = *reg >> 7;
	*reg = (*reg & 0xFF >> 1) << 1 | F_C;
	F_C = bit7;
	F_Z = *reg == 0;
	F_N = F_H = 0;
}


void CPU::RLC(u8* reg)
{
	F_C = *reg >> 7;
	*reg = _rotl8(*reg, 1); // rotate A left one bit.
	F_Z = *reg == 0;
	F_N = F_H = 0;
}


// Rotate register reg right through carry.
void CPU::RR(u8* reg)
{
	bool bit0 = *reg & 1u;
	*reg = *reg >> 1 | F_C << 7;
	F_C = bit0;
	F_Z = *reg == 0;
	F_N = F_H = 0;
}


void CPU::RRC(u8* reg)
{
	F_C = *reg & 1u;
	*reg = _rotr8(*reg, 1); // rotate A right one bit.
	F_Z = *reg == 0;
	F_N = F_H = 0;
}


void CPU::SLA(u8* reg)
{
	F_C = *reg >> 7;
	*reg = *reg << 1 & 0xFF;
	F_Z = *reg == 0;
	F_N = F_H = 0;
}


void CPU::SRA(u8* reg)
{
	F_C = *reg & 1u;
	*reg = *reg >> 1 | (*reg >> 7) << 7;
	F_Z = *reg == 0;
	F_N = F_H = 0;
}


void CPU::SRL(u8* reg)
{
	F_C = *reg & 1u;
	*reg >>= 1;
	F_Z = *reg == 0;
	F_N = F_H = 0;
}


void CPU::BIT(u8 pos, u8* reg)
{
	F_Z = CheckBit(reg, pos) == 0;
	F_N = 0;
	F_H = 1;
}


void CPU::RES(u8 pos, u8* reg)
{
	ClearBit(reg, pos);
}


void CPU::SET(u8 pos, u8* reg)
{
	SetBit(reg, pos);
}


void CPU::SWAP(u8* reg)
{
	*reg = _rotl8(*reg, 4);
	F_Z = *reg == 0;
	F_N = F_H = F_C = 0;
}


void CPU::CB(u8 opcode)
{
	u8* reg = nullptr;
	bool reg_is_HL = false;
	u8 ret;
	switch ((opcode & 0xF) % 8)
	{
	case 0: reg = &B; break;
	case 1: reg = &C; break;
	case 2: reg = &D; break;
	case 3: reg = &E; break;
	case 4: reg = &H; break;
	case 5: reg = &L; break;
	case 6:
		ret = bus->Read(HL);
		reg = &ret;
		reg_is_HL = true;
		break;
	case 7: reg = &A; break;
	}

	switch (opcode / 8)
	{
	case 0: RLC (reg); break;
	case 1: RRC (reg); break;
	case 2: RL  (reg); break;
	case 3: RR  (reg); break;
	case 4: SLA (reg); break;
	case 5: SRA (reg); break;
	case 6: SWAP(reg); break;
	case 7: SRL (reg); break;
	default:
		if      (opcode <= 0x7F) BIT((opcode - 0x40) / 8, reg);
		else if (opcode <= 0xBF) RES((opcode - 0x80) / 8, reg);
		else                     SET((opcode - 0xC0) / 8, reg);
		break;
	}

	// do not write to (HL) again for BIT instructions; these have no side effects on (HL)
	if (reg_is_HL && !(opcode >= 0x40 && opcode <= 0x7F))
		bus->Write(HL, *reg);
}


void CPU::CALL(bool cond)
{
	u16 newPC = Read_u16();
	if (cond)
	{
		PushPC();
		PC = newPC;
		action_taken = true;
	}
}


void CPU::RET(bool cond)
{
	if (cond)
	{
		PopPC();
		action_taken = true;
	}
}


// Relative Jump by adding e8 to the address of the instruction following the JR
void CPU::JR(bool cond)
{
	s8 offset = Read_s8();
	if (cond)
	{
		PC += offset;
		action_taken = true;
	}
}


void CPU::JP(bool cond)
{
	u16 word = Read_u16();
	if (cond)
	{
		PC = word;
		action_taken = true;
	}
}


void CPU::RST(u16 addr)
{
	PushPC();
	PC = addr;
}


unsigned CPU::OneInstruction()
{
	u8 opcode = bus->Read(PC);

	#ifdef DEBUG
		char buf[200]{};
		sprintf(buf, "#%i\tPC: %04X\tOP: %02X\tSP: %04X\tAF: %04X\tBC: %04X\tDE: %04X\tHL: %04X\tIME: %i\tIE: %02X\tIF: %02X\tLCDC: %02X\tSTAT: %02X\tLY: %02X\tROM: %04X",
			instruction_counter++, (int)PC, (int)opcode, (int)SP, (int)AF, (int)BC, (int)DE, (int)HL, (int)IME, 
			(int)bus->Read(Bus::Addr::IE), (int)bus->Read(Bus::Addr::IF), (int)bus->Read(Bus::Addr::LCDC), (int)bus->Read(Bus::Addr::STAT), (int)bus->Read(Bus::Addr::LY), (int)bus->GetCurrentRomBank());
		ofs << buf << std::endl;
	#endif
	
	// If the previous instruction was HALT, there is a hardware bug in which PC is not incremented after the current instruction
	if (!HALT_bug)
		PC++;
	else
		HALT_bug = false;

	m_cycles_instr = 0;
	ExecuteInstruction(opcode); // note: 'm_cycles_instr' is incremented in ExecuteInstruction() when the opcode is prefixed
	m_cycles_instr += opcode_m_cycle_len[opcode] + action_taken * opcode_m_cycle_len_jump[opcode];
	
	action_taken = false;

	return m_cycles_instr;
}

void CPU::Update()
{
	if (HDMA_transfer_active) 
		return;

	if (speed_switch_active && --speed_switch_m_cycles_remaining == 0)
		ExitSpeedSwitch();

	if (HALT)
	{
		m_cycles_until_next_instr = CheckInterrupts();
		return;
	}

	if (--m_cycles_until_next_instr == 0)
	{
		unsigned m_cycles_update = 0;
		// If handling interrupts took at least 1 m-cycle on last update, then do not check interrupts again, only execute an instruction. 
		// This allows for "finer" updating of the cpu
		if (!interrupt_dispatched_on_last_update)
		{
			m_cycles_update += CheckInterrupts();
			if (m_cycles_update == 0)
				m_cycles_update += OneInstruction();
		}
		else
		{
			m_cycles_update += OneInstruction();
			interrupt_dispatched_on_last_update = false;
		}
		m_cycles_until_next_instr = m_cycles_instr;
	}
	// todo: it is possible for OneInstruction() to return 0 if an undefined opcode is met

	if (EI_called && instr_until_set_IME-- == 0)
	{
		IME = 1;
		EI_called = false;
	}
}


unsigned CPU::CheckInterrupts()
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
		if (AND != 0) // if any enabled interrupt is requested
		{
			PushPC();

			for (int i = 0; i <= 4; i++)
			{
				if (CheckBit(AND, i) == 1)
				{
					PC = interrupt_vector[i];
					ClearBit(IF, i);
				}
			}

			int m_cycles = HALT ? 5 : 6; // takes 5 m-cycles to dispatch interrupt, an extra m-cycle if in HALT mode
			IME = HALT = false;
			interrupt_dispatched_on_last_update = true;
			return m_cycles;
		}
	}
	else if (HALT)
	{ // check if HALT mode should be exited, which is when enabled interrupt is requested
		if ((*IF & *IE & 0x1F) != 0)
		{
			HALT = false;
			interrupt_dispatched_on_last_update = true;
			return 1; // one m-cycle to exit HALT mode
		}
	}
	interrupt_dispatched_on_last_update = false;
	return 0;
}


void CPU::EnterHALT()
{
	if (IME || (*IF & *IE & 0x1F) == 0)
	{
		HALT = true;
	}
	else
	{ // HALT is not entered, causing a bug where the CPU fails to increase PC when executing the next instruction
		HALT_bug = true;
		m_cycles_instr += 1;
	}
}


void CPU::EnterSTOP()
{
	// on DMG, enter low power mode. On the CGB, switch between normal and double speed (if KEY1.0 has been set)
	if (System::mode == System::Mode::CGB)
	{
		u8 KEY1 = bus->Read(Bus::Addr::KEY1);
		if (CheckBit(KEY1, 0) == 1)
			InitiateSpeedSwitch();
	}
	else
	{
		bus->ResetDIV();

		STOP = true;
		// todo. Seems like no licensed rom makes use of STOP in DMG mode anyways
	}
}


void CPU::PushPC()
{
	bus->Write(--SP, PC >> 8);
	bus->Write(--SP, PC & 0xFF);
}


void CPU::PopPC()
{
	u8 low = bus->Read(SP++);
	u8 high = bus->Read(SP++);
	PC = high << 8 | low;
}


void CPU::RequestInterrupt(CPU::Interrupt interrupt)
{
	SetBit(IF, interrupt);
}


void CPU::DAA()
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


void CPU::IllegalOpcode(u8 opcode)
{
	wxMessageBox(wxString::Format("Illegal opcode 0x%02X encountered.", opcode));
}


void CPU::State(Serialization::BaseFunctor& functor)
{
	functor.fun(&HALT, sizeof(bool));
	functor.fun(&HDMA_transfer_active, sizeof(bool));
	functor.fun(&speed_switch_active, sizeof(bool));
	functor.fun(&STOP, sizeof(bool));

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

	functor.fun(&action_taken, sizeof(bool));
	functor.fun(&EI_called, sizeof(bool));
	functor.fun(&HALT_bug, sizeof(bool));
	functor.fun(&IME, sizeof(bool));
	functor.fun(&interrupt_dispatched_on_last_update, sizeof(bool));

	functor.fun(&m_cycles_instr, sizeof(unsigned));
	functor.fun(&m_cycles_until_next_instr, sizeof(unsigned));
	functor.fun(&speed_switch_m_cycles_remaining, sizeof(unsigned));
	functor.fun(&instr_until_set_IME, sizeof(unsigned));
}