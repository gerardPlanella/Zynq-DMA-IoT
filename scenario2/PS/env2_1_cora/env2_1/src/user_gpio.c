/*
 * user_gpio.c
 *
 *  Created on: Mar 6, 2020
 *      Author: Gerard Planella
 */

#include "user_gpio.h"

static XGpio gpio[GPIO_INSTANCES];
//Mirror variable for XPAR_GPIO_1_DEVICE_ID Channel 1, the only GPIO we write to
static u32 gpioStatus;
//Delays for the counters used
static u32 counterDelays[N_COUNTERS];

//Initializes a GPIO controller for each GPIO instance
int user_gpio_init(void){
	int i, status = 0;


	status = XGpio_Initialize(&gpio[(int)COUNTER_VALUE_ID], COUNTER_VALUE_ID );
	if (status != XST_SUCCESS) {
	  return XST_FAILURE;
	}

	/* Set the direction for all signals to be inputs */
	XGpio_SetDataDirection(&gpio[(int)COUNTER_VALUE_ID], 1, 0xFFFFFFFF);

	status = XGpio_Initialize(&gpio[(int)COUNTER_SIGNALS_ID], COUNTER_SIGNALS_ID );
	if (status != XST_SUCCESS) {
	  return XST_FAILURE;
	}

	/* Set the direction for all signals to be inputs */
	XGpio_SetDataDirection(&gpio[(int)COUNTER_SIGNALS_ID], 1, 0xFFFFFFFF);


	gpioStatus = 0x0;


	status = XGpio_Initialize(&gpio[(int)LED_DEVICE_ID], LED_DEVICE_ID);
	if (status != XST_SUCCESS) {
	  return XST_FAILURE;
	}

	/* Set the direction for all signals to be outputs */
	XGpio_SetDataDirection(&gpio[(int)LED_DEVICE_ID], 1, 0);



	for(i = 0; i < N_COUNTERS; i++) counterDelays[i] = 0;
	if(user_gpio_counters_init(DELAY_TESTS) != XST_SUCCESS) return XST_FAILURE;

	return XST_SUCCESS;
}

//Initializes Counter Delays
int user_gpio_counters_init(int delay_tests){
	int i;

	for(i = 0; i < N_COUNTERS; i++){
		counterDelays[i] = test_counter_delay((char)(i + 1), delay_tests);
		if(counterDelays[i] == 0) return XST_FAILURE;
	}
	return XST_SUCCESS;
}

u32 test_counter_delay(char counter, int N){
	int j;
	u32 count;
	u32 average = 0;

	for (j = 0; j < N; j++){
		count = 0;
		user_gpio_set_counter(counter, COUNTER_SRESET);
		user_gpio_set_counter(counter, COUNTER_ENABLE);
		user_gpio_set_counter(counter, COUNTER_NOPTION);
		user_gpio_get_count(&count, counter);
		average+=count;
	}
	average = average / N;
	xil_printf("Counter %d average delay: %u\n\r", (int)counter, (u32)(average));
	return average;
}

//Returns the pointer to the correct GPIO controller given a device_id
XGpio * get_gpio(u16 device_id){
	return &gpio[(int)device_id];
}

//Reads from a specific channel in a GPIO with id = device_id
int user_gpio_read(u16 device_id, char channel, u32* data){
	XGpio *gpio = get_gpio(device_id);

	//We check that the gpio has been initialized
	if (gpio->IsReady == 0){
		xil_printf("GPIO not initialized\r\n");
		return XST_FAILURE;
	}

	 /* Read the state of the data so that it can be  verified */
	 *data = XGpio_DiscreteRead(gpio, channel);

	 return XST_SUCCESS;


}

int user_gpio_read_bit(u16 device_id, char channel, u32 bitMask, char* value){
	u32 data;

	if(user_gpio_read(device_id, channel, &data) != XST_SUCCESS) return XST_FAILURE;

	*value = 0x0;
	if(data & bitMask) *value = 0x1;

	 return XST_SUCCESS;
}

int user_gpio_write_bit(u16 device_id, char channel, u32 bitMask, char value){
	u32 data = gpioStatus;

	if(channel == 1 && device_id == COUNTER_SIGNALS_ID){
		if(value){
			data |= bitMask;
		}else{
			data &= (~bitMask);
		}
		//This function updates gpioStatus
		if(user_gpio_write(device_id, channel, data)!= XST_SUCCESS) return XST_FAILURE;

	}else{
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

//Writes "data" to a specific channel in a GPIO with id = device_id
int user_gpio_write(u16 device_id, char channel, u32 data){
	XGpio *gpio = get_gpio(device_id);

	//We check that the GPIO has been initialized
	if (gpio->IsReady == 0){
		xil_printf("GPIO not initialized\r\n");
		return XST_FAILURE;
	}

	XGpio_DiscreteWrite(gpio, channel, data);
	if(channel == 1 && device_id == COUNTER_SIGNALS_ID) gpioStatus = data;

	 return XST_SUCCESS;
}

//Get the count value for counters 1 or 2
int user_gpio_get_count(u32 *count, char counter){
	u32 data;
	int status;

	status = user_gpio_read(COUNTER_VALUE_ID, counter, &data);
	if(status != XST_SUCCESS){
		return 0;
	}
	if(data < counterDelays[(int)(counter - 1)]) return 0;
	data-=counterDelays[(int)(counter - 1)];
	*count = data;

	return XST_SUCCESS;
}

//Set the Enable and reset values for counters 1 or 2
int user_gpio_set_counter(char counter, char request){
	u16 device_id = COUNTER_SIGNALS_ID;
	u32 enable = 0x1;
	u32 reset = 0x2;

	if(counter == 2){
		enable = enable << 2;
		reset = reset << 2;
	}
	/*
	 * We divide the assignment in two calls as SReset has more priority than CE,
	 * if we want to reset and then enable using only one call to this function we
	 * have an uncertainty of what will happen to the counter.
	 */
	user_gpio_write_bit(device_id, 1, reset, request & COUNTER_SRESET);
	user_gpio_write_bit(device_id, 1, enable, request & COUNTER_ENABLE);


	return XST_SUCCESS;
}

int user_gpio_set_led(char led, char request){
	u16 device_id  = LED_DEVICE_ID;
	u32 bitMask = 0x0;

	XGpio *gpio = get_gpio(device_id);

	//We check that the GPIO has been initialized
	if (gpio->IsReady == 0){
		xil_printf("GPIO not initialized\r\n");
		return XST_FAILURE;
	}

	bitMask = (u32)(request & 0x7) ;
	bitMask = led? ((bitMask << 3) & 0x38) : bitMask;


	XGpio_DiscreteWrite(gpio, 1, bitMask);


	return XST_SUCCESS;
}
