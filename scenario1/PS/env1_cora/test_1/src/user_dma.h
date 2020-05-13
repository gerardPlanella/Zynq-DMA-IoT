/*
 * user_dma.h
 *
 *  Created on: Mar 6, 2020
 *      Author: Gerard Planella
 */

#ifndef SRC_USER_DMA_H_
#define SRC_USER_DMA_H_

#include "xaxidma.h"
#include "xparameters.h"
#include "xdebug.h"

#include "user_gpio.h"
#include "user_interrupts.h"

#define DMA_FLAG_ACTIVE 1
#define DMA_FLAG_NACTIVE 0

#define DMA_PROFILING 0x1
#define DMA_LATENCY 0x2
#define DMA_TOTAL 0x4
#define DMA_NOPTION 0

#define DMA_MM2S XAXIDMA_DMA_TO_DEVICE
#define DMA_S2MM XAXIDMA_DEVICE_TO_DMA

#define COUNTER_LATENCY 2
#define COUNTER_PROFILING 1
#define COUNTER_TOTAL COUNTER_LATENCY

#define DDR_BASE_ADDR 	XPAR_PS7_DDR_0_S_AXI_BASEADDR
#define DDR_HIGH_ADDR 	XPAR_PS7_DDR_0_S_AXI_HIGHADDR
#define TEST_RX_ADDR	0x10000000
#define TEST_TX_ADDR 	TEST_RX_ADDR



#define TX 1
#define RX 0


// Timeout loop counter for reset
#define RESET_TIMEOUT_COUNTER	500

typedef struct dmaFlags{
	char transfer_done[2];
	char error[2];
} DmaFlags;

int user_axidma_init(void);


//Enable or disable DMA interrupts
int user_axidma_interrupts_config(char enable, char mm2s, char s2mm, char scugic);

//DMA transfer function from DDR to PL AXIS interface (FIFO), after calling the function one must wait for the flags.tx_done or flags.rx_done flag to activate.
int user_dma_transfer(char mm2s, u32 address, u32 nBytes, char option);

//Updates DmaFlags object
void user_dma_get_status(DmaFlags *flags_usr);

//Reads status register for either RX or TX channels
void user_dma_get_status_reg(char rx_tx, u32 *status);

//Reads the S2MM_LENGTH register, At the completion of the S2MM transfer, the number of actual bytes written on the S2MM AXI4 interface is updated to the S2MM_LENGTH register.
void user_dma_get_s2mm_length(u32 *status);
//Resets DMA and halts code until reset completes
void user_dma_reset();


#endif /* SRC_USER_DMA_H_ */
