#pragma once

#include "initialisation.h"
#include "config.h"
#include <cmath>

// LUT sizes are probably all going to be constant
static constexpr uint32_t pdLutSize = 1024;
static constexpr uint32_t pitchLutSize = 4096;
static constexpr uint32_t sinLutSize = 16384;

extern const float PDSquareLUT[pdLutSize];
extern const float PDSawLUT[pdLutSize];
extern const float PDWave3LUT[pdLutSize];
extern const float PDWave4LUT[pdLutSize];
extern const float PDWave5LUT[pdLutSize];

// Create an array of pointers to the PD LUTs
extern const float* LUTArray[7];

extern float SineLUT[sinLutSize];
extern float PitchLUT[pitchLutSize];
extern const uint32_t midiLUTSize;
extern float* MidiLUT;
extern const float midiLUTFirstNote;
extern const float midiLUTNotes;

void CreateLUTs();
