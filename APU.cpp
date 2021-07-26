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
	audioSpec.freq = sample_rate;
	audioSpec.format = AUDIO_F32;
	audioSpec.channels = 2;
	audioSpec.samples = sample_size;
	audioSpec.callback = NULL;

	SDL_AudioSpec obtainedSpec;
	SDL_OpenAudio(&audioSpec, &obtainedSpec);
	SDL_PauseAudio(0);
}


void APU::Update()
{
	if (!enabled) return;

	for (int i = 0; i < 4; i++) // Update() is called each m-cycle, but apu is updated each t-cycle
	{
		// note: the frame sequencer is updated from the Timer class

		// update channel 1
		if (CheckBit(NR52, CH1) == 1 && --frequency_timer[CH1] == 0)
		{
			u16 freq = *NR13 | (*NR14 & 7) << 8;
			frequency_timer[CH1] = (2048 - freq) * 4;
			wave_duty_pos_1 = (wave_duty_pos_1 + 1) & 7;
			amplitude[CH1] = SQUARE_WAVE_DUTY_TABLE[channel_duty_1][wave_duty_pos_1];
		}

		// update channel 2
		if (CheckBit(NR52, CH2) == 1 && --frequency_timer[CH2] == 0)
		{
			u16 freq = *NR23 | (*NR24 & 7) << 8;
			frequency_timer[CH2] = (2048 - freq) * 4;
			wave_duty_pos_2 = (wave_duty_pos_2 + 1) & 7;
			amplitude[CH2] = SQUARE_WAVE_DUTY_TABLE[channel_duty_2][wave_duty_pos_2];
		}

		// update channel 3
		if (CheckBit(NR52, CH3) == 1 && --frequency_timer[CH3] == 0)
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
		}

		// update channel 4
		if (CheckBit(NR52, CH4) == 1 && --frequency_timer[CH4] == 0)
		{
			frequency_timer[CH4] = divisors[*NR43 & 7] << (*NR43 >> 4);

			u8 xor_result = (LFSR & 1) ^ ((LFSR & 2) >> 1);
			LFSR = (LFSR >> 1) | (xor_result << 14);
			if (CheckBit(NR43, 3) == 1)
			{
				LFSR &= !(1 << 6);
				LFSR |= xor_result << 6;
			}

			amplitude[CH4] = ~LFSR & 1;
		}

		// Sample. To get exactly 48000 samples per second, 7 samples are taken 73 t-cycles apart.
		// The 8th sample is then taken 74 t-cycles later. The next 7 samples are taken 73 t-cycles apart, and so on.
		// In double speed mode, samples are instead taken 146 t-cycles apart. The 8th sample is taken 148 t-cycles later, and so on.
		if (--t_cycles_until_sample == 0)
		{
			Mix_and_Pan();

			t_cycles_until_sample = t_cycles_per_sample;
			if (++sample_mod == 7)
			{
				t_cycles_until_sample += System::speed_mode;
				sample_mod = 0;
			}
		}
	}
}


void APU::Step_Frame_Sequencer()
{
	// note: this function is called from the Timer class, as DIV increases

	if (fs_step_mod % 2 == 0)
	{
		StepLength();
	}
	if (fs_step_mod % 4 == 2)
	{
		StepSweep();
	}
	if (fs_step_mod == 7)
	{
		// step volume envelope for each channel (1, 2, 4)
		if (envelope[CH1].active) StepEnvelope(CH1);
		if (envelope[CH2].active) StepEnvelope(CH2);
		if (envelope[CH4].active) StepEnvelope(CH4);
	}

	fs_step_mod = (fs_step_mod + 1) & 7;
}


void APU::EnableEnvelope(Channel channel)
{
	// Note: channel = 0, 1, 3 (CH1, CH2, CH4). NR12, NR22, NR42 are stored in an array (BusImpl::memory),
	//   where NR12 has addr 0xFF12, NR22 has 0xFF17, NR42 has 0xFF21
	u8* NRx2 = NR12 + 5 * channel;

	envelope[channel].initial_volume = (*NRx2 & 0xF0) >> 4;
	envelope[channel].direction = CheckBit(NRx2, 3);
	envelope[channel].period = *NRx2 & 7;
	envelope[channel].period_timer = envelope[channel].period;
	envelope[channel].current_volume = envelope[channel].initial_volume;
	envelope[channel].active = true;
}


void APU::DisableEnvelope(Channel channel)
{
	envelope[channel].active = false;
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
			if (envelope[channel].direction && envelope[channel].current_volume < 0xF)
				envelope[channel].current_volume++;
			else if (!envelope[channel].direction && envelope[channel].current_volume > 0)
				envelope[channel].current_volume--;
		}
	}
}


void APU::EnableSweep()
{
	sweep.period = (*NR10 & 7 << 4) >> 4;
	sweep.direction = CheckBit(NR10, 3);
	sweep.shift = *NR10 & 7;

	sweep.shadow_freq = *NR13 | (*NR14 & 7) << 8;
	sweep.timer = sweep.period != 0 ? sweep.period : 8;
	sweep.enabled = sweep.period != 0 || sweep.shift != 0;

	if (sweep.shift != 0)
		Sweep_Compute_New_Freq();
}


u16 APU::Sweep_Compute_New_Freq()
{
	u16 new_freq = sweep.shadow_freq >> sweep.shift;
	// sweep.direction == false means additions, == true means subtraction
	new_freq = sweep.direction ? sweep.shadow_freq - new_freq : sweep.shadow_freq + new_freq;
	if (new_freq >= 2048)
	{
		ClearBit(NR52, CH1); // disable channel 1
	}
	return new_freq;
}


void APU::StepSweep()
{
	if (sweep.timer > 0)
		sweep.timer--;

	if (sweep.timer != 0)
		return;

	sweep.timer = sweep.period > 0 ? sweep.period : 8;
	if (sweep.enabled && sweep.period > 0)
	{
		u16 new_freq = Sweep_Compute_New_Freq();
		if (new_freq < 2048 && sweep.shift > 0)
		{
			// update shadow frequency and CH1 frequency registers with new frequency
			sweep.shadow_freq = new_freq;
			*NR13 = new_freq & 0xFF;
			*NR14 &= ~7;
			*NR14 |= (new_freq & 7 << 8) >> 8;

			Sweep_Compute_New_Freq();
		}
	}
}


void APU::Mix_and_Pan()
{
	f32 right_amp = 0.0f, left_amp = 0.0f;

	if (CheckBit(NR52, CH1) == 1)
	{
		u8 ch1_output = amplitude[CH1] * (envelope[CH1].active ? envelope[CH1].current_volume : 1);
		if (CheckBit(NR51, CH1) == 1)
			right_amp += ch1_output;
		if (CheckBit(NR51, CH1 + 4) == 1)
			left_amp += ch1_output;
	}
	if (CheckBit(NR52, CH2) == 1)
	{
		u8 ch2_output = amplitude[CH2] * (envelope[CH2].active ? envelope[CH2].current_volume : 1);
		if (CheckBit(NR51, CH2) == 1)
			right_amp += ch2_output;
		if (CheckBit(NR51, CH2 + 4) == 1)
			left_amp += ch2_output;
	}
	if (CheckBit(NR52, CH3) == 1)
	{
		u8 ch3_output = amplitude[CH3] * channel_3_output_level[(*NR32 & 3 << 5) >> 5]; // not sure how this works yet
		if (CheckBit(NR51, CH3) == 1)
			right_amp += ch3_output;
		if (CheckBit(NR51, CH3 + 4) == 1)
			left_amp += ch3_output;
	}
	if (CheckBit(NR52, CH4) == 1)
	{
		u8 ch4_output = amplitude[CH4] * (envelope[CH4].active ? envelope[CH4].current_volume : 1);
		if (CheckBit(NR51, CH4) == 1)
			right_amp += ch4_output;
		if (CheckBit(NR51, CH4 + 4) == 1)
			left_amp += ch4_output;
	}

	right_amp *= 0.25f;
	left_amp *= 0.25f;

	u8 right_vol = *NR50 & 7;
	u8 left_vol = (*NR50 & 7 << 4) >> 4;

	f32 left_sample = left_vol * left_amp;
	f32 right_sample = right_vol * right_amp;

	sample_buffer[buffer_index++] = left_sample;
	sample_buffer[buffer_index++] = right_sample;

	if (buffer_index == sample_size)
	{
		while (SDL_GetQueuedAudioSize(1) > sample_size * sizeof(f32));
		SDL_QueueAudio(1, sample_buffer, sample_size * sizeof(f32));
		buffer_index = 0;
	}
}


void APU::EnableLength(Channel channel)
{
	// Note: channel = 0, 1, 2, 3 ((CH1, CH2, CH3, CH4)). NR11, NR21, NR31, NR41 are stored in an array (BusImpl::memory),
	//   where NR11 has index 0xFF11, NR21 has 0xFF16, NR31 has 0xFF1B, NR41 has 0xFF20

	// the length timer is reloaded only if the length timer is 0
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
	if (CheckBit(NR14, 6) == 1 && --length_timer[0] == 0) ClearBit(NR52, 0);
	if (CheckBit(NR24, 6) == 1 && --length_timer[1] == 0) ClearBit(NR52, 1);
	if (CheckBit(NR34, 6) == 1 && --length_timer[2] == 0) ClearBit(NR52, 2);
	if (CheckBit(NR44, 6) == 1 && --length_timer[3] == 0) ClearBit(NR52, 3);
}


void APU::Trigger(Channel channel)
{
	// note: channel = 0, 1, 2, 3 (CH1, CH2, CH3, CH4)

	SetBit(NR52, channel); // enable channel

	if (channel != CH3)
		EnableEnvelope(channel);

	if (channel == CH1)
		EnableSweep();

	EnableLength(channel);

	if (channel == CH4)
		LFSR = 0x7FFF; // all (15) bits are set to 1

	SetInitialFrequencyTimer(channel);
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


void APU::SetInitialFrequencyTimer(Channel channel)
{
	// note: channel = 0, 1, 2, 3
	u8* NRx3 = NR13 + 5 * channel;
	u8* NRx4 = NR14 + 5 * channel;

	frequency_timer[channel] = *NRx3 | (*NRx4 & 7) << 8;
	wave_duty_pos_1++;
}


void APU::Deserialize(std::ifstream& ifs)
{

}


void APU::Serialize(std::ofstream& ofs)
{

}