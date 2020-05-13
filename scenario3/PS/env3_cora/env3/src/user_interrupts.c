/*
 * user_interrupts.c
 *
 *  Created on: Mar 7, 2020
 *      Author: gerard
 */

#include "user_interrupts.h"

//Interrupt Controller Variables
static XScuGic interrupt_controller; 	     //Instance of the Interrupt Controller
static XScuGic_Config *gic_config;    //The configuration parameters of the controller

//Initialize Interrupts for the GPIO
void interrupts_init(void){
	//Enable DMA S2MM interrupt
	XScuGic_Enable(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR);

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler)XScuGic_InterruptHandler,
			(void *)&interrupt_controller);

	Xil_ExceptionEnable();
}


int interrupts_dma_init(XAxiDma * axiDmaPtr, Xil_ExceptionHandler handler_dma_mm2s, Xil_ExceptionHandler handler_dma_s2mm, Xil_ExceptionHandler handler_fifo){
	int status;
	u16 device_id = XPAR_SCUGIC_SINGLE_DEVICE_ID;

	//Look for the interrupt controller device configuration
	gic_config = XScuGic_LookupConfig(device_id);
	if(gic_config == NULL){
		return XST_FAILURE;
	}

	//Initialize the fields and vector table for the interrupt controller, interrupts are disabled
	status = XScuGic_CfgInitialize(&interrupt_controller, gic_config, gic_config->CpuBaseAddress);
	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}


	XScuGic_SetPriorityTriggerType(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR,
	0x02, 0x3);
/*
	if(handler_dma_mm2s != NULL){
		XScuGic_SetPriorityTriggerType(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR,
			0xA8, 0x3);
	}
*/
	/*	Connection between the Int_Id of the interrupt source and the associated
		handler that is to run when the interrupt is recognized.
	*/
	status = XScuGic_Connect(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR,
			(Xil_InterruptHandler)handler_dma_s2mm,
				   axiDmaPtr);

	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}



	/*	Connection between the Int_Id of the interrupt source and the associated
		handler that is to run when the interrupt is recognized.
	*/
//	if(handler_dma_mm2s != NULL){
//
//		status = XScuGic_Connect(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR,
//			   	   	   	   	   (Xil_InterruptHandler)handler_dma_mm2s,
//							   axiDmaPtr);
//
//		if (status != XST_SUCCESS) {
//			return XST_FAILURE;
//		}
//		//Enable DMA MM2S interrupt
//		XScuGic_Enable(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR);
//	}

	if(interrupts_fifo_init(get_gpio(INTERRUPT_DEVICE_ID), handler_fifo, GPIO_FIFO_NCHANNELS) != XST_SUCCESS){
		xil_printf("FIFO Interrupt Initialisation failed\n\r");
	}

	interrupts_init();

	return XST_SUCCESS;
}


//Initializes FIFO interrupts
int interrupts_fifo_init(XGpio *gpio, Xil_ExceptionHandler handler_fifo, int nChannels){
	int status;

	XScuGic_SetPriorityTriggerType(&interrupt_controller, FIFO_EMPTY_INT, 0x08, 0x3);

	/*
	 * Connect the interrupt handler that will be called when an
	 * interrupt occurs for the device.
	 */
	status = XScuGic_Connect(&interrupt_controller, FIFO_EMPTY_INT, (Xil_ExceptionHandler)handler_fifo, gpio);
	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/* Enable the interrupt for the GPIO device.*/
	XScuGic_Enable(&interrupt_controller, FIFO_EMPTY_INT);

	if(nChannels < 2){
		user_gpio_interrupts_enable(GPIO_DISABLE, GPIO_ENABLE, GPIO_DISABLE);
	}else{
		user_gpio_interrupts_enable(GPIO_DISABLE, GPIO_ENABLE, GPIO_ENABLE);
	}
	return XST_SUCCESS;

}



int user_interrupts_axidma_config(char enable, char mm2s, char s2mm){
	if (enable == DMA_FLAG_ACTIVE) {
		/* Enable all interrupts */
		if(mm2s == DMA_FLAG_ACTIVE){
			//XScuGic_Enable(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR);
		}
		if (s2mm == DMA_FLAG_ACTIVE){
			XScuGic_Enable(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR);
		}
	}else{
		if(mm2s == DMA_FLAG_ACTIVE){
			//XScuGic_Disable(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR);
		}
		if (s2mm == DMA_FLAG_ACTIVE){
			XScuGic_Disable(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR);
		}
	}
	return XST_SUCCESS;
}

