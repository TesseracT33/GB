#pragma once

#include "SDL.h"

#include "Bus.h"
#include "Utils.h"

///// WIP //////

class APU final : public Serializable
{
public:
	enum Channel { CH1, CH2, CH3, CH4 };

	Bus* bus;

	bool enabled = false;
	u8 channel_duty_1 = 0;
	u8 channel_duty_2 = 0;
	
	void Initialize();
	void Reset();
	void ResetAllRegisters();
	void SetEnvelopeParams(Channel channel);
	void SetLengthParams(Channel channel);
	void SetSweepParams();
	void StepFrameSequencer();
	void Trigger(Channel channel);
	void Update();
	void UpdateCH3DACStatus();

	void Serialize(std::ofstream& ofs) override;
	void Deserialize(std::ifstream& ifs) override;

private:
	struct Envelope
	{
		enum Direction { Downwards, Upwards } direction;
		u8 period, period_timer;
	} envelope[4]{}; // envelope[2] is not used since CH3 doesn't have envelope

	struct Sweep
	{
		enum Direction { Increase, Decrease } direction;
		bool enabled;
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

	static const int sample_rate = 48000;
	static const int sample_buffer_size = 1024;
	const u8 ch3_output_level_shift[4] = { 4, 0, 1, 2 };
	const u8 ch4_divisors[8] = { 8, 16, 32, 48, 64, 80, 96, 112 }; // maps divisor codes to divisors in CH4 freq timer calculation

	u8* NR10, * NR11, * NR12, * NR13, * NR14;
	u8*         NR21, * NR22, * NR23, * NR24;
	u8* NR30, * NR31, * NR32, * NR33, * NR34;
	u8*         NR41, * NR42, * NR43, * NR44;
	u8* NR50, * NR51, * NR52;

	bool set_ch3_wave_pos_to_one_after_sample = false;
	bool channel_is_enabled[4]{};
	bool DAC_is_enabled[4]{};

	u8 frame_seq_step_counter = 0;
	u8 sample_counter = 0;
	u8 wave_pos[3]{}; // does not apply to CH4
	u8 volume[4]{};

	u16 LFSR = 0x7FFF;
	u16 freq_timer[4]{};
	u16 length_timer[4]{};

	unsigned sample_buffer_index = 0;
	unsigned t_cycles_per_sample = (System::t_cycles_per_sec_base * System::speed_mode) / sample_rate; // todo: needs to be reset when speed_mode is changed
	unsigned t_cycles_until_sample = t_cycles_per_sample;

	f32 ch_output[4]{};
	f32 sample_buffer[sample_buffer_size]{};
	
	SDL_AudioSpec audio_spec;

	u16 ComputeNewSweepFreq();
	void DisableChannel(Channel channel);
	void EnableChannel(Channel channel);
	void EnableEnvelope(Channel channel);
	void EnableLength(Channel channel);
	void EnableSweep();
	f32 GetChannel1Amplitude();
	f32 GetChannel2Amplitude();
	f32 GetChannel3Amplitude();
	f32 GetChannel4Amplitude();
	void PrepareChannelAfterTrigger(Channel channel);
	void Sample();
	void StepChannel1();
	void StepChannel2();
	void StepChannel3();
	void StepChannel4();
	void StepEnvelope(Channel channel);
	void StepLength();
	void StepSweep();
};