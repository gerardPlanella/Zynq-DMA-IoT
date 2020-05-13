/*
 * user_tests.c
 *
 *  Created on: Mar 8, 2020
 *      Author: Gerard Planella Fontanillas
 */

#include "user_tests.h"


//Performs a transaction between BRAM or DDR and calculates the different metrics
int user_tests_1(int nTest, u32 nBytes, Result *result, u32 address_o, u32 address_d){
	int status;

	xil_printf("\n\t\t ---- Profiling Test %d ---- \n\n\r", nTest);

	if(test_1_profiling_latency(result, address_o, address_d) != XST_SUCCESS){
		xil_printf("[TEST] Profiling and Latency failed \r\n");
		return XST_FAILURE;
	}

	xil_printf("\n\t\t ---- Drop Rate & Throughput Test %d ---- \n\n\r", nTest);

	status = test_1_dropping(nBytes, result, address_o, address_d);
	if(status != XST_SUCCESS){
		xil_printf("[TEST] Dropping failed \r\n");
		return XST_FAILURE;
	}
	return XST_SUCCESS;
}

//Calculates t_profiling and t_latency and inserts the results into result
int test_1_profiling_latency(Result *result, u32 address_o, u32 address_d){
	CdmaFlags flags;
	int status;
	u32 read;
	u32 dataLength = BYTES_BRAM(1); //One FIFO word


	/*
	 * First part of the test: Latency and Profiling calculation
	 */


	if(test_init_memories(address_o, address_d, dataLength) != XST_SUCCESS){
		xil_printf("[TEST | PROFILING] Memory initialization failed\r\n");
		return XST_FAILURE;
	}

	//Initialize the counters
	test_init_counters(CDMA_LATENCY | CDMA_PROFILING);

	if(whichMemory(address_o, dataLength) == DDR){
			//Flush The Cache
			Xil_DCacheFlushRange((UINTPTR)address_o, dataLength);
	}
	if(whichMemory(address_d, dataLength) == DDR){
		//Flush The Cache
		Xil_DCacheFlushRange((UINTPTR)address_d, dataLength);
	}

	//Start The transfer
	//xil_printf("Starting CDMA transfer...\r\n");
	status = user_cdma_transfer(address_o, address_d, dataLength, CDMA_PROFILING | CDMA_LATENCY);
	if(status != XST_SUCCESS){
		return XST_FAILURE;
	}
	//Wait for the transfer to end
	do{
		user_cdma_get_status(&flags);
	}while(!flags.transfer_done && !flags.error);

	xil_printf("CDMA Latency Transfer Done\n\r");

	//Verify the transfer
	if(test_verify_transfer(dataLength, flags, address_o, address_d, &read) != XST_SUCCESS){
		xil_printf("[TEST | PROFILING] Result verification failed read = %u\r\n", read);
		return XST_FAILURE;
	}

	user_gpio_get_count(&(result->t_profiling), COUNTER_PROFILING); //Counter 1 is started in user_dma_transfer() when DMA_PROFIING option is used
	user_gpio_set_counter(COUNTER_PROFILING, COUNTER_SRESET);

	user_gpio_get_count(&(result->t_latency), COUNTER_LATENCY); //Counter 2 is stopped in user_dma_transfer() when DMA_LATENCY option is used
	user_gpio_set_counter(COUNTER_LATENCY, COUNTER_SRESET);



	xil_printf("Latency and Profiling calculated successfully p: %u   l: %u\r\n", result->t_profiling, result->t_latency);


	return XST_SUCCESS;
}

//Calculates t_total, total_bytes and drop_rate and inserts the results into results[mm2s]
int test_1_dropping(u32 nBytes, Result *result, u32 address_o, u32 address_d){
	CdmaFlags flags;
	int status;
	u32 read;
	u32 dataLength = nBytes;
	u32 result_aux;
	char str_number[MAXC];

	/*
	 * Second part of the test: Throughput and drop rate calculation
	 */



	if(test_init_memories(address_o, address_d, dataLength) != XST_SUCCESS){
		xil_printf("[TEST | DROP_RATE] Memory initialization failed\r\n");
		return XST_FAILURE;
	}

	//Initialize the counters
	test_init_counters(CDMA_TOTAL);
	//Configure and start the DMA transfer
	if(whichMemory(address_o, dataLength) == DDR){
		//Flush The Cache
		Xil_DCacheFlushRange((UINTPTR)address_o, nBytes);
	}
	if(whichMemory(address_d, dataLength) == DDR){
		//Flush The Cache
		Xil_DCacheFlushRange((UINTPTR)address_d, nBytes);
	}
	//xil_printf("Starting CDMA transfer...\r\n");
	status = user_cdma_transfer(address_o, address_d, dataLength, CDMA_TOTAL);
	if(status != XST_SUCCESS){
		user_gpio_set_counter(1, COUNTER_SRESET);
		return XST_FAILURE;
	}
	//xil_printf("Waiting...\r\n");
	//Wait for the transfer to end
	do{
		user_cdma_get_status(&flags);
	}while(!flags.transfer_done && !flags.error);
	xil_printf("CDMA transfer done\r\n");
	//Read the total time taken, this counter is stopped in the interrupt
	user_gpio_get_count(&(result->t_total), COUNTER_TOTAL);
	user_gpio_set_counter(COUNTER_TOTAL, COUNTER_SRESET);
	xil_printf("Counters Stopped\r\n");
	if(test_verify_transfer(dataLength, flags, address_o, address_d, &read) != XST_SUCCESS){
		xil_printf("[TEST | DROP RATE] Result verification failed read = %u\r\n", read);
		return XST_FAILURE;
	}
	xil_printf("Result Verified\r\n");

	result->bytes_transferred = read;
	if(read == NBYTES){
		result->drop_rate = 0;
	}else{
		result->drop_rate = (DP_DROP* ((u32)dataLength - read))/NBYTES; //Always smaller or equal to 1*DR_DROP
	}

	result_aux = (DP_THRP *  result->bytes_transferred * (F_CLK_PL / MEG)) / result->t_total ;

	result->throughput[0] = result_aux / DP_THRP;
	result->throughput[1] = result_aux % DP_THRP;

	visualizeFloat(str_number, DP_THRP, result->throughput[0], result->throughput[1]);


	xil_printf("Transferred Bytes %u,  Drop rate 0.%u, Total time %u and Throughput %s MBps calculated successfully\n\r",
			result->bytes_transferred,
			result->drop_rate,
			result->t_total,
			str_number);


	return XST_SUCCESS;
}

//Verifies if a transfer has been executed correctly by checking the memories and DMA status register and error flags, returns amount of data transferred correctly
int test_verify_transfer(u32 dataLength, CdmaFlags flags, u32 address_o, u32 address_d, u32* read){
	u32 status_reg;

	//Check for errors in the transfer
	if(flags.error == CDMA_FLAG_ACTIVE){
		xil_printf("DMA Error flag activated\r\n");
		user_cdma_get_status_reg(&status_reg);
		xil_printf("Status Register: 0x%x\r\n", status_reg);
		return XST_FAILURE;
	}
	//Check the memory to verify the transfer
	*read = user_tests_readMemory(address_d, dataLength);
	if(*read == READ_ERROR){
		xil_printf("[TRANSFER VERIFICATION] Read Error\n\r");
		return XST_FAILURE;
	}
	return XST_SUCCESS;
}

//Fills Memory with nBytes bytes, incrementing by 0x1h and starting at 'address'
int user_tests_fillMemory(u32 address, u32 nBytes, char empty){
	int i = 0;
	u16 value = 0;
	u32 words = WORDS_DDR(nBytes);
	u32 addr_offset = 0;
	u16 *mem;

	if(whichMemory(address, nBytes) == XST_FAILURE) return XST_FAILURE;

	for(i = 0; i < words; i++){
		mem = (u16 *)(address + addr_offset);
		*mem = (empty == NEMPTY ? value++ : 0);
		addr_offset+= BYTES_DDR(1);
	}

	if(empty == NEMPTY){
		if(user_tests_readMemory(address, nBytes) < nBytes){
			//xil_printf("[USER_TESTS_FILLMEMORY] Invalid read amount\r\n");
			return XST_FAILURE;
		}

	}

	xil_printf("[USER_TESTS_FILLMEMORY] Data written from 0x%x to 0x%x with final value %u\r\n", address, (address + addr_offset - BYTES_DDR(1)), (empty == NEMPTY ? value - 1 : 0));

	return XST_SUCCESS;
}

//Returns the total number of bytes written
u32 user_tests_readMemory(u32 address, u32 nBytes){
	u16 j = 0;
	u16 value = 0;
	u16 words = (u16) WORDS_DDR(nBytes);
	u32 addr_offset = 0;
	u32 ko = 0;
	u16 *mem;
	int memory = whichMemory(address, nBytes);

	if(memory == XST_FAILURE){
		xil_printf("[ReadMemory]Invalid Read Address\r\n");
		return XST_FAILURE;
	}


	if(memory == DDR){
		xil_printf("Flushing cache\r\n");
		Xil_DCacheFlushRange((UINTPTR)address, nBytes);
	}
	if(memory == BRAM){
		xil_printf("Debug Verify tx 1.1\r\n");
		while(user_cdma_isBusy());
	}
	for(j = 0; j < words; j++){
		mem = (u16 *)(address + addr_offset);
		value = *mem;
		if(value != j){
			ko++;
			//xil_printf("Incorrect Value %u read at 0x%x\r\n", value, address + addr_offset);
		}
		addr_offset+=BYTES_DDR(1);

	}
	if(j < ko) j = ko;
	xil_printf("Read: Requested = %u, Read = %u \r\n", nBytes, BYTES_DDR((j - ko)));

	return (BYTES_DDR((j - ko)));
}

//Checks if address and data belong to DDR or BRAM and also checks if nBytes will fit in the memory range
int whichMemory(u32 address, u32 nBytes){
	if((address <= BRAM_HIGH_ADDR) && (address >= BRAM_BASE_ADDR)){
		if((address + WORDS_BRAM(nBytes)) <= BRAM_HIGH_ADDR){
			return BRAM;
		}else{
			return XST_FAILURE;
		}
	}else if(address <= DDR_HIGH_ADDR && address >= DDR_BASE_ADDR){
		if((address + WORDS_DDR(nBytes)) <= DDR_HIGH_ADDR){
			return DDR;
		}else{
			return XST_FAILURE;
		}
	}
	return XST_FAILURE;
}



//Initiates memories for both metrics tests
int test_init_memories(u32 address_o, u32 address_d, u32 dataLength){
	if(user_tests_fillMemory(address_o, dataLength, NEMPTY) != XST_SUCCESS){
		xil_printf("Origin memory initialization failed \n\r");
		return XST_FAILURE;
	}

	if(user_tests_fillMemory(address_d, dataLength, EMPTY) != XST_SUCCESS){
			xil_printf("Destination memory empty failed \n\r");
			return XST_FAILURE;
		}

	return XST_SUCCESS;
}

void test_init_counters(u32 option){
	if(option & (CDMA_LATENCY | CDMA_PROFILING)){
		//Restart the counters
		user_gpio_set_counter(COUNTER_PROFILING, COUNTER_SRESET);
		user_gpio_set_counter(COUNTER_LATENCY, COUNTER_SRESET);

		//We start with a transfer of one word in order to compute the latency time and profiling
		user_gpio_set_counter(COUNTER_PROFILING, COUNTER_ENABLE);
	}else if(option & CDMA_TOTAL){
		//Reset and enable counter 1
		user_gpio_set_counter(COUNTER_TOTAL, COUNTER_SRESET);
		//Counter Enabled
		user_gpio_set_counter(COUNTER_TOTAL, COUNTER_ENABLE);
	}
}

//Analyzes the results from every test
int user_tests_analyse_results(Result results[TESTS][SUB_TESTS], Result average[SUB_TESTS], char visualize){
	int i, j;
	Result *current;
	char str_number[MAXC];
	char* mode;
	int f_clk = ((int)F_CLK_PL) / MEG;
	u32 num_aux[2];



	if(visualize == RESULTS_VISUALIZE)xil_printf("\n------- Results -------\n\n\r");

	for(i = 0; i < SUB_TESTS; i++){
		average[i].drop_rate = 0;
		average[i].t_latency = 0;
		average[i].t_profiling = 0;
		average[i].t_total = 0;
		average[i].throughput[0] = 0;
		average[i].throughput[1] = 0;
	}

	for(i = 0; i < TESTS; i++){
		if (visualize == RESULTS_VISUALIZE) xil_printf("\n---- Test %d ----\n\r", i + 1);
		for(j = 0; j < SUB_TESTS; j++){
			switch (j){
				case 0:
					mode = "DDR to DDR";
					break;
				case 1:
					mode = "BRAM to BRAM";
					break;
				case 2:
					mode = "DDR to BRAM";
					break;
				case 3:
					mode = "BRAM to DDR";
					break;
				default:
					mode = "Unknown Mode";
			}

			current = &results[i][j];

			if (visualize == RESULTS_VISUALIZE){
				xil_printf("\t---- Sub-test [%s] ----\n\r", mode);
				xil_printf("\t\tClock Frequency %dMHz\n\r", f_clk);
				xil_printf("\t\tTotal time: %u CLK\n\r"			, current->t_total);
				xil_printf("\t\tTotal Transfer: %d Bytes\n\r"			, NBYTES);
				visualizeFloat(str_number, DP_THRP, current->throughput[0], current->throughput[1]);
				xil_printf("\t\tThroughput: %s MBps\n\r" 		, str_number);
				visualizeFloat(str_number, DP_DROP, 0, current->drop_rate);
				xil_printf("\t\tDrop Rate: %s \n\r"			, str_number);
				xil_printf("\t\tLatency: %u CLK\n\r"			, current->t_latency);
				xil_printf("\t\tProfiling: %u CLK\n\r"	, current->t_profiling, str_number);
			}
			if(TESTS > 1){
				num_aux[0]  			= 	(current->throughput[0] * DP_THRP / TESTS) / DP_THRP;
				num_aux[1]  			= 	(current->throughput[0] * DP_THRP / TESTS) % DP_THRP;
				num_aux[1]   			+= 	current->throughput[1] / TESTS;
				num_aux[1]				+= 	average[j].throughput[1];
				num_aux[0]				+= 	(num_aux[1] / (10 * DP_THRP));
				average[j].throughput[1]= 	num_aux[1];
				average[j].throughput[0]+=  num_aux[0];
				average[j].drop_rate   	+= (current->drop_rate * DP_DROP  /TESTS);
				average[j].t_total 	   	+= (current->t_total     /TESTS);
				average[j].t_profiling 	+= (current->t_profiling /TESTS);
				average[j].t_latency   	+= (current->t_latency   /TESTS);
			}
		}

	}
	if (visualize == RESULTS_VISUALIZE && TESTS > 1){
		xil_printf("\nAverage Results\r\n\n");
		for(j = 0; j < SUB_TESTS; j++){
			switch (j){
				case 0:
					mode = "DDR to DDR";
					break;
				case 1:
					mode = "BRAM to BRAM";
					break;
				case 2:
					mode = "DDR to BRAM";
					break;
				case 3:
					mode = "BRAM to DDR";
					break;
				default:
					mode = "Unknown Mode";
			}

			current = &average[j];
			xil_printf("---- Average Results [%s] ----\n\r", mode);
			xil_printf("\t\tClock Frequency %dMHz\n\r", f_clk);
			xil_printf("\t\tTotal time: %u CLK\n\r"			, current->t_total);
			xil_printf("\t\tTotal Transfer: %d Bytes\n\r"		, NBYTES);
			visualizeFloat(str_number, DP_THRP, current->throughput[0], current->throughput[1]);
			xil_printf("\t\tThroughput: %s MBps\n\r" 		, str_number);
			visualizeFloat(str_number, DP_DROP, 0, current->drop_rate);
			xil_printf("\t\tDrop Rate: %s \n\r"			, str_number);
			xil_printf("\t\tLatency: %u CLK\n\r"			, current->t_latency);
			xil_printf("\t\tProfiling: %u CLK\n\r"	, current->t_profiling, str_number);
		}
	}


	return XST_SUCCESS;

}


//Converts upper and lower values of a float represented by two numbers to a string
void visualizeFloat(char number_string[MAXC],int decimalPlaces, u32 value_upper, u32 value_lower){
	int i = 0, j = 0, dp = 0;
	char lowerNumber[MAXC];

	do{
		decimalPlaces /= 10;
		dp++;
	}while(decimalPlaces >= 10);


	for (i = dp - 1, j = 10; i >= 0; i--, j*=10){
		lowerNumber[i] = '0' + ((value_lower % j) / (j / 10));
	}
	lowerNumber[dp] = '\0';
	sprintf(number_string, "%lu.%s", value_upper, lowerNumber);
}
