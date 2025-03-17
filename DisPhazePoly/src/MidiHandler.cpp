#include <MidiHandler.h>
#include "USB.h"
#include "LUT.h"
#include "PhaseDistortion.h"


void MidiHandler::SendData(uint8_t* buffer, uint32_t size)
{
	usb->SendData(buffer, size, inEP);
}

void MidiHandler::DataIn()
{

}

void MidiHandler::DataOut()
{
	// Handle incoming midi command here
	if (outBuffCount == 4) {
		midiEvent(*outBuff);

	} else if (outBuff[1] == 0xF0 && outBuffCount > 3) {		// Sysex - Doesn't do anything at present
		// sysEx will be padded when supplied by usb - add only actual sysEx message bytes to array
		uint8_t sysExCnt = 2, i = 0;
		for (i = 0; i < 32; ++i) {
			if (outBuff[sysExCnt] == 0xF7) {
				break;
			}
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
	midiReceived = SysTickVal;			// Allows auto switching between midi and CV mode

	// Note off - set envelope to release
	if (midiData.msg == NoteOff) {
		for (uint8_t i = 0; i < noteCount; ++i) {
			if (midiNotes[i].noteValue == midiData.db1 && midiNotes[i].envelope != R) {		// note found - set to release
				midiNotes[i].envelope = R;
			}
		}
	}

	if (midiData.msg == NoteOn) {
		if (!phaseDist.cfg.polyphonic) {
			phaseDist.ChangePoly();
		}

		// Check if note is already sounding and reinitialise timing if so
		for (uint8_t i = 0; i < noteCount; ++i) {

			if (midiNotes[i].noteValue == midiData.db1) {		// Note already playing - reinitialise to Attack
				midiNotes[i].envelope = A;
				midiNotes[i].pdEnvelope = pdEnv::A;
				return;
			}
		}

		// Set next note to be received midi note
		midiNotes[noteCount].origNote = midiData.db1;
		midiNotes[noteCount].noteValue = static_cast<float>(midiData.db1);
		midiNotes[noteCount].envelope = A;						// Initialise VCA envelope to attack
		midiNotes[noteCount].pdEnvelope = pdEnv::A;				// Initialise PD envelope to attack
		midiNotes[noteCount].vcaLevel = 0.0f;
		midiNotes[noteCount].pdLevel = 0.0f;
		midiNotes[noteCount].samplePos1 = 0;
		midiNotes[noteCount].samplePos2 = 0;

		++noteCount;

		if (noteCount > polyCount) {							// Polyphony exceeded
			midiNotes[0].envelope = FR;							// Fast release envelope
			//RemoveNote(0);
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


void MidiHandler::ActivateEP()
{
	EndPointActivate(USB::Midi_In,   Direction::in,  EndPointType::Bulk);
	EndPointActivate(USB::Midi_Out,  Direction::out, EndPointType::Bulk);

	EndPointTransfer(Direction::out, USB::Midi_Out, USB::ep_maxPacket);
}


void MidiHandler::ClassSetup(usbRequest& req)
{

}


void MidiHandler::ClassSetupData(usbRequest& req, const uint8_t* data)
{

}



// Descriptor definition here as requires constants from USB class
const uint8_t MidiHandler::Descriptor[] = {
	// B.3.1 Standard Audio Control standard Interface Descriptor
	0x09,									// length of descriptor in bytes
	USB::InterfaceDescriptor,				// interface descriptor type
	USB::AudioInterface,					// index of this interface
	0x00,									// alternate setting for this interface
	0x00,									// endpoints excl 0: number of endpoint descriptors to follow
	0x01,									// AUDIO
	0x01,									// AUDIO_Control
	0x00,									// bInterfaceProtocol
	USB::AudioClass,						// string index for interface

	// B.3.2 Class-specific AC Interface Descriptor
	0x09,									// length of descriptor in bytes
	USB::ClassSpecificInterfaceDescriptor,	// descriptor type
	0x01,									// header functional descriptor
	0x00, 0x01,								// bcdADC
	0x09, 0x00,								// wTotalLength
	0x01,									// bInCollection
	0x01,									// baInterfaceNr[1]

	// B.4 MIDIStreaming Interface Descriptors
	// B.4.1 Standard MS Interface Descriptor
	0x09,									// bLength
	USB::InterfaceDescriptor,				// bDescriptorType: interface descriptor
	USB::MidiInterface,						// bInterfaceNumber
	0x00,									// bAlternateSetting
	0x02,									// bNumEndpoints
	0x01,									// bInterfaceClass: Audio
	0x03,									// bInterfaceSubClass: MIDIStreaming
	0x00,									// InterfaceProtocol
	USB::AudioClass,						// iInterface: String Descriptor

	// B.4.2 Class-specific MS Interface Descriptor
	0x07,									// length of descriptor in bytes
	USB::ClassSpecificInterfaceDescriptor,	// bDescriptorType: Class Specific Interface Descriptor
	0x01,									// header functional descriptor
	0x0, 0x01,								// bcdADC
	MidiHandler::MidiClassDescSize, 0,		// wTotalLength

	// B.4.3 MIDI IN Jack Descriptor (Embedded)
	0x06,									// bLength
	USB::ClassSpecificInterfaceDescriptor,	// descriptor type
	0x02,									// bDescriptorSubtype: MIDI_IN_JACK
	0x01,									// bJackType: Embedded
	0x01,									// bJackID
	0x00,									// iJack: No String Descriptor

	// Table B4.4 Midi Out Jack Descriptor (Embedded)
	0x09,									// length of descriptor in bytes
	USB::ClassSpecificInterfaceDescriptor,	// descriptor type
	0x03,									// MIDI_OUT_JACK descriptor
	0x01,									// bJackType: Embedded
	0x02,									// bJackID
	0x01,									// No of input pins
	0x01,									// ID of the Entity to which this Pin is connected.
	0x01,									// Output Pin number of the Entity to which this Input Pin is connected.
	0x00,									// iJack

	//B.5.1 Standard Bulk OUT Endpoint Descriptor
	0x09,									// bLength
	USB::EndpointDescriptor,				// bDescriptorType = endpoint
	USB::Midi_Out,							// bEndpointAddress
	USB::Bulk,								// bmAttributes: 2:Bulk
	LOBYTE(USB::ep_maxPacket),				// wMaxPacketSize
	HIBYTE(USB::ep_maxPacket),
	0x00,									// bInterval in ms : ignored for bulk
	0x00,									// bRefresh Unused
	0x00,									// bSyncAddress Unused

	// B.5.2 Class-specific MS Bulk OUT Endpoint Descriptor
	0x05,									// bLength of descriptor in bytes
	0x25,									// bDescriptorType (Audio Endpoint Descriptor)
	0x01,									// bDescriptorSubtype: MS General
	0x01,									// bNumEmbMIDIJack
	0x01,									// baAssocJackID ID of the Embedded MIDI IN Jack.

	//B.6.1 Standard Bulk IN Endpoint Descriptor
	0x09,									// bLength
	USB::EndpointDescriptor,				// bDescriptorType = endpoint
	USB::Midi_In,							// bEndpointAddress IN endpoint number 3
	USB::Bulk,								// bmAttributes: 2: Bulk, 3: Interrupt endpoint
	LOBYTE(USB::ep_maxPacket),				// wMaxPacketSize
	HIBYTE(USB::ep_maxPacket),
	0x00,									// bInterval in ms
	0x00,									// bRefresh
	0x00,									// bSyncAddress

	// B.6.2 Class-specific MS Bulk IN Endpoint Descriptor
	0x05,									// bLength of descriptor in bytes
	0x25,									// bDescriptorType
	0x01,									// bDescriptorSubtype
	0x01,									// bNumEmbMIDIJack
	0x02,									// baAssocJackID ID of the Embedded MIDI OUT Jack

};


uint32_t MidiHandler::GetInterfaceDescriptor(const uint8_t** buffer) {
	*buffer = Descriptor;
	return sizeof(Descriptor);
}

