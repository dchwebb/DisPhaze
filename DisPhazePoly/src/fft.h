#pragma once

#include "initialisation.h"
#include <numbers>
#include <cmath>
#include <Array>

class Calib;


class FFT {
	friend class Calib;

public:
	void CalcFFT();

	static constexpr uint32_t fftSamples = 4096;
	static constexpr uint32_t sinLUTSize = fftSamples;

	// Sample capture rate (85Mhz / clockDivider) - chosen to give a reasonably close integer number of cycles for A=440Hz
	static constexpr uint32_t timerDefault = 6033;


	float sinBuffer[fftSamples];						// holds raw samples captured in interrupt for FFT analysis

private:

	float cosBuffer[fftSamples];						// Stores working cosine part of FFT calculation
	uint16_t fftErrors = 0;


	float HarmonicFreq(const float harmonicNumber);

public:
	constexpr auto CreateSinLUT()		// constexpr function to generate LUT in Flash
	{
		std::array<float, sinLUTSize> array {};
		for (uint32_t s = 0; s < sinLUTSize; ++s){
			array[s] = std::sin(s * 2.0f * std::numbers::pi / sinLUTSize);
		}
		return array;
	}

};


extern FFT fft;
