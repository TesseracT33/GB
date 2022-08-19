export module DMA;

import Util;

import <string_view>;

namespace DMA
{
	export
	{
		enum class Reg {
			DMA, HDMA1, HDMA2, HDMA3, HDMA4, HDMA5
		};

		template<Reg reg>
		u8 ReadReg();

		template<Reg reg>
		void WriteReg(u8 value);

		bool CgbDmaCurrentlyCopyingData();
		void Initialize();
		void HdmaStartBlockCopy(); // called by PPU during HBlank
		bool HdmaTransferActive();
		void StreamState(SerializationStream& stream);
		void Update();
	}

	enum class CgbDmaType {
		GDMA, HDMA
	};

	template<CgbDmaType>
	void StartCgbDmaTransfer(u8 data_written_to_hdma5);

	template<CgbDmaType>
	void UpdateCgbDma();

	void StartDmaTransfer(u8 data_written_to_dma_reg);
	void UpdateDma();

	constexpr uint dma_byte_length = 160;

	bool dma_transfer_active;
	bool gdma_transfer_active;
	bool hdma_currently_copying_block;
	bool hdma_transfer_active;

	u16 dma_bytes_written;
	u16 dma_dst_addr;
	u16 dma_src_addr;
	u16 hdma_byte_length;
	u16 hdma_bytes_written;
	u16 hdma_dst_addr;
	u16 hdma_src_addr;
}