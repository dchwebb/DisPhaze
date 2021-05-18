
void TIM3_IRQHandler(void)
{
	// Send next samples to DAC
	if (TIM3->SR & TIM_SR_UIF) { 					// if UIF flag is set

		TIM3->SR &= ~TIM_SR_UIF;					// clear UIF flag
		DAC->SWTRIGR |= DAC_SWTRIGR_SWTRIG1;		// Tell the DAC to output the next value
		DAC->SWTRIGR |= DAC_SWTRIGR_SWTRIG2;		// Tell the DAC to output the next value

		if (dacRead == 1) {							// If the buffer has not been refilled increment overrun warning
			overrun++;
		}

		dacRead = 1;
	}
}


void EXTI15_10_IRQHandler(void)
{
	// Read PC10 and PC12 for octave up and down switch
	if (GPIOC->IDR & GPIO_IDR_IDR_10) {
		RelPitch = OCTAVEUP;
	} else if (GPIOC->IDR & GPIO_IDR_IDR_12) {
		RelPitch = OCTAVEDOWN;
	} else {
		RelPitch = NONE;
	}
	EXTI->PR |= EXTI_PR_PR10 | EXTI_PR_PR12;		// Clear interrupt pending
}


void EXTI9_5_IRQHandler(void)
{
	mixOn = (GPIOB->IDR & GPIO_IDR_IDR_8);			// Read PB8 - DAC2 Output to Mix mode
	ringModOn = (GPIOC->IDR & GPIO_IDR_IDR_6);		// Read PC6 - DAC1 Output to Ring Mod mode

	EXTI->PR |= EXTI_PR_PR8 | EXTI_PR_PR6;			// Clear interrupt pending
}
