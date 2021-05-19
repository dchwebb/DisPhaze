#include "stm32f4xx.h"
#include "initialisation.h"
#include "PhaseDistortion.h"


CalibSettings calibSettings;
PhaseDistortion phaseDist;

extern uint32_t SystemCoreClock;
volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];

bool dacRead = false;				// Tells the main loop when to queue up the next samples
volatile uint32_t overrun;			// For monitoring if samples are not being delivered to the DAC quickly enough

int calibration = 0;
int calibBtn = 0;


//	Use extern C to allow linker to find ISR
extern "C" {
#include "interrupts.h"
}


int main(void)
{
	SystemInit();					// Activates floating point coprocessor and resets clock
	SystemClock_Config();			// Configure the clock and PLL
	SystemCoreClockUpdate();		// Update SystemCoreClock (system clock frequency) derived from settings of oscillators, prescalers and PLL

	CalibRestore(calibSettings);	// Restore calibration settings from flash memory
	CreateLUTs();					// Create pitch and sine wave look up tables
	InitSwitches();					// Configure switches for Ring mod, mix and octave selection
	InitDAC();						// DAC1 Output on PA4 (Pin 20); DAC2 Output on PA5 (Pin 21)
	InitTimer();					// Sample output timer 3 - fires interrupt to trigger sample output from DAC
	InitADC();						// Configure ADC for analog controls

	EXTI15_10_IRQHandler();			// Call the Interrupt event handler to set up the octave up/down switch to current position
	EXTI9_5_IRQHandler();			// Call the Interrupt event handler to set up the mix switch to current position

	while (1) {

		// Toggle calibration mode when button pressed using simple debouncer
		if (READ_BIT(GPIOB->IDR, GPIO_IDR_IDR_5)) {
			calibBtn++;

			if (calibBtn == 200) {
				calibration = !calibration;

				// write calibration settings to flash
				if (!calibration) {
					calibSettings.Scale = 0.5f;
					//WriteToFlash(calibration);
				}
			}
		} else {
			calibBtn = 0;
		}


		if (calibration) {
			// Generate square wave
			if (dacRead) {
				static float pitch, fineTune, samplePos1;

				pitch = ((3.0f * pitch) + ADC_array[ADC_Pitch]) / 4.0f;
				fineTune = ((15.0f * fineTune) + ADC_array[ADC_FTune]) / 16.0f;
				DAC->DHR12R1 = (samplePos1 > SAMPLERATE / 2) ? 4095: 0;
				dacRead = 0;
				float adjPitch = static_cast<float>(pitch) + static_cast<float>(2048 - fineTune) / 32.0f;
				samplePos1 += (2299.0f * std::pow(2.0f, adjPitch / -583.0f));	// Increase 2299 to increase pitch
				while (samplePos1 >= SAMPLERATE) {
					samplePos1-= SAMPLERATE;
				}
			}
			continue;
		}


		// Ready for next sample
		if (dacRead) {
			phaseDist.CalcNextSamples();
		}


	}
}

