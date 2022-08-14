#define SDL_MAIN_HANDLED

import Core;
import Emulator;
import Frontend;
import GB;
import UserMessage;

import <format>;
import <memory>;
import <string>;

int main(int argc, char** argv)
{
	std::shared_ptr<Core> core = std::make_shared<GB>();
	Emulator::SetCore(core);
	if (!Frontend::Initialize()) {
		exit(1);
	}
	/* Optional CLI arguments (beyond executable path):
		1; path to rom
		2; path to bios
	*/
	bool boot_game_immediately = false;
	if (argc >= 2) {
		std::string rom_path = argv[1];
		if (!core->LoadRom(rom_path)) {
			UserMessage::Show(std::format("Could not open rom file at path \"{}\"\n", rom_path),
				UserMessage::Type::Warning);
		}
		else {
			boot_game_immediately = true;
		}
		if (argc >= 3) {
			std::string bios_path{};
			bios_path = argv[2];
			if (!core->LoadBios(bios_path)) {
				UserMessage::Show(std::format("Could not open the bios file at path \"{}\"\n", bios_path),
					UserMessage::Type::Warning);
			}
		}
	}
	Frontend::RunGui(boot_game_immediately);
	Frontend::Shutdown();
}