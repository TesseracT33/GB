#pragma once

class System
{
public:
	System() = delete;

	enum class Mode { NONE, DMG, CGB, CGB_IN_DMG_MODE } static mode;
	enum SpeedMode : unsigned { SINGLE = 1u, DOUBLE = 2u } static speed_mode;

	// currently unused
	static bool SGB_available;

	static void ToggleSpeedMode();
};