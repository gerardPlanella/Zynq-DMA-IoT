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
volatile TestVar testvar;
volatile DmaTracker tracker[2];



//Handler for the DMA s2mm interrupt
static void handler_axidma_s2mm(void *CallbackRef);

//Handler for the DMA mm2s interrupt
static void handler_axidma_mm2s(void *CallbackRef);

//Handler for the fifo empty signal status change
static void handler_fifo(void *CallbackRef);



int user_axidma_init(void){
	int status = 0;
	XAxiDma_Config *config;

	flags.transfer_done[0] = flags.transfer_done[1] = flags.error[0] = flags.error[1] = DMA_FLAG_NACTIVE;
	resetTracker();

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

	status = interrupts_dma_init(&axiDma, ((Xil_ExceptionHandler)handler_axidma_mm2s), ((Xil_ExceptionHandler)handler_axidma_s2mm), ((Xil_ExceptionHandler)handler_fifo));
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

int user_dma_transfer(char mm2s, u32 address, u32 nBytes_in, char option){
	u32 WordBits;
	int RingIndex = 0;
	u32 count;
	u32 nBytes = 0;


	flags.transfer_done[0] = flags.transfer_done[1] = 0;
	flags.error[0] = flags.error[1] = 0;


	//Restarts TestVar shared variable
	resetTestVar();

	if(option & DMA_TOTAL_TG){
		resetTracker();
		//We will configure the DMA to transfer the maximum amount of bytes it can move
		user_dma_get_MaxLen(mm2s, &nBytes);
		tracker[DMA_S2MM].address = address;
		tracker[DMA_S2MM].nBytes = nBytes_in;
		tracker[DMA_S2MM].emptyWait = 0;
		tracker[DMA_S2MM].initialDmaLength = 0;
		testvar.n_configs_dma = 1;
		nBytes -= 1;
	}else{
		nBytes = nBytes_in;
	}

	// If Scatter Gather is included then, cannot submit
	if (XAxiDma_HasSg(&axiDma)) return XST_FAILURE;

	//xil_printf("[AXI DMA] No SG\r\n");

	if(mm2s == DMA_MM2S){
		xil_printf("[AXI DMA] MM2S Request\r\n");

		if ((nBytes < 1) || (nBytes > axiDma.TxBdRing.MaxTransferLen)) return XST_INVALID_PARAM;

		if (!axiDma.HasMm2S) return XST_FAILURE;

		tracker[DMA_MM2S].initialDmaLength = 0;

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
		if((option & DMA_TOTAL_TG)){
			user_gpio_set_counter(COUNTER_PROFILING, COUNTER_NOPTION);
			if(user_gpio_get_count(&count, COUNTER_PROFILING) != XST_SUCCESS) return XST_FAILURE;
			testvar.t_profiling = count;
			testvar.test_running = 1;
			//user_dma_get_s2mm_length(&(tracker[DMA_MM2S].initialDmaLength));
		}

		if((option & DMA_LATENCY) || (option & DMA_TOTAL)){
			test_active = DMA_FLAG_ACTIVE;
			//We start counter 1 in order to measure the latency time
			if(option & DMA_LATENCY) user_gpio_set_counter(COUNTER_LATENCY, COUNTER_ENABLE);
		}



		// Writing to the BTT register starts the transfer

		XAxiDma_WriteReg(axiDma.TxBdRing.ChanBase,
					XAXIDMA_BUFFLEN_OFFSET, nBytes);
	}
	else if(mm2s == DMA_S2MM){
		//xil_printf("[AXI DMA] S2MM Request\r\n");
		if ((nBytes < 1) || (nBytes > axiDma.RxBdRing[RingIndex].MaxTransferLen)) return XST_INVALID_PARAM;
		//xil_printf("[AXI DMA] Correct Bytes\r\n");
		if (!axiDma.HasS2Mm) return XST_FAILURE;
		//xil_printf("[AXI DMA] Has S2MM\r\n");

		tracker[DMA_S2MM].initialDmaLength = 0;

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

		if((option & DMA_PROFILING)){
			//We stop counter 2, this gives us the profiling time
			user_gpio_set_counter(COUNTER_PROFILING, COUNTER_NOPTION);
		}
		if((option & DMA_TOTAL_TG)){
			user_gpio_set_counter(COUNTER_PROFILING, COUNTER_NOPTION);
			resetTestVar();
			if(user_gpio_get_count(&count, COUNTER_PROFILING) != XST_SUCCESS) return XST_FAILURE;
			testvar.t_profiling = count;
			testvar.test_running = 1;
			//user_dma_get_s2mm_length(&(tracker[DMA_S2MM].initialDmaLength));
		}

		if((option & DMA_LATENCY) || (option & DMA_TOTAL)){
			test_active = DMA_FLAG_ACTIVE;
			//We start counter 1 in order to measure the latency time
			if(option & DMA_LATENCY) user_gpio_set_counter(COUNTER_LATENCY, COUNTER_ENABLE);
		}

		// Writing to the BTT register starts the transfer
		XAxiDma_WriteReg(axiDma.RxBdRing[RingIndex].ChanBase,
					XAXIDMA_BUFFLEN_OFFSET, nBytes);
	}



	return XST_SUCCESS;
}

int user_dma_retransfer(char mm2s){
	u32 count;
	int RingIndex = 0;
	u32 nBytes = 0;

	//xil_printf("[DMA RETRANSFER] Start\r\n");

	//We will configure the DMA to transfer the maximum amount of bytes it can move
	user_dma_get_MaxLen(mm2s, &nBytes);

	if(mm2s == DMA_S2MM){
		if(!(XAxiDma_ReadReg(axiDma.RxBdRing[RingIndex].ChanBase,
				XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK)) {
			if (XAxiDma_Busy(&axiDma,mm2s)) {
				return XST_FAILURE;
			}
		}

		XAxiDma_WriteReg(axiDma.RxBdRing[RingIndex].ChanBase,
						 XAXIDMA_DESTADDR_OFFSET, LOWER_32_BITS((UINTPTR) tracker[DMA_S2MM].address));

		XAxiDma_WriteReg(axiDma.RxBdRing[RingIndex].ChanBase,
				XAXIDMA_CR_OFFSET,
			XAxiDma_ReadReg(axiDma.RxBdRing[RingIndex].ChanBase,
			XAXIDMA_CR_OFFSET)| XAXIDMA_CR_RUNSTOP_MASK);
	}else{
		// If the engine is doing transfer, cannot submit
		if(!(XAxiDma_ReadReg(axiDma.TxBdRing.ChanBase, XAXIDMA_SR_OFFSET) & XAXIDMA_HALTED_MASK)) {
			if (XAxiDma_Busy(&axiDma,mm2s))return XST_FAILURE;
		}

		XAxiDma_WriteReg(axiDma.TxBdRing.ChanBase,
								 XAXIDMA_SRCADDR_OFFSET, LOWER_32_BITS((UINTPTR) tracker[DMA_MM2S].address));


		XAxiDma_WriteReg(axiDma.TxBdRing.ChanBase,
				XAXIDMA_CR_OFFSET,
				XAxiDma_ReadReg(
				axiDma.TxBdRing.ChanBase,
				XAXIDMA_CR_OFFSET)| XAXIDMA_CR_RUNSTOP_MASK);

	}


	user_gpio_set_counter(COUNTER_PROFILING, COUNTER_NOPTION);
	if(user_gpio_get_count(&count, COUNTER_PROFILING) != XST_SUCCESS) return XST_FAILURE;
	testvar.test_running = DMA_FLAG_ACTIVE;
	testvar.t_profiling += count;
	testvar.n_configs_dma++;
	//xil_printf("[DMA RETRANSFER]Transfer Begin\r\n");
	// Writing to the BTT register to start the transfer
	if(mm2s == DMA_S2MM){
		XAxiDma_IntrEnable(&axiDma, XAXIDMA_IRQ_ALL_MASK,XAXIDMA_DEVICE_TO_DMA);
		XAxiDma_WriteReg(axiDma.RxBdRing[RingIndex].ChanBase,
								XAXIDMA_BUFFLEN_OFFSET, nBytes);
	}else{
		XAxiDma_IntrEnable(&axiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
		XAxiDma_WriteReg(axiDma.TxBdRing.ChanBase,
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


static void handler_axidma_s2mm(void *CallbackRef){

	XAxiDma *dma = (XAxiDma *)CallbackRef;

	int timeOut;
	u32 status;
	u32 maxLen;



	if(testvar.test_running){
		//Restart the counter for calculating total profiling time
		user_gpio_set_counter(COUNTER_PROFILING, COUNTER_SRESET);
		user_gpio_set_counter(COUNTER_PROFILING, COUNTER_ENABLE);
	}

	//Read interrupt
	status = XAxiDma_IntrGetIrq(dma, XAXIDMA_DEVICE_TO_DMA);

	//Acknowledge interrupt
	XAxiDma_IntrAckIrq(dma, status, XAXIDMA_DEVICE_TO_DMA);
	if (!(status & XAXIDMA_IRQ_ALL_MASK)){
		xil_printf("[DMA S2MM IRQ] Error from DMA in S2MM handler: 0x%x\r\n", status);
		user_dma_get_status_reg(DMA_S2MM, &status);
		xil_printf("[DMA S2MM IRQ] Unknown interrupt Status Register S2MM: 0x%x\r\n", status);
		return;
	}

	user_axidma_interrupts_config(DMA_FLAG_NACTIVE, DMA_FLAG_NACTIVE, DMA_FLAG_ACTIVE, DMA_FLAG_NACTIVE);

	/*
	 * If error interrupt is asserted, raise error flag, reset the
	 * hardware to recover from the error, and return with no further
	 * processing.
	 */
	if ((status & XAXIDMA_IRQ_ERROR_MASK)) {
		xil_printf("[DMA S2MM IRQ] Error from DMA in S2MM handler: 0x%x\r\n", status);
		user_dma_get_status_reg(DMA_S2MM, &status);
		xil_printf("[DMA S2MM IRQ] Status Register S2MM: 0x%x\r\n", status);
		flags.error[DMA_S2MM] = DMA_FLAG_ACTIVE;
		user_dma_get_s2mm_length(&status);
		xil_printf("[DMA S2MM IRQ] Length Register S2MM: 0x%x\r\n", status);

		XAxiDma_Reset(dma);

		timeOut = RESET_TIMEOUT_COUNTER;
		while (timeOut) {
			if (XAxiDma_ResetIsDone(dma)) {
				break;
			}
			timeOut -= 1;
		}
		xil_printf("[DMA S2MM IRQ] DMA Reset Timeout\r\n");
		if(testvar.test_running){
			testvar.test_running = 0;
		}
		return;
	}

	/*
	 * If Completion interrupt is asserted, then set the TxDone flag
	 */
	if ((status & XAXIDMA_IRQ_IOC_MASK)) {
		flags.transfer_done[DMA_S2MM] = DMA_FLAG_ACTIVE;
		if(test_active) {
			//We stop the counter to measure the latency
			user_gpio_set_counter(COUNTER_LATENCY, COUNTER_NOPTION);
			test_active = 0;
		}
		if(testvar.test_running){
			user_dma_get_s2mm_length(&status);
			//Increase the byte count for the DMA transfer
			testvar.total_length_dma += status;
			user_dma_get_MaxLen(DMA_S2MM, &maxLen);
			if(status > maxLen){
				xil_printf("DMA Length %u exceeds maximum permitted %u, DMA will Halt\n\r", status, maxLen);
			}
			if(user_gpio_fifo_is_ovf()) testvar.fifo_ovf_cnt++;
			tracker[DMA_S2MM].address += status;
			if((!user_trafgen_isDone() && tracker[DMA_S2MM].emptyWait == 0)/* || (!user_gpio_fifo_isProgEmpty(1))*/){
				//Reconfigure a transfer
				if(user_gpio_fifo_isProgEmpty(1)){
					tracker[DMA_S2MM].emptyWait = 1;
					//Enable GPIO Interrupts
					user_gpio_interrupts_enable(GPIO_ENABLE, GPIO_ENABLE, GPIO_DISABLE);
				}else{
					user_dma_retransfer(DMA_S2MM);
				}
			}else if(tracker[DMA_S2MM].emptyWait == 0 || (user_trafgen_isDone() && tracker[DMA_S2MM].emptyWait == 1)){
				//We stop the total time counter
				user_gpio_set_counter(COUNTER_TOTAL, COUNTER_NOPTION);
				xil_printf("[DMA S2MM IRQ]: Stop Reconfiguration\r\n");
				testvar.test_running = 0;
				resetTracker();

			}
		}

	}
}

//Handler for the FIFO empty signal status change, CHANNEL 1 = RX, CHANNEL 2 = TX
static void handler_fifo(void *CallbackRef){
	XGpio *gpioPtr = (XGpio *)CallbackRef;
	char interrupt[2] = {0, 0};

	//xil_printf("FIFO_IRQ\r\n");
	if(tracker[DMA_S2MM].emptyWait == 1 && !user_gpio_fifo_isProgEmpty(1)){
		user_gpio_set_counter(COUNTER_PROFILING, COUNTER_SRESET);
		user_gpio_set_counter(COUNTER_PROFILING, COUNTER_ENABLE);
		tracker[DMA_S2MM].emptyWait = 0;
		interrupt[0] = 1;
		user_gpio_interrupts_enable(GPIO_DISABLE, GPIO_ENABLE, GPIO_DISABLE);

	}

	if(tracker[DMA_MM2S].emptyWait == 1 && !user_gpio_fifo_isProgEmpty(2)){
		tracker[DMA_MM2S].emptyWait = 0;
		interrupt[1] = 1;
		user_gpio_interrupts_enable(GPIO_DISABLE, GPIO_DISABLE, GPIO_ENABLE);

	}

	/* Clear the Interrupt */
	XGpio_InterruptClear(gpioPtr, XGPIO_IR_MASK);

	if(interrupt[0] == 1){
		user_dma_retransfer(DMA_S2MM);
	}

	if(interrupt[1] == 1){
		user_dma_retransfer(DMA_MM2S);
	}


}


void user_dma_get_status(DmaFlags *flags_usr){
	flags_usr->error[0] = flags.error[0];
	flags_usr->error[1] = flags.error[1];
	flags_usr->transfer_done[0] = flags.transfer_done[0];
	flags_usr->transfer_done[1] = flags.transfer_done[1];
}



void user_dma_get_status_reg(char mm2s, u32 *status){
	*status = 0;
	UINTPTR chan;

	if(mm2s == DMA_S2MM){
		chan = axiDma.RxBdRing[0].ChanBase;
	}
	if(mm2s == DMA_MM2S){
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
		user_dma_get_status_reg(DMA_MM2S, &status);
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
		user_dma_get_status_reg(DMA_MM2S, &status);
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
		user_dma_get_status_reg(DMA_MM2S, &status);
		xil_printf("Status Register MM2S: 0x%x\r\n", status);
		xil_printf("[DMA]: MM2S IRQ DONE!\r\n");
	}
}

//Returns testVar variable
void user_dma_get_TestVar(TestVar *testvar_out){
	testvar_out->fifo_rd = testvar.fifo_rd;
	testvar_out->fifo_wr = testvar.fifo_wr;
	testvar_out->n_configs_dma = testvar.n_configs_dma;
	testvar_out->t_profiling = testvar.t_profiling;
	testvar_out->t_total = testvar.t_total;
	testvar_out->test_running = testvar.test_running;
	testvar_out->total_length_dma = testvar.total_length_dma;
	testvar_out->fifo_ovf_cnt = testvar.fifo_ovf_cnt;
}

//Restarts TestVar shared variable
void resetTestVar(void){
	testvar.test_running = 0;
	testvar.fifo_rd = testvar.fifo_wr = 0;
	testvar.n_configs_dma = 0;
	testvar.t_profiling = 0;
	testvar.t_total = 0;
	testvar.total_length_dma = 0;
	testvar.fifo_ovf_cnt = 0;
}

void resetTracker(void){
	tracker[DMA_S2MM].address = tracker[DMA_S2MM].emptyWait = tracker[DMA_S2MM].nBytes = 0;
	tracker[DMA_MM2S].address = tracker[DMA_MM2S].emptyWait = tracker[DMA_MM2S].nBytes = 0;
}

void user_dma_get_MaxLen(char mm2s, u32 *length){
	int RingIndex = 0;

	if(mm2s == DMA_S2MM || mm2s == DMA_TG){
		*length = axiDma.RxBdRing[RingIndex].MaxTransferLen;
	}else if(mm2s == DMA_MM2S){
		*length = axiDma.TxBdRing.MaxTransferLen;
	}else{
		*length = 0;
	}
}

