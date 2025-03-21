#include "USB.h"
#include "CDCHandler.h"
#include "Calib.h"
#include "PhaseDistortion.h"

#if FFT_ANALYSIS
#include "fft.h"
#endif

void CDCHandler::ProcessCommand()
{
	if (!cmdPending) {
		return;
	}

	std::string_view cmd {comCmd};

	// Provide option to switch to USB DFU mode - this allows the MCU to be programmed with STM32CubeProgrammer in DFU mode
	if (state == serialState::dfuConfirm) {
		if (cmd.compare("y") == 0 || cmd.compare("Y") == 0) {
			usb->SendString("Switching to DFU Mode ...\r\n");
			uint32_t old = SysTickVal;
			while (SysTickVal < old + 100) {};		// Give enough time to send the message
			JumpToBootloader();
		} else {
			state = serialState::pending;
			usb->SendString("Upgrade cancelled\r\n");
		}


	} else if (calib.calibrating) {						// Send command to calibration process
		calib.Calibrate(cmd[0]);

	} else if (cmd.compare("help") == 0) {
		usb->SendString("Mountjoy DisPhaze Poly - supported commands:\n\n"
				"help           -  Shows this information\n"
				"info           -  Display current settings\n"
				"dfu            -  USB firmware upgrade\n"
				"poly           -  Switches between polyphonic and monophonic mode\n"
				"attack:x.xx    -  Set polyphonic attack time in ms\n"
				"decay:x.xx     -  Set decay time in ms\n"
				"sustain:x.xx   -  Set sustain 0.0 - 1.0\n"
				"release:x.xx   -  Set envelope release time in ms\n"
				"comphold:x.xx  -  Set compressor hold time in ms\n"
				"comprel:x.xx   -  Set compressor release rate\n"
				"compthresh:x.xx-  Set compressor threshold\n"
		);

	} else if (cmd.compare("info") == 0) {

		printf("Build Date: %s %s\r\n"
				"Polyphonic mode: %s\r\n"
				"VCA Envelope: A: %.2f ms, D: %.2f ms, S: %.2f, R: %.2f ms\r\n"
				"Compressor: Hold time %.2f ms; Release: %.8f; Threshold: %.2f\r\n"
				"Calibration base: %.2f, mult: %.2f\r\n"
				"Config sector: %lu; address: %p\r\n"
				"\r\n"
				, __DATE__, __TIME__,
				(phaseDist.cfg.polyphonic ? "on" : "off"),
				phaseDist.cfg.envelope.A * 1000.0f,
				phaseDist.cfg.envelope.D * 1000.0f,
				phaseDist.cfg.envelope.S,
				phaseDist.cfg.envelope.R * 1000.0f,
				phaseDist.cfg.compressor.holdTime,
				phaseDist.cfg.compressor.release,
				phaseDist.cfg.compressor.threshold,
				calib.cfg.pitchBase,
				-1.0f / calib.cfg.pitchMult,
				config.currentSector,
				config.flashConfigAddr + config.currentSettingsOffset / 4		// pitch multiplier is stored as a negative reciprocal for calculation

		 );

#if FFT_ANALYSIS
	} else if (cmd.compare("fft") == 0) {					// Perform FFT analysis
		printf("Running FFT\r\n");
		extern bool startFFT;
		startFFT = true;
#endif

	} else if (cmd.compare("dfu") == 0) {					// USB DFU firmware upgrade
		printf("Start DFU upgrade mode? Press 'y' to confirm.\r\n");
		state = serialState::dfuConfirm;

	} else if (cmd.compare("calib") == 0) {					// Start calibration process
		calib.Calibrate('s');

	} else if (cmd.compare("poly") == 0) {
		phaseDist.ChangePoly();
		printf("Polyphonic mode: %s\r\n", phaseDist.cfg.polyphonic ? "on": "off");

	} else if (cmd.compare(0, 7, "attack:") == 0) {			// Envelope attack time in seconds
		float val = ParseFloat(cmd, ':', 0.0f, 1000.0f);
		if (val > 0.0f) {
			phaseDist.cfg.envelope.A = val * 0.001;
			phaseDist.UpdateConfig();
			config.ScheduleSave();
		}
		printf("Attack set to: %.2f ms\r\n", phaseDist.cfg.envelope.A * 1000.0f);

	} else if (cmd.compare(0, 6, "decay:") == 0) {			// Envelope decay time in seconds
		float val = ParseFloat(cmd, ':', 0.0f, 1000.0f);
		if (val > 0.0f) {
			phaseDist.cfg.envelope.D = val * 0.001;
			phaseDist.UpdateConfig();
			config.ScheduleSave();
		}
		printf("Decay set to: %.2f ms\r\n", phaseDist.cfg.envelope.D * 1000.0f);

	} else if (cmd.compare(0, 8, "sustain:") == 0) {		// Envelope sustain amount
		float val = ParseFloat(cmd, ':', 0.0f, 1.0f);
		phaseDist.cfg.envelope.S = val;
		config.ScheduleSave();
		printf("Sustain set to: %.2f\r\n", phaseDist.cfg.envelope.S);

	} else if (cmd.compare(0, 8, "release:") == 0) {		// Envelope release time in seconds
		float val = ParseFloat(cmd, ':', 0.0f, 1000.0f);
		if (val > 0.0f) {
			phaseDist.cfg.envelope.R = val * 0.001;
			phaseDist.UpdateConfig();
			config.ScheduleSave();
		}
		printf("Release set to: %.2f ms\r\n", phaseDist.cfg.envelope.R * 1000.0f);

	} else if (cmd.compare(0, 8, "pitchbase:") == 0) {		// Pitch base calib settings
		float val = ParseFloat(cmd, ':', 0.0f, 1.0f);
		calib.cfg.pitchBase = val;
		calib.UpdatePitchLUT();
		config.ScheduleSave();
		printf("Pitch Base set to: %.2f\r\n", calib.cfg.pitchBase);

	} else if (cmd.compare(0, 10, "pitchmult:") == 0) {		// Pitch multiplier calib settings
		float val = ParseFloat(cmd, ':', 0.0f, 1.0f);
		calib.cfg.pitchMult = val;
		calib.UpdatePitchLUT();
		config.ScheduleSave();
		printf("Pitch Multiplier set to: %.2f\r\n", calib.cfg.pitchMult);

	} else if (cmd.compare(0, 9, "comphold:") == 0) {		// compressor hold time in ms
		float val = ParseFloat(cmd, ':', 0.0f, 2000.0f);
		phaseDist.cfg.compressor.holdTime = val;
		phaseDist.UpdateConfig();
		config.ScheduleSave();
		printf("Compressor Hold time set to: %.2f ms\r\n", phaseDist.cfg.compressor.holdTime);

	} else if (cmd.compare(0, 8, "comprel:") == 0) {		// compressor release rate
		float val = ParseFloat(cmd, ':', 0.0f, 1.0f);
		phaseDist.cfg.compressor.release = val;
		phaseDist.UpdateConfig();
		config.ScheduleSave();
		printf("Compressor release rate set to: %.2f\r\n", phaseDist.cfg.compressor.release);

	} else if (cmd.compare(0, 11, "compthresh:") == 0) {	// Set compressor threshold
		float val = ParseFloat(cmd, ':', 0.1f, 1.0f);
		phaseDist.cfg.compressor.threshold = val;
		phaseDist.UpdateConfig();
		config.ScheduleSave();
		printf("Compressor threshold set to: %.8f\r\n", phaseDist.cfg.compressor.threshold);


	} else {
		usb->SendString("Unrecognised command. Type 'help' for supported commands\n");
	}
	cmdPending = false;

}


int32_t CDCHandler::ParseInt(const std::string_view cmd, const char precedingChar, const int32_t low, const int32_t high) {
	int32_t val = -1;
	const int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(&cmd[pos + 1], "0123456789-") > 0) {
		val = std::stoi(&cmd[pos + 1]);
	}
	if (high > low && (val > high || val < low)) {
		printf("Must be a value between %ld and %ld\r\n", low, high);
		return low - 1;
	}
	return val;
}


float CDCHandler::ParseFloat(const std::string_view cmd, const char precedingChar, const float low = 0.0f, const float high = 0.0f) {
	float val = -1.0f;
	const int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(&cmd[pos + 1], "0123456789.") > 0) {
		val = std::stof(&cmd[pos + 1]);
	}
	if (high > low && (val > high || val < low)) {
		printf("Must be a value between %f and %f\r\n", low, high);
		return low - 1.0f;
	}
	return val;
}




void CDCHandler::DataIn()
{
	if (inBuffSize > 0 && inBuffSize % USB::ep_maxPacket == 0) {
		inBuffSize = 0;
		EndPointTransfer(Direction::in, inEP, 0);				// Fixes issue transmitting an exact multiple of max packet size (n x 64)
	}
}


// As this is called from an interrupt assign the command to a variable so it can be handled in the main loop
void CDCHandler::DataOut()
{
	// Check if sufficient space in command buffer
	const uint32_t newCharCnt = std::min(outBuffCount, maxCmdLen - 1 - buffPos);

	strncpy(&comCmd[buffPos], (char*)outBuff, newCharCnt);
	buffPos += newCharCnt;

	// Check if cr has been sent yet
	if (comCmd[buffPos - 1] == 13 || comCmd[buffPos - 1] == 10 || buffPos == maxCmdLen - 1) {
		comCmd[buffPos - 1] = '\0';
	cmdPending = true;
		buffPos = 0;
	}
}


void CDCHandler::ActivateEP()
{
	EndPointActivate(USB::CDC_In,   Direction::in,  EndPointType::Bulk);			// Activate CDC in endpoint
	EndPointActivate(USB::CDC_Out,  Direction::out, EndPointType::Bulk);			// Activate CDC out endpoint
	EndPointActivate(USB::CDC_Cmd,  Direction::in,  EndPointType::Interrupt);		// Activate Command IN EP

	EndPointTransfer(Direction::out, USB::CDC_Out, USB::ep_maxPacket);
}


void CDCHandler::ClassSetup(usbRequest& req)
{
	if (req.RequestType == DtoH_Class_Interface && req.Request == GetLineCoding) {
		SetupIn(req.Length, (uint8_t*)&lineCoding);
	}

	if (req.RequestType == HtoD_Class_Interface && req.Request == SetLineCoding) {
		// Prepare to receive line coding data in ClassSetupData
		usb->classPendingData = true;
		EndPointTransfer(Direction::out, 0, req.Length);
	}
}


void CDCHandler::ClassSetupData(usbRequest& req, const uint8_t* data)
{
	// ClassSetup passes instruction to set line coding - this is the data portion where the line coding is transferred
	if (req.RequestType == HtoD_Class_Interface && req.Request == SetLineCoding) {
		lineCoding = *(LineCoding*)data;
	}
}





// Descriptor definition here as requires constants from USB class
const uint8_t CDCHandler::Descriptor[] = {
	// IAD Descriptor - Interface association descriptor for CDC class
	0x08,									// bLength (8 bytes)
	USB::IadDescriptor,						// bDescriptorType
	USB::CDCCmdInterface,					// bFirstInterface
	0x02,									// bInterfaceCount
	0x02,									// bFunctionClass (Communications and CDC Control)
	0x02,									// bFunctionSubClass
	0x01,									// bFunctionProtocol
	USB::CommunicationClass,				// String Descriptor

	// Interface Descriptor
	0x09,									// bLength: Interface Descriptor size
	USB::InterfaceDescriptor,				// bDescriptorType: Interface
	USB::CDCCmdInterface,					// bInterfaceNumber: Number of Interface
	0x00,									// bAlternateSetting: Alternate setting
	0x01,									// bNumEndpoints: 1 endpoint used
	0x02,									// bInterfaceClass: Communication Interface Class
	0x02,									// bInterfaceSubClass: Abstract Control Model
	0x01,									// bInterfaceProtocol: Common AT commands
	USB::CommunicationClass,				// iInterface

	// Header Functional Descriptor
	0x05,									// bLength: Endpoint Descriptor size
	USB::ClassSpecificInterfaceDescriptor,	// bDescriptorType: CS_INTERFACE
	0x00,									// bDescriptorSubtype: Header Func Desc
	0x10,									// bcdCDC: spec release number
	0x01,

	// Call Management Functional Descriptor
	0x05,									// bFunctionLength
	USB::ClassSpecificInterfaceDescriptor,	// bDescriptorType: CS_INTERFACE
	0x01,									// bDescriptorSubtype: Call Management Func Desc
	0x00,									// bmCapabilities: D0+D1
	0x01,									// bDataInterface: 1

	// ACM Functional Descriptor
	0x04,									// bFunctionLength
	USB::ClassSpecificInterfaceDescriptor,	// bDescriptorType: CS_INTERFACE
	0x02,									// bDescriptorSubtype: Abstract Control Management desc
	0x02,									// bmCapabilities

	// Union Functional Descriptor
	0x05,									// bFunctionLength
	USB::ClassSpecificInterfaceDescriptor,	// bDescriptorType: CS_INTERFACE
	0x06,									// bDescriptorSubtype: Union func desc
	0x00,									// bMasterInterface: Communication class interface
	0x01,									// bSlaveInterface0: Data Class Interface

	// Endpoint 2 Descriptor
	0x07,									// bLength: Endpoint Descriptor size
	USB::EndpointDescriptor,				// bDescriptorType: Endpoint
	USB::CDC_Cmd,							// bEndpointAddress
	USB::Interrupt,							// bmAttributes: Interrupt
	0x08,									// wMaxPacketSize
	0x00,
	0x10,									// bInterval

	//---------------------------------------------------------------------------

	// Data class interface descriptor
	0x09,									// bLength: Endpoint Descriptor size
	USB::InterfaceDescriptor,				// bDescriptorType:
	USB::CDCDataInterface,					// bInterfaceNumber: Number of Interface
	0x00,									// bAlternateSetting: Alternate setting
	0x02,									// bNumEndpoints: Two endpoints used
	0x0A,									// bInterfaceClass: CDC
	0x00,									// bInterfaceSubClass:
	0x00,									// bInterfaceProtocol:
	0x00,									// iInterface:

	// Endpoint OUT Descriptor
	0x07,									// bLength: Endpoint Descriptor size
	USB::EndpointDescriptor,				// bDescriptorType: Endpoint
	USB::CDC_Out,							// bEndpointAddress
	USB::Bulk,								// bmAttributes: Bulk
	LOBYTE(USB::ep_maxPacket),				// wMaxPacketSize:
	HIBYTE(USB::ep_maxPacket),
	0x00,									// bInterval: ignore for Bulk transfer

	// Endpoint IN Descriptor
	0x07,									// bLength: Endpoint Descriptor size
	USB::EndpointDescriptor,				// bDescriptorType: Endpoint
	USB::CDC_In,							// bEndpointAddress
	USB::Bulk,								// bmAttributes: Bulk
	LOBYTE(USB::ep_maxPacket),				// wMaxPacketSize:
	HIBYTE(USB::ep_maxPacket),
	0x00,									// bInterval: ignore for Bulk transfer
};


uint32_t CDCHandler::GetInterfaceDescriptor(const uint8_t** buffer) {
	*buffer = Descriptor;
	return sizeof(Descriptor);
}
