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
//#include "platform.h"
#include "xil_types.h"

/*---- User Libraries ---- */
#include "user_gpio.h"
#include "user_cdma.h"

#include "user_tests.h"

//Configures the system, peripherals and interrupts
int system_setup(void);


int peripheral_test(u32 address);

int main()
{
	int status, i, j;
	Result results[TESTS][SUB_TESTS];
	Result average[SUB_TESTS];

	//Check how much the time increases when disabling the use of the Cache!!
	//Xil_DCacheDisable();


	xil_printf("\r\n\n---- Entered main() ----\r\n\n");

	for(i = 0; i <= TESTS; i++){
		for (j = 0; j < SUB_TESTS; j++){
			if(i == TESTS){
				average[j].bytes_transferred = average[j].drop_rate = average[j].t_latency = average[j].t_profiling =
						average[j].t_total = average[j].throughput[0] = average[j].throughput[1] = 0;
			}else{
				results[i][j].bytes_transferred = results[i][j].drop_rate = results[i][j].t_latency =
						results[i][j].t_profiling = results[i][j].t_total = results[i][j].throughput[0] =
								results[i][j].throughput[1] = 0;
			}
		}
	}

	xil_printf("\n\n---- System Setup Start ----\r\n");
	//Initialize the system
	status = system_setup();
	if (status == XST_FAILURE){
		xil_printf("System setup failed\n\r");
		return XST_FAILURE;
	}
	xil_printf("\n\n---- System Setup End ----\r\n");


	xil_printf("\n\n---- Peripheral Test Start ----\r\n");

	if(peripheral_test(DDR_ADDR_1) != XST_SUCCESS) return XST_FAILURE;
	if(peripheral_test(BRAM_ADDR_1) != XST_SUCCESS) return XST_FAILURE;
	if(peripheral_test(DDR_ADDR_2) != XST_SUCCESS) return XST_FAILURE;
	if(peripheral_test(BRAM_ADDR_2) != XST_SUCCESS) return XST_FAILURE;

	xil_printf("\n\n---- Peripheral Test End ----\r\n\n");

	//Working LED
	user_gpio_set_led(1, LED_B);

	//Start Test
    xil_printf("\n\n----	Tests Start	---\n\n\r");


    for(i = 0; i < TESTS; i++){
    	status = user_tests_1(DDR_DDR, NBYTES, &(results[i][DDR_DDR]), DDR_ADDR_1, DDR_ADDR_2);
    	if (status != XST_SUCCESS){
			xil_printf("Error performing test number %d: DDR2DDR\n\r", i);
			return XST_FAILURE;
		}
    	status = user_tests_1(BRAM_BRAM, NBYTES, &(results[i][BRAM_BRAM]), BRAM_ADDR_1, BRAM_ADDR_2);
		if (status != XST_SUCCESS){
			xil_printf("Error performing test number %d: BRAM2BRAM\n\r", i);
			return XST_FAILURE;
		}
		status = user_tests_1(DDR_BRAM, NBYTES, &(results[i][DDR_BRAM]), DDR_ADDR_1, BRAM_ADDR_1);
		if (status != XST_SUCCESS){
			xil_printf("Error performing test number %d: DDR2BRAM\n\r", i);
			return XST_FAILURE;
		}
		status = user_tests_1(BRAM_DDR, NBYTES, &(results[i][BRAM_DDR]), BRAM_ADDR_1, DDR_ADDR_1);
		if (status != XST_SUCCESS){
			xil_printf("Error performing test number %d: BRAM2DDR\n\r", i);
			return XST_FAILURE;
		}

    }

    //End Test
    xil_printf("\n\n----	Tests End	---\n\n\r");

    xil_printf("\n\n----	Result analysis Start	---\n\n\r");

    status = user_tests_analyse_results(results, average ,RESULTS_VISUALIZE);
    if (status != XST_SUCCESS){
		xil_printf("Result analysis failed\n");
		return XST_FAILURE;
	}
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
    status = user_axicdma_init();
    if (status != XST_SUCCESS){
		xil_printf("CDMA initialization failed\n\r");
		return XST_FAILURE;
	}

    return XST_SUCCESS;
}

int peripheral_test(u32 address){
	xil_printf("Filling Memory...\r\n");
	if(user_tests_fillMemory(address, NBYTES, NEMPTY) != XST_SUCCESS) return XST_FAILURE;
	xil_printf("Memory Filled\r\n");
	return XST_SUCCESS;
}
