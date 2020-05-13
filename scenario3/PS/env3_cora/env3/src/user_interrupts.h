/*
 * user_interrupts.h
 *
 *  Created on: Mar 7, 2020
 *      Author: gerard
 */

#ifndef SRC_USER_INTERRUPTS_H_
#define SRC_USER_INTERRUPTS_H_

#include <stdio.h>

#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"
#include "xil_types.h"
#include "xaxidma.h"
#include "user_dma.h"
#include "user_gpio.h"
#include "xparameters.h"
#include "xgpio.h"


//Initialize DMA interrupts
int interrupts_dma_init(XAxiDma * axiDmaPtr, Xil_ExceptionHandler handler_dma_mm2s, Xil_ExceptionHandler handler_dma_s2mm, Xil_ExceptionHandler handler_fifo);
//Initializes GPIO interrupts, must be called after interrupts_dma_init
int interrupts_fifo_init(XGpio *gpio, Xil_ExceptionHandler handler_dma_mm2s, int nChannels);
//Initializes interrupt Exceptions
void interrupts_init(void);
//Enable or disable dma interrupts
int user_interrupts_axidma_config(char enable, char mm2s, char s2mm);

#endif /* SRC_USER_INTERRUPTS_H_ */
