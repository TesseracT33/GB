#pragma once

#include "Bus.h"
#include "CPU.h"

class Serial : public Component 
{
public:
	Bus* bus;
	CPU* cpu;

	void Initialize();
	void Reset();
	void TriggerTransfer();
	void Update();

	void State(Serialization::BaseFunctor& functor) override;

private:
	const u8 incoming_byte = 0xFF;
	const unsigned m_cycles_per_transfer_update = 2048;

	bool transfer_active = false;

	u8 outgoing_byte;
	u8 bits_transferred;

	unsigned m_cycles_until_transfer_update = 0;

	u8* SB, *SC;
};

