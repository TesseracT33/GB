#include "APU.h"

using namespace Util;

///// WIP //////

void APU::Initialize()
{
	NR10 = bus->ReadIOPointer(Bus::Addr::NR10);
	NR11 = bus->ReadIOPointer(Bus::Addr::NR11);
	NR12 = bus->ReadIOPointer(Bus::Addr::NR12);
	NR13 = bus->ReadIOPointer(Bus::Addr::NR13);
	NR14 = bus->ReadIOPointer(Bus::Addr::NR14);

	NR21 = bus->ReadIOPointer(Bus::Addr::NR21);
	NR22 = bus->ReadIOPointer(Bus::Addr::NR22);
	NR23 = bus->ReadIOPointer(Bus::Addr::NR23);
	NR24 = bus->ReadIOPointer(Bus::Addr::NR24);

	NR30 = bus->ReadIOPointer(Bus::Addr::NR30);
	NR31 = bus->ReadIOPointer(Bus::Addr::NR31);
	NR32 = bus->ReadIOPointer(Bus::Addr::NR32);
	NR33 = bus->ReadIOPointer(Bus::Addr::NR33);
	NR34 = bus->ReadIOPointer(Bus::Addr::NR34);

	NR41 = bus->ReadIOPointer(Bus::Addr::NR41);
	NR42 = bus->ReadIOPointer(Bus::Addr::NR42);
	NR43 = bus->ReadIOPointer(Bus::Addr::NR43);
	NR44 = bus->ReadIOPointer(Bus::Addr::NR44);

	NR50 = bus->ReadIOPointer(Bus::Addr::NR50);
	NR51 = bus->ReadIOPointer(Bus::Addr::NR51);
	NR52 = bus->ReadIOPointer(Bus::Addr::NR52);
}


void APU::Reset()
{
	// todo
	audio_spec.freq = sample_rate;
	audio_spec.format = AUDIO_F32;
	audio_spec.channels = 2;
	audio_spec.samples = sample_buffer_size / 2;
	audio_spec.callback = NULL;

	SDL_AudioSpec obtainedSpec;
	SDL_OpenAudio(&audio_spec, &obtainedSpec);
	SDL_PauseAudio(0);
}


void APU::ResetAllRegisters()
{
	// zero all registers between NR10 and NR52
	for (u8* addr = NR10; addr <= NR52; addr++)
		*addr = 0;

	duty[CH1] = duty[CH2] = 0;
	wave_pos[CH1] = wave_pos[CH2] = wave_pos[CH3] = 0;
	freq_timer[CH1] = freq_timer[CH2] = freq_timer[CH3] = freq_timer[CH4] = 0;
	// todo: what other regs to reset?
}


void APU::WriteToReg(u16 addr, u8 data)
{
	// note: once this function is called, the actual register has already been written to (with data)
	switch (addr)
	{
	case Bus::Addr::NR10: // 0xFF10
		SetSweepParams();
		break;

	case Bus::Addr::NR11: // 0xFF11
		duty[CH1] = data >> 6;
		length_timer[CH1] = 64 - (data & 0x3F);
		break;

	case Bus::Addr::NR12: // 0xFF12
		SetEnvelopeParams(CH1);
		DAC_is_enabled[CH1] = data >> 3;
		// todo: argentum disables channel if dac is disabled
		break;

	case Bus::Addr::NR13: // 0xFF13
		freq[CH1] &= 0x700;
		freq[CH1] |= data;
		break;

	case Bus::Addr::NR14: // 0xFF14
		freq[CH1] &= 0xFF;
		freq[CH1] |= data << 8;

		length_is_enabled[CH1] = data & 0x40;
		if (length_timer[CH1] == 0)
			length_timer[CH1] = 64;

		if (CheckBit(data, 7) == 1 && DAC_is_enabled[CH1])
			Trigger(CH1);

		break;

	case Bus::Addr::NR21: // 0xFF16
		duty[CH2] = data >> 6;
		length_timer[CH2] = 64 - (data & 0x3F);
		break;

	case Bus::Addr::NR22: // 0xFF17
		SetEnvelopeParams(CH2);
		DAC_is_enabled[CH2] = data >> 3;
		break;

	case Bus::Addr::NR23: // 0xFF18
		freq[CH2] &= 0x700;
		freq[CH2] |= data;
		break;

	case Bus::Addr::NR24: // 0xFF19
		freq[CH2] &= 0xFF;
		freq[CH2] |= data << 8;

		length_is_enabled[CH2] = data & 0x40;
		if (length_timer[CH2] == 0)
			length_timer[CH2] = 64;

		if (CheckBit(data, 7) == 1 && DAC_is_enabled[CH2])
			Trigger(CH2);

		break;

	case Bus::Addr::NR30: // 0xFF1A
		DAC_is_enabled[CH3] = *NR30 >> 7;
		break;

	case Bus::Addr::NR31: // 0xFF1B
		length_timer[CH2] = 256 - data;
		break;

	case Bus::Addr::NR32: // 0xFF1C
		ch3_output_level = (data >> 5) & 3;
		break;

	case Bus::Addr::NR33: // 0xFF1D
		freq[CH3] &= 0x700;
		freq[CH3] |= data;
		break;

	case Bus::Addr::NR34: // 0xFF1E
		freq[CH3] &= 0xFF;
		freq[CH3] |= data << 8;

		length_is_enabled[CH3] = data & 0x40;
		if (length_timer[CH3] == 0)
			length_timer[CH3] = 256;

		if (CheckBit(data, 7) == 1 && DAC_is_enabled[CH3])
			Trigger(CH3);

		break;

	case Bus::Addr::NR41: // 0xFF20
		length_timer[CH4] = 64 - (data & 0x3F);
		break;

	case Bus::Addr::NR42: // 0xFF21
		SetEnvelopeParams(APU::Channel::CH4);
		DAC_is_enabled[CH4] = data >> 3;
		break;

	case Bus::Addr::NR43: // 0xFF22
		break;

	case Bus::Addr::NR44: // 0xFF23
		length_is_enabled[CH4] = data & 0x40;
		if (length_timer[CH4] == 0)
			length_timer[CH4] = 64;

		if (CheckBit(data, 7) == 1 && DAC_is_enabled[CH4])
			Trigger(CH4);

		break;

	case Bus::Addr::NR50: // 0xFF24
		break;

	case Bus::Addr::NR51: // 0xFF25
		break;

	case Bus::Addr::NR52: // 0xFF26
		// If bit 7 is reset, then all of the sound system is immediately shut off, and all audio regs are cleared
		this->enabled = data & 0x80;
		if (!this->enabled)
			ResetAllRegisters();
		break;
	}
}


void APU::Update()
{
	if (!enabled) return;

	for (int i = 0; i < 4; i++) // Update() is called each m-cycle, but apu is updated each t-cycle
	{
		// note: the frame sequencer is updated from the Timer class

		// update channel 1-4
		for (int i = 0; i < 4; i++)
		{
			if (freq_timer[i] > 0)
				freq_timer[i]--;
			if (freq_timer[i] == 0)
				StepChannel(static_cast<Channel>(i));
		}

		if (--t_cycles_until_sample == 0)
		{
			Sample();
			t_cycles_until_sample = t_cycles_per_sample;

			// To get exactly 48000 samples per second, 7 samples are taken 73 t-cycles apart.
			// The 8th sample is then taken 74 t-cycles later. The next 7 samples are taken 73 t-cycles apart, and so on.
			// In double speed mode, samples are instead taken 146 t-cycles apart. The 8th sample is taken 148 t-cycles later, and so on.
			//sample_counter++;
			//if (sample_counter == 6)
			//	t_cycles_until_sample += 2;
			//else if (sample_counter == 7)
			//	sample_counter = 0;
		}
	}
}


void APU::StepChannel(Channel channel)
{
	switch (channel)
	{
	case CH1:
	{
		u16 freq = *NR13 | (*NR14 & 7) << 8;
		freq_timer[CH1] = (2048 - freq) * 4;
		wave_pos[CH1] = (wave_pos[CH1] + 1) & 7;
		break;
	}

	case CH2:
	{
		u16 freq = *NR23 | (*NR24 & 7) << 8;
		freq_timer[CH2] = (2048 - freq) * 4;
		wave_pos[CH2] = (wave_pos[CH2] + 1) & 7;
		break;
	}

	case CH3:
	{
		u16 freq = *NR33 | (*NR34 & 7) << 8;
		freq_timer[CH3] = (2048 - freq) * 2;
		if (set_ch3_wave_pos_to_one_after_sample)
		{
			wave_pos[CH3] = 1;
			set_ch3_wave_pos_to_one_after_sample = false;
		}
		else
			wave_pos[CH3] = (wave_pos[CH3] + 1) & 31;
		break;
	}

	case CH4:
	{
		freq_timer[CH4] = ch4_divisors[*NR43 & 7] << (*NR43 >> 4);
		u8 xor_result = (LFSR & 1) ^ ((LFSR & 2) >> 1);
		LFSR = (LFSR >> 1) | (xor_result << 14);
		if (CheckBit(NR43, 3) == 1)
		{
			LFSR &= !(1 << 6);
			LFSR |= xor_result << 6;
		}
		break;
	}
	}
}


f32 APU::GetChannel1Amplitude()
{
	if (channel_is_enabled[CH1] && DAC_is_enabled[CH1])
		return square_wave_duty_table[duty[CH1]][wave_pos[CH1]] / 7.5f - 1.0f;
	return 0.0f;
}


f32 APU::GetChannel2Amplitude()
{
	if (channel_is_enabled[CH2] && DAC_is_enabled[CH2])
		return square_wave_duty_table[duty[CH2]][wave_pos[CH2]] / 7.5f - 1.0f;
	return 0.0f;
}


f32 APU::GetChannel3Amplitude()
{
	if (channel_is_enabled[CH3] && DAC_is_enabled[CH3])
	{
		u8 sample = bus->Read(Bus::Addr::WAV_START + wave_pos[CH3] / 2);
		if (wave_pos[CH3] % 2)
			sample &= 0xF;
		else
			sample >>= 4;

		sample >>= ch3_output_level_shift[(*NR32 & 3 << 5) >> 5];
		return sample / 7.5f - 1.0f;
	}
	return 0.0f;
}


f32 APU::GetChannel4Amplitude()
{
	if (channel_is_enabled[CH4] && DAC_is_enabled[CH4])
		return (~LFSR & 1) / 7.5f - 1.0f;
	return 0.0f;
}


void APU::StepFrameSequencer()
{
	// note: this function is called from the Timer class, as DIV increases

	if (frame_seq_step_counter % 2 == 0)
	{
		StepLength();
	}
	if (frame_seq_step_counter % 4 == 2)
	{
		//if (channel_is_enabled[CH1])
		//	StepSweep();
		StepSweep();
	}
	if (frame_seq_step_counter == 7)
	{
		// step volume envelope for each channel (1, 2, 4)
		//if (channel_is_enabled[CH1]) StepEnvelope(CH1);
		//if (channel_is_enabled[CH2]) StepEnvelope(CH2);
		//if (channel_is_enabled[CH4]) StepEnvelope(CH4);

		StepEnvelope(CH1);
		StepEnvelope(CH2);
		StepEnvelope(CH4);
	}

	frame_seq_step_counter = (frame_seq_step_counter + 1) & 7;
}


void APU::Trigger(Channel channel)
{
	EnableChannel(channel);

	//EnableLength(channel);

	if (channel == CH1)
		EnableSweep();

	if (channel != CH3)
		EnableEnvelope(channel);

	PrepareChannelAfterTrigger(channel);
}


void APU::PrepareChannelAfterTrigger(Channel channel)
{
	// note: channel = 0, 1, 2, 3
	u8* NRx3 = NR13 + 5 * channel;
	u8* NRx4 = NR14 + 5 * channel;

	freq_timer[channel] = *NRx3 | (*NRx4 & 7) << 8;

	// todo: correct?
	if (channel == CH1)
	{
		wave_pos[CH1] = 0;
	}
	else if (channel == CH2)
	{
		wave_pos[CH2] = 0;
	}
	else if (channel == CH3)
	{
		// When restarting CH3, it resumes playing the last 4-bit sample it read from wave RAM, or 0 if no sample has been read since APU reset.
		// After this, it starts with the second sample in wave RAM 
		set_ch3_wave_pos_to_one_after_sample = true;
	}
	else if (channel == CH4)
	{
		LFSR = 0x7FFF; // all (15) bits are set to 1
	}
}


void APU::EnableChannel(Channel channel)
{
	SetBit(NR52, channel);
	channel_is_enabled[channel] = true;
}


void APU::DisableChannel(Channel channel)
{
	ClearBit(NR52, channel);
	channel_is_enabled[channel] = false;
}


void APU::EnableEnvelope(Channel channel)
{
	// Note: channel = 0, 1, 3 (CH1, CH2, CH4). NR12, NR22, NR42 are stored in an array (BusImpl::memory),
	//   where NR12 has addr 0xFF12, NR22 has 0xFF17, NR42 has 0xFF21
	u8* NRx2 = NR12 + 5 * channel;

	SetEnvelopeParams(channel);
	volume[channel] = *NRx2 >> 4;
	envelope[channel].period_timer = envelope[channel].period;
}


void APU::SetEnvelopeParams(Channel channel)
{
	u8* NRx2 = NR12 + 5 * channel;
	envelope[channel].direction = CheckBit(NRx2, 3) == 0 ? Envelope::Direction::Downwards : Envelope::Direction::Upwards;
	envelope[channel].period = *NRx2 & 7;
}


void APU::StepEnvelope(Channel channel)
{
	if (envelope[channel].period != 0)
	{
		if (envelope[channel].period_timer > 0)
			envelope[channel].period_timer--;

		if (envelope[channel].period_timer == 0)
		{
			envelope[channel].period_timer = envelope[channel].period;
			if (volume[channel] < 0xF && envelope[channel].direction == Envelope::Direction::Upwards)
				volume[channel]++;
			else if (volume[channel] > 0x0 && envelope[channel].direction == Envelope::Direction::Downwards)
				volume[channel]--;
		}
	}
}


void APU::EnableSweep()
{
	SetSweepParams();
	sweep.shadow_freq = *NR13 | (*NR14 & 7) << 8;
	sweep.timer = sweep.period != 0 ? sweep.period : 8;
	sweep.enabled = sweep.period != 0 || sweep.shift != 0;

	if (sweep.shift != 0)
		ComputeNewSweepFreq();
}


void APU::SetSweepParams()
{
	sweep.period = (*NR10 >> 4) & 7;
	sweep.direction = CheckBit(NR10, 3) == 0 ? Sweep::Direction::Increase : Sweep::Direction::Decrease;
	sweep.shift = *NR10 & 7;
}


void APU::StepSweep()
{
	if (sweep.timer > 0)
		sweep.timer--;

	if (sweep.timer == 0)
	{
		sweep.timer = sweep.period > 0 ? sweep.period : 8;
		if (sweep.enabled && sweep.period > 0)
		{
			u16 new_freq = ComputeNewSweepFreq();
			if (new_freq < 2048 && sweep.shift > 0)
			{
				// update shadow frequency and CH1 frequency registers with new frequency
				sweep.shadow_freq = new_freq;
				*NR13 = new_freq & 0xFF;
				*NR14 &= ~7;
				*NR14 |= (new_freq & 7 << 8) >> 8;

				ComputeNewSweepFreq();
			}
		}
	}
}


u16 APU::ComputeNewSweepFreq()
{
	u16 new_freq = sweep.shadow_freq >> sweep.shift;

	if (sweep.direction == Sweep::Direction::Increase)
		new_freq = sweep.shadow_freq + new_freq;
	else
		new_freq = sweep.shadow_freq - new_freq;

	if (new_freq >= 2048)
		DisableChannel(CH1);

	return new_freq;
}


void APU::EnableLength(Channel channel)
{
	// the length timer is reloaded only if the current length timer is 0
	if (length_timer[channel] == 0)
	{
		if (channel == CH3)
		{
			length_timer[channel] = 256 - *NR31;
		}
		else
		{
			// Note: channel = 0, 1, 2, 3 ((CH1, CH2, CH3, CH4)). NR11, NR21, NR31, NR41 are stored in an array (BusImpl::memory),
			//   where NR11 has index 0xFF11, NR21 has 0xFF16, NR31 has 0xFF1B, NR41 has 0xFF20
			u8* NRx1 = NR11 + 5 * channel;
			length_timer[channel] = 64 - (*NRx1 & 0x3F);
		}
	}
}


void APU::StepLength()
{
	for (int i = 0; i < 4; i++)
	{
		if (CheckBit(NR14 + 5 * i, 6) == 1 && length_timer[i] > 0)
		{
			length_timer[i]--;
			if (length_timer[i] == 0)
				DisableChannel(static_cast<Channel>(i));
		}
	}
}


void APU::Sample()
{
	f32 right_output = 0.0f, left_output = 0.0f;

	ch_output[CH1] = GetChannel1Amplitude() * volume[CH1];
	ch_output[CH2] = GetChannel2Amplitude() * volume[CH2];
	ch_output[CH3] = GetChannel3Amplitude()              ;
	ch_output[CH4] = GetChannel4Amplitude() * volume[CH4];

	ch_output[CH2] = 0;
	ch_output[CH3] = 0;
	ch_output[CH4] = 0;

#ifdef CH1_DEBUG
	char buf[200]{};
	sprintf(buf, "amp: %f   vol: %i   out: %f   len: %i   freq: %i",
		GetChannel1Amplitude(), (int)volume[0], ch_output[CH1], (int)length_timer[0], (int)freq_timer[0]);
	ofs << buf << std::endl;
#endif

	for (int i = 0; i < 4; i++)
	{
		if (CheckBit(NR51, i) == 1)
			right_output += ch_output[i];
		if (CheckBit(NR51, i + 4) == 1)
			left_output += ch_output[i];
	}

	right_output /= 4.0f;
	left_output /= 4.0f;

	u8 right_vol = *NR50 & 7;
	u8 left_vol = (*NR50 & 7 << 4) >> 4;

	f32 left_sample = (left_vol / 7.0f) * left_output;
	f32 right_sample = (right_vol / 7.0f) * right_output;

	sample_buffer[sample_buffer_index++] = left_sample;
	sample_buffer[sample_buffer_index++] = right_sample;

	if (sample_buffer_index == sample_buffer_size)
	{
		while (SDL_GetQueuedAudioSize(1) > sample_buffer_size * sizeof(f32));
		SDL_QueueAudio(1, sample_buffer, sample_buffer_size * sizeof(f32));
		sample_buffer_index = 0;
	}
}


void APU::Deserialize(std::ifstream& ifs)
{

}


void APU::Serialize(std::ofstream& ofs)
{

}