# Summary
gb-chan is a Nintendo Game Boy emulator written in C++ with a graphical interface (using wxWidgets-3). It relies on SDL2 for video, audio and input.

![gb1](https://imgbox.com/pPp2HJnb)
![gb2](https://imgbox.com/hb3IwDTb)
![gb3](https://imgbox.com/hS1IXnGF)
![gb4](https://imgbox.com/xTrAw66X)

# What is the Nintendo Game Boy?
The Nintendo Game Boy is a handheld video game system initially released in 1989. Its specs include an 8-bit processor running at 4.19 MHz, 8 KiB of video memory, a 160x144 resolution 2-bit monochromatic display.

# Current features
- OK-ish game compatibility.
- A listbox for displaying and selecting games.
- Configurable input bindings.
- Colour scheme configuration with 13 selectable predefined palettes.
- All configuration settings are saved to disk, and loaded every time the application is started. 
- Game save data is saved to disk for games that utilize battery-buffered external cartridge RAM (however, this has not been tested much yet). 

# WIP features (incomplete or non-functioning)
- Audio output (incomplete).
- Save states (incomplete).
- Controller input support (incomplete and non-functioning).

# Future features (hopefully)
- Game Boy Colour support (already underway).

# Tests
The emulator passes the following tests:
- Blargg -- cpu_instr
- mattcurrie -- dmg-acid2
- mooneye -- all MBC1, MBC2 and MBC5 tests, except for mbc1/multicart_rom_8Mb

# Known bugs and issues
- Various games have issues (Cannot get past the menus in Pokémon Red/Blue, input not working in Alleyway).
- Some games do not boot correctly if the boot rom is not booted first.
- Controller input is not detected at all.
- When you open the input binding window, a wxWidgets warning is displayed.

# Notes on usage
It is recommended that you supply a boot rom (currently, some games have issues when booted without the boot rom (this will be fixed)). The boot rom for the original Game Boy should have file extension .gb and be 256 bytes in size. You can supply the emulator with it from within the GUI. 

# Compiling and running
Current external dependencies are wxWidgets and SDL2. I only supply Visual Studio solution files. The project settings were as follows:

C/C++ -- Additional Include Directories:
F:\SDKs\wxWidgets-3.1.3\include\msvc; F:\SDKs\wxWidgets-3.1.3\include; F:\SDKs\SDL2-2.0.12\include

Linker -- Additional Library Directories:
F:\SDKs\wxWidgets-3.1.3\lib\vc_x64_lib; F:\SDKs\SDL2-2.0.12\lib\x64;

Linker -- Input -- Additional Dependencies:
SDL2.lib; SDL2main.lib;