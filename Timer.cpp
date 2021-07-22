#include "Timer.h"

using namespace Util;


void Timer::Initialize()
{
	DIV_bus = bus->ReadIOPointer(Bus::Addr::DIV);
	TAC = bus->ReadIOPointer(Bus::Addr::TAC);
	TIMA = bus->ReadIOPointer(Bus::Addr::TIMA);
	TMA = bus->ReadIOPointer(Bus::Addr::TMA);
}


void Timer::Reset()
{
	SetBit(TAC, 2);
	DIV = prev_AND_result = AND_bit_pos_index = 0;
	DIV_enabled = TIMA_enabled = true;
	awaiting_interrupt_request = false;
}


void Timer::Update()
{
	for (int i = 0; i < 4; i++)
	{
		if (awaiting_interrupt_request && --t_cycles_until_interrupt_request == 0)
		{
			*TIMA = *TMA;
			cpu->RequestInterrupt(CPU::Interrupt::Timer);
			awaiting_interrupt_request = false;
		}

		if (!DIV_enabled) continue;

		DIV++;
		if ((DIV & 0xFF) == 0)
		{
			(*DIV_bus)++;

			// step the APU frame sequencer when bit 5 of upper byte of DIV becomes 1 (bit 6 in double speed mode)
			if ((*DIV_bus & 0x20 * System::speed_mode) == 0x20 * System::speed_mode)
				apu->Step_Frame_Sequencer();
		}

		bool AND_result = CheckBit(DIV, AND_bit_pos[AND_bit_pos_index]) && TIMA_enabled;
		if (AND_result == 0 && prev_AND_result == 1)
		{
			(*TIMA)++;
			if (*TIMA == 0)
			{
				awaiting_interrupt_request = true;
				t_cycles_until_interrupt_request = 4;
			}
		}
		prev_AND_result = AND_result;
	}
}


void Timer::SetTIMAFreq(u8 index)
{
	AND_bit_pos_index = index;
}


void Timer::ResetDIV()
{
	DIV = *DIV_bus = 0;
}


void Timer::Deserialize(std::ifstream& ifs)
{
	ifs.read((char*)&DIV, sizeof(DIV));
	ifs.read((char*)&prev_AND_result, sizeof(bool));
	ifs.read((char*)&TIMA_enabled, sizeof(bool));
	ifs.read((char*)&DIV_enabled, sizeof(bool));
	ifs.read((char*)&awaiting_interrupt_request, sizeof(bool));
	ifs.read((char*)&t_cycles_until_interrupt_request, sizeof(unsigned));
	ifs.read((char*)&AND_bit_pos_index, sizeof(unsigned));
}


void Timer::Serialize(std::ofstream& ofs)
{
	ofs.write((char*)&DIV, sizeof(DIV));
	ofs.write((char*)&prev_AND_result, sizeof(bool));
	ofs.write((char*)&TIMA_enabled, sizeof(bool));
	ofs.write((char*)&DIV_enabled, sizeof(bool));
	ofs.write((char*)&awaiting_interrupt_request, sizeof(bool));
	ofs.write((char*)&t_cycles_until_interrupt_request, sizeof(unsigned));
	ofs.write((char*)&AND_bit_pos_index, sizeof(unsigned));
}