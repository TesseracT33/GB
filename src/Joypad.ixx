export module Joypad;

import Util;

import <array>;
import <utility>;

namespace Joypad
{
	export
	{
		enum class Button {
			A, B, Select, Start, Right, Left, Up, Down
		};

		void Initialize();
		void NotifyButtonPressed(uint button_index);
		void NotifyButtonReleased(uint button_index);
		u8 ReadP1();
		void StreamState(SerializationStream& stream);
		void WriteP1(u8 data);
	}

	void UpdateOutputLines();

	u8 p1;

	std::array<bool, 8> button_currently_held;
}