#include "System.h"

System::Mode System::mode = System::Mode::NONE;
System::SpeedMode System::speed_mode = System::SpeedMode::SINGLE;
bool System::SGB_available = false;

void System::ToggleSpeedMode()
{
	if (speed_mode == SINGLE) speed_mode = DOUBLE;
	else speed_mode = SINGLE;
}