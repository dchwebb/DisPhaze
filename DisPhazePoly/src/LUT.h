#pragma once

#include "initialisation.h"
#include "config.h"
#include <cmath>

// LUT sizes are probably all going to be constant
#define LUTSIZE 1024
#define SINLUTSIZE 16384

extern const float PDSquareLUT[LUTSIZE];
extern const float PDSawLUT[LUTSIZE];
extern const float PDWave3LUT[LUTSIZE];
extern const float PDWave4LUT[LUTSIZE];
extern const float PDWave5LUT[LUTSIZE];

// Create an array of pointers to the PD LUTs
extern const float* LUTArray[7];
//extern const uint8_t noOfLUTs;

extern float SineLUT[SINLUTSIZE];
extern float PitchLUT[LUTSIZE];

void CreateLUTs();
