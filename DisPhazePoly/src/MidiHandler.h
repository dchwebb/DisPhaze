#pragma once

#include "initialisation.h"
#include "USBHandler.h"
#define MIDIQUEUESIZE 50

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
	void RemoveNote(uint8_t note);
	void serialHandler(uint32_t data);

	enum MIDIType {Unknown = 0, NoteOn = 0x9, NoteOff = 0x8, PolyPressure = 0xA, ControlChange = 0xB, ProgramChange = 0xC, ChannelPressure = 0xD, PitchBend = 0xE, System = 0xF };
	enum env : uint8_t {A = 0, D = 1, S = 2, R = 3, FR = 4};			// FR = fast release
	enum class pdEnv : uint8_t {A, D, Off};

	struct MidiNote {
		float noteValue;		// MIDI note value adjusted with pitch bend
		uint8_t origNote;		// MIDI note value
		uint8_t envelope;		// Stage in vca envelope
		pdEnv pdEnvelope;		// Stage in phase distortion envelope
		float samplePos1;		// Current position within cycle
		float samplePos2;
		float vcaLevel;			// Store amplitude envelope level
		float pdLevel;			// Store Phase distortion envelope level
	};

	inline static constexpr uint8_t polyCount = 5;
	std::array<MidiNote, polyCount * 2>midiNotes;			// Double notes to the array to allow easier shuffling of polyphony and Fast release notes
	uint8_t noteCount = 0;									// Number of notes currently sounding
	uint16_t pitchBend = 8192;								// Pitchbend amount in raw format (0 - 16384)
	const float pitchBendSemiTones = 12.0f;					// Number of semitones for a full pitchbend

private:
	void midiEvent(const uint32_t data);
	void QueueInc();

	uint32_t xfer_buff[64];									// OUT Data filled in RxLevel Interrupt

	// Struct for holding incoming USB MIDI data
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
	};

	uint8_t Queue[MIDIQUEUESIZE];			// hold incoming serial MIDI bytes
	uint8_t QueueRead = 0;
	uint8_t QueueWrite = 0;
	uint8_t QueueSize = 0;

	uint8_t sysEx[32];
	uint8_t sysExCount = 0;
};
