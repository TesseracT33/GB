module System;

import APU;
import Bus;
import CPU;
import DMA;
import Serial;
import Timer;
import PPU;

namespace System
{
	void EndSpeedSwitchInitialization()
	{
		if (speed == Speed::Single) {
			speed = Speed::Double;
		}
		else {
			speed = Speed::Single;
		}
		prepare_speed_switch = false;
	}


	void Initialize()
	{
		prepare_speed_switch = false;
		speed = Speed::Single;
	}


	u8 ReadKey1()
	{
		// bits 1-6 always return 1. in DMG mode, always return 0xFF
		if (mode == Mode::DMG) {
			return 0xFF;
		}
		else {
			static_assert(std::to_underlying(Speed::Single) == 1);
			static_assert(std::to_underlying(Speed::Double) == 2);
			bool bit0 = prepare_speed_switch;
			bool bit7 = std::to_underlying(speed) - 1;
			return 0x7E | bit0 | bit7 << 7;
		}
	}


	bool SpeedSwitchPrepared()
	{
		return prepare_speed_switch;
	}


	void StepAllComponentsButCpu()
	{
		APU::Update();
		DMA::Update();
		PPU::Update();
		Serial::Update();
		Timer::Update();
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamPrimitive(speed);
		stream.StreamPrimitive(prepare_speed_switch);
	}


	void WriteKey1(u8 data)
	{
		if (mode == Mode::CGB) {
			if (!prepare_speed_switch && (data & 1)) {
				prepare_speed_switch = true;
			}
		}
	}
}