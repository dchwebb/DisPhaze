#include "initialisation.h"
#include <algorithm>

GpioPin debugPin {GPIOB, 12, GpioPin::Type::Output};

volatile ADCValues adc;

#define AHB_PRESCALAR 0b0000
#define APB1_PRESCALAR 0b101		// AHB divided by 4 APB Prescaler: 0b0xx: AHB clock not divided, 0b100 div by  2, 0b101: div by  4, 0b110 div by  8; 0b111 div by 16
#define APB2_PRESCALAR 0b101


void InitClocks()
{
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;				// Enable Power Control clock
	PWR->CR |= PWR_CR_VOS;							// Enable VOS voltage scaling - allows maximum clock speed

	RCC->CR |= RCC_CR_HSEON;						// HSE ON
	while ((RCC->CR & RCC_CR_HSERDY) == 0);			// Wait till HSE is ready

	// PLL: 12MHz / 6(M) * 168(N) / 2(P) = 168MHz
	RCC->PLLCFGR = RCC_PLLCFGR_PLLSRC_HSE |			// PLL source is HSE
			(6   << RCC_PLLCFGR_PLLM_Pos) |
			(168 << RCC_PLLCFGR_PLLN_Pos) |
			(0   << RCC_PLLCFGR_PLLP_Pos) |			// Divide by 2 (0 = /2, 1 = /4, 2 = /6, 3 = /8)
			(7   << RCC_PLLCFGR_PLLQ_Pos);			// 48MHz for USB: 8MHz / 4(M) * 168(N) / 7(Q) = 48MHz

	//	Set AHB, APB1 and APB2 prescalars
	RCC->CFGR |= (AHB_PRESCALAR << RCC_CFGR_HPRE_Pos) |
			(APB1_PRESCALAR << RCC_CFGR_PPRE1_Pos) |
			(APB2_PRESCALAR << RCC_CFGR_PPRE2_Pos) |
			RCC_CFGR_SW_1;							// Select PLL as SYSCLK

	FLASH->ACR |= FLASH_ACR_LATENCY_5WS;			// Clock faster than 150MHz requires 5 Wait States for Flash memory access time

	RCC->CR |= RCC_CR_PLLON;						// Switch ON the PLL
	while ((RCC->CR & RCC_CR_PLLRDY) == 0);			// Wait till PLL is ready
	while ((RCC->CFGR & RCC_CFGR_SWS_PLL) == 0);	// System clock switch status SWS = 0b10 = PLL is really selected

	FLASH->ACR |= FLASH_ACR_PRFTEN;					// Enable the Flash prefetch

	// Enable data and instruction cache
	FLASH->ACR |= FLASH_ACR_ICEN;
	FLASH->ACR |= FLASH_ACR_DCEN;

	SystemCoreClockUpdate();						// Update SystemCoreClock variable
}


void InitHardware()
{
	InitClocks();
	InitSysTick();
	InitDAC();						// DAC1 Output on PA4 (Pin 20); DAC2 Output on PA5 (Pin 21)
	InitADC();						// Configure ADC for analog controls
	InitDebugTimer();				// Timer to check available calculation time
	InitPWMTimer();					// PWM timers for LED control
	InitMidiUART();

}


void InitSysTick()
{
	SysTick_Config(SystemCoreClock / sysTickInterval);		// gives 1ms
	NVIC_SetPriority(SysTick_IRQn, 0);
}


void InitDAC()
{
	// Once the DAC channelx is enabled, the corresponding GPIO pin (PA4 or PA5) is automatically connected to the analog converter output (DAC_OUTx).
	// In order to avoid parasitic consumption, the PA4 or PA5 pin should first be configured to analog (AIN).

	// Enable DAC and GPIO Clock
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;			// Enable GPIO Clock
	RCC->APB1ENR |= RCC_APB1ENR_DACEN;				// Enable DAC Clock

	DAC->CR |= DAC_CR_EN1;							// Enable DAC using PA4 (DAC_OUT1)
	DAC->CR |= DAC_CR_BOFF1;						// Enable DAC channel output buffer to reduce the output impedance
	DAC->CR |= DAC_CR_TEN1;							// DAC 1 enable trigger - allows samples to be loaded into DAC then sent on interrupt
	DAC->CR |= DAC_CR_TSEL1;						// Set trigger to software (0b111: Software trigger)

	DAC->CR |= DAC_CR_EN2;							// Enable DAC using PA5 (DAC_OUT2)
	DAC->CR |= DAC_CR_BOFF2;						// Enable DAC channel output buffer
	DAC->CR |= DAC_CR_TEN2;							// DAC 2 enable trigger
	DAC->CR |= DAC_CR_TSEL2;						// Set trigger to software (0b111: Software trigger)
}


void InitSampleTimer()
{
	//	Setup Timer 3 on an interrupt to trigger sample loading
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;				// Enable Timer 3
	TIM3->PSC = (SystemCoreClock / sampleRate) / 4;	// Set prescaler to fire at sample rate - this is divided by 4 to match the APB1 prescaler (42MHz)
	TIM3->ARR = 1; 									// Set maximum count value (auto reload register) - set to system clock / sampling rate

	TIM3->DIER |= TIM_DIER_UIE;						//  DMA/interrupt enable register
	NVIC_EnableIRQ(TIM3_IRQn);
	NVIC_SetPriority(TIM3_IRQn, 0);

	TIM3->CR1 |= TIM_CR1_CEN;
	TIM3->EGR |= TIM_EGR_UG;
}


void InitDebugTimer()
{
	//	Setup Timer 5 for debug timing
	RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;				// Enable Timer 5
	TIM5->CR1 |= TIM_CR1_CEN;
	TIM5->EGR |= TIM_EGR_UG;
}


void InitAdcPins(ADC_TypeDef* ADC_No, std::initializer_list<uint8_t> channels)
{
	uint8_t sequence = 1;

	for (auto channel: channels) {
		// Set conversion sequence to order ADC channels are passed to this function
		if (sequence < 7) {
			ADC_No->SQR3 |= channel << ((sequence - 1) * 5);
		} else if (sequence < 13) {
			ADC_No->SQR2 |= channel << ((sequence - 7) * 5);
		} else {
			ADC_No->SQR1 |= channel << ((sequence - 13) * 5);
		}

		// 000: 3 cycles, 001: 15 cycles, 010: 28 cycles, 011: 56 cycles, 100: 84 cycles, 101: 112 cycles, 110: 144 cycles, 111: 480 cycles
		if (channel < 10)
			ADC_No->SMPR2 |= 0b010 << (3 * channel);
		else
			ADC_No->SMPR1 |= 0b010 << (3 * (channel - 10));

		sequence++;
	}
}


void InitADC(void)
{
	//  Using ADC1

	//	Setup Timer 8 to trigger ADC
	RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;				// Enable Timer clock
	TIM8->CR2 |= TIM_CR2_MMS_2;						// 100: Compare - OC1REF signal is used as trigger output (TRGO)
	TIM8->PSC = 20 - 1;								// Prescaler
	TIM8->ARR = 100 - 1;							// Auto-reload register (ie reset counter) divided by 100
	TIM8->CCR1 = 50 - 1;							// Capture and compare - ie when counter hits this number PWM high
	TIM8->CCER |= TIM_CCER_CC1E;					// Capture/Compare 1 output enable
	TIM8->CCMR1 |= TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;		// 110 PWM Mode 1
	TIM8->CR1 |= TIM_CR1_CEN;

	// Enable GPIO and ADC1 clock source
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOBEN | RCC_AHB1ENR_GPIOCEN;
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

	GPIOA->MODER |= GPIO_MODER_MODER1 | GPIO_MODER_MODER2 | GPIO_MODER_MODER3 | GPIO_MODER_MODER7;		// Configure PA1 (1), PA2 (2), PA3 (3) PA7 (7) to Analog mode (0b11)
	GPIOB->MODER |= GPIO_MODER_MODER0 | GPIO_MODER_MODER1;		// Configure PB0 (8), PB1 (9) to Analog mode (0b11)
	GPIOC->MODER |= GPIO_MODER_MODER0 | GPIO_MODER_MODER1 | GPIO_MODER_MODER2 | GPIO_MODER_MODER4;	// Configure PC0 (10), PC1 (11), PC2 (12), PC4 (14) to Analog mode (0b11)

	ADC1->CR1 |= ADC_CR1_SCAN;						// Activate scan mode
	ADC1->SQR1 = (ADC_BUFFER_LENGTH - 1) << ADC_SQR1_L_Pos;		// Number of conversions in sequence
	InitAdcPins(ADC1, {8, 3, 14, 9, 11, 2, 12, 1, 7, 10});

	ADC1->CR2 |= ADC_CR2_EOCS;						// Trigger interrupt on end of each individual conversion
	ADC1->CR2 |= ADC_CR2_EXTEN_0;					// ADC hardware trigger 00: Trigger detection disabled; 01: Trigger detection on the rising edge; 10: Trigger detection on the falling edge; 11: Trigger detection on both the rising and falling edges
	ADC1->CR2 |= 0b1110 << ADC_CR2_EXTSEL_Pos;		// ADC External trigger: 1110 = TIM8_TRGO event

	// Enable DMA - DMA2, Channel 0, Stream 0  = ADC1 (Manual p207)
	ADC1->CR2 |= ADC_CR2_DMA;						// Enable DMA Mode on ADC1
	ADC1->CR2 |= ADC_CR2_DDS;						// DMA requests are issued as long as data are converted and DMA=1
	RCC->AHB1ENR|= RCC_AHB1ENR_DMA2EN;

	DMA2_Stream0->CR &= ~DMA_SxCR_DIR;				// 00 = Peripheral-to-memory
	DMA2_Stream0->CR |= DMA_SxCR_PL_1;				// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High
	DMA2_Stream0->CR |= DMA_SxCR_PSIZE_0;			// Peripheral size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream0->CR |= DMA_SxCR_MSIZE_0;			// Memory size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream0->CR &= ~DMA_SxCR_PINC;				// Peripheral not in increment mode
	DMA2_Stream0->CR |= DMA_SxCR_MINC;				// Memory in increment mode
	DMA2_Stream0->CR |= DMA_SxCR_CIRC;				// circular mode to keep refilling buffer
	DMA2_Stream0->CR &= ~DMA_SxCR_DIR;				// data transfer direction: 00: peripheral-to-memory; 01: memory-to-peripheral; 10: memory-to-memory

	DMA2_Stream0->NDTR |= ADC_BUFFER_LENGTH;		// Number of data items to transfer (ie size of ADC buffer)
	DMA2_Stream0->PAR = (uint32_t)(&(ADC1->DR));	// Configure the peripheral data register address
	DMA2_Stream0->M0AR = (uint32_t)(&adc);			// Configure the memory address (note that M1AR is used for double-buffer mode)
	DMA2_Stream0->CR &= ~DMA_SxCR_CHSEL;			// channel select to 0 for ADC1

	DMA2_Stream0->CR |= DMA_SxCR_EN;				// Enable DMA2
	ADC1->CR2 |= ADC_CR2_ADON;						// Activate ADC

	/*
	//	Setup Timer 2 to trigger ADC
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;				// Enable Timer 2 clock
	TIM2->CR2 |= TIM_CR2_MMS_2;						// 100: Compare - OC1REF signal is used as trigger output (TRGO)
	TIM2->PSC = 20 - 1;								// Prescaler
	TIM2->ARR = 100 - 1;							// Auto-reload register (ie reset counter) divided by 100
	TIM2->CCR1 = 50 - 1;							// Capture and compare - ie when counter hits this number PWM high
	TIM2->CCER |= TIM_CCER_CC1E;					// Capture/Compare 1 output enable
	TIM2->CCMR1 |= TIM_CCMR1_OC1M_1 |TIM_CCMR1_OC1M_2;	// 110 PWM Mode 1
	TIM2->CR1 |= TIM_CR1_CEN;

	// Enable ADC2 and GPIO clock sources
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	RCC->APB2ENR |= RCC_APB2ENR_ADC2EN;

	// Enable ADC - PB0: IN8; PB1: IN9; PA1: IN1; PA2: IN2; PA3: IN3; PC0: IN10, PC2: IN12, PC4: IN14; PA7: IN7, PC1: IN11
	GPIOB->MODER |= GPIO_MODER_MODER0;				// Set PB0 to Analog mode (0b11)
	GPIOB->MODER |= GPIO_MODER_MODER1;				// Set PB1 to Analog mode (0b11)
	GPIOA->MODER |= GPIO_MODER_MODER1;				// Set PA1 to Analog mode (0b11)
	GPIOA->MODER |= GPIO_MODER_MODER2;				// Set PA2 to Analog mode (0b11)
	GPIOA->MODER |= GPIO_MODER_MODER3;				// Set PA3 to Analog mode (0b11)
	GPIOC->MODER |= GPIO_MODER_MODER0;				// Set PC0 to Analog mode (0b11)
	GPIOC->MODER |= GPIO_MODER_MODER2;				// Set PC2 to Analog mode (0b11)
	GPIOC->MODER |= GPIO_MODER_MODER4;				// Set PC4 to Analog mode (0b11)
	GPIOA->MODER |= GPIO_MODER_MODER7;				// Set PA7 to Analog mode (0b11)
	GPIOC->MODER |= GPIO_MODER_MODER1;				// Set PA7 to Analog mode (0b11)

	ADC2->CR1 |= ADC_CR1_SCAN;						// Activate scan mode
	ADC2->SQR1 = (ADC_BUFFER_LENGTH - 1) << 20;		// Number of conversions in sequence
	InitAdcPins(ADC2, {8, 9, 1, 2, 3, 10, 12, 14, 7, 11});

	ADC2->CR2 |= ADC_CR2_EOCS;						// Trigger interrupt on end of each individual conversion
	ADC2->CR2 |= ADC_CR2_EXTEN_0;					// ADC hardware trigger 00: Trigger detection disabled; 01: Trigger detection on the rising edge; 10: Trigger detection on the falling edge; 11: Trigger detection on both the rising and falling edges
	ADC2->CR2 |= ADC_CR2_EXTSEL_1 | ADC_CR2_EXTSEL_2;	// ADC External trigger: 0110 = TIM2_TRGO event

	// Enable DMA - DMA2, Channel 1, Stream 2  = ADC2 (Manual p207)
	ADC2->CR2 |= ADC_CR2_DMA;						// Enable DMA Mode on ADC2
	ADC2->CR2 |= ADC_CR2_DDS;						// DMA requests are issued as long as data are converted and DMA=1
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

	DMA2_Stream2->CR &= ~DMA_SxCR_DIR;				// 00 = Peripheral-to-memory
	DMA2_Stream2->CR |= DMA_SxCR_PL_1;				// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High
	DMA2_Stream2->CR |= DMA_SxCR_PSIZE_0;			// Peripheral size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream2->CR |= DMA_SxCR_MSIZE_0;			// Memory size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream2->CR &= ~DMA_SxCR_PINC;				// Peripheral not in increment mode
	DMA2_Stream2->CR |= DMA_SxCR_MINC;				// Memory in increment mode
	DMA2_Stream2->CR |= DMA_SxCR_CIRC;				// circular mode to keep refilling buffer
	DMA2_Stream2->CR &= ~DMA_SxCR_DIR;				// data transfer direction: 00: peripheral-to-memory; 01: memory-to-peripheral; 10: memory-to-memory

	DMA2_Stream2->NDTR |= ADC_BUFFER_LENGTH;		// Number of data items to transfer (ie size of ADC buffer)
	DMA2_Stream2->PAR = (uint32_t)(&(ADC2->DR));	// Configure the peripheral data register address
	DMA2_Stream2->M0AR = (uint32_t)(ADC_array);		// Configure the memory address (note that M1AR is used for double-buffer mode)
	DMA2_Stream2->CR |= DMA_SxCR_CHSEL_0;			// channel select to 1 for ADC2

	DMA2_Stream2->CR |= DMA_SxCR_EN;				// Enable DMA2
	ADC2->CR2 |= ADC_CR2_ADON;						// Activate ADC
*/
}

void InitPWMTimer()
{
	// Red LED: PB3 (TIM2 CH2); Green LED: PB7 (TIM4 CH2)
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN | RCC_APB1ENR_TIM4EN;

	// Enable channel 2 PWM output pins on PB3
	GPIOB->MODER &= ~(GPIO_MODER_MODE3_0 | GPIO_MODER_MODE7_0);			// 00: Input mode; 01: General purpose output mode; 10: Alternate function mode; 11: Analog mode (default)
	GPIOB->MODER |= GPIO_MODER_MODE3_1 | GPIO_MODER_MODE7_1;
	GPIOB->AFR[0] |= GPIO_AFRL_AFSEL3_0;			// Timer 2 Output channel is AF1
	GPIOB->AFR[0] |= GPIO_AFRL_AFSEL7_1;			// Timer 4 Output channel is AF2

	// Timing calculations: Clock = 168MHz / (PSC + 1) = 33.6m counts per second
	// ARR = number of counts per PWM tick = 4096
	// 33.6m / ARR = 8.2kHz of PWM square wave with 4096 levels of output

	// Red LED: PB3 (TIM2 CH2)
	TIM2->CCMR1 |= TIM_CCMR1_OC2PE;					// Output compare 2 preload enable
	TIM2->CCMR1 |= (TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2);	// 0110: PWM mode 1 - In upcounting, channel 2 active if TIMx_CNT<TIMx_CCR2
	TIM2->CCR2 = 0x0;								// Initialise PWM level
	TIM2->ARR = 0xFFF;								// Total number of PWM ticks = 4096
	TIM2->PSC = 4;									// Prescaler
	TIM2->CR1 |= TIM_CR1_ARPE;						// 1: TIMx_ARR register is buffered
	TIM2->CCER |= TIM_CCER_CC2E;					// Capture mode enabled / OC1 signal is output on the corresponding output pin
	TIM2->EGR |= TIM_EGR_UG;						// 1: Re-initialize the counter and generates an update of the registers
	TIM2->CR1 |= TIM_CR1_CEN;						// Enable counter

	// Green LED: PB7 (TIM4 CH2)
	TIM4->CCMR1 |= TIM_CCMR1_OC2PE;					// Output compare 2 preload enable
	TIM4->CCMR1 |= (TIM_CCMR1_OC2M_1 | TIM_CCMR1_OC2M_2);	// 0110: PWM mode 1 - In upcounting, channel 2 active if TIMx_CNT<TIMx_CCR2
	TIM4->CCR2 = 0x0;								// Initialise PWM level
	TIM4->ARR = 0xFFF;								// Total number of PWM ticks = 4096
	TIM4->PSC = 4;									// Prescaler
	TIM4->CR1 |= TIM_CR1_ARPE;						// 1: TIMx_ARR register is buffered
	TIM4->CCER |= TIM_CCER_CC2E;					// Capture mode enabled / OC1 signal is output on the corresponding output pin
	TIM4->EGR |= TIM_EGR_UG;						// 1: Re-initialize the counter and generates an update of the registers
	TIM4->CR1 |= TIM_CR1_CEN;						// Enable counter
}


void InitMidiUART()
{
	RCC->APB1ENR |= RCC_APB1ENR_UART4EN;			// UART4 clock enable

	GpioPin::Init(GPIOC, 11, GpioPin::Type::AlternateFunction, 8);	// PC11 UART4_RX

	int Baud = (SystemCoreClock / 4) / (16 * 31250);
	UART4->BRR |= Baud << 4;						// Baud Rate (called USART_BRR_DIV_Mantissa) = (Sys Clock: 168MHz / APB1 Prescaler DIV4: 42MHz) / (16 * 31250) = 84
	UART4->CR1 &= ~USART_CR1_M;						// Clear bit to set 8 bit word length
	UART4->CR1 |= USART_CR1_RE;						// Receive enable

	// Set up interrupts
	UART4->CR1 |= USART_CR1_RXNEIE;
	NVIC_SetPriority(UART4_IRQn, 2);				// Lower is higher priority
	NVIC_EnableIRQ(UART4_IRQn);

	UART4->CR1 |= USART_CR1_UE;						// USART Enable
}


void DelayMS(uint32_t ms)
{
	// Crude delay system
	const uint32_t now = SysTickVal;
	while (now + ms > SysTickVal) {};
}


// FIXME - need to update startup*.s; check memory region for one that does not get cleared on reboot
void JumpToBootloader()
{
	*reinterpret_cast<unsigned long *>(0x00000000) = 0xDEADBEEF; 	// Use ITCM RAM for DFU flag as this is not cleared at restart

	__disable_irq();

	FLASH->ACR &= ~FLASH_ACR_DCEN;					// Disable data cache

	// Not sure why but seem to need to write this value twice or gets lost - caching issue?
	*reinterpret_cast<unsigned long *>(0x00000000) = 0xDEADBEEF;

	__DSB();
	NVIC_SystemReset();

	while (1) {
		// Code should never reach this loop
	}
}
