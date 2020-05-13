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
#include "xaxicdma.h"
#include "user_cdma.h"


//Initialize interrupts
int interrupts_cdma_init(XAxiCdma * axiCdmaPtr);

//Enable or disable dma interrupts
int user_interrupts_axicdma_config(char enable);

#endif /* SRC_USER_INTERRUPTS_H_ */
