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
	audioSpec.freq = 48000;
	audioSpec.format = AUDIO_F32;
	audioSpec.channels = 2;
	audioSpec.samples = sample_buffer_size;
	audioSpec.callback = NULL;

	SDL_AudioSpec obtainedSpec;
	SDL_OpenAudio(&audioSpec, &obtainedSpec);
	SDL_PauseAudio(0);
}


void APU::ResetAllRegisters()
{
	*NR10 = *NR11 = *NR12 = *NR13 = *NR14 = 0;
	*NR21 = *NR22 = *NR23 = *NR24 = 0;
	*NR30 = *NR31 = *NR32 = *NR33 = *NR34 = 0;
	*NR41 = *NR42 = *NR43 = *NR44 = 0;
	*NR50 = *NR51 = *NR52 = 0;

	this->Reset();
}


void APU::Update()
{
	if (!enabled) return;

	for (int i = 0; i < 4; i++) // Update() is called each m-cycle, but apu is updated each t-cycle
	{
		// note: the frame sequencer is updated from the Timer class

		// update channel 1
		if (ChannelIsEnabled(CH1) && --frequency_timer[CH1] == 0)
			StepChannel1();

		// update channel 2
		if (ChannelIsEnabled(CH2) && --frequency_timer[CH2] == 0)
			StepChannel2();

		// update channel 3
		if (ChannelIsEnabled(CH3) && --frequency_timer[CH3] == 0)
			StepChannel3();

		// update channel 4
		if (ChannelIsEnabled(CH4) && --frequency_timer[CH4] == 0)
			StepChannel4();

		if (--t_cycles_until_sample == 0)
		{
			Sample();
			t_cycles_until_sample = t_cycles_per_sample;

			// To get exactly 48000 samples per second, 7 samples are taken 73 t-cycles apart.
			// The 8th sample is then taken 74 t-cycles later. The next 7 samples are taken 73 t-cycles apart, and so on.
			// In double speed mode, samples are instead taken 146 t-cycles apart. The 8th sample is taken 148 t-cycles later, and so on.
			sample_counter++;
			if (sample_counter == 7)
				t_cycles_until_sample += System::speed_mode;
			else if (sample_counter == 8)
				sample_counter = 0;
		}
	}
}


void APU::StepChannel1()
{
	u16 freq = *NR13 | (*NR14 & 7) << 8;
	frequency_timer[CH1] = (2048 - freq) * 4;
	wave_duty_pos_1 = (wave_duty_pos_1 + 1) & 7;
	amplitude[CH1] = square_wave_duty_table[channel_duty_1][wave_duty_pos_1];
}


void APU::StepChannel2()
{
	u16 freq = *NR23 | (*NR24 & 7) << 8;
	frequency_timer[CH2] = (2048 - freq) * 4;
	wave_duty_pos_2 = (wave_duty_pos_2 + 1) & 7;
	amplitude[CH2] = square_wave_duty_table[channel_duty_2][wave_duty_pos_2];
}


void APU::StepChannel3()
{
	// todo: where to check for channel enabled in NR52 or NR30?
	u16 freq = *NR33 | (*NR34 & 7) << 8;
	frequency_timer[CH3] = (2048 - freq) * 8;

	if (CheckBit(NR30, 7) == 1)
	{
		wave_duty_pos_3 = (wave_duty_pos_3 + 1) & 3;
		if (wave_duty_pos_3 == 0)
		{
			if (channel_3_high_nibble_played)
				channel_3_byte_index = (channel_3_byte_index + 1) & 0xF;
			channel_3_high_nibble_played = !channel_3_high_nibble_played;
		}
		u8 sample = bus->Read(Bus::Addr::WAV_START + channel_3_byte_index);
		u8 start_pos = channel_3_high_nibble_played ? 0 : 4;
		amplitude[CH3] = CheckBit(sample, start_pos + wave_duty_pos_3);
	}
	else
	{
		amplitude[CH3] = 0;
	}
}


void APU::StepChannel4()
{
	frequency_timer[CH4] = ch4_divisors[*NR43 & 7] << (*NR43 >> 4);

	u8 xor_result = (LFSR & 1) ^ ((LFSR & 2) >> 1);
	LFSR = (LFSR >> 1) | (xor_result << 14);
	if (CheckBit(NR43, 3) == 1)
	{
		LFSR &= !(1 << 6);
		LFSR |= xor_result << 6;
	}

	amplitude[CH4] = ~LFSR & 1;
}


void APU::StepFrameSequencer()
{
	// note: this function is called from the Timer class, as DIV increases

	if (fs_step_mod % 2 == 0)
	{
		StepLength();
	}
	if (fs_step_mod % 4 == 2)
	{
		if (ChannelIsEnabled(CH1))
			StepSweep();
	}
	if (fs_step_mod == 7)
	{
		// step volume envelope for each channel (1, 2, 4)
		if (ChannelIsEnabled(CH1)) StepEnvelope(CH1);
		if (ChannelIsEnabled(CH2)) StepEnvelope(CH1);
		if (ChannelIsEnabled(CH4)) StepEnvelope(CH1);
	}

	fs_step_mod = (fs_step_mod + 1) & 7;
}


void APU::Trigger(Channel channel)
{
	EnableChannel(channel);

	if (channel != CH3)
		EnableEnvelope(channel);

	if (channel == CH1)
		EnableSweep();

	EnableLength(channel);

	PrepareChannelAfterTrigger(channel);
}


void APU::PrepareChannelAfterTrigger(Channel channel)
{
	// note: channel = 0, 1, 2, 3
	u8* NRx3 = NR13 + 5 * channel;
	u8* NRx4 = NR14 + 5 * channel;

	frequency_timer[channel] = *NRx3 | (*NRx4 & 7) << 8;

	// todo: correct?
	if (channel == CH1)
	{
		wave_duty_pos_1 = 0;
		amplitude[channel] = square_wave_duty_table[channel_duty_1][0];
	}
	else if (channel == CH2)
	{
		wave_duty_pos_2 = 0;
		amplitude[channel] = square_wave_duty_table[channel_duty_2][0];
	}
	else if (channel == CH4)
		LFSR = 0x7FFF; // all (15) bits are set to 1
}


void APU::EnableChannel(Channel channel)
{
	SetBit(NR52, channel);
}


void APU::DisableChannel(Channel channel)
{
	ClearBit(NR52, channel);
}


void APU::EnableEnvelope(Channel channel)
{
	// Note: channel = 0, 1, 3 (CH1, CH2, CH4). NR12, NR22, NR42 are stored in an array (BusImpl::memory),
	//   where NR12 has addr 0xFF12, NR22 has 0xFF17, NR42 has 0xFF21
	u8* NRx2 = NR12 + 5 * channel;

	volume[channel] = *NRx2 >> 4;
	envelope[channel].direction = CheckBit(NRx2, 3) == 0 ? Envelope::Direction::Downwards : Envelope::Direction::Upwards;
	envelope[channel].period = *NRx2 & 7;
	envelope[channel].period_timer = envelope[channel].period;
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
	sweep.period = (*NR10 & 7 << 4) >> 4;
	sweep.direction = CheckBit(NR10, 3) == 0 ? Sweep::Direction::Increase : Sweep::Direction::Decrease;
	sweep.shift = *NR10 & 7;

	sweep.shadow_freq = *NR13 | (*NR14 & 7) << 8;
	sweep.timer = sweep.period != 0 ? sweep.period : 8;
	sweep.enabled = sweep.period != 0 || sweep.shift != 0;

	if (sweep.shift != 0)
		ComputeNewSweepFreq();
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
	// Note: channel = 0, 1, 2, 3 ((CH1, CH2, CH3, CH4)). NR11, NR21, NR31, NR41 are stored in an array (BusImpl::memory),
	//   where NR11 has index 0xFF11, NR21 has 0xFF16, NR31 has 0xFF1B, NR41 has 0xFF20

	// the length timer is reloaded only if the current length timer is 0
	if (length_timer[channel] != 0)
		return;

	if (channel == CH3)
	{
		length_timer[channel] = 256 - *NR31;
	}
	else
	{
		u8* NRx1 = NR11 + 5 * channel;
		length_timer[channel] = 64 - (*NRx1 & 0x3F);
	}
}


void APU::StepLength()
{
	if (ChannelIsEnabled(CH1) && CheckBit(NR14, 6) == 1 && --length_timer[CH1] == 0) DisableChannel(CH1);
	if (ChannelIsEnabled(CH2) && CheckBit(NR24, 6) == 1 && --length_timer[CH2] == 0) DisableChannel(CH2);
	if (ChannelIsEnabled(CH3) && CheckBit(NR34, 6) == 1 && --length_timer[CH3] == 0) DisableChannel(CH3);
	if (ChannelIsEnabled(CH4) && CheckBit(NR44, 6) == 1 && --length_timer[CH4] == 0) DisableChannel(CH4);
}


void APU::Sample()
{
	f32 right_amp = 0.0f, left_amp = 0.0f;

	for (int i = 0; i < 4; i++)
	{
		if (ChannelIsEnabled(i))
		{
			u8 ch_output = amplitude[i] * volume[i];
			if (CheckBit(NR51, i) == 1)
				right_amp += ch_output;
			if (CheckBit(NR51, i + 4) == 1)
				left_amp += ch_output;
		}
	}

	right_amp *= 0.25f;
	left_amp *= 0.25f;

	u8 right_vol = *NR50 & 7;
	u8 left_vol = (*NR50 & 7 << 4) >> 4;

	f32 left_sample = left_vol * left_amp;
	f32 right_sample = right_vol * right_amp;

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