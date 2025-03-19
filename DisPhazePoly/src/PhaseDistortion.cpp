#include "PhaseDistortion.h"
#include "Calib.h"
#include "USB.h"



void PhaseDistortion::CalcNextSamples()
{
	debugPin.SetHigh();
	// Sample calculation timing
	sampleCalcWindow = TIM5->CNT;
	TIM5->EGR |= TIM_EGR_UG;

	// Check status of action button
	if (actionButton.LongPress()) {
		ChangePoly();

	} else if (actionButton.Pressed()) {
		// Activate envelope detection mode
		if (cfg.polyphonic) {
			detectEnv = !detectEnv;
			if (detectEnv) {
				envDetect.state = envDetectState::waitZero;
			}
		}
	}

	// envelope detection - NB VCA is inverted
	if (detectEnv) {
		DetectEnvelope();
	}

	//	Coarse tuning (octaves) - add some hysteresis to prevent jumping
	if (coarseTune > adc.CTune + 128 || coarseTune < adc.CTune - 128) {
		coarseTune = adc.CTune;
	}
	fineTune = ((15 * fineTune) + adc.FTune) / 16;		// Fine tune with smoothing


	// Analog selection of PD LUT table allows a smooth transition between LUTs for DAC1 and a stepped transition for DAC2
	pd1Type = ((31 * pd1Type) + adc.Osc1Type) / 32;
	float pdLut1 = static_cast<float>(pd1Type * (pd1LutCount - 1)) / 4096.0f;

	// Apply hysteresis on PD 2 discrete LUT selector
	if (pd2Type > adc.Osc2Type + 128 || pd2Type < adc.Osc2Type - 128) {
		pd2Type = adc.Osc2Type;
	}
	uint8_t pdLut2 = pd2Type * pd2LutCount / 4096;
	bool pd2Resonant = (pdLut2 > 4);


	// Get PD amount from pot and CV ADCs; Currently seeing 0v as ~3000 and 5V as ~960 (converted to 0-5 for monophonic, 0-1.66 for polyphonic)
	float tmpPD1Scale = static_cast<float>(std::min((3800 - adc.PD1CV) + adc.PD1Pot, 5000)) / (cfg.polyphonic ? 3000.0f : 1000.0f);
	pd1Scale = std::max(((pd1Scale * 31) + tmpPD1Scale) / 32, 0.0f);

	float tmpPD2Scale = static_cast<float>(std::min((4090 - adc.PD2CV) + adc.PD2Pot, 5000)) / (cfg.polyphonic ? 3000.0f : 1000.0f);
	if (pd2Resonant) {
		pd2Scale = (pd2Scale * 0.995f) + (tmpPD2Scale * 0.005f);		// Heavily damp PD2 scale if using resonant waves to reduce jitter noise
	} else {
		pd2Scale = ((pd2Scale * 31) + tmpPD2Scale) / 32;
	}

	OutputSamples output;

	if (cfg.polyphonic) {
		output = PolyOutput(pdLut1, pdLut2, pd2Resonant);
	} else {
		output = MonoOutput(pdLut1, pdLut2, pd2Resonant);
	}

	// Set DAC output values for when sample interrupt next fires (NB DAC and channels are reversed: ie DAC1 connects to channel2 and vice versa)
	DAC->DHR12R2 = static_cast<int32_t>((1.0f + output.out1) * 2047.0f);
	DAC->DHR12R1 = static_cast<int32_t>((1.0f + output.out2) * 2047.0f);

	SetLED();

	phaseDist.sampleCalcTime = TIM5->CNT;

	debugPin.SetLow();
}



PhaseDistortion::OutputSamples PhaseDistortion::MonoOutput(float pdLut1, uint8_t pdLut2, float pd2Resonant)
{
	// Get VCA levels (filtering out very low level VCA signals)
	if (adc.VCA > 4070) {
		VCALevel = 0.0f;
	} else if (adc.VCA < 30) {
		VCALevel = 1.0f;
	} else {
		VCALevel = ((VCALevel * 31) + (4096.0f - adc.VCA) / 4096) / 32;		// Convert ADC for VCA to float between 0 and 1 with damping
	}

	// Calculate frequencies
	pitch = ((3 * pitch) + adc.Pitch_CV) / 4;				// 1V/Oct input with smoothing
	float freq1 = calib.pitchLUT[(pitch + ((2048 - fineTune) / 32))];
	//float freq1 = calib.pitchLUT[adc.Pitch_CV];				// For testing
	float freq2;

	OctaveCalc(freq1, freq2);

	// jump forward to the next sample position
	samplePos1 += (uint32_t)freq1;
	samplePos2 += (uint32_t)freq2;

	// Calculate output as a float from -1 to +1 checking phase distortion and phase offset as required
	float sample1 = GetBlendPhaseDist(pdLut1, samplePos1, pd1Scale);
	float sample2 = pd2Resonant ?
			GetResonantWave(pdLut2, samplePos2, pd2Scale) :
			GetPhaseDist(LUTArray[pdLut2], samplePos2, pd2Scale);

	if (ringModSwitch.IsHigh())
		sample1 *= sample2;
	if (mixSwitch.IsHigh())
		sample2 += sample1;

	OutputSamples output;
	output.out1 = sample1 * VCALevel;
	output.out2 = sample2 * VCALevel;

	return output;
}


PhaseDistortion::OutputSamples PhaseDistortion::PolyOutput(float pdLut1, uint8_t pdLut2, float pd2Resonant)
{
	// If note processing is taking too long kill oldest note
	if (sampleCalcTime > maxSampleTime) {
		usb.midi.midiNotes[0].envelope = MidiHandler::env::FR;
	}

	uint8_t polyNotes = usb.midi.noteCount;
	polyLevel = 0.0f;
	int8_t removeNote = -1;		// To enable removal of notes that have completed release envelope
	OutputSamples output;

	float pb = usb.midi.pitchBendSemiTones * ((usb.midi.pitchBend - 8192) / 8192.0f);		// convert raw pitchbend to midi note number
	float finetuneAdjust = pb - (2048.0f - fineTune) / 1024.0f;
//	float finetuneAdjust = 0.0f;			// for testing
	uint32_t pdlut1Int = (uint32_t)std::round(pdLut1);

	for (uint8_t n = 0; n < polyNotes; ++n) {
		auto& midiNote = usb.midi.midiNotes[n];

		// Calculate frequencies
		float lutIndex = (midiNote.noteValue + finetuneAdjust - midiLUTFirstNote) * midiLUTSize / midiLUTNotes;
		float freq1 = MidiLUT[static_cast<uint32_t>(lutIndex)];
		float freq2;

		OctaveCalc(freq1, freq2);

		// Jump forward to the next sample position
		midiNote.samplePos1 += (uint32_t)freq1;
		midiNote.samplePos2 += (uint32_t)freq2;


		// Calculate output as a float from -1 to +1 checking phase distortion and phase offset as required
		float sample1 = GetPhaseDist(LUTArray[pdlut1Int], midiNote.samplePos1, pd1Scale);
		float sample2 = pd2Resonant ?
				GetResonantWave(pdLut2, midiNote.samplePos2, pd2Scale) :
				GetPhaseDist(LUTArray[pdLut2], midiNote.samplePos2, pd2Scale);


		if (ringModSwitch.IsHigh())
			sample1 *= sample2;
		if (mixSwitch.IsHigh())
			sample2 += sample1;


		// Calculate envelope levels of polyphonic voices
		if (detectEnv) {
			sample1 *= VCALevel;
			sample2 *= VCALevel;

		} else {

			switch (midiNote.envelope) {
			case MidiHandler::env::A:
				midiNote.vcaLevel += cfg.envelope.AInc;

				if (midiNote.vcaLevel >= 1.0f) {
					midiNote.envelope = MidiHandler::env::D;
				}
				break;

			case MidiHandler::env::D:
				midiNote.vcaLevel -= cfg.envelope.DInc;

				if (midiNote.vcaLevel <= cfg.envelope.S) {
					midiNote.envelope = MidiHandler::env::S;
				}
				break;

			case MidiHandler::env::S:
				midiNote.vcaLevel = cfg.envelope.S;
				break;

			case MidiHandler::env::R:
				midiNote.vcaLevel -= cfg.envelope.RInc;
				if (midiNote.vcaLevel <= 0.0f) {
					midiNote.vcaLevel = 0.0f;
					removeNote = n;
				}
				break;

			case MidiHandler::env::FR:
				midiNote.vcaLevel -= cfg.envelope.FRInc;
				if (midiNote.vcaLevel <= 0.0f) {
					midiNote.vcaLevel = 0.0f;
					removeNote = n;
				}
				break;
			}

			sample1 *= midiNote.vcaLevel;			// Scale sample output level based on envelope
			sample2 *= midiNote.vcaLevel;

			polyLevel += midiNote.vcaLevel;			// For LED display
		}

		output.out1 += sample1;
		output.out2 += sample2;
	}

	if (removeNote >= 0) {
		usb.midi.RemoveNote(removeNote);
	}
	output.out1 = Compress(output.out1, 0);
	output.out2 = Compress(output.out2, 1);

	return output;
}


// models waves 6-8 of the Casio CZ which are saw/triangle envelopes into which harmonics are added as the phase distortion increases
float PhaseDistortion::GetResonantWave(const uint8_t pdLut2, const uint32_t LUTPosition, float scale)
{
	scale = ((scale / 5.0f) * 23.0f) + 1.0f;		// Sets number of sine waves per cycle: scale input 0-5 to 1-24 (original only went to 16)

	// offset to 3/4 of the way through the sine wave so each cycle starts at the flat top of the wave
	constexpr float sineOffset = static_cast<float>(3 * (sinLutSize / 4));

	// Scale the amplitude of the cycle from 1 to 0 to create a saw tooth type envelope on each cycle
	constexpr float lutScale = 1.0f / std::pow(2.0f, 32.0f);
	const float lutPosition = lutScale * LUTPosition;

	float ampMod = 1.0f;
	if (pdLut2 == 5) {								// Saw tooth - amplitude linearly reduces over full cycle
		ampMod = (1.0f - lutPosition);
	} else if (lutPosition > 0.5f) {				// Both triangle (wave 7) and loud saw (wave 8) fade out over second half of cycle
		ampMod = (2.0f * (1.0f - lutPosition));
	} else if (pdLut2 == 6) {						// Triangle (wave 7) fades in over first half of cycle
		ampMod = (lutPosition * 2.0f);
	}

	float pos = sinLutWrap(((LUTPosition >> 18) * scale) + sineOffset);
	float sineSample = SineLUT[static_cast<uint32_t>(pos)];
	sineSample = (sineSample + 1.0f) * ampMod;		// Offset so all positive and then apply amplitude envelope to create sawtooth

	return sineSample - 1.0f;
}


// Generate a phase distorted sine wave - pass LUT containing PD offsets, LUT position as a fraction of the wave cycle and a scaling factor
float PhaseDistortion::GetPhaseDist(const float* PdLUT, const uint32_t LUTPosition, const float scale)
{
	constexpr float mult = std::pow(2.0f, 32.0f);
	float phaseDist = mult * PdLUT[LUTPosition >> 22] * scale;			// Phase distortion LUTs are 2^10, LUT position is 2^32

	return SineLUT[(uint32_t)(LUTPosition  + phaseDist) >> 18];
}


// Generate a phase distorted sine wave - pass LUT containing PD offsets, LUT position as a fraction of the wave cycle and a scaling factor
float PhaseDistortion::GetBlendPhaseDist(const float pdBlend, const uint32_t LUTPosition, const float scale)
{
	// Get the two PD LUTs that will be blended
	const float* pdLUTBlendA = LUTArray[static_cast<uint8_t>(pdBlend)];
	const float* pdLUTBlendB = LUTArray[static_cast<uint8_t>(pdBlend + 1) % pd1LutCount];

	// Get the values from each LUT for the sample position
	float phaseDistA = pdLUTBlendA[LUTPosition >> 22] * scale;
	float phaseDistB = pdLUTBlendB[LUTPosition >> 22] * scale;

	// Get the weighted blend of the two PD amounts
	float blend = pdBlend - (uint8_t)pdBlend;
	float phaseDist = ((1 - blend) * phaseDistA) + (blend * phaseDistB);

	// Add main wave position to phase distortion position and ensure in bounds
	return SineLUT[(uint32_t)(LUTPosition  + phaseDist) >> 18];
}



float PhaseDistortion::Compress(float level, uint8_t channel)
{
	float absLevel = std::fabs(level);

	// If current level will result in an output > 1.0f initiate compression to force this to maximum level
	if (absLevel * cfg.compressor.threshold > 1.0f) {
		compState[channel] = CompState::hold;
		compLevel[channel] = 1.0f / absLevel;
		compHoldTimer[channel] = 0.0f;
	} else {
		switch (compState[channel]) {
		case CompState::hold:
			++compHoldTimer[channel];
			if (compHoldTimer[channel] >= compHoldSamples) {					// Start release phase once hold time has finished
				compState[channel] = CompState::release;
			}
			break;

		// Ramp down compression level
		case CompState::release:
			compLevel[channel] = compLevel[channel] + cfg.compressor.release;
			if (compLevel[channel] >= cfg.compressor.threshold) {
				compLevel[channel] = cfg.compressor.threshold;
				compState[channel] = CompState::none;
			}
			break;

		case CompState::none:
			break;
		}
	}
	return compLevel[channel] * level;
}


void PhaseDistortion::DetectEnvelope()
{
	uint32_t lvl = 4096 - adc.VCA;
	envDetect.smoothLevel = ((15 * envDetect.smoothLevel) + lvl) / 16;
	VCALevel = envDetect.smoothLevel / 4096.0f;				// While envelope detection is ongoing any output level will be set by incoming envelope
	++envDetect.counter;


	switch  (envDetect.state) {
	case envDetectState::waitZero:			// Wait until level settles around 0
		RedLED = 0;
		GreenLED = 4095;

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
		cfg.envelope.A = std::max(envDetect.AttackTime, 1UL);
		cfg.envelope.D = std::max(envDetect.DecayTime, 1UL);
		cfg.envelope.S = static_cast<float>(envDetect.SustainLevel) / static_cast<float>(envDetect.maxLevel);
		cfg.envelope.R = std::max(envDetect.ReleaseTime, 1UL);

		cfg.envelope.UpdateIncrements();
	}

}


void PhaseDistortion::OctaveCalc(float& freq1, float& freq2)
{
	if (coarseTune > 3412) {
		freq1 *= 4;
	} else if (coarseTune > 2728) {
		freq1 *= 2;
	} else if (coarseTune < 682) {
		freq1 /= 4;
	} else if (coarseTune < 1364) {
		freq1 /= 2;
	}

	freq2 = freq1;
	if (octaveUp.IsHigh()) {
		freq2 *= 2.0f;
	} else if (octaveDown.IsHigh()) {
		freq2 /= 2.0f;
	}
}


void PhaseDistortion::SetLED()
{
	if (ledState != oldLedState) {
		if (ledState == LEDState::SwitchToMono || ledState == LEDState::SwitchToPoly) {
			ledCounter = 0;
		}
		if (ledState == LEDState::Mono) {
			RedLED = 0;
		}
		if (ledState == LEDState::Poly) {
			GreenLED = 0;
		}

		oldLedState = ledState;
	}

	if (ledState == LEDState::SwitchToMono || ledState == LEDState::SwitchToPoly) {
		static constexpr uint32_t fadeTime = 70000;
		if (++ledCounter >= fadeTime) {
			ledState = (ledState == LEDState::SwitchToPoly) ? LEDState::Poly : LEDState::Mono;
			ledCounter = 0;
		}
		if (ledState == LEDState::SwitchToMono) {
			RedLED = 4095 * (fadeTime - ledCounter) / fadeTime;
			GreenLED = 4095 * ledCounter / fadeTime;
		} else {
			GreenLED= 4095 * (fadeTime - ledCounter) / fadeTime;
			RedLED = 4095 * ledCounter / fadeTime;
		}
	} else if (ledState == LEDState::Mono) {
		GreenLED = (uint32_t)(VCALevel * 4095.0f);
	} else if (ledState == LEDState::Poly && !detectEnv) {
		RedLED = static_cast<uint32_t>((polyLevel * 4095.0f) / usb.midi.polyCount);
	}
}


void PhaseDistortion::SetSampleRate()
{
	uint32_t sampleTime = (SystemCoreClock / 4) / (cfg.polyphonic ? sampleRatePoly : sampleRateMono);
	TIM3->PSC = sampleTime;
	maxSampleTime = (sampleTime * 2) - 100;
}


void PhaseDistortion::ChangePoly()
{
	cfg.polyphonic = !cfg.polyphonic;
	config.ScheduleSave();
	if (!cfg.polyphonic) {
		detectEnv = false;
		ledState = LEDState::SwitchToMono;
	} else {
		ledState = LEDState::SwitchToPoly;
	}
	SetSampleRate();
}


inline float PhaseDistortion::sinLutWrap(float pos)
{
	while (pos >= sinLutSize) {
		pos -= sinLutSize;
	}
	while (pos < 0) {
		pos =  sinLutSize + pos;
	}
	return pos;
}


float PhaseDistortion::FastTanh(const float x)
{
	// Apply FastTan approximation to limit sample from -1 to +1
	float x2 = x * x;
	float a = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
	float b = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
	return a / b;
}

void PhaseDistortion::UpdateConfig()
{
	phaseDist.compHoldSamples = (phaseDist.cfg.compressor.holdTime * sampleRatePoly) / 1000;		// Convert ms to sample count
	phaseDist.ledState = phaseDist.cfg.polyphonic ? LEDState::Poly : LEDState::Mono;
	phaseDist.SetSampleRate();
}
