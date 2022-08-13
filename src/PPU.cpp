module PPU;

import CPU;
import DMA;
import PPU.Palettes;
import System;

import Util.Bit;

import Video;

namespace PPU
{
	u8 ReadBCPD()
	{
		u8 addr = bcps & 0x3F;
		return bg_palette_ram[addr];
	}


	u8 ReadBCPS()
	{
		return bcps | 0x40; /* bit 6 always reads 1 (?) */
	}


	u8 ReadBGP()
	{
		return bgp;
	}


	u8 ReadLCDC()
	{
		return std::bit_cast<u8, decltype(lcdc)>(lcdc);
	}


	u8 ReadLY()
	{
		return ly;
	}


	u8 ReadLYC()
	{
		return lyc;
	}


	u8 ReadOamCpu(u16 addr)
	{
		return stat.lcd_mode <= 1 ? oam[addr] : 0xFF;
	}


	u8 ReadOBP0()
	{
		return obp_dmg[0];
	}


	u8 ReadOBP1()
	{
		return obp_dmg[1];
	}


	u8 ReadOCPD()
	{
		u8 addr = ocps & 0x3F;
		return obj_palette_ram[addr];
	}


	u8 ReadOCPS()
	{
		return ocps | 0x40; /* bit 6 always reads 1 (?) */
	}


	u8 ReadOPRI()
	{
		static_assert(std::to_underlying(ObjPriorityMode::OamIndex) == 0);
		static_assert(std::to_underlying(ObjPriorityMode::Coordinate) == 1);
		return 0xFE | std::to_underlying(obj_priority_mode);
	}


	u8 ReadSCX()
	{
		return scx;
	}


	u8 ReadSCY()
	{
		return scy;
	}


	u8 ReadSTAT()
	{
		return std::bit_cast<u8, decltype(stat)>(stat) | 0x80;
	}


	u8 ReadVBK()
	{
		return System::mode == System::Mode::CGB
			? 0xFE | current_vram_bank
			: 0xFF;
	}


	u8 ReadWX()
	{
		return wx;
	}


	u8 ReadWY()
	{
		return wy;
	}


	void WriteBCPD(u8 data)
	{
		u8 addr = bcps & 0x3F;
		bg_palette_ram[addr] = data;

		/* Update the bg palette containing actual RGB colors. */
		u16 color_data = bg_palette_ram[addr & ~1] | bg_palette_ram[addr | 1] << 8;
		auto col_index = addr / 2;
		cgb_bg_palette[col_index] = CgbColorDataToRGB(color_data);

		if (bcps & 0x80) {
			addr++;
			bcps = addr & 0x3F | bcps & ~0x3F;
		}
	}


	void WriteBCPS(u8 data)
	{
		bcps = data;
	}


	void WriteBGP(u8 data)
	{
		bgp = data;
	}


	void WriteLCDC(u8 data)
	{
		u8 prev_lcdc = std::bit_cast<u8, decltype(lcdc)>(lcdc);
		lcdc = std::bit_cast<decltype(lcdc), u8>(data);
		ReadLcdcFlags();
		if ((prev_lcdc ^ data) & 0x80) {
			if (lcdc.lcd_enable) {
				// TODO: probably something happens here
				PrepareForNewScanline();
			}
			else {
				stat_interrupt_cond = false;
				SetLcdMode(LcdMode::HBlank);
				ly = m_cycle_counter = framebuffer_pos = 0;
			}
			CheckStatInterrupt();
		}
	}


	void WriteLY(u8 data)
	{
		ly = 0;
		m_cycle_counter = 0;
		PrepareForNewFrame(); /* TODO: not sure exactly what should happen here */
		PrepareForNewScanline();
		CheckStatInterrupt();
		wy_equalled_ly_this_frame |= wy == 0;
	}


	void WriteLYC(u8 data)
	{
		lyc = data;
		CheckStatInterrupt();
	}


	void WriteOamCpu(u16 addr, u8 data)
	{
		if (stat.lcd_mode <= 1) {
			oam[addr] = data;
		}
	}


	void WriteOBP0(u8 data)
	{
		obp_dmg[0] = data;
	}


	void WriteOBP1(u8 data)
	{
		obp_dmg[1] = data;
	}


	void WriteOCPD(u8 data)
	{
		u8 addr = ocps & 0x3F;
		obj_palette_ram[addr] = data;

		/* Update the bg palette containing actual RGB colors. */
		u16 color_data = obj_palette_ram[addr & ~1] | obj_palette_ram[addr | 1] << 8;
		auto col_index = addr / 2;
		cgb_obj_palette[col_index] = CgbColorDataToRGB(color_data);

		if (ocps & 0x80) {
			addr++;
			ocps = addr & 0x3F | ocps & ~0x3F;
		}
	}


	void WriteOCPS(u8 data)
	{
		ocps = data;
	}


	void WriteOPRI(u8 data)
	{
		if (data & 1) {
			obj_priority_mode = ObjPriorityMode::Coordinate;
		}
		else {
			obj_priority_mode = ObjPriorityMode::OamIndex;
		}
	}


	void WriteSCX(u8 data)
	{
		scx = data;
	}


	void WriteSCY(u8 data)
	{
		scy = data;
	}


	void WriteSTAT(u8 data)
	{
		/* Only bits 3-6 are writeable */
		u8 prev_stat = std::bit_cast<u8, decltype(stat)>(stat);
		u8 new_stat = prev_stat & ~0x78 | data & 0x78;
		stat = std::bit_cast<decltype(stat), u8>(new_stat);
		CheckStatInterrupt();
	}


	void WriteVBK(u8 data)
	{
		if (System::mode == System::Mode::CGB) {
			current_vram_bank = data & 1;
		}
	}


	void WriteWX(u8 data)
	{
		wx = data;
	}


	void WriteWY(u8 data)
	{
		wy = data;
		wy_equalled_ly_this_frame |= ly == wy;
	}


	void SetDmgPalette(DmgPalette palette)
	{
		dmg_palette = palette;
	}


	RGB CgbColorDataToRGB(u16 color_data)
	{
		u8 red = color_data & 0x1F;
		u8 green = color_data >> 5 & 0x1F;
		u8 blue = color_data >> 10 & 0x1F;
		// convert each 5-bit channel to 8-bit channels (https://github.com/mattcurrie/dmg-acid2)
		red = red << 3 | red >> 2;
		green = green << 3 | green >> 2;
		blue = blue << 3 | blue >> 2;
		return { red, green, blue };
	}


	void Initialize()
	{
		Video::SetFramebufferPtr(framebuffer.data());
		Video::SetFramebufferSize(resolution_x, resolution_y);
		Video::SetPixelFormat(Video::PixelFormat::RGB888);

		dmg_palette = Palettes::grayscale;

		oam.fill(0);
		vram.fill(0);

		bgp = ly = lyc = scx = scy = wx = wy = 0;
		obp_dmg[0] = obp_dmg[1] = 0xFF;
		lcdc = std::bit_cast<decltype(lcdc), u8>(u8(0));
		stat = std::bit_cast<decltype(stat), u8>(u8(0));
		ReadLcdcFlags();

		sprite_buffer.clear();
		sprite_buffer.reserve(sprite_buffer_capacity);
		ClearFifos();

		stat_interrupt_cond = false;
		wy_equalled_ly_this_frame = false;

		current_vram_bank = 0;
		framebuffer_pos = 0;
		leftmost_bg_pixels_to_discard = 0;
		m_cycle_counter = 0;
		oam_addr = 0;

		bg_tile_fetcher.Reset(true);
		sprite_fetcher.Reset();
		pixel_shifter.Reset();

		obj_priority_mode = ObjPriorityMode::Coordinate;
		SetLcdMode(LcdMode::SearchOam);

		CheckStatInterrupt();
	}


	void Update()
	{
		if (!lcdc.lcd_enable) {
			return;
		}
		// 114 m-cycles per scanline in single-speed mode. for double-speed, all numbers should be doubled
		// 154 scanlines in total (LY == current scanline). Only scanlines 0-143 are visible.
		if (ly < 144) {
			if (m_cycle_counter < 20 * std::to_underlying(System::speed)) {
				ScanOam();
			}
			else {
				if (m_cycle_counter == 20 * std::to_underlying(System::speed)) {
					SetLcdMode(LcdMode::DriverTransfer);
					ClearFifos();
				}
				for (int i = 0; i < 4; ++i) {
					UpdatePixelFetchers();
				}
			}
		}
		else if (ly == 144 && m_cycle_counter == 1) {
			EnterVBlank();
		}
		if (++m_cycle_counter == m_cycles_per_scanline * std::to_underlying(System::speed)) {
			m_cycle_counter = 0;
			if (ly == 153) {
				ly = 0;
				PrepareForNewFrame();
				PrepareForNewScanline();
			}
			else {
				ly++;
				if (ly < 144) {
					PrepareForNewScanline();
				}
			}
			wy_equalled_ly_this_frame |= wy == ly;
			CheckStatInterrupt();
		}
	}


	u8 ReadVRAM(u16 addr)
	{
		addr &= (vram_bank_size - 1);
		return vram[addr + current_vram_bank * vram_bank_size];
	}


	u8 ReadVramCpu(u16 addr)
	{
		return stat.lcd_mode == 3
			? 0xFF
			: ReadVRAM(addr);
	}


	void WriteVramCpu(u16 addr, u8 data)
	{
		if (stat.lcd_mode != 3) {
			addr &= (vram_bank_size - 1);
			vram[addr + current_vram_bank * vram_bank_size] = data;
		}
	}


	void EnterVBlank()
	{
		bg_tile_fetcher.window_line_counter = -1;
		SetLcdMode(LcdMode::VBlank);
		CPU::RequestInterrupt(CPU::Interrupt::VBlank);
		Video::NotifyNewGameFrameReady();
	}


	void EnterHBlank()
	{
		SetLcdMode(LcdMode::HBlank);
		if (DMA::HdmaTransferActive()) {
			DMA::HdmaStartBlockCopy();
		}
		bg_tile_fetcher.paused = sprite_fetcher.paused = pixel_shifter.paused = true;
	}


	void SetLcdMode(LcdMode lcd_mode)
	{
		stat.lcd_mode = std::to_underlying(lcd_mode);
		CheckStatInterrupt();
	}


	void CheckStatInterrupt()
	{
		if (!lcdc.lcd_enable) {
			return;
		}

		stat.lyc_equals_ly = ly == lyc;

		bool stat_cond_met_ly_equals_lyc = ly == lyc && stat.lyc_equals_ly_stat_int_enable;
		bool stat_cond_met_lcd_mode = stat.lcd_mode == 0 && stat.hblank_stat_int_enable
			|| stat.lcd_mode == 1 && stat.vblank_stat_int_enable
			|| stat.lcd_mode == 2 && stat.oam_stat_int_enable;
		bool prev_stat_interrupt_cond = stat_interrupt_cond;
		stat_interrupt_cond = stat_cond_met_ly_equals_lyc || stat_cond_met_lcd_mode;

		if (!prev_stat_interrupt_cond && stat_interrupt_cond) {
			CPU::RequestInterrupt(CPU::Interrupt::Stat);
		}
	}


	void ReadLcdcFlags()
	{
		sprite_height = lcdc.obj_size ? 16 : 8;
		bg_tile_map_base_addr = lcdc.bg_tile_map_area ? 0x9C00 : 0x9800;
		tile_data_base_addr = lcdc.bg_tile_data_area ? 0x8000 : 0x9000;
		window_tile_map_base_addr = lcdc.window_tile_map_area ? 0x9C00 : 0x9800;
		tile_nums_are_signed = tile_data_base_addr == 0x9000;
	}


	template<TileType tile_type>
	RGB GetColourFromPixel(FifoPixel pixel)
	{
		if (System::mode == System::Mode::DMG) {
			// which bits of the colour palette does the colour id map to?
			auto shift = pixel.col_id * 2;
			auto col_index = [&] {
				if constexpr (tile_type == TileType::BG) {
					return bgp;
				}
				else {
					return obp_dmg[pixel.palette & 1];
				}
			}() >> shift & 3;
			return dmg_palette[col_index];
		}
		else {
			auto col_index = 4 * pixel.palette + pixel.col_id;
			if constexpr (tile_type == TileType::BG) {
				return cgb_bg_palette[col_index];
			}
			else {
				return cgb_obj_palette[col_index];
			}
		}
	}


	void BackgroundTileFetcher::Step()
	{
		switch (tile_fetch_state_machine[step]) {
			case TileFetchStep::TileNum: {
				u16 tile_num_addr;
				u8 tile_col, tile_row;
				if (window_reached) {
					tile_num_addr = window_tile_map_base_addr;
					tile_col = tile_x_pos;
					tile_row = window_line_counter / 8;
				}
				else {
					tile_num_addr = bg_tile_map_base_addr;
					tile_col = (tile_x_pos + scx / 8) & 0x1F;
					tile_row = ((ly + scy) & 0xFF) / 8;
				}
				tile_num_addr += (32 * tile_row + tile_col) & 0x3FF;
				tile_num = tile_nums_are_signed ? (s8)ReadVRAM(tile_num_addr) : ReadVRAM(tile_num_addr);
				step = (step + 1) & 7;
				break;
			}

			case TileFetchStep::TileDataLow:
				tile_data_addr = tile_data_base_addr + 16 * tile_num + 2 * ([&] {
					if (window_reached) {
						return window_line_counter;
					}
					else {
						return ly + scy;
					}
				}() % 8);
				tile_data_low = ReadVRAM(tile_data_addr);
				step = (step + 1) & 7;
				break;

			case TileFetchStep::TileDataHigh:
				// The first time the background fetcher completes this step on a scanline, the status is fully reset and operation restarts at Step 1.
				if (fetch_tile_data_high_step_reached_this_scanline) {
					tile_data_high = ReadVRAM(tile_data_addr + 1);
					step = (step + 1) & 7;
				}
				else {
					fetch_tile_data_high_step_reached_this_scanline = true;
					step = 0;
				}
				break;

			case TileFetchStep::PushTile:
				// Step 4 is only executed if the background FIFO is fully empty.
				// If it is not, this step repeats every cycle until it succeeds.
				if (!bg_pixel_fifo.empty()) {
					return;
				}
				// Pixel 0 in the tile is bit 7 of tile data. Pixel 1 is bit 6 etc..
				for (int i = 7; i >= 0; --i) {
					// combine data 2 and data 1 to get the colour id for this pixel in the tile
					u8 col_id = GetBit(tile_data_high, i) << 1 | GetBit(tile_data_low, i);
					bg_pixel_fifo.emplace(col_id);
				}
				tile_x_pos = (tile_x_pos + 1) & 0x1F;
				step = (step + 1) & 7;
				break;

			case TileFetchStep::Sleep:
				step = (step + 1) & 7;
				break;
		}
	}


	void BackgroundTileFetcher::Reset(bool reset_window_line_counter)
	{
		step = 0;
		paused = window_reached = fetch_tile_data_high_step_reached_this_scanline = false;
		tile_x_pos = 0;
		if (reset_window_line_counter) {
			window_line_counter = -1;
		}
	}


	void SpriteFetcher::Step()
	{
		switch (tile_fetch_state_machine[step]) {
			case TileFetchStep::TileNum:
				tile_num = sprite.tile_num;
				if (sprite_height == 16)  {
					// if 8x16 sprites are used, find if low or high tile is used for the current scanline
					// To calculate the tile number of the top tile, the tile number in the OAM entry is used and the least significant bit is set to 0. 
					// The tile number of the bottom tile is calculated by setting the least significant bit to 1.
					if ((ly < sprite.pixel_y_pos - 8) ^ sprite.y_flip) {
						tile_num &= 0xFE;
					}
					else {
						tile_num |= 1;
					}
				}
				break;

			case TileFetchStep::TileDataLow:
				tile_addr = 0x8000 + 16 * tile_num + [&] {
					if (sprite.y_flip) {
						return 2 * (7 - (ly - sprite.pixel_y_pos + 16) % 8);
					}
					else {
						return 2 * ((ly - sprite.pixel_y_pos + 16) % 8);
					}
				}();
				tile_data_low = ReadVRAM(tile_addr);
				break;

			case TileFetchStep::TileDataHigh:
				tile_data_high = ReadVRAM(tile_addr + 1);
				break;

			case TileFetchStep::PushTile: {
				// Only pixels which are actually visible on the screen are loaded into the FIFO
				// e.g., if pixel shifter xPos = 0, then a sprite with an X-value of 8 would have all 8 pixels loaded, 
				// while a sprite with an X-value of 7 would only have the rightmost 7 pixels loaded
				// No need to worry about pixels going off the right side of the screen, since the PPU wouldn't push these pixels anyways
				auto pixels_to_ignore_left = std::max(0, 8 + pixel_shifter.pixel_x_pos - sprite.pixel_x_pos);

				if (obj_priority_mode == ObjPriorityMode::OamIndex) {
					// TODO (CGB)
				}
				// Pixel 0 in the tile is bit 7 of tile data. Pixel 1 is bit 6 etc..
				// insert as many pixels into FIFO as there are free slots
				// if FIFO already contains n no. of pixels, insert only the last 8 - n pixels from the current sprite¨
				auto InsertPixel = [&](int tile_data_bit_index) {
					auto col_id = GetBit(tile_data_high, tile_data_bit_index) << 1 
						| GetBit(tile_data_low, tile_data_bit_index);
					sprite_pixel_fifo.emplace(col_id, sprite.palette, sprite.oam_index, sprite.obj_to_bg_priority);
				};
				auto num_pixels_to_insert = std::max(sprite_pixel_fifo.size(), std::size_t(pixels_to_ignore_left));
				if (sprite.x_flip) {
					for (int i = int(num_pixels_to_insert); i <= 7; ++i) {
						InsertPixel(i);
					}
				}
				else {
					for (int i = 7 - int(num_pixels_to_insert); i >= 0; --i) {
						InsertPixel(i);
					}
				}
				bg_tile_fetcher.paused = pixel_shifter.paused = false;
				this->paused = true;
				break;
			}

			case TileFetchStep::Sleep:
				break;
		}

		step = (step + 1) & 7;
	}


	void SpriteFetcher::Reset()
	{
		step = 0;
		paused = true; /* must find sprite in OAM before fetch starts */
	}


	void PixelShifter::Reset()
	{
		pixel_x_pos = 0;
		paused = false;
	}


	void ShiftPixel()
	{
		if (bg_pixel_fifo.empty()) {
			return;
		}
		FifoPixel bg_pixel = bg_pixel_fifo.front();
		bg_pixel_fifo.pop();

		// SCX % 8 background pixels are discarded at the start of each scanline rather than being pushed to the LCD
		if (leftmost_bg_pixels_to_discard > 0) {
			leftmost_bg_pixels_to_discard--;
			return;
		}

		RGB pixel = [&] {
			if (System::mode == System::Mode::DMG && !lcdc.bg_enable) {
				bg_pixel.col_id = 0;
			}
			if (sprite_pixel_fifo.empty()) {
				return GetColourFromPixel<TileType::BG>(bg_pixel);
			}
			else {
				FifoPixel sprite_pixel = sprite_pixel_fifo.front();
				sprite_pixel_fifo.pop();

				if (System::mode == System::Mode::DMG) {
					if (sprite_pixel.col_id == 0 || sprite_pixel.bg_priority && bg_pixel.col_id != 0) {
						return GetColourFromPixel<TileType::BG>(bg_pixel);
					}
					else {
						return GetColourFromPixel<TileType::OBJ>(sprite_pixel);
					}
				}
				else { /* CGB */
					if (lcdc.bg_enable) { /* Window Master Priority set */
						// TODO: sprite pixel displayed even if sprite color id is 0?
						return GetColourFromPixel<TileType::OBJ>(sprite_pixel);
					}
					else if (sprite_pixel.col_id == 0 || sprite_pixel.bg_priority && bg_pixel.col_id != 0) {
						return GetColourFromPixel<TileType::BG>(bg_pixel);
					}
					else {
						return GetColourFromPixel<TileType::OBJ>(sprite_pixel);
					}
				}
			}
		}();

		PushPixel(pixel);
	}


	void PushPixel(RGB pixel)
	{
		framebuffer[framebuffer_pos++] = pixel.r;
		framebuffer[framebuffer_pos++] = pixel.g;
		framebuffer[framebuffer_pos++] = pixel.b;
		if (++pixel_shifter.pixel_x_pos == resolution_x) {
			EnterHBlank();
		}
		// After each pixel is shifted out, the PPU checks if it has reached the window.
		// If it has, the background fetcher state is fully reset to step 1
		if (!bg_tile_fetcher.window_reached) {
			CheckIfWindowReached();
		}
	}


	void ScanOam()
	{
		/* This function is called once every m-cycle. It takes two t-cycles to check one oam entry. */
		for (int i = 0; i < 2 && sprite_buffer.size() < sprite_buffer_capacity; ++i) {
			u8 pixel_y_pos = oam[oam_addr];
			if (ly + 16u >= pixel_y_pos && ly + 16u < pixel_y_pos + sprite_height) {
				u8 pixel_x_pos = oam[oam_addr + 1];
				u8 tile_num = oam[oam_addr + 2];
				u8 attributes = oam[oam_addr + 3];
				bool x_flip = attributes & 0x20;
				bool y_flip = attributes & 0x40;
				bool obj_to_bg_priority = attributes & 0x80;
				if (System::mode == System::Mode::DMG) {
					bool palette = attributes & 0x10;
					sprite_buffer.emplace_back(tile_num, pixel_x_pos, pixel_y_pos, palette, obj_to_bg_priority, x_flip, y_flip);
				}
				else {
					u8 palette = attributes & 7;
					bool vram_bank = attributes & 8; // todo: how does this dictate which bank is used? isn't this set by writing to VBK?
					u8 oam_index = oam_addr / 4;
					sprite_buffer.emplace_back(tile_num, pixel_x_pos, pixel_y_pos, palette, obj_to_bg_priority, x_flip, y_flip, vram_bank, oam_index);
				}
			}
			oam_addr += 4;
		}
	}


	void AttemptSpriteFetch()
	{
		auto candidate = sprite_buffer.end();

		if (System::mode == System::Mode::DMG || obj_priority_mode == ObjPriorityMode::Coordinate) {
			for (auto it = sprite_buffer.begin(); it != sprite_buffer.end(); ++it) {
				if (it->pixel_x_pos <= pixel_shifter.pixel_x_pos + 8 && 
					(candidate == sprite_buffer.end() || it->pixel_x_pos < candidate->pixel_x_pos)) {
					candidate = it;
				}
			}
		}
		else { /* Oam index priority mode */
			for (auto it = sprite_buffer.begin(); it != sprite_buffer.end(); ++it) {
				if (it->pixel_x_pos <= pixel_shifter.pixel_x_pos + 8) {
					candidate = it;
					break;
				}
			}
		}

		if (candidate != sprite_buffer.end()) {
			sprite_fetcher.sprite = *candidate;
			sprite_buffer.erase(candidate);
			sprite_fetcher.paused = false;
			bg_tile_fetcher.paused = pixel_shifter.paused = true;
			bg_tile_fetcher.step = 0;
		}
	}


	void ClearFifos()
	{
		while (!bg_pixel_fifo.empty()) {
			bg_pixel_fifo.pop();
		}
		while (!sprite_pixel_fifo.empty()) {
			sprite_pixel_fifo.pop();
		}
	}


	void UpdatePixelFetchers()
	{
		if (pixel_shifter.pixel_x_pos >= resolution_x) {
			return;
		}
		if (lcdc.obj_enable) {
			AttemptSpriteFetch();
			if (!sprite_fetcher.paused) {
				sprite_fetcher.Step();
			}
		}
		if (!bg_tile_fetcher.paused) {
			bg_tile_fetcher.Step();
		}
		if (!pixel_shifter.paused) {
			ShiftPixel();
		}
	}


	void CheckIfWindowReached()
	{
		// After each pixel is shifted out, the PPU checks if it has reached the window.
		// If all of the below conditions apply, window fetching starts.
		if (lcdc.window_enable && lcdc.bg_enable && wy_equalled_ly_this_frame && pixel_shifter.pixel_x_pos >= wx - 7) {
			bg_tile_fetcher.step = 0;
			bg_tile_fetcher.tile_x_pos = 0;
			bg_tile_fetcher.window_reached = true;
			while (!bg_pixel_fifo.empty()) {
				bg_pixel_fifo.pop();
			}
			bg_tile_fetcher.window_line_counter++;
		}
	}


	void PrepareForNewScanline()
	{
		bg_tile_fetcher.Reset(false);
		sprite_fetcher.Reset();
		pixel_shifter.Reset();
		sprite_buffer.clear();
		oam_addr = 0;
		// SCX % 8 bg pixels are discarded at the start of each scanline rather than being pushed to the LCD
		leftmost_bg_pixels_to_discard = scx % 8;
		SetLcdMode(LcdMode::SearchOam);
	}


	void PrepareForNewFrame()
	{
		wy_equalled_ly_this_frame = ly == wy;
		framebuffer_pos = 0;
	}


	void StreamState(SerializationStream& stream)
	{
		/* TODO */
	}
}