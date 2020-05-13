/*H**********************************************************************
* FILENAME :        main.c             DESIGN REF: E1T1
*
* DESCRIPTION :
*       Main file for test in environment 1
*
* AUTHOR :    Gerard Planella        START DATE :    6 Mar 2020
*
*H*/

#include <stdio.h>

#include "xparameters.h"
#include "xdebug.h"
#include "xil_types.h"

/*---- User Libraries ---- */
#include "user_gpio.h"
#include "user_dma.h"

#include "user_tests.h"

//Configures the system, peripherals and interrupts
int system_setup(void);

int test_fifo(u32 address);

int peripheral_test(u32 address);

int main()
{
	int status, i, s2mm_ko = 0;
	Result results_mm2s[TESTS_MM2S];
	Result results_s2mm[TESTS_S2MM];
	Result average[SUB_TESTS];

	//Check how much the time increases when disabling the use of the Cache!!
	//Xil_DCacheDisable();


	xil_printf("\r\n\n---- Entered main() ----\r\n\n");


	xil_printf("\n\n---- System Setup Start ----\r\n");
	//Initialize the system
	status = system_setup();
	if (status == XST_FAILURE){
		xil_printf("System setup failed\n\r");
		return XST_FAILURE;
	}
	xil_printf("\n\n---- System Setup End ----\r\n");


	xil_printf("\n\n---- Peripheral Test Start ----\r\n");

	if(peripheral_test(TEST_TX_ADDR) != XST_SUCCESS) return XST_FAILURE;

	xil_printf("\n\n---- Peripheral Test End ----\r\n\n");

	//Working LED
	user_gpio_set_led(1, LED_B);

	//Start Test
    xil_printf("\n\n----	Tests Start	---\n\n\r");


    for(i = 0; i < TESTS_MM2S; i++){
    	status = user_tests_1(i + 1, NBYTES, &results_mm2s[i], DMA_MM2S, TEST_TX_ADDR);
    	if (status != XST_SUCCESS){
			xil_printf("Error performing test number %d: MM2S\n\r", i);
			return XST_FAILURE;
		}
    }

    i = 0;
    while(i < TESTS_S2MM){
		status = user_tests_1(i + 1, NBYTES, &results_s2mm[i], DMA_S2MM, TEST_RX_ADDR);
		if (status == XST_SUCCESS){
			i++;
		}else{
			s2mm_ko++;
		}
		//Reset DMA
		user_dma_reset();
    }


    //End Test
    xil_printf("\n\n----	Tests End	---\n\n\r");

    xil_printf("\n\n----	Result analysis Start	---\n\n\r");

    status = user_tests_analyse_results(results_s2mm, results_mm2s, average , RESULTS_VISUALIZE);
    if (status != XST_SUCCESS){
		xil_printf("Result analysis failed\n");
		return XST_FAILURE;
	}
    xil_printf("\t\tS2MM DMA Blockages: %d out of %d\n\r", s2mm_ko, (s2mm_ko + TESTS_S2MM));
    xil_printf("\n\n----	Result analysis End	---\n\n\r");

    user_gpio_set_led(0, LED_OFF);
	//cleanup_platform();
	xil_printf("\r\n\n----	Exiting main()	----\r\n");

	return XST_SUCCESS;

}

//Configures the system, peripherals and interrupts
int system_setup(void){
	int status;
	//Initializes UART and enables Caches
    //init_platform();

    //Initialize GPIOs
    status = user_gpio_init();
    if (status != XST_SUCCESS){
		xil_printf("GPIOs initialization failed\n\r");
		return XST_FAILURE;
	}

    //Initialize DMA
    status = user_axidma_init();
    if (status != XST_SUCCESS){
		xil_printf("DMA initialization failed\n\r");
		return XST_FAILURE;
	}

    return XST_SUCCESS;
}

int peripheral_test(u32 address){
	  //Fill DDR for test start

	user_tests_fillDDR(address, NBYTES, EMPTY);

	if(test_fifo(address) != XST_SUCCESS) return XST_FAILURE;

	return XST_SUCCESS;
}


int test_fifo(u32 address){
	FifoParams fifo;

    if(user_tests_fillFIFO(address, NBYTES, EMPTY) != XST_SUCCESS){
			xil_printf("FIFO empty failed \r\n");
			return XST_FAILURE;
	}

    if(user_gpio_get_fifo(&fifo, FIFO_ALL) != XST_SUCCESS){
		xil_printf("[FIFO]: Get FIFO failed\r\n");
		return XST_FAILURE;
	}

    xil_printf("Empty FIFO 1: \n\r\tData Count %u\n\r\tRD Busy %x\n\r\tWR BUSY %x\n\r\tUDF %x\n\r\tOVF %x\n\n\r", fifo.data_count, fifo.rd_busy, fifo.wr_busy, fifo.udf_err, fifo.ovf_err);

    if(user_tests_fillFIFO(address, NBYTES, NEMPTY) != XST_SUCCESS){
    		xil_printf("FIFO full failed \r\n");
    		return XST_FAILURE;
    }

    if(user_gpio_get_fifo(&fifo, FIFO_ALL) != XST_SUCCESS){
   		xil_printf("[FIFO]: Get FIFO failed\r\n");
   		return XST_FAILURE;
   	}

    xil_printf("Full FIFO: \n\r\tData Count %u\n\r\tRD Busy %x\n\r\tWR BUSY %x\n\r\tUDF %x\n\r\tOVF %x\n\n\r", fifo.data_count, fifo.rd_busy, fifo.wr_busy, fifo.udf_err, fifo.ovf_err);

    if(user_tests_fillFIFO(address, NBYTES, EMPTY) != XST_SUCCESS){
			xil_printf("FIFO empty failed \r\n");
			return XST_FAILURE;
	}

    if(user_gpio_get_fifo(&fifo, FIFO_ALL) != XST_SUCCESS){
   		xil_printf("[FIFO]: Get FIFO failed\r\n");
   		return XST_FAILURE;
   	}

    xil_printf("Empty FIFO 2: \n\r\tData Count %u\n\r\tRD Busy %x\n\r\tWR BUSY %x\n\r\tUDF %x\n\r\tOVF %x\n\n\r", fifo.data_count, fifo.rd_busy, fifo.wr_busy, fifo.udf_err, fifo.ovf_err);

    return XST_SUCCESS;
}





