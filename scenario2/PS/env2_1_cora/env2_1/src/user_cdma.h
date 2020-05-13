/*
 * user_cdma.h
 *
 *  Created on: Mar 28, 2020
 *      Author: gerard
 */

#ifndef SRC_USER_CDMA_H_
#define SRC_USER_CDMA_H_

#include "xparameters.h"
#include "xaxicdma.h"

#include "user_gpio.h"
#include "user_interrupts.h"


#define BRAM_BASE_ADDR XPAR_AXI_BRAM_CTRL_0_S_AXI_BASEADDR
#define BRAM_HIGH_ADDR XPAR_AXI_BRAM_CTRL_0_S_AXI_HIGHADDR

#define DDR_BASE_ADDR 	XPAR_PS7_DDR_0_S_AXI_BASEADDR
#define DDR_HIGH_ADDR 	XPAR_PS7_DDR_0_S_AXI_HIGHADDR

#define BRAM_ADDR_1 BRAM_BASE_ADDR
#define BRAM_ADDR_2 (BRAM_BASE_ADDR + 0x8000)

#define DDR_ADDR_1 0x10000000
#define DDR_ADDR_2 (DDR_ADDR_1 + 0x8000)

#define CDMA_FLAG_ACTIVE 1
#define CDMA_FLAG_NACTIVE 0

#define CDMA_PROFILING 0x1
#define CDMA_LATENCY 0x2
#define CDMA_TOTAL 0x4
#define CDMA_NOPTION 0

#define COUNTER_LATENCY 2
#define COUNTER_PROFILING 1
#define COUNTER_TOTAL COUNTER_LATENCY

// Timeout loop counter for reset
#define RESET_TIMEOUT_COUNTER	500

typedef struct cdmaFlags{
	char transfer_done;
	char error;
} CdmaFlags;

int user_axicdma_init(void);

//Enable or disable CDMA interrupts
int user_axicdma_interrupts_config(char enable, char scugic);

//DMA transfer function for MM2MM
int user_cdma_transfer(u32 address_o, u32 address_d, u32 nBytes, char option);

//Updates CdmaFlags object
void user_cdma_get_status(CdmaFlags *flags_usr);

//Reads status register for either RX or TX channels
void user_cdma_get_status_reg(u32 *status);

//Checks if the CDMA is busy
char user_cdma_isBusy(void);

#endif /* SRC_USER_CDMA_H_ */
