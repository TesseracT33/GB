#pragma once

#include "Bus.h"
#include "CPU.h"
#include "Utils.h"

class DMA final : public Serializable
{
public:
	Bus* bus;
	CPU* cpu;

	void Reset();
	void Update();

	// DMA-specific
	void InitiateDMATransfer(u8 data);
	void UpdateDMA();
	bool DMA_transfer_active = false;
	u16 DMA_source_addr = 0;
	const int DMA_written_bytes_total = 160;
	int DMA_written_bytes = 0;

	// HDMA-specific (also includes GDMA stuff)
	void InitiateHDMATransfer(u8 data); // called when HDMA transfer is first started
	void Initiate_HDMA_Update(); // called during HBlank when a block is to be copied
	void UpdateHDMA(); // called each m-cycle when a block is copied
	bool HDMA_transfer_active = false;
	bool HDMA_currently_copying_block = false;
	bool HDMA_transfer_started_when_lcd_off = false;
	u16 HDMA_source_addr = 0;
	u16 HDMA_dest_addr = 0x8000; // dest addr. is always in VRAM
	u8 HDMA_current_block_copy_byte_index = 0;
	int HDMA_written_blocks = 0;
	int HDMA_written_blocks_total = 0;
	int HDMA_corrupted_bytes_written = 0;
	const u8 HDMA_corrupted_byte = 0; // todo: not sure what to set this as

	// GDMA-specific
	void InitiateGDMATransfer(u8 data);
	void UpdateGDMA();
	bool GDMA_transfer_active = false;
	u8 GDMA_written_blocks_total = 0;
	u8 GDMA_written_blocks = 0;
	int GDMA_corrupted_bytes_written = 0;
	u8 GDMA_current_block_copy_byte_index = 0;

	void Serialize(std::ofstream& ofs) override;
	void Deserialize(std::ifstream& ifs) override;
};