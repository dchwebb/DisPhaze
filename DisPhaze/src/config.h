#pragma once

#include "initialisation.h"
#include "PhaseDistortion.h"

#define CONFIG_VERSION 1
#define ADDR_FLASH_SECTOR_10    reinterpret_cast<uint32_t*>(0x080C0000) // Base address of Sector 10, 128 Kbytes
#define FLASH_PSIZE_WORD        ((uint32_t)0x00000200)
#define FLASH_ALL_ERRORS (FLASH_SR_EOP | FLASH_SR_SOP |FLASH_SR_WRPERR | FLASH_SR_PGAERR |FLASH_SR_PGPERR | FLASH_SR_PGSERR)

struct configValues {
	char StartMarker[4] = "CFG";		// Start Marker
	uint8_t Version = CONFIG_VERSION;	// version of saved config struct format

	// Settings
	float spread = 0.0f;
	float offset = 0.0f;

	char EndMarker[4] = "END";			// End Marker
};


// Class used to store calibration settings - note this uses the Standard Peripheral Driver code
struct Config {
public:
	float tuningOffset = 0.0f;
	float tuningSpread = 0.0f;
	bool calibrating = 0;

	void Calibrate();
	void SaveConfig();
	void SetConfig(configValues &cv);	// sets properties of class to match current values
	void RestoreConfig();				// gets config from Flash, checks and updates settings accordingly

private:
	int calibBtn = 0;					// For simple debouncing

	void FlashUnlock();
	void FlashLock();
	void FlashEraseSector(uint8_t Sector);
	void FlashWaitForLastOperation();
	void FlashProgram(uint32_t* dest_addr, uint32_t* src_addr, size_t size);
};
