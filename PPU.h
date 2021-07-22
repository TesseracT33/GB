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

#include <chrono>

class PPU final : public Serializable, public Configurable
{
public:
	~PPU();

	Bus* bus;
	CPU* cpu;
	DMA* dma;

	void Serialize(std::ofstream& ofs) override;
	void Deserialize(std::ifstream& ifs) override;
	void SaveConfig(std::ofstream& ofs) override;
	void LoadConfig(std::ifstream& ifs) override;
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
	u8 Get_GCB_Colour(u8 index, bool BGP);
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
		u8 col_id = 0; // a value between 0 and 3
		u8 palette = 0; // on CGB a value between 0 and 7 and on DMG this only applies to sprites (0 or 1)
		u8 sprite_priority = 0; // on CGB this is the OAM index for the sprite and on DMG this doesn’t exist
		bool bg_priority = 0; // holds the value of the OBJ-to-BG Priority bit

		static const size_t size = sizeof(u8) * 3 + sizeof(bool);

		// use: BG pixels on DMG
		Pixel(u8 col_id) { this->col_id = col_id; }
		// use: BG pixels on CGB
		Pixel(u8 col_id, u8 palette) { this->col_id = col_id; this->palette = palette; }
		// use: OAM pixels on DMG
		Pixel(u8 col_id, u8 palette, bool bg_priority) {
			this->col_id = col_id; this->palette = palette; this->bg_priority = bg_priority;
		}
		// use: OAM pixels on CGB
		Pixel(u8 col_id, u8 palette, u8 sprite_priority, bool bg_priority) {
			this->col_id = col_id; this->palette = palette; this->sprite_priority = sprite_priority; this->bg_priority = bg_priority;
		}
	};

	// either DMG or CGB
	struct Sprite
	{
		u8 tile_num = 0, yPos = 0, xPos = 0, palette = 0;
		bool bg_priority = 0, yFlip = 0, xFlip = 0, ySize16 = 0;

		// CBG-specific
		bool VRAM_bank = 0;
		int OAM_index = 0;

		Sprite() = default;
		// DMG sprite
		Sprite(u8 tileNum, u8 yPos, u8 xPos, u8 palette, bool bg_priority, bool yFlip, bool xFlip, bool ySize16)
		{
			this->tile_num = tileNum;
			this->yPos = yPos;
			this->xPos = xPos;
			this->palette = palette;
			this->bg_priority = bg_priority;
			this->yFlip = yFlip;
			this->xFlip = xFlip;
			this->ySize16 = ySize16;
		}
		// CGB sprite
		Sprite(u8 tile_num, u8 yPos, u8 xPos, u8 palette, bool bg_priority, bool yFlip, bool xFlip, bool ySize16, bool VRAM_bank, int OAM_index)
		{
			this->tile_num = tile_num;
			this->yPos = yPos;
			this->xPos = xPos;
			this->palette = palette;
			this->bg_priority = bg_priority;
			this->yFlip = yFlip;
			this->xFlip = xFlip;
			this->ySize16 = ySize16;
			this->VRAM_bank = VRAM_bank;
			this->OAM_index = OAM_index;
		}
	};

	struct BackgroundTileFetcher // note: also includes window tiles
	{
		bool paused = false;
		int step = 1;
		int t_cycles_until_update = 0;
		u8 xPos = 0;

		bool window_reached = false;
		bool step3_completed_on_current_scanline = false;
		u8 leftmost_pixels_to_ignore = 0; // section: SCX at a sub-tile-layer
		int window_line_counter = -1; // set to 0 when window_reached is set to true during a scanline for the first time in a frame

		void StartOver()
		{
			step = 1;
			t_cycles_until_update = 0;
		}
		void Reset(bool reset_window_line_counter)
		{
			step = 1;
			t_cycles_until_update = 0;
			paused = window_reached = step3_completed_on_current_scanline = false;
			xPos = 0;
			if (reset_window_line_counter)
				window_line_counter = -1;
		}
	} bg_tile_fetcher;

	struct SpriteTileFetcher
	{
		int step = 1;
		int t_cycles_until_update = 0;
		bool paused = false; // if the fetcher is currently doing anything at all
		bool active = false; // if a sprite fetch has been initiated, and is currently in process
		Sprite sprite;

		void Reset()
		{
			step = 1;
			t_cycles_until_update = 0;
			paused = active = false;
		}
	} sprite_tile_fetcher;

	struct PixelShifter
	{
		u8 xPos = 0;
		int t_cycles_until_update = 0;
		bool paused = false;

		void Reset()
		{
			xPos = 0;
			t_cycles_until_update = 0;
			paused = false;
		}
	} pixel_shifter;


	static const unsigned resolution_x = 160;
	static const unsigned resolution_y = 144;
	static const unsigned colour_channels = 3;
	const unsigned default_scale = 6;
	const unsigned t_cycles_per_scanline = 456;

	bool CheckIfReachedWindow();
	inline void ClearFIFOs();
	inline void EnterVBlank();
	inline void EnterHBlank();
	inline void PrepareForNewFrame();
	inline void PrepareForNewScanline();
	inline void SetScreenMode(PPU::LCDStatus mode);
	void FetchBackgroundTile();
	void FetchSprite();
	void ResetGraphics();
	void PushPixel(SDL_Color& col);
	void Search_OAM_for_Sprites();
	void SetDisplayScale(unsigned scale);
	void ShiftPixel();
	void TryToInitiateSpriteFetch();
	void UpdatePixelFetchers();
	SDL_Color GetColourFromPixel(Pixel& pixel, TileType object_type) const;

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
	
	unsigned t_cycle_counter = 0;
	unsigned pixel_offset_x = 0;
	unsigned pixel_offset_y = 0;
	unsigned scale_temp = 0;
	unsigned pixel_offset_x_temp = 0;
	unsigned pixel_offset_y_temp = 0;
	unsigned rgb_arr_pos = 0;
	unsigned OAM_check_interval = 0;

	static const int rbg_arr_size = resolution_x * resolution_y * colour_channels;
	u8 rgb_arr[rbg_arr_size]{};

	u16 OAM_sprite_addr = Bus::Addr::OAM_START;

	const SDL_Color color_white = Themes::grayscale[0];
	const SDL_Color color_light_grey = Themes::grayscale[1];
	const SDL_Color color_dark_grey = Themes::grayscale[2];
	const SDL_Color color_black = Themes::grayscale[3];
	const SDL_Color default_palette[4] = { color_white, color_light_grey, color_dark_grey, color_black };
	SDL_Color GB_palette[4] = { color_white, color_light_grey, color_dark_grey, color_black };

	std::vector<Sprite> sprite_buffer;
	std::queue<Pixel> background_FIFO;
	std::queue<Pixel> sprite_FIFO;

	SDL_Renderer* renderer = nullptr;
	SDL_Window* window = nullptr;
	SDL_Rect rect;

	/// GBC-specific

	// Palette memory. Each array defines 8 palettes, with each palette consisting of four colours. Each colour is two bytes.
	u8 BGP_reg[64]{};
	u8 OBP_reg[64]{};

	// actual colours resulting from the above palette memory. 8 palettes of 4 colours each.
	SDL_Color BGP_GBC[32]{}; 
	SDL_Color OBP_GBC[32]{};
};