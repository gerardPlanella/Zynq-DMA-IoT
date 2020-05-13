/*
 * user_gpio.h
 *
 *  Created on: Mar 6, 2020
 *      Author: Gerard Planella
 */

#ifndef SRC_USER_GPIO_H_
#define SRC_USER_GPIO_H_

#include "xgpio.h"
#include "xparameters.h"
#include "xstatus.h"
#include "xil_types.h"

#define GPIO_INSTANCES 4

//Get FIFO options
#define FIFO_RD_BUSY 0x1
#define FIFO_WR_BUSY 0x2
#define FIFO_OVF 0x4
#define FIFO_UDF 0x8
#define FIFO_DATA_COUNT 0x10
#define FIFO_ALL 0xFF


//LED bits and options
#define LED_R 0x01
#define LED_G 0x02
#define LED_B 0x04
#define LED_OFF 0x00

//GPIO bits for signals
#define COUNTER_1_CE 0x1
#define COUNTER_1_SR 0x2
#define COUNTER_2_CE 0x4
#define COUNTER_2_SR 0x8
#define FIFO_RESET 0x10

//FIFO full and empty bit positions in GPIO id = FIFO_RESET_DEVICE_ID
#define FIFO_RD_BUSY_BIT 0x1
#define FIFO_WR_BUSY_BIT 0x2
#define FIFO_UDF_BIT 0x4
#define FIFO_OVF_BIT 0x8


//GPIO device IDs
#define FIFO_DATA_DEVICE_ID 	XPAR_AXI_GPIO_0_DEVICE_ID
#define COUNTER_VALUE_ID 		XPAR_AXI_GPIO_1_DEVICE_ID
#define FIFO_SIGNALS_DEVICE_ID 	XPAR_AXI_GPIO_2_DEVICE_ID
#define COUNTER_SIGNALS_ID		XPAR_AXI_GPIO_2_DEVICE_ID
#define FIFO_RESET_DEVICE_ID 	XPAR_AXI_GPIO_2_DEVICE_ID
#define LED_DEVICE_ID 			XPAR_AXI_GPIO_3_DEVICE_ID


//Set counter options
#define COUNTER_NOPTION 0x00
#define COUNTER_ENABLE 0x01
#define COUNTER_SRESET 0x02

#define FIFO_RESET_DELAY 10000

#define N_COUNTERS 2
#define DELAY_TESTS 20

typedef struct{
	char rd_busy;
	char wr_busy;
	char ovf_err; //Underflow error
	char udf_err; //Overflow error1
	u32 data_count;
}FifoParams;


//Initializes a GPIO controller for each GPIO instance
int user_gpio_init(void);
//Initializes Counter Delays and averages 'delay_tests' times
int user_gpio_counters_init(int delay_tests);
u32 test_counter_delay(char counter, int N);
//Reads from a specific channel in a GPIO with id = device_id
int user_gpio_read(u16 device_id, char channel, u32* data);
//Writes "data" to a specific channel in a GPIO with id = device_id
int user_gpio_write(u16 device_id, char channel, u32 data);
//Reads bits from gpio using bitMask
int user_gpio_read_bit(u16 device_id, char channel, u32 bitMask, char* value);
//Sets bits to value using bitMask to select them
int user_gpio_write_bit(u16 device_id, char channel, u32 bitMask, char value);
//Get the count value for counters 1 or 2
int user_gpio_get_count(u32 *count, char counter);
//Set the Enable and reset values for counters 1 or 2
int user_gpio_set_counter(char counter, char request);

int user_gpio_set_led(char led, char request);
/*
 * Reads the FIFO parameters:
 * e.g- user_gpio_get_fifo(&fifo, FIFO_WRITE_CNT | FIFO_FULL);
 */
int user_gpio_get_fifo(FifoParams *fifo, char request);

//Resets the FIFO
int user_gpio_reset_fifo(void);






#endif /* SRC_USER_GPIO_H_ */
