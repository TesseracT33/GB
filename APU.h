#pragma once

#include "SDL.h"

#include "Bus.h"
#include "Utils.h"

///// WIP //////

class APU final : public Serializable
{
public:
	Bus* bus;

	u8* NR10, *NR11, *NR12, *NR13, *NR14;
	u8*        NR21, *NR22, *NR23, *NR24;
	u8* NR30, *NR31, *NR32, *NR33, *NR34;
	u8*        NR41, *NR42, *NR43, *NR44;
	u8* NR50, *NR51, *NR52;

	enum Channel { CH1, CH2, CH3, CH4 };

	const int SQUARE_WAVE_DUTY_TABLE[4][8] =
	{
		0, 0, 0, 0, 0, 0, 0, 1,
		1, 0, 0, 0, 0, 0, 0, 1,
		1, 0, 0, 0, 0, 1, 1, 1,
		0, 1, 1, 1, 1, 1, 1, 0
	};

	u16 frequency_timer[4]{};
	u8 wave_duty_pos_1 = 0;
	u8 wave_duty_pos_2 = 0;
	u8 wave_duty_pos_3 = 0;
	u8 channel_duty_1 = 0;
	u8 channel_duty_2 = 0;

	u8 channel_3_byte_index = 0;
	bool channel_3_high_nibble_played = false; // todo: reset in various places

	int amplitude[4]{};

	void SetInitialFrequencyTimer(Channel channel);

	u8 fs_step_mod = 0;

	bool enabled = false;
	
	void Initialize();
	void Reset();
	void Update();

	void Step_Frame_Sequencer();

	void Serialize(std::ofstream& ofs) override;
	void Deserialize(std::ifstream& ifs) override;

	struct Envelope
	{
		u8 initial_volume, period = 0;
		bool direction = 0; // 0: addition. 1: subtraction
		int period_timer = 0, current_volume = 0;

		bool active = false;
	};
	Envelope envelope[4]{}; // note: envelope[2] is not used since CH3 doesn't have envelope
	void EnableEnvelope(Channel channel);
	void DisableEnvelope(Channel channel);


	void EnableLength(Channel channel);

	void Trigger(Channel channel);

	void ResetAllRegisters();


private:
	/// Sweep
	void EnableSweep();
	void StepSweep();
	u16 Sweep_Compute_New_Freq();
	struct Sweep
	{
		bool enabled = false;

		u8 period = 0;
		bool direction = 0;
		u8 shift = 0;

		u8 timer = 0;
		u16 shadow_freq = 0;
	} sweep;

	void StepEnvelope(Channel channel);
	void Mix_and_Pan();

	int length_timer[4]{};
	void StepLength();

	const int sample_rate = 48000;
	const int t_cycles_per_sec_base = 4194304;
	int t_cycles_per_sample = (t_cycles_per_sec_base * System::speed_mode) / sample_rate; // todo: needs to be reset when speed_mode is changed
	int t_cycles_until_sample = t_cycles_per_sample;
	int sample_mod = 0;

	u16 LFSR = 0;

	u8 channel_3_shift[4] = { 4, 0, 1, 2 };
	f32 channel_3_output_level[4] = { 0.0f, 1.0f, 0.5f, 0.25f };

	// maps divisor codes to divisors in CH4
	const int divisors[8] = { 8, 16, 32, 48, 64, 80, 96, 112 };

	// sampling-related
	static const int sample_size = 1024;
	f32 sample_buffer[sample_size]{};
	int buffer_index = 0;

	SDL_AudioSpec audioSpec;
};