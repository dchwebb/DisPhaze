#include <config.h>

extern bool dacRead;

void Config::Calibrate() 			// Checks if calibrate button has been pressed and runs calibration routine if so
{
	// Toggle calibration mode when button pressed using simple debouncer
//	if (READ_BIT(GPIOB->IDR, GPIO_IDR_IDR_5)) {
//		calibBtn++;
//
//		if (calibBtn == 200) {
//			calibrating = !calibrating;
//
//			// write calibration settings to flash (These are applied when CreateLUTs() is run on startup)
//			if (!calibrating) {
//				SaveConfig();
//				CreateLUTs();
//			}
//		}
//	} else {
//		calibBtn = 0;
//	}


	if (calibrating) {
		// Generate square wave
		if (dacRead) {
			static float pitch, fineTune, samplePos1;
			tuningOffset = ((31.0f * tuningOffset) + ((ADC_array[ADC_PD1Pot] - 2048)) / 20.0f) / 32.0f;		// Use pd1 amount pot to set the tuning offset
			tuningSpread = ((31.0f * tuningSpread) + ((ADC_array[ADC_PD2Pot] - 2048)) / 100.0f) / 32.0f;	// Use pd2 amount pot to set the tuning spread

			pitch = ((31.0f * pitch) + ADC_array[ADC_Pitch]) / 32.0f;
			fineTune = ((31.0f * fineTune) + ADC_array[ADC_FTune]) / 32.0f;
			float adjPitch = pitch + (2048.0f - fineTune) / 32.0f;

			// Defaults: PITCH_SPREAD -583.0f, PITCH_OFFSET 2299.0f
			samplePos1 += ((PITCH_OFFSET + tuningOffset) * std::pow(2.0f, adjPitch / (PITCH_SPREAD + tuningSpread)));
			while (samplePos1 >= SAMPLERATE) {
				samplePos1-= SAMPLERATE;
			}

			DAC->DHR12R1 = (samplePos1 > SAMPLERATE / 2) ? 4095: 0;
			dacRead = 0;

		}

	}
}

// Write calibration settings to Flash memory
void Config::SaveConfig()
{
	configValues cv;
	SetConfig(cv);

	__disable_irq();						// Disable Interrupts
	FlashUnlock();							// Unlock Flash memory for writing
	FLASH->SR = FLASH_ALL_ERRORS;			// Clear error flags in Status Register
	FlashEraseSector(10);					// Erase sector 10 (has to be erased before write)
	FlashProgram(ADDR_FLASH_SECTOR_10, reinterpret_cast<uint32_t*>(&cv), sizeof(cv));
	FlashLock();							// Lock the Flash memory
	__enable_irq(); 						// Enable Interrupts
}


void Config::SetConfig(configValues &cv)
{
	cv.offset = tuningOffset;
	cv.spread = tuningSpread;
}


// Restore calibration settings from flash memory
void Config::RestoreConfig()
{
	// create temporary copy of settings from memory to check if they are valid
	configValues cv;
	memcpy(reinterpret_cast<uint32_t*>(&cv), ADDR_FLASH_SECTOR_10, sizeof(cv));

	if (strcmp(cv.StartMarker, "CFG") == 0 && strcmp(cv.EndMarker, "END") == 0 && cv.Version == CONFIG_VERSION) {
		tuningOffset = cv.offset;
		tuningSpread = cv.spread;
	}
}


void Config::FlashUnlock()
{
	if ((FLASH->CR & FLASH_CR_LOCK) != 0) {
		FLASH->KEYR = 0x45670123;
		FLASH->KEYR = 0xCDEF89AB;
	}
}


void Config::FlashLock()
{
	FLASH->CR |= FLASH_CR_LOCK;
}


void Config::FlashEraseSector(uint8_t sector)
{
	FlashWaitForLastOperation();

	FLASH->CR &= FLASH_CR_PSIZE_Msk;
	FLASH->CR |= FLASH_PSIZE_WORD;				// Set the erase programming size to a word (corresponds to voltage level 3 - ie 3.3V with no external programming voltage)
	FLASH->CR |= FLASH_CR_SER;					// Sector erase
	FLASH->CR &= ~FLASH_CR_SNB_Msk;
	FLASH->CR |= sector << FLASH_CR_SNB_Pos;	// Sector number
	FLASH->CR |= FLASH_CR_STRT;					// Trigger erase operation

	FlashWaitForLastOperation();

	FLASH->CR &= ~FLASH_CR_SER;
	FLASH->CR &= ~FLASH_CR_SNB_Msk;
}


void Config::FlashWaitForLastOperation(void)
{
	while ((FLASH->SR & FLASH_ALL_ERRORS) != 0) {}
}


void Config::FlashProgram(uint32_t* dest_addr, uint32_t* src_addr, size_t size)
{
	FlashWaitForLastOperation();

	// if the previous operation is completed, proceed to program the new data
	FLASH->CR &= FLASH_CR_PSIZE_Msk;
	FLASH->CR |= FLASH_PSIZE_WORD;				// Set programming size to a word
	FLASH->CR |= FLASH_CR_PG;					// Set programming bit

	// Program 4 bytes at a time
	for (uint16_t b = 0; b < std::ceil(static_cast<float>(size) / 4); ++b) {
		*dest_addr = *src_addr;
		dest_addr++;
		src_addr++;

		FlashWaitForLastOperation();
	}

	FLASH->CR &= ~FLASH_CR_PG;					// Disable programming bit
}
