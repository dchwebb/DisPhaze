#include "USB.h"
#include "CDCHandler.h"

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
			usb->SendString("Mountjoy MSC_CDC - supported commands:\n\n"
					"help      -  Shows this information\n"
			);
		} else {
			usb->SendString("Unrecognised command. Type 'help' for supported commands\n");
		}
		cmdPending = false;
	}
}


