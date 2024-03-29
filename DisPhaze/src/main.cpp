#include "initialisation.h"
#include "PhaseDistortion.h"
#include "config.h"
#include "USB.h"
#include "CDCHandler.h"

/*
 * Calibration process:
 *
 * 1. Set fine tune and both phase distortion amount knobs to the center position
 * 2. Connect a voltage source to the 1V/Oct input
 * 3. Press the calibration button once - a square wave will be output on channel 1
 * 4. The top left knob will adjust the tuning offset (will be affected by spread setting)
 * 5. The top right knob will adjust the tuning spread (test by checking different octaves)
 * 6. When the calibration is complete press the calibration button again to save
 */


extern uint32_t SystemCoreClock;
volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];

bool dacRead = false;				// Tells the main loop when to queue up the next samples
volatile uint32_t overrun;			// For monitoring if samples are not being delivered to the DAC quickly enough

volatile uint32_t debugInterval;	// Debug timing counter - duration of sample interval available for calculations
volatile uint32_t debugWorkTime;	// Debug timing counter - how much time spent calculating

extern "C" {						// Use extern C to allow linker to find ISR
#include "interrupts.h"
}


int main(void)
{
	SystemInit();					// Activates floating point coprocessor and resets clock
	SystemClock_Config();			// Configure the clock and PLL
	SystemCoreClockUpdate();		// Update SystemCoreClock (system clock frequency) derived from settings of oscillators, prescalers and PLL

	config.RestoreConfig();			// Restore calibration settings from flash memory
	CreateLUTs();					// Create pitch and sine wave look up tables
	InitSwitches();					// Configure switches for Ring mod, mix and octave selection
	InitDAC();						// DAC1 Output on PA4 (Pin 20); DAC2 Output on PA5 (Pin 21)
	InitTimer();					// Sample output timer 3 - fires interrupt to trigger sample output from DAC
	InitADC();						// Configure ADC for analog controls
	InitDebugTimer();				// Timer to check available calculation time

//	usb.InitUSB();
//	usb.cdcDataHandler = std::bind(CDCHandler, std::placeholders::_1, std::placeholders::_2);

	EXTI15_10_IRQHandler();			// Call the Interrupt event handler to set up the octave up/down switch to current position
	EXTI9_5_IRQHandler();			// Call the Interrupt event handler to set up the mix switch to current position

	while (1) {
		config.Calibrate();			// Checks if calibrate button has been pressed and runs calibration routine if so

		// Ready for next sample (Calibrating sends out a square wave for tuning so disables normal output)
		if (dacRead && !config.calibrating) {
			phaseDist.CalcNextSamples();
			debugWorkTime = TIM4->CNT;
		}

		// Check for incoming CDC commands
		if (CmdPending) {
			if (!CDCCommand(ComCmd)) {
				usb.SendString("Unrecognised command. Type 'help' for supported commands\n");
			}
			CmdPending = false;
		}

	}
}

