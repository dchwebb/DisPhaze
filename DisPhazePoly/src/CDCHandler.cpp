#include "USB.h"
#include "CDCHandler.h"
#include "PhaseDistortion.h"

void CDCHandler::DataIn()
{

}

// As this is called from an interrupt assign the command to a variable so it can be handled in the main loop
void CDCHandler::DataOut()
{
	uint32_t maxLen = std::min((uint32_t)CDC_CMD_LEN - 1, outBuffCount);
	strncpy(comCmd, (char*)outBuff, CDC_CMD_LEN);
	comCmd[maxLen] = '\0';
	cmdPending = true;
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

void CDCHandler::ProcessCommand()
{
	if (cmdPending) {
		std::string cmd = std::string(comCmd);
		if (cmd.compare("help\n") == 0) {
			usb->SendString("Mountjoy DisPhaze Poly - supported commands:\n\n"
					"help           -  Shows this information\n"
					"info           -  Display current settings\n"
					"poly           -  Switches between polyphonic and monophonic mode\n"
					"attack:xxxx    -  Set polyphonic attack time in samples\n"
					"decay:xxxx     -  Set decay time in samples\n"
					"sustain:x.xx   -  Set sustain 0.0 - 1.0\n"
					"release:xxxx   -  Set release time in samples\n"
			);

		} else if (cmd.compare("info\n") == 0) {

			usb->SendString("Polyphonic mode: " + std::string(phaseDist.polyphonic ? "on\r\n": "off\r\n") +
					"Envelope: A:" + std::to_string(phaseDist.envelope.A) +
					", D: " + std::to_string(phaseDist.envelope.D) +
					", S: 0." + std::to_string(static_cast<uint8_t>(100.0f * phaseDist.envelope.S)) +
					", R: " + std::to_string(phaseDist.envelope.R) +
					"\n\r");

		} else if (cmd.compare("poly\n") == 0) {
			phaseDist.polyphonic = !phaseDist.polyphonic;
			usb->SendString("Polyphonic mode: " + std::string(phaseDist.polyphonic ? "on\r\n": "off\r\n"));

		} else if (cmd.compare(0, 7, "attack:") == 0) {			// Envelope attack time in samples
			uint16_t val = ParseInt(cmd, ':', 1, 65535);
			if (val > 0) {
				phaseDist.envelope.A = val;
				//config.SaveConfig();
			}
			usb->SendString("Attack set to: " + std::to_string(phaseDist.envelope.A) + " samples\r\n");

		} else if (cmd.compare(0, 6, "decay:") == 0) {			// Envelope decay time in samples
			uint16_t val = ParseInt(cmd, ':', 1, 65535);
			if (val > 0) {
				phaseDist.envelope.D = val;
				//config.SaveConfig();
			}
			usb->SendString("Decay set to: " + std::to_string(phaseDist.envelope.D) + " samples\r\n");

		} else if (cmd.compare(0, 8, "sustain:") == 0) {		// Envelope sustain amount
			float val = ParseFloat(cmd, ':', 0.0f, 1.0f);
			if (val > 0.0f) {
				phaseDist.envelope.S = val;
				//config.SaveConfig();
			}
			std::string s = std::to_string(static_cast<uint8_t>(100.0f * phaseDist.envelope.S));		// FIXME - using to_string on float crashes for some reason
			usb->SendString("Sustain set to: 0." + s + "\r\n");

		} else if (cmd.compare(0, 8, "release:") == 0) {		// Envelope release time in samples
			uint16_t val = ParseInt(cmd, ':', 1, 65535);
			if (val > 0) {
				phaseDist.envelope.R = val;
				//config.SaveConfig();
			}
			usb->SendString("Release set to: " + std::to_string(phaseDist.envelope.R) + " samples\r\n");

		} else {
			usb->SendString("Unrecognised command. Type 'help' for supported commands\n");
		}
		cmdPending = false;
	}
}


int32_t CDCHandler::ParseInt(const std::string cmd, const char precedingChar, int low = 0, int high = 0) {
	int32_t val = -1;
	int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(cmd.substr(pos + 1).c_str(), "0123456789-") > 0) {
		val = stoi(cmd.substr(pos + 1));
	}
	if (high > low && (val > high || val < low)) {
		usb->SendString("Must be a value between " + std::to_string(low) + " and " + std::to_string(high) + "\r\n");
		return low - 1;
	}
	return val;
}

float CDCHandler::ParseFloat(const std::string cmd, const char precedingChar, float low = 0.0, float high = 0.0) {
	float val = -1.0f;
	int8_t pos = cmd.find(precedingChar);		// locate position of character preceding
	if (pos >= 0 && std::strspn(cmd.substr(pos + 1).c_str(), "0123456789.") > 0) {
		val = stof(cmd.substr(pos + 1));
	}
	if (high > low && (val > high || val < low)) {
		usb->SendString("Must be a value between " + std::to_string(low) + " and " + std::to_string(high) + "\r\n");
		return low - 1.0f;
	}
	return val;
}
