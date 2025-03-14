#include "uartHandler.h"
#include <iomanip>
#include <string>
#include "USB.h"

std::string IntToString(const int32_t& v);
std::string HexToString(const uint32_t& v, const bool& spaces);
std::string HexByte(const uint16_t& v);
void uartSendString(const std::string& s);


volatile uint8_t uartCmdPos = 0;
volatile char uartCmd[100];
volatile uint8_t uartCmdRdy = false;


void InitUart() {
	// 405 USART3: TX = PB10 (29); RX = PB11 (30)

	RCC->APB1ENR |= RCC_APB1ENR_USART3EN;			// UART clock enable
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;			// GPIO port enable

	GPIOB->MODER |= GPIO_MODER_MODER10_1;			// Set alternate function
	GPIOB->AFR[1] |= 7 << GPIO_AFRH_AFSEL10_Pos;	// Alternate function for UART3_TX is AF7
	GPIOB->MODER |= GPIO_MODER_MODER11_1;			// Set alternate function
	GPIOB->AFR[1] |= 7 << GPIO_AFRH_AFSEL11_Pos;	// Alternate function for UART3_RX is AF7

	int Baud = (SystemCoreClock / 4) / (16 * 115200);		// NB must be an integer or timing will be out
	USART3->BRR |= Baud << USART_BRR_DIV_Mantissa_Pos;		// Baud Rate (called USART_BRR_DIV_Mantissa) = (Sys Clock: 168MHz / APB1 Prescaler DIV4: 42MHz) / (16 * 115200) = 22.8
	USART3->BRR |= 12;								// Fraction: (168MHz / 4) / (16 * 115200) = 22.786458: multiply remainder by 16: 16 * .786458 = 12.58
	USART3->CR1 &= ~USART_CR1_M;					// Clear bit to set 8 bit word length
	USART3->CR1 |= USART_CR1_RE;					// Receive enable
	USART3->CR1 |= USART_CR1_TE;					// Transmitter enable

	// Set up interrupts
	USART3->CR1 |= USART_CR1_RXNEIE;
	NVIC_SetPriority(USART3_IRQn, 3);				// Lower is higher priority
	NVIC_EnableIRQ(USART3_IRQn);

	USART3->CR1 |= USART_CR1_UE;					// USART Enable

	// configure GPIO to act as button on nucleo board (as user button is already a cv output)
//	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
//	GPIOB->PUPDR |= GPIO_PUPDR_PUPDR11_0;			// Set pin to pull up:  01 Pull-up; 10 Pull-down; 11 Reserved
//	GPIOB->MODER &= ~GPIO_MODER_MODE11_Msk;

}
/*
std::string IntToString(const int32_t& v) {
	return std::to_string(v);
}

char buf[50];
std::string HexToString(const uint32_t& v, const bool& spaces) {
	sprintf(buf, "%X", (unsigned int)v);
	return std::string(buf);
//	std::string(buf).append("\r\n")

}

std::string HexByte(const uint16_t& v) {
//	char buf[50];
	sprintf(buf, "%X", v);
	return std::string(buf);

}
*/
void uartSendChar(char c) {
	while ((USART3->SR & USART_SR_TXE) == 0);
	USART3->DR = c;
}

size_t uartSendString(const char* s, size_t len) {
	char c = s[0];
	for (uint8_t i = 0; i < len; ++i) {
		while ((USART3->SR & USART_SR_TXE) == 0);
		USART3->DR = c;
		c = s[i];
	}
	return len;
}

void uartSendString(const char* s) {
	char c = s[0];
	uint8_t i = 0;
	while (c) {
		while ((USART3->SR & USART_SR_TXE) == 0);
		USART3->DR = c;
		c = s[++i];
	}
}

void uartSendString(const std::string& s) {
	for (char c : s) {
		while ((USART3->SR & USART_SR_TXE) == 0);
		USART3->DR = c;
	}
}

// Check if a command has been received from USB, parse and action as required
uint8_t uartCommand()
{
	//char buf[50];

	if (!uartCmdRdy) {
		return false;
	}

	std::string comCmd = std::string((const char*)uartCmd);

	if (comCmd.compare("help\n") == 0) {

		uartSendString("Mountjoy UART\r\n"
				"\r\nSupported commands:\r\n"
				"info        -  Show diagnostic information\r\n"
				"usbdebug    -  Start USB debugging\r\n"
				"\r\n"
		);

	} else if (strcmp((const char*)uartCmd, "usbdebug\n") == 0) {
#if (USB_DEBUG)
		usb.OutputDebug();
#endif
	} else {
		uartSendString(": Unrecognised command Type 'help' for supported commands\r\n");
	}

	uartCmdRdy = false;
	return true;
}



extern "C" {
/*
// To enable USB for printf commands (To print floats enable 'Use float with printf from newlib-nano' MCU Build Settings)
size_t _write(int handle, const unsigned char* buf, size_t bufSize)
{
	return uartSendString((const char*)buf, bufSize);
}
*/

// USART Decoder
void USART3_IRQHandler() {
	if (!uartCmdRdy) {
		uartCmd[uartCmdPos] = USART3->DR; 				// accessing RDR automatically resets the receive flag
		if (uartCmd[uartCmdPos] == 10) {
			uartCmd[uartCmdPos + 1] = 0;
			uartCmdRdy = true;
			uartCmdPos = 0;
		} else {
			uartCmdPos++;
		}
	}
}
}
