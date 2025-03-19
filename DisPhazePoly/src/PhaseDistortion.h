#pragma once
#include "initialisation.h"
#include "LUT.h"
#include "MidiHandler.h"
#include "configManager.h"
#include <algorithm>


struct PhaseDistortion {
public:
	static constexpr uint8_t pd1LutCount = 5;
	static constexpr uint8_t pd2LutCount = 8;

	void CalcNextSamples();
	void SetSampleRate();
	static void UpdateConfig();
	void ChangePoly();

	uint32_t sampleCalcWindow = 0;			// Set in interrupt to calculate available sample calculation time
	uint32_t sampleCalcTime = 0;			// Set in interrupt to calculate if poluphonic sample calculation is too slow

	struct Env {
		// Constructor calculates increment values based on default envelope settings
		Env() {
			UpdateIncrements();
		}

		uint32_t A = 1000;
		uint32_t D = 4000;
		float S = 0.5f;
		uint32_t R = 5000;
		uint32_t FR = 20;

		uint32_t A_pd = 500;
		uint32_t D_pd = 75000;

		// Incremental variables for adding to current level based on envelope position
		float AInc, DInc, RInc, FRInc, A_pd_Inc, D_pd_Inc;

		// For computational efficiency envelope times in samples are converted to addition increments
		void UpdateIncrements() {
			AInc = 1.0f / A;
			DInc = 1.0f / D;
			RInc = 1.0f / R;
			FRInc = 1.0f / FR;
			A_pd_Inc = 1.0f / A_pd;
			D_pd_Inc = 1.0f / D_pd;
		}

	};

	struct {
		bool polyphonic = false;
		Env envelope;
		struct Compressor {
			float holdTime = 350.0f;				// Hold time in ms before limiter is released
			float release = 0.0000001f;				// Larger = faster release, smaller = slower
			float threshold = 0.35f;				// Default compressor level
		} compressor;
	} cfg;

	ConfigSaver configSaver = {
		.settingsAddress = &cfg,
		.settingsSize = sizeof(cfg),
		.validateSettings = UpdateConfig
	};


private:

	uint16_t pd1Type = 0;			// Phase distortion type knob position with smoothing
	uint16_t pd2Type = 0;
	float pd1Scale = 0.0f;			// Amount of phase distortion with smoothing
	float pd2Scale = 0.0f;
	uint32_t samplePos1 = 0;		// Current position within cycle
	uint32_t samplePos2 = 0;

	float VCALevel;					// Output level with smoothing
	float polyLevel;				// Polyphonic output level for LED brightness
	uint16_t pitch;					// Pitch with smoothing
	int16_t fineTune = 0;
	int16_t coarseTune = 0;

	bool detectEnv = false;			// Activated with action button to detect envelope on VCA input

	enum class envDetectState {waitZero, waitAttack, Attack, Decay, Sustain, Release};
	struct {
		envDetectState state;
		uint32_t stateCount;
		uint32_t stateCount2;
		int32_t counter;			// For flashing LEDs, handling decay to zero etc

		int32_t smoothLevel;
		int32_t maxLevel;
		int32_t minLevel;
		int32_t levelTime;

		uint32_t AttackTime;
		uint32_t DecayTime;
		int32_t SustainLevel;
		uint32_t ReleaseTime;
	} envDetect;

	struct OutputSamples {
		float out1 = 0.0f;
		float out2 = 0.0f;
	};


	// Compressor/Limiter settings
	enum class CompState {none, hold, release};
	CompState compState[2] = {CompState::none, CompState::none};
	uint32_t compHoldSamples = 0;					// Compressor hold time converted from ms to samples
	float compLevel[2] = {cfg.compressor.threshold, cfg.compressor.threshold};		// Compressor level adjusted for input
	uint32_t compHoldTimer[2] = {0, 0};						// Compressor hold counter

	// LED settings
	enum class LEDState {Off, Mono, Poly, SwitchToMono, SwitchToPoly, WaitForEnv, Attack, Decay, Sustain, Release};
	LEDState ledState = cfg.polyphonic ? LEDState::Poly : LEDState::Mono;
	LEDState oldLedState = LEDState::Off;
	uint32_t ledCounter = 0;


	float GetPhaseDist(const float* PdLUT, const uint32_t LUTPosition, const float scale);
	float GetBlendPhaseDist(const float PDBlend, const uint32_t LUTPosition, const float scale);
	float GetResonantWave(const uint8_t pdLut2, const uint32_t LUTPosition, const float scale);
	static float sinLutWrap(float pos);
	float Compress(float x, uint8_t channel);
	void DetectEnvelope();
	void OctaveCalc(float& freq1, float& freq12);
	OutputSamples PolyOutput(float pdLut1, uint8_t pdLut2, float pd2Resonant);
	OutputSamples MonoOutput(float pdLut1, uint8_t pdLut2, float pd2Resonant);
	void SetLED();
	float FastTanh(const float x);

	Btn actionButton {{GPIOB, 5, GpioPin::Type::InputPulldown}};			// FIXME
	//Btn actionButton {{GPIOB, 5, GpioPin::Type::InputPullup}};			// FIXME - setting for DW module
	GpioPin ringModSwitch {GPIOC, 6, GpioPin::Type::Input};
	GpioPin mixSwitch {GPIOC, 13, GpioPin::Type::Input};
	GpioPin octaveUp {GPIOA, 0, GpioPin::Type::InputPulldown};
	GpioPin octaveDown {GPIOC, 3, GpioPin::Type::InputPulldown};
};

extern PhaseDistortion phaseDist;
