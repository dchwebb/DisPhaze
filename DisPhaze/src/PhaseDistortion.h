#pragma once

#include "initialisation.h"
#include "LUT.h"
#include <algorithm>

class PhaseDistortion {
public:
	bool ringModOn = false;
	bool mixOn = false;

	float pd1Scale = 0.0f;			// Amount of phase distortion
	float pd2Scale = 0.0f;
	uint16_t pd1Type = 0;			// Phase distortion type knob position with smoothing
	uint16_t pd2Type = 0;
	float pdLut1 = 0.0f;			// Phase distortion LUT - float as channel 1 allows blending
	uint8_t pdLut2 = 0;				// PD LUT is being used for channel 2

	float VCALevel;

	float freq1 = 440.0f;
	float freq2 = 440.0f;
	float samplePos1 = 0.0f;
	float samplePos2 = 0.0f;
	uint16_t pitch;
	int16_t fineTune = 0;
	int16_t coarseTune = 0;
	float tuningOffset = 0.0f;
	float tuningScale = 0.0f;

	// used to adjust pitch of oscillator two relative to oscillator one
	enum RelativePitch { NONE, OCTAVEDOWN, OCTAVEUP	} relativePitch;

	float Interpolate(float* LUT, float& LUTPosition);
	float GetPhaseDist(const float* PdLUT, const float LUTPosition, const float scale, const float offset);
	float GetBlendPhaseDist(const float PDBlend, const float LUTPosition, const float scale, const float& offset);
	void CalcNextSamples();
};
