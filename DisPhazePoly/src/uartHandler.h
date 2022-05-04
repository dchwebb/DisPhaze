#include "stm32f4xx.h"
#include <string>

//C:/ST/STM32CubeIDE_1.0.2/STM32CubeIDE/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.9-2020-q2-update.win32_1.5.0.202011040924/tools/arm-none-eabi/include/c++/9.3.1/iomanip

extern volatile uint8_t uartCmdPos;
extern volatile char uartCmd[100];
extern volatile uint8_t uartCmdRdy;

uint8_t uartCommand();
void uartSendChar(char c);
void uartSendString(const char* s);
void InitUart();
void usbPrintDebug();


size_t uartSendString(const char* s, size_t len);
void uartSendString(const std::string& s);
