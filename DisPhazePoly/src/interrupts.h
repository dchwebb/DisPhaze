float samplePosDebug = 0;

void TIM3_IRQHandler(void)
{
	// Send next samples to DAC
	if (TIM3->SR & TIM_SR_UIF) { 					// if UIF flag is set

		TIM3->SR &= ~TIM_SR_UIF;					// clear UIF flag
		DAC->SWTRIGR |= DAC_SWTRIGR_SWTRIG1;		// Tell the DAC to output the next value
		DAC->SWTRIGR |= DAC_SWTRIGR_SWTRIG2;		// Tell the DAC to output the next value

		phaseDist.CalcNextSamples();
	}
}


void OTG_FS_IRQHandler(void) {
	usb.InterruptHandler();
}

// MIDI Decoder
void UART4_IRQHandler(void) {
	if (UART4->SR | USART_SR_RXNE) {
		usb.midi.serialHandler(UART4->DR); 				// accessing DR automatically resets the receive flag
	}
}

void SysTick_Handler(void)
{
	++SysTickVal;
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
