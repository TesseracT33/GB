#pragma once

#include "wx/wx.h"
#include "SDL.h"

#include <queue>
#include <vector>

#include "Bus.h"
#include "Configurable.h"
#include "CPU.h"
#include "DMA.h"
#include "System.h"
#include "Themes.h"
#include "Utils.h"

class PPU final : public Component
{
public:
	~PPU();

	Bus* bus;
	CPU* cpu;
	DMA* dma;

	void State(Serialization::BaseFunctor& functor) override;
	void Configure(Serialization::BaseFunctor& functor) override;
	void SetDefaultConfig() override;

	void CheckSTATInterrupt();
	bool CreateRenderer(const void* window_handle);
	void DisableLCD();
	void EnableLCD();
	int  GetWindowWidth() { return resolution_x * scale; };
	int  GetWindowHeight() { return resolution_y * scale; };
	void Initialize();
	void RenderGraphics();
	void Reset();
	void SetColour(int id, SDL_Color color);
	void SetLCDCFlags();
	void SetScale(unsigned scale);
	void SetWindowSize(unsigned width, unsigned height);
	void Update();

	bool LCD_enabled = false;
	bool WY_equals_LY_in_current_frame = false;
	unsigned scale = 1;

	/// GBC-specific ////////////////
	void Set_CGB_Colour(u8 index, u8 data, bool BGP);
	u8 Get_CGB_Colour(u8 index, bool BGP);
	enum OBJ_Priority_Mode { OAM_index, Coordinate } obj_priority_mode;

	// If a speed switch was started during HBlank or VBlank, the PPU can't access VRAM or OAM, resulting in only black pixels being pushed
	// If it was started in during OAM scan (mode 2), then the ppu can access VRAM, but not OAM. The result is that sprites are not rendered
	// https://gbdev.io/pandocs/CGB_Registers.html
	bool can_access_VRAM = true;
	bool can_access_OAM = true;



private:
	enum LCDStatus { HBlank, VBlank, Search_OAM_RAM, Driver_transfer };

	enum class TileType { BG, OBJ };

	// either DMG or CGB
	struct Pixel
	{
		u8 col_id; // a value between 0 and 3
		u8 palette; // on CGB a value between 0 and 7 and on DMG this only applies to sprites (0 or 1)
		u8 sprite_priority; // on CGB this is the OAM index for the sprite and on DMG this doesn’t exist
		bool bg_priority; // holds the value of the OBJ-to-BG Priority bit

		// BG  pixels on DMG: arg 1       only
		// BG  pixels on CGB: arg 1, 2    only
		// OAM pixels on DMG: arg 1, 2, 3 only
		// OAM pixels on CGB: all arguments
		Pixel(u8 _col_id = 0, u8 _palette = 0, bool _bg_priority = 0, u8 _sprite_priority = 0)
			: col_id(_col_id), palette(_palette), bg_priority(_bg_priority), sprite_priority(_sprite_priority) {}
	};

	// either DMG or CGB
	struct Sprite
	{
		u8 tile_num = 0, y_pos = 0, x_pos = 0, palette = 0;
		bool bg_priority = 0, y_flip = 0, x_flip = 0;

		// CBG-specific
		bool VRAM_bank = 0;
		int OAM_index = 0;

		Sprite() = default;

		// The two last arguments are only supplied for CGB sprites
		Sprite(u8 _tile_num, u8 _y_pos, u8 _x_pos, u8 _palette, bool _bg_priority, bool _y_flip, bool _x_flip, bool _VRAM_bank = 0, int _OAM_index = 0)
			: tile_num(_tile_num), y_pos(_y_pos), x_pos(_x_pos), palette(_palette), bg_priority(_bg_priority), y_flip(_y_flip),
			x_flip(_x_flip), VRAM_bank(_VRAM_bank), OAM_index(_OAM_index) {}
	};

	enum class TileFetchStep { TileNum, TileDataLow, TileDataHigh, PushTile };

	struct BackgroundTileFetcher // note: also includes window tiles
	{
		bool paused = false;
		TileFetchStep step = TileFetchStep::TileNum;
		int t_cycles_until_update = 0;
		u8 x_pos = 0;

		bool window_reached = false;
		bool step_3_completed_on_current_scanline = false;
		int window_line_counter = -1; // set to 0 when window_reached is set to true during a scanline for the first time in a frame

		unsigned leftmost_pixels_to_ignore;

		void StartOver()
		{
			step = TileFetchStep::TileNum;;
			t_cycles_until_update = 0;
		}
		void Reset(bool reset_window_line_counter)
		{
			step = TileFetchStep::TileNum;;
			t_cycles_until_update = 0;
			paused = window_reached = step_3_completed_on_current_scanline = false;
			x_pos = 0;
			if (reset_window_line_counter)
				window_line_counter = -1;
		}
	} bg_tile_fetcher;

	struct SpriteTileFetcher
	{
		TileFetchStep step = TileFetchStep::TileNum;
		int t_cycles_until_update = 0;
		bool paused = false; // if the fetcher is currently doing anything at all
		bool active = false; // if a sprite fetch has been initiated, and is currently in process
		Sprite sprite;

		void Reset()
		{
			step = TileFetchStep::TileNum;
			t_cycles_until_update = 0;
			paused = active = false;
		}
	} sprite_tile_fetcher;

	struct PixelShifter
	{
		u8 x_pos = 0;
		int t_cycles_until_update = 0;
		bool paused = false;

		void Reset()
		{
			x_pos = 0;
			t_cycles_until_update = 0;
			paused = false;
		}
	} pixel_shifter;

	static const unsigned resolution_x = 160;
	static const unsigned resolution_y = 144;
	static const unsigned colour_channels = 3;
	static const unsigned framebuffer_arr_size = resolution_x * resolution_y * colour_channels;

	const unsigned t_cycles_per_scanline = 456;
	const SDL_Color color_white = Themes::grayscale[0];
	const SDL_Color color_light_grey = Themes::grayscale[1];
	const SDL_Color color_dark_grey = Themes::grayscale[2];
	const SDL_Color color_black = Themes::grayscale[3];

	// config-related
	const unsigned default_scale = 6;
	const SDL_Color default_palette[4] = { color_white, color_light_grey, color_dark_grey, color_black };

	u8* LCDC;
	u8* LY;
	u8* LYC;
	u8* STAT;
	u8* SCX;
	u8* SCY;
	u8* WX;
	u8* WY;
	u8* BGP;
	u8* OBP[2]{}; // OBP0 and OBP1

	// LCDC flags
	bool BG_enabled;
	bool BG_master_priority;
	bool sprites_enabled;
	bool window_display_enabled;
	u8 sprite_height;
	u16 addr_BG_tile_map;
	u16 addr_tile_data;
	u16 addr_window_tile_map;

	bool reset_graphics_after_render = false;
	bool STAT_cond_met_LY_LYC = false;
	bool STAT_cond_met_LCD_mode = false;
	bool tile_data_signed = false;

	u8 leftmost_pixels_to_ignore;
	u8 framebuffer[framebuffer_arr_size]{};

	u16 OAM_sprite_addr = Bus::Addr::OAM_START;

	unsigned t_cycle_counter = 0;
	unsigned pixel_offset_x = 0;
	unsigned pixel_offset_y = 0;
	unsigned scale_temp = 0;
	unsigned pixel_offset_x_temp = 0;
	unsigned pixel_offset_y_temp = 0;
	unsigned framebuffer_pos = 0;
	unsigned OAM_check_interval = 0;

	SDL_Color GB_palette[4] = { color_white, color_light_grey, color_dark_grey, color_black };

	std::vector<Sprite> sprite_buffer;
	std::queue<Pixel> background_FIFO;
	std::queue<Pixel> sprite_FIFO;

	SDL_Renderer* renderer = nullptr;
	SDL_Window* window = nullptr;
	SDL_Rect rect;

	bool CheckIfReachedWindow();
	void ClearFIFOs();
	void EnterVBlank();
	void EnterHBlank();
	void PrepareForNewFrame();
	void PrepareForNewScanline();
	void SetScreenMode(PPU::LCDStatus mode);
	void FetchBackgroundTile();
	void FetchSprite();
	void ResetGraphics();
	void PushPixel(SDL_Color& col);
	void SearchOAMForSprite();
	void SetDisplayScale(unsigned scale);
	void ShiftPixel();
	void TryToInitiateSpriteFetch();
	void UpdatePixelFetchers();
	SDL_Color GetColourFromPixel(Pixel& pixel, TileType object_type) const;

	/// GBC-specific

	// Palette memory. Each array defines 8 palettes, with each palette consisting of four colours. Each colour is two bytes.
	u8 BGP_reg[64]{};
	u8 OBP_reg[64]{};

	// actual colours resulting from the above palette memory. 8 palettes of 4 colours each.
	SDL_Color BGP_GBC[32]{};
	SDL_Color OBP_GBC[32]{};
};