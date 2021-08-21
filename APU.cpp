#include "APU.h"

using namespace Util;

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
	audio_spec.samples = sample_buffer_size;
	audio_spec.callback = NULL;

	SDL_AudioSpec obtainedSpec;
	SDL_OpenAudio(&audio_spec, &obtainedSpec);
	SDL_PauseAudio(0);

	ResetAllRegisters();

	// set wave ram pattern (https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware)
	u8* wav_start = bus->ReadIOPointer(Bus::Addr::WAV_START);
	const u8* source = (System::mode == System::Mode::DMG ? dmg_initial_wave_ram : cgb_initial_wave_ram);
	memcpy(wav_start, source, 0x10 * sizeof(u8));

	ch3_output_level = ch3_sample_buffer = frame_seq_step_counter = sample_counter = 0;
	duty[CH1] = duty[CH2] = 0;
}


void APU::ResetAllRegisters()
{
	// zero all registers between NR10 and NR52
	for (u8* addr = NR10; addr <= NR52; addr++)
		*addr = 0;

	for (u16 addr = Bus::Addr::NR10; addr <= Bus::Addr::NR44; addr++)
		WriteToAudioReg(addr, 0);

	wave_pos[CH1] = wave_pos[CH2] = wave_pos[CH3] = 0;

	for (int i = 0; i < 4; i++)
	{
		channel_is_enabled[i] = DAC_is_enabled[i] = length_is_enabled[i] = 0;
		volume[i] = freq_timer[i] = 0;
	}

	wave_ram_accessible_by_cpu_when_ch3_enabled = true;
	first_half_of_length_period = false;

	// todo: what other regs to reset?
}


void APU::WriteToAudioReg(u16 addr, u8 data)
{
	// Called from Bus. At that point, the actual register has already been written to (with data)
	// addr is between NR10 and NR52 (inclusive)
	switch (addr)
	{
	case Bus::Addr::NR10: // 0xFF10
		SetSweepParams();
		break;

	case Bus::Addr::NR11: // 0xFF11
		duty[CH1] = data >> 6;
		length[CH1] = data & 0x3F;
		length_counter[CH1] = 64 - length[CH1];
		break;

	case Bus::Addr::NR12: // 0xFF12
		SetEnvelopeParams(CH1);
		DAC_is_enabled[CH1] = data >> 3;
		if (!DAC_is_enabled[CH1])
			DisableChannel(CH1);
		break;

	case Bus::Addr::NR13: // 0xFF13
		freq[CH1] &= 0x700;
		freq[CH1] |= data;
		break;

	case Bus::Addr::NR14: // 0xFF14
	{
		freq[CH1] &= 0xFF;
		freq[CH1] |= (data & 7) << 8;

		bool enable_length = data & 0x40;

		if (first_half_of_length_period && enable_length &&
			!length_is_enabled[CH1] && length_counter[CH1] > 0)
		{
			length_counter[CH1]--;
			if (length_counter[CH1] == 0)
				DisableChannel(CH1);
		}
		length_is_enabled[CH1] = enable_length;

		if (data & 0x80)
			Trigger(CH1);

		break;
	}

	case Bus::Addr::NR21: // 0xFF16
		duty[CH2] = data >> 6;
		length[CH2] = data & 0x3F;
		length_counter[CH2] = 64 - length[CH2];
		break;

	case Bus::Addr::NR22: // 0xFF17
		SetEnvelopeParams(CH2);
		DAC_is_enabled[CH2] = data >> 3;
		if (!DAC_is_enabled[CH2])
			DisableChannel(CH2);
		break;

	case Bus::Addr::NR23: // 0xFF18
		freq[CH2] &= 0x700;
		freq[CH2] |= data;
		break;

	case Bus::Addr::NR24: // 0xFF19
	{
		freq[CH2] &= 0xFF;
		freq[CH2] |= (data & 7) << 8;

		bool enable_length = data & 0x40;

		if (first_half_of_length_period && enable_length &&
			!length_is_enabled[CH2] && length_counter[CH2] > 0)
		{
			length_counter[CH2]--;
			if (length_counter[CH2] == 0)
				DisableChannel(CH2);
		}
		length_is_enabled[CH2] = enable_length;

		if (data & 0x80)
			Trigger(CH2);

		break;
	}

	case Bus::Addr::NR30: // 0xFF1A
		DAC_is_enabled[CH3] = data >> 7;
		if (!DAC_is_enabled[CH3])
			DisableChannel(CH3);
		break;

	case Bus::Addr::NR31: // 0xFF1B
		length[CH3] = data;
		length_counter[CH3] = 256 - length[CH3];
		break;

	case Bus::Addr::NR32: // 0xFF1C
		ch3_output_level = (data >> 5) & 3;
		break;

	case Bus::Addr::NR33: // 0xFF1D
		freq[CH3] &= 0x700;
		freq[CH3] |= data;
		break;

	case Bus::Addr::NR34: // 0xFF1E
	{
		freq[CH3] &= 0xFF;
		freq[CH3] |= (data & 7) << 8;

		bool enable_length = data & 0x40;

		if (first_half_of_length_period && enable_length &&
			!length_is_enabled[CH3] && length_counter[CH3] > 0)
		{
			length_counter[CH3]--;
			if (length_counter[CH3] == 0)
				DisableChannel(CH3);
		}
		length_is_enabled[CH3] = enable_length;

		if (data & 0x80)
			Trigger(CH3);

		break;
	}

	case Bus::Addr::NR41: // 0xFF20
		length[CH4] = data & 0x3F;
		length_counter[CH4] = 64 - length[CH4];
		break;

	case Bus::Addr::NR42: // 0xFF21
		SetEnvelopeParams(APU::Channel::CH4);
		DAC_is_enabled[CH4] = data >> 3;
		if (!DAC_is_enabled[CH4])
			DisableChannel(CH4);
		break;

	case Bus::Addr::NR43: // 0xFF22
		break;

	case Bus::Addr::NR44: // 0xFF23
	{
		bool enable_length = data & 0x40;

		if (first_half_of_length_period && enable_length &&
			!length_is_enabled[CH4] && length_counter[CH4] > 0)
		{
			length_counter[CH4]--;
			if (length_counter[CH4] == 0)
				DisableChannel(CH4);
		}
		length_is_enabled[CH4] = enable_length;

		if (data & 0x80)
			Trigger(CH4);

		break;
	}

	case Bus::Addr::NR50: // 0xFF24
		break;

	case Bus::Addr::NR51: // 0xFF25
		break;

	case Bus::Addr::NR52: // 0xFF26
		// If bit 7 is reset, then all of the sound system is immediately shut off, and all audio regs are cleared
		data & 0x80 ? EnableAPU() : DisableAPU();
		break;

	default:
		break;
	}
}


u8 APU::ReadFromWaveRam(u16 addr)
{
	// Reading from wave ram BY THE CPU when the wave channel is enabled gives special behaviour
	// It is equivalent to accessing the current byte selected by the waveform position (https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware)
	if (this->enabled && channel_is_enabled[CH3])
	{
		if (!wave_ram_accessible_by_cpu_when_ch3_enabled)
			return 0xFF;
		addr = Bus::Addr::WAV_START + wave_pos[CH3] / 2;
	}
	return bus->Read(addr, false, true);
}


void APU::WriteToWaveRam(u16 addr, u8 data)
{
	if (this->enabled && channel_is_enabled[CH3])
	{
		if (!wave_ram_accessible_by_cpu_when_ch3_enabled)
			return;
		addr = Bus::Addr::WAV_START + wave_pos[CH3] / 2;
	}
	bus->Write(addr, data, false, true);
}


void APU::DisableAPU()
{
	ResetAllRegisters();
	this->enabled = false;
}


void APU::EnableAPU()
{
	// On CGB, length counters are reset when powered up.
	// On DMG, they are unaffected, and not clocked.
	// source: blargg test rom 'dmg_sound' -- '08-len ctr during power'
	if (System::mode == System::Mode::CGB)
	{
		for (int i = 0; i < 4; i++)
			length_counter[i] = 0;
	}

	frame_seq_step_counter = 0;
	this->enabled = true;
}


void APU::Update()
{
	for (int i = 0; i < 4; i++) // Update() is called each m-cycle, but apu is updated each t-cycle
	{
		if (--t_cycles_until_sample == 0)
		{
			Sample();
			t_cycles_until_sample = t_cycles_per_sample;
		}

		if (wave_ram_accessible_by_cpu_when_ch3_enabled)
		{
			t_cycles_since_ch3_read_wave_ram++;
			if (t_cycles_since_ch3_read_wave_ram == t_cycles_until_cpu_cant_read_wave_ram)
				wave_ram_accessible_by_cpu_when_ch3_enabled = false;
		}

		if (!enabled) continue;

		// note: the frame sequencer is updated from the Timer class

		// update channels 1-4
		if (freq_timer[CH1] > 0)
			freq_timer[CH1]--;
		if (freq_timer[CH1] == 0)
			StepChannel1();

		if (freq_timer[CH2] > 0)
			freq_timer[CH2]--;
		if (freq_timer[CH2] == 0)
			StepChannel2();

		if (freq_timer[CH3] > 0)
			freq_timer[CH3]--;
		if (freq_timer[CH3] == 0)
			StepChannel3();

		if (freq_timer[CH4] > 0)
			freq_timer[CH4]--;
		if (freq_timer[CH4] == 0)
			StepChannel4();
	}
}


void APU::StepChannel1()
{
	freq_timer[CH1] = (2048 - freq[CH1]) * 4;
	wave_pos[CH1] = (wave_pos[CH1] + 1) & 7;
}


void APU::StepChannel2()
{
	freq_timer[CH2] = (2048 - freq[CH2]) * 4;
	wave_pos[CH2] = (wave_pos[CH2] + 1) & 7;
}


void APU::StepChannel3()
{
	freq_timer[CH3] = (2048 - freq[CH3]) * 2;
	wave_pos[CH3] = (wave_pos[CH3] + 1) & 31;
	ch3_sample_buffer = bus->Read(Bus::Addr::WAV_START + wave_pos[CH3] / 2, false, true);

	wave_ram_accessible_by_cpu_when_ch3_enabled = true;
	t_cycles_since_ch3_read_wave_ram = 0;
}


void APU::StepChannel4()
{
	freq_timer[CH4] = ch4_divisors[*NR43 & 7] << (*NR43 >> 4);
	u8 xor_result = (LFSR & 1) ^ ((LFSR & 2) >> 1);
	LFSR = (LFSR >> 1) | (xor_result << 14);
	if (CheckBit(NR43, 3) == 1)
	{
		LFSR &= !(1 << 6);
		LFSR |= xor_result << 6;
	}
}


f32 APU::GetChannel1Amplitude()
{
	if (channel_is_enabled[CH1] && DAC_is_enabled[CH1])
		return square_wave_duty_table[duty[CH1]][wave_pos[CH1]] * volume[CH1] / 7.5f - 1.0f;
	return 0.0f;
}


f32 APU::GetChannel2Amplitude()
{
	if (channel_is_enabled[CH2] && DAC_is_enabled[CH2])
		return square_wave_duty_table[duty[CH2]][wave_pos[CH2]] * volume[CH2] / 7.5f - 1.0f;
	return 0.0f;
}


f32 APU::GetChannel3Amplitude()
{
	if (channel_is_enabled[CH3] && DAC_is_enabled[CH3])
	{
		u8 sample = ch3_sample_buffer;
		if (wave_pos[CH3] % 2)
			sample &= 0xF;
		else
			sample >>= 4;

		sample >>= ch3_output_level_shift[ch3_output_level];
		return sample / 7.5f - 1.0f;
	}
	return 0.0f;
}


f32 APU::GetChannel4Amplitude()
{
	if (channel_is_enabled[CH4] && DAC_is_enabled[CH4])
		return (~LFSR & 1) * volume[CH4] / 7.5f - 1.0f;
	return 0.0f;
}


void APU::StepFrameSequencer()
{
	// note: this function is called from the Timer class, as DIV increases
	if (frame_seq_step_counter % 2 == 0)
	{
		StepLength(CH1);
		StepLength(CH2);
		StepLength(CH3);
		StepLength(CH4);
	}
	if (frame_seq_step_counter % 4 == 2)
	{
		StepSweep();
	}
	if (frame_seq_step_counter == 7)
	{
		StepEnvelope(CH1);
		StepEnvelope(CH2);
		StepEnvelope(CH4);
	}

	first_half_of_length_period = !(frame_seq_step_counter & 1);

	frame_seq_step_counter = (frame_seq_step_counter + 1) & 7;
}


void APU::Trigger(Channel channel)
{
	DAC_is_enabled[channel] ? EnableChannel(channel) : DisableChannel(channel);

	if (channel == CH1)
		EnableSweep();

	if (channel != CH3)
		EnableEnvelope(channel);

	if (channel == CH4)
		LFSR = 0x7FFF; // all (15) bits are set to 1

	if (length_counter[channel] == 0)
	{
		length_counter[channel] = (channel == CH3) ? 256 : 64;
		if (length_is_enabled[channel] && first_half_of_length_period)
			length_counter[channel]--;
	}

	// When triggering a square channel, the low two bits of the frequency timer are NOT modified (https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware)
	if (channel <= CH2)
		freq_timer[channel] = (freq_timer[channel] & 3) | (freq[channel] & ~3);
	else
		freq_timer[channel] = freq[channel];

	if (channel != CH4)
		wave_pos[channel] = 0;
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
	envelope[channel].period_timer = envelope[channel].period;
	volume[channel] = envelope[channel].initial_volume;
	envelope[channel].is_updating = true;
}


void APU::SetEnvelopeParams(Channel channel)
{
	u8* NRx2 = NR12 + 5 * channel;

	Envelope::Direction new_direction = CheckBit(NRx2, 3) == 0 ? Envelope::Direction::Downwards : Envelope::Direction::Upwards;
	
	if (channel_is_enabled[channel])
	{
		if (envelope[channel].period == 0 && envelope[channel].is_updating
			|| envelope[channel].direction == Envelope::Direction::Downwards)
			volume[channel]++;

		if (new_direction != envelope[channel].direction)
			volume[channel] = 0x10 - volume[channel];

		volume[channel] &= 0xF;
	}

	envelope[channel].initial_volume = *NRx2 >> 4;
	envelope[channel].direction = new_direction;
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
			else
				envelope[channel].is_updating = false;
		}
	}
}


void APU::EnableSweep()
{
	SetSweepParams();
	sweep.shadow_freq = freq[CH1];
	sweep.timer = sweep.period != 0 ? sweep.period : 8;
	sweep.enabled = sweep.period != 0 || sweep.shift != 0;
	sweep.negate_has_been_used = false;

	if (sweep.shift > 0)
		ComputeNewSweepFreq();
}


void APU::SetSweepParams()
{
	sweep.period = (*NR10 >> 4) & 7;
	sweep.direction = CheckBit(NR10, 3) == 0 ? Sweep::Direction::Increase : Sweep::Direction::Decrease;
	sweep.shift = *NR10 & 7;

	if (sweep.direction == Sweep::Direction::Increase && sweep.negate_has_been_used)
		DisableChannel(CH1);
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
				*NR14 |= (new_freq >> 8) & 7;

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

	if (sweep.direction == Sweep::Direction::Decrease)
		sweep.negate_has_been_used = true;

	return new_freq;
}


void APU::StepLength(Channel channel)
{
	if (length_is_enabled[channel] && length_counter[channel] > 0)
	{
		length_counter[channel]--;
		if (length_counter[channel] == 0)
			DisableChannel(channel);
	}
}


void APU::Sample()
{
	f32 right_output = 0.0f, left_output = 0.0f;

	f32 ch_output[4];
	ch_output[CH1] = GetChannel1Amplitude();
	ch_output[CH2] = GetChannel2Amplitude();
	ch_output[CH3] = GetChannel3Amplitude();
	ch_output[CH4] = GetChannel4Amplitude();

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
		while (SDL_GetQueuedAudioSize(1) > sample_buffer_size * sizeof(f32) * 2);
		SDL_QueueAudio(1, sample_buffer, sample_buffer_size * sizeof(f32));
		sample_buffer_index = 0;
	}
}


void APU::State(Serialization::BaseFunctor& functor)
{
	functor.fun(&enabled, sizeof(bool));

	functor.fun(envelope, sizeof(Envelope) * std::size(envelope));
	functor.fun(&sweep, sizeof(Sweep));

	functor.fun(&wave_ram_accessible_by_cpu_when_ch3_enabled, sizeof(bool));
	functor.fun(&first_half_of_length_period, sizeof(bool));

	functor.fun(channel_is_enabled, sizeof(bool) * std::size(channel_is_enabled));
	functor.fun(DAC_is_enabled, sizeof(bool) * std::size(DAC_is_enabled));
	functor.fun(length_is_enabled, sizeof(bool) * std::size(length_is_enabled));

	functor.fun(&ch3_output_level, sizeof(u8));
	functor.fun(&ch3_sample_buffer, sizeof(u8));
	functor.fun(&frame_seq_step_counter, sizeof(u8));
	functor.fun(&sample_counter, sizeof(u8));

	functor.fun(duty, sizeof(u8) * std::size(duty));
	functor.fun(length, sizeof(u8) * std::size(length));
	functor.fun(volume, sizeof(u8) * std::size(volume));
	functor.fun(wave_pos, sizeof(u8) * std::size(wave_pos));

	functor.fun(&LFSR, sizeof(u16));

	functor.fun(freq, sizeof(u16) * std::size(freq));
	functor.fun(freq_timer, sizeof(u16) * std::size(freq_timer));
	functor.fun(length_counter, sizeof(u16) * std::size(length_counter));

	functor.fun(&sample_buffer_index, sizeof(unsigned));
	functor.fun(&t_cycles_per_sample, sizeof(unsigned));
	functor.fun(&t_cycles_until_sample, sizeof(unsigned));
	functor.fun(&t_cycles_since_ch3_read_wave_ram, sizeof(unsigned));

	functor.fun(sample_buffer, sizeof(f32) * std::size(sample_buffer));
}


void APU::Configure(Serialization::BaseFunctor& functor)
{

}


void APU::SetDefaultConfig()
{

}