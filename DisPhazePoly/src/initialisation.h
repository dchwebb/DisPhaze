#pragma once

#include "stm32f4xx.h"
#include <cstring>
#include "GpioPin.h"

static constexpr uint32_t sampleRateMono = 88000;
static constexpr uint32_t sampleRatePoly = 66000;

#define PITCH_SPREAD -583.0f
static constexpr float PITCH_OFFSET = 1129.5f;

static constexpr uint32_t sysTickInterval = 1000;
extern uint32_t SystemCoreClock;
extern volatile uint32_t SysTickVal;

static constexpr uint32_t adcMax = 4095;
#define ADC_BUFFER_LENGTH 10

#define FFT_ANALYSIS false

struct ADCValues {
	uint16_t Pitch_CV; 	// PB0 ADC12_IN8   Pin 27
	uint16_t VCA;     	// PA3 ADC123_IN3  Pin 17
	uint16_t FTune;   	// PC4 ADC12_IN14  Pin 24
	uint16_t CTune;   	// PB1 ADC12_IN9   Pin 26
	uint16_t Osc1Type;	// PC1 ADC12_IN11  Pin 9
	uint16_t Osc2Type;	// PA2 ADC123_IN2  Pin 16
	uint16_t PD1CV;   	// PC2 ADC123_IN12 Pin 10
	uint16_t PD2CV;   	// PA1 ADC123_IN1  Pin 15
	uint16_t PD1Pot;  	// PA7 ADC12_IN7   Pin 23
	uint16_t PD2Pot;  	// PC0 ADC123_IN10 Pin 8
};

extern volatile ADCValues adc;

extern GpioPin debugPin;


void InitHardware();
void InitClocks();
void InitSysTick();
void InitDAC();
void InitGPIO();
void InitSampleTimer();
void InitADC();
void InitDebugTimer();
void InitPWMTimer();
void InitMidiUART();
void DelayMS(uint32_t ms);
void JumpToBootloader();
