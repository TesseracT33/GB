#include "DMA.h"

using namespace Util;

void DMA::Reset()
{
	DMA_transfer_active = HDMA_transfer_active = GDMA_transfer_active = false;
	HDMA_dest_addr = 0x8000;
}


void DMA::InitiateDMATransfer(u8 data)
{
	DMA_source_addr = data << 8;
	DMA_written_bytes = 0;
	DMA_transfer_active = true;
}


void DMA::UpdateDMA()
{
	// one byte is transfered every m-cycle, even in double-speed mode. The copy starts 1 m-cycle after the DMA transfer has been initiated.
	u16 source = DMA_source_addr + DMA_written_bytes;
	u16 dest = Bus::Addr::OAM_START + DMA_written_bytes;
	bus->Write(dest, bus->Read(source));
	if (++DMA_written_bytes == DMA_written_bytes_total)
		DMA_transfer_active = false;
	// TODO: If HALT or STOP mode are entered during a DMA copy, the copy will still complete correctly.
	//     The same happens with GBC performing a speed switch. The copy is paused during HALT and
	//      STOP modes, and it's also paused during the speed switch.
}


void DMA::InitiateHDMATransfer(u8 data)
{
	HDMA_written_blocks = 0;
	HDMA_written_blocks_total = data & 0x7F;
	HDMA_transfer_active = true;

	// start the copy of one block initially if LCD is off or HBlank is active
	u8 LCDC = bus->Read(Bus::Addr::LCDC);
	u8 STAT = bus->Read(Bus::Addr::STAT);
	HDMA_transfer_started_when_lcd_off = !CheckBit(LCDC, 7);
	if (HDMA_transfer_started_when_lcd_off || (STAT & 3) == 0)
	{
		Initiate_HDMA_Update();
	}
}


void DMA::Initiate_HDMA_Update()
{
	// Copy time per HBL update is 1+8=9 m-cycles, or 1+16=17 in double speed mode
	// During each update, the cpu is stopped. 
	// 16 bytes (1 block) is copied in total, so 2 bytes should be copied per m-cycle in single speed mode, and 1 byte per m-cycle in double speed
	if (HDMA_transfer_active && !cpu->speed_switch_active)
	{
		HDMA_currently_copying_block = cpu->HDMA_transfer_active = true;
		HDMA_current_block_copy_byte_index = 0;
		HDMA_corrupted_bytes_written = 0;
	}
}


void DMA::UpdateHDMA()
{
	if (HDMA_transfer_active && !cpu->speed_switch_active)
	{
		// copy two bytes if in single speed mode, one byte if in double speed mode
		int HDMA_bytes_per_m_cycle = (System::speed_mode == System::SpeedMode::SINGLE) ? 2 : 1;
		for (int i = 0; i < HDMA_bytes_per_m_cycle; i++)
		{
			// if source is within VRAM, first copy two "corrupted" bytes, then just 0xFF
			// in HDMA, the corrupted bytes are copied for each block
			if (HDMA_source_addr >= 0x8000 && HDMA_source_addr <= 0x9FFF)
			{
				if (HDMA_corrupted_bytes_written < 2)
				{
					bus->Write(HDMA_dest_addr, HDMA_corrupted_byte);
					HDMA_corrupted_bytes_written++;
				}
				else
					bus->Write(HDMA_dest_addr, 0xFF);
			}
			else
				bus->Write(HDMA_dest_addr, bus->Read(HDMA_source_addr));

			HDMA_source_addr++;
			HDMA_dest_addr++;
			HDMA_current_block_copy_byte_index++;

			if (HDMA_current_block_copy_byte_index == 16)
			{
				HDMA_currently_copying_block = cpu->HDMA_transfer_active = false;
				HDMA_written_blocks++;
				if (HDMA_written_blocks == HDMA_written_blocks_total)
				{
					HDMA_transfer_active = false;
					u8* HDMA5 = bus->ReadIOPointer(Bus::Addr::HDMA5);
					*HDMA5 = 0xFF;
				}
			}
		}
	}
}


void DMA::InitiateGDMATransfer(u8 data)
{
	GDMA_written_blocks_total = data & 0x7F; // bits 0-6.
	GDMA_written_blocks = 0;
	GDMA_transfer_active = cpu->HDMA_transfer_active = true;
	GDMA_corrupted_bytes_written = 0;
	GDMA_current_block_copy_byte_index = 0;
}


void DMA::UpdateGDMA()
{
	// copy two bytes if in single speed mode, one byte if in double speed mode
	int GDMA_bytes_per_m_cycle = (System::speed_mode == System::SpeedMode::SINGLE) ? 2 : 1;
	for (int i = 0; i < GDMA_bytes_per_m_cycle; i++)
	{
		// if source is within VRAM, first copy two "corrupted" bytes, then just 0xFF
		// in GDMA, the corrupted bytes are copied once for the entire copy (not once per block as in HDMA)
		if (HDMA_source_addr >= 0x8000 && HDMA_source_addr <= 0x9FFF)
		{
			if (GDMA_corrupted_bytes_written < 2)
			{
				bus->Write(HDMA_dest_addr, HDMA_corrupted_byte);
				GDMA_corrupted_bytes_written++;
			}
			else
				bus->Write(HDMA_dest_addr, 0xFF);
		}
		else
			bus->Write(HDMA_dest_addr, bus->Read(HDMA_source_addr));

		HDMA_source_addr++;
		HDMA_dest_addr++;
		GDMA_current_block_copy_byte_index++;

		if (GDMA_current_block_copy_byte_index == 16)
		{
			GDMA_written_blocks++;
			if (GDMA_written_blocks == GDMA_written_blocks_total)
			{
				GDMA_transfer_active = cpu->HDMA_transfer_active = false;
				u8* HDMA5 = bus->ReadIOPointer(Bus::Addr::HDMA5);
				*HDMA5 = 0xFF;
			}
		}
	}
}


void DMA::Update()
{
	if (DMA_transfer_active)
		UpdateDMA();

	if (HDMA_transfer_active && HDMA_currently_copying_block)
		UpdateHDMA();

	if (GDMA_transfer_active)
		UpdateGDMA();
}


void DMA::Deserialize(std::ifstream& ifs)
{
	ifs.read((char*)&DMA_transfer_active, sizeof(bool));
	ifs.read((char*)&DMA_source_addr, sizeof(u16));
	ifs.read((char*)&DMA_written_bytes, sizeof(int));
	
	ifs.read((char*)&HDMA_transfer_active, sizeof(bool));
	ifs.read((char*)&HDMA_currently_copying_block, sizeof(bool));
	ifs.read((char*)&HDMA_transfer_started_when_lcd_off, sizeof(bool));
	ifs.read((char*)&HDMA_source_addr, sizeof(u16));
	ifs.read((char*)&HDMA_dest_addr, sizeof(u16));
	ifs.read((char*)&HDMA_current_block_copy_byte_index, sizeof(u8));
	ifs.read((char*)&HDMA_written_blocks, sizeof(int));
	ifs.read((char*)&HDMA_written_blocks_total, sizeof(int));
	ifs.read((char*)&HDMA_corrupted_bytes_written, sizeof(int));

	ifs.read((char*)&GDMA_transfer_active, sizeof(bool));
	ifs.read((char*)&GDMA_written_blocks_total, sizeof(u8));
	ifs.read((char*)&GDMA_written_blocks, sizeof(u8));
	ifs.read((char*)&GDMA_corrupted_bytes_written, sizeof(int));
	ifs.read((char*)&GDMA_current_block_copy_byte_index, sizeof(u8));
}


void DMA::Serialize(std::ofstream& ofs)
{
	ofs.write((char*)&DMA_transfer_active, sizeof(bool));
	ofs.write((char*)&DMA_source_addr, sizeof(u16));
	ofs.write((char*)&DMA_written_bytes, sizeof(int));

	ofs.write((char*)&HDMA_transfer_active, sizeof(bool));
	ofs.write((char*)&HDMA_currently_copying_block, sizeof(bool));
	ofs.write((char*)&HDMA_transfer_started_when_lcd_off, sizeof(bool));
	ofs.write((char*)&HDMA_source_addr, sizeof(u16));
	ofs.write((char*)&HDMA_dest_addr, sizeof(u16));
	ofs.write((char*)&HDMA_current_block_copy_byte_index, sizeof(u8));
	ofs.write((char*)&HDMA_written_blocks, sizeof(int));
	ofs.write((char*)&HDMA_written_blocks_total, sizeof(int));
	ofs.write((char*)&HDMA_corrupted_bytes_written, sizeof(int));

	ofs.write((char*)&GDMA_transfer_active, sizeof(bool));
	ofs.write((char*)&GDMA_written_blocks_total, sizeof(u8));
	ofs.write((char*)&GDMA_written_blocks, sizeof(u8));
	ofs.write((char*)&GDMA_corrupted_bytes_written, sizeof(int));
	ofs.write((char*)&GDMA_current_block_copy_byte_index, sizeof(u8));
}