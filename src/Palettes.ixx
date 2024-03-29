export module PPU.Palettes;

import PPU;
import Util;

import <array>;

export namespace PPU::Palettes
{
	// https://lospec.com/palette-list/2-bit-grayscale
	constexpr DmgPalette grayscale = { { { 0xFF, 0xFF, 0xFF },
									{ 0xB6, 0xB6, 0xB6 },
									{ 0x67, 0x67, 0x67 },
									{ 0x00, 0x00, 0x00 } } };

	constexpr DmgPalette inverted_grayscale = { { { 0x00, 0x00, 0x00 },
																{ 0x67, 0x67, 0x67 },
																{ 0xB6, 0xB6, 0xB6 },
																{ 0xFF, 0xFF, 0xFF } } };

	// https://lospec.com/palette-list/nostalgia
	constexpr DmgPalette nostalgia = { { { 0xD0, 0xD0, 0x58 },
													   { 0xA0, 0xA8, 0x40 },
													   { 0x70, 0x80, 0x28 },
													   { 0x40, 0x50, 0x10 } } };

	// https://lospec.com/palette-list/ice-cream-gb
	constexpr DmgPalette icecream = { { { 0x7C, 0x3F, 0x58 },
													  { 0xEB, 0x6B, 0x6F },
													  { 0xF9, 0xA8, 0x75 },
													  { 0xFF, 0xF6, 0xD3 } } };

	// https://lospec.com/palette-list/kirokaze-gameboy
	constexpr DmgPalette kirokaze = { { { 0x33, 0x2C, 0x50 },
													  { 0x46, 0x87, 0x8F },
													  { 0x94, 0xE3, 0x44 },
													  { 0xE2, 0xF3, 0xE4 } } };

	// https://lospec.com/palette-list/spacehaze
	constexpr DmgPalette spacehaze = { { { 0xF8, 0xE3, 0xC4 },
													   { 0xCC, 0x34, 0x95 },
													   { 0x6B, 0x1F, 0xB1 },
													   { 0x0B, 0x06, 0x30 } } };

	// https://lospec.com/palette-list/purpledawn
	constexpr DmgPalette purpledawn = { { { 0xEE, 0xFD, 0xED },
														{ 0x9A, 0x7B, 0xBC },
														{ 0x2D, 0x75, 0x7E },
														{ 0x00, 0x1B, 0x2E } } };

	// https://lospec.com/palette-list/hollow
	constexpr DmgPalette hollow = { { { 0x0F, 0x0F, 0x1B },
													{ 0x56, 0x5A, 0x75 },
													{ 0xC6, 0xB7, 0xBE },
													{ 0xFA, 0xFB, 0xF6 } } };

	// https://lospec.com/palette-list/bicycle
	constexpr DmgPalette bicycle = { { { 0x16, 0x16, 0x16 },
													 { 0xAB, 0x46, 0x46 },
													 { 0x8F, 0x9B, 0xF6 },
													 { 0xF0, 0xF0, 0xF0 } } };

	// https://lospec.com/palette-list/red-is-dead
	constexpr DmgPalette redisdead = { { { 0xFF, 0xFC, 0xFE },
													   { 0xFF, 0x00, 0x15 },
													   { 0x86, 0x00, 0x20 },
													   { 0x11, 0x07, 0x0A } } };

	// https://lospec.com/palette-list/sea-salt-ice-cream
	constexpr DmgPalette seasalticecream = { { { 0xFF, 0xF6, 0xD3 },
															 { 0x8B, 0xE6, 0xFF },
															 { 0x60, 0x8E, 0xCF },
															 { 0x33, 0x36, 0xE8 } } };

	// https://lospec.com/palette-list/amber-crtgb
	constexpr DmgPalette amber = { { { 0x0D, 0x04, 0x05 },
												   { 0x5E, 0x12, 0x10 },
												   { 0xD3, 0x56, 0x00 },
												   { 0xFE, 0xD0, 0x18 } } };

	// https://lospec.com/palette-list/festive-gb
	constexpr DmgPalette festive = { { { 0x0A, 0x09, 0x2F },
													 { 0xFF, 0xFF, 0xFF },
													 { 0x00, 0x86, 0x00 },
													 { 0x9F, 0x0a, 0x0a } } };
}