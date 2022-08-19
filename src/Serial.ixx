export module Serial;

import Util;

namespace Serial
{
	export
	{
		void Initialize();
		u8 ReadSB();
		u8 ReadSC();
		void StreamState(SerializationStream& stream);
		void Update();
		void WriteSB(u8 data);
		void WriteSC(u8 data);
	}

	void TriggerTransfer();

	constexpr u8 incoming_byte = 0;

	constexpr uint m_cycles_per_transfer_update = 2048;

	bool transfer_active;

	u8 outgoing_byte;
	u8 sb, sc;
	
	uint m_cycles_until_transfer_update;
	uint num_bits_transferred;
}