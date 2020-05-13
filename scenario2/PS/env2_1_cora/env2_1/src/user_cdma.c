/*
 * user_cdma.c
 *
 *  Created on: Mar 28, 2020
 *      Author: gerard
 */
#include "user_cdma.h"

static XAxiCdma axiCdma;
volatile CdmaFlags flags;
volatile char test_active;

//Handler for the DMA mm2s interrupt
static void handler_axicdma(void *CallBackRef, u32 IrqMask, int *IgnorePtr);

int user_axicdma_init(void){
	int status = 0;
	XAxiCdma_Config *config;


	flags.transfer_done = flags.error = test_active = CDMA_FLAG_NACTIVE;

	config = XAxiCdma_LookupConfig(XPAR_AXI_CDMA_0_DEVICE_ID);
	if (!config) {
		xil_printf("No configuration found for %d\r\n", XPAR_AXI_CDMA_0_DEVICE_ID);
		return XST_FAILURE;
	}

	/* Initialize CDMA engine */
	status = XAxiCdma_CfgInitialize(&axiCdma, config, config->BaseAddress);
	if (status != XST_SUCCESS) {
		xil_printf("Initialization failed %d\r\n", status);
		return XST_FAILURE;
	}

	//Reset to ensure CDMA begins halted
	XAxiCdma_Reset(&axiCdma);
	while(!XAxiCdma_ResetIsDone(&axiCdma)){}

	status = user_axicdma_interrupts_config(CDMA_FLAG_NACTIVE, CDMA_FLAG_NACTIVE);
	if (status != XST_SUCCESS) {
		xil_printf("Failed to disable CDMA interrupts\r\n");
		return XST_FAILURE;
	}

	status = interrupts_cdma_init(&axiCdma);
	if (status != XST_SUCCESS) {
		xil_printf("[CDMA Init]: Interrupt setup failed %d\r\n", status);
		return XST_FAILURE;
	}


	user_axicdma_interrupts_config(CDMA_FLAG_ACTIVE, CDMA_FLAG_NACTIVE);
	return XST_SUCCESS;


}
/*****************************************************************************/
/*
 * Check whether the hardware is in simple mode
 *
 * @param	InstancePtr is the driver instance we are working on
 *
 * @return
 *		- 1 if the hardware is in simple mode
 *		- 0 if the hardware is in SG mode
 *
 * @note	None.
 *
 *****************************************************************************/
int isSimpleMode(XAxiCdma *InstancePtr)
{
	return ((XAxiCdma_ReadReg(InstancePtr->BaseAddr, XAXICDMA_CR_OFFSET) &
		XAXICDMA_CR_SGMODE_MASK) ? 0 : 1);
}

/*****************************************************************************/
/*
 * Change the hardware mode
 *
 * If to switch to SG mode, check whether needs to setup the current BD
 * pointer register.
 *
 * @param	InstancePtr is the driver instance we are working on
 * @param	Mode is the mode to switch to.
 *
 * @return
 *		- XST_SUCCESS if mode switch is successful
 *		- XST_DEVICE_BUSY if the engine is busy, so cannot switch mode
 *		- XST_INVALID_PARAM if pass in invalid mode value
 *		- XST_FAILURE if:Hardware is simple mode only build
 *		Mode switch failed
 *
 * @note	None.
 *
 *****************************************************************************/
int switchMode(XAxiCdma *InstancePtr, int Mode)
{

	if (Mode == XAXICDMA_SIMPLE_MODE) {

		if (isSimpleMode(InstancePtr)) {
			return XST_SUCCESS;
		}

		if (XAxiCdma_IsBusy(InstancePtr)) {
			xdbg_printf(XDBG_DEBUG_ERROR,
			    "SwitchMode: engine is busy\r\n");
			return XST_DEVICE_BUSY;
		}

		/* Keep the CDESC so that CDESC will be
		 * reloaded when switch to SG mode again
		 *
		 * We know CDESC is valid because the hardware can only
		 * be in SG mode if a SG transfer has been submitted.
		 */
		InstancePtr->BdaRestart = XAxiCdma_BdRingNext(InstancePtr,
		    XAxiCdma_BdRingGetCurrBd(InstancePtr));

		/* Update the CR register to switch to simple mode
		 */
		XAxiCdma_WriteReg(InstancePtr->BaseAddr, XAXICDMA_CR_OFFSET,
		  (XAxiCdma_ReadReg(InstancePtr->BaseAddr, XAXICDMA_CR_OFFSET)
		  & ~XAXICDMA_CR_SGMODE_MASK));

		/* Hardware mode switch is quick, should succeed right away
		 */
		if (isSimpleMode(InstancePtr)) {

			return XST_SUCCESS;
		}
		else {
			return XST_FAILURE;
		}
	}
	else if (Mode == XAXICDMA_SG_MODE) {

		if (!isSimpleMode(InstancePtr)) {
			return XST_SUCCESS;
		}

		if (InstancePtr->SimpleOnlyBuild) {
			xdbg_printf(XDBG_DEBUG_ERROR,
			    "SwitchMode: hardware simple mode only\r\n");
			return XST_FAILURE;
		}

		if (XAxiCdma_IsBusy(InstancePtr)) {
			xdbg_printf(XDBG_DEBUG_ERROR,
			    "SwitchMode: engine is busy\r\n");
			return XST_DEVICE_BUSY;
		}

		/* Update the CR register to switch to SG mode
		 */
		XAxiCdma_WriteReg(InstancePtr->BaseAddr, XAXICDMA_CR_OFFSET,
		  (XAxiCdma_ReadReg(InstancePtr->BaseAddr, XAXICDMA_CR_OFFSET)
		  | XAXICDMA_CR_SGMODE_MASK));

		/* Hardware mode switch is quick, should succeed right away
		 */
		if (!isSimpleMode(InstancePtr)) {

			/* Update the CDESC register, because the hardware is
			 * to start from the CDESC
			 */
			XAxiCdma_BdSetCurBdPtr(InstancePtr,
				(UINTPTR)InstancePtr->BdaRestart);

			return XST_SUCCESS;
		}
		else {
			return XST_FAILURE;
		}
	}
	else {	/* Invalid mode */
		return XST_INVALID_PARAM;
	}
}


//DMA transfer function for MM2MM
int user_cdma_transfer(u32 address_o, u32 address_d, u32 nBytes, char option){
	u32 wordBytes;

	if ((nBytes < 1) || (nBytes > XAXICDMA_MAX_TRANSFER_LEN)) {
		return XST_INVALID_PARAM;
	}

	wordBytes = (u32)(axiCdma.WordLength - 1);

	if ((address_o & wordBytes) || (address_d & wordBytes)) {
		if (!axiCdma.HasDRE) {
			return XST_INVALID_PARAM;
		}
	}

	//Check if the CDMA is busy
	if (XAxiCdma_IsBusy(&axiCdma)) {
		return XST_FAILURE;
	}

	//Check if the CDMA is halted
	if (axiCdma.SimpleNotDone) {
		return XST_FAILURE;
	}

	if (!isSimpleMode(&axiCdma)) {
		if (switchMode(&axiCdma, XAXICDMA_SIMPLE_MODE) != XST_SUCCESS) {
			return XST_FAILURE;
		}
	}

	/* Setup the flag so that others will not step on us
	 *
	 * This flag is only set if callback function is used and if the
	 * system is in interrupt mode; otherwise, when the hardware is done
	 * with the transfer, the driver is done with the transfer
	 */
	if (((XAxiCdma_IntrGetEnabled(&axiCdma) & XAXICDMA_XR_IRQ_SIMPLE_ALL_MASK)) != 0x0) {
		axiCdma.SimpleNotDone = 1;
	}

	axiCdma.SimpleCallBackFn = (XAxiCdma_CallBackFn)handler_axicdma;
	axiCdma.SimpleCallBackRef = ((void *) &axiCdma);

	XAxiCdma_WriteReg(axiCdma.BaseAddr, XAXICDMA_SRCADDR_OFFSET, LOWER_32_BITS(address_o));

	XAxiCdma_WriteReg(axiCdma.BaseAddr, XAXICDMA_DSTADDR_OFFSET, LOWER_32_BITS(address_d));

	if((option & CDMA_PROFILING)){
		//We stop counter 2, this gives us the profiling time
		user_gpio_set_counter(COUNTER_PROFILING, COUNTER_NOPTION);
	}


	if((option & CDMA_LATENCY) || (option & CDMA_TOTAL)){
		//We start counter 1 in order to measure the latency time
		if(option & CDMA_LATENCY)user_gpio_set_counter(COUNTER_LATENCY, COUNTER_ENABLE);
		//This flag is used by the interrupt handlers in order to know if
		//they should stop or start the counters for performing the required tests
		test_active = 1;
	}

	/* Writing to the BTT register starts the transfer
	 */
	XAxiCdma_WriteReg(axiCdma.BaseAddr, XAXICDMA_BTT_OFFSET, nBytes);
	return XST_SUCCESS;
}

static void handler_axicdma(void *CallBackRef, u32 IrqMask, int *IgnorePtr){
	XAxiCdma *cdma = (XAxiCdma *)CallBackRef;
	int timeOut;
	u32 status;


	//xil_printf("[CDMA] IRQ\r\n");

	if (IrqMask & XAXICDMA_XR_IRQ_ERROR_MASK) {
		flags.error = CDMA_FLAG_ACTIVE;
		//Reset should never fail for transmit channel
		XAxiCdma_Reset(cdma);

		timeOut = RESET_TIMEOUT_COUNTER;
		while (timeOut) {
			if (XAxiCdma_ResetIsDone(cdma)) {
				break;
			}
			timeOut -= 1;
		}
		xil_printf("CDMA Reset Timeout\r\n");
		return;
	}

	if (IrqMask & XAXICDMA_XR_IRQ_IOC_MASK) {
		flags.transfer_done = CDMA_FLAG_ACTIVE;
		if(test_active) {
			user_gpio_set_counter(COUNTER_LATENCY, COUNTER_NOPTION);
			test_active = 0;
		}
		user_cdma_get_status_reg(&status);
		xil_printf("CDMA Status Register: 0x%x\r\n", status);
		xil_printf("[CDMA]: IRQ DONE\r\n");
	}
}

//Enable or disable CDMA interrupts
int user_axicdma_interrupts_config(char enable,  char scugic){
	if (enable == CDMA_FLAG_ACTIVE) {
		XAxiCdma_IntrEnable(&axiCdma, XAXICDMA_XR_IRQ_ALL_MASK);
	}else{
		XAxiCdma_IntrDisable(&axiCdma, XAXICDMA_XR_IRQ_ALL_MASK);
	}
	if(scugic == CDMA_FLAG_ACTIVE){
		user_interrupts_axicdma_config(enable);
	}
	return XST_SUCCESS;
}

//Updates CdmaFlags object
void user_cdma_get_status(CdmaFlags *flags_usr){
	flags_usr->error = flags.error;
	flags_usr->transfer_done = flags.transfer_done;
}

//Reads status register for either RX or TX channels
void user_cdma_get_status_reg(u32 *status){
	*status = XAxiCdma_ReadReg(axiCdma.BaseAddr, XAXICDMA_SR_OFFSET);
}

char user_cdma_isBusy(void){
	if (XAxiCdma_IsBusy(&axiCdma)) {
		return CDMA_FLAG_ACTIVE;
	}
	return CDMA_FLAG_NACTIVE;
}
