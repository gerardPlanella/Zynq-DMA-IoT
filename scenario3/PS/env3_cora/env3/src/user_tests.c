/*
 * user_tests.c
 *
 *  Created on: Mar 8, 2020
 *      Author: Gerard Planella Fontanillas
 */

#include "user_tests.h"

//Performs a test for a certain mode and calculates the throughput, drop rate and profiling
int user_tests_3_mode(u32 nBytes, u32 mode, u32 length, u16 delay, u32 start_addr, Result *result){
	TrafGenState trafgen;
	FifoParams fifo;
	DmaFlags flags;
	TestVar testvar;
	//AxisCount stream_cnt;
	u32 dataLength = nBytes;
	u64 bytes_aux;
	u32 aux;
	int i;
	int status;


	trafgen.count = trafgen.done = trafgen.error_count = trafgen.error_status = 0;

	//Resets FIFO and DDR
	if(test_init_memories(DMA_TG, start_addr, dataLength) != XST_SUCCESS){
		xil_printf("[MODE TEST]Memory Initialisation Failed\r\n");
		return XST_FAILURE;
	}

	//Restart the traffic Generator
	user_trafgen_reset();

	user_trafgen_enable(TG_DISABLE);

	user_trafgen_set_mode(nBytes, mode, length, delay, &dataLength);


	//Initialize the counters
	test_init_counters(DMA_TOTAL_TG | DMA_PROFILING);

	//Flush The Cache
	Xil_DCacheFlush();

	//Enable the traffic generator
	user_trafgen_enable(TG_ENABLE);

	//Start The transfer
	status = user_dma_transfer(DMA_S2MM, start_addr, nBytes, DMA_TOTAL_TG);
	if(status != XST_SUCCESS){
		return XST_FAILURE;
	}

	//Wait for the transfer to end
	do{
		user_dma_get_status(&flags);
		user_dma_get_TestVar(&testvar);
		user_trafgen_get_state(TG_ALL, &trafgen);

		user_dma_get_status_reg(DMA_S2MM, &aux);
		if(user_gpio_get_fifo(&fifo, FIFO_ALL) != XST_SUCCESS){
			xil_printf("[FIFO]: Get FIFO failed\r\n");
			return XST_FAILURE;
		}
		if(++i > TEST_TIMEOUT && !(mode & MODE_RAND)){
			xil_printf("[MODE TEST]Test Timeout, restarting test...\r\n");
			return XST_FAILURE;
		}

	}while(!flags.error[DMA_S2MM] && testvar.test_running);

	//Read the total time taken, this counter is stopped in the interrupt
	user_gpio_get_count(&(result->t_total), COUNTER_TOTAL);
	user_gpio_set_counter(COUNTER_TOTAL, COUNTER_SRESET);

	xil_printf("DMA Mode Transfer Done\n\r");

	if(user_gpio_get_fifo(&fifo, FIFO_ALL) != XST_SUCCESS){
		xil_printf("[FIFO]: Gcet   FIFO failed\r\n");
		return XST_FAILURE;
	}



	//Verify the transfer
	if(test_3_verify_transfer(DMA_TOTAL_TG, dataLength, flags, start_addr, &fifo, &trafgen) != XST_SUCCESS){
		xil_printf("[TEST | MODE] Result verification failed\r\n");
		xil_printf("FIFO: \n\r\tWrite Count %u\n\r\tRead Count %u\n\r\tRD Busy %x\n\r\tWR BUSY %x\n\r\tEMPTY %x\n\r\tOVF %x\n\n\r",
											fifo.wr_count, fifo.rd_count, fifo.rd_busy, fifo.wr_busy, fifo.empty, fifo.ovf_err);

		xil_printf("[MODE | VERIFY]Bytes Read %u, Requested %u. Traffic generator. Err Count: %u, Status %u, Done %u, Count %u, Run %u\r\n",
				testvar.total_length_dma, nBytes, trafgen.error_count, trafgen.error_status, trafgen.done, trafgen.count, testvar.test_running);
		user_dma_reset();
		return XST_FAILURE;
	}

	user_dma_get_TestVar(&testvar);
	//Enables or disables the traffic generator
	user_trafgen_enable(TG_DISABLE);

	if(user_gpio_get_fifo(&fifo, FIFO_ALL) != XST_SUCCESS){
		xil_printf("[FIFO]: Get FIFO failed\r\n");
		return XST_FAILURE;
	}

	result->n_ovf_fifo = testvar.fifo_ovf_cnt;

	//Dropped bytes and total bytes transferred
	result->bytes_transferred =  dataLength;
	bytes_aux = (result->bytes_transferred < testvar.total_length_dma)? 0 : (result->bytes_transferred - testvar.total_length_dma);
	result->dropped_bytes = (u32)(bytes_aux & 0xFFFFFFFF);

	xil_printf("Total Transfer: %u, DMA Transferred bytes: %u, %u Bytes Dropped\r\n", result->bytes_transferred, testvar.total_length_dma, result->dropped_bytes);

	if(result->bytes_transferred < result->dropped_bytes){
		xil_printf("[MODE] Error, Bytes transferred < Bytes Read, Bytes Transferred: %u Total DMA Length: %u\r\n", result->bytes_transferred, testvar.total_length_dma);
		return XST_FAILURE;
	}

	result->drop_rate =(DP_DROP* (result->dropped_bytes/result->bytes_transferred)); //Always smaller or equal to 1*DR_DROP
	result->t_profiling = testvar.t_profiling;
	result->n_configs_dma = testvar.n_configs_dma;


	bytes_aux = ((F_CLK_PL/MEG) * (DP_THRP * (result->bytes_transferred))) / result->t_total;
	if(bytes_aux & ~0xFFFFFFFF){
		xil_printf("[TESTS | MODE] Variable bytes_aux exceeds u32 limit\r\n");
		return XST_FAILURE;
	}
	bytes_aux &= 0xFFFFFFFF;

	result->throughput[0] = (u32)((bytes_aux) / DP_THRP);
	result->throughput[1] = (bytes_aux) % DP_THRP;

//	visualizeFloat(str_thrp, DP_THRP, result->throughput[0], result->throughput[1]);
//	visualizeFloat(str_drop, DP_DROP, 0, result->drop_rate);
//
//	xil_printf("Transferred Bytes %u,  Drop rate %s, Total time %u CLK, Throughput %s MBps, NConfigs %d, Profiling Time %u CLK calculated successfully\n\r",
//			result->bytes_transferred,
//			str_drop,
//			result->t_total,
//			str_thrp,
//			result->n_configs_dma,
//			result->t_profiling);

	return XST_SUCCESS;
}

//Performs a test to measure the latency time
int user_tests_3_latency(Result *result, u32 address){
	int status;
	u32 dataLength = BYTES_FIFO(1);
	TrafGenState trafgen;
	FifoParams fifo;
	DmaFlags flags;
	TestVar testvar;


	trafgen.count = trafgen.done = trafgen.error_count = trafgen.error_status = 0;

	//Resets FIFO and DDR
	if(test_init_memories(DMA_TG, address, dataLength) != XST_SUCCESS){
		return XST_FAILURE;
	}
	//Restart the traffic Generator
	user_trafgen_reset();

	//Set the TG to constant transfer mode for 4 bytes with 1 beat.
	user_trafgen_set_mode(BYTES_FIFO(1), MODE_CONST | SUBMODE_LEN, 1, 0, &dataLength);

	//Initialize the counters
	test_init_counters(DMA_LATENCY);

	//Enables or disables the traffic generator
	user_trafgen_enable(TG_ENABLE);

	//Flush The Cache
	Xil_DCacheFlushRange((UINTPTR)address, dataLength);
	//Start The transfer
	status = user_dma_transfer(DMA_S2MM, address, 4800, DMA_LATENCY);
	if(status != XST_SUCCESS){
		xil_printf("[LATENCY TEST] DMA Config Fail\r\n");
		return XST_FAILURE;
	}

	//Wait for the transfer to end
	do{
		user_dma_get_status(&flags);
		user_dma_get_TestVar(&testvar);
	}while(!flags.error[DMA_S2MM] && !flags.transfer_done[DMA_S2MM] && !trafgen_error_check());

	//Verify the transfer
	if(test_3_verify_transfer(DMA_LATENCY, dataLength, flags, address, &fifo, &trafgen) != XST_SUCCESS){
		xil_printf("[TEST | LATENCY] Result verification failed");
		return XST_FAILURE;
	}

	user_gpio_get_count(&(result->t_latency), COUNTER_LATENCY); //Counter 2 is stopped in user_dma_transfer() when DMA_LATENCY option is used
	user_gpio_set_counter(COUNTER_LATENCY, COUNTER_SRESET);

	xil_printf("Latency calculated successfully %u\r\n", result->t_latency);

	//Enables or disables the traffic generator
	user_trafgen_enable(TG_DISABLE);

	return XST_SUCCESS;
}
//Verifies the transfer for the Traffic Generator tests
int test_3_verify_transfer(char option, u32 nBytes, DmaFlags flags, u32 address, FifoParams *fifo, TrafGenState *trafgen){
	u32 status_reg;
	u32 length;

	user_trafgen_get_state(TG_ALL, trafgen);
	user_dma_get_s2mm_length(&length);

	if(flags.error[DMA_S2MM] == DMA_FLAG_ACTIVE){
		if(user_gpio_get_fifo(fifo, FIFO_ALL) != XST_SUCCESS){
			xil_printf("[FIFO]: Gcet   FIFO failed\r\n");
			return XST_FAILURE;
		}
		xil_printf("FIFO: \n\r\tWrite Count %u\n\r\tRead Count %u\n\r\tRD Busy %x\n\r\tWR BUSY %x\n\r\tEMPTY %x\n\r\tOVF %x\n\n\r",
				fifo->wr_count, fifo->rd_count, fifo->rd_busy, fifo->wr_busy, fifo->empty, fifo->ovf_err);
		xil_printf("DMA Error flag activated\r\n");
		user_dma_get_status_reg(DMA_S2MM, &status_reg);
		xil_printf("Status Register: 0x%x\r\n", status_reg);
		return XST_FAILURE;
	}

	if(option & DMA_LATENCY){
		if(trafgen->error_count || trafgen->error_status || !trafgen->done || length < nBytes){
			xil_printf("[LATENCY | VERIFY]DMA length %u, requested %u. Traffic generator. Err Count: %u, Status %u, Done %u, Count %u\r\n",
					length, nBytes, trafgen->error_count, trafgen->error_status, trafgen->done, trafgen->count);
			return XST_FAILURE;
		}
	}
	if(option & DMA_TOTAL_TG){
		if(trafgen->error_status || !trafgen->done /*|| stream->ovf */){
			//xil_printf("[MODE VERIFY] Error: trafgen status %u, trafgen done %u, stream ovf %u, stream cnt %u\r\n", trafgen->error_status, trafgen->done, stream->ovf, stream->count);
			xil_printf("[MODE VERIFY] Error: trafgen status %u, trafgen done %u\r\n", trafgen->error_status, trafgen->done);
			return XST_FAILURE;
		}
	}
	if(option & DMA_TOTAL){
		if(trafgen->error_status){
			xil_printf("[MODE | VERIFY] Traffic generator error\r\n");
			return XST_FAILURE;
		}
	}
	return XST_SUCCESS;
}

int test_init_memories(char mm2s, u32 address, u32 dataLength){

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

	}else if (mm2s == DMA_S2MM){

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

	if(option & DMA_LATENCY){
		user_gpio_set_counter(COUNTER_LATENCY, COUNTER_SRESET);
	}

	if((option & DMA_TOTAL) || (option & DMA_TOTAL_TG)){
		//Reset and enable counter 1
		user_gpio_set_counter(COUNTER_TOTAL, COUNTER_SRESET);
		//Counter Enabled
		user_gpio_set_counter(COUNTER_TOTAL, COUNTER_ENABLE);
	}
	if(option &  DMA_PROFILING){
		//Restart the counters
		user_gpio_set_counter(COUNTER_PROFILING, COUNTER_SRESET);
		//We start with a transfer of one word in order to compute the latency time and profiling
		user_gpio_set_counter(COUNTER_PROFILING, COUNTER_ENABLE);
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

			xil_printf("FIFO: \n\r\tWrite Count %u\n\r\tRead Count %u\n\r\tRD Busy %x\n\r\tWR BUSY %x\n\r\tEMPTY %x\n\r\tOVF %x\n\n\r",
					fifo->wr_count, fifo->rd_count, fifo->rd_busy, fifo->wr_busy, fifo->empty, fifo->ovf_err);
		}
		xil_printf("DMA Error flag activated\r\n");
		user_dma_get_status_reg(mm2s, &status_reg);
		xil_printf("Status Register: 0x%x\r\n", status_reg);
		return XST_FAILURE;
	}

	if(option & (DMA_LATENCY | DMA_PROFILING)){
		//Check the memory to verify the transfer
		if(mm2s == DMA_MM2S){
			if(user_gpio_get_fifo(fifo, FIFO_WR_COUNT) != XST_SUCCESS){
				xil_printf("[FIFO]: Get FIFO failed\r\n");
				return XST_FAILURE;
			}
			*read = BYTES_FIFO(fifo->wr_count);
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
			if(user_gpio_get_fifo(fifo, FIFO_WR_COUNT) != XST_SUCCESS){
				xil_printf("[FIFO]: Get FIFO failed\r\n");
				return XST_FAILURE;
			}
			*read = BYTES_FIFO(fifo->wr_count);
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

	//xil_printf("[USER_TESTS_FILLDDR] Data written from 0x%x to 0x%x with final value %u\r\n", address, (address + addr_offset - BYTES_DDR(1)), (empty == NEMPTY ? value - 1 : 0));

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
	u32 bytesWritten = 0, toWrite = 0;
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

			xil_printf("FIFO: \n\r\tWrite Count %u\n\r\tRead Count %u\n\r\tRD Busy %x\n\r\tWR BUSY %x\n\r\tEMPTY %x\n\r\tOVF %x\n\n\r", fifo.wr_count, fifo.rd_count, fifo.rd_busy, fifo.wr_busy, fifo.empty, fifo.ovf_err);

		}
		return status;
	}

	if (empty == NEMPTY){
		if(user_tests_fillDDR(address, nBytes, NEMPTY) != XST_SUCCESS){
			xil_printf("[USER_TESTS_FILLFIFO] DDR FILL error before init\n\r");
			return XST_FAILURE;
		}
		Xil_DCacheFlushRange((UINTPTR)address, nBytes);
		while(bytesWritten < nBytes){
			toWrite = nBytes - bytesWritten;
			if(toWrite < 4) toWrite = 4;
			if(user_dma_transfer(DMA_MM2S, address + bytesWritten, toWrite, DMA_NOPTION) != XST_SUCCESS){
				xil_printf("FIFO Fill: DMA Transfer Error\n\r");
				return XST_FAILURE;
			}

			do{
				user_dma_get_status(&flags);
			}while(!flags.transfer_done[DMA_MM2S] && !flags.error[DMA_MM2S]);

			if(flags.error[DMA_MM2S]){
				xil_printf("FIFO Fill: DMA Flag Error\n\r");
				return XST_FAILURE;
			}

			if(user_gpio_get_fifo(&fifo, FIFO_WR_COUNT) != XST_SUCCESS);
			bytesWritten = BYTES_FIFO(fifo.wr_count);
		}
		xil_printf("Bytes written: %u\n\r", bytesWritten);
		user_tests_fillDDR(address, nBytes, EMPTY);
		return XST_SUCCESS;
	}
	xil_printf("Wrong FIFO empty option \r\n");
	return XST_FAILURE;
}


//Analyzes the results from every test
int user_tests_analyse_results(int nTests, Result *results, Result *average, char option){
	int i;
	Result *current;
	char str_number[MAXC];
	int f_clk = ((int)F_CLK_PL) / MEG;
	u32 num_aux[2];

	average->drop_rate = 0;
	average->t_latency = 0;
	average->t_profiling = 0;
	average->t_total = 0;
	average->throughput[0] = 0;
	average->throughput[1] = 0;
	average->n_configs_dma = 0;
	average->dropped_bytes = 0;
	average->n_ovf_fifo = 0;



	for(i = 0; i < nTests; i++){
		current = &(results[i]);

		if (option & RESULTS_VISUALIZE){
			xil_printf("\t---- Sub-test [%d] ----\n\r", i);
			xil_printf("\t\tClock Frequency %dMHz\n\r", f_clk);
			if(option & VISUALIZE_LATENCY){
				xil_printf("\t\tLatency: %u CLK\n\r"			, current->t_latency);
			}else{
				xil_printf("\t\tTotal time: %u CLK\n\r"			, current->t_total);
				xil_printf("\t\tTotal Transfer: %u Bytes\n\r"			, current->bytes_transferred);
				xil_printf("\t\tDropped Bytes: %u \n\r", current->dropped_bytes);
				xil_printf("\t\tDetected Overflows: %u \n\r", current->n_ovf_fifo);
				xil_printf("\t\tAverage Configuration time: %u CLK\n\r"	, current->t_profiling/current->n_configs_dma, str_number);
				xil_printf("\t\tDMA configurations: %u\r\n", current->n_configs_dma);
			}
		}
		if(nTests > 1){
			if(option & VISUALIZE_LATENCY){
				average->t_latency   	+= (current->t_latency   /nTests);
			}else {
				num_aux[0]  			= 	(current->throughput[0] * DP_THRP / nTests) / DP_THRP;
				num_aux[1]  			= 	(current->throughput[0] * DP_THRP / nTests) % DP_THRP;
				num_aux[1]   			+= 	current->throughput[1] / nTests;
				num_aux[1]				+= 	average->throughput[1];
				num_aux[0]				+= 	(num_aux[1] / (10 * DP_THRP));
				average->throughput[1]= 	num_aux[1];
				average->throughput[0]+=  num_aux[0];
				average->drop_rate   	+= current->drop_rate;
				average->n_ovf_fifo		+= (current->n_ovf_fifo/nTests);
				average->dropped_bytes	+= (current ->dropped_bytes/nTests);
				average->t_total 	   	+= (current->t_total     /nTests);
				average->t_profiling 	+= ((current->t_profiling/current->n_configs_dma) /nTests);
				average->n_configs_dma 	+= (current->n_configs_dma/nTests);
			}
		}else if(nTests > 0){
			if(option & VISUALIZE_LATENCY){
				average->t_latency   	+= (current->t_latency   /nTests);
			}else {
				average->throughput[1]= current->throughput[1];
				average->throughput[0]= current->throughput[0];
				average->drop_rate = 	current->drop_rate;
				average->t_total = current->t_total;
				average->dropped_bytes = current->dropped_bytes;
				average->n_ovf_fifo = current->n_ovf_fifo;
				average->t_profiling = (current->t_profiling/current->n_configs_dma);
				average->n_configs_dma = current->n_configs_dma;
			}
		}else{
			return XST_FAILURE;
		}
	}


	average->drop_rate = (average->drop_rate)/nTests;





	if ((option & AVERAGE_VISUALIZE) && TESTS_S2MM > 1){
		xil_printf("\n\n\t----	Averages	----\n\r");
		current = average;
		xil_printf("\t\tClock Frequency %dMHz\n\r", f_clk);
		if(option & VISUALIZE_LATENCY){
			xil_printf("\t\tLatency: %u CLK\n\r"			, current->t_latency);
		}else{
			xil_printf("\t\tTotal time: %u CLK\n\r"			, current->t_total);
			xil_printf("\t\tDropped Bytes: %u \n\r"	, current->dropped_bytes);
			xil_printf("\t\tDetected Overflows: %u \n\r", current->n_ovf_fifo);
			xil_printf("\t\tProfiling: %u CLK\n\r"	, current->t_profiling, str_number);
			xil_printf("\t\tDMA configurations: %u\r\n", current->n_configs_dma);
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



