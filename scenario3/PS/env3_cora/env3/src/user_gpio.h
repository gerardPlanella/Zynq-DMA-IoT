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

#include "user_types.h"

#define GPIO_INSTANCES 5

#define GPIO_FIFO_NCHANNELS 1 //Number of channels for dma interrupts

#define GPIO_ENABLE 1
#define GPIO_DISABLE 0

//Get FIFO options
#define FIFO_RD_BUSY 0x1
#define FIFO_WR_BUSY 0x2
#define FIFO_OVF 0x4
#define FIFO_EMPTY 0x8
#define FIFO_RD_COUNT 0x10
#define FIFO_WR_COUNT 0x20
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
#define TG_RESET 0x20
#define COUNTER_OVF_CE 0x40
#define COUNTER_OVF_SR 0x80

//FIFO full and empty bit positions in GPIO id = FIFO_RESET_DEVICE_ID
#define FIFO_RD_BUSY_BIT 0x1
#define FIFO_WR_BUSY_BIT 0x2
#define FIFO_EMPTY_BIT 0x4
#define FIFO_OVF_BIT 0x8



//GPIO device IDs
#define FIFO_DATA_DEVICE_ID 		XPAR_AXI_GPIO_0_DEVICE_ID
#define COUNTER_VALUE_ID 			XPAR_AXI_GPIO_1_DEVICE_ID
#define FIFO_SIGNALS_DEVICE_ID 		XPAR_AXI_GPIO_2_DEVICE_ID
#define COUNTER_SIGNALS_ID			XPAR_AXI_GPIO_2_DEVICE_ID
#define TRAFFIC_GEN_ERR_DEVICE_ID 	XPAR_AXI_GPIO_2_DEVICE_ID
#define FIFO_RESET_DEVICE_ID 		XPAR_AXI_GPIO_2_DEVICE_ID
#define TRAFGEN_RESET_DEVICE_ID		XPAR_AXI_GPIO_2_DEVICE_ID
#define AXIS_CNT_SIGNALS_DEVICE_ID	XPAR_AXI_GPIO_2_DEVICE_ID
#define LED_DEVICE_ID 				XPAR_AXI_GPIO_3_DEVICE_ID
#define OVF_CNT_DEVICE_ID			XPAR_AXI_GPIO_0_DEVICE_ID //XPAR_AXI_GPIO_0_DEVICE_ID
#define INTERRUPT_DEVICE_ID			XPAR_AXI_GPIO_4_DEVICE_ID

#define FIFO_EMPTY_INT XPAR_FABRIC_AXI_GPIO_4_IP2INTC_IRPT_INTR

//Set counter options
#define COUNTER_NOPTION 0x00
#define COUNTER_ENABLE 0x01
#define COUNTER_SRESET 0x02

#define FIFO_RESET_DELAY 10000

#define N_COUNTERS 2
#define DELAY_TESTS 20


//Initializes a GPIO controller for each GPIO instance
int user_gpio_init(void);
//Enables GPIO Interrupts
void user_gpio_interrupts_enable(char enable, char channel1, char channel2);
//Reads from a specific channel in a GPIO with id = device_id
int user_gpio_read(u16 device_id, char channel, u32* data);
//Writes "data" to a specific channel in a GPIO with id = device_id
int user_gpio_write(u16 device_id, char channel, u32 data);
//Reads bits from gpio using bitMask
int user_gpio_read_bit(u16 device_id, char channel, u32 bitMask, char* value);
//Sets bits to value using bitMask to select them
int user_gpio_write_bit(u16 device_id, char channel, u32 bitMask, char value);
//Allows to control the On-Board RGB LEDs
int user_gpio_set_led(char led, char request);
//Returns the pointer to the correct GPIO controller given a device_id
XGpio * get_gpio(u16 device_id);


/*
 * Counters
 */

//Initializes Counter Delays and averages 'delay_tests' times
int user_gpio_counters_init(int delay_tests);
//Tests the delay time when operating the counters
u32 test_counter_delay(char counter, int N);
//Get the count value for counters 1 or 2
int user_gpio_get_count(u32 *count, char counter);
//Set the Enable and reset values for counters 1 or 2
int user_gpio_set_counter(char counter, char request);

/*
 * FIFO
 */

//Resets the FIFO and Halts execution until Reset is successful
int user_gpio_reset_fifo(void);
/*
 * Reads the FIFO parameters:
 * e.g- user_gpio_get_fifo(&fifo, FIFO_WRITE_CNT | FIFO_FULL);
 */
int user_gpio_get_fifo(FifoParams *fifo, char request);

//Checks if the fifo is empty
char user_gpio_fifo_isEmpty(void);
//Checks fifo for the programmable empty flag
char user_gpio_fifo_isProgEmpty(int nChannel);
//Checks if Overflow bit is active
char user_gpio_fifo_is_ovf(void);


///*
// * FIFO Overflow Counter
// */
int user_gpio_get_cnt_ovf(u32 *ovf_cnt);

int user_gpio_set_cnt_ovf(char request);
/*
 * Traffic Generator
 */
//Performs Hardware reset to the traffic generator
int user_gpio_reset_trafgen(void);









#endif /* SRC_USER_GPIO_H_ */
