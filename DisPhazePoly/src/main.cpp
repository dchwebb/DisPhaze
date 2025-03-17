#include "initialisation.h"
#include "PhaseDistortion.h"
#include "configManager.h"
#include "USB.h"
#include "CDCHandler.h"
#include "Calib.h"

#if (USB_DEBUG)
#include "uartHandler.h"
#endif


/* FIXME
 * Better note killing algorithm - zero crossings, look for release notes etc
 * Colours on Envelope detection (green for sustain phase); possibly envelope detect PD??
  */


PhaseDistortion phaseDist;
Calib calib;

extern uint32_t SystemCoreClock;
volatile uint32_t SysTickVal;
volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];

extern "C" {						// Use extern C to allow linker to find ISR
#include "interrupts.h"
}

Config config{&phaseDist.configSaver, &calib.configSaver};		// Construct config handler with list of configSavers


int main(void)
{
	InitHardware();

	config.RestoreConfig();			// Restore calibration settings from flash memory and create MIDI to pitch LUT if needed
	CreateLUTs();					// Create pitch and sine wave look up tables
	InitSampleTimer();				// Sample output timer 3 - fires interrupt to trigger sample output from DAC
	phaseDist.SetSampleRate();		// Double sample rate for monophonic mode
	usb.Init(false);


	while (1) {
		calib.Calibrate();			// Calibration state machine
		usb.cdc.ProcessCommand();	// Check for incoming CDC commands
		config.SaveConfig();		// Save any scheduled changes
#if (USB_DEBUG)
		if (uartCmdRdy) {
			uartCommand();
		}
#endif


	}
}

