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

	struct Env {
		float A = 0.001f;				// Time in seconds
		float D = 0.05f;
		float S = 0.5f;
		float R = 0.05f;
		float FR = 0.00045f;			// Fast release
	};

	struct {
		bool polyphonic = false;
		Env envelope;
		struct Compressor {
			float holdTime = 350.0f;	// Hold time in ms before limiter is released
			float release = 0.0000001f;	// Larger = faster release, smaller = slower
			float threshold = 0.35f;	// Default compressor level
		} compressor;
	} cfg;

	ConfigSaver configSaver = {
		.settingsAddress = &cfg,
		.settingsSize = sizeof(cfg),
		.validateSettings = UpdateConfig
	};


private:
	uint32_t sampleCalcWindow = 0;		// Set in interrupt to calculate available sample calculation time
	uint32_t sampleCalcTime = 0;		// Set in interrupt to calculate if poluphonic sample calculation is too slow

	uint16_t pd1Type = 0;				// Phase distortion type knob position with smoothing
	uint16_t pd2Type = 0;
	float pd1Scale = 0.0f;				// Amount of phase distortion with smoothing
	float pd2Scale = 0.0f;
	uint32_t samplePos1 = 0;			// Current position within cycle
	uint32_t samplePos2 = 0;

	float VCALevel;						// Output level with smoothing
	float polyLevel;					// Polyphonic output level for LED brightness
	uint16_t pitch;						// Pitch with smoothing
	int16_t fineTune = 0;
	int16_t coarseTune = 0;

	uint32_t maxSampleTime = 0;			// Maximum sample time allowed before polyphonic notes are truncated
	bool detectEnv = false;				// Activated with action button to detect envelope on VCA input

	// Incremental variables for adding to current level based on envelope position
	float attackInc, decayInc, releaseInc, fastRelInc;


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

		uint32_t attackTime;		// Detected times in samples
		uint32_t decayTime;
		int32_t sustainLevel;
		uint32_t releaseTime;
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
	uint32_t compHoldTimer[2] = {0, 0};												// Compressor hold counter

	// LED settings
	enum class LEDState {Off, Mono, Poly, SwitchToMono, SwitchToPoly, DetectEnvelope, WaitForEnv, Attack, Decay, Sustain, Release};
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

	Btn actionButton {{GPIOB, 5, GpioPin::Type::InputPulldown}};
	GpioPin ringModSwitch {GPIOC, 6, GpioPin::Type::Input};
	GpioPin mixSwitch {GPIOC, 13, GpioPin::Type::Input};
	GpioPin octaveUp {GPIOA, 0, GpioPin::Type::InputPulldown};
	GpioPin octaveDown {GPIOC, 3, GpioPin::Type::InputPulldown};

	volatile uint32_t& GreenLED = TIM4->CCR2;
	volatile uint32_t& RedLED = TIM2->CCR2;
};

extern PhaseDistortion phaseDist;
