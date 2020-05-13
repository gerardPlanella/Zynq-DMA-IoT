/*
 * user_tests.h
 *
 *  Created on: Mar 8, 2020
 *      Author: Gerard Planella
 */

#ifndef SRC_USER_TESTS_H_
#define SRC_USER_TESTS_H_

#include <stdio.h>

#include "xparameters.h"
#include "xdebug.h"
#include "xil_types.h"


/*---- User Libraries ---- */
#include "user_gpio.h"
#include "user_cdma.h"
#include "user_interrupts.h"

typedef struct{
	u32 	t_profiling; //Time that the CPU interacts with the AXI DMA
	u32 	t_latency;	// Time taken for a word transfer, from the moment the transfer is started by the DMA, to when it is received at the DDR
	u32		t_total;	// Total time taken for a transaction, from its request to its completion
	u32 	drop_rate;	// Amount of bytes that failed to transfer as a factor of the total amount of bytes transferred (BER)
	u32 	throughput[2]; // Amount of Mega bytes per second transferred
	u32 	bytes_transferred; //Total amount of bytes intended to transfer, includes failed ones
}Result;


#define WORDS_BRAM(X) 	(X >> 2)
#define WORDS_DDR(X) 	(X >> 1)
#define BYTES_DDR(X)  	((X << 1) & (~0x01))
#define BYTES_BRAM(X) 	((X << 2) & (~0x03))

#define NEMPTY 0
#define EMPTY 1

#define RESULTS_VISUALIZE  1
#define RESULTS_NVISUALIZE 0

#define READ_ERROR 0

#define TESTS 10 //Total number of tests for averaging
#define SUB_TESTS 4 // DDR to DDR, BRAM to BRAM, DDR from and to BRAM
#define DDR_DDR 0
#define BRAM_BRAM 1
#define DDR_BRAM 2
#define BRAM_DDR 3

#define DDR 3
#define BRAM 2

#define K 1000
#define MEG 1000000
#define NBYTES 10*K

#define WORD 2

#define F_CLK_PL 	75000000

//Decimal Places for results
#define DP_DROP 1000
#define DP_THRP 1000

#define MAXC 40



//Performs a transaction between BRAM or DDR and calculates the different metrics
int user_tests_1(int nTest, u32 nBytes, Result *result, u32 address_o, u32 address_d);

//Analyzes the results from every test
int user_tests_analyse_results(Result results[TESTS][SUB_TESTS], Result average[SUB_TESTS], char visualize);

//Fills Memory with nBytes bytes, incrementing by 0x1h and starting at 'address'
int user_tests_fillMemory(u32 address, u32 nBytes, char empty);

//Returns the total number of bytes written
u32 user_tests_readMemory(u32 address, u32 nBytes);

//Checks if address and data belong to DDR or BRAM and also checks if nBytes will fit in the memory range
int whichMemory(u32 address, u32 nBytes);

//Calculates t_profiling and t_latency and inserts the results into results[mm2s]
int test_1_profiling_latency(Result *result, u32 address_o, u32 address_d);

//Calculates t_total, total_bytes and drop_rate and inserts the results into results[mm2s]
int test_1_dropping(u32 nBytes, Result *result, u32 address_o, u32 address_d);

//Converts upper and lower values of a float represented by two numbers to a string
void visualizeFloat(char number_string[MAXC],int decimalPlaces, u32 value_upper, u32 value_lower);

//Verifies if a transfer has been executed correctly by checking the memories and DMA status register and error flags, returns amount of data transferred correctly
int test_verify_transfer(u32 dataLength, CdmaFlags flags, u32 address_o, u32 address_d, u32* read);

//Initiates the counters for the both metrics tests
void test_init_counters(u32 option);

//Initiates memories for both metrics tests
int test_init_memories(u32 address_o, u32 address_d, u32 dataLength);


#endif /* SRC_USER_TESTS_H_ */
