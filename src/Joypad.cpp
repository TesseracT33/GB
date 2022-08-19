module Joypad;

import CPU;
import Input;

namespace Joypad
{
	void Initialize()
	{
		p1 = 0xFF;
		button_currently_held.fill(false);
	}


	void NotifyButtonPressed(uint button_index)
	{
		button_currently_held[button_index] = true;
		UpdateOutputLines();
	}


	void NotifyButtonReleased(uint button_index)
	{
		button_currently_held[button_index] = false;
		UpdateOutputLines();
	}


	u8 ReadP1()
	{
		return p1;
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamPrimitive(p1);
	}


	void UpdateOutputLines()
	{
		static constexpr auto index_a = std::to_underlying(Button::A);
		static constexpr auto index_b = std::to_underlying(Button::B);
		static constexpr auto index_select = std::to_underlying(Button::Select);
		static constexpr auto index_start = std::to_underlying(Button::Start);
		static constexpr auto index_right = std::to_underlying(Button::Right);
		static constexpr auto index_left = std::to_underlying(Button::Left);
		static constexpr auto index_up = std::to_underlying(Button::Up);
		static constexpr auto index_down = std::to_underlying(Button::Down);

		u8 prev_p1 = p1;
		bool p14 = p1 & 0x10;
		bool p15 = p1 & 0x20;
		bool p10 = !(button_currently_held[index_right] && !p14 || button_currently_held[index_a] && !p15);
		bool p11 = !(button_currently_held[index_left] && !p14 || button_currently_held[index_b] && !p15);
		bool p12 = !(button_currently_held[index_up] && !p14 || button_currently_held[index_select] && !p15);
		bool p13 = !(button_currently_held[index_down] && !p14 || button_currently_held[index_start] && !p15);
		p1 &= 0xF0;
		p1 |= p10 | p11 << 1 | p12 << 2 | p13 << 3;
		// request interrupt if P10, P11, P12, or P13 has gone from 1 to 0
		if (~p1 & prev_p1 & 0xF) {
			CPU::RequestInterrupt(CPU::Interrupt::Joypad);
		}
	}


	void WriteP1(u8 data)
	{
		u8 prev_select = p1 & 0x30;
		u8 new_select = data & 0x30;
		if (prev_select != new_select) {
			// only bits 4-5 are writable
			p1 = p1 & 0xCF | new_select;
			// update bits 0-3 of P1. These are not writeable, but can be changed if bits 4-5 are changed, if a button is also currently held
			UpdateOutputLines();
		}
	}
}