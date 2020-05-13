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
#include "xgpio.h"

#include "user_gpio.h"
#include "user_interrupts.h"
#include "user_types.h"
#include "user_trafgen.h"


#define DMA_FLAG_ACTIVE 1
#define DMA_FLAG_NACTIVE 0

#define DMA_PROFILING 0x1
#define DMA_LATENCY 0x2
#define DMA_TOTAL 0x4
#define DMA_TOTAL_TG 0x8
#define DMA_NOPTION 0

#define DMA_MM2S XAXIDMA_DMA_TO_DEVICE
#define DMA_S2MM XAXIDMA_DEVICE_TO_DMA
#define DMA_TG	 0x02

#define COUNTER_LATENCY 2
#define COUNTER_PROFILING 1
#define COUNTER_TOTAL COUNTER_LATENCY

//BTT for DMA retransfer function
#define BYTES_RETRANSFER 100*MEG



// Timeout loop counter for reset
#define RESET_TIMEOUT_COUNTER	500

typedef struct dmaFlags{
	char transfer_done[2];
	char error[2];
} DmaFlags;

typedef struct{
	u32 address;
	u32 nBytes;
	u32 initialDmaLength; //Used to ensure bytes from last test are not counted
	char emptyWait;
}DmaTracker;

int user_axidma_init(void);


//Enable or disable DMA interrupts
int user_axidma_interrupts_config(char enable, char mm2s, char s2mm, char scugic);
//DMA transfer function from DDR to PL AXIS interface (FIFO), after calling the function one must wait for the flags.tx_done or flags.rx_done flag to activate.
int user_dma_transfer(char mm2s, u32 address, u32 nBytes_in, char option);
//Updates DmaFlags object
void user_dma_get_status(DmaFlags *flags_usr);
//Reads status register for either RX or TX channels
void user_dma_get_status_reg(char rx_tx, u32 *status);
//Reads the S2MM_LENGTH register, At the completion of the S2MM transfer, the number of actual bytes written on the S2MM AXI4 interface is updated to the S2MM_LENGTH register.
void user_dma_get_s2mm_length(u32 *status);
//Resets DMA and halts code until reset completes
void user_dma_reset();
//Returns testVar variable
void user_dma_get_TestVar(TestVar *testvar_out);
//Returns the maximum transfer length in Bytes for the DMA in a certain direction
void user_dma_get_MaxLen(char mm2s, u32 *length);
//Restarts TestVar shared variable
void resetTestVar(void);
//Resets Tracker variable to 0
void resetTracker(void);

#endif /* SRC_USER_DMA_H_ */
