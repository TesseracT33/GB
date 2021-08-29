#include "PPU.h"

using namespace Util;

PPU::~PPU()
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
}


bool PPU::CreateRenderer(const void* window_handle)
{
	this->window = SDL_CreateWindowFrom(window_handle);
	if (window == NULL)
	{
		wxMessageBox("Could not create the SDL window!");
		return false;
	}

	this->renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
	if (renderer == NULL)
	{
		wxMessageBox("Renderer could not be created!");
		SDL_DestroyWindow(window);
		window = nullptr;
		return false;
	}

	return true;
}


void PPU::Initialize()
{
	LCDC = bus->ReadIOPointer(Bus::Addr::LCDC);
	LY = bus->ReadIOPointer(Bus::Addr::LY);
	LYC = bus->ReadIOPointer(Bus::Addr::LYC);
	STAT = bus->ReadIOPointer(Bus::Addr::STAT);
	SCX = bus->ReadIOPointer(Bus::Addr::SCX);
	SCY = bus->ReadIOPointer(Bus::Addr::SCY);
	WX = bus->ReadIOPointer(Bus::Addr::WX);
	WY = bus->ReadIOPointer(Bus::Addr::WY);
	BGP = bus->ReadIOPointer(Bus::Addr::BGP);
	OBP[0] = bus->ReadIOPointer(Bus::Addr::OBP0);
	OBP[1] = bus->ReadIOPointer(Bus::Addr::OBP1);
}


// note: this function should be called after bus.Reset(), so that SetLCDCFlags() works
void PPU::Reset()
{
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);

	sprite_buffer.clear();
	ClearFIFOs();
	SetLCDCFlags();

	WY_equals_LY_in_current_frame = false;
	t_cycle_counter = 0;

	rect.x = pixel_offset_x;
	rect.y = pixel_offset_y;
	reset_graphics_after_render = false;

	OAM_sprite_addr = Bus::Addr::OAM_START;

	bg_tile_fetcher.Reset(true);
	sprite_tile_fetcher.Reset();
	pixel_shifter.Reset();
	STAT_cond_met_LY_LYC = STAT_cond_met_LCD_mode = false;

	can_access_VRAM = can_access_OAM = true;
	obj_priority_mode = OBJ_Priority_Mode::OAM_index;

	CheckSTATInterrupt();
}


void PPU::RenderGraphics()
{
	SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(framebuffer, resolution_x, resolution_y,
		8 * colour_channels, resolution_x * colour_channels, 0x0000FF, 0x00FF00, 0xFF0000, 0);

	SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

	rect.w = resolution_x * scale;
	rect.h = resolution_y * scale;
	rect.x = pixel_offset_x;
	rect.y = pixel_offset_y;
	SDL_RenderCopy(renderer, texture, NULL, &rect);

	SDL_RenderPresent(renderer);

	if (reset_graphics_after_render)
		ResetGraphics();

	SDL_FreeSurface(surface);
	SDL_DestroyTexture(texture);
}


void PPU::Update()
{
	if (!LCD_enabled) return;

	// 456 t-cycles per scanline in single-speed mode. for double-speed, all numbers should be doubled

	// the following t_cycle_counter comparisons can only be true at the beginning of an m-cycle, so this can exist outside of the for-loop
	if (*LY < 144)
	{
		if (t_cycle_counter == 0)
		{
			if (*LY == 0)
				PrepareForNewFrame();
			PrepareForNewScanline();
			SetScreenMode(LCDStatus::Search_OAM_RAM);
		}
		else if (t_cycle_counter == 80 * System::speed_mode)
		{
			SetScreenMode(LCDStatus::Driver_transfer);
			ClearFIFOs();
		}
	}
	else if (*LY == 144 && t_cycle_counter == 4)
	{
		EnterVBlank();
	}

	for (int i = 0; i < 4; i++) // Update() is called each m-cycle, but ppu is updated each t-cycle
	{
		if (*LY < 144)
		{
			if (t_cycle_counter < 80 * System::speed_mode)
			{
				if (t_cycle_counter % OAM_check_interval == 0 && sprite_buffer.size() < 10 && can_access_OAM)
					Search_OAM_for_Sprites();
			}
			else if (pixel_shifter.x_pos < resolution_x)
			{
				UpdatePixelFetchers();
			}
		}

		t_cycle_counter++;
	}

	// can only happen at the end of an m-cycle, so its not needed in the above for-loop
	if (t_cycle_counter == t_cycles_per_scanline * System::speed_mode)
	{
		t_cycle_counter = 0;
		*LY = (*LY + 1) % 154;

		WY_equals_LY_in_current_frame |= *WY == *LY;
		CheckSTATInterrupt();
	}
}


void PPU::EnterVBlank()
{
	bg_tile_fetcher.window_line_counter = -1;
	SetScreenMode(LCDStatus::VBlank);
	cpu->RequestInterrupt(CPU::Interrupt::VBlank);
	RenderGraphics();
}


void PPU::EnterHBlank()
{
	SetScreenMode(LCDStatus::HBlank);
	if (dma->HDMA_transfer_active)
		dma->Initiate_HDMA_Update();
	bg_tile_fetcher.paused = sprite_tile_fetcher.paused = pixel_shifter.paused = true;
}


void PPU::SetScreenMode(PPU::LCDStatus mode)
{
	*STAT = *STAT & ~3 | mode;
	CheckSTATInterrupt();
}


void PPU::DisableLCD()
{
	LCD_enabled = false;
	ClearBit(LCDC, 7);
	STAT_cond_met_LY_LYC = STAT_cond_met_LCD_mode = false;
	SetScreenMode(LCDStatus::HBlank);
	*LY = t_cycle_counter = 0;

	// If the HDMA transfer started when the lcd was on, then when the lcd is disabled, one block is copied after the switch
	if (dma->HDMA_transfer_active && !dma->HDMA_transfer_started_when_lcd_off)
		dma->Initiate_HDMA_Update();
}


void PPU::EnableLCD()
{
	LCD_enabled = true;
	SetBit(LCDC, 7);
	CheckSTATInterrupt();
}


void PPU::CheckSTATInterrupt()
{
	if (!LCD_enabled) return;

	bool STAT_LY_EQUALS_LYC = *LY == *LYC;
	STAT_LY_EQUALS_LYC ? SetBit(STAT, 2) : ClearBit(STAT, 2);
	bool STAT_ENABLE_LYC_COMPARE = CheckBit(STAT, 6);
	bool prev_STAT_cond_met_LY_LYC = STAT_cond_met_LY_LYC;
	STAT_cond_met_LY_LYC = STAT_LY_EQUALS_LYC && STAT_ENABLE_LYC_COMPARE;

	u8 LCD_mode = *STAT & 3;
	bool STAT_ENABLE_HBL = CheckBit(STAT, 3);
	bool STAT_ENABLE_VBL = CheckBit(STAT, 4);
	bool STAT_ENABLE_OAM = CheckBit(STAT, 5);
	bool prev_STAT_cond_met_LCD_mode = STAT_cond_met_LCD_mode;
	STAT_cond_met_LCD_mode = LCD_mode == 0 && STAT_ENABLE_HBL ||
		LCD_mode == 1 && (STAT_ENABLE_VBL || STAT_ENABLE_OAM) || LCD_mode == 2 && STAT_ENABLE_OAM;

	if (!prev_STAT_cond_met_LY_LYC && !prev_STAT_cond_met_LCD_mode &&
		(STAT_cond_met_LY_LYC || STAT_cond_met_LCD_mode))
	{
		cpu->RequestInterrupt(CPU::Interrupt::STAT);
	}
}


void PPU::ResetGraphics()
{
	scale = scale_temp;
	pixel_offset_x = pixel_offset_x_temp;
	pixel_offset_y = pixel_offset_y_temp;

	SDL_RenderClear(renderer);
	reset_graphics_after_render = false;
}


void PPU::SetLCDCFlags()
{
	BG_enabled = BG_master_priority = CheckBit(LCDC, 0);
	sprites_enabled = CheckBit(LCDC, 1);
	if (!sprites_enabled)
	{
		int a = 3;
	}
	else
	{
		int a = 3;
	}
	sprite_height = CheckBit(LCDC, 2) ? 16 : 8;
	addr_BG_tile_map = CheckBit(LCDC, 3) ? 0x9C00 : 0x9800;
	addr_tile_data = CheckBit(LCDC, 4) ? 0x8000 : 0x9000;
	window_display_enabled = CheckBit(LCDC, 5);
	addr_window_tile_map = CheckBit(LCDC, 6) ? 0x9C00 : 0x9800;
	LCD_enabled = CheckBit(LCDC, 7);

	tile_data_signed = addr_tile_data == 0x9000;
}


void PPU::SetDisplayScale(unsigned scale)
{
	if (scale > 0)
	{
		this->scale_temp = scale;
		reset_graphics_after_render = true;
	}
}


SDL_Color PPU::GetColourFromPixel(Pixel& pixel, TileType object_type) const
{
	if (System::mode != System::Mode::CGB)
	{
		// which bits of the colour palette does the colour id map to?
		u8 shift = pixel.col_id * 2;
		u8 col_index;
		if (object_type == TileType::BG) col_index = (*BGP & 3 << shift) >> shift;
		else                             col_index = (*OBP[pixel.palette & 1] & 3 << shift) >> shift;
		return GB_palette[col_index];
	}
	else
	{
		if (object_type == TileType::BG) return BGP_GBC[4 * pixel.palette + pixel.col_id];
		else                             return OBP_GBC[4 * pixel.palette + pixel.col_id];
	}
}


void PPU::FetchBackgroundTile()
{
	static u8 tile_data_low, tile_data_high;
	static u16 tile_addr;
	static s16 tile_num;

	switch (bg_tile_fetcher.step)
	{

	case TileFetchStep::TileNum:
	{
		u16 tile_num_addr;
		u8 tile_col, tile_row;
		if (bg_tile_fetcher.window_reached)
		{
			tile_num_addr = addr_window_tile_map;
			tile_col = bg_tile_fetcher.x_pos;
			tile_row = bg_tile_fetcher.window_line_counter / 8;
		}
		else
		{
			tile_num_addr = addr_BG_tile_map;
			tile_col = (bg_tile_fetcher.x_pos + *SCX / 8) & 0x1F;
			tile_row = ((*LY + *SCY) & 0xFF) / 8;
		}
		tile_num_addr += (32 * tile_row + tile_col) & 0x3FF;

		tile_num = tile_data_signed ? (s8)bus->Read(tile_num_addr, true) : bus->Read(tile_num_addr, true);

		bg_tile_fetcher.step = TileFetchStep::TileDataLow;
		bg_tile_fetcher.t_cycles_until_update = 1;
		break;
	}

	case TileFetchStep::TileDataLow:
	{
		tile_addr = addr_tile_data + tile_num * 16;
		if (bg_tile_fetcher.window_reached)
			tile_addr += 2 * (bg_tile_fetcher.window_line_counter % 8);
		else
			tile_addr += 2 * ((*LY + *SCY) % 8);

		tile_data_low = bus->Read(tile_addr, true);

		bg_tile_fetcher.step = TileFetchStep::TileDataHigh;
		bg_tile_fetcher.t_cycles_until_update = 1;
		break;
	}

	case TileFetchStep::TileDataHigh:
	{
		tile_data_high = bus->Read(tile_addr + 1, true);
		bg_tile_fetcher.t_cycles_until_update = 1;
		// The first time the background fetcher completes step 3 on a scanline, the status is fully reset and operation restarts at Step 1.
		if (bg_tile_fetcher.step_3_completed_on_current_scanline)
			bg_tile_fetcher.step = TileFetchStep::PushTile;
		else
		{
			bg_tile_fetcher.step_3_completed_on_current_scanline = true;
			bg_tile_fetcher.step = TileFetchStep::TileNum;
		}
		break;
	}

	case TileFetchStep::PushTile:
	{
		// Step 4 is only executed if the background FIFO is fully empty.
		// If it is not, this step repeats every cycle until it succeeds
		if (!background_FIFO.empty()) return;

		// Pixel 0 in the tile is bit 7 of tile data. Pixel 1 is bit 6 etc..
		for (int i = 7; i >= 0; i--)
		{
			// combine data 2 and data 1 to get the colour id for this pixel in the tile
			u8 col_id = CheckBit(tile_data_high, i) << 1 | CheckBit(tile_data_low, i);
			background_FIFO.emplace(col_id);
		}

		bg_tile_fetcher.x_pos++;
		bg_tile_fetcher.x_pos &= 0x1F;

		bg_tile_fetcher.step = TileFetchStep::TileNum;
		bg_tile_fetcher.t_cycles_until_update = 1;
		// since pushing all pixels take 2 t-cycles, but this op takes 1 cycle in the emu, the ppu must wait one extra cycle before fetching pixels for background FIFO
		if (pixel_shifter.t_cycles_until_update == 0)
			pixel_shifter.t_cycles_until_update = 1;
		break;
	}
	}
}


void PPU::FetchSprite()
{
	static u8 tile_data_low, tile_data_high, tile_num;
	static u16 tile_addr;

	switch (sprite_tile_fetcher.step)
	{
	case TileFetchStep::TileNum:
	{
		if (*LY == 0x68 - 16 && pixel_shifter.x_pos == 0x40 - 8)
		{
			int a = 3;
		}
		if (*LY == 0x69 - 16 && pixel_shifter.x_pos == 0x40 - 8)
		{
			int a = 3;
		}

		if (!sprite_tile_fetcher.sprite.y_size_16)
		{
			tile_num = sprite_tile_fetcher.sprite.tile_num;
		}
		else
		{
			// if 8x16 sprites are used, find if low or high tile is used for the current scanline
			// To calculate the tile number of the top tile, the tile number in the OAM entry is used and the least significant bit is set to 0. 
			// The tile number of the bottom tile is calculated by setting the least significant bit to 1.
			if (*LY < sprite_tile_fetcher.sprite.y_pos - 16 + 8)
			{
				if (!sprite_tile_fetcher.sprite.y_flip) tile_num = sprite_tile_fetcher.sprite.tile_num & 0xFE;
				else                                    tile_num = sprite_tile_fetcher.sprite.tile_num | 1;
			}
			else
			{
				if (!sprite_tile_fetcher.sprite.y_flip) tile_num = sprite_tile_fetcher.sprite.tile_num | 1;
				else                                    tile_num = sprite_tile_fetcher.sprite.tile_num & 0xFE;
			}
		}
		sprite_tile_fetcher.step = TileFetchStep::TileDataLow;
		sprite_tile_fetcher.t_cycles_until_update = 1;
		break;
	}

	case TileFetchStep::TileDataLow:
	{
		if (*LY == 0x68 - 16 && pixel_shifter.x_pos == 0x40 - 8)
		{
			int a = 3;
		}
		if (*LY == 0x69 - 16 && pixel_shifter.x_pos == 0x40 - 8)
		{
			int a = 3;
		}

		tile_addr = 0x8000 + 16 * tile_num;
		if (!sprite_tile_fetcher.sprite.y_flip)
			tile_addr += 2 * ((*LY - sprite_tile_fetcher.sprite.y_pos + 16) % 8);
		else
			tile_addr += 2 * (7 - (*LY - sprite_tile_fetcher.sprite.y_pos + 16) % 8);

		tile_data_low = bus->Read(tile_addr, true);

		sprite_tile_fetcher.step = TileFetchStep::TileDataHigh;
		sprite_tile_fetcher.t_cycles_until_update = 1;
		break;
	}

	case TileFetchStep::TileDataHigh:
	{
		if (*LY == 0x68 - 16 && pixel_shifter.x_pos >= 0x40 - 8)
		{
			int a = 3;
		}
		if (*LY == 0x69 - 16 && pixel_shifter.x_pos == 0x40 - 8)
		{
			int a = 3;
		}

		tile_data_high = bus->Read(tile_addr + 1, true);
		sprite_tile_fetcher.step = TileFetchStep::PushTile;
		sprite_tile_fetcher.t_cycles_until_update = 1;
		break;
	}

	case TileFetchStep::PushTile:
	{
		// Only pixels which are actually visible on the screen are loaded into the FIFO
		// e.g., if pixel shifter xPos = 0, then a sprite with an X-value of 8 would have all 8 pixels loaded, 
		// while a sprite with an X-value of 7 would only have the rightmost 7 pixels loaded
		// No need to worry about pixels going off the right side of the screen, since the PPU wouldn't push these pixels anyways
		int pixels_to_ignore_left = std::max(0, 8 - sprite_tile_fetcher.sprite.x_pos + pixel_shifter.x_pos);

		if (obj_priority_mode == OBJ_Priority_Mode::OAM_index)
		{
			// todo
		}

		// Pixel 0 in the tile is bit 7 of tile data. Pixel 1 is bit 6 etc..
		// insert as many pixels into FIFO as there are free slots
		// if FIFO already contains n no. of pixels, insert only the last 8 - n pixels from the current sprite
		if (!sprite_tile_fetcher.sprite.x_flip)
		{
			for (int i = 7 - std::max((int)sprite_FIFO.size(), pixels_to_ignore_left); i >= 0; i--)
			{
				u8 col_id = CheckBit(tile_data_high, i) << 1 | CheckBit(tile_data_low, i);
				sprite_FIFO.emplace(col_id, sprite_tile_fetcher.sprite.palette, sprite_tile_fetcher.sprite.bg_priority);
			}
		}
		else
		{
			for (int i = std::max((int)sprite_FIFO.size(), pixels_to_ignore_left); i <= 7; i++)
			{
				u8 col_id = CheckBit(tile_data_high, i) << 1 | CheckBit(tile_data_low, i);
				sprite_FIFO.emplace(col_id, sprite_tile_fetcher.sprite.palette, sprite_tile_fetcher.sprite.bg_priority);
			}
		}

		sprite_tile_fetcher.step = TileFetchStep::TileNum;
		sprite_tile_fetcher.t_cycles_until_update = 1;
		bg_tile_fetcher.paused = pixel_shifter.paused = sprite_tile_fetcher.active = false;
		// 1 cycle delay since step 4 takes 2 cycles, and all other components are paused at this time
		bg_tile_fetcher.t_cycles_until_update = pixel_shifter.t_cycles_until_update += 1;
		break;
	}
	}
}

void PPU::TryToInitiateSpriteFetch()
{
	const size_t no_of_sprites = sprite_buffer.size();
	if (no_of_sprites == 0) return;

	// some code duplication below, but this is to reduce the amount of branching
	if (System::mode == System::Mode::DMG || obj_priority_mode == OBJ_Priority_Mode::Coordinate)
	{
		for (size_t i = 0; i < no_of_sprites; i++)
		{
			if (sprite_buffer[i].x_pos - 8 <= pixel_shifter.x_pos)
			{
				sprite_tile_fetcher.sprite = sprite_buffer[i];
				sprite_buffer.erase(sprite_buffer.begin() + i);
				bg_tile_fetcher.StartOver();
				bg_tile_fetcher.paused = pixel_shifter.paused = true;
				sprite_tile_fetcher.active = true;

				FetchSprite();
				return;
			}
		}
	}
	//else
	//{
	//	bool sprite_found = false;
	//	int sprite_found_index;
	//	for (size_t i = 0; i < noOfSprites; i++)
	//	{
	//		if (!sprite_found && sprite_buffer[i].xPos - 8 <= pixelShifter.xPos)
	//		{
	//			spriteFetcher.sprite = sprite_buffer[i];
	//			sprite_buffer.erase(sprite_buffer.begin() + i);
	//			backgroundPixelFetcher.StartOver();
	//			backgroundPixelFetcher.paused = pixelShifter.paused = true;
	//			spriteFetcher.active = true;

	//			FetchSprite();
	//			sprite_found = true;
	//			sprite_found_index = i;
	//		}
	//		else if (sprite_found && sprite_buffer[i].xPos - 8 <= pixelShifter.xPos + 7)
	//		{
	//			if (sprite_buffer[i].OAM_index < sprite_buffer[sprite_found_index].OAM_index)
	//		}
	//	}
	//	sprite_buffer.erase(sprite_buffer.begin() + sprite_found_index);
	//}
}


void PPU::ShiftPixel()
{
	if (background_FIFO.empty()) return;

	SDL_Color col;

	Pixel& bg_pixel = background_FIFO.front();

	if (!BG_enabled && System::mode != System::Mode::CGB)
		bg_pixel.col_id = 0;

	if (sprite_FIFO.empty())
	{
		col = GetColourFromPixel(bg_pixel, TileType::BG);
	}
	else
	{
		Pixel& sprite_pixel = sprite_FIFO.front();

		if (*LY == 0x68 - 16 && pixel_shifter.x_pos == 0x40 - 8)
		{
			int a = 3;
		}
		if (*LY == 0x69 - 16 && pixel_shifter.x_pos == 0x40 - 8)
		{
			int a = 3;
		}

		// During a speed switch, if the switch was started during HBlank or VBlank, then the ppu cannot access vram during this time.
		// The result is "black pixels" being pushed (https://gbdev.io/pandocs/CGB_Registers.html). 
		// todo: emulate this better? Currently, a black pixel is pushed only if the speed switch is active during the same t-cycle as the ppu shifts pixels
		if (!can_access_VRAM)
			col = color_black;
		else if (System::mode == System::Mode::CGB && BG_master_priority || !BG_enabled && System::mode != System::Mode::CGB)
			col = GetColourFromPixel(sprite_pixel, TileType::OBJ);
		else if (!sprites_enabled || sprite_pixel.col_id == 0 || sprite_pixel.bg_priority && bg_pixel.col_id != 0)
			col = GetColourFromPixel(bg_pixel, TileType::BG);
		else
			col = GetColourFromPixel(sprite_pixel, TileType::OBJ);

		sprite_FIFO.pop();
	}
	background_FIFO.pop();

	// SCX % 8 pixels are discarded at the start of each scanline rather than being pushed to the LCD
	if (leftmost_pixels_to_ignore > 0)
	{
		leftmost_pixels_to_ignore--;
		return;
	}

	PushPixel(col);
}


void PPU::PushPixel(SDL_Color& col)
{
	framebuffer[framebuffer_pos] = col.r;
	framebuffer[framebuffer_pos + 1] = col.g;
	framebuffer[framebuffer_pos + 2] = col.b;
	framebuffer_pos += 3;

	if (++pixel_shifter.x_pos == resolution_x)
		EnterHBlank();

	// After each pixel is shifted out, the PPU checks if it has reached the window.
	// If it has, the background fetcher state is fully reset to step 1
	if (!bg_tile_fetcher.window_reached)
		CheckIfReachedWindow();
}


// 2 t-cycles per sprite and function call
void PPU::Search_OAM_for_Sprites()
{
	u8 y_pos = bus->Read(OAM_sprite_addr, true);

	if (*LY >= y_pos - 16 && *LY < y_pos - 16 + sprite_height)
	{
		u8 x_pos = bus->Read(OAM_sprite_addr + 1, true);
		if (x_pos > 0)
		{
			u8 tile_num = bus->Read(OAM_sprite_addr + 2, true);
			u8 flags = bus->Read(OAM_sprite_addr + 3, true);

			bool x_flip = CheckBit(flags, 5);
			bool y_flip = CheckBit(flags, 6);
			bool bg_priority = CheckBit(flags, 7);
			bool y_size_16 = sprite_height == 16;

			if (System::mode != System::Mode::CGB)
			{
				u8 palette = CheckBit(flags, 4);
				sprite_buffer.emplace_back(tile_num, y_pos, x_pos, palette, bg_priority, y_flip, x_flip, y_size_16);
			}
			else
			{
				u8 palette = flags & 7;
				bool VRAM_bank = CheckBit(flags, 3); // todo: how does this dictate which bank is used? isn't this set by writing to VBK?
				int OAM_index = 0.25 * (OAM_sprite_addr - Bus::Addr::OAM_START);
				sprite_buffer.emplace_back(tile_num, y_pos, x_pos, palette, bg_priority, y_flip, x_flip, y_size_16, VRAM_bank, OAM_index);
			}
		}
	}
	OAM_sprite_addr += 4;
}


void PPU::ClearFIFOs()
{
	while (!background_FIFO.empty()) background_FIFO.pop();
	while (!sprite_FIFO.empty()) sprite_FIFO.pop();
}


void PPU::UpdatePixelFetchers()
{
	if (!bg_tile_fetcher.paused)
	{
		if (bg_tile_fetcher.t_cycles_until_update == 0)
			FetchBackgroundTile();
		else
			bg_tile_fetcher.t_cycles_until_update--;
	}

	if (!sprite_tile_fetcher.paused)
	{
		if (sprite_tile_fetcher.active)
		{
			if (sprite_tile_fetcher.t_cycles_until_update == 0)
				FetchSprite();
			else
				sprite_tile_fetcher.t_cycles_until_update--;
		}
		else
		{
			TryToInitiateSpriteFetch();
		}
	}

	if (!pixel_shifter.paused)
	{
		if (pixel_shifter.t_cycles_until_update == 0)
			ShiftPixel();
		else
			pixel_shifter.t_cycles_until_update--;
	}
}


bool PPU::CheckIfReachedWindow()
{
	// After each pixel is shifted out, the PPU checks if it has reached the window.
	// If all of the below conditions apply, Window Fetching starts.
	if (window_display_enabled && BG_enabled && WY_equals_LY_in_current_frame && pixel_shifter.x_pos >= *WX - 7)
	{
		bg_tile_fetcher.StartOver();
		bg_tile_fetcher.x_pos = 0;
		bg_tile_fetcher.window_reached = true;
		while (!background_FIFO.empty()) background_FIFO.pop();

		bg_tile_fetcher.window_line_counter++;

		return true;
	}
	return false;
}


void PPU::PrepareForNewScanline()
{
	bg_tile_fetcher.Reset(false);
	sprite_tile_fetcher.Reset();
	pixel_shifter.Reset();
	sprite_buffer.clear();
	OAM_sprite_addr = static_cast<u16>(Bus::Addr::OAM_START);

	// SCX % 8 pixels are discarded at the start of each scanline rather than being pushed to the LCD
	leftmost_pixels_to_ignore = *SCX % 8;
}


void PPU::PrepareForNewFrame()
{
	WY_equals_LY_in_current_frame = *LY == *WY;
	framebuffer_pos = 0;
	OAM_check_interval = 2 * System::speed_mode; // in single speed mode, OAM is checked every 2 t-cycles, 4 in double speed
}


void PPU::SetColour(int id, SDL_Color color)
{
	assert(id >= 0 && id <= 3);
	GB_palette[id] = color;
}


void PPU::Set_CGB_Colour(u8 index, u8 data, bool BGP)
{
	auto GetSDLColFromPaletteData = [&](u16 data)
	{
		u8 red = data & 0x1F;
		u8 green = (data >> 5) & 0x1F;
		u8 blue = (data >> 10) & 0x1F;

		// convert each 5-bit channel to 8-bit channels (https://github.com/mattcurrie/dmg-acid2)
		red = (red << 3) | (red >> 2);
		green = (green << 3) | (green >> 2);
		blue = (blue << 3) | (blue >> 2);

		return SDL_Color{ red, green, blue, 0 };
	};

	u16 colour_data;
	if (BGP)
	{
		BGP_reg[index] = data;
		if (index % 2) colour_data = BGP_reg[index - 1] | BGP_reg[index] << 8;
		else           colour_data = BGP_reg[index] | BGP_reg[index + 1] << 8;
		BGP_GBC[index / 2] = GetSDLColFromPaletteData(colour_data);
	}
	else // OBP
	{
		OBP_reg[index] = data;
		if (index % 2) colour_data = OBP_reg[index - 1] | OBP_reg[index] << 8;
		else           colour_data = OBP_reg[index] | OBP_reg[index + 1] << 8;
		OBP_GBC[index / 2] = GetSDLColFromPaletteData(colour_data);
	}
}


u8 PPU::Get_CGB_Colour(u8 index, bool BGP)
{
	if (BGP) return BGP_reg[index];
	return OBP_reg[index];
}


void PPU::SetScale(unsigned scale)
{
	if (scale > 0)
	{
		this->scale_temp = scale;
		pixel_offset_x_temp = 0;
		pixel_offset_y_temp = 0;
		reset_graphics_after_render = true;
	}
}


void PPU::SetWindowSize(unsigned width, unsigned height)
{
	if (width > 0 && height > 0)
	{
		scale_temp = std::min(width / resolution_x, height / resolution_y);
		pixel_offset_x_temp = 0.5 * (width - scale_temp * resolution_x);
		pixel_offset_y_temp = 0.5 * (height - scale_temp * resolution_y);
		reset_graphics_after_render = true;
	}
}


void PPU::State(Serialization::BaseFunctor& functor)
{
	Serialization::STD_vector(functor, sprite_buffer);
	Serialization::STD_queue(functor, background_FIFO);
	Serialization::STD_queue(functor, sprite_FIFO);

	functor.fun(&bg_tile_fetcher, sizeof(BackgroundTileFetcher));
	functor.fun(&sprite_tile_fetcher, sizeof(SpriteTileFetcher));
	functor.fun(&pixel_shifter, sizeof(PixelShifter));

	functor.fun(&LCD_enabled, sizeof(bool));
	functor.fun(&WY_equals_LY_in_current_frame, sizeof(bool));
	functor.fun(&scale, sizeof(unsigned));

	functor.fun(&obj_priority_mode, sizeof(OBJ_Priority_Mode));
	functor.fun(&can_access_VRAM, sizeof(bool));
	functor.fun(&can_access_OAM, sizeof(bool));

	// LDCD flags
	functor.fun(&BG_enabled, sizeof(bool));
	functor.fun(&BG_master_priority, sizeof(bool));
	functor.fun(&sprites_enabled, sizeof(bool));
	functor.fun(&window_display_enabled, sizeof(bool));
	functor.fun(&sprite_height, sizeof(u8));
	functor.fun(&addr_BG_tile_map, sizeof(u16));
	functor.fun(&addr_tile_data, sizeof(u16));
	functor.fun(&addr_window_tile_map, sizeof(u16));

	functor.fun(&reset_graphics_after_render, sizeof(bool));
	functor.fun(&STAT_cond_met_LY_LYC, sizeof(bool));
	functor.fun(&STAT_cond_met_LCD_mode, sizeof(bool));
	functor.fun(&tile_data_signed, sizeof(bool));

	functor.fun(&leftmost_pixels_to_ignore, sizeof(u8));
	functor.fun(framebuffer, std::size(framebuffer) * sizeof(u8));

	functor.fun(&OAM_sprite_addr, sizeof(u16));

	functor.fun(&t_cycle_counter, sizeof(unsigned));
	functor.fun(&pixel_offset_x, sizeof(unsigned));
	functor.fun(&pixel_offset_y, sizeof(unsigned));
	functor.fun(&scale_temp, sizeof(unsigned));
	functor.fun(&pixel_offset_x_temp, sizeof(unsigned));
	functor.fun(&pixel_offset_y_temp, sizeof(unsigned));
	functor.fun(&framebuffer_pos, sizeof(unsigned));
	functor.fun(&OAM_check_interval, sizeof(unsigned));

	functor.fun(GB_palette, std::size(GB_palette) * sizeof(SDL_Color));

	functor.fun(&rect, sizeof(SDL_Rect));

	functor.fun(BGP_reg, std::size(BGP_reg) * sizeof(u8));
	functor.fun(OBP_reg, std::size(OBP_reg) * sizeof(u8));

	functor.fun(BGP_GBC, std::size(BGP_GBC) * sizeof(SDL_Color));
	functor.fun(OBP_GBC, std::size(OBP_GBC) * sizeof(SDL_Color));
}


void PPU::Configure(Serialization::BaseFunctor& functor)
{
	functor.fun(&scale, sizeof(unsigned));
	functor.fun(GB_palette, std::size(GB_palette) * sizeof(SDL_Color));
}


void PPU::SetDefaultConfig()
{
	scale = default_scale;
	for (int i = 0; i < 4; i++)
		GB_palette[i] = default_palette[i];
}