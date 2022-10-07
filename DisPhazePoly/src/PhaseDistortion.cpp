#include "PhaseDistortion.h"
#include "USB.h"



inline float PhaseDistortion::sinLutWrap(float pos)
{
	while (pos >= SINLUTSIZE) {
		pos -= SINLUTSIZE;
	}
	while (pos < 0) {
		pos =  SINLUTSIZE + pos;
	}
	return pos;
}

//	Interpolate between two positions (derived from a float and its rounded value) in a LUT
float PhaseDistortion::Interpolate(float* LUT, float& LUTPosition)
{
	float s1 = LUT[static_cast<int32_t>(LUTPosition)];
	float s2 = LUT[static_cast<int32_t>(LUTPosition + 1) % LUTSIZE];
	return s1 + ((s2 - s1) * (LUTPosition - std::round(LUTPosition)));
}


float PhaseDistortion::GetResonantWave(const float LUTPosition, float scale, const uint8_t pdLut2)
{
	// models waves 6-8 of the Casio CZ which are saw/triangle envelopes into which harmonics are added as the phase distortion increases
	static float lastSample = 0.0f;

	scale = ((scale / 5.0f) * 23.0f) + 1.0f;		// Sets number of sine waves per cycle: scale input 0-5 to 1-24 (original only went to 16)

	// offset to 3/4 of the way through the sine wave so each cycle starts at the flat top of the wave
	constexpr float sineOffset = static_cast<float>(3 * (SINLUTSIZE / 4));

	// Scale the amplitude of the cycle from 1 to 0 to create a saw tooth type envelope on each cycle
	float ampMod = 1.0f;
	if (pdLut2 == 5) {								// Saw tooth - amplitude linearly reduces over full cycle
		ampMod = (1.0f - LUTPosition);
	} else if (LUTPosition > 0.5f) {				// Both triangle (wave 7) and loud saw (wave 8) fade out over second half of cycle
		ampMod = (2.0f * (1.0f - LUTPosition));
	} else if (pdLut2 == 6) {						// Triangle (wave 7) fades in over first half of cycle
		ampMod = (LUTPosition * 2.0f);
	}

	float pos = sinLutWrap(((LUTPosition * SINLUTSIZE) * scale) + sineOffset);
	float sineSample = SineLUT[static_cast<uint32_t>(pos)];
	sineSample = (sineSample + 1.0f) * ampMod;		// Offset so all positive and then apply amplitude envelope to create sawtooth

	// FIXME: removed as glitching in polyphonic mode. If required will need to store lastSample against each note
	//lastSample = (lastSample * 0.85f) + (0.15f * (sineSample - 1.0f));			// Remove offset and damp
	return sineSample - 1.0f;
}


// Generate a phase distorted sine wave - pass LUT containing PD offsets, LUT position as a fraction of the wave cycle and a scaling factor
float PhaseDistortion::GetPhaseDist(const float* PdLUT, const float LUTPosition, const float scale)
{
	float phaseDist = PdLUT[static_cast<int32_t>(LUTPosition * LUTSIZE)] * SINLUTSIZE * scale;

	// Add main wave position to phase distortion position and ensure in bounds
	float pos = sinLutWrap((LUTPosition * SINLUTSIZE) + phaseDist);

	return SineLUT[static_cast<uint32_t>(pos)];
}


// Generate a phase distorted sine wave - pass LUT containing PD offsets, LUT position as a fraction of the wave cycle and a scaling factor
float PhaseDistortion::GetBlendPhaseDist(const float pdBlend, const float LUTPosition, const float scale)
{
	// Get the two PD LUTs that will be blended
	const float* pdLUTBlendA = LUTArray[static_cast<uint8_t>(pdBlend)];
	const float* pdLUTBlendB = LUTArray[static_cast<uint8_t>(pdBlend + 1) % pd1LutCount];

	// Get the values from each LUT for the sample position
	float phaseDistA = pdLUTBlendA[(int)(LUTPosition * LUTSIZE)] * SINLUTSIZE * scale;
	float phaseDistB = pdLUTBlendB[(int)(LUTPosition * LUTSIZE)] * SINLUTSIZE * scale;

	// Get the weighted blend of the two PD amounts
	float blend = pdBlend - (uint8_t)pdBlend;
	float phaseDist = ((1 - blend) * phaseDistA) + (blend * phaseDistB);

	// Add main wave position to phase distortion position and ensure in bounds
	float pos = sinLutWrap((LUTPosition * SINLUTSIZE) + phaseDist);

	return SineLUT[static_cast<uint32_t>(pos)];
}


uint32_t overload = 0;
volatile uint32_t timingTestStart = 0;
volatile uint32_t timingTest = 0;

void PhaseDistortion::CalcNextSamples()
{
	uint8_t polyNotes;
	float finetuneAdjust = 0.0f;
	float sampleOut1 = 0.0f, sampleOut2 = 0.0f, freq1 = 0.0f, freq2 = 0.0f;


	// Check status of action button
	if ((GPIOB->IDR & GPIO_IDR_IDR_12) == 0) {
		++actionBtnTime;
	} else {
		if (actionBtnTime > 30000) {			// long press = switch polyphonic/monophonic
			polyphonic = !polyphonic;

		} else if (actionBtnTime > 20) {		// Short press = activate envelope detection

			// Activate envelope detection mode
			if (polyphonic) {
				detectEnv = !detectEnv;
				GreenLED = 0;
				RedLED = 0;
				if (detectEnv) {
					envDetect.state = envDetectState::waitZero;
				}
			}
		}
		actionBtnTime = 0;
	}


	// Envelope detection - NB VCA is inverted
	if (detectEnv) {
		DetectEnvelope();
	}


	//	Coarse tuning (octaves) - add some hysteresis to prevent jumping
	if (coarseTune > ADC_array[ADC_CTune] + 128 || coarseTune < ADC_array[ADC_CTune] - 128) {
		coarseTune = ADC_array[ADC_CTune];
	}
	fineTune = ((15 * fineTune) + ADC_array[ADC_FTune]) / 16;		// Fine tune with smoothing


	// Analog selection of PD LUT table allows a smooth transition between LUTs for DAC1 and a stepped transition for DAC2
	pd1Type = ((31 * pd1Type) + ADC_array[ADC_Osc1Type]) / 32;
	float pdLut1 = static_cast<float>(pd1Type * (pd1LutCount - 1)) / 4096.0f;

	// Apply hysteresis on PD 2 discrete LUT selector
	if (pd2Type > ADC_array[ADC_Osc2Type] + 128 || pd2Type < ADC_array[ADC_Osc2Type] - 128) {
		pd2Type = ADC_array[ADC_Osc2Type];
	}
	uint8_t pdLut2 = pd2Type * pd2LutCount / 4096;
	bool pd2Resonant = (pdLut2 > 4);


	// Get PD amount from pot and CV ADCs; Currently seeing 0v as ~3000 and 5V as ~960 (converted to 0-5 for monophonic, 0-1.66 for polyphonic)
	float tmpPD1Scale = static_cast<float>(std::min((3800 - ADC_array[ADC_PD1CV]) + ADC_array[ADC_PD1Pot], 5000)) / (polyphonic ? 3000.0f : 1000.0f);
	pd1Scale = std::max(((pd1Scale * 31) + tmpPD1Scale) / 32, 0.0f);

	float tmpPD2Scale = static_cast<float>(std::min((4090 - ADC_array[ADC_PD2CV]) + ADC_array[ADC_PD2Pot], 5000)) / (polyphonic ? 3000.0f : 1000.0f);
	if (pd2Resonant) {
		pd2Scale = (pd2Scale * 0.995f) + (tmpPD2Scale * 0.005f);		// Heavily damp PD2 scale if using resonant waves to reduce jitter noise
	} else {
		pd2Scale = ((pd2Scale * 31) + tmpPD2Scale) / 32;
	}


	// Set up polyphonic and monophonic functions
	if (polyphonic) {
		// If note processing is taking too long kill oldest note
		extern uint32_t debugWorkTime;
		if (debugWorkTime > 1850) {
			//usb.midi.RemoveNote(0);
			usb.midi.midiNotes[0].envelope = MidiHandler::env::FR;
			++overload;
		}

		polyNotes = usb.midi.noteCount;
		float pb = usb.midi.pitchBendSemiTones * ((usb.midi.pitchBend - 8192) / 8192.0f);		// convert raw pitchbend to midi note number
		finetuneAdjust = pb - (2048.0f - fineTune) / 1024.0f;

	} else {
		// Get VCA levels (filtering out very low level VCA signals)
		if (ADC_array[ADC_VCA] > 4070) {
			VCALevel = 0.0f;
		} else if (ADC_array[ADC_VCA] < 30) {
			VCALevel = 1.0f;
		} else {
			VCALevel = ((VCALevel * 31) + (4096.0f - ADC_array[ADC_VCA]) / 4096) / 32;		// Convert ADC for VCA to float between 0 and 1 with damping
		}
		polyNotes = 1;
	}

	int8_t removeNote = -1;		// To enable removal of notes that have completed release envelope
	float polyLevel = 0;		// Get maximum level of polyphonic notes to display on LED


	for (uint8_t n = 0; n < polyNotes; ++n) {
		auto& midiNote = usb.midi.midiNotes[n];

		// Calculate frequencies
		if (polyphonic) {
			float lutIndex = (midiNote.noteValue + finetuneAdjust - midiLUTFirstNote) * midiLUTSize / midiLUTNotes;
			freq1 = MidiLUT[static_cast<uint32_t>(lutIndex)];
		} else {
			pitch = ((3 * pitch) + ADC_array[ADC_Pitch]) / 4;				// 1V/Oct input with smoothing
			freq1 = PitchLUT[(pitch + ((2048 - fineTune) / 32)) / 4];		// divide by four as there are 1024 items in DAC CV Voltage to Pitch Freq LUT and 4096 possible DAC voltage values
		}

		if (coarseTune > 3412) {
			freq1 *= 4;
		} else if (coarseTune > 2728) {
			freq1 *= 2;
		} else if (coarseTune < 682) {
			freq1 /= 4;
		} else if (coarseTune < 1364) {
			freq1 /= 2;
		}

		// octave down
		switch (relativePitch) {
			case NONE: 			freq2 = freq1; 			break;
			case OCTAVEDOWN:	freq2 = freq1 / 2.0f;	break;
			case OCTAVEUP: 		freq2 = freq1 * 2.0f;	break;
		}

		float& sp1 = polyphonic ? midiNote.samplePos1 : samplePos1;
		float& sp2 = polyphonic ? midiNote.samplePos2 : samplePos2;

		// jump forward to the next sample position
		sp1 += freq1;
		while (sp1 >= SAMPLERATE) {
			sp1 -= SAMPLERATE;
		}

		sp2 += freq2;
		while (sp2 >= SAMPLERATE) {
			sp2 -= SAMPLERATE;
		}


		// Calculate Phase Distortion envelope scaling (Attack and Decay phase scaling the PD amount from CV and pot)
		switch (midiNote.pdEnvelope) {
		case MidiHandler::pdEnv::A:
			midiNote.pdLevel += envelope.A_pd_Inc;
			if (midiNote.pdLevel >= 1.0f) {
				midiNote.pdEnvelope = MidiHandler::pdEnv::D;
			}
			break;
		case MidiHandler::pdEnv::D:
			midiNote.pdLevel -= envelope.D_pd_Inc;
			if (midiNote.pdLevel <= 0.0f) {
				midiNote.pdLevel = 0.0f;
				midiNote.pdEnvelope = MidiHandler::pdEnv::Off;
			}
			break;
		default:
			break;
		}



		// Calculate output as a float from -1 to +1 checking phase distortion and phase offset as required
		float sample1 = GetBlendPhaseDist(pdLut1, sp1 / SAMPLERATE, pd1Scale * midiNote.pdLevel);
		float sample2 = pd2Resonant ?
				GetResonantWave(sp2 / SAMPLERATE, pd2Scale * midiNote.pdLevel, pdLut2) :
				GetPhaseDist(LUTArray[pdLut2], sp2 / SAMPLERATE, pd2Scale * midiNote.pdLevel);
		if (ringModOn)			sample1 *= sample2;
		if (mixOn) 				sample2 += sample1;


		// Calculate envelope levels of polyphonic voices
		if (polyphonic && !detectEnv) {
			//timingTestStart = TIM5->CNT;

			switch (midiNote.envelope) {
			case MidiHandler::env::A:
				midiNote.vcaLevel += envelope.AInc;


				if (midiNote.vcaLevel >= 1.0f) {
					midiNote.envelope = MidiHandler::env::D;
				}
				break;

			case MidiHandler::env::D:
				midiNote.vcaLevel -= envelope.DInc;

				if (midiNote.vcaLevel <= envelope.S) {
					midiNote.envelope = MidiHandler::env::S;
				}
				break;

			case MidiHandler::env::S:
				midiNote.vcaLevel = envelope.S;
				break;

			case MidiHandler::env::R:
				midiNote.vcaLevel -= envelope.RInc;
				if (midiNote.vcaLevel <= 0.0f) {
					midiNote.vcaLevel = 0.0f;
					removeNote = n;
				}
				break;

			case MidiHandler::env::FR:
				midiNote.vcaLevel -= envelope.FRInc;
				if (midiNote.vcaLevel <= 0.0f) {
					midiNote.vcaLevel = 0.0f;
					removeNote = n;
				}
				break;
			}

			//timingTest = TIM5->CNT - timingTestStart;

			sample1 *= midiNote.vcaLevel;			// Scale sample output level based on envelope
			sample2 *= midiNote.vcaLevel;

			polyLevel += midiNote.vcaLevel;		// For LED display

		} else {
			sample1 *= VCALevel;
			sample2 *= VCALevel;
		}

		sampleOut1 += sample1;
		sampleOut2 += sample2;
	}

	if (removeNote >= 0) {
		usb.midi.RemoveNote(removeNote);
	}

	if (polyphonic) {
		sampleOut1 = Compress(sampleOut1, 0);
		sampleOut2 = Compress(sampleOut2, 1);

	}


	// Set DAC output values for when sample interrupt next fires (NB DAC and channels are reversed: ie DAC1 connects to channel2 and vice versa)
	DAC->DHR12R2 = static_cast<int32_t>((1.0f + sampleOut1) * 2047.0f);

#ifdef DEBUG_ENV_DETECT			// channel 2 is used to send out pulses to indicate envelope detection transitions
	if (!detectEnv) {
#endif

	DAC->DHR12R1 = static_cast<int32_t>((1.0f + sampleOut2) * 2047.0f);

#ifdef DEBUG_ENV_DETECT
	}
#endif

	// Set LED based envelope detection or poly level output
	if (!detectEnv) {
		if (polyphonic) {
			RedLED = static_cast<uint32_t>((polyLevel * 4095.0f) / usb.midi.polyCount);
		} else {
			GreenLED = static_cast<uint32_t>(VCALevel * 4095.0f);								// Set LED PWM level to VCA
		}
	}
}




float PhaseDistortion::Compress(float level, uint8_t channel)
{
	level = filter.a0 * level + filter.oldLevel[channel] * filter.b1;
	filter.oldLevel[channel] = level;

	float absLevel = std::fabs(level);

	// If current level will result in an output > 1.0f initiate compression to force this to maximum level
	if (absLevel * compLevel[channel] > 1.0f) {
		compState[channel] = CompState::hold;
		compLevel[channel] = 1.0f / absLevel;
		compHoldTimer[channel] = 0.0f;
	} else {
		switch (compState[channel]) {
		case CompState::hold:
			++compHoldTimer[channel];
			if (compHoldTimer[channel] >= compHold) {					// Start release phase once hold time has finished
				compState[channel] = CompState::release;
			}
			break;

		// Ramp down compression level
		case CompState::release:
			compLevel[channel] = compLevel[channel] + compRelease;
			if (compLevel[channel] >= defaultLevel) {
				compLevel[channel] = defaultLevel;
				compState[channel] = CompState::none;
			}
			break;

		case CompState::none:
			break;
		}
	}
	return compLevel[channel] * level;
}


// little routine to generate a pulse on channel 2 at each envelope detection change
#ifdef DEBUG_ENV_DETECT
bool debugState = false;
void DebugState() {
//	DAC->DHR12R1 = debugState ? 4095 : 0;
//	debugState = !debugState;
}
uint32_t lastSamples[256];		// debug
uint8_t lastSampPos = 0;		// debug
#else
#define DebugState()
#endif

void PhaseDistortion::DetectEnvelope()
{
	uint32_t lvl = 4096 - ADC_array[ADC_VCA];
	envDetect.smoothLevel = ((15 * envDetect.smoothLevel) + lvl) / 16;
	VCALevel = envDetect.smoothLevel / 4096.0f;				// While envelope detection is ongoing any output level will be set by incoming envelope
	++envDetect.counter;

#ifdef DEBUG_ENV_DETECT
	lastSamples[lastSampPos++] = envDetect.smoothLevel;		// debug
#endif

	switch  (envDetect.state) {
	case envDetectState::waitZero:			// Wait until level settles around 0
		RedLED = 0;
		GreenLED = 4095;

		DebugState();

		if (lvl < 10) {
			++envDetect.stateCount;
			if (envDetect.stateCount > 2) {
				envDetect.stateCount = 0;
				envDetect.state = envDetectState::waitAttack;
				envDetect.smoothLevel = lvl;
				envDetect.counter = 0;
			}
		} else {
			envDetect.stateCount = 0;
		}
		break;

	case envDetectState::waitAttack:

		// Flash red LED until starting
		if (envDetect.counter & (1 << 14)) {
			GreenLED = 0;
		} else {
			GreenLED = 4095;
		}

		if (envDetect.smoothLevel > 10) {
			++envDetect.stateCount;
			if (envDetect.stateCount > 3) {
				RedLED = 0;
				GreenLED = 4095;

				envDetect.state = envDetectState::Attack;
				envDetect.AttackTime = 0;
				envDetect.ReleaseTime = 0;
				envDetect.stateCount = 0;
				envDetect.maxLevel = 0;
				envDetect.counter = 0;

				DebugState();
			}
		} else {
			envDetect.stateCount = 0;
		}
		break;

	case envDetectState::Attack:

		++envDetect.AttackTime;

		// Wait for envelope to decrease - either switch to decay or release if stuck on high level for long time
		if (envDetect.smoothLevel < envDetect.maxLevel - 30) {
			envDetect.DecayTime = 0;

			if (envDetect.counter - envDetect.levelTime > 4000) {
				envDetect.SustainLevel = envDetect.maxLevel;
				envDetect.AttackTime = envDetect.levelTime;			// Attack time is moment maximum level was reached
				envDetect.minLevel = envDetect.smoothLevel;
				envDetect.state = envDetectState::Release;
			} else {
				RedLED = 4095;
				GreenLED = 0;
				envDetect.state = envDetectState::Decay;
				envDetect.stateCount = 0;
				envDetect.minLevel = envDetect.smoothLevel;
			}
			DebugState();
		} else {
			envDetect.stateCount = 0;
		}

		if (envDetect.smoothLevel > envDetect.maxLevel) {
			envDetect.maxLevel = envDetect.smoothLevel;
			envDetect.levelTime = envDetect.counter;
		}
		break;

	case envDetectState::Decay:
	{
		++envDetect.DecayTime;

		// If the smoothed level does not fall below minimum recorded level for x samples assume in sustain phase
		if (envDetect.smoothLevel >= envDetect.minLevel) {
			++envDetect.stateCount;

			if (envDetect.stateCount > 2000) {
				envDetect.DecayTime = envDetect.levelTime;
				envDetect.state = envDetectState::Sustain;
				envDetect.SustainLevel = envDetect.smoothLevel;
				DebugState();
			}
		} else {
			envDetect.stateCount = 0;
			envDetect.minLevel = envDetect.smoothLevel;
			envDetect.levelTime = envDetect.DecayTime;		// Capture the time envelope hits minimum detected level
		}

		// Decay to zero
		if (envDetect.smoothLevel < 10)	{
			envDetect.SustainLevel = 0;
			detectEnv = false;
		}

		break;
	}


	case envDetectState::Sustain:
		RedLED = 2000;
		GreenLED = 4095;

		// Once the current level has fallen 40 below the stable sustain level assume release phase
		if (envDetect.SustainLevel < 45 || envDetect.SustainLevel - envDetect.smoothLevel > 40) {
			envDetect.state = envDetectState::Release;

			envDetect.stateCount = 0;
			envDetect.stateCount2 = 0;
			envDetect.minLevel = envDetect.smoothLevel;
			DebugState();
		}

		break;

	case envDetectState::Release:

		++envDetect.ReleaseTime;
		RedLED = 4095;
		GreenLED = 0;

		// With very slow envelopes can hit the release phase early - attempt to detect
		if (envDetect.smoothLevel >= envDetect.minLevel) {
			if (envDetect.smoothLevel > 100) {
				++envDetect.stateCount2;
				if (envDetect.stateCount2 > 1000) {
					envDetect.state = envDetectState::Sustain;
					envDetect.SustainLevel = envDetect.smoothLevel;
					envDetect.DecayTime = envDetect.levelTime - envDetect.AttackTime;
					envDetect.ReleaseTime = 0;
					DebugState();
				}
			}
		} else {
			envDetect.stateCount2 = 0;
			envDetect.minLevel = envDetect.smoothLevel;
			envDetect.levelTime = envDetect.counter;
		}

		if (envDetect.smoothLevel < 10) {
			++envDetect.stateCount;
			if (envDetect.stateCount > 2) {
				detectEnv = false;
			}
		} else {
			envDetect.stateCount = 0;
		}

		break;
	}

	if (!detectEnv) {
		GreenLED = 0;
		RedLED = 0;
		envelope.A = std::max(envDetect.AttackTime, 1UL);
		envelope.D = std::max(envDetect.DecayTime, 1UL);
		envelope.S = static_cast<float>(envDetect.SustainLevel) / static_cast<float>(envDetect.maxLevel);
		envelope.R = std::max(envDetect.ReleaseTime, 1UL);

		envelope.UpdateIncrements();

		DebugState();
	}

}
