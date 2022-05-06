#include <MidiHandler.h>
#include "USB.h"
#include "LUT.h"

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



void MidiHandler::midiEvent(const uint32_t* data)
{
	midiData = *(MidiData*)data;

	// Note off - if note is in middle of sequence shuffle rest of notes up the array
	if (midiData.msg == NoteOff) {
		bool moveUp = false;
		for (uint8_t i = 0; i < noteCount; ++i) {
			if (midiNotes[i].noteValue == midiData.db1) {		// note already in array - overwrite with later notes
				moveUp = true;
			} else {
				if (moveUp) {
					midiNotes[i - 1] = midiNotes[i];
				}
			}
		}
		if (moveUp) {
			--noteCount;
		}
	}

	if (midiData.msg == NoteOn) {
		for (uint8_t i = 0; i < noteCount; ++i) {
			if (midiNotes[i].noteValue == midiData.db1) {		// note already in array
				midiNotes[i].timeOn = 0;
				return;
			}
		}
		midiNotes[noteCount].noteValue = midiData.db1;
		midiNotes[noteCount].timeOn = 0;
		midiNotes[noteCount].envelope = A;						// Initialise to attack
		//midiNotes[noteCount].freq = MidiLUT[midiData.db1];
		midiNotes[noteCount].samplePos1 = 0;
		midiNotes[noteCount].samplePos2 = 0;
		++noteCount;

		if (noteCount > polyCount) {							// Polyphony exceeded - shuffle up
			for (uint8_t i = 1; i < noteCount; ++i) {
				midiNotes[i - 1] = midiNotes[i];
			}
			--noteCount;
		}
	}
/*
	if (midiData.msg == NoteOff || midiData.msg == NoteOn) {
		std::string out = "Note count: " + std::to_string(noteCount) + " [" +
				std::to_string(midiNotes[0].noteValue) + " " + std::to_string(midiNotes[0].timeOn) + ", " +
				std::to_string(midiNotes[1].noteValue) + " " + std::to_string(midiNotes[1].timeOn) + ", " +
				std::to_string(midiNotes[2].noteValue) + " " + std::to_string(midiNotes[2].timeOn) + ", " +
				std::to_string(midiNotes[3].noteValue) + " " + std::to_string(midiNotes[3].timeOn) + "]\r";
		usb->SendString(out.c_str());
	}
	*/
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






