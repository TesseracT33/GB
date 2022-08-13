module DMA;

import Bus;
import CPU;
import Debug;
import System;

import Util;

namespace DMA
{
	bool CgbDmaCurrentlyCopyingData()
	{
		return gdma_transfer_active || hdma_currently_copying_block;
	}


	void Initialize()
	{
		dma_transfer_active = gdma_transfer_active = hdma_transfer_active = hdma_currently_copying_block = false;
		hdma_dst_addr = 0x8000;
	}


	void HdmaStartBlockCopy()
	{
		hdma_currently_copying_block = true;
	}


	bool HdmaTransferActive()
	{
		return hdma_transfer_active;
	}


	template<Reg reg>
	u8 ReadReg()
	{
		if constexpr (reg == Reg::DMA) {
			return dma_src_addr >> 8;
		}
		else if constexpr (reg == Reg::HDMA5) {
			if (System::mode == System::Mode::CGB && hdma_transfer_active) {
				return (hdma_byte_length - hdma_bytes_written) / 16 - 1;
			}
			else {
				return 0xFF;
			}
		}
		else { // HDMA1-4
			return 0xFF;
		}
	}


	void StartDmaTransfer(u8 data_written_to_dma_reg)
	{
		dma_dst_addr = 0xFE00;
		dma_src_addr = data_written_to_dma_reg << 8;
		dma_bytes_written = 0;
		dma_transfer_active = true;
		/* The copy starts 1 m-cycle after the DMA transfer has been initiated. */
		if constexpr (Debug::log_dma) {
			Debug::LogDma(dma_src_addr);
		}
	}


	template<CgbDmaType dma_type>
	void StartCgbDmaTransfer(u8 data_written_to_hdma5)
	{
		hdma_byte_length = ((data_written_to_hdma5 & 0x7F) + 1) * 16;
		hdma_bytes_written = 0;
		if constexpr (dma_type == CgbDmaType::GDMA) {
			gdma_transfer_active = true;
		}
		else {
			hdma_transfer_active = true;
		}
		if constexpr (Debug::log_dma) {
			static constexpr std::string_view type = [&] {
				if constexpr (dma_type == CgbDmaType::GDMA) return "GDMA";
				else                                        return "HDMA";
			}();
			Debug::LogHdma(type, hdma_dst_addr, hdma_src_addr, hdma_byte_length);
		}
	}


	void StreamState(SerializationStream& stream)
	{
		stream.StreamPrimitive(dma_transfer_active);
		stream.StreamPrimitive(gdma_transfer_active);
		stream.StreamPrimitive(hdma_currently_copying_block);
		stream.StreamPrimitive(hdma_transfer_active);
		stream.StreamPrimitive(dma_bytes_written);
		stream.StreamPrimitive(dma_dst_addr);
		stream.StreamPrimitive(dma_src_addr);
		stream.StreamPrimitive(hdma_byte_length);
		stream.StreamPrimitive(hdma_bytes_written);
		stream.StreamPrimitive(hdma_dst_addr);
		stream.StreamPrimitive(hdma_src_addr);
	}


	void Update()
	{
		if (dma_transfer_active) {
			UpdateDma();
		}
		if (gdma_transfer_active) {
			UpdateCgbDma<CgbDmaType::GDMA>();
		}
		else if (hdma_transfer_active && hdma_currently_copying_block) {
			UpdateCgbDma<CgbDmaType::HDMA>();
		}
	}


	void UpdateDma()
	{
		/* One byte is transferred per m-cycle (single/double speed mode irrelevant) */
		Bus::Write(dma_dst_addr, Bus::Read(dma_src_addr));
		if (++dma_bytes_written == dma_byte_length) {
			dma_transfer_active = false;
		}
		else {
			++dma_dst_addr;
			++dma_src_addr;
		}
	}


	template<CgbDmaType dma_type>
	void UpdateCgbDma()
	{
		/* "In both Normal Speed and Double Speed Mode it takes about 8 μs to transfer a block of $10 bytes. 
		   That is, 8 M-cycles in Normal Speed Mode, and 16 “fast” M-cycles in Double Speed Mode."
		   Source: https://gbdev.io/pandocs/CGB_Registers.html#transfer-timings. 
		   Note: this function is called once every m-cycle. Thus, we copy two bytes per update in single speed mode,
		   and a single byte in double speed mode. */
		auto CopyByte = [&] {
			// if source addr is within VRAM ($8000-$9FFF), copy just 0xFF
			// note: variable hdma_source_addr is shared between hdma and gdma
			if ((hdma_src_addr & 0xE000) == 0x8000) {
				Bus::Write(hdma_dst_addr, 0xFF);
			}
			else {
				Bus::Write(hdma_dst_addr, Bus::Read(hdma_src_addr));
			}
			++hdma_bytes_written;
			++hdma_src_addr;
			++hdma_dst_addr;
		};

		CopyByte();
		if (System::speed == System::Speed::Single) {
			CopyByte();
		}
		/* An even number of bytes are always copied during each GDMA/HDMA. 
		   Thus, the below checks do not have to take place inside 'CopyByte' */
		if constexpr (dma_type == CgbDmaType::HDMA) {
			if ((hdma_bytes_written & 0xF) == 0) { /* Blocks are 0x10 bytes large; means that a block copy has finished. */
				hdma_currently_copying_block = false;
			}
		}
		if (hdma_bytes_written == hdma_byte_length) {
			if constexpr (dma_type == CgbDmaType::GDMA) {
				gdma_transfer_active = false;
			}
			else {
				hdma_transfer_active = false;
			}
		}
	}


	template<Reg reg>
	void WriteReg(u8 data)
	{
		if constexpr (reg == Reg::DMA) {
			StartDmaTransfer(data);
		}
		else if (System::mode == System::Mode::CGB) {
			if constexpr (reg == Reg::HDMA1) {
				hdma_src_addr &= 0xFF;
				hdma_src_addr |= data << 8;
				// Selecting an address in the range $E000-$FFFF will read addresses from $A000-$BFFF
				if (hdma_src_addr >= 0xE000) {
					hdma_src_addr -= 0x4000;
				}
			}
			else if constexpr (reg == Reg::HDMA2) {
				// bits 0-3 are ignored (they are always set to 0)
				hdma_src_addr &= 0xFF00;
				hdma_src_addr |= data & 0xF0;
			}
			else if constexpr (reg == Reg::HDMA3) {
				// bits 5-7 are ignored (they are always set to 0). Dest is always in the range 0x8000-0x9FF0
				hdma_dst_addr &= 0xFF;
				hdma_dst_addr |= (data & 0x1F) << 8;
				hdma_dst_addr |= 0x8000;
			}
			else if constexpr (reg == Reg::HDMA4) {
				// bits 0-3 are ignored (they are always set to 0)
				hdma_dst_addr &= 0xFF00;
				hdma_dst_addr |= data & 0xF0;
			}
			else if constexpr (reg == Reg::HDMA5) {
				if (System::mode == System::Mode::CGB) {
					if (data & 0x80) { /* HDMA */
						if (!CPU::IsHalted() && !CPU::IsStopped()) {
							data &= 0x7F;
							StartCgbDmaTransfer<CgbDmaType::HDMA>(data);
						}
					}
					else { /* GDMA */
						// Stop HDMA copy if it is active. However, do not start GDMA transfer at this time.
						if (hdma_transfer_active) {
							hdma_transfer_active = false;
						}
						else {
							StartCgbDmaTransfer<CgbDmaType::GDMA>(data);
						}
					}
				}
			}
			else {
				static_assert(AlwaysFalse<reg>);
			}
		}
	}


	template u8 ReadReg<Reg::DMA>();
	template u8 ReadReg<Reg::HDMA1>();
	template u8 ReadReg<Reg::HDMA2>();
	template u8 ReadReg<Reg::HDMA3>();
	template u8 ReadReg<Reg::HDMA4>();
	template u8 ReadReg<Reg::HDMA5>();
	
	template void WriteReg<Reg::DMA>(u8);
	template void WriteReg<Reg::HDMA1>(u8);
	template void WriteReg<Reg::HDMA2>(u8);
	template void WriteReg<Reg::HDMA3>(u8);
	template void WriteReg<Reg::HDMA4>(u8);
	template void WriteReg<Reg::HDMA5>(u8);
}