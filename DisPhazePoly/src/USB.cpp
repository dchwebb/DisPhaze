//#include <MidiHandler.h>
#include "USB.h"

volatile bool debugStart = true;


// procedure to allow classes to pass configuration data back via endpoint 1 (eg CDC line setup, MSC MaxLUN etc)
void USB::EP0In(const uint8_t* buff, uint32_t size)
{
	ep0.inBuffSize = size;
	ep0.inBuff = buff;
	ep0State = EP0State::DataIn;

	USBUpdateDbg({}, {}, {}, ep0.inBuffSize, {}, (uint32_t*)ep0.inBuff);

	EPStartXfer(Direction::in, 0, size);		// sends blank request back
}


void USB::InterruptHandler()						// In Drivers\STM32F4xx_HAL_Driver\Src\stm32f4xx_hal_pcd.c
{
	int epnum, ep_intr, epint;

	// Handle spurious interrupt
	if ((USB_OTG_FS->GINTSTS & USB_OTG_FS->GINTMSK) == 0) {
		return;
	}

	///////////		10			RXFLVL: RxQLevel Interrupt:  Rx FIFO non-empty Indicates that there is at least one packet pending to be read from the Rx FIFO.
	if (ReadInterrupts(USB_OTG_GINTSTS_RXFLVL))	{
		USB_OTG_FS->GINTMSK &= ~USB_OTG_GINTSTS_RXFLVL;

		uint32_t receiveStatus = USB_OTG_FS->GRXSTSP;		// OTG status read and pop register: not shown in SFR, but read only (ie do not pop) register under OTG_FS_GLOBAL->FS_GRXSTSR_Device
		epnum = receiveStatus & USB_OTG_GRXSTSP_EPNUM;		// Get the endpoint number
		uint16_t packetSize = (receiveStatus & USB_OTG_GRXSTSP_BCNT) >> 4;

		USBUpdateDbg(receiveStatus, {}, epnum, packetSize, {}, nullptr);

		if (((receiveStatus & USB_OTG_GRXSTSP_PKTSTS) >> 17) == STS_DATA_UPDT && packetSize != 0) {		// 2 = OUT data packet received
			ReadPacket(classes[epnum]->outBuff, packetSize, classes[epnum]->outBuffOffset);
			USBUpdateDbg({}, {}, {}, {}, {}, classes[epnum]->outBuff + classes[epnum]->outBuffOffset);
			if (classes[epnum]->outBuffPackets > 1) {
				classes[epnum]->outBuffOffset += (packetSize / 4);			// When receiving multiple packets increase buffer offset (packet size in bytes -> 32 bit ints)
			}
		} else if (((receiveStatus & USB_OTG_GRXSTSP_PKTSTS) >> 17) == STS_SETUP_UPDT) {				// 6 = SETUP data packet received
			ReadPacket(classes[epnum]->outBuff, 8U, 0);
			USBUpdateDbg({}, {}, {}, {}, {}, classes[epnum]->outBuff);
		} else if (((receiveStatus & USB_OTG_GRXSTSP_PKTSTS) >> 17) == STS_XFER_COMP) {					// 3 = transfer completed
			classes[epnum]->outBuffOffset = 0;
		}
		if (packetSize != 0) {
			classes[epnum]->outBuffCount = packetSize;
		}
		USB_OTG_FS->GINTMSK |= USB_OTG_GINTSTS_RXFLVL;
	}


	/////////// 	80000 		OEPINT OUT endpoint interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_OEPINT)) {

		// Read the output endpoint interrupt register to ascertain which endpoint(s) fired an interrupt
		ep_intr = ((USBx_DEVICE->DAINT & USBx_DEVICE->DAINTMSK) & USB_OTG_DAINTMSK_OEPM_Msk) >> 16;

		// process each endpoint in turn incrementing the epnum and checking the interrupts (ep_intr) if that endpoint fired
		epnum = 0;
		while (ep_intr != 0) {
			if ((ep_intr & 1) != 0) {
				epint = USBx_OUTEP(epnum)->DOEPINT & USBx_DEVICE->DOEPMSK;

				USBUpdateDbg(epint, {}, epnum, {}, {}, nullptr);

				if ((epint & USB_OTG_DOEPINT_XFRC) == USB_OTG_DOEPINT_XFRC) {		// 0x01 Transfer completed

					USBx_OUTEP(epnum)->DOEPINT = USB_OTG_DOEPINT_XFRC;				// Clear interrupt

					if (epnum == 0) {

						if (devState == DeviceState::Configured && classPendingData) {
							if ((req.RequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS) {
								// Previous OUT interrupt contains instruction (eg host sending CDC LineCoding); next command sends data (Eg LineCoding data)
								for (auto c : classes) {
									if (c->interface == req.Index) {
										c->ClassSetupData(req, (uint8_t*)ep0.outBuff);
									}
								}

							}
							classPendingData = false;
							EPStartXfer(Direction::in, 0, 0);
						}

						ep0State = EP0State::Idle;
						USBx_OUTEP(epnum)->DOEPCTL |= USB_OTG_DOEPCTL_STALL;
					} else {
						// Call appropriate data handler depending on endpoint of data
						if (classes[epnum]->outBuffPackets <= 1) {
							EPStartXfer(Direction::out, epnum, classes[epnum]->outBuffCount);
						}
						classes[epnum]->DataOut();
//						if (epnum == MIDI_Out) {
//							midiDataHandler((uint8_t*)endPoints[epnum]->outBuff, endPoints[epnum]->outBuffCount);
//						}
					}
				}

				if ((epint & USB_OTG_DOEPINT_STUP) == USB_OTG_DOEPINT_STUP) {		// SETUP phase done: the application can decode the received SETUP data packet.
					// Parse Setup Request containing data in outBuff filled by RXFLVL interrupt
					req.loadData((uint8_t*)classes[epnum]->outBuff);

					USBUpdateDbg({}, req, {}, {}, {}, nullptr);

					ep0State = EP0State::Setup;

					switch (req.RequestType & 0x1F) {		// originally USBD_LL_SetupStage
					case USB_REQ_RECIPIENT_DEVICE:
						StdDevReq();
						break;

					case USB_REQ_RECIPIENT_INTERFACE:
						if ((req.RequestType & USB_REQ_TYPE_MASK) == USB_REQ_TYPE_CLASS) {		// 0xA1 & 0x60 == 0x20

							// req.Index holds interface - locate which handler this relates to
							if (req.Length > 0) {
								for (auto c : classes) {
									if (c->interface == req.Index) {
										c->ClassSetup(req);
									}
								}
							} else {
								EPStartXfer(Direction::in, 0, 0);
							}

						}
						break;

					case USB_REQ_RECIPIENT_ENDPOINT:
						break;

					default:
						CtlError();
						break;
					}

					USBx_OUTEP(epnum)->DOEPINT = USB_OTG_DOEPINT_STUP;				// Clear interrupt
				}

				if ((epint & USB_OTG_DOEPINT_OTEPDIS) == USB_OTG_DOEPINT_OTEPDIS) {	// OUT token received when endpoint disabled
					USBx_OUTEP(epnum)->DOEPINT = USB_OTG_DOEPINT_OTEPDIS;			// Clear interrupt
				}
				if ((epint & USB_OTG_DOEPINT_OTEPSPR) == USB_OTG_DOEPINT_OTEPSPR) {	// Status Phase Received interrupt
					USBx_OUTEP(epnum)->DOEPINT = USB_OTG_DOEPINT_OTEPSPR;			// Clear interrupt
				}
				if ((epint & USB_OTG_DOEPINT_NAK) == USB_OTG_DOEPINT_NAK) {			// 0x2000 OUT NAK interrupt
					USBx_OUTEP(epnum)->DOEPINT = USB_OTG_DOEPINT_NAK;				// Clear interrupt
				}
			}
			epnum++;
			ep_intr >>= 1U;
		}

	}

	///////////		40000 		IEPINT: IN endpoint interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_IEPINT)) {

		// Read in the device interrupt bits [initially 1]
		ep_intr = (USBx_DEVICE->DAINT & USBx_DEVICE->DAINTMSK) & USB_OTG_DAINTMSK_IEPM_Msk;

		// process each endpoint in turn incrementing the epnum and checking the interrupts (ep_intr) if that endpoint fired
		epnum = 0;
		while (ep_intr != 0) {
			if ((ep_intr & 1) != 0) {

				epint = USBx_INEP(epnum)->DIEPINT & (USBx_DEVICE->DIEPMSK | (((USBx_DEVICE->DIEPEMPMSK >> (epnum & EP_ADDR_MASK)) & 0x1U) << 7));

				USBUpdateDbg(epint, {}, epnum, {}, {}, nullptr);

				if ((epint & USB_OTG_DIEPINT_XFRC) == USB_OTG_DIEPINT_XFRC) {			// 0x1 Transfer completed interrupt
					uint32_t fifoemptymsk = (0x1UL << (epnum & EP_ADDR_MASK));
					USBx_DEVICE->DIEPEMPMSK &= ~fifoemptymsk;

					USBx_INEP(epnum)->DIEPINT = USB_OTG_DIEPINT_XFRC;

					if (epnum == 0) {
						if (ep0State == EP0State::DataIn && ep0.inBuffRem == 0) {
							ep0State = EP0State::StatusOut;								// After completing transmission on EP0 send an out packet [HAL_PCD_EP_Receive]
							EPStartXfer(Direction::out, 0, ep_maxPacket);

						} else if (ep0State == EP0State::DataIn && ep0.inBuffRem > 0) {		// For EP0 long packets are sent separately rather than streamed out of the FIFO
							ep0.inBuffSize = ep0.inBuffRem;
							ep0.inBuffRem = 0;
							USBUpdateDbg({}, {}, {}, ep0.inBuffSize, {}, (uint32_t*)ep0.inBuff);

							EPStartXfer(Direction::in, 0, ep0.inBuffSize);
						}
					} else {
						classes[epnum]->DataIn();
						transmitting = false;
					}
				}

				if ((epint & USB_OTG_DIEPINT_TXFE) == USB_OTG_DIEPINT_TXFE) {			// 0x80 Transmit FIFO empty
					USBUpdateDbg({}, {}, {}, classes[epnum]->inBuffSize, {}, (uint32_t*)classes[epnum]->inBuff);

					if (epnum == 0) {
						if (ep0.inBuffSize > ep_maxPacket) {
							ep0.inBuffRem = classes[epnum]->inBuffSize - ep_maxPacket;
							ep0.inBuffSize = ep_maxPacket;
						}

						WritePacket(ep0.inBuff, epnum, ep0.inBuffSize);

						ep0.inBuff += ep0.inBuffSize;		// Move pointer forwards
						uint32_t fifoemptymsk = 1;
						USBx_DEVICE->DIEPEMPMSK &= ~fifoemptymsk;
					} else {

						// For regular endpoints keep writing packets to the FIFO while space available [PCD_WriteEmptyTxFifo]
						uint16_t len = std::min(classes[epnum]->inBuffSize - classes[epnum]->inBuffCount, (uint32_t)ep_maxPacket);
						uint16_t len32b = (len + 3) / 4;			// FIFO size is in 4 byte words

						// INEPTFSAV[15:0]: IN endpoint Tx FIFO space available: 0x0: Endpoint Tx FIFO is full; 0x1: 1 31-bit word available; 0xn: n words available
						while (((USBx_INEP(epnum)->DTXFSTS & USB_OTG_DTXFSTS_INEPTFSAV) >= len32b) && (classes[epnum]->inBuffCount < classes[epnum]->inBuffSize) && (classes[epnum]->inBuffSize != 0)) {

							len = std::min(classes[epnum]->inBuffSize - classes[epnum]->inBuffCount, (uint32_t)ep_maxPacket);
							len32b = (len + 3) / 4;

							WritePacket(classes[epnum]->inBuff, epnum, len);

							classes[epnum]->inBuff += len;
							classes[epnum]->inBuffCount += len;
						}

						if (classes[epnum]->inBuffSize <= classes[epnum]->inBuffCount) {
							uint32_t fifoemptymsk = (0x1UL << (epnum & EP_ADDR_MASK));
							USBx_DEVICE->DIEPEMPMSK &= ~fifoemptymsk;
						}
					}
				}

				if ((epint & USB_OTG_DIEPINT_TOC) == USB_OTG_DIEPINT_TOC) {					// Timeout condition
					USBx_INEP(epnum)->DIEPINT = USB_OTG_DIEPINT_TOC;
				}
				if ((epint & USB_OTG_DIEPINT_ITTXFE) == USB_OTG_DIEPINT_ITTXFE) {			// IN token received when Tx FIFO is empty
					USBx_INEP(epnum)->DIEPINT = USB_OTG_DIEPINT_ITTXFE;
				}
				if ((epint & USB_OTG_DIEPINT_INEPNE) == USB_OTG_DIEPINT_INEPNE) {			// IN endpoint NAK effective
					USBx_INEP(epnum)->DIEPINT = USB_OTG_DIEPINT_INEPNE;
				}
				if ((epint & USB_OTG_DIEPINT_EPDISD) == USB_OTG_DIEPINT_EPDISD) {			// Endpoint disabled interrupt
					USBx_INEP(epnum)->DIEPINT = USB_OTG_DIEPINT_EPDISD;
				}

			}
			epnum++;
			ep_intr >>= 1U;
		}

	}


	/////////// 	800 		USBSUSP: Suspend Interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_USBSUSP)) {
		if ((USBx_DEVICE->DSTS & USB_OTG_DSTS_SUSPSTS) == USB_OTG_DSTS_SUSPSTS) {
			devState = DeviceState::Suspended;
			USBx_PCGCCTL |= USB_OTG_PCGCCTL_STOPCLK;
		}
		USB_OTG_FS->GINTSTS &= USB_OTG_GINTSTS_USBSUSP;
	}


	/////////// 	1000 		USBRST: Reset Interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_USBRST))	{
		USBx_DEVICE->DCTL &= ~USB_OTG_DCTL_RWUSIG;

		// USB_FlushTxFifo
		USB_OTG_FS->GRSTCTL = (USB_OTG_GRSTCTL_TXFFLSH | (0x10 << 6));
		while ((USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH) == USB_OTG_GRSTCTL_TXFFLSH);

		for (int i = 0; i < 6; i++) {				// hpcd->Init.dev_endpoints
			USBx_INEP(i)->DIEPINT = 0xFB7FU;		// see p1177 for explanation: based on datasheet should be more like 0b10100100111011
			USBx_INEP(i)->DIEPCTL &= ~USB_OTG_DIEPCTL_STALL;
			USBx_OUTEP(i)->DOEPINT = 0xFB7FU;
			USBx_OUTEP(i)->DOEPCTL &= ~USB_OTG_DOEPCTL_STALL;
		}
		USBx_DEVICE->DAINTMSK |= 0x10001U;

		USBx_DEVICE->DOEPMSK |= USB_OTG_DOEPMSK_STUPM | USB_OTG_DOEPMSK_XFRCM | USB_OTG_DOEPMSK_EPDM | USB_OTG_DOEPMSK_OTEPSPRM;			//  | USB_OTG_DOEPMSK_NAKM
		USBx_DEVICE->DIEPMSK |= USB_OTG_DIEPMSK_TOM | USB_OTG_DIEPMSK_XFRCM | USB_OTG_DIEPMSK_EPDM;

		// Set Default Address to 0 (will be set later when address instruction received from host)
		USBx_DEVICE->DCFG &= ~USB_OTG_DCFG_DAD;

		// setup EP0 to receive SETUP packets
		//if ((USBx_OUTEP(0U)->DOEPCTL & USB_OTG_DOEPCTL_EPENA) != USB_OTG_DOEPCTL_EPENA)	{

			// Set PKTCNT to 1, XFRSIZ to 24, STUPCNT to 3 (number of back-to-back SETUP data packets endpoint can receive)
			USBx_OUTEP(0U)->DOEPTSIZ = (1U << 19) | (3U * 8U) | USB_OTG_DOEPTSIZ_STUPCNT;
		//}

		USB_OTG_FS->GINTSTS &= USB_OTG_GINTSTS_USBRST;
	}


	/////////// 	2000		ENUMDNE: Enumeration done Interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_ENUMDNE)) {
		// Set the Maximum packet size of the IN EP based on the enumeration speed
		USBx_INEP(0U)->DIEPCTL &= ~USB_OTG_DIEPCTL_MPSIZ;
		USBx_DEVICE->DCTL |= USB_OTG_DCTL_CGINAK;		//  Clear global IN NAK

		// Assuming Full Speed USB and clock > 32MHz Set USB Turnaround time
		USB_OTG_FS->GUSBCFG &= ~USB_OTG_GUSBCFG_TRDT;
		USB_OTG_FS->GUSBCFG |= (6 << 10);

		ActivateEndpoint(0, Direction::out, Control);			// Open EP0 OUT
		ActivateEndpoint(0, Direction::in, Control);			// Open EP0 IN

		ep0State = EP0State::Idle;

		USB_OTG_FS->GINTSTS &= USB_OTG_GINTSTS_ENUMDNE;
	}


	///////////		40000000	SRQINT: Connection event Interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_SRQINT))	{
		//HAL_PCD_ConnectCallback(hpcd);		// this doesn't seem to do anything
		USB_OTG_FS->GINTSTS &= USB_OTG_GINTSTS_SRQINT;
	}


	/////////// 	80000000	WKUINT: Resume Interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_WKUINT)) {
		// Clear the Remote Wake-up Signaling
		USBx_DEVICE->DCTL &= ~USB_OTG_DCTL_RWUSIG;
		USB_OTG_FS->GINTSTS &= USB_OTG_GINTSTS_WKUINT;
	}


	/////////// OTGINT: Handle Disconnection event Interrupt
	if (ReadInterrupts(USB_OTG_GINTSTS_OTGINT)) {
		uint32_t temp = USB_OTG_FS->GOTGINT;

		if ((temp & USB_OTG_GOTGINT_SEDET) == USB_OTG_GOTGINT_SEDET)
		{
			//HAL_PCD_DisconnectCallback(hpcd);
			//pdev->pClass->DeInit(pdev, (uint8_t)pdev->dev_config);
		}
		USB_OTG_FS->GOTGINT |= temp;
	}

}



void USB::Init()
{
#if (USB_DEBUG)
	InitUart();
#endif
	// USB_OTG_FS GPIO Configuration: PA8: USB_OTG_FS_SOF; PA9: USB_OTG_FS_VBUS; PA10: USB_OTG_FS_ID; PA11: USB_OTG_FS_DM; PA12: USB_OTG_FS_DP
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;

	// PA8 (SOF), PA10 (ID), PA11 (DM), PA12 (DP) (NB PA9 - VBUS uses default values)
	GPIOA->MODER |= GPIO_MODER_MODER10_1 | GPIO_MODER_MODER11_1 | GPIO_MODER_MODER12_1;					// 10: Alternate function mode
	GPIOA->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR10 | GPIO_OSPEEDER_OSPEEDR11 | GPIO_OSPEEDER_OSPEEDR12;		// 11: High speed
	GPIOA->AFR[1] |= (10 << 12) | (10 << 16);															// Alternate Function 10 is OTG_FS

	RCC->AHB2ENR |= RCC_AHB2ENR_OTGFSEN;				// USB OTG FS clock enable
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;				// Enable system configuration clock: used to manage external interrupt line connection to GPIOs

	NVIC_SetPriority(OTG_FS_IRQn, 0);
	NVIC_EnableIRQ(OTG_FS_IRQn);

	USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_PWRDWN;			// Activate the transceiver in transmission/reception. When reset, the transceiver is kept in power-down. 0 = USB FS transceiver disabled; 1 = USB FS transceiver enabled
	USB_OTG_FS->GUSBCFG |= USB_OTG_GUSBCFG_FDMOD;		// Force USB device mode

	// Clear all transmit FIFO address and data lengths - these will be set to correct values below for endpoints 0,1 and 2
	for (uint8_t i = 0U; i < 15U; i++) {
		USB_OTG_FS->DIEPTXF[i] = 0U;
	}

	USBx_DEVICE->DCFG |= USB_OTG_DCFG_DSPD;				// 11: Full speed using internal FS PHY

	USB_OTG_FS->GRSTCTL |= USB_OTG_GRSTCTL_TXFNUM_4;	// Select buffers to flush. 10000: Flush all the transmit FIFOs in device or host mode
	USB_OTG_FS->GRSTCTL |= USB_OTG_GRSTCTL_TXFFLSH;		// Flush the TX buffers
	while ((USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_TXFFLSH) == USB_OTG_GRSTCTL_TXFFLSH);

	USB_OTG_FS->GRSTCTL = USB_OTG_GRSTCTL_RXFFLSH;		// Flush the RX buffers
	while ((USB_OTG_FS->GRSTCTL & USB_OTG_GRSTCTL_RXFFLSH) == USB_OTG_GRSTCTL_RXFFLSH);

	USB_OTG_FS->GINTSTS = 0xBFFFFFFFU;					// Clear pending interrupts (except SRQINT Session request/new session detected)

	// Enable interrupts
	USB_OTG_FS->GINTMSK = 0U;							// Disable all interrupts
	USB_OTG_FS->GINTMSK |= USB_OTG_GINTMSK_RXFLVLM | USB_OTG_GINTMSK_USBSUSPM |			// Receive FIFO non-empty mask; USB suspend
			USB_OTG_GINTMSK_USBRST | USB_OTG_GINTMSK_ENUMDNEM |							// USB reset; Enumeration done
			USB_OTG_GINTMSK_IEPINT | USB_OTG_GINTMSK_OEPINT | USB_OTG_GINTMSK_WUIM |	// IN endpoint; OUT endpoint; Resume/remote wakeup detected
			USB_OTG_GINTMSK_SRQIM | USB_OTG_GINTMSK_OTGINT;								// Session request/new session detected; OTG interrupt

	// FIXME - add additional for extra endpoints (and increase for MSC endpoint?)
	// NB - FIFO Sizes are in words NOT bytes. There is a total size of 320 (320x4 = 1280 bytes) available which is divided up thus:
	// FIFO		Start		Size
	// RX 		0			128
	// EP0 TX	128			64
	// EP1 TX	192			64
	// EP2 TX 	256			64

	USB_OTG_FS->GRXFSIZ = 128;		 					// Rx FIFO depth

	// Endpoint 0 Transmit FIFO size/address (as in device mode - this is also used as the non-periodic transmit FIFO size in host mode)
	USB_OTG_FS->DIEPTXF0_HNPTXFSIZ = (64 << USB_OTG_TX0FD_Pos) |		// IN Endpoint 0 Tx FIFO depth
			(128 << USB_OTG_TX0FSA_Pos);								// IN Endpoint 0 FIFO transmit RAM start address - this is offset from the RX FIFO (set above to 128)

	// Endpoint 1 FIFO size/address (address is offset from EP0 address+size above)
	USB_OTG_FS->DIEPTXF[0] = (64 << USB_OTG_DIEPTXF_INEPTXFD_Pos) |		// IN endpoint 1 Tx FIFO depth
			(192 << USB_OTG_DIEPTXF_INEPTXSA_Pos);  					// IN endpoint 1 FIFO transmit RAM start address

	// Endpoint 2 FIFO size/address (address is offset from EP1 address+size above)
	USB_OTG_FS->DIEPTXF[1] = (64 << USB_OTG_DIEPTXF_INEPTXFD_Pos) |		// IN endpoint 2 Tx FIFO depth
			(256 << USB_OTG_DIEPTXF_INEPTXSA_Pos);  					// IN endpoint 2 FIFO transmit RAM start address

	USB_OTG_FS->GCCFG |= USB_OTG_GCCFG_VBUSBSEN; 		// Enable HW VBUS sensing
    USBx_DEVICE->DCTL &= ~USB_OTG_DCTL_SDIS;			// Activate USB
    USB_OTG_FS->GAHBCFG |= USB_OTG_GAHBCFG_GINT;		// Activate USB Interrupts
}


void USB::ActivateEndpoint(uint8_t endpoint, Direction direction, EndPointType eptype)
{
	endpoint = endpoint & 0xF;

	if (direction == Direction::in) {
		USBx_DEVICE->DAINTMSK |= USB_OTG_DAINTMSK_IEPM & (uint32_t)(1UL << (endpoint & EP_ADDR_MASK));

		if ((USBx_INEP(endpoint)->DIEPCTL & USB_OTG_DIEPCTL_USBAEP) == 0U) {
			USBx_INEP(endpoint)->DIEPCTL |= (ep_maxPacket & USB_OTG_DIEPCTL_MPSIZ) |
					((uint32_t)eptype << 18) | (endpoint << 22) |
					USB_OTG_DIEPCTL_USBAEP;
		}
	} else {
		USBx_DEVICE->DAINTMSK |= USB_OTG_DAINTMSK_OEPM & ((uint32_t)(1UL << (endpoint & EP_ADDR_MASK)) << 16);

		if (((USBx_OUTEP(endpoint)->DOEPCTL) & USB_OTG_DOEPCTL_USBAEP) == 0U) {
			USBx_OUTEP(endpoint)->DOEPCTL |= (ep_maxPacket & USB_OTG_DOEPCTL_MPSIZ) |
					((uint32_t)eptype << 18) |
					USB_OTG_DOEPCTL_USBAEP;
		}
	}
}


void USB::ReadPacket(const uint32_t* dest, uint16_t len, uint32_t offset)
{
	// Read a packet from the RX FIFO
	uint32_t* pDest = (uint32_t*)dest + offset;
	uint32_t count32b = ((uint32_t)len + 3U) / 4U;

	for (uint32_t i = 0; i < count32b; i++)	{
		*pDest = USBx_DFIFO(0);
		pDest++;
	}
}


void USB::WritePacket(const uint8_t* src, uint8_t endpoint, uint32_t len)
{
	uint32_t* pSrc = (uint32_t*)src;
	uint32_t count32b = ((uint32_t)len + 3U) / 4U;

	for (uint32_t i = 0; i < count32b; i++) {
		USBx_DFIFO(endpoint) = *pSrc;
		pSrc++;
	}
}

// Descriptors in usbd_desc.c
void USB::GetDescriptor() {
	uint32_t strSize;

	switch (req.Value >> 8)	{
	case USB_DESC_TYPE_DEVICE:
		return EP0In(USBD_FS_DeviceDesc, sizeof(USBD_FS_DeviceDesc));
		break;

	case USB_DESC_TYPE_CONFIGURATION:
		return EP0In(ConfigDesc, sizeof(ConfigDesc));
		break;

	case USB_DESC_TYPE_BOS:
		return EP0In(USBD_FS_BOSDesc, sizeof(USBD_FS_BOSDesc));
		break;

//	case USB_DESC_TYPE_DEVICE_QUALIFIER:
//		return EP0In(USBD_MSC_DeviceQualifierDesc, sizeof(USBD_MSC_DeviceQualifierDesc));
//		break;

	case USB_DESC_TYPE_STRING:

		switch ((uint8_t)(req.Value)) {
		case USBD_IDX_LANGID_STR:			// 300
			return EP0In(USBD_LangIDDesc, sizeof(USBD_LangIDDesc));
			break;

		case USBD_IDX_MFC_STR:				// 301
			strSize = StringToUnicode((uint8_t*)USBD_MANUFACTURER_STRING, USBD_StrDesc);
			return EP0In(USBD_StrDesc, strSize);
			break;

		case USBD_IDX_PRODUCT_STR:			// 302
			strSize = StringToUnicode((uint8_t*)USBD_PRODUCT_STRING, USBD_StrDesc);
			return EP0In(USBD_StrDesc, strSize);
			break;

		case USBD_IDX_SERIAL_STR:			// 303
			{
				// STM32 unique device ID (96 bit number starting at UID_BASE)
				uint32_t deviceserial0 = *(uint32_t*) UID_BASE;
				uint32_t deviceserial1 = *(uint32_t*) UID_BASE + 4;
				uint32_t deviceserial2 = *(uint32_t*) UID_BASE + 8;
				deviceserial0 += deviceserial2;

				if (deviceserial0 != 0) {
					IntToUnicode(deviceserial0, &USBD_StringSerial[2], 8);
					IntToUnicode(deviceserial1, &USBD_StringSerial[18], 4);
				}
				return EP0In(USBD_StringSerial, sizeof(USBD_StringSerial));
			}
			break;
	    case USBD_IDX_CFG_STR:				// 304
	    	strSize = StringToUnicode((uint8_t*)USBD_CFG_STRING, USBD_StrDesc);
	    	return EP0In(USBD_StrDesc, strSize);
	    	break;

	    case USBD_IDX_MSC_STR:				// 305
	    	strSize = StringToUnicode((uint8_t*)USBD_MSC_STRING, USBD_StrDesc);
	    	return EP0In(USBD_StrDesc, strSize);
			break;

	    case USBD_IDX_CDC_STR:				// 306
	    	strSize = StringToUnicode((uint8_t*)USBD_CDC_STRING, USBD_StrDesc);
	    	return EP0In(USBD_StrDesc, strSize);
			break;

	    case USBD_IDX_MIDI_STR:				// 307
	    	strSize = StringToUnicode((uint8_t*)USBD_MIDI_STRING, USBD_StrDesc);
	    	return EP0In(USBD_StrDesc, strSize);
			break;

		default:
			CtlError();
			return;
		}
		break;

		default:
			CtlError();
			return;
	}

	if (req.Length == 0U) {
		EPStartXfer(Direction::in, 0, 0);			// FIXME - this never seems to happen
	}
}


uint32_t USB::StringToUnicode(const uint8_t* desc, uint8_t *unicode)
{
	uint32_t idx = 2;

	if (desc != NULL) {
		while (*desc != '\0') {
			unicode[idx++] = *desc++;
			unicode[idx++] = 0;
		}
		unicode[0] = idx;
		unicode[1] = USB_DESC_TYPE_STRING;
	}
	return idx;
}


void USB::IntToUnicode(uint32_t value, uint8_t* pbuf, uint8_t len)
{
	for (uint8_t idx = 0; idx < len; idx++) {
		if ((value >> 28) < 0xA) {
			pbuf[2 * idx] = (value >> 28) + '0';
		} else {
			pbuf[2 * idx] = (value >> 28) + 'A' - 10;
		}

		value = value << 4;
		pbuf[2 * idx + 1] = 0;
	}
}


void USB::StdDevReq()
{
	uint8_t dev_addr;
	switch (req.RequestType & USB_REQ_TYPE_MASK)
	{
	case USB_REQ_TYPE_CLASS:
	case USB_REQ_TYPE_VENDOR:
		// pdev->pClass->Setup(pdev, req);
		break;

	case USB_REQ_TYPE_STANDARD:

		switch (req.Request)
		{
		case USB_REQ_GET_DESCRIPTOR:
			GetDescriptor();
			break;

		case USB_REQ_SET_ADDRESS:
			dev_addr = (uint8_t)(req.Value) & 0x7FU;
			USBx_DEVICE->DCFG &= ~(USB_OTG_DCFG_DAD);
			USBx_DEVICE->DCFG |= ((uint32_t)dev_addr << 4) & USB_OTG_DCFG_DAD;
			ep0State = EP0State::StatusIn;
			EPStartXfer(Direction::in, 0, 0);
			devState = DeviceState::Addressed;
			break;

		case USB_REQ_SET_CONFIGURATION:
			if (devState == DeviceState::Addressed) {
				devState = DeviceState::Configured;

				ActivateEndpoint(Midi_In,   Direction::in,  Bulk);			// Activate MSC in endpoint
				ActivateEndpoint(Midi_Out,  Direction::out, Bulk);			// Activate MSC out endpoint
				ActivateEndpoint(CDC_In,   Direction::in,  Bulk);			// Activate CDC in endpoint
				ActivateEndpoint(CDC_Out,  Direction::out, Bulk);			// Activate CDC out endpoint
				ActivateEndpoint(CDC_Cmd,  Direction::in,  Interrupt);		// Activate Command IN EP
//				ActivateEndpoint(MIDI_In,  Direction::in,  Bulk);			// Activate MIDI in endpoint
//				ActivateEndpoint(MIDI_Out, Direction::out, Bulk);			// Activate MIDI out endpoint

				ep0State = EP0State::StatusIn;
				EPStartXfer(Direction::in, 0, 0);

			}
			break;

		/*
		case USB_REQ_GET_CONFIGURATION:
			// USBD_GetConfig (pdev, req);
			break;

		case USB_REQ_GET_STATUS:
			//USBD_GetStatus (pdev, req);
			break;

		case USB_REQ_SET_FEATURE:
			//USBD_SetFeature (pdev, req);
			break;

		case USB_REQ_CLEAR_FEATURE:
			//USBD_ClrFeature (pdev, req);
			break;
*/

		default:
			CtlError();
			break;
		}
		break;

		default:
			CtlError();
			break;
	}

}

void USB::EPStartXfer(Direction direction, uint8_t endpoint, uint32_t xfer_len) {

	if (direction == Direction::in) {				// IN endpoint

		endpoint = endpoint & EP_ADDR_MASK;			// Strip out 0x80 if endpoint passed eg as 0x81

		USBx_INEP(endpoint)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_PKTCNT);
		USBx_INEP(endpoint)->DIEPTSIZ &= ~(USB_OTG_DIEPTSIZ_XFRSIZ);

		if (endpoint == 0 && xfer_len > ep_maxPacket) {				// If the transfer is larger than the maximum packet size send the maximum size and use the remaining flag to trigger a second send
			ep0.inBuffRem = xfer_len - ep_maxPacket;
			xfer_len = ep_maxPacket;
		}

		USBx_INEP(endpoint)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_PKTCNT & (((xfer_len + ep_maxPacket - 1) / ep_maxPacket) << 19));
		USBx_INEP(endpoint)->DIEPTSIZ |= (USB_OTG_DIEPTSIZ_XFRSIZ & xfer_len);

		USBx_INEP(endpoint)->DIEPCTL |= (USB_OTG_DIEPCTL_CNAK | USB_OTG_DIEPCTL_EPENA);	// EP enable, IN data in FIFO

		// Enable the Tx FIFO Empty Interrupt for this EP
		if (xfer_len > 0) {
			USBx_DEVICE->DIEPEMPMSK |= 1UL << endpoint;
		}
	} else { 		// OUT endpoint

		classes[endpoint]->outBuffPackets = 1;
		classes[endpoint]->outBuffOffset = 0;

		// If the transfer is larger than the maximum packet size send the total size and number of packets calculated from the end point maximum packet size
		if (xfer_len > ep_maxPacket) {
			classes[endpoint]->outBuffPackets = (xfer_len + ep_maxPacket - 1U) / ep_maxPacket;
		}

		USBx_OUTEP(endpoint)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_XFRSIZ);
		USBx_OUTEP(endpoint)->DOEPTSIZ &= ~(USB_OTG_DOEPTSIZ_PKTCNT);

		USBx_OUTEP(endpoint)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_PKTCNT & (classes[endpoint]->outBuffPackets << 19));
		USBx_OUTEP(endpoint)->DOEPTSIZ |= (USB_OTG_DOEPTSIZ_XFRSIZ & xfer_len);

		USBx_OUTEP(endpoint)->DOEPCTL |= (USB_OTG_DOEPCTL_CNAK | USB_OTG_DOEPCTL_EPENA);		// EP enable
	}
}

void USB::CtlError() {
	USBx_INEP(0)->DIEPCTL |= USB_OTG_DIEPCTL_STALL;
	USBx_OUTEP(0)->DOEPCTL |= USB_OTG_DOEPCTL_STALL;

	// Set PKTCNT to 1, XFRSIZ to 24, STUPCNT to 3 (number of back-to-back SETUP data packets endpoint can receive)
	//USBx_OUTEP(0U)->DOEPTSIZ = (1U << 19) | (3U * 8U) | USB_OTG_DOEPTSIZ_STUPCNT;
}


bool USB::ReadInterrupts(uint32_t interrupt) {

#if (USB_DEBUG)
	if (((USB_OTG_FS->GINTSTS & USB_OTG_FS->GINTMSK) & interrupt) == interrupt && usbDebugEvent < USB_DEBUG_COUNT && debugStart) {
		usbDebugNo = usbDebugEvent % USB_DEBUG_COUNT;
		usbDebug[usbDebugNo].eventNo = usbDebugEvent;
		usbDebug[usbDebugNo].Interrupt = USB_OTG_FS->GINTSTS & USB_OTG_FS->GINTMSK;
		usbDebugEvent++;
	}
#endif

	return ((USB_OTG_FS->GINTSTS & USB_OTG_FS->GINTMSK) & interrupt) == interrupt;
}


void USB::SendData(const uint8_t* data, uint16_t len, uint8_t endpoint)
{
	endpoint &= EP_ADDR_MASK;
	if (devState == DeviceState::Configured) {
		if (!transmitting) {
			transmitting = true;
			classes[endpoint]->inBuff = data;
			classes[endpoint]->inBuffSize = len;
			classes[endpoint]->inBuffCount = 0;
			ep0State = EP0State::DataIn;
			EPStartXfer(Direction::in, endpoint, len);
		}
	}
}


void USB::SendString(const char* s)
{
	while (transmitting);
	SendData((uint8_t*)s, strlen(s), CDC_In);
}


#if (USB_DEBUG)

#include <string>

std::string IntToString(const int32_t& v) {
	return std::to_string(v);
}

std::string HexToString(const uint32_t& v, const bool& spaces) {
	char buf[20];
	if (spaces) {
		if (v != 0) {
			uint8_t* bytes = (uint8_t*)&v;
			sprintf(buf, "%02X%02X%02X%02X", bytes[0], bytes[1], bytes[2], bytes[3]);
		} else {
			sprintf(buf, " ");
		}
	} else {
		sprintf(buf, "%X", (unsigned int)v);
	}
	return std::string(buf);

}

std::string HexByte(const uint16_t& v) {
	char buf[50];
	sprintf(buf, "%X", v);
	return std::string(buf);

}


void USB::OutputDebug() {

	uartSendString("Event,Interrupt,Desc,Int Data,Desc,Endpoint,mRequest,Request,Value,Index,Length,Scsi,PacketSize,XferBuff\n");
	uint16_t evNo = usbDebugEvent % USB_DEBUG_COUNT;
	std::string interrupt, subtype;
	for (int i = 0; i < USB_DEBUG_COUNT; ++i) {
		switch (usbDebug[evNo].Interrupt) {
		case USB_OTG_GINTSTS_RXFLVL:
			interrupt = "RXFLVL";

			switch ((usbDebug[evNo].IntData & USB_OTG_GRXSTSP_PKTSTS) >> 17) {
			case STS_DATA_UPDT:			// 2 = OUT data packet received
				subtype = "Out packet rec";
				break;
			case STS_XFER_COMP:			// 3 = Transfer completed
				subtype = "Transfer completed";
				break;
			case STS_SETUP_UPDT:		// 6 = SETUP data packet received
				subtype = "Setup packet rec";
				break;
			case STS_SETUP_COMP:		// 4 = SETUP comp
				subtype = "Setup comp";
				break;
			default:
				subtype = "";
			}

			break;
		case USB_OTG_GINTSTS_SRQINT:
			interrupt = "SRQINT";
			break;
		case USB_OTG_GINTSTS_USBSUSP:
			interrupt = "USBSUSP";
			break;
		case USB_OTG_GINTSTS_WKUINT:
			interrupt = "WKUINT";
			break;
		case USB_OTG_GINTSTS_USBRST:
			interrupt = "USBRST";
			subtype = "";
			break;
		case USB_OTG_GINTSTS_ENUMDNE:
			interrupt = "ENUMDNE";
			break;
		case USB_OTG_GINTSTS_OEPINT:
			interrupt = "OEPINT";

			switch (usbDebug[evNo].IntData) {
			case USB_OTG_DOEPINT_XFRC:
				subtype = "Transfer completed";
				break;
			case USB_OTG_DOEPINT_STUP:
				subtype = "Setup phase done";
				break;
			default:
				subtype = "";
			}
			break;
		case USB_OTG_GINTSTS_IEPINT:
			interrupt = "IEPINT";

				switch (usbDebug[evNo].IntData) {
				case USB_OTG_DIEPINT_XFRC:
					subtype = "Transfer completed";
					break;
				case USB_OTG_DIEPINT_TXFE:
					subtype = "Transmit FIFO empty";
					break;
				default:
					subtype = "";
				}

			break;
		default:
			interrupt = "";
		}

		if (usbDebug[evNo].Interrupt != 0) {
			uartSendString(IntToString(usbDebug[evNo].eventNo) + ","
					+ HexToString(usbDebug[evNo].Interrupt, false) + "," + interrupt + ","
					+ HexToString(usbDebug[evNo].IntData, false) + "," + subtype + ","
					+ IntToString(usbDebug[evNo].endpoint) + ","
					+ HexByte(usbDebug[evNo].Request.RequestType) + ", "
					+ HexByte(usbDebug[evNo].Request.Request) + ", "
					+ HexByte(usbDebug[evNo].Request.Value) + ", "
					+ HexByte(usbDebug[evNo].Request.Index) + ", "
					+ HexByte(usbDebug[evNo].Request.Length) + ", "
					+ HexByte(usbDebug[evNo].scsiOpCode) + ", "
					+ IntToString(usbDebug[evNo].PacketSize) + ", "
					+ HexToString(usbDebug[evNo].xferBuff[0], true) + " "
					+ HexToString(usbDebug[evNo].xferBuff[1], true) + " "
					+ HexToString(usbDebug[evNo].xferBuff[2], true) + " "
					+ HexToString(usbDebug[evNo].xferBuff[3], true) + "\n");
		}
		evNo = (evNo + 1) % USB_DEBUG_COUNT;
	}
}


/*
startup sequence:
Event,Interrupt,Desc,Int Data,Desc,Endpoint,mRequest,Request,Value,Index,Length,Scsi,PacketSize,XferBuff
0,40000000,SRQINT,0,,0,0, 0, 0, 0, 0, 0, 0,
1,800,USBSUSP,0,,0,0, 0, 0, 0, 0, 0, 0,
2,1000,USBRST,0,,0,0, 0, 0, 0, 0, 0, 0,
3,2000,ENUMDNE,0,,0,0, 0, 0, 0, 0, 0, 0,
4,10,RXFLVL,8C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060001 00004000
5,10,RXFLVL,880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
6,80000,OEPINT,8,Setup phase done,0,80, 6, 100, 0, 40, 0, 18, 12010102 EF020140 83042A57 00020102
7,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 18, 12010102 EF020140 83042A57 00020102
8,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
9,10,RXFLVL,A50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
10,10,RXFLVL,A70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
11,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
12,10,RXFLVL,AC0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 00052A00
13,10,RXFLVL,A80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
14,80000,OEPINT,8,Setup phase done,0,0, 5, 2A, 0, 0, 0, 0,
15,10,RXFLVL,1EC0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060001 00001200
16,10,RXFLVL,1E80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
17,80000,OEPINT,8,Setup phase done,0,80, 6, 100, 0, 12, 0, 18, 12010102 EF020140 83042A57 00020102
18,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 18, 12010102 EF020140 83042A57 00020102
19,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
20,10,RXFLVL,50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
21,10,RXFLVL,70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
22,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
23,10,RXFLVL,8C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060002 0000FF00
24,10,RXFLVL,880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
25,80000,OEPINT,8,Setup phase done,0,80, 6, 200, 0, FF, 0, 32, 09022000 010104C0 32090400 00020806
26,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 32, 09022000 010104C0 32090400 00020806
27,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
28,10,RXFLVL,850000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
29,10,RXFLVL,870000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
30,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
31,10,RXFLVL,8C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 8006000F 0000FF00
32,10,RXFLVL,880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
33,80000,OEPINT,8,Setup phase done,0,80, 6, F00, 0, FF, 0, 12, 050F0C00 01071002 02000000 1A030000
34,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 12, 050F0C00 01071002 02000000 1A030000
35,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
36,10,RXFLVL,850000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
37,10,RXFLVL,870000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
38,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
39,10,RXFLVL,8C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060303 0904FF00
40,10,RXFLVL,880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
41,80000,OEPINT,8,Setup phase done,0,80, 6, 303, 409, FF, 0, 26, 1A033000 30003500 43003000 30003600
42,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 26, 1A033000 30003500 43003000 30003600
43,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
44,10,RXFLVL,850000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
45,10,RXFLVL,870000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
46,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
47,10,RXFLVL,8C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060003 0000FF00
48,10,RXFLVL,880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
49,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, FF, 0, 4, 04030904 0A060002 01000040 01000000
50,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 4, 04030904 0A060002 01000040 01000000
51,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
52,10,RXFLVL,A50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
53,10,RXFLVL,A70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
54,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
55,10,RXFLVL,AC0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060203 0904FF00
56,10,RXFLVL,A80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
57,80000,OEPINT,8,Setup phase done,0,80, 6, 302, 409, FF, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
58,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
59,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
60,10,RXFLVL,A50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
61,10,RXFLVL,A70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
62,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
63,10,RXFLVL,AC0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060006 00000A00
64,10,RXFLVL,A80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
65,80000,OEPINT,8,Setup phase done,0,80, 6, 600, 0, A, 0, 0,
66,10,RXFLVL,12C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060003 00000200
67,10,RXFLVL,1280000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
68,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, 2, 0, 4, 04030904 0A060002 01000040 01001A03
69,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 2, 04030904 0A060002 01000040 01001A03
70,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
71,10,RXFLVL,1250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
72,10,RXFLVL,1270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
73,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
74,10,RXFLVL,12C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060003 00000400
75,10,RXFLVL,1280000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
76,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, 4, 0, 4, 04030904 0A060002 01000040 01001A03
77,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 4, 04030904 0A060002 01000040 01001A03
78,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
79,10,RXFLVL,1250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
80,10,RXFLVL,1270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
81,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
82,10,RXFLVL,12C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060303 09040200
83,10,RXFLVL,1280000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
84,80000,OEPINT,8,Setup phase done,0,80, 6, 303, 409, 2, 0, 26, 1A033000 30003500 43003000 30003600
85,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 2, 1A033000 30003500 43003000 30003600
86,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
87,10,RXFLVL,1250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
88,10,RXFLVL,1270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
89,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
90,10,RXFLVL,12C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060303 09041A00
91,10,RXFLVL,1280000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
92,80000,OEPINT,8,Setup phase done,0,80, 6, 303, 409, 1A, 0, 26, 1A033000 30003500 43003000 30003600
93,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 26, 1A033000 30003500 43003000 30003600
94,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
95,10,RXFLVL,1250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
96,10,RXFLVL,1270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
97,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
98,10,RXFLVL,16C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 00090100
99,10,RXFLVL,1680000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
100,80000,OEPINT,8,Setup phase done,0,0, 9, 1, 0, 0, 0, 0,
101,10,RXFLVL,18C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, A1FE0000 00000100
102,10,RXFLVL,1880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
103,80000,OEPINT,8,Setup phase done,0,A1, FE, 0, 0, 1, 0, 1,
104,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 1,
105,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
106,10,RXFLVL,1850000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
107,10,RXFLVL,1870000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
108,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
109,10,RXFLVL,18401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 20559EDE 24000000 80000612
110,10,RXFLVL,1860001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
111,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 12, 0,
112,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 36, 00800202 1F000000 53544D20 20202020
113,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
114,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 20559EDE
115,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
116,10,RXFLVL,18501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 B08479D9 24000000 80000612
117,10,RXFLVL,1870001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
118,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 12, 0,
119,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 36, 00800202 1F000000 53544D20 20202020
120,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
121,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 B08479D9
122,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
123,10,RXFLVL,18401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 E05445E2 FC000000 80000A23
124,10,RXFLVL,1860001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
125,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 23, 0,
126,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 12, 00000008 00000031 02000200
127,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
128,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 E05445E2 F0000000
129,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
130,10,RXFLVL,18501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 E034FFE0 FF000000 80000612
131,10,RXFLVL,1870001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
132,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 12, 0,
133,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00800008 20202020 00800202 1F000000
134,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
135,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 E034FFE0 F7000000
136,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
137,10,RXFLVL,C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060003 0000FF00
138,10,RXFLVL,80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
139,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, FF, 0, 4, 04030904 0A060002 01000040 01001A03
140,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 4, 04030904 0A060002 01000040 01001A03
141,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
142,10,RXFLVL,50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
143,10,RXFLVL,70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
144,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
145,10,RXFLVL,C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060003 0000FF00
146,10,RXFLVL,80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
147,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, FF, 0, 4, 04030904 0A060002 01000040 01001A03
148,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 4, 04030904 0A060002 01000040 01001A03
149,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
150,10,RXFLVL,50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
151,10,RXFLVL,70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
152,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
153,10,RXFLVL,C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060103 0904FF00
154,10,RXFLVL,80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
155,80000,OEPINT,8,Setup phase done,0,80, 6, 301, 409, FF, 0, 34, 22034D00 6F007500 6E007400 6A006F00
156,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 34, 22034D00 6F007500 6E007400 6A006F00
157,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
158,10,RXFLVL,50000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
159,10,RXFLVL,70000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
160,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
161,10,RXFLVL,C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060103 0904FF00
162,10,RXFLVL,80000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
163,80000,OEPINT,8,Setup phase done,0,80, 6, 301, 409, FF, 0, 34, 22034D00 6F007500 6E007400 6A006F00
164,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 34, 22034D00 6F007500 6E007400 6A006F00
165,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
166,10,RXFLVL,250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
167,10,RXFLVL,270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
168,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
169,10,RXFLVL,2C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060203 0904FF00
170,10,RXFLVL,280000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
171,80000,OEPINT,8,Setup phase done,0,80, 6, 302, 409, FF, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
172,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
173,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
174,10,RXFLVL,250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
175,10,RXFLVL,270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
176,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
177,10,RXFLVL,2C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060203 0904FF00
178,10,RXFLVL,280000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
179,80000,OEPINT,8,Setup phase done,0,80, 6, 302, 409, FF, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
180,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
181,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
182,10,RXFLVL,250000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
183,10,RXFLVL,270000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
184,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
185,10,RXFLVL,2401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A098CBE3 08000000 80000A25
186,10,RXFLVL,260001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
187,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
188,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 02000200
189,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
190,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A098CBE3
191,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
192,10,RXFLVL,2501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A008D3E8 C0000000 8000061A
193,10,RXFLVL,270001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
194,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 1A, 0,
195,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 23, 22000000 08120000
196,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
197,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A008D3E8 A9000000
198,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
199,10,RXFLVL,4401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A058DDE2 C0000000 8000061A
200,10,RXFLVL,460001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
201,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 1A, 0,
202,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 23, 22000000 08120000
203,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
204,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A058DDE2 A9000000
205,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
206,10,RXFLVL,4501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 E0F4B4EA 08000000 80000A25
207,10,RXFLVL,470001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
208,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
209,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 02000200
210,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
211,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 E0F4B4EA
212,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
213,10,RXFLVL,4401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 E0C4D5E3 24000000 80000612
214,10,RXFLVL,460001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
215,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 12, 0,
216,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 36, 00800202 1F000000 53544D20 20202020
217,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
218,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 E0C4D5E3
219,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
220,10,RXFLVL,4501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A058DDE2 08000000 80000A25
221,10,RXFLVL,470001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
222,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
223,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 02000200
224,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
225,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A058DDE2
226,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
227,10,RXFLVL,6401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A058DDE2 08000000 80000A25
228,10,RXFLVL,660001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
229,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
230,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 02000200
231,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
232,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A058DDE2
233,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
234,10,RXFLVL,6501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A058DDE2 08000000 80000A25
235,10,RXFLVL,670001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
236,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
237,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 02000200
238,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
239,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A058DDE2
240,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
241,10,RXFLVL,6401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A058DDE2 00020000 80000A28
242,10,RXFLVL,660001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
243,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
244,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512, EBFE904D 53444F53 352E3000 02010100
245,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
246,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
247,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
248,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A058DDE2
249,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
250,10,RXFLVL,8501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A068D0DB 00200000 80000A28
251,10,RXFLVL,870001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
252,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
253,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512, EBFE904D 53444F53 352E3000 02010100
254,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
255,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
256,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
257,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512, F8FFFFFF 0F000000
258,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
259,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
260,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
261,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512, 53544D33 32202020 54585420
262,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
263,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
264,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
265,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
266,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
267,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
268,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
269,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
270,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
271,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
272,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
273,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
274,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
275,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
276,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
277,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
278,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
279,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
280,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
281,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
282,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
283,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
284,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
285,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
286,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
287,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
288,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
289,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
290,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
291,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
292,10,RXFLVL,16C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060003 00000400
293,10,RXFLVL,1680000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
294,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, 4, 0, 4, 04030904 0A060002 01000040 01001A03
295,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 4, 04030904 0A060002 01000040 01001A03
296,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
297,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
298,10,RXFLVL,1650000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
299,10,RXFLVL,1670000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
300,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
301,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
302,10,RXFLVL,16C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060103 0904FF00
303,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
304,10,RXFLVL,1680000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
305,80000,OEPINT,8,Setup phase done,0,80, 6, 301, 409, FF, 0, 34, 22034D00 6F007500 6E007400 6A006F00
306,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 34, 22034D00 6F007500 6E007400 6A006F00
307,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
308,10,RXFLVL,1650000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
309,10,RXFLVL,1670000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
310,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
311,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
312,40010,,16C0080,Transfer completed,0,0, 0, 0, 0, 0, 0, 8, 80060003 00000400
313,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
314,10,RXFLVL,1680000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
315,80000,OEPINT,8,Setup phase done,0,80, 6, 300, 0, 4, 0, 4, 04030904 0A060002 01000040 01002203
316,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 4, 04030904 0A060002 01000040 01002203
317,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
318,10,RXFLVL,1650000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
319,10,RXFLVL,1670000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
320,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
321,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
322,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
323,10,RXFLVL,18C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060203 0904FF00
324,10,RXFLVL,1880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
325,80000,OEPINT,8,Setup phase done,0,80, 6, 302, 409, FF, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
326,40000,IEPINT,80,Transmit FIFO empty,0,0, 0, 0, 0, 0, 0, 26, 1A034D00 6F007500 6E007400 6A006F00
327,40000,IEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
328,10,RXFLVL,1850000,Out packet rec,0,0, 0, 0, 0, 0, 0, 0,
329,10,RXFLVL,1870000,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
330,80000,OEPINT,1,Transfer completed,0,0, 0, 0, 0, 0, 0, 0,
331,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
332,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
333,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
334,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
335,10,RXFLVL,18C0080,Setup packet rec,0,0, 0, 0, 0, 0, 0, 8, 80060006 00000A00
336,10,RXFLVL,1880000,Setup comp,0,0, 0, 0, 0, 0, 0, 0,
337,80000,OEPINT,8,Setup phase done,0,80, 6, 600, 0, A, 0, 0,
338,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
339,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
340,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
341,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
342,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
343,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
344,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
345,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
346,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
347,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
348,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
349,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
350,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
351,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A068D0DB
352,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
353,10,RXFLVL,1E401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A098CBE3 08000000 80000A25
354,10,RXFLVL,1E60001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
355,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
356,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200
357,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
358,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A098CBE3
359,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
360,10,RXFLVL,1E501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A098CBE3 08000000 80000A25
361,10,RXFLVL,1E70001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
362,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
363,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200
364,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
365,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A098CBE3
366,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
367,10,RXFLVL,1E401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A098CBE3 08000000 80000A25
368,10,RXFLVL,1E60001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
369,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
370,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200
371,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
372,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A098CBE3
373,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
374,10,RXFLVL,501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A0D8D9DB 00020000 80000A28
375,10,RXFLVL,70001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
376,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 28, 0,
377,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512, 53544D33 32202020 54585420
378,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
379,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 512,
380,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
381,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A0D8D9DB
382,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
383,10,RXFLVL,401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A098CCDE 08000000 80000A25
384,10,RXFLVL,60001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
385,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
386,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 54585420
387,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
388,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A098CCDE
389,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
390,10,RXFLVL,2501F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A01897DD 08000000 80000A25
391,10,RXFLVL,270001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
392,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 25, 0,
393,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 8, 00000031 00000200 54585420
394,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
395,40000,IEPINT,80,Transmit FIFO empty,1,0, 0, 0, 0, 0, 0, 13, 55534253 A01897DD
396,40000,IEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
397,10,RXFLVL,2401F1,Out packet rec,1,0, 0, 0, 0, 0, 0, 31, 55534243 A068D0DB 08000000 80000A25
398,10,RXFLVL,260001,Transfer completed,1,0, 0, 0, 0, 0, 0, 0,
399,80000,OEPINT,1,Transfer completed,1,0, 0, 0, 0, 0, 2A, 13, 55534253 609400E2

*/

#endif
