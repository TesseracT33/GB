export module System;

import Util;

import <utility>;

export namespace System
{
	enum class Mode {
		DMG, CGB
	} mode;
	
	enum class Speed : uint {
		Single = 1, Double = 2
	} speed = Speed::Single;

	void EndSpeedSwitchInitialization();
	void Initialize();
	u8 ReadKey1();
	bool SpeedSwitchPrepared();
	void StepAllComponentsButCpu();
	void StreamState(SerializationStream& stream);
	void WriteKey1(u8 data);

	constexpr uint m_cycles_per_frame_base = 17556;
	constexpr uint m_cycles_per_sec_base = 1048576;
	constexpr uint t_cycles_per_sec_base = 4194304;

	bool prepare_speed_switch; /* change by writing to KEY1.0 */
}