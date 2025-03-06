#include "initialisation.h"
#include "PhaseDistortion.h"
#include "config.h"
#include "USB.h"
#include "CDCHandler.h"

#if (USB_DEBUG)
#include "uartHandler.h"
#endif

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

/* V3 Changes:
 * Octave Up PC10 > PA0
 * Octave Dn PC11 > PC3
 * DAC2_Mix PB8 > PC13
 * Swap:
 * PD1_Type
 * VCA
 * PD2_CV
 * PD2_POT
 */

/* FIXME
 * Octave switch currently on interrupt - probably more efficient to do in sample loop
 * Better note killing algorithm - zero crossings, look for release notes etc
 * Config saving for envelopes
 * Colours on Envelope detection (green for sustain phase); possibly envelope detect PD??
 * Auto detect whether input is MIDI or Pitch; monophonic mode available on MIDI input
 */



Config config;
PhaseDistortion phaseDist;

USB usb;

extern uint32_t SystemCoreClock;
volatile uint32_t SysTickVal;
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
	InitHardware();

	config.RestoreConfig();			// Restore calibration settings from flash memory and create MIDI to pitch LUT if needed
	CreateLUTs();					// Create pitch and sine wave look up tables
	InitSampleTimer();				// Sample output timer 3 - fires interrupt to trigger sample output from DAC
	phaseDist.SetSampleRate();		// Double sample rate for monophonic mode
	usb.Init();


	while (1) {
		config.Calibrate();			// Checks if calibrate button has been pressed and runs calibration routine if so

#if (USB_DEBUG)
		if (uartCmdRdy) {
			uartCommand();
		}
#endif

		usb.cdc.ProcessCommand();	// Check for incoming CDC commands

	}
}

