#pragma once

#include "initialisation.h"
#include "LUT.h"
#include "MidiHandler.h"
#include <algorithm>



struct PhaseDistortion {
public:
	bool ringModOn = false;
	bool mixOn = false;
	bool polyphonic = true;
	enum RelativePitch { NONE, OCTAVEDOWN, OCTAVEUP	} relativePitch;		// used to adjust pitch of oscillator 2 relative to oscillator 1
	static constexpr uint8_t pd1LutCount = 5;
	static constexpr uint8_t pd2LutCount = 8;

	void CalcNextSamples();
	float GetLevel(MidiHandler::MidiNote& midiNote);

	struct {
		int32_t A = 10000;
		uint32_t D = 10000;
		float S = 0.5f;
		uint32_t R = 5000;
		uint32_t FR = 3;

		float AMult = 1.0f / A;
		float DMult = 1.0f / D;
		float RMult = 1.0f / R;
		float FRMult = 1.0f / FR;
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

	// Compressor/Limiter settings
	enum class CompState {none, hold, release};
	CompState compState[2] = {CompState::none, CompState::none};
	static constexpr uint16_t compHold = 5000;				// Hold time in samples before limiter is released
	static constexpr float compRelease = 0.00001f;			// Larger = faster release, smaller = slower
	static constexpr float defaultLevel = 0.4f;				// Default compressor level
	float compLevel[2] = {defaultLevel, defaultLevel};		// Compressor level adjusted for input
	uint16_t compHoldTimer[2] = {0, 0};							// Compressor hold counter



	float Interpolate(float* LUT, float& LUTPosition);
	float GetPhaseDist(const float* PdLUT, const float LUTPosition, const float scale);
	float GetBlendPhaseDist(const float PDBlend, const float LUTPosition, const float scale);
	float GetResonantWave(const float LUTPosition, const float scale, const uint8_t pdLut2);
	static float sinLutWrap(float pos);
	float Compress(float x, uint8_t channel);
	void DetectEnvelope();
};

extern PhaseDistortion phaseDist;
