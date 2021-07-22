#pragma once
#include "APU.h"
#include "Bus.h"
#include "CPU.h"
#include "System.h"
#include "Utils.h"

class Timer final : public Serializable
{
public: 
	APU* apu;
	Bus* bus;
	CPU* cpu;

	u8* DIV_bus;
	u8* TAC;
	u8* TIMA;
	u8* TMA;

	u16 DIV = 0;
	bool prev_AND_result = 0;

	bool TIMA_enabled = true;
	bool DIV_enabled = true;

	void Serialize(std::ofstream& ofs) override;
	void Deserialize(std::ifstream& ifs) override;

	void Update();
	void Initialize();
	void Reset();

	void SetTIMAFreq(u8 index);
	void ResetDIV();

private:
	bool awaiting_interrupt_request = false;
	unsigned t_cycles_until_interrupt_request = 0;

	const u8 AND_bit_pos[4] = { 9, 3, 5, 7 };
	unsigned AND_bit_pos_index = 0;
};

