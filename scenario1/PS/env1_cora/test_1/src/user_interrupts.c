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

int interrupts_dma_init(XAxiDma * axiDmaPtr, Xil_ExceptionHandler handler_dma_mm2s, Xil_ExceptionHandler handler_dma_s2mm){
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
	0xA0, 0x3);


	XScuGic_SetPriorityTriggerType(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR,
	0xA8, 0x3);

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
	status = XScuGic_Connect(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR,
				   (Xil_InterruptHandler)handler_dma_mm2s,
				   axiDmaPtr);

	if (status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	//Enable DMA S2MM interrupt
	XScuGic_Enable(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR);

	//Enable DMA S2MM interrupt
	XScuGic_Enable(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR);

	Xil_ExceptionInit();
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			(Xil_ExceptionHandler)XScuGic_InterruptHandler,
			(void *)&interrupt_controller);

	Xil_ExceptionEnable();


	return XST_SUCCESS;
}

int user_interrupts_axidma_config(char enable, char mm2s, char s2mm){
	if (enable == DMA_FLAG_ACTIVE) {
		/* Enable all interrupts */
		if(mm2s == DMA_FLAG_ACTIVE){
			XScuGic_Enable(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR);
		}
		if (s2mm == DMA_FLAG_ACTIVE){
			XScuGic_Enable(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR);
		}
	}else{
		if(mm2s == DMA_FLAG_ACTIVE){
			XScuGic_Disable(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_MM2S_INTROUT_INTR);
		}
		if (s2mm == DMA_FLAG_ACTIVE){
			XScuGic_Disable(&interrupt_controller, XPAR_FABRIC_AXI_DMA_0_S2MM_INTROUT_INTR);
		}
	}
	return XST_SUCCESS;
}

