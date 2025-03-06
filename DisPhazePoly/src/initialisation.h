#pragma once

#include "stm32f4xx.h"
#include <cstring>
#include <Array>
#include "GpioPIn.h"

#define SAMPLERATE 44000
#define PITCH_SPREAD -583.0f
#define PITCH_OFFSET 2259.0f

static constexpr uint32_t sysTickInterval = 1000;
extern volatile uint32_t SysTickVal;

#define ADC_BUFFER_LENGTH 10


struct ADCValues {
	uint16_t Pitch;   	// PB0 ADC12_IN8   Pin 27
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

#define GreenLED TIM4->CCR2
#define RedLED   TIM2->CCR2
extern GpioPin debugPin;


void InitHardware();
void InitClocks();
void InitSysTick();
void InitDAC();
void InitGPIO();
void InitTimer();
void InitADC();
void InitDebugTimer();
void InitPWMTimer();
void InitMidiUART();
