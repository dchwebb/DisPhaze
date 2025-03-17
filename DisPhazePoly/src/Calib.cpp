#include <Calib.h>
#include <cstdio>


void Calib::UpdatePitchLUT()
{
	for (uint32_t i = 0; i < adcMax + 1; ++i) {
		calib.pitchLUT[i] = calib.cfg.pitchBase * std::pow(2.0f, calib.cfg.pitchMult * i) / sampleRate;
	}
}


void Calib::Calibrate(char key)
{
	// Can be called either from main (key = 0) to update calibration values or from serial console (key != 0) to update state
	if (key == 0 && calibrating) {
		if (state == State::Octave0) {
			adcOctave0 += (float)adc.Pitch_CV;
			if (++calibCount == 2000) {
				printf("Pitch reading: %d (expected around 3572)\r\n"
						"Apply 1V to Pitch input\r\n"
						"Enter 'y' to continue, 'x' to cancel\r\n",
						(int)(adcOctave0 / 2000.0f)
						);
				state = State::Waiting1;
			}
		}
		if (state == State::Octave1) {
			adcOctave1 += (float)adc.Pitch_CV;
			if (++calibCount == 2000) {
				printf("Pitch reading: %d (expected around 2985)\r\n"
						"Enter 'y' to save calibration, 'x' to cancel\r\n",
						(int)(adcOctave1 / 2000.0f)
						);
				state = State::PendingSave;
			}
		}
	}

	// Start instruction
	if (key == 's') {
		calibrating = true;
		printf("Calibrating\r\n"
				"Apply 0V (lowest C) to Pitch input\r\n"
				"Enter 'y' to continue, 'x' to cancel\r\n");
		state = State::Waiting0;
	}

	if (key == 'y') {
		switch (state) {
		case State::Waiting0:
			state = State::Octave0;
			adcOctave0 = (float)adc.Pitch_CV;
			calibCount = 1;
			break;
		case State::Waiting1:
			state = State::Octave1;
			adcOctave1 = (float)adc.Pitch_CV;
			calibCount = 1;
			break;
		case State::PendingSave: {
			state = State::Waiting0;
			const float voltSpread = (adcOctave0 - adcOctave1) / 2000.0f;

			// 65.41f is frequency at 1V
			// FIXME - experimental adjustments of both Base and Pitch to improve accuracy on test module
			cfg.pitchBase = hertzAt1V / std::pow(2.0f, (-adcOctave1 / 2000.0f) / voltSpread) - 20;
			cfg.pitchMult = -1.0f / (voltSpread + 2);

			printf("Calibration saved\r\n");
			config.SaveConfig(true);
			UpdatePitchLUT();
			calibrating = false;
			}
			break;
		default:
			break;
		}
	}

	if (key == 'x') {
		printf("Cancelled calibration\r\n");
		calibrating = false;
	}

}



void Calib::UpdateConfig()
{
	if (calib.cfg.pitchBase == 0.0f) {
		calib.cfg.pitchBase = pitchBaseDef;
	}
	if (calib.cfg.pitchMult == 0.0f) {
		calib.cfg.pitchMult = pitchMultDef;
	}
	calib.UpdatePitchLUT();
}

