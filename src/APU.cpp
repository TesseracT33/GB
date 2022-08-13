module APU;

import Bus;
import System;

import Audio;

import Util;

namespace APU
{
	bool Enabled()
	{
		return apu_enabled;
	}


	template<Reg reg>
	u8 ReadReg()
	{
		using enum Reg;
		if constexpr (reg == NR10) return nr10 | 0x80;
		if constexpr (reg == NR11) return nr11 | 0x3F;
		if constexpr (reg == NR14) return nr14 | 0xBF;
		if constexpr (reg == NR21) return nr21 | 0x3F;
		if constexpr (reg == NR24) return nr24 | 0xBF;
		if constexpr (reg == NR30) return nr30 | 0x7F;
		if constexpr (reg == NR32) return nr32 | 0x9F;
		if constexpr (reg == NR34) return nr34 | 0xBF;
		if constexpr (reg == NR44) return nr44 | 0xBF;
		if constexpr (reg == NR52) return nr52 | 0x70;
		return 0xFF;
	}


	template<Reg reg>
	void WriteReg(u8 data)
	{
		using enum Reg;

		if constexpr (reg == NR52) {
			// If bit 7 is reset, then all of the sound system is immediately shut off, and all audio regs are cleared
			nr52 = data;
			nr52 & 0x80 ? EnableAPU() : DisableAPU();
		}
		else if (!apu_enabled) {
			/* When powered off, writes to registers NR10-NR51 are ignored while power remains off,
			except on the DMG, where length counters are unaffected by power and can still be written while off. */
			if constexpr (reg == NR11) {
				if (System::mode == System::Mode::DMG) {
					nr11 = nr11 & 0xC0 | data & 0x3F;
					pulse_ch_1.length_counter.length = data & 0x3F;
					pulse_ch_1.length_counter.value = 64 - pulse_ch_1.length_counter.length;
				}
			}
			if constexpr (reg == NR21) {
				if (System::mode == System::Mode::DMG) {
					nr21 = nr21 & 0xC0 | data & 0x3F;
					pulse_ch_2.length_counter.length = data & 0x3F;
					pulse_ch_2.length_counter.value = 64 - pulse_ch_2.length_counter.length;
				}
			}
			if constexpr (reg == NR31) {
				if (System::mode == System::Mode::DMG) {
					nr31 = nr31 & 0xC0 | data & 0x3F;
					wave_ch.length_counter.length = data & 0x3F;
					wave_ch.length_counter.value = 256 - wave_ch.length_counter.length;
				}
			}
			if constexpr (reg == NR41) {
				if (System::mode == System::Mode::DMG) {
					nr41 = nr41 & 0xC0 | data & 0x3F;
					noise_ch.length_counter.length = data & 0x3F;
					noise_ch.length_counter.value = 64 - noise_ch.length_counter.length;
				}
			}
		}
		else {
			if constexpr (reg == NR10) {
				nr10 = data;
				pulse_ch_1.sweep.period = nr10 >> 4 & 7;
				pulse_ch_1.sweep.direction = nr10 & 8 ? Direction::Decreasing : Direction::Increasing;
				pulse_ch_1.sweep.shift = nr10 & 7;
				if (pulse_ch_1.sweep.direction == Direction::Increasing && pulse_ch_1.sweep.negate_has_been_used) {
					pulse_ch_1.Disable();
				}
			}
			if constexpr (reg == NR11) {
				nr11 = data;
				pulse_ch_1.duty = data >> 6;
				pulse_ch_1.length_counter.length = data & 0x3F;
				pulse_ch_1.length_counter.value = 64 - pulse_ch_1.length_counter.length;
			}
			if constexpr (reg == NR12) {
				nr12 = data;
				pulse_ch_1.envelope.SetParams(data);
				pulse_ch_1.dac_enabled = data & 0xF8;
				if (!pulse_ch_1.dac_enabled) {
					pulse_ch_1.Disable();
				}
			}
			if constexpr (reg == NR13) {
				nr13 = data;
				pulse_ch_1.freq &= 0x700;
				pulse_ch_1.freq |= data;
			}
			if constexpr (reg == NR14) {
				nr14 = data;
				pulse_ch_1.freq &= 0xFF;
				pulse_ch_1.freq |= (data & 7) << 8;
				// Extra length clocking occurs when writing to NRx4 when the frame sequencer's next step 
				// is one that doesn't clock the length counter. In this case, if the length counter was 
				// PREVIOUSLY disabled and now enabled and the length counter is not zero, it is decremented. 
				// If this decrement makes it zero and trigger is clear, the channel is disabled.
				// https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior
				bool enable_length = data & 0x40;
				bool trigger = data & 0x80;
				if (enable_length && !pulse_ch_1.length_counter.enabled && pulse_ch_1.length_counter.value > 0 && frame_seq_step_counter & 1 && !trigger) {
					if (--pulse_ch_1.length_counter.value == 0) {
						pulse_ch_1.Disable();
					}
				}
				pulse_ch_1.length_counter.enabled = enable_length;
				if (trigger) {
					pulse_ch_1.Trigger();
				}
			}
			if constexpr (reg == NR21) {
				nr21 = data;
				pulse_ch_2.duty = data >> 6;
				pulse_ch_2.length_counter.length = data & 0x3F;
				pulse_ch_2.length_counter.value = 64 - pulse_ch_2.length_counter.length;
			}
			if constexpr (reg == NR22) {
				nr22 = data;
				pulse_ch_2.envelope.SetParams(data);
				pulse_ch_2.dac_enabled = data & 0xF8;
				if (!pulse_ch_2.dac_enabled) {
					pulse_ch_2.Disable();
				}
			}
			if constexpr (reg == NR23) {
				nr23 = data;
				pulse_ch_2.freq &= 0x700;
				pulse_ch_2.freq |= data;
			}
			if constexpr (reg == NR24) {
				nr24 = data;
				pulse_ch_2.freq &= 0xFF;
				pulse_ch_2.freq |= (data & 7) << 8;
				bool enable_length = data & 0x40;
				bool trigger = data & 0x80;
				if (enable_length && !pulse_ch_2.length_counter.enabled && pulse_ch_2.length_counter.value > 0 && frame_seq_step_counter & 1 && !trigger) {
					if (--pulse_ch_2.length_counter.value == 0) {
						pulse_ch_2.Disable();
					}
				}
				pulse_ch_2.length_counter.enabled = enable_length;
				if (trigger) {
					pulse_ch_2.Trigger();
				}
			}
			if constexpr (reg == NR30) {
				nr30 = data;
				wave_ch.dac_enabled = data & 0x80;
				if (!wave_ch.dac_enabled) {
					wave_ch.Disable();
				}
			}
			if constexpr (reg == NR31) {
				nr31 = data;
				wave_ch.length_counter.length = data & 0x3F;
				wave_ch.length_counter.value = 256 - wave_ch.length_counter.length;
			}
			if constexpr (reg == NR32) {
				nr32 = data;
				wave_ch.output_level = data >> 5 & 3;
			}
			if constexpr (reg == NR33) {
				nr33 = data;
				wave_ch.freq &= 0x700;
				wave_ch.freq |= data;
			}
			if constexpr (reg == NR34) {
				nr34 = data;
				wave_ch.freq &= 0xFF;
				wave_ch.freq |= (data & 7) << 8;
				bool enable_length = data & 0x40;
				bool trigger = data & 0x80;
				if (enable_length && !wave_ch.length_counter.enabled && wave_ch.length_counter.value > 0 && frame_seq_step_counter & 1 && !trigger) {
					if (--wave_ch.length_counter.value == 0) {
						wave_ch.Disable();
					}
				}
				wave_ch.length_counter.enabled = enable_length;
				if (trigger) {
					wave_ch.Trigger();
				}
			}
			if constexpr (reg == NR41) {
				nr41 = data;
				noise_ch.length_counter.length = data & 0x3F;
				noise_ch.length_counter.value = 64 - noise_ch.length_counter.length;
			}
			if constexpr (reg == NR42) {
				nr42 = data;
				noise_ch.envelope.SetParams(data);
				noise_ch.dac_enabled = data & 0xF8;
				if (!noise_ch.dac_enabled) {
					noise_ch.Disable();
				}
			}
			if constexpr (reg == NR43) {
				nr43 = data;
			}
			if constexpr (reg == NR44) {
				nr44 = data;
				bool enable_length = data & 0x40;
				bool trigger = data & 0x80;
				if (enable_length && !noise_ch.length_counter.enabled && noise_ch.length_counter.value > 0 && frame_seq_step_counter & 1 && !trigger) {
					if (--noise_ch.length_counter.value == 0) {
						noise_ch.Disable();
					}
				}
				noise_ch.length_counter.enabled = enable_length;
				if (trigger) {
					noise_ch.Trigger();
				}
			}
			if constexpr (reg == NR50) {
				nr50 = data;
			}
			if constexpr (reg == NR51) {
				nr51 = data;
			}
		}
	}


	void Initialize()
	{
		ResetAllRegisters();
		pulse_ch_1.Initialize();
		pulse_ch_2.Initialize();
		wave_ch.Initialize();
		noise_ch.Initialize();
		wave_ram_accessible_by_cpu_when_ch3_enabled = true;
		frame_seq_step_counter = 0;
		ApplyNewSampleRate();

		// set initial wave ram pattern (https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware)
		static constexpr std::array<u8, 0x10> initial_wave_ram_cgb = {
			0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
			0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF
		};
		static constexpr std::array<u8, 0x10> initial_wave_ram_dmg = {
			0x84, 0x40, 0x43, 0xAA, 0x2D, 0x78, 0x92, 0x3C,
			0x60, 0x59, 0x59, 0xB0, 0x34, 0xB8, 0x2E, 0xDA
		};
		const auto* source = System::mode == System::Mode::DMG 
			? initial_wave_ram_dmg.data()
			: initial_wave_ram_cgb.data();
		std::memcpy(wave_ram.data(), source, sizeof(wave_ram));
	}


	void ApplyNewSampleRate()
	{
		sample_rate = Audio::GetSampleRate();
		t_cycle_sample_counter = 0;
	}


	void ResetAllRegisters()
	{
		using enum Reg;
		WriteReg<NR10>(0); WriteReg<NR11>(0); WriteReg<NR12>(0); WriteReg<NR13>(0); WriteReg<NR14>(0);
		WriteReg<NR21>(0); WriteReg<NR22>(0); WriteReg<NR23>(0); WriteReg<NR24>(0);
		WriteReg<NR30>(0); WriteReg<NR31>(0); WriteReg<NR32>(0); WriteReg<NR33>(0); WriteReg<NR34>(0);
		WriteReg<NR41>(0); WriteReg<NR42>(0); WriteReg<NR43>(0); WriteReg<NR44>(0);
		WriteReg<NR50>(0); WriteReg<NR51>(0);
		nr52 = 0; /* Do not use WriteReg, as it will make a call to ResetAllRegisters again when 0 is written to nr52. */
	}


	u8 ReadWaveRamCpu(u16 addr)
	{
		addr &= 0xF;
		// If the wave channel is enabled, accessing any byte from $FF30-$FF3F 
		// is equivalent to accessing the current byte selected by the waveform position.
		// https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware
		if (apu_enabled && wave_ch.enabled) {
			if (wave_ram_accessible_by_cpu_when_ch3_enabled) {
				return wave_ram[wave_ch.wave_pos / 2]; /* each byte encodes two samples */
			}
			else {
				return 0xFF;
			}
		}
		else {
			return wave_ram[addr];
		}
	}


	void WriteWaveRamCpu(u16 addr, u8 data)
	{
		addr &= 0xF;
		if (apu_enabled && wave_ch.enabled) {
			if (wave_ram_accessible_by_cpu_when_ch3_enabled) {
				wave_ram[wave_ch.wave_pos / 2] = data; /* each byte encodes two samples */
			}
		}
		else {
			wave_ram[addr] = data;
		}
	}


	void DisableAPU()
	{
		ResetAllRegisters();
		apu_enabled = false;
		pulse_ch_1.wave_pos = pulse_ch_2.wave_pos = 0;
		wave_ch.sample_buffer = 0;
		frame_seq_step_counter = 0;
	}


	void EnableAPU()
	{
		// On CGB, length counters are reset when powered up.
		// On DMG, they are unaffected, and not clocked.
		// source: blargg test rom 'dmg_sound' -- '08-len ctr during power'
		if (System::mode == System::Mode::CGB) {
			pulse_ch_1.length_counter.value = 0;
			pulse_ch_2.length_counter.value = 0;
			wave_ch.length_counter.value = 0;
			noise_ch.length_counter.value = 0;
		}
		frame_seq_step_counter = 0;
		apu_enabled = true;
	}


	void Update()
	{
		// Update() is called each m-cycle, but apu is updated each t-cycle
		for (int i = 0; i < 4; ++i) {
			t_cycle_sample_counter += sample_rate;
			if (t_cycle_sample_counter >= System::t_cycles_per_sec_base) {
				Sample();
				t_cycle_sample_counter -= System::t_cycles_per_sec_base;
			}
			if (wave_ram_accessible_by_cpu_when_ch3_enabled) {
				t_cycles_since_ch3_read_wave_ram++;
				static constexpr int t_cycles_until_cpu_cant_read_wave_ram = 3;
				if (t_cycles_since_ch3_read_wave_ram == t_cycles_until_cpu_cant_read_wave_ram) {
					wave_ram_accessible_by_cpu_when_ch3_enabled = false;
				}
			}
			if (!apu_enabled) {
				continue;
			}
			pulse_ch_1.Step();
			pulse_ch_1.Step();
			wave_ch.Step();
			noise_ch.Step();
			// note: the frame sequencer is updated from the Timer module
		}
	}


	template<uint id>
	void PulseChannel<id>::Step()
	{
		if (timer == 0) {
			timer = (2048 - freq) * 4;
			wave_pos = (wave_pos + 1) & 7;
		}
		else {
			--timer;
		}
	}


	void WaveChannel::Step()
	{
		if (timer == 0) {
			timer = (2048 - freq) * 2;
			wave_pos = (wave_pos + 1) & 0x1F;
			sample_buffer = wave_ram[wave_pos / 2];
			wave_ram_accessible_by_cpu_when_ch3_enabled = true;
			t_cycles_since_ch3_read_wave_ram = 0;
		}
		else {
			--timer;
		}
	}


	void NoiseChannel::Step()
	{
		if (timer == 0) {
			static constexpr std::array divisor_table = {
				8, 16, 32, 48, 64, 80, 96, 112
			};
			auto divisor_code = nr43 & 7;
			auto clock_shift = nr43 >> 4;
			timer = divisor_table[divisor_code] << clock_shift;
			bool xor_result = (lfsr & 1) ^ (lfsr >> 1 & 1);
			lfsr = lfsr >> 1 | xor_result << 14;
			if (nr43 & 8) {
				lfsr &= ~(1 << 6);
				lfsr |= xor_result << 6;
			}
		}
		else {
			--timer;
		}
	}


	template<uint id>
	f32 PulseChannel<id>::GetOutput()
	{
		static constexpr std::array duty_table = {
			0, 0, 0, 0, 0, 0, 0, 1,
			1, 0, 0, 0, 0, 0, 0, 1,
			1, 0, 0, 0, 0, 1, 1, 1,
			0, 1, 1, 1, 1, 1, 1, 0
		};
		return enabled * dac_enabled * volume * duty_table[8 * duty + wave_pos] / 7.5f - 1.0f;
	}


	f32 WaveChannel::GetOutput()
	{
		if (enabled && dac_enabled) {
			auto sample = sample_buffer;
			if (wave_pos & 1) {
				sample &= 0xF;
			}
			else {
				sample >>= 4;
			}
			static constexpr std::array output_level_shift = { 4, 0, 1, 2 };
			sample >>= output_level_shift[output_level];
			return sample / 7.5f - 1.0f;
		} else {
			return 0.0f;
		}
	}


	f32 NoiseChannel::GetOutput()
	{
		return enabled * dac_enabled * volume * (~lfsr & 1) / 7.5f - 1.0f;
	}


	void StepFrameSequencer()
	{
		// note: this function is called from the Timer module as DIV increases
		if (frame_seq_step_counter % 2 == 0) {
			pulse_ch_1.length_counter.Clock();
			pulse_ch_2.length_counter.Clock();
			wave_ch.length_counter.Clock();
			noise_ch.length_counter.Clock();
			if (frame_seq_step_counter % 4 == 2) {
				pulse_ch_1.sweep.Clock();
			}
		}
		else if (frame_seq_step_counter == 7) {
			pulse_ch_1.envelope.Clock();
			pulse_ch_2.envelope.Clock();
			noise_ch.envelope.Clock();
		}
		frame_seq_step_counter = (frame_seq_step_counter + 1) & 7;
	}


	void Envelope::Enable()
	{
		timer = period;
		is_updating = true;
		ch->volume = initial_volume;
	}


	void Envelope::SetParams(u8 data)
	{
		// "Zombie" mode
		// https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior
		Direction new_direction = data & 8 ? Direction::Increasing : Direction::Decreasing;
		if (ch->enabled) {
			if (period == 0 && is_updating || direction == Direction::Decreasing) {
				ch->volume++;
			}
			if (new_direction != direction) {
				ch->volume = 0x10 - ch->volume;
			}
			ch->volume &= 0xF;
		}
		initial_volume = data >> 4;
		direction = new_direction;
		period = data & 7;
	}


	void Envelope::Clock()
	{
		if (period != 0) {
			if (timer > 0) {
				timer--;
			}
			if (timer == 0) {
				timer = period > 0 ? period : 8;
				if (ch->volume < 0xF && direction == Direction::Increasing) {
					ch->volume++;
				}
				else if (ch->volume > 0x0 && direction == Direction::Decreasing) {
					ch->volume--;
				}
				else {
					is_updating = false;
				}
			}
		}
	}


	void Sweep::Enable()
	{
		shadow_freq = ch->freq;
		timer = period > 0 ? period : 8;
		enabled = period != 0 || shift != 0;
		negate_has_been_used = false;
		if (shift > 0) {
			ComputeNewFreq();
		}
	}


	void Sweep::Clock()
	{
		if (timer > 0) {
			timer--;
		}
		if (timer == 0) {
			timer = period > 0 ? period : 8;
			if (enabled && period > 0) {
				auto new_freq = ComputeNewFreq();
				if (new_freq < 2048 && shift > 0) {
					// update shadow frequency and CH1 frequency registers with new frequency
					shadow_freq = new_freq;
					nr13 = new_freq & 0xFF;
					nr14 = (new_freq >> 8) & 7;
					ComputeNewFreq();
				}
			}
		}
	}


	uint Sweep::ComputeNewFreq()
	{
		uint new_freq = shadow_freq >> shift;
		if (direction == Direction::Increasing) {
			new_freq = shadow_freq + new_freq;
		}
		else {
			new_freq = shadow_freq - new_freq;
		}
		if (new_freq >= 2048) {
			ch->Disable();
		}
		if (direction == Direction::Decreasing) {
			negate_has_been_used = true;
		}
		return new_freq;
	}


	void LengthCounter::Clock()
	{
		if (enabled && value > 0) {
			if (--value == 0) {
				ch->Disable();
			}
		}
	}


	void Sample()
	{
		f32 right_output = 0.0f, left_output = 0.0f;

		if (nr51 & 0x11) {
			f32 pulse_ch_1_output = pulse_ch_1.GetOutput();
			right_output += pulse_ch_1_output * (nr51 & 1);
			left_output += pulse_ch_1_output * (nr51 >> 4 & 1);
		}
		if (nr51 & 0x22) {
			f32 pulse_ch_2_output = pulse_ch_2.GetOutput();
			right_output += pulse_ch_2_output * (nr51 >> 1 & 1);
			left_output += pulse_ch_2_output * (nr51 >> 5 & 1);
		}
		if (nr51 & 0x44) {
			f32 wave_ch_output = wave_ch.GetOutput();
			right_output += wave_ch_output * (nr51 >> 2 & 1);
			left_output += wave_ch_output * (nr51 >> 6 & 1);
		}
		if (nr51 & 0x88) {
			f32 noise_ch_output = noise_ch.GetOutput();
			right_output += noise_ch_output * (nr51 >> 3 & 1);
			left_output += noise_ch_output * (nr51 >> 7 & 1);
		}

		const auto right_vol = nr50 & 7;
		const auto left_vol = nr50 >> 4 & 7;

		const f32 left_sample = left_vol / 28.0f * left_output;
		const f32 right_sample = right_vol / 28.0f * right_output;

		Audio::EnqueueSample(left_sample);
		Audio::EnqueueSample(right_sample);
	}


	template<uint id>
	void PulseChannel<id>::Initialize()
	{
		dac_enabled = enabled = false;
		volume = duty = wave_pos = 0;
		output = 0.0f;
		envelope.Initialize();
		length_counter.Initialize();
		sweep.Initialize();
	}


	void WaveChannel::Initialize()
	{
		dac_enabled = enabled = false;
		volume = wave_pos = output_level = sample_buffer = 0;
		output = 0.0f;
		length_counter.Initialize();
	}


	void NoiseChannel::Initialize()
	{
		dac_enabled = enabled = false;
		volume = 0;
		output = 0.0f;
		lfsr = 0x7FFF;
		envelope.Initialize();
		length_counter.Initialize();
	}


	void Envelope::Initialize()
	{
		is_updating = false;
		initial_volume = period = timer = 0;
		direction = Direction::Decreasing;
	}

	void LengthCounter::Initialize()
	{
		enabled = false;
		value = length = 0;
	}

	void Sweep::Initialize()
	{
		enabled = negate_has_been_used = false;
		period = shadow_freq = shift = timer = 0;
		direction = Direction::Decreasing;
	}


	template<uint id>
	void PulseChannel<id>::Disable()
	{
		static_assert(id == 0 || id == 1);
		nr52 &= ~(1 << id);
		enabled = false;
	}


	void WaveChannel::Disable()
	{
		nr52 &= ~(1 << 2);
		enabled = false;
	}


	void NoiseChannel::Disable()
	{
		nr52 &= ~(1 << 3);
		enabled = false;
	}


	template<uint id>
	void PulseChannel<id>::Enable()
	{
		nr52 |= 1 << id;
		enabled = true;
	}


	void WaveChannel::Enable()
	{
		nr52 |= 1 << 2;
		enabled = true;
	}


	void NoiseChannel::Enable()
	{
		nr52 |= 1 << 3;
		enabled = true;
	}


	template<uint id>
	void PulseChannel<id>::Trigger()
	{
		if (dac_enabled) {
			Enable();
		}
		if constexpr (id == 0) {
			sweep.Enable();
		}
		envelope.Enable();
		// Enabling in first half of length period should clock length
		// dmg_sound 03-trigger test rom
		// TODO 
		if (length_counter.value == 0) {
			length_counter.value = 64;
			if (length_counter.enabled && frame_seq_step_counter <= 3) {
				--length_counter.value;
			}
		}
		// When triggering a square channel, the low two bits of the frequency timer are NOT modified.
		// https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior
		timer = timer & 3 | freq & ~3;
		envelope.timer = envelope.period > 0 ? envelope.period : 8;
		// If a channel is triggered when the frame sequencer's next step will clock the volume envelope, 
		// the envelope's timer is reloaded with one greater than it would have been.
		// https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior
		if (frame_seq_step_counter == 7) {
			envelope.timer++;
		}
	}


	void WaveChannel::Trigger()
	{
		if (dac_enabled) {
			Enable();
		}
		if (length_counter.value == 0) {
			length_counter.value = 256;
			if (length_counter.enabled && frame_seq_step_counter <= 3) {
				--length_counter.value;
			}
		}
		// Reload period. 
		// The low two bits of the frequency timer are NOT modified.
		// https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware#Obscure_Behavior
		timer = timer & 3 | freq & ~3;
		wave_pos = 0;
	}


	void NoiseChannel::Trigger()
	{
		if (dac_enabled) {
			Enable();
		}
		envelope.Enable();
		if (length_counter.value == 0) {
			length_counter.value = 64;
			if (length_counter.enabled && frame_seq_step_counter <= 3) {
				--length_counter.value;
			}
		}
		// Reload period. 
		// The low two bits of the frequency timer are NOT modified.
		// https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware
		timer = timer & 3 | freq & ~3;
		envelope.timer = envelope.period > 0 ? envelope.period : 8;
		// If a channel is triggered when the frame sequencer's next step will clock the volume envelope, 
		// the envelope's timer is reloaded with one greater than it would have been.
		// https://gbdev.gg8.se/wiki/articles/Gameboy_sound_hardware
		if (frame_seq_step_counter == 7) {
			envelope.timer++;
		}
		lfsr = 0x7FFF;
	}


	void StreamState(SerializationStream& stream)
	{
		/* TODO */
	}


	template u8 ReadReg<Reg::NR10>();
	template u8 ReadReg<Reg::NR11>();
	template u8 ReadReg<Reg::NR12>();
	template u8 ReadReg<Reg::NR13>();
	template u8 ReadReg<Reg::NR14>();
	template u8 ReadReg<Reg::NR21>();
	template u8 ReadReg<Reg::NR22>();
	template u8 ReadReg<Reg::NR23>();
	template u8 ReadReg<Reg::NR24>();
	template u8 ReadReg<Reg::NR30>();
	template u8 ReadReg<Reg::NR31>();
	template u8 ReadReg<Reg::NR32>();
	template u8 ReadReg<Reg::NR33>();
	template u8 ReadReg<Reg::NR34>();
	template u8 ReadReg<Reg::NR41>();
	template u8 ReadReg<Reg::NR42>();
	template u8 ReadReg<Reg::NR43>();
	template u8 ReadReg<Reg::NR44>();
	template u8 ReadReg<Reg::NR50>();
	template u8 ReadReg<Reg::NR51>();
	template u8 ReadReg<Reg::NR52>();
	template u8 ReadReg<Reg::PCM12>();
	template u8 ReadReg<Reg::PCM34>();

	template void WriteReg<Reg::NR10>(u8);
	template void WriteReg<Reg::NR11>(u8);
	template void WriteReg<Reg::NR12>(u8);
	template void WriteReg<Reg::NR13>(u8);
	template void WriteReg<Reg::NR14>(u8);
	template void WriteReg<Reg::NR21>(u8);
	template void WriteReg<Reg::NR22>(u8);
	template void WriteReg<Reg::NR23>(u8);
	template void WriteReg<Reg::NR24>(u8);
	template void WriteReg<Reg::NR30>(u8);
	template void WriteReg<Reg::NR31>(u8);
	template void WriteReg<Reg::NR32>(u8);
	template void WriteReg<Reg::NR33>(u8);
	template void WriteReg<Reg::NR34>(u8);
	template void WriteReg<Reg::NR41>(u8);
	template void WriteReg<Reg::NR42>(u8);
	template void WriteReg<Reg::NR43>(u8);
	template void WriteReg<Reg::NR44>(u8);
	template void WriteReg<Reg::NR50>(u8);
	template void WriteReg<Reg::NR51>(u8);
	template void WriteReg<Reg::NR52>(u8);
	template void WriteReg<Reg::PCM12>(u8);
	template void WriteReg<Reg::PCM34>(u8);
}