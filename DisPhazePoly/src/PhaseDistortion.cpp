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

	lastSample = (lastSample * 0.85f) + (0.15f * (sineSample - 1.0f));			// Remove offset and damp
	return lastSample;
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



uint32_t lastSamples[256];
uint8_t lastSampPos = 0;

void PhaseDistortion::DetectEnvelope()
{
	uint32_t lvl = 4096 - ADC_array[ADC_VCA];
	envDetect.smoothLevel = ((15 * envDetect.smoothLevel) + lvl) / 16;
	VCALevel = envDetect.smoothLevel / 4096.0f;
	lastSamples[lastSampPos++] = envDetect.smoothLevel;

	switch  (envDetect.state) {
	case envDetectState::waitZero:			// Wait until level settles around 0
		RedLED = 4095;
		GreenLED = 4095;
		DAC->DHR12R1 = 0;

		if (lvl < 10) {
			++envDetect.stateCount;
			if (envDetect.stateCount > 2) {
				envDetect.stateCount = 0;
				envDetect.state = envDetectState::waitAttack;
				envDetect.smoothLevel = lvl;
			}
		} else {
			envDetect.stateCount = 0;
		}
		break;

	case envDetectState::waitAttack:
		if (envDetect.smoothLevel > 10) {
			++envDetect.stateCount;
			if (envDetect.stateCount > 3) {
				envDetect.state = envDetectState::Attack;
				envDetect.AttackTime = 0;
				envDetect.stateCount = 0;
				envDetect.maxLevel = 0;

				DAC->DHR12R1 = 4095;		// debug
			}
		} else {
			envDetect.stateCount = 0;
		}
		break;

	case envDetectState::Attack:

		++envDetect.AttackTime;
		envDetect.maxLevel = std::max(envDetect.maxLevel, envDetect.smoothLevel);

		// Count envelope not increasing
		if (envDetect.smoothLevel + 10 < envDetect.maxLevel && envDetect.maxLevel > 2000) {
			++envDetect.stateCount;
			if (envDetect.stateCount > 8) {
				envDetect.state = envDetectState::Decay;
				envDetect.DecayTime = 0;
				envDetect.stateCount = 0;
				envDetect.minLevel = envDetect.smoothLevel;
				DAC->DHR12R1 = 0;		// debug
			}
		} else {
			envDetect.stateCount = 0;
		}

		break;

	case envDetectState::Decay:
	{
		RedLED = 4095;
		GreenLED = 0;

		++envDetect.DecayTime;

		if (envDetect.smoothLevel >= envDetect.minLevel) {
			++envDetect.stateCount;

			if (envDetect.stateCount > 50) {
				envDetect.state = envDetectState::Sustain;
				envDetect.SustainTime = 0;
				envDetect.SustainStable = false;
				envDetect.SustainLevel = envDetect.smoothLevel;
				envDetect.SustainStableLevel = envDetect.smoothLevel;
				envDetect.stateCount = 0;
				DAC->DHR12R1 = 4095;		// debug
			}
		} else {
			envDetect.stateCount = 0;
			envDetect.minLevel = envDetect.smoothLevel;
		}


		// Decay to zero
		if (envDetect.smoothLevel < 10 && envDetect.stateCount > 2)	{

		}

		break;
	}

	case envDetectState::Sustain:
		RedLED = 0;
		GreenLED = 4095;

		// Every 2000 samples check if the sustain level is unchanged. If so then lock that value to enable detection of slow releases
		if (envDetect.SustainTime == 2000) {
			if (envDetect.SustainStableLevel - (int32_t)envDetect.smoothLevel > 2) {
				envDetect.SustainTime = 0;
			} else {
				envDetect.state = envDetectState::SustainStable;
				DAC->DHR12R1 = 0;		// debug
			}
			envDetect.SustainStableLevel = envDetect.SustainLevel;
		} else {
			++envDetect.SustainTime;
		}

		envDetect.SustainLevel = (envDetect.SustainLevel * 15 + envDetect.smoothLevel) / 16;		// Average sustain level
		break;

	case envDetectState::SustainStable:
		RedLED = 3000;
		GreenLED = 4095;

		if (envDetect.SustainStableLevel - (int32_t)envDetect.smoothLevel > 40) {
			envDetect.state = envDetectState::Release;
			envDetect.ReleaseTime = 0;
			envDetect.stateCount = 0;
			DAC->DHR12R1 = 4095;		// debug
		} else {
			envDetect.stateCount = 0;
		}

		break;

	case envDetectState::Release:

		++envDetect.ReleaseTime;
		RedLED = 4095;
		GreenLED = 0;

		if (envDetect.smoothLevel < 10) {
			++envDetect.stateCount;

			if (envDetect.stateCount > 2) {
				GreenLED = 0;
				RedLED = 0;
				detectEnv = false;
				DAC->DHR12R1 = 4095;		// debug
			}
		} else {
			envDetect.stateCount = 0;
		}

		break;
	}

}


volatile uint32_t shortPressCnt = 0;
volatile uint32_t longPressCnt = 0;

void PhaseDistortion::CalcNextSamples()
{
	uint8_t polyNotes;
	float finetuneAdjust = 0.0f;
	float sampleOut1 = 0.0f, sampleOut2 = 0.0f, freq1 = 0.0f, freq2 = 0.0f;


	// Check status of action button
	if ((GPIOB->IDR & GPIO_IDR_IDR_12) == 0) {
		++actionBtnTime;
	} else {
		if (actionBtnTime > 30000) {			// long press
			++longPressCnt;
		} else if (actionBtnTime > 20) {		// Short press
			++shortPressCnt;

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
	fineTune = ((31 * fineTune) + ADC_array[ADC_FTune]) / 32;		// Fine tune with smoothing


	// Analog selection of PD LUT table allows a smooth transition between LUTs for DAC1 and a stepped transition for DAC2
	pd1Type = ((31 * pd1Type) + ADC_array[ADC_Osc1Type]) / 32;
	float pdLut1 = static_cast<float>(pd1Type * (pd1LutCount - 1)) / 4096.0f;

	// Apply hysteresis on PD 2 discrete LUT selector
	if (pd2Type > ADC_array[ADC_Osc2Type] + 128 || pd2Type < ADC_array[ADC_Osc2Type] - 128) {
		pd2Type = ADC_array[ADC_Osc2Type];
	}
	uint8_t pdLut2 = pd2Type * pd2LutCount / 4096;
	bool pd2Resonant = (pdLut2 > 4);


	// Get PD amount from pot and CV ADCs; Currently seeing 0v as ~3000 and 5V as ~960 (converted to 0.0f - 5.0f)
	float tmpPD1Scale = static_cast<float>(std::min((3800 - ADC_array[ADC_PD1CV]) + ADC_array[ADC_PD1Pot], 5000)) / 1000.0f;	// Convert PD amount for OSC1
	pd1Scale = std::max(((pd1Scale * 31) + tmpPD1Scale) / 32, 0.0f);

	float tmpPD2Scale = static_cast<float>(std::min((4090 - ADC_array[ADC_PD2CV]) + ADC_array[ADC_PD2Pot], 5000)) / 1000.0f;	// Convert PD amount for OSC2
	if (pd2Resonant) {
		pd2Scale = (pd2Scale * 0.995f) + (tmpPD2Scale * 0.005f);		// Heavily damp PD2 scale if using resonant waves to reduce jitter noise
	} else {
		pd2Scale = ((pd2Scale * 31) + tmpPD2Scale) / 32;
	}



	// Set up polyphonic and monophonic functions
	if (polyphonic) {
		polyNotes = usb.midi.noteCount;
		float pb = usb.midi.pitchBendSemiTones * ((usb.midi.pitchBend - 8192) / 8192.0f);		// convert raw pitchbend to midi note number
		finetuneAdjust = pb - (2048.0f - fineTune) / 512.0f;
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

	int8_t removeNote = -1;		// To enable removeal of notes that have completed release envelope
	float polyLevel = 0;		// Get maximum level of polyphonic notes to display on LED

	for (uint8_t n = 0; n < polyNotes; ++n) {
		auto& midiNote = usb.midi.midiNotes[n];

		// Calculate frequencies
		if (polyphonic) {
			float lutIndex = (midiNote.noteValue + finetuneAdjust - midiLUTFirstNote) * midiLUTSize / midiLUTNotes;
			freq1 = MidiLUT[static_cast<uint32_t>(lutIndex)];

			++midiNote.envTime;
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

		// Calculate output as a float from -1 to +1 checking phase distortion and phase offset as required
		float sample1 = GetBlendPhaseDist(pdLut1, sp1 / SAMPLERATE, pd1Scale);
		float sample2 = pd2Resonant ?
				GetResonantWave(sp2 / SAMPLERATE, pd2Scale, pdLut2) :
				GetPhaseDist(LUTArray[pdLut2], sp2 / SAMPLERATE, pd2Scale);
		if (ringModOn)			sample1 *= sample2;
		if (mixOn) 				sample2 += sample1;


		// Calculate envelope levels of polyphonic voices
		if (polyphonic && !detectEnv) {
			float sampleLevel = 0.0f;
			switch (midiNote.envelope) {
			case MidiHandler::env::A:
				sampleLevel = static_cast<float>(midiNote.envTime) / envelope.A;
				midiNote.releaseLevel = sampleLevel;

				if (midiNote.envTime >= envelope.A) {
					midiNote.envTime = 0;
					midiNote.envelope = MidiHandler::env::D;
				}
				break;


			case MidiHandler::env::D:
				sampleLevel = 1.0f - static_cast<float>(midiNote.envTime) / envelope.D;
				midiNote.releaseLevel = sampleLevel;

				if (sampleLevel <= envelope.S) {
					midiNote.envTime = 0;
					midiNote.envelope = MidiHandler::env::S;
				}
				break;


			case MidiHandler::env::S:

				sampleLevel = envelope.S;

				break;

			case MidiHandler::env::R:
				sampleLevel = midiNote.releaseLevel - static_cast<float>(midiNote.envTime) / envelope.R;

				if (sampleLevel <= 0.0f) {
					sampleLevel = 0.0f;
					removeNote = n;
				}
				break;
			}

			sample1 *= sampleLevel;		// Scale sample output level based on envelope
			sample2 *= sampleLevel;

			polyLevel += sampleLevel;

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


	// Set DAC output values for when sample interrupt next fires (NB DAC and channels are reversed: ie DAC1 connects to channel2 and vice versa)
	DAC->DHR12R2 = static_cast<int32_t>((1.0f + Compress(sampleOut1)) * 2047.0f);

	// Debug - remove if
	if (!detectEnv) {
	DAC->DHR12R1 = static_cast<int32_t>((1.0f + Compress(sampleOut2)) * 2047.0f);
	}
	// Set LED based envelope detection or poly level output
	if (!detectEnv) {
		if (polyphonic) {
			RedLED = static_cast<uint32_t>((polyLevel * 4095.0f) / usb.midi.polyCount);
		} else {
			GreenLED = static_cast<uint32_t>(VCALevel * 4095.0f);								// Set LED PWM level to VCA
		}
	}
}

uint32_t overload = 0;


// Fast Tanh Algorithm source: https://varietyofsound.wordpress.com/2011/02/14/efficient-tanh-computation-using-lamberts-continued-fraction/
float PhaseDistortion::Compress(float x)
{
	const float compStart = 0.70f;

	x = 0.40f * x;

	float absX = std::fabs(x);
	if (absX < compStart) {
		return x;
	} else if (x >= 1.4f) {
		++overload;
		return 1.0f;
	} else if (x <= -1.4f) {
		return -1.0f;
		++overload;
	} else  {
		float comp = 1.0f - (absX - compStart) * 0.41f;
		return std::clamp(comp * x, -1.0f, 1.0f);
	}


	//return std::clamp(0.65f * x, -1.0f, 1.0f);


	/*
	// Algorithm to approximate tanh compression
	float x2 = x * x;
	float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
	float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
	return a / b;
	*/
}
