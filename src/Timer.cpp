module Timer;

import APU;
import CPU;
import System;

namespace Timer
{
	void Initialize()
	{
		WriteDIV(0x00);
		WriteTAC(0x04);
		WriteTIMA(0x00);
		WriteTMA(0x00);
		prev_tima_and_result = prev_div_and_result = 0;
		awaiting_interrupt_request = false;
		div_enabled = true;
	}


	u8 ReadDIV()
	{
		return div >> 8;
	}


	u8 ReadTAC()
	{
		return tac;
	}


	u8 ReadTIMA()
	{
		return tima;
	}


	u8 ReadTMA()
	{
		return tma;
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamPrimitive(awaiting_interrupt_request);
		stream.StreamPrimitive(div_enabled);
		stream.StreamPrimitive(prev_div_and_result);
		stream.StreamPrimitive(prev_tima_and_result);
		stream.StreamPrimitive(tima_enabled);
		stream.StreamPrimitive(tac);
		stream.StreamPrimitive(tima);
		stream.StreamPrimitive(tma);
		stream.StreamPrimitive(div);
		stream.StreamPrimitive(and_bit_pos_index);
	}


	void Update()
	{
		if (awaiting_interrupt_request) {
			tima = tma;
			CPU::RequestInterrupt(CPU::Interrupt::Timer);
			awaiting_interrupt_request = false;
		}

		if (div_enabled) {
			div += 4; /* one increment per t-cycle */
			static constexpr std::array and_bit_pos = { 9, 3, 5, 7 };
			bool and_result = tima_enabled && (div >> and_bit_pos[and_bit_pos_index] & 1);
			if (and_result == 0 && prev_tima_and_result == 1) {
				if (++tima == 0) {
					awaiting_interrupt_request = true; /* 1 m-cycle delay until interrupt fire */
				}
			}
			prev_tima_and_result = and_result;
			/* Step the APU frame sequencer when bit 13 of DIV becomes 1 (bit 14 in double speed mode).
			   This is equivalent to bits 0-12 being cleared (0-13 in double speed mode). */
			and_result = div & (0x3FFF >> (2 - std::to_underlying(System::speed)));
			if (APU::Enabled() && and_result == 0 && prev_div_and_result == 1) {
				APU::StepFrameSequencer();
			}
			prev_div_and_result = and_result;
		}
	}


	void WriteDIV(u8 data)
	{
		div = 0;
	}


	void WriteTAC(u8 data)
	{
		tac = data & 7; /* Only the lower 3 bits are R/W. TODO: what do the rest return? */
		and_bit_pos_index = data & 3;
		tima_enabled = data & 4;
	}


	void WriteTIMA(u8 data)
	{
		tima = data;
	}


	void WriteTMA(u8 data)
	{
		tma = data;
	}
}