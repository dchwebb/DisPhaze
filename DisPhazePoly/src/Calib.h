#pragma once

#include "initialisation.h"
#include "configManager.h"
#include "LUT.h"
#include <cmath>

class Calib {
public:
	void Calibrate(char key = 0);
	static void UpdatePitchLUT();
	static void UpdateConfig();

	// 0v = 3572; 1v = 2985; 2v = 2397; 3v = 1808; 4v = 1219; 5v = 631; 6V = 42
	// C: 16.35 Hz 32.70 Hz; 65.41 Hz; 130.81 Hz; 261.63 Hz; 523.25 Hz; 1046.50 Hz; 2093.00 Hz; 4186.01 Hz

	// Calculations: 587 is difference in cv between two octaves; 2985 is cv at 1v and 65.41 is desired Hz at 1v

	static constexpr float hertzAt1V = 65.41f;
	static constexpr float pitchBaseDef = hertzAt1V / std::pow(2.0, -2985.0f / 587.0f);
	static constexpr float pitchMultDef = -1.0f / 587.0f;

	struct {
		float pitchBase = pitchBaseDef;
		float pitchMult = pitchMultDef;
	} cfg;

	ConfigSaver configSaver = {
		.settingsAddress = &cfg,
		.settingsSize = sizeof(cfg),
		.validateSettings = UpdateConfig
	};

	float pitchLUT[adcMax + 1];

	bool calibrating;			// Triggered by serial console
	enum class State {Waiting0, Waiting1, Octave0, Octave1, PendingSave};
	State state;

private:
	float adcOctave0;
	float adcOctave1;
	uint32_t calibCount = 0;
};

extern Calib calib;
