#include "USB.h"
#include "CDCHandler.h"
#include "PhaseDistortion.h"

void CDCHandler::ProcessCommand()
{
	if (!cmdPending) {
		return;
	}

	std::string_view cmd {comCmd};

	if (cmd.compare("help") == 0) {
		usb->SendString("Mountjoy DisPhaze Poly - supported commands:\n\n"
				"help           -  Shows this information\n"
				"info           -  Display current settings\n"
				"poly           -  Switches between polyphonic and monophonic mode\n"
				"attack:xxxx    -  Set polyphonic attack time in samples\n"
				"decay:xxxx     -  Set decay time in samples\n"
				"sustain:x.xx   -  Set sustain 0.0 - 1.0\n"
				"release:xxxx   -  Set release time in samples\n"
				"pda:xxxx       -  Set PD attack time in samples\n"
				"pdd:xxxx       -  Set PD decay time in samples\n"
				"filter:x.xx    -  Set filter level 0.0 - 1.0\n"
		);

	} else if (cmd.compare("info") == 0) {

		usb->SendString("Polyphonic mode: " + std::string(phaseDist.polyphonic ? "on\r\n": "off\r\n") +
				"Filter level: 0." + std::to_string(static_cast<uint8_t>(100.0f * phaseDist.filter.b1)) +
				"\r\nVCA Envelope: A:" + std::to_string(phaseDist.envelope.A) +
				", D: " + std::to_string(phaseDist.envelope.D) +
				", S: 0." + std::to_string(static_cast<uint8_t>(100.0f * phaseDist.envelope.S)) +
				", R: " + std::to_string(phaseDist.envelope.R) +
				"\r\nPD Envelope: A:" + std::to_string(phaseDist.envelope.A_pd) +
				", D: " + std::to_string(phaseDist.envelope.D_pd) +

				"\n\r");

	} else if (cmd.compare("poly") == 0) {
		phaseDist.polyphonic = !phaseDist.polyphonic;
		usb->SendString("Polyphonic mode: " + std::string(phaseDist.polyphonic ? "on\r\n": "off\r\n"));

	} else if (cmd.compare(0, 7, "attack:") == 0) {			// Envelope attack time in samples
		uint32_t val = ParseInt(cmd, ':', 1, UINT32_MAX);
		if (val > 0) {
			phaseDist.envelope.A = val;
			phaseDist.envelope.UpdateIncrements();
			//config.SaveConfig();
		}
		usb->SendString("Attack set to: " + std::to_string(phaseDist.envelope.A) + " samples\r\n");

	} else if (cmd.compare(0, 6, "decay:") == 0) {			// Envelope decay time in samples
		uint32_t val = ParseInt(cmd, ':', 1, UINT32_MAX);
		if (val > 0) {
			phaseDist.envelope.D = val;
			phaseDist.envelope.UpdateIncrements();
			//config.SaveConfig();
		}
		usb->SendString("Decay set to: " + std::to_string(phaseDist.envelope.D) + " samples\r\n");

	} else if (cmd.compare(0, 8, "sustain:") == 0) {		// Envelope sustain amount
		float val = ParseFloat(cmd, ':', 0.0f, 1.0f);
		phaseDist.envelope.S = val;
			//config.SaveConfig();
		std::string s = std::to_string(static_cast<uint8_t>(100.0f * phaseDist.envelope.S));		// FIXME - using to_string on float crashes for some reason
		usb->SendString("Sustain set to: 0." + s + "\r\n");

	} else if (cmd.compare(0, 8, "release:") == 0) {		// Envelope release time in samples
		uint32_t val = ParseInt(cmd, ':', 1, UINT32_MAX);
		if (val > 0) {
			phaseDist.envelope.R = val;
			phaseDist.envelope.UpdateIncrements();
			//config.SaveConfig();
		}
		usb->SendString("Release set to: " + std::to_string(phaseDist.envelope.R) + " samples\r\n");

	} else if (cmd.compare(0, 4, "pda:") == 0) {			// Envelope attack time in samples
		uint32_t val = ParseInt(cmd, ':', 1, UINT32_MAX);
		if (val > 0) {
			phaseDist.envelope.A_pd = val;
			phaseDist.envelope.UpdateIncrements();
			//config.SaveConfig();
		}
		usb->SendString("PD Attack set to: " + std::to_string(phaseDist.envelope.A_pd) + " samples\r\n");

	} else if (cmd.compare(0, 4, "pdd:") == 0) {			// Envelope decay time in samples
		uint32_t val = ParseInt(cmd, ':', 1, UINT32_MAX);
		if (val > 0) {
			phaseDist.envelope.D_pd = val;
			phaseDist.envelope.UpdateIncrements();
			//config.SaveConfig();
		}
		usb->SendString("PD Decay set to: " + std::to_string(phaseDist.envelope.D_pd) + " samples\r\n");

	} else if (cmd.compare(0, 7, "filter:") == 0) {			// Filter decay time
		float val = ParseFloat(cmd, ':', 0.0f, 1.0f);
		if (val > 0.0f) {
			phaseDist.filter.SetDecay(val);
		}
		std::string s = std::to_string(static_cast<uint8_t>(100.0f * phaseDist.filter.b1));		// FIXME - using to_string on float crashes for some reason
		usb->SendString("Filter decay set to: 0." + s + "\r\n");

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
