#pragma once

#include "SDL.h"

#include <array>
#include <vector>

namespace Themes
{
	// https://lospec.com/palette-list/2-bit-grayscale
	constexpr std::array<SDL_Color, 4> grayscale = { { { 0xFF, 0xFF, 0xFF, 0 },
												       { 0xB6, 0xB6, 0xB6, 0 },
													   { 0x67, 0x67, 0x67, 0 },
													   { 0x00, 0x00, 0x00, 0 } } };

	constexpr std::array<SDL_Color, 4> inverted_grayscale = { { { 0x00, 0x00, 0x00, 0 },
													            { 0x67, 0x67, 0x67, 0 },
													            { 0xB6, 0xB6, 0xB6, 0 },
													            { 0xFF, 0xFF, 0xFF, 0 } } };

	// https://lospec.com/palette-list/nostalgia
	constexpr std::array<SDL_Color, 4> nostalgia = { { { 0xD0, 0xD0, 0x58, 0 },
												       { 0xA0, 0xA8, 0x40, 0 },
													   { 0x70, 0x80, 0x28, 0 },
												       { 0x40, 0x50, 0x10, 0 } } };

	// https://lospec.com/palette-list/ice-cream-gb
	constexpr std::array<SDL_Color, 4> icecream = { { { 0x7C, 0x3F, 0x58, 0 },
												      { 0xEB, 0x6B, 0x6F, 0 },
												      { 0xF9, 0xA8, 0x75, 0 },
												      { 0xFF, 0xF6, 0xD3, 0 } } };

	// https://lospec.com/palette-list/kirokaze-gameboy
	constexpr std::array<SDL_Color, 4> kirokaze = { { { 0x33, 0x2C, 0x50, 0 },
												      { 0x46, 0x87, 0x8F, 0 },
												      { 0x94, 0xE3, 0x44, 0 },
												      { 0xE2, 0xF3, 0xE4, 0 } } };

	// https://lospec.com/palette-list/spacehaze
	constexpr std::array<SDL_Color, 4> spacehaze = { { { 0xF8, 0xE3, 0xC4, 0 },
												       { 0xCC, 0x34, 0x95, 0 },
													   { 0x6B, 0x1F, 0xB1, 0 },
													   { 0x0B, 0x06, 0x30, 0 } } };

	// https://lospec.com/palette-list/purpledawn
	constexpr std::array<SDL_Color, 4> purpledawn = { { { 0xEE, 0xFD, 0xED, 0 },
													    { 0x9A, 0x7B, 0xBC, 0 },
													    { 0x2D, 0x75, 0x7E, 0 },
													    { 0x00, 0x1B, 0x2E, 0 } } };

	// https://lospec.com/palette-list/hollow
	constexpr std::array<SDL_Color, 4> hollow = { { { 0x0F, 0x0F, 0x1B, 0 },
												    { 0x56, 0x5A, 0x75, 0 },
												    { 0xC6, 0xB7, 0xBE, 0 },
												    { 0xFA, 0xFB, 0xF6, 0 } } };

	// https://lospec.com/palette-list/bicycle
	constexpr std::array<SDL_Color, 4> bicycle = { { { 0x16, 0x16, 0x16, 0 },
												     { 0xAB, 0x46, 0x46, 0 },
												     { 0x8F, 0x9B, 0xF6, 0 },
													 { 0xF0, 0xF0, 0xF0, 0 } } };

	// https://lospec.com/palette-list/red-is-dead
	constexpr std::array<SDL_Color, 4> redisdead = { { { 0xFF, 0xFC, 0xFE, 0 },
													   { 0xFF, 0x00, 0x15, 0 },
												       { 0x86, 0x00, 0x20, 0 },
													   { 0x11, 0x07, 0x0A, 0 } } };

	// https://lospec.com/palette-list/sea-salt-ice-cream
	constexpr std::array<SDL_Color, 4> seasalticecream = { { { 0xFF, 0xF6, 0xD3, 0 },
														     { 0x8B, 0xE6, 0xFF, 0 },
														     { 0x60, 0x8E, 0xCF, 0 },
														     { 0x33, 0x36, 0xE8, 0 } } };

	// https://lospec.com/palette-list/amber-crtgb
	constexpr std::array<SDL_Color, 4> amber = { { { 0x0D, 0x04, 0x05, 0 },
											       { 0x5E, 0x12, 0x10, 0 },
												   { 0xD3, 0x56, 0x00, 0 },
												   { 0xFE, 0xD0, 0x18, 0 } } };

	// https://lospec.com/palette-list/festive-gb
	constexpr std::array<SDL_Color, 4> festive = { { { 0x0A, 0x09, 0x2F, 0 },
											         { 0xFF, 0xFF, 0xFF, 0 },
											         { 0x00, 0x86, 0x00, 0 },
												     { 0x9F, 0x0a, 0x0a, 0 } } };

	typedef std::pair<const char*, std::array<SDL_Color, 4>> DMG_Palette;

	const std::vector<DMG_Palette> palette_vec = { std::make_pair("Grayscale", grayscale),
												   std::make_pair("Inverted grayscale", inverted_grayscale),
		                                           std::make_pair("Nostalgia", nostalgia),
	                                               std::make_pair("Ice Cream", icecream),
												   std::make_pair("Kirokaze", kirokaze),
												   std::make_pair("Space Haze", spacehaze),
												   std::make_pair("Purple Dawn", purpledawn),
												   std::make_pair("Hollow", hollow),
												   std::make_pair("Bicycle", bicycle),
												   std::make_pair("Red is Dead", redisdead),
												   std::make_pair("Sea Salt Ice Cream", seasalticecream),
												   std::make_pair("Amber", amber),
												   std::make_pair("Festive", festive) };
}