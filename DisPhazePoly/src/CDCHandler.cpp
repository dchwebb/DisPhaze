#include "CDCHandler.h"


bool CDCCommand(const std::string ComCmd) {
	//std::stringstream ss;

	if (ComCmd.compare("help\n") == 0) {
		usb.SendString("Mountjoy DisPhaze - supported commands:\n\n"
				"help      -  Shows this information\n"
		);

	} else {
		return false;
	}

	return true;
}

// As this is called from an interrupt assign the command to a variable so it can be handled in the main loop
bool CmdPending = false;
char ComCmd[CDC_CMD_LEN];
void CDCHandler(const uint8_t* data, uint32_t length) {
	uint32_t maxLen = std::min((uint32_t)CDC_CMD_LEN - 1, (uint32_t)length);
	strncpy(ComCmd, (char*)data, CDC_CMD_LEN);
	ComCmd[maxLen] = '\0';
	CmdPending = true;
}
