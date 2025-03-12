#pragma once
#include "initialisation.h"
#include "LUT.h"
#include "MidiHandler.h"
#include <algorithm>


struct PhaseDistortion {
public:

	bool polyphonic = false;

	static constexpr uint8_t pd1LutCount = 5;
	static constexpr uint8_t pd2LutCount = 8;

	void CalcNextSamples();
	void SetSampleRate();

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

	} envelope;

	// Polyphonic Output smoothing filter
	struct {
		float b1 = 0.15;		// filter coefficients: filter cutoff = (−ln(b1) / 2π) * (sample_rate / 2) = ~6.6kHz
		float a0 = 1.0f - b1;
		float oldLevel[2] = {0.0f, 0.0f};

		void SetDecay(float d) {
			b1 = d;
			a0 = 1.0f - d;
		}
	} filter;

private:

	uint16_t pd1Type = 0;			// Phase distortion type knob position with smoothing
	uint16_t pd2Type = 0;
	float pd1Scale = 0.0f;			// Amount of phase distortion with smoothing
	float pd2Scale = 0.0f;
	float samplePos1 = 0.0f;		// Current position within cycle
	float samplePos2 = 0.0f;

	float VCALevel;					// Output level with smoothing
	float polyLevel;				// Polyphonic output level for LED brightness
	uint16_t pitch;					// Pitch with smoothing
	int16_t fineTune = 0;
	int16_t coarseTune = 0;

	uint32_t actionBtnTime = 0;		// Duration of action button press
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
	static constexpr uint16_t compHold = 6000;				// Hold time in samples before limiter is released
	static constexpr float compRelease = 0.000005f;			// Larger = faster release, smaller = slower
	static constexpr float defaultLevel = 0.35f;			// Default compressor level
	float compLevel[2] = {defaultLevel, defaultLevel};		// Compressor level adjusted for input
	uint16_t compHoldTimer[2] = {0, 0};						// Compressor hold counter

	// LED settings
	enum class LEDState {Off, Mono, Poly, SwitchToMono, SwitchToPoly, WaitForEnv, Attack, Decay, Sustain, Release};
	LEDState ledState = polyphonic ? LEDState::Poly : LEDState::Mono;
	LEDState oldLedState = LEDState::Off;
	uint32_t ledCounter = 0;


	float GetPhaseDist(const float* PdLUT, const float LUTPosition, const float scale);
	float GetBlendPhaseDist(const float PDBlend, const float LUTPosition, const float scale);
	float GetResonantWave(const float LUTPosition, const float scale, const uint8_t pdLut2);
	static float sinLutWrap(float pos);
	float Compress(float x, uint8_t channel);
	void DetectEnvelope();
	void OctaveCalc(float& freq1, float& freq12);
	OutputSamples PolyOutput(float pdLut1, uint8_t pdLut2, float pd2Resonant);
	OutputSamples MonoOutput(float pdLut1, uint8_t pdLut2, float pd2Resonant);
	void SetLED();

	Btn actionButton {{GPIOB, 5, GpioPin::Type::InputPullup}};
	GpioPin ringModSwitch {GPIOC, 6, GpioPin::Type::Input};
	GpioPin mixSwitch {GPIOC, 13, GpioPin::Type::Input};
	GpioPin octaveUp {GPIOA, 0, GpioPin::Type::Input};
	GpioPin octaveDown {GPIOC, 3, GpioPin::Type::Input};
};

extern PhaseDistortion phaseDist;
