#pragma once

#include "initialisation.h"
#include "USBHandler.h"


class USB;

class MidiHandler : public USBHandler {
public:
	MidiHandler(USB* usb, uint8_t inEP, uint8_t outEP, int8_t interface) : USBHandler(usb, inEP, outEP, interface) {
		outBuff = xfer_buff;
	}

	void DataIn() override;
	void DataOut() override;
	void ClassSetup(usbRequest& req) override;
	void ClassSetupData(usbRequest& req, const uint8_t* data) override;

private:
	void midiEvent(const uint32_t* data);

	uint32_t xfer_buff[64];				// OUT Data filled in RxLevel Interrupt

	union MidiData {
		MidiData(uint32_t d) : data(d) {};
		MidiData()  {};

		uint32_t data;
		struct {
			uint8_t CIN : 4;
			uint8_t cable : 4;
			uint8_t chn : 4;
			uint8_t msg : 4;
			uint8_t db1;
			uint8_t db2;
		};
	} midiData;

	uint8_t sysEx[32];
	uint8_t sysExCount = 0;
};
