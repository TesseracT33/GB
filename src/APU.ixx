export module APU;

import Util;

import <array>;
import <cstring>;

namespace APU
{
	export
	{
		enum class Reg {
			NR10, NR11, NR12, NR13, NR14, NR21, NR22, NR23, NR24, NR30, NR31, 
			NR32, NR33, NR34, NR41, NR42, NR43, NR44, NR50, NR51, NR52,
			PCM12, PCM34
		};

		template<Reg reg>
		u8 ReadReg();

		template<Reg reg>
		void WriteReg(u8 value);

		void ApplyNewSampleRate();
		bool Enabled();
		void Initialize(bool hle_boot_rom);
		u8 ReadWaveRamCpu(u16 addr);
		void StepFrameSequencer();
		void StreamState(SerializationStream& stream);
		void Update();
		void WriteWaveRamCpu(u16 addr, u8 data);
	}

	enum class Direction { 
		Decreasing, Increasing /* Sweep and envelope */
	};

	struct Channel
	{
		virtual void Disable() = 0;
		bool dac_enabled;
		bool enabled;
		uint freq;
		uint timer;
		uint volume;
		f32 output;
	};

	struct Envelope
	{
		explicit Envelope(Channel* ch) : ch(ch) {}
		
		void Clock();
		void Enable();
		void Initialize();
		void SetParams(u8 data);

		bool is_updating;
		uint initial_volume;
		uint period;
		uint timer;
		Direction direction;
		Channel* const ch;
	};

	struct LengthCounter
	{
		explicit LengthCounter(Channel* ch) : ch(ch) {}
		
		void Clock();
		void Initialize();

		bool enabled;
		uint value;
		uint length;
		Channel* const ch;
	};

	struct Sweep
	{
		explicit Sweep(Channel* ch) : ch(ch) {}

		void Clock();
		uint ComputeNewFreq();
		void Enable();
		void Initialize();

		bool enabled;
		bool negate_has_been_used;
		uint period;
		uint shadow_freq;
		uint shift;
		uint timer;
		Direction direction;
		Channel* const ch;
	};

	template<uint id>
	struct PulseChannel : Channel
	{
		void Disable() override;
		void Enable();
		void EnableEnvelope();
		f32 GetOutput();
		void Initialize();
		void Step();
		void Trigger();

		uint duty;
		uint wave_pos;
		Envelope envelope{this};
		LengthCounter length_counter{this};
		Sweep sweep{this};
	};

	PulseChannel<0> pulse_ch_1;
	PulseChannel<1> pulse_ch_2;

	struct WaveChannel : Channel
	{
		void Disable() override;
		void Enable();
		f32 GetOutput();
		void Initialize();
		void Step();
		void Trigger();

		uint output_level;
		uint wave_pos;
		u8 sample_buffer;
		LengthCounter length_counter{this};
	} wave_ch;

	struct NoiseChannel : Channel
	{
		void Disable() override;
		void Enable();
		void EnableEnvelope();
		f32 GetOutput();
		void Initialize();
		void Step();
		void Trigger();

		u16 lfsr;
		Envelope envelope{this};
		LengthCounter length_counter{this};
	} noise_ch;

	void DisableAPU();
	void EnableAPU();
	void ResetAllRegisters();
	void Sample();
	
	bool apu_enabled;
	bool wave_ram_accessible_by_cpu_when_ch3_enabled = true;

	u8 nr10, nr11, nr12, nr13, nr14, nr21, nr22, nr23, nr24,
		nr30, nr31, nr32, nr33, nr34, nr41, nr42, nr43, nr44,
		nr50, nr51, nr52;

	uint frame_seq_step_counter;
	uint sample_rate;
	uint t_cycle_sample_counter;
	uint t_cycles_since_ch3_read_wave_ram;

	std::array<u8, 0x10> wave_ram;
}