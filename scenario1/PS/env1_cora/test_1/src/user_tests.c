/*
 * user_tests.c
 *
 *  Created on: Mar 8, 2020
 *      Author: Gerard Planella Fontanillas
 */

#include "user_tests.h"

//Performs a transaction from DDR to FIFO and then from FIFO to DDR
int user_tests_1(int nTest, u32 nBytes, Result *result, char mm2s, u32 address){
	int status;

	xil_printf("\n\t\t ---- Profiling Test %d|%u ---- \n\n\r", nTest, mm2s);

	status = test_1_profiling_latency(result, mm2s, address);
	if(status != XST_SUCCESS){
		xil_printf("[TEST] Profiling and Latency failed \r\n");
		return XST_FAILURE;
	}


	xil_printf("\n\t\t ---- Drop Rate & Throughput Test %d|%u ---- \n\n\r", nTest, mm2s);

	status = test_1_dropping(nBytes, result, mm2s, address);
	if(status != XST_SUCCESS){
		xil_printf("[TEST] Dropping failed \r\n");
		return XST_FAILURE;
	}




	return XST_SUCCESS;
}

//Calculates t_profiling and t_latency and inserts the results into results[mm2s]
int test_1_profiling_latency(Result *result, char mm2s, u32 address){
	DmaFlags flags;
	FifoParams fifo;
	int status;
	u32 read;
	u32 dataLength = BYTES_FIFO(1); //One FIFO word


	/*
	 * First part of the test: Latency and Profiling calculation
	 */


	if(test_init_memories(mm2s, address, dataLength) != XST_SUCCESS){
		xil_printf("[TEST | PROFILING] Memory initialization failed\r\n");
		return XST_FAILURE;
	}

	 if(user_gpio_get_fifo(&fifo, FIFO_ALL) != XST_SUCCESS){
		xil_printf("[FIFO]: Get FIFO failed\r\n");
		return XST_FAILURE;
	}

	xil_printf("FIFO: \n\r\tData Count %u\n\r\tRD Busy %x\n\r\tWR BUSY %x\n\r\tUDF %x\n\r\tOVF %x\n\n\r", fifo.data_count, fifo.rd_busy, fifo.wr_busy, fifo.udf_err, fifo.ovf_err);

	//Initialize the counters
	test_init_counters(DMA_LATENCY | DMA_PROFILING);
	//Flush The Cache
	Xil_DCacheFlushRange((UINTPTR)address, dataLength);
	//Start The transfer
	status = user_dma_transfer(mm2s, address, (mm2s == DMA_MM2S? dataLength : NBYTES), DMA_PROFILING | DMA_LATENCY);
	if(status != XST_SUCCESS){
		return XST_FAILURE;
	}

	//Wait for the transfer to end
	do{
		user_dma_get_status(&flags);
	}while(!flags.transfer_done[(int)mm2s] && !flags.error[(int)mm2s]);

	xil_printf("DMA Latency Transfer Done\n\r");

	//Verify the transfer
	if(test_verify_transfer(DMA_LATENCY | DMA_PROFILING, mm2s, dataLength, flags, address, &fifo, &read) != XST_SUCCESS){
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
int test_1_dropping(u32 nBytes, Result *result, char mm2s, u32 address){
	DmaFlags flags;
	FifoParams fifo;
	int status;
	u32 read;
	u32 dataLength = nBytes;
	u64 result_aux;
	char str_thrp[MAXC], str_drop[MAXC];

	/*
	 * Second part of the test: Throughput and drop rate calculation
	 */



	if(test_init_memories(mm2s, address, dataLength) != XST_SUCCESS){
		xil_printf("[TEST | DROP_RATE] Memory initialization failed\r\n");
		return XST_FAILURE;
	}



	if(mm2s == DMA_S2MM){
		if(user_gpio_get_fifo(&fifo, FIFO_ALL) != XST_SUCCESS){
			xil_printf("[FIFO]: Get FIFO failed\r\n");
			return XST_FAILURE;
		}

		xil_printf("FIFO: \n\r\tData Count %u\n\r\tRD Busy %x\n\r\tWR BUSY %x\n\r\tUDF %x\n\r\tOVF %x\n\n\r", fifo.data_count, fifo.rd_busy, fifo.wr_busy, fifo.udf_err, fifo.ovf_err);

	}

	//Initialize the counters
	test_init_counters(DMA_TOTAL);
	//Flush The Cache
	Xil_DCacheFlushRange((UINTPTR)address, nBytes);
	//Configure and start the DMA transfer
	status = user_dma_transfer(mm2s, address, (u32)dataLength, DMA_TOTAL);
	if(status != XST_SUCCESS){
		user_gpio_set_counter(1, COUNTER_SRESET);
		return XST_FAILURE;
	}

	//Wait for the transfer to end
	do{
		user_dma_get_status(&flags);
	}while(!flags.transfer_done[(int)mm2s] && !flags.error[(int)mm2s]);

	//Read the total time taken, this counter is stopped in the interrupt
	user_gpio_get_count(&(result->t_total), COUNTER_TOTAL);
	user_gpio_set_counter(COUNTER_TOTAL, COUNTER_SRESET);

	if(test_verify_transfer(DMA_TOTAL, mm2s, dataLength, flags, address, &fifo, &read) != XST_SUCCESS){
		xil_printf("[TEST | DROP RATE] Result verification failed read = %u\r\n", read);
		return XST_FAILURE;
	}

	result->bytes_transferred = read;
	if(read == dataLength){
		result->drop_rate = 0;
	}else{
		result->drop_rate = (DP_DROP* ((u32)dataLength - read))/NBYTES; //Always smaller or equal to 1*DR_DROP
	}

	result_aux = (DP_THRP *  result->bytes_transferred * (F_CLK_PL / MEG)) / result->t_total ;

	result->throughput[0] = ((u32)result_aux) / DP_THRP;
	result->throughput[1] = ((u32)result_aux) % DP_THRP;

	visualizeFloat(str_thrp, DP_THRP, result->throughput[0], result->throughput[1]);
	visualizeFloat(str_drop, DP_DROP, 0, result->drop_rate);


	xil_printf("Transferred Bytes %u,  Drop rate %s, Total time %u and Throughput %s MBps calculated successfully\n\r",
			result->bytes_transferred,
			str_drop,
			result->t_total,
			str_thrp);


	return XST_SUCCESS;

}

int test_init_memories(char mm2s, u32 address, u32 dataLength){
	xil_printf("Emptying FIFO\r\n");
	//Empty the FIFO
	if(user_tests_fillFIFO(address, dataLength, EMPTY) != XST_SUCCESS){
		xil_printf("[TEST] Profiling: FIFO empty failed \r\n");
		return XST_FAILURE;
	}

	if(user_tests_fillDDR(address, NBYTES, EMPTY) != XST_SUCCESS){
		xil_printf("[TEST] Profiling: DDR empty failed \n\r");
		return XST_FAILURE;
	}

	if(mm2s == DMA_MM2S){

		//Initialize DDR for transfer
		xil_printf("Filling DDR\r\n");
		if(user_tests_fillDDR(address, dataLength, NEMPTY) != XST_SUCCESS){
			xil_printf("[TEST] Profiling: DDR fill failed \n\r");
			return XST_FAILURE;
		}

	}else{

		//Initialize FIFO for transfer
		xil_printf("Filling FIFO\r\n");
		if(user_tests_fillFIFO(address, dataLength, NEMPTY) != XST_SUCCESS){
			xil_printf("[TEST] Profiling: FIFO Fill failed \r\n");
			return XST_FAILURE;
		}

		xil_printf("Emptying DDR\n\r");
		//Empty the DDR
		if(user_tests_fillDDR(address, NBYTES, EMPTY) != XST_SUCCESS){
			xil_printf("[TEST] Profiling: DDR empty failed \n\r");
			return XST_FAILURE;
		}
	}
	return XST_SUCCESS;
}

void test_init_counters(u32 option){
	if(option & (DMA_LATENCY | DMA_PROFILING)){
		//Restart the counters
		user_gpio_set_counter(COUNTER_PROFILING, COUNTER_SRESET);
		user_gpio_set_counter(COUNTER_LATENCY, COUNTER_SRESET);

		//We start with a transfer of one word in order to compute the latency time and profiling
		user_gpio_set_counter(COUNTER_PROFILING, COUNTER_ENABLE);
	}else if(option & DMA_TOTAL){
		//Reset and enable counter 1
		user_gpio_set_counter(COUNTER_TOTAL, COUNTER_SRESET);
		//Counter Enabled
		user_gpio_set_counter(COUNTER_TOTAL, COUNTER_ENABLE);
	}
}

int test_verify_transfer(char option, char mm2s, u32 dataLength, DmaFlags flags, u32 address, FifoParams *fifo, u32* read){
	u32 status_reg;

	//Check for errors in the transfer
	if(flags.error[(int)mm2s] == DMA_FLAG_ACTIVE){
		if(mm2s == DMA_S2MM){
			if(user_gpio_get_fifo(fifo, FIFO_ALL) != XST_SUCCESS){
				xil_printf("[FIFO]: Get FIFO failed\r\n");
				return XST_FAILURE;
			}

			xil_printf("FIFO: \n\r\tData Count %u\n\r\tRD Busy %x\n\r\tWR BUSY %x\n\r\tUDF %x\n\r\tOVF %x\n\n\r", fifo->data_count, fifo->rd_busy, fifo->wr_busy, fifo->udf_err, fifo->ovf_err);
		}
		xil_printf("DMA Error flag activated\r\n");
		user_dma_get_status_reg(mm2s, &status_reg);
		xil_printf("Status Register: 0x%x\r\n", status_reg);
		return XST_FAILURE;
	}

	if(option & (DMA_LATENCY | DMA_PROFILING)){
		//Check the memory to verify the transfer
		if(mm2s == DMA_MM2S){
			if(user_gpio_get_fifo(fifo, FIFO_DATA_COUNT) != XST_SUCCESS){
				xil_printf("[FIFO]: Get FIFO failed\r\n");
				return XST_FAILURE;
			}
			*read = BYTES_FIFO(fifo->data_count);
			if(dataLength != *read){
				xil_printf("FIFO read error");
				return XST_FAILURE;
			}

		}else{

			//We check if the transfer was successful
			*read = user_tests_readDDR(address, dataLength);
			if(*read == DDR_READ_ERROR || *read < dataLength){
				xil_printf("[DDR] Read Error\n\r");
				return XST_FAILURE;
			}
		}
	}else if(option & DMA_TOTAL){
		if(mm2s == DMA_MM2S){
				if(user_gpio_get_fifo(fifo, FIFO_DATA_COUNT) != XST_SUCCESS){
					xil_printf("[FIFO]: Get FIFO failed\r\n");
					return XST_FAILURE;
				}
				*read = BYTES_FIFO(fifo->data_count);
			}else{
				//We check if the transfer was successful
				*read = user_tests_readDDR(address, dataLength);
				if(*read == DDR_READ_ERROR){
					xil_printf("[DDR] Read Error\n\r");
					return XST_FAILURE;
				}
			}
	}

	return XST_SUCCESS;
}

//Fills RAM with nBytes bytes, incrementing by 0x1h and starting at XPAR_PS7_DDR_0_S_AXI_BASEADDR with the value 1
int user_tests_fillDDR(u32 address, u32 nBytes, char empty){
	int i = 0;
	u16 value = 0;
	u32 words = (u16)WORDS_DDR(nBytes);
	u32 addr_offset = 0;
	u16 *mem;

	if (address + nBytes >= DDR_HIGH_ADDR){
		xil_printf("[USER_TESTS_FILLDDR] Invalid address range\r\n");
		return XST_FAILURE;
	}

	for(i = 0; i < words; i++){
		mem = (u16 *)(address + addr_offset);
		*mem = (empty == NEMPTY ? value++ : 0);
		addr_offset+=BYTES_DDR(1);
	}

	if(empty == NEMPTY){
		if(user_tests_readDDR(address, nBytes) < nBytes){
			xil_printf("[USER_TESTS_FILLDDR] Invalid read amount\r\n");
			return XST_FAILURE;
		}

	}

	xil_printf("[USER_TESTS_FILLDDR] Data written from 0x%x to 0x%x with final value %u\r\n", address, (address + addr_offset - BYTES_DDR(1)), (empty == NEMPTY ? value - 1 : 0));

	return XST_SUCCESS;
}

//Returns the total number of bytes written in the RAM
u32 user_tests_readDDR(u32 address, u32 nBytes){
	u16 j = 0;
	u16 value = 0;
	u16 words = (u16) WORDS_DDR(nBytes);
	u32 addr_offset = 0;
	u32 ko = 0;
	u16 *mem;

	if (address + nBytes >= DDR_HIGH_ADDR){
		xil_printf("[USER_TESTS_READDDR] Invalid address range\r\n");
		return DDR_READ_ERROR;
	}

	Xil_DCacheFlushRange((UINTPTR)address, nBytes);
	for(j = 0; j < words; j++){
		mem = (u16 *)(address + addr_offset);
		value = *mem;
		if(value != j){
			ko++;
			//xil_printf("Incorrect Value %u read at 0x%x\r\n", value, address + addr_offset);
		}
		addr_offset+=BYTES_DDR(1);

	}
	xil_printf("DDR Read: Requested = %u, Read = %u \r\n", nBytes, BYTES_DDR((j - ko)));

	return (BYTES_DDR((j - ko)));
}

//If empty is NEMPTY the FIFO is filled with nBytes bytes. If empty is EMPTY the FIFO is restarted
int user_tests_fillFIFO(u32 address, u32 nBytes, char empty){
	u32 bytesWritten = 0, toWrite = 0, bytes = 0;
	DmaFlags flags;
	int status;
	FifoParams fifo;


	if (empty == EMPTY){
		status = user_gpio_reset_fifo();
		if (status == XST_FAILURE){
			xil_printf("FIFO reset failed\n\r");
			if(user_gpio_get_fifo(&fifo, FIFO_ALL) != XST_SUCCESS){
				xil_printf("[FIFO]: Get FIFO failed\r\n");
				return XST_FAILURE;
			}

			xil_printf("FIFO: \n\r\tData Count %u\n\r\tRD Busy %x\n\r\tWR BUSY %x\n\r\tUDF %x\n\r\tOVF %x\n\n\r", fifo.data_count, fifo.rd_busy, fifo.wr_busy, fifo.udf_err, fifo.ovf_err);

		}
		return status;
	}

	if (empty == NEMPTY){
		if(user_tests_fillDDR(address, nBytes, NEMPTY) != XST_SUCCESS){
			xil_printf("[USER_TESTS_FILLFIFO] DDR FILL error before init\n\r");
			return XST_FAILURE;
		}
		Xil_DCacheFlushRange((UINTPTR)address, nBytes);
		while(bytes < nBytes){
			toWrite = nBytes - bytes;
			if(toWrite < 4) toWrite = 4;
			if(user_dma_transfer(DMA_MM2S, address + bytes, toWrite, DMA_NOPTION) != XST_SUCCESS){
				xil_printf("FIFO Fill: DMA Transfer Error\n\r");
			}

			do{
				user_dma_get_status(&flags);
			}while(!flags.transfer_done[DMA_MM2S] && !flags.error[DMA_MM2S]);

			if(flags.error[DMA_MM2S]){
				xil_printf("FIFO Fill: DMA Flag Error\n\r");
				return XST_FAILURE;
			}

			if(user_gpio_get_fifo(&fifo, FIFO_DATA_COUNT) != XST_SUCCESS);
			bytesWritten = BYTES_FIFO(fifo.data_count);
			bytes += bytesWritten;
			xil_printf("Bytes written: %u\n\r", bytes);
		}
		xil_printf("Bytes written: %u\n\r", bytesWritten);
		user_tests_fillDDR(address, nBytes, EMPTY);
		return XST_SUCCESS;
	}
	xil_printf("Wrong FIFO empty option \r\n");
	return XST_FAILURE;
}


//Analyzes the results from every test
int user_tests_analyse_results(Result results_s2mm[TESTS_S2MM], Result results_mm2s[TESTS_MM2S], Result average[SUB_TESTS], char visualize){
	int i, j;
	Result *current;
	char str_number[MAXC];
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

	for(j = 0; j < SUB_TESTS; j++){
		for(i = 0; i < (j == DMA_MM2S? TESTS_MM2S: TESTS_S2MM); i++){
			if (visualize == RESULTS_VISUALIZE) xil_printf("\n---- Test ----\n\r", i);

				current = (j == 0) ? &results_mm2s[i]:&results_s2mm[i];

			if (visualize == RESULTS_VISUALIZE){
				xil_printf("\t---- Sub-test [%s| %d] ----\n\r", (j == DMA_MM2S ? "MM2S" : "S2MM"), i);
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
			if((j == DMA_MM2S? TESTS_MM2S: TESTS_S2MM) > 1){
				num_aux[0]  			= 	(current->throughput[0] * DP_THRP / TESTS_MM2S) / DP_THRP;
				num_aux[1]  			= 	(current->throughput[0] * DP_THRP / TESTS_MM2S) % DP_THRP;
				num_aux[1]   			+= 	current->throughput[1] / TESTS_MM2S;
				num_aux[1]				+= 	average[j].throughput[1];
				num_aux[0]				+= 	(num_aux[1] / (10 * DP_THRP));
				average[j].throughput[1]= 	num_aux[1];
				average[j].throughput[0]+=  num_aux[0];
				average[j].drop_rate   	+= (current->drop_rate * DP_DROP  /TESTS_MM2S);
				average[j].t_total 	   	+= (current->t_total     /TESTS_MM2S);
				average[j].t_profiling 	+= (current->t_profiling /TESTS_MM2S);
				average[j].t_latency   	+= (current->t_latency   /TESTS_MM2S);
			}else{
				average[j].throughput[1]= current->throughput[1];
				average[j].throughput[0]= current->throughput[0];
				average[j].drop_rate = current->drop_rate;
				average[j].t_total = current->t_total;
				average[j].t_profiling = current->t_profiling;
				average[j].t_latency = current->t_latency;
			}
		}
	}
	if (visualize == RESULTS_VISUALIZE && TESTS_MM2S > 1){
		xil_printf("\n\n----	Averages	----\n\r");
		for(j = 0; j < SUB_TESTS; j++){
			current = &average[j];
			xil_printf("\t----  %s ----\n\r", (j == DMA_MM2S ? "MM2S" : "S2MM"));
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


///Converts upper and lower values of a float represented by two numbers to a string
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
