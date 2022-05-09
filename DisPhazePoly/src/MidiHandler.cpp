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
		midiEvent(*outBuff);

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



void MidiHandler::midiEvent(const uint32_t data)
{
	auto midiData = MidiData(data);

	// Note off - if note is in middle of sequence shuffle rest of notes up the array
	if (midiData.msg == NoteOff) {
		for (uint8_t i = 0; i < noteCount; ++i) {
			if (midiNotes[i].noteValue == midiData.db1) {		// note found - set to release
				midiNotes[i].envelope = R;
				midiNotes[i].envTime = 0;
			}
		}
	}

	if (midiData.msg == NoteOn) {
		// Check if note is already sounding and reinitialise timing if so
		for (uint8_t i = 0; i < noteCount; ++i) {
			if (midiNotes[i].noteValue == midiData.db1) {		// Note already playing - reinitialise to Attack
				midiNotes[noteCount].envelope = A;
				midiNotes[i].envTime = 0;
				return;
			}
		}

		// Set next note to be received midi note
		midiNotes[noteCount].origNote = midiData.db1;
		midiNotes[noteCount].noteValue = static_cast<float>(midiData.db1);
		midiNotes[noteCount].envTime = 0;
		midiNotes[noteCount].envelope = A;						// Initialise to attack
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

	if (midiData.msg == PitchBend) {
		pitchBend = static_cast<uint32_t>(midiData.db1) + (midiData.db2 << 7);
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


void MidiHandler::serialHandler(uint32_t data)
{
	Queue[QueueWrite] = data;
	QueueSize++;
	QueueWrite = (QueueWrite + 1) % MIDIQUEUESIZE;

	MIDIType type = static_cast<MIDIType>(Queue[QueueRead] >> 4);
	uint8_t channel = Queue[QueueRead] & 0x0F;

	//NoteOn = 0x9, NoteOff = 0x8, PolyPressure = 0xA, ControlChange = 0xB, ProgramChange = 0xC, ChannelPressure = 0xD, PitchBend = 0xE, System = 0xF
	while ((QueueSize > 2 && (type == NoteOn || type == NoteOff || type == PolyPressure ||  type == ControlChange ||  type == PitchBend)) ||
			(QueueSize > 1 && (type == ProgramChange || type == ChannelPressure))) {

		MidiData event;
		event.chn = channel;
		event.msg = (uint8_t)type;

		QueueInc();
		event.db1 = Queue[QueueRead];
		QueueInc();
		if (type == ProgramChange || type == ChannelPressure) {
			event.db2 = 0;
		} else {
			event.db2 = Queue[QueueRead];
			QueueInc();
		}

		midiEvent(event.data);

		type = static_cast<MIDIType>(Queue[QueueRead] >> 4);
		channel = Queue[QueueRead] & 0x0F;
	}

	// Clock
	if (QueueSize > 0 && Queue[QueueRead] == 0xF8) {
		midiEvent(0xF800);
		QueueInc();
	}

	//	handle unknown data in queue
	if (QueueSize > 2 && type != 0x9 && type != 0x8 && type != 0xD && type != 0xE) {
		QueueInc();
	}
}


inline void MidiHandler::QueueInc() {
	QueueSize--;
	QueueRead = (QueueRead + 1) % MIDIQUEUESIZE;
}

// Called when the release phase of a note has ended to remove from polyphony array
void MidiHandler::RemoveNote(uint8_t note) {
	for (uint8_t i = note; i < noteCount; ++i) {
		midiNotes[i] = midiNotes[i + 1];
	}
	--noteCount;
}

void MidiHandler::ClassSetup(usbRequest& req)
{

}


void MidiHandler::ClassSetupData(usbRequest& req, const uint8_t* data)
{

}






