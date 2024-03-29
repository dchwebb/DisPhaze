#pragma once

#include "initialisation.h"
#include "LUT.h"
#include <algorithm>



class PhaseDistortion {
public:
	bool ringModOn = false;
	bool mixOn = false;
	enum RelativePitch { NONE, OCTAVEDOWN, OCTAVEUP	} relativePitch;		// used to adjust pitch of oscillator 2 relative to oscillator 1
	static constexpr uint8_t pd1LutCount = 5;
	static constexpr uint8_t pd2LutCount = 8;

	void CalcNextSamples();

private:
	float pd1Scale = 0.0f;			// Amount of phase distortion
	float pd2Scale = 0.0f;
	uint16_t pd1Type = 0;			// Phase distortion type knob position with smoothing
	uint16_t pd2Type = 0;
	float pdLut1 = 0.0f;			// Phase distortion LUT - float as channel 1 allows blending
	uint8_t pdLut2 = 0;				// PD LUT is being used for channel 2
	bool pd2Resonant;				// Using resonant wave for channel 2
	float lastSample = 0.0f;		// For smoothing resonant wave

	float VCALevel;

	float freq1 = 440.0f;
	float freq2 = 440.0f;
	float samplePos1 = 0.0f;
	float samplePos2 = 0.0f;
	uint16_t pitch;
	uint16_t centerPitchAdj = 10;	// ADC is inaccurate around center reading of 2047 - add this offset to compensate
	int16_t fineTune = 0;
	int16_t coarseTune = 0;


	float Interpolate(float* LUT, float& LUTPosition);
	float GetPhaseDist(const float* PdLUT, const float LUTPosition, const float scale);
	float GetBlendPhaseDist(const float PDBlend, const float LUTPosition, const float scale);
	float GetResonantWave(const float LUTPosition, const float scale);
	static float sinLutWrap(float pos);

};

extern PhaseDistortion phaseDist;
