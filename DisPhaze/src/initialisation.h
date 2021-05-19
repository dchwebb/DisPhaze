#pragma once

#include "stm32f4xx.h"
#include "stm32f4xx_flash.h"
#include <cstring>


#define SAMPLERATE 72000

#define ADC_BUFFER_LENGTH 10
extern volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];
enum ADC_Controls {
	ADC_Pitch    = 0,	// PB0 ADC12_IN8   Pin 27
	ADC_VCA      = 5,	// PC0 ADC123_IN10 Pin 8
	ADC_FTune    = 7,	// PB1 ADC12_IN9   Pin 26
	ADC_CTune    = 1,	// PC4 ADC12_IN14  Pin 24
	ADC_Osc1Type = 2,	// PA1 ADC123_IN1  Pin 15
	ADC_Osc2Type = 3,	// PA2 ADC123_IN2  Pin 16
	ADC_PD1CV    = 6,	// PC2 ADC123_IN12 Pin 10
	ADC_PD2CV    = 4,	// PA3 ADC123_IN3  Pin 17
	ADC_PD1Pot   = 8,	// PA7 ADC12_IN7   Pin 23
	ADC_PD2Pot   = 9,	// PC1 ADC12_IN11  Pin 9
};




// Create a struct to store calibration settings - note this uses the Standard Periphal Driver code
struct CalibSettings {
	char OffsetMarker[4] = "TOF";		// calibration tuning offset
	uint32_t Offset = 0;
	char ScaleMarker[4] = "TSC";		// calibration tuning scale
	float Scale = 0;
};
#define ADDR_FLASH_SECTOR_10    reinterpret_cast<uint32_t*>(0x080C0000) // Base address of Sector 10, 128 Kbytes

void SystemClock_Config();
void InitSysTick(uint32_t ticks, uint32_t calib);void InitDAC();void InitSwitches();
void InitTimer();
void InitADC();
void WriteToFlash(CalibSettings& c);
void CalibRestore(CalibSettings& c);

