#include "Serial.h"


void Serial::Initialize()
{
	SB = bus->ReadIOPointer(Bus::Addr::SB);
	SC = bus->ReadIOPointer(Bus::Addr::SC);
}


void Serial::Reset()
{
	transfer_active = false;
}


void Serial::TriggerTransfer()
{
	if (transfer_active) return; // todo: correct?

	transfer_active = true;
	m_cycles_until_transfer_update = m_cycles_per_transfer_update;
	outgoing_byte = *SB;
	bits_transferred = 0;
}


void Serial::Update()
{
	if (!transfer_active) return;

	if (--m_cycles_until_transfer_update == 0)
	{
		bits_transferred++;
		// see https://gbdev.io/pandocs/Serial_Data_Transfer_(Link_Cable).html for the blend of outgoing and incoming bytes
		*SB = (outgoing_byte << bits_transferred) | (incoming_byte >> (8 - bits_transferred));

		bool transfer_complete = bits_transferred == 8;
		if (transfer_complete)
		{
			*SC &= 0x7F;
			cpu->RequestInterrupt(CPU::Interrupt::Serial);
			transfer_active = false;
			return;
		}

		m_cycles_until_transfer_update = m_cycles_per_transfer_update;
	}
}


void Serial::Serialize(std::ofstream& ofs)
{
	ofs.write((char*)&transfer_active, sizeof(bool));
	ofs.write((char*)&outgoing_byte, sizeof(u8));
	ofs.write((char*)&bits_transferred, sizeof(u8));
	ofs.write((char*)&m_cycles_until_transfer_update, sizeof(unsigned));
}


void Serial::Deserialize(std::ifstream& ifs)
{
	ifs.read((char*)&transfer_active, sizeof(bool));
	ifs.read((char*)&outgoing_byte, sizeof(u8));
	ifs.read((char*)&bits_transferred, sizeof(u8));
	ifs.read((char*)&m_cycles_until_transfer_update, sizeof(unsigned));
}