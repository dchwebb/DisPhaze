#include <MidiHandler.h>
#include "USB.h"

void MidiHandler::DataIn()
{

}

void MidiHandler::DataOut()
{
	// Handle incoming midi command here
	if (outBuffCount == 4) {
		midiEvent(outBuff);

	} else if (outBuff[1] == 0xF0 && outBuffCount > 3) {		// Sysex
		// sysEx will be padded when supplied by usb - add only actual sysEx message bytes to array
		uint8_t sysExCnt = 2, i = 0;
		for (i = 0; i < 32; ++i) {
			if (outBuff[sysExCnt] == 0xF7) break;
			sysEx[i] = outBuff[sysExCnt++];

			// remove 1 byte padding at the beginning of each 32bit word
			if (sysExCnt % 4 == 0) {
				++sysExCnt;
			}
		}
		sysExCount = i;
	}
}



void MidiHandler::midiEvent(const uint32_t* data) {

	midiData = *(MidiData*)data;
}

void MidiHandler::ClassSetup(usbRequest& req)
{
//	switch (req.Request) {
//	case BOT_GET_MAX_LUN:
//		if ((req.Value == 0) && ((req.RequestType & 0x80U) == 0x80U)) {
//			SetupIn(req.Length, &maxLUN);
//		}
//		break;
//	}
}


void MidiHandler::ClassSetupData(usbRequest& req, const uint8_t* data)
{

}






