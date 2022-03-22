#include "initialisation.h"
#include <algorithm>

#ifdef STM32F446xx

#define USE_HSI
#define PLL_M 8
#define PLL_N 180
#define PLL_P 0		//  Main PLL (PLL) division factor for main system clock can be 2 (PLL_P = 0), 4 (PLL_P = 1), 6 (PLL_P = 2), 8 (PLL_P = 3)
#define PLL_Q 0
#define AHB_PRESCALAR 0b0000
#define APB1_PRESCALAR 0b100		// AHB divided by 2 APB Prescaler: 0b0xx: AHB clock not divided, 0b100 div by  2, 0b101: div by  4, 0b110 div by  8; 0b111 div by 16
#define APB2_PRESCALAR 0b100

#else

#define USE_HSE
#define PLL_M 4
#define PLL_N 168	// Max is 168
#define PLL_P 0		// Main PLL (PLL) division factor for main system clock can be 2 (PLL_P = 0), 4 (PLL_P = 1), 6 (PLL_P = 2), 8 (PLL_P = 3)
#define PLL_Q 7		// 48MHz for USB: 8MHz / 4(M) * 168(N) / 7(Q) = 48MHz
#define AHB_PRESCALAR 0b0000
#define APB1_PRESCALAR 0b101		// AHB divided by 4 APB Prescaler: 0b0xx: AHB clock not divided, 0b100 div by  2, 0b101: div by  4, 0b110 div by  8; 0b111 div by 16
#define APB2_PRESCALAR 0b101

#endif

/* AHB Prescaler
0xxx: system clock not divided
1000: system clock divided by 2
1001: system clock divided by 4
1010: system clock divided by 8
1011: system clock divided by 16
1100: system clock divided by 64
1101: system clock divided by 128
1110: system clock divided by 256
1111: system clock divided by 512
*/

void SystemClock_Config(void)
{
	uint32_t temp = 0x00000000;

	RCC->APB1ENR |= RCC_APB1ENR_PWREN;				// Enable Power Control clock
	PWR->CR |= PWR_CR_VOS;							// Enable VOS voltage scaling - allows maximum clock speed

#ifdef USE_HSE
	RCC->CR |= RCC_CR_HSEON;						// HSE ON
	while ((RCC->CR & RCC_CR_HSERDY) == 0);			// Wait till HSE is ready
	temp = RCC_PLLCFGR_PLLSRC_HSE;					// PLL source is HSE
#endif

#ifdef USE_HSI
	RCC->CR |= RCC_CR_HSION;					// HSI ON
	while((RCC->CR & RCC_CR_HSIRDY) == 0);		// Wait till HSI is ready
#endif

	//	Set the clock multipliers and dividers
	temp |= (uint32_t)PLL_M;
	temp |= ((uint32_t)PLL_N << 6);
	temp |= ((uint32_t)PLL_P << 16);
	temp |= ((uint32_t)PLL_Q << 24);
	RCC->PLLCFGR = temp;

	//	Set AHB, APB1 and APB2 prescalars
	temp = RCC->CFGR;
	temp |= ((uint32_t)AHB_PRESCALAR << 4);
	temp |= ((uint32_t)APB1_PRESCALAR << 10);
	temp |= ((uint32_t)APB2_PRESCALAR << 13);
	temp |= RCC_CFGR_SW_1;							// Select PLL as SYSCLK
	RCC->CFGR = temp;

	FLASH->ACR |= FLASH_ACR_LATENCY_5WS;			// Clock faster than 150MHz requires 5 Wait States for Flash memory access time

	RCC->CR |= RCC_CR_PLLON;						// Switch ON the PLL
	while ((RCC->CR & RCC_CR_PLLRDY) == 0);			// Wait till PLL is ready
	while ((RCC->CFGR & RCC_CFGR_SWS_PLL) == 0);	// System clock switch status SWS = 0b10 = PLL is really selected

	// STM32F405x/407x/415x/417x Revision Z (0x1001) devices: prefetch is supported DW - assume revision Y (0x100F) is OK
	volatile uint32_t idNumber = DBGMCU->IDCODE;
	idNumber = idNumber >> 16;
	if (idNumber == 0x1001 || idNumber == 0x100F)
		FLASH->ACR |= FLASH_ACR_PRFTEN;				// Enable the Flash prefetch

	// Enable data and instruction cache
	FLASH->ACR |= FLASH_ACR_ICEN;
	FLASH->ACR |= FLASH_ACR_DCEN;
}


void InitSysTick(uint32_t ticks, uint32_t calib)
{
	// Register macros found in core_cm4.h
	SysTick->CTRL = 0;								// Disable SysTick
	SysTick->LOAD = (ticks - 1) - calib;			// Set reload register - ie number of ticks before interrupt fired

	// Set priority of Systick interrupt to least urgency (ie largest priority value)
	NVIC_SetPriority (SysTick_IRQn, (1 << __NVIC_PRIO_BITS) - 1);

	SysTick->VAL = 0;								// Reset the SysTick counter value

//	SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk;	// Select processor clock: 1 = processor clock; 0 = external clock
	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;		// Enable SysTick interrupt
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;		// Enable SysTick
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
	DAC->CR |= DAC_CR_TEN1;							// DAC 1 enable trigger
	DAC->CR |= DAC_CR_TSEL1;						// Set trigger to software (0b111: Software trigger)

	DAC->CR |= DAC_CR_EN2;							// Enable DAC using PA5 (DAC_OUT2)
	DAC->CR |= DAC_CR_BOFF2;						// Enable DAC channel output buffer
	DAC->CR |= DAC_CR_TEN2;							// DAC 2 enable trigger
	DAC->CR |= DAC_CR_TSEL2;						// Set trigger to software (0b111: Software trigger)
}


void InitSwitches()
{
	// MODER: 00: Input (default), 01: Output, 10: Alternate function, 11: Analog

	//	Enable GPIO and external interrupt clocks
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;			// reset and clock control - advanced high performance bus - GPIO port B
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;			// reset and clock control - advanced high performance bus - GPIO port C
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;			// Enable system configuration clock: used to manage external interrupt line connection to GPIOs

	// PC6 Ring Mod Button in
	GPIOC->MODER &= ~(GPIO_MODER_MODER6);			// input mode is default
	GPIOC->PUPDR |= GPIO_PUPDR_PUPDR6_0;			// Set pin to pull up:  01 Pull-up; 10 Pull-down; 11 Reserved

	// PB8 DAC2 Mix mode switch
	GPIOB->MODER &= ~(GPIO_MODER_MODER8);			// input mode is default
	GPIOB->PUPDR |= GPIO_PUPDR_PUPDR8_1;			// Set pin to pull down:  01 Pull-up; 10 Pull-down; 11 Reserved

	// PB5 Calibration button
	GPIOB->MODER &= ~(GPIO_MODER_MODER5);			// input mode is default
	GPIOB->PUPDR |= GPIO_PUPDR_PUPDR5_1;			// Set pin to pull up:  01 Pull-up; 10 Pull-down; 11 Reserved

	// Set up PC10 and PC12 for octave up and down switch

	GPIOC->MODER |= GPIO_MODER_MODER10_0;			// Debug

	GPIOC->MODER &= ~(GPIO_MODER_MODER10);			// input mode is default
	GPIOC->MODER &= ~(GPIO_MODER_MODER12);			// input mode is default
	GPIOC->PUPDR |= GPIO_PUPDR_PUPDR10_1;			// Set pin to pull down:  01 Pull-up; 10 Pull-down; 11 Reserved
	GPIOC->PUPDR |= GPIO_PUPDR_PUPDR12_1;			// Set pin to pull down:  01 Pull-up; 10 Pull-down; 11 Reserved

	// configure PB8 switch to fire on an interrupt (mix)
	SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI8_PB;	// Select Pin PB8 which uses External interrupt 3
	EXTI->RTSR |= EXTI_RTSR_TR8;					// Enable rising edge trigger for line 8
	EXTI->FTSR |= EXTI_FTSR_TR8;					// Enable falling edge trigger for line 8
	EXTI->IMR |= EXTI_IMR_MR8;						// Activate interrupt using mask register 8

	// configure PC6 switch to fire on an interrupt (ring mod)
	SYSCFG->EXTICR[1] |= SYSCFG_EXTICR2_EXTI6_PC;	// Select Pin PC6 which uses External interrupt 2
	EXTI->RTSR |= EXTI_RTSR_TR6;					// Enable rising edge trigger for line 6
	EXTI->FTSR |= EXTI_FTSR_TR6;					// Enable falling edge trigger for line 6
	EXTI->IMR |= EXTI_IMR_MR6;						// Activate interrupt using mask register 6

	NVIC_SetPriority(EXTI9_5_IRQn, 3);
	NVIC_EnableIRQ(EXTI9_5_IRQn);

	// configure PC10 & PC12 switch to fire on an interrupt
	SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI10_PC;	// Select Pin PC10 which uses External interrupt 3
	EXTI->RTSR |= EXTI_RTSR_TR10;					// Enable rising edge trigger for line 10
	EXTI->FTSR |= EXTI_FTSR_TR10;					// Enable falling edge trigger for line 10
	EXTI->IMR |= EXTI_IMR_MR10;						// Activate interrupt using mask register 10

	SYSCFG->EXTICR[3] |= SYSCFG_EXTICR4_EXTI12_PC;	// Select Pin PC12 which uses External interrupt 4
	EXTI->RTSR |= EXTI_RTSR_TR12;					// Enable rising edge trigger for line 12
	EXTI->FTSR |= EXTI_FTSR_TR12;					// Enable falling edge trigger for line 12
	EXTI->IMR |= EXTI_IMR_MR12;						// Activate interrupt using mask register 12

	NVIC_SetPriority(EXTI15_10_IRQn, 3);
	NVIC_EnableIRQ(EXTI15_10_IRQn);
}


void InitTimer()
{
	//	Setup Timer 3 on an interrupt to trigger sample loading
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;				// Enable Timer 3
	TIM3->PSC = (SystemCoreClock / SAMPLERATE) / 4;	// Set prescaler to fire at sample rate - this is divided by 4 to match the APB2 prescaler
	TIM3->ARR = 1; 									// Set maximum count value (auto reload register) - set to system clock / sampling rate

	TIM3->DIER |= TIM_DIER_UIE;						//  DMA/interrupt enable register
	NVIC_EnableIRQ(TIM3_IRQn);
	NVIC_SetPriority(TIM3_IRQn, 0);

	TIM3->CR1 |= TIM_CR1_CEN;
	TIM3->EGR |= TIM_EGR_UG;
}



void InitDebugTimer()
{
	//	Setup Timer 4 for debug timing
	RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;				// Enable Timer 4
	TIM4->CR1 |= TIM_CR1_CEN;
	TIM4->EGR |= TIM_EGR_UG;
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

	/*
	 * Using ADC1
	 *
	 */
	//	Setup Timer 2 to trigger ADC
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;				// Enable Timer 2 clock
	TIM2->CR2 |= TIM_CR2_MMS_2;						// 100: Compare - OC1REF signal is used as trigger output (TRGO)
	TIM2->PSC = 20 - 1;								// Prescaler
	TIM2->ARR = 100 - 1;							// Auto-reload register (ie reset counter) divided by 100
	TIM2->CCR1 = 50 - 1;							// Capture and compare - ie when counter hits this number PWM high
	TIM2->CCER |= TIM_CCER_CC1E;					// Capture/Compare 1 output enable
	TIM2->CCMR1 |= TIM_CCMR1_OC1M_1 |TIM_CCMR1_OC1M_2;		// 110 PWM Mode 1
	TIM2->CR1 |= TIM_CR1_CEN;

	// Enable ADC1 clock source
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;


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


	ADC1->CR1 |= ADC_CR1_SCAN;						// Activate scan mode
	ADC1->SQR1 = (ADC_BUFFER_LENGTH - 1) << ADC_SQR1_L_Pos;		// Number of conversions in sequence
	InitAdcPins(ADC1, {8, 9, 1, 2, 3, 10, 12, 14, 7, 11});

	ADC1->CR2 |= ADC_CR2_EOCS;						// Trigger interrupt on end of each individual conversion
	ADC1->CR2 |= ADC_CR2_EXTEN_0;					// ADC hardware trigger 00: Trigger detection disabled; 01: Trigger detection on the rising edge; 10: Trigger detection on the falling edge; 11: Trigger detection on both the rising and falling edges
	ADC1->CR2 |= ADC_CR2_EXTSEL_1 | ADC_CR2_EXTSEL_2;	// ADC External trigger: 0110 = TIM2_TRGO event

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
	DMA2_Stream0->M0AR = (uint32_t)(ADC_array);		// Configure the memory address (note that M1AR is used for double-buffer mode)
	DMA2_Stream0->CR &= ~DMA_SxCR_CHSEL;			// channel select to 0 for ADC1

	DMA2_Stream0->CR |= DMA_SxCR_EN;				// Enable DMA2
	ADC1->CR2 |= ADC_CR2_ADON;						// Activate ADC

}

