#include "fft.h"
#include <cstdio>

#ifdef FFT_ANALYSIS
FFT fft;
#endif

// Create sine look up table as constexpr so will be stored in flash
constexpr std::array<float, FFT::sinLUTSize> sineLUT = fft.CreateSinLUT();


void FFT::CalcFFT()
{
	// Carry out Fast Fourier Transform; sinBuffer is a pointer to the current sample capture buffer
	uint16_t bitReverse = 0;
	const uint16_t fftbits = log2(fftSamples);

	// Bit reverse samples
	for (uint32_t i = 0; i < fftSamples; ++i) {
		// assembly bit reverses i and then rotates right to correct bit length
		asm("rbit %[result], %[value]\n\t"
			"ror %[result], %[shift]"
			: [result] "=r" (bitReverse) : [value] "r" (i), [shift] "r" (32 - fftbits));

		if (bitReverse > i) {
			// bit reverse samples
			const float temp = sinBuffer[i];
			sinBuffer[i] = sinBuffer[bitReverse];
			sinBuffer[bitReverse] = temp;
		}
	}


	// Step through each column in the butterfly diagram
	uint32_t node = 1;
	while (node < fftSamples) {

		if (node == 1) {

			// for the first loop the sine and cosine values will be 1 and 0 in all cases, simplifying the logic
			for (uint32_t p1 = 0; p1 < fftSamples; p1 += 2) {
				const uint32_t p2 = p1 + node;
				const float sinP2 = sinBuffer[p2];
				sinBuffer[p2] = sinBuffer[p1] - sinP2;
				cosBuffer[p2] = 0;
				sinBuffer[p1] = sinBuffer[p1] + sinP2;
				cosBuffer[p1] = 0;
			}

		} else if (node == fftSamples / 2) {

			// last node - this only needs to calculate the first half of the FFT results as the remainder are redundant
			for (uint32_t p1 = 1; p1 < fftSamples / 2; ++p1) {
				const uint16_t b = std::round(p1 * sinLUTSize / (2 * node));
				const float s = sineLUT[b];
				const float c = sineLUT[b + sinLUTSize / 4 % sinLUTSize];

				const int p2 = p1 + node;

				sinBuffer[p1] += c * sinBuffer[p2] - s * cosBuffer[p2];
				cosBuffer[p1] += c * cosBuffer[p2] + s * sinBuffer[p2];
			}

		} else {
			// All but first and last nodes: step through each value of the W function
			for (uint32_t Wx = 0; Wx < node; ++Wx) {

				// Use Sine LUT to generate sine and cosine values faster than sine or cosine functions
				const int b = std::round(Wx * sinLUTSize / (2 * node));
				const float s = sineLUT[b];
				const float c = sineLUT[b + sinLUTSize / 4 % sinLUTSize];

				// replace pairs of nodes with updated values
				for (uint32_t p1 = Wx; p1 < fftSamples; p1 += node * 2) {
					const int p2 = p1 + node;

					const float sinP1 = sinBuffer[p1];
					const float cosP1 = cosBuffer[p1];
					const float sinP2 = sinBuffer[p2];
					const float cosP2 = cosBuffer[p2];

					const float t1 = c * sinP2 - s * cosP2;
					const float t2 = c * cosP2 + s * sinP2;

					sinBuffer[p2] = sinP1 - t1;
					cosBuffer[p2] = cosP1 - t2;
					sinBuffer[p1] = sinP1 + t1;
					cosBuffer[p1] = cosP1 + t2;
				}
			}
		}
		node = node * 2;
	}

	// Output results
	for (uint32_t i = 0; i < fftSamples / 2; ++i) {
		const float freq = (float)(i * sampleRatePoly) / (float)fftSamples;
		const float hypotenuse = std::hypot(sinBuffer[i], cosBuffer[i]);
		printf("%.2f %.2f\r\n", freq, hypotenuse);
	}
}




float FFT::HarmonicFreq(const float harmonicNumber)
{
	return (float)sampleRatePoly / fftSamples;
}


