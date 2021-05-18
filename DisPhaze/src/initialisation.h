#include "stm32f4xx.h"
#include "stm32f4xx_flash.h"
#include <cstring>


#define SAMPLERATE 72000

#define ADC_BUFFER_LENGTH 10
extern volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];

// Create a struct to store calibration settings - note this uses the Standard Periphal Driver code
struct CalibSettings {
	char OffsetMarker[4] = "TOF";		// calibration tuning offset
	uint32_t Offset = 0;
	char ScaleMarker[4] = "TSC";		// calibration tuning scale
	float Scale = 0;
};
#define ADDR_FLASH_SECTOR_10    reinterpret_cast<uint32_t*>(0x080C0000) // Base address of Sector 10, 128 Kbytes

void SystemClock_Config();
void InitSysTick(uint32_t ticks, uint32_t calib);void InitDAC();void InitSwitches();
void InitTimer();
void InitADC();
void WriteToFlash(CalibSettings& c);
void CalibRestore(CalibSettings& c);

