void TIM3_IRQHandler(void)
{
	// Send next samples to DAC
	if (TIM3->SR & TIM_SR_UIF) { 					// if UIF flag is set

		TIM3->SR &= ~TIM_SR_UIF;					// clear UIF flag
		DAC->SWTRIGR |= DAC_SWTRIGR_SWTRIG1;		// Tell the DAC to output the next value
		DAC->SWTRIGR |= DAC_SWTRIGR_SWTRIG2;		// Tell the DAC to output the next value

//		if (dacRead) {								// If the buffer has not been refilled increment overrun warning
//			overrun++;
//		}

		// Debug timing
		debugInterval = TIM5->CNT;
		TIM5->EGR |= TIM_EGR_UG;

		//dacRead = true;

		// Ready for next sample (Calibrating sends out a square wave for tuning so disables normal output)
		if (!config.calibrating) {
			phaseDist.CalcNextSamples();
			debugWorkTime = TIM5->CNT;

			if (debugWorkTime > 1150) {								// If the buffer has not been refilled increment overrun warning
				overrun++;
			}
		}
	}
}


void EXTI0_IRQHandler(void)
{
	// PA0 - octave up; PC3 Octave Down
	if (GPIOA->IDR & GPIO_IDR_IDR_0) {
		phaseDist.relativePitch = PhaseDistortion::OCTAVEUP;
	} else if (GPIOC->IDR & GPIO_IDR_IDR_3) {
		phaseDist.relativePitch = PhaseDistortion::OCTAVEDOWN;
	} else {
		phaseDist.relativePitch = PhaseDistortion::NONE;
	}
	EXTI->PR |= EXTI_PR_PR0;			// Clear interrupt pending
}


void EXTI3_IRQHandler(void)
{
	// PA0 - octave up; PC3 Octave Down
	if (GPIOA->IDR & GPIO_IDR_IDR_0) {
		phaseDist.relativePitch = PhaseDistortion::OCTAVEUP;
	} else if (GPIOC->IDR & GPIO_IDR_IDR_3) {
		phaseDist.relativePitch = PhaseDistortion::OCTAVEDOWN;
	} else {
		phaseDist.relativePitch = PhaseDistortion::NONE;
	}
	EXTI->PR |= EXTI_PR_PR3;			// Clear interrupt pending
}

void EXTI15_10_IRQHandler(void)
{
	phaseDist.mixOn = (GPIOC->IDR & GPIO_IDR_IDR_13);			// Read PC13 - DAC2 Output to Mix mode

	EXTI->PR |= EXTI_PR_PR13;		// Clear interrupt pending
}


void EXTI9_5_IRQHandler(void)
{
	phaseDist.ringModOn = (GPIOC->IDR & GPIO_IDR_IDR_6);		// Read PC6 - DAC1 Output to Ring Mod mode

	EXTI->PR |= EXTI_PR_PR6;			// Clear interrupt pending
}


void OTG_FS_IRQHandler(void) {
	usb.InterruptHandler();
}

void NMI_Handler(void) {}
void HardFault_Handler(void) {
	while (1) {}
}
void MemManage_Handler(void) {
	while (1) {}
}
void BusFault_Handler(void) {
	while (1) {}
}
void UsageFault_Handler(void) {
	while (1) {}
}
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}
