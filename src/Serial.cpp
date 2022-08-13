module Serial;

import CPU;

namespace Serial
{
	void Initialize()
	{
		transfer_active = false;
		sb = sc = 0; /* TODO: what are def values? */
	}


	u8 ReadSB()
	{
		return sb;
	}


	u8 ReadSC()
	{
		return sc;
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamPrimitive(transfer_active);
		stream.StreamPrimitive(outgoing_byte);
		stream.StreamPrimitive(sb);
		stream.StreamPrimitive(sc);
		stream.StreamPrimitive(m_cycles_until_transfer_update);
		stream.StreamPrimitive(num_bits_transferred);
	}


	void TriggerTransfer()
	{
		if (!transfer_active) { /* TODO: correct? */
			transfer_active = true;
			m_cycles_until_transfer_update = m_cycles_per_transfer_update;
			outgoing_byte = sb;
			num_bits_transferred = 0;
		}
	}
		
	
	void Update()
	{
		if (transfer_active && --m_cycles_until_transfer_update == 0) {
			++num_bits_transferred;
			// see https://gbdev.io/pandocs/Serial_Data_Transfer_(Link_Cable).html for the blend of outgoing and incoming bytes
			sb = (outgoing_byte << num_bits_transferred) | (incoming_byte >> (8 - num_bits_transferred));
			bool transfer_complete = num_bits_transferred == 8;
			if (transfer_complete) {
				sc &= 0x7F;
				CPU::RequestInterrupt(CPU::Interrupt::Serial);
				transfer_active = false;
			}
			else {
				m_cycles_until_transfer_update = m_cycles_per_transfer_update;
			}
		}
	}


	void WriteSB(u8 data)
	{
		sb = data;
	}


	void WriteSC(u8 data)
	{
		sc = data;
		if (data == 0x81) {
			TriggerTransfer();
		}
	}
}