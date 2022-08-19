export module PPU;

import Util;

import <algorithm>;
import <array>;
import <bit>;
import <queue>;
import <utility>;
import <vector>;

namespace PPU
{
	export
	{
		struct RGB
		{
			u8 r, g, b;
		};

		using DmgPalette = std::array<RGB, 4>;

		void Initialize(bool hle_boot_rom);
		u8 ReadBCPD();
		u8 ReadBCPS();
		u8 ReadBGP();
		u8 ReadLCDC();
		u8 ReadLY();
		u8 ReadLYC();
		u8 ReadOamCpu(u16 addr); // TODO: impl
		u8 ReadOBP0();
		u8 ReadOBP1();
		u8 ReadOCPD();
		u8 ReadOCPS();
		u8 ReadOPRI();
		u8 ReadSCX();
		u8 ReadSCY();
		u8 ReadSTAT();
		u8 ReadVBK();
		u8 ReadVramCpu(u16 addr);
		u8 ReadWY();
		u8 ReadWX();
		void SetDmgPalette(DmgPalette palette);
		void StreamState(SerializationStream& stream);
		void Update();
		void WriteBCPD(u8 data);
		void WriteBCPS(u8 data);
		void WriteBGP(u8 data);
		void WriteLCDC(u8 data);
		void WriteLY(u8 data);
		void WriteLYC(u8 data);
		void WriteOamCpu(u16 addr, u8 data);
		void WriteOBP0(u8 data);
		void WriteOBP1(u8 data);
		void WriteOCPD(u8 data);
		void WriteOCPS(u8 data);
		void WriteOPRI(u8 data);
		void WriteSCX(u8 data);
		void WriteSCY(u8 data);
		void WriteSTAT(u8 data);
		void WriteVBK(u8 data);
		void WriteVramCpu(u16 addr, u8 data);
		void WriteWX(u8 data);
		void WriteWY(u8 data);
	}

	enum class LcdMode : u8 {
		HBlank, VBlank, SearchOam, DriverTransfer
	};

	enum class ObjPriorityMode {
		OamIndex, Coordinate
	} obj_priority_mode;

	enum class TileFetchStep {
		TileNum, TileDataLow, TileDataHigh, PushTile, Sleep
	};

	enum class TileType {
		BG, OBJ
	};

	constexpr std::array tile_fetch_state_machine = {
		TileFetchStep::Sleep, TileFetchStep::TileNum,
		TileFetchStep::Sleep, TileFetchStep::TileDataLow,
		TileFetchStep::Sleep, TileFetchStep::TileDataHigh,
		TileFetchStep::Sleep, TileFetchStep::PushTile
	};

	struct BackgroundTileFetcher // note: also includes window tiles
	{
		void Reset(bool reset_window_line_counter);
		void Step();

		bool fetch_tile_data_high_step_reached_this_scanline;
		bool paused;
		bool window_reached;
		int window_line_counter; // == -1 before the window has been reached during the current scanline
		uint step;

		u8 tile_x_pos; // (0-31) index of the tile on the screen (256x256)
		u8 tile_data_low, tile_data_high;
		u16 tile_data_addr;
		s16 tile_num;
	} bg_tile_fetcher;

	// either DMG or CGB
	struct FifoPixel
	{
		u8 col_id; // a value between 0 and 3
		u8 palette; // on CGB a value between 0 and 7 and on DMG this only applies to sprites (0 or 1)
		u8 sprite_priority; // on CGB this is the OAM index for the sprite and on DMG this doesn't exist
		bool bg_priority; // holds the value of the OBJ-to-BG Priority bit
		// BG  pixels on DMG: arg 1       only
		// BG  pixels on CGB: arg 1, 2    only
		// OAM pixels on DMG: arg 1, 2, 3 only
		// OAM pixels on CGB: all arguments
	};

	struct PixelShifter
	{
		void Reset();

		u8 pixel_x_pos = 0;
		bool paused = false;
	} pixel_shifter;

	// either DMG or CGB
	struct Sprite
	{
		Sprite() = default;
		// DMG constructor
		Sprite(u8 tile_num, u8 pixel_x_pos, u8 pixel_y_pos, u8 palette, bool obj_to_bg_priority, bool x_flip, bool y_flip)
			: tile_num(tile_num), pixel_x_pos(pixel_x_pos), pixel_y_pos(pixel_y_pos), palette(palette), 
			obj_to_bg_priority(obj_to_bg_priority), x_flip(x_flip), y_flip(y_flip), vram_bank(0), oam_index(0) {}
		// CGB constructor
		Sprite(u8 tile_num, u8 pixel_x_pos, u8 pixel_y_pos, u8 palette, bool obj_to_bg_priority, bool x_flip, bool y_flip, bool vram_bank, u8 oam_index)
			: tile_num(tile_num), pixel_x_pos(pixel_x_pos), pixel_y_pos(pixel_y_pos), palette(palette),
			obj_to_bg_priority(obj_to_bg_priority), x_flip(x_flip), y_flip(y_flip), vram_bank(vram_bank), oam_index(oam_index) {}
		
		u8 tile_num, pixel_x_pos, pixel_y_pos, palette;
		bool obj_to_bg_priority, x_flip, y_flip;
		bool vram_bank; /* cgb */
		u8 oam_index; /* cgb */
	};

	struct SpriteFetcher
	{
		void Reset();
		void Step();

		bool paused; // == true even if it is looking for sprites in OAM
		uint step;
		Sprite sprite; // properties of sprite being fetched

		u8 tile_data_low, tile_data_high, tile_num;
		u16 tile_addr;
	} sprite_fetcher;

	template<TileType>
	RGB GetColourFromPixel(FifoPixel pixel);

	void AttemptSpriteFetch();
	RGB CgbColorDataToRGB(u16 color_data);
	void CheckIfWindowReached();
	void CheckStatInterrupt();
	void ClearFifos();
	void EnterHBlank();
	void EnterVBlank();
	void PrepareForNewFrame();
	void PrepareForNewScanline();
	void PushPixel(RGB rgb);
	void ReadLcdcFlags();
	u8 ReadVRAM(u16 addr);
	void ScanOam();
	void SetLcdMode(LcdMode mode);
	void ShiftPixel();
	void UpdatePixelFetchers();

	constexpr uint resolution_x = 160;
	constexpr uint resolution_y = 144;
	constexpr uint num_colour_channels = 3;
	constexpr uint framebuffer_size = resolution_x * resolution_y * num_colour_channels;
	constexpr uint m_cycles_per_scanline = 144;
	constexpr uint sprite_buffer_capacity = 10;
	constexpr uint vram_bank_size = 0x2000;

	bool stat_interrupt_cond = false;
	bool tile_nums_are_signed = false;
	bool wy_equalled_ly_this_frame;

	uint current_vram_bank;
	uint framebuffer_pos;
	uint leftmost_bg_pixels_to_discard;
	uint m_cycle_counter;
	uint oam_addr;
	uint sprite_height;

	/* registers */
	u8 bgp;
	u8 ly;
	u8 lyc;
	u8 scx;
	u8 scy;
	u8 wx;
	u8 wy;

	/* cgb */
	u8 bcps;
	u8 ocps;

	struct
	{
		u8 bg_enable : 1;
		u8 obj_enable : 1;
		u8 obj_size : 1;
		u8 bg_tile_map_area : 1;
		u8 bg_tile_data_area : 1;
		u8 window_enable : 1;
		u8 window_tile_map_area : 1;
		u8 lcd_enable : 1;
	} lcdc;

	struct
	{
		u8 lcd_mode : 2;
		u8 lyc_equals_ly : 1;
		u8 hblank_stat_int_enable : 1;
		u8 vblank_stat_int_enable : 1;
		u8 oam_stat_int_enable : 1;
		u8 lyc_equals_ly_stat_int_enable : 1;
		u8 : 1;
	} stat;

	// LCDC-flag-related
	u16 bg_tile_map_base_addr;
	u16 tile_data_base_addr;
	u16 window_tile_map_base_addr;

	DmgPalette dmg_palette;

	std::array<u8, framebuffer_size> framebuffer;
	std::array<u8, 2> obp_dmg; // OBP0 and OBP1
	std::array<u8, 0x4000> vram;
	std::array<u8, 0xA0> oam;
	// Palette memory. Each array defines 8 palettes, with each palette consisting of four colours. Each colour is two bytes.
	std::array<u8, 0x40> bg_palette_ram; /* cgb */
	std::array<u8, 0x40> obj_palette_ram; /* cgb */
	// actual colours resulting from the above palette memory. 8 palettes of 4 colours each.
	std::array<RGB, 0x20> cgb_bg_palette; /* cgb */
	std::array<RGB, 0x20> cgb_obj_palette; /* cgb */

	std::queue<FifoPixel> bg_pixel_fifo;
	std::queue<FifoPixel> sprite_pixel_fifo;

	std::vector<Sprite> sprite_buffer;
}