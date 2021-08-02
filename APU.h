#pragma once

#include "SDL.h"

#include "Bus.h"
#include "Utils.h"

///// WIP //////

//#define CH1_DEBUG

class APU final : public Serializable
{
#ifdef CH1_DEBUG
	std::ofstream ofs{ "apu_debug.txt", std::ofstream::out };
#endif
public:
	enum Channel : u8 { CH1, CH2, CH3, CH4 };

	Bus* bus;

	bool enabled = false;
	
	void Initialize();
	void Reset();
	void StepFrameSequencer();
	void Update();
	void WriteToAudioReg(u16 addr, u8 data);

	u8 ReadFromWaveRam(u16 addr);
	void WriteToWaveRam(u16 addr, u8 data);

	void Serialize(std::ofstream& ofs) override;
	void Deserialize(std::ifstream& ifs) override;

private:
	struct Envelope
	{
		enum class Direction { Downwards, Upwards } direction;
		u8 initial_volume, period, period_timer;
		bool is_updating;
	} envelope[4]{}; // envelope[2] is not used since CH3 doesn't have envelope

	struct Sweep
	{
		enum class Direction { Increase, Decrease } direction;
		bool enabled = false, negate_has_been_used = false;
		u8 period, shift, timer;
		u16 shadow_freq;
	} sweep; // for channel 1

	const u8 square_wave_duty_table[4][8] =
	{
		0, 0, 0, 0, 0, 0, 0, 1,
		1, 0, 0, 0, 0, 0, 0, 1,
		1, 0, 0, 0, 0, 1, 1, 1,
		0, 1, 1, 1, 1, 1, 1, 0
	};

	static const int sample_rate = 65536;
	static const int sample_buffer_size = 2048;
	const u8 ch3_output_level_shift[4] = { 4, 0, 1, 2 };
	const u8 ch4_divisors[8] = { 8, 16, 32, 48, 64, 80, 96, 112 }; // maps divisor codes to divisors in CH4 freq timer calculation

	const u8 dmg_initial_wave_ram[0x10] = { 0x84, 0x40, 0x43, 0xAA, 0x2D, 0x78, 0x92, 0x3C,
	                                        0x60, 0x59, 0x59, 0xB0, 0x34, 0xB8, 0x2E, 0xDA };
	const u8 cgb_initial_wave_ram[0x10] = { 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF,
											0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF };

	u8* NR10, * NR11, * NR12, * NR13, * NR14;
	u8*         NR21, * NR22, * NR23, * NR24;
	u8* NR30, * NR31, * NR32, * NR33, * NR34;
	u8*         NR41, * NR42, * NR43, * NR44;
	u8* NR50, * NR51, * NR52;

	bool wave_ram_accessible_by_cpu_when_ch3_enabled = true;
	bool first_half_of_length_period = false;
	bool channel_is_enabled[4]{};
	bool DAC_is_enabled[4]{};
	bool length_is_enabled[4]{};

	u8 ch3_output_level = 0;
	u8 ch3_sample_buffer = 0;
	u8 frame_seq_step_counter = 0;
	u8 sample_counter = 0;

	u8 duty[2]{}; // only applies to CH1 and CH2
	u8 length[4]{};
	u8 volume[4]{};
	u8 wave_pos[3]{}; // does not apply to CH4

	u16 LFSR = 0x7FFF;

	u16 freq[4]{};
	u16 freq_timer[4]{};
	u16 length_counter[4]{}; // todo: should it be u8?

	unsigned sample_buffer_index = 0;
	unsigned t_cycles_per_sample = (System::t_cycles_per_sec_base * System::speed_mode) / sample_rate; // todo: needs to be reset when speed_mode is changed
	unsigned t_cycles_until_sample = t_cycles_per_sample;
	unsigned t_cycles_since_ch3_read_wave_ram = 0;
	const unsigned t_cycles_until_cpu_cant_read_wave_ram = 3;

	f32 ch_output[4]{};
	f32 sample_buffer[sample_buffer_size]{};
	
	SDL_AudioSpec audio_spec;

	u16  ComputeNewSweepFreq();
	void DisableAPU();
	void DisableChannel(Channel channel);
	void EnableAPU();
	void EnableChannel(Channel channel);
	void EnableEnvelope(Channel channel);
	void EnableSweep();
	f32  GetChannel1Amplitude();
	f32  GetChannel2Amplitude();
	f32  GetChannel3Amplitude();
	f32  GetChannel4Amplitude();
	void ResetAllRegisters();
	void Sample();
	void SetEnvelopeParams(Channel channel);
	void SetSweepParams();
	void StepChannel1();
	void StepChannel2();
	void StepChannel3();
	void StepChannel4();
	void StepEnvelope(Channel channel);
	void StepLength(Channel channel);
	void StepSweep();
	void Trigger(Channel channel);
};