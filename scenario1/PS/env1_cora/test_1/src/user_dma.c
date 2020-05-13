/*
 * user_dma.c
 *
 *  Created on: Mar 6, 2020
 *      Author: Gerard Planella
 */

#include "user_dma.h"


/*
 * Flags interrupt handlers use to notify the application context the events.
 */
volatile DmaFlags flags;
static XAxiDma axiDma;
volatile char test_active;



//Handler for the DMA s2mm interrupt
static void handler_axidma_s2mm(void *CallbackRef);

//Handler for the DMA mm2s interrupt
static void handler_axidma_mm2s(void *CallbackRef);



int user_axidma_init(void){
	int status = 0;
	XAxiDma_Config *config;


	flags.transfer_done[0] = flags.transfer_done[1] = flags.error[0] = flags.error[1] = DMA_FLAG_NACTIVE;

	config = XAxiDma_LookupConfig(XPAR_AXI_DMA_0_DEVICE_ID);
	if (!config) {
		xil_printf("No configuration found for %d\r\n", XPAR_AXI_DMA_0_DEVICE_ID);
		return XST_FAILURE;
	}

	/* Initialize DMA engine */
	status = XAxiDma_CfgInitialize(&axiDma, config);
	if (status != XST_SUCCESS) {
		xil_printf("Initialization failed %d\r\n", status);
		return XST_FAILURE;
	}

	XAxiDma_Reset(&axiDma);
	while(!XAxiDma_ResetIsDone(&axiDma)){}

	if(XAxiDma_HasSg(&axiDma)){
		xil_printf("Device configured as SG mode \r\n");
		return XST_FAILURE;
	}

	status = user_axidma_interrupts_config(DMA_FLAG_NACTIVE, DMA_FLAG_ACTIVE, DMA_FLAG_ACTIVE, DMA_FLAG_NACTIVE);
	if (status != XST_SUCCESS) {
		xil_printf("Failed to disable DMA interrupts\r\n");
		return XST_FAILURE;
	}

	status = interrupts_dma_init(&axiDma, ((Xil_ExceptionHandler)handler_axidma_mm2s), ((Xil_ExceptionHandler)handler_axidma_s2mm));
	if (status != XST_SUCCESS) {
		xil_printf("[DMA Init]: Interrupt setup failed %d\r\n", status);
		return XST_FAILURE;
	}


	user_axidma_interrupts_config(DMA_FLAG_ACTIVE, DMA_FLAG_ACTIVE, DMA_FLAG_ACTIVE, DMA_FLAG_NACTIVE);
	return XST_SUCCESS;

}

/*
 *DMA transfer function from DDR to PL AXIS interface (FIFO), after calling the function one must wait for the flags.tx_done or flags.rx_done flag to activate.
 *We also interact with the counters to be able to produce the time measurements for the test
 */

int user_dma_transfer(char mm2s, u32 address, u32 nBytes, char option){
	u32 WordBits;
	int RingIndex = 0;

	flags.transfer_done[0] = flags.transfer_done[1] = 0;
	flags.error[0] = flags.error[1] = 0;

	if(mm2s == DMA_MM2S){

		if ((nBytes < 1)) return XST_INVALID_PARAM;

		if (nBytes > axiDma.TxBdRing.MaxTransferLen) nBytes = axiDma.TxBdRing.MaxTransferLen;

		if (!axiDma.HasMm2S) return XST_FAILURE;

		user_axidma_interrupts_config(DMA_FLAG_ACTIVE, DMA_FLAG_ACTIVE, DMA_FLAG_NACTIVE, DMA_FLAG_NACTIVE);
		user_axidma_interrupts_config(DMA_FLAG_NACTIVE, DMA_FLAG_NACTIVE, DMA_FLAG_ACTIVE, DMA_FLAG_NACTIVE);

		// If the engine is doing transfer, cannot submit
		if(!(XAxiDma_ReadReg(axiDma.TxBdRing.ChanBase, XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK)) {
			if (XAxiDma_Busy(&axiDma,mm2s))return XST_FAILURE;
		}


		if (!axiDma.MicroDmaMode) {
			WordBits = (u32)((axiDma.TxBdRing.DataWidth) - 1);
		}
		else {
			WordBits = XAXIDMA_MICROMODE_MIN_BUF_ALIGN;
		}

		if (((UINTPTR) &address & WordBits) && !axiDma.TxBdRing.HasDRE) {
			return XST_INVALID_PARAM;
		}

		XAxiDma_WriteReg(axiDma.TxBdRing.ChanBase,
						 XAXIDMA_SRCADDR_OFFSET, LOWER_32_BITS((UINTPTR) address));


		XAxiDma_WriteReg(axiDma.TxBdRing.ChanBase,
				XAXIDMA_CR_OFFSET,
				XAxiDma_ReadReg(
				axiDma.TxBdRing.ChanBase,
				XAXIDMA_CR_OFFSET)| XAXIDMA_CR_RUNSTOP_MASK);
		if((option & DMA_PROFILING)){
			//We stop counter 2, this gives us the profiling time
			user_gpio_set_counter(COUNTER_PROFILING, COUNTER_NOPTION);
		}

		if((option & DMA_LATENCY) || (option & DMA_TOTAL)){
			//We start counter 1 in order to measure the latency time
			if(option & DMA_LATENCY) user_gpio_set_counter(COUNTER_LATENCY, COUNTER_ENABLE);
			test_active = 1;
		}



		// Writing to the BTT register starts the transfer

		XAxiDma_WriteReg(axiDma.TxBdRing.ChanBase,
					XAXIDMA_BUFFLEN_OFFSET, nBytes);
	}
	else if(mm2s == DMA_S2MM){
		if ((nBytes < 1)) return XST_INVALID_PARAM;

		if (nBytes > axiDma.RxBdRing[RingIndex].MaxTransferLen) nBytes = axiDma.RxBdRing[RingIndex].MaxTransferLen;

		if (!axiDma.HasS2Mm) return XST_FAILURE;

		user_axidma_interrupts_config(DMA_FLAG_ACTIVE, DMA_FLAG_NACTIVE, DMA_FLAG_ACTIVE, DMA_FLAG_NACTIVE);
		user_axidma_interrupts_config(DMA_FLAG_NACTIVE, DMA_FLAG_ACTIVE, DMA_FLAG_NACTIVE, DMA_FLAG_NACTIVE);


		if(!(XAxiDma_ReadReg(axiDma.RxBdRing[RingIndex].ChanBase,
				XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK)) {
			if (XAxiDma_Busy(&axiDma,mm2s)) {
				return XST_FAILURE;
			}
		}

		if (!axiDma.MicroDmaMode) {
			WordBits =
			 (u32)((axiDma.RxBdRing[RingIndex].DataWidth) - 1);
		}
		else {
			WordBits = XAXIDMA_MICROMODE_MIN_BUF_ALIGN;
		}

		if (((UINTPTR) &address & WordBits)) {
			if (!axiDma.RxBdRing[RingIndex].HasDRE) {
				return XST_INVALID_PARAM;
			}
		}




		XAxiDma_WriteReg(axiDma.RxBdRing[RingIndex].ChanBase,
						 XAXIDMA_DESTADDR_OFFSET, LOWER_32_BITS((UINTPTR) address));

		XAxiDma_WriteReg(axiDma.RxBdRing[RingIndex].ChanBase,
				XAXIDMA_CR_OFFSET,
			XAxiDma_ReadReg(axiDma.RxBdRing[RingIndex].ChanBase,
			XAXIDMA_CR_OFFSET)| XAXIDMA_CR_RUNSTOP_MASK);
		// Writing to the BTT register starts the transfer
		if((option & DMA_PROFILING)){
			//We stop counter 2, this gives us the profiling time
			user_gpio_set_counter(COUNTER_PROFILING, COUNTER_NOPTION);
		}


		if((option & DMA_LATENCY) || (option & DMA_TOTAL)){
			//We start counter 1 in order to measure the latency time
			if(option & DMA_LATENCY) user_gpio_set_counter(COUNTER_LATENCY, COUNTER_ENABLE);
			//This flag is used by the interrupt handlers in order to know if
			//they should stop or start the counters for performing the required tests
			test_active = 1;


		}


		XAxiDma_WriteReg(axiDma.RxBdRing[RingIndex].ChanBase,
					XAXIDMA_BUFFLEN_OFFSET, nBytes);
	}



	return XST_SUCCESS;
}


int user_axidma_interrupts_config(char enable, char mm2s, char s2mm, char scugic){
	if (enable == DMA_FLAG_ACTIVE) {
		/* Enable all interrupts */
		if(mm2s == DMA_FLAG_ACTIVE){
			XAxiDma_IntrEnable(&axiDma, XAXIDMA_IRQ_ALL_MASK,
											XAXIDMA_DMA_TO_DEVICE);
		}
		if (s2mm == DMA_FLAG_ACTIVE){
			XAxiDma_IntrEnable(&axiDma, XAXIDMA_IRQ_ALL_MASK,
											XAXIDMA_DEVICE_TO_DMA);
		}
	}else{
		if(mm2s == DMA_FLAG_ACTIVE){
			XAxiDma_IntrDisable(&axiDma, XAXIDMA_IRQ_ALL_MASK,
											XAXIDMA_DMA_TO_DEVICE);
		}
		if (s2mm == DMA_FLAG_ACTIVE){
			XAxiDma_IntrDisable(&axiDma, XAXIDMA_IRQ_ALL_MASK,
											XAXIDMA_DEVICE_TO_DMA);
		}
	}
	if(scugic == DMA_FLAG_ACTIVE){
		user_interrupts_axidma_config(enable, mm2s, s2mm);
	}
	return XST_SUCCESS;
}

static void handler_axidma_mm2s(void *CallbackRef){

	XAxiDma *dma = (XAxiDma *)CallbackRef;
	int timeOut;
	u32 status;

	//xil_printf("[DMA]: MM2S IRQ\r\n");

	//Read interrupt
	status = XAxiDma_IntrGetIrq(dma, XAXIDMA_DMA_TO_DEVICE);

	//Acknowledge interrupt
	XAxiDma_IntrAckIrq(dma, status, XAXIDMA_DMA_TO_DEVICE);
	if (!(status & XAXIDMA_IRQ_ALL_MASK)){
		user_dma_get_status_reg(TX, &status);
		xil_printf("Status Register MM2S: 0x%x\r\n", status);
		return;
	}

	user_axidma_interrupts_config(DMA_FLAG_NACTIVE, DMA_FLAG_ACTIVE, DMA_FLAG_NACTIVE, DMA_FLAG_NACTIVE);



	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((status & XAXIDMA_IRQ_ERROR_MASK)) {
		xil_printf("Error from DMA in MM2S handler: 0x%x\r\n", status);
		flags.error[DMA_MM2S] = DMA_FLAG_ACTIVE;
		user_dma_get_status_reg(TX, &status);
		xil_printf("Status Register MM2S: 0x%x\r\n", status);

		 //Reset should never fail for transmit channel
		XAxiDma_Reset(dma);

		timeOut = RESET_TIMEOUT_COUNTER;
		while (timeOut) {
			if (XAxiDma_ResetIsDone(dma)) {
				break;
			}
			timeOut -= 1;
		}
		xil_printf("DMA Reset Timeout\r\n");
		return;
	}

	/*
	 * If Completion interrupt is asserted, then set the TxDone flag
	 */
	if ((status & XAXIDMA_IRQ_IOC_MASK)) {
		flags.transfer_done[DMA_MM2S] = DMA_FLAG_ACTIVE;
		if(test_active) {
			user_gpio_set_counter(COUNTER_LATENCY, COUNTER_NOPTION);
			test_active = 0;
		}
		user_dma_get_status_reg(TX, &status);
		xil_printf("Status Register MM2S: 0x%x\r\n", status);
		xil_printf("[DMA]: MM2S IRQ DONE!\r\n");
	}
}

static void handler_axidma_s2mm(void *CallbackRef){

	XAxiDma *dma = (XAxiDma *)CallbackRef;
	int timeOut;
	u32 status;

	//xil_printf("[DMA]: S2MM IRQ\r\n");

	/*
	user_dma_get_status_reg(RX, &status);
	xil_printf("Status Register S2MM: 0x%x\r\n", status);
	user_dma_get_s2mm_length(&status);
	xil_printf("Length Register S2MM: 0x%x\r\n", status);
	*/

	//Read interrupt
	status = XAxiDma_IntrGetIrq(dma, XAXIDMA_DEVICE_TO_DMA);

	//Acknowledge interrupt
	XAxiDma_IntrAckIrq(dma, status, XAXIDMA_DEVICE_TO_DMA);
	if (!(status & XAXIDMA_IRQ_ALL_MASK)){
		xil_printf("Error from DMA in S2MM handler: 0x%x\r\n", status);
		user_dma_get_status_reg(RX, &status);
		xil_printf("Unknown interrupt Status Register S2MM: 0x%x\r\n", status);
		return;
	}

	user_axidma_interrupts_config(DMA_FLAG_NACTIVE, DMA_FLAG_NACTIVE, DMA_FLAG_ACTIVE, DMA_FLAG_NACTIVE);

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((status & XAXIDMA_IRQ_ERROR_MASK)) {
		xil_printf("Error from DMA in S2MM handler: 0x%x\r\n", status);
		user_dma_get_status_reg(RX, &status);
		xil_printf("Status Register S2MM: 0x%x\r\n", status);
		flags.error[DMA_S2MM] = DMA_FLAG_ACTIVE;
		user_dma_get_s2mm_length(&status);
		xil_printf("Length Register S2MM: 0x%x\r\n", status);

		XAxiDma_Reset(dma);

		timeOut = RESET_TIMEOUT_COUNTER;
		while (timeOut) {
			if (XAxiDma_ResetIsDone(dma)) {
				break;
			}
			timeOut -= 1;
		}
		xil_printf("DMA Reset Timeout\r\n");
		return;
	}

	/*
	 * If Completion interrupt is asserted, then set the TxDone flag
	 */
	if ((status & XAXIDMA_IRQ_IOC_MASK)) {
		flags.transfer_done[DMA_S2MM] = DMA_FLAG_ACTIVE;
		if(test_active) {
			user_gpio_set_counter(COUNTER_LATENCY, COUNTER_NOPTION);
			test_active = 0;
		}
		user_dma_get_status_reg(RX, &status);
		xil_printf("Status Register S2MM: 0x%x\r\n", status);
		user_dma_get_s2mm_length(&status);
		xil_printf("Length Register S2MM: 0x%x\r\n", status);
		xil_printf("[DMA]: S2MM IRQ DONE!\r\n");
	}
}



void user_dma_get_status(DmaFlags *flags_usr){
	flags_usr->error[0] = flags.error[0];
	flags_usr->error[1] = flags.error[1];
	flags_usr->transfer_done[0] = flags.transfer_done[0];
	flags_usr->transfer_done[1] = flags.transfer_done[1];
}



void user_dma_get_status_reg(char rx_tx, u32 *status){
	*status = 0;
	UINTPTR chan;

	if(rx_tx == RX){
		chan = axiDma.RxBdRing[0].ChanBase;
	}
	if(rx_tx == TX){
		chan = axiDma.TxBdRing.ChanBase;
	}

	*status = XAxiDma_ReadReg(chan, XAXIDMA_SR_OFFSET);
}

void user_dma_get_s2mm_length(u32 *status){
	*status = (XAxiDma_ReadReg(axiDma.RxBdRing[0].ChanBase, (u32)XAXIDMA_BUFFLEN_OFFSET)) & 0x3FFFFFF;
}

void user_dma_reset(){
	XAxiDma_Reset(&axiDma);
	while(!XAxiDma_ResetIsDone(&axiDma)){}
}
