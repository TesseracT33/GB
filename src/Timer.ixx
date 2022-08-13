export module Timer;

import NumericalTypes;
import SerializationStream;

namespace Timer
{
	export
	{
		void Initialize();
		u8 ReadDIV();
		u8 ReadTAC();
		u8 ReadTIMA();
		u8 ReadTMA();
		void StreamState(SerializationStream& stream);
		void Update();
		void WriteDIV(u8 data);
		void WriteTAC(u8 data);
		void WriteTIMA(u8 data);
		void WriteTMA(u8 data);
	}

	bool awaiting_interrupt_request;
	bool div_enabled;
	bool prev_div_and_result;
	bool prev_tima_and_result;
	bool tima_enabled;

	u8 tac;
	u8 tima;
	u8 tma;
	u16 div;

	uint and_bit_pos_index = 0;
}