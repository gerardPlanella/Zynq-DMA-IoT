/*
 * user_gpio.c
 *
 *  Created on: Mar 6, 2020
 *      Author: Gerard Planella
 */

#include "user_gpio.h"

static XGpio gpio[GPIO_INSTANCES];
//Mirror variable for XPAR_GPIO_2_DEVICE_ID Channel 1, the only GPIO we write to
static u32 gpioStatus;
//Delays for the counters used
static u32 counterDelays[N_COUNTERS];

//Initializes a GPIO controller for each GPIO instance
int user_gpio_init(void){
	int i, status = 0;


	status = XGpio_Initialize(&gpio[(int)FIFO_DATA_DEVICE_ID], FIFO_DATA_DEVICE_ID );
	if (status != XST_SUCCESS) {
		xil_printf("NOO");
	  return XST_FAILURE;
	}

	/* Set the direction for all signals to be inputs */
	XGpio_SetDataDirection(&gpio[(int)FIFO_DATA_DEVICE_ID], 1, 0xFFFFFFFF);

	/* Set the direction for all signals to be inputs */
	XGpio_SetDataDirection(&gpio[(int)FIFO_DATA_DEVICE_ID], 2, 0xFFFFFFFF);

	status = XGpio_Initialize(&gpio[(int)COUNTER_VALUE_ID], COUNTER_VALUE_ID);
	if (status != XST_SUCCESS) {
	  return XST_FAILURE;
	}

	/* Set the direction for all signals to be inputs */
	XGpio_SetDataDirection(&gpio[(int)COUNTER_VALUE_ID], 1, 0xFFFFFFFF);
	XGpio_SetDataDirection(&gpio[(int)COUNTER_VALUE_ID], 2, 0xFFFFFFFF);

	status = XGpio_Initialize(&gpio[(int)FIFO_SIGNALS_DEVICE_ID], FIFO_SIGNALS_DEVICE_ID);
	if (status != XST_SUCCESS) {
	  return XST_FAILURE;
	}

	XGpio_SetDataDirection(&gpio[(int)FIFO_SIGNALS_DEVICE_ID], 1, 0);
	XGpio_SetDataDirection(&gpio[(int)FIFO_SIGNALS_DEVICE_ID], 2, 0xFFFFFFFF);


	status = XGpio_Initialize(&gpio[(int)LED_DEVICE_ID], LED_DEVICE_ID);
	if (status != XST_SUCCESS) {
	  return XST_FAILURE;
	}

	/* Set the direction for all signals to be outputs */
	XGpio_SetDataDirection(&gpio[(int)LED_DEVICE_ID], 1, 0);

//	status = XGpio_Initialize(&gpio[(int)OVF_CNT_DEVICE_ID], OVF_CNT_DEVICE_ID );
//	if (status != XST_SUCCESS) {
//	  return XST_FAILURE;
//	}
//
//	/* Set the direction for all signals to be inputs */
//	XGpio_SetDataDirection(&gpio[(int)OVF_CNT_DEVICE_ID], 1, 0xFFFFFFFF);

	status = XGpio_Initialize(&gpio[(int)INTERRUPT_DEVICE_ID], INTERRUPT_DEVICE_ID);
	if (status != XST_SUCCESS) {
	  return XST_FAILURE;
	}

	/* Set the direction for all signals to be inputs */
	XGpio_SetDataDirection(&gpio[(int)INTERRUPT_DEVICE_ID], 1, 0xFFFFFFFF);


	gpioStatus = 0x0;

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
//Tests the delay time when operating the counters
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

	if(value){
		data |= bitMask;
	}else{
		data &= (~bitMask);
	}

	//This function updates gpioStatus
	if(user_gpio_write(device_id, channel, data)!= XST_SUCCESS) return XST_FAILURE;

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
	if(channel == 1 && device_id == FIFO_SIGNALS_DEVICE_ID) gpioStatus = data;

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


/*
 * Reads the FIFO write or read count
 */
int user_gpio_get_fifo(FifoParams *fifo, char request){

	if((request & FIFO_RD_BUSY)){
		if(user_gpio_read_bit(FIFO_SIGNALS_DEVICE_ID, 2, (u32)FIFO_RD_BUSY_BIT, &(fifo->rd_busy)) != XST_SUCCESS) return XST_FAILURE;
	}

	if ((request & FIFO_WR_BUSY)){
		if(user_gpio_read_bit(FIFO_SIGNALS_DEVICE_ID, 2, (u32)FIFO_WR_BUSY_BIT, &(fifo->wr_busy)) != XST_SUCCESS) return XST_FAILURE;
	}

	if ((request & FIFO_OVF)){
		if(user_gpio_read_bit(FIFO_SIGNALS_DEVICE_ID, 2, (u32)FIFO_OVF_BIT, &(fifo->ovf_err)) != XST_SUCCESS) return XST_FAILURE;
	}
	if ((request & FIFO_EMPTY)){
		if(user_gpio_read_bit(FIFO_SIGNALS_DEVICE_ID, 2, (u32)FIFO_EMPTY_BIT, &(fifo->empty)) != XST_SUCCESS) return XST_FAILURE;
	}
	if ((request & FIFO_RD_COUNT)){
		user_gpio_read(FIFO_DATA_DEVICE_ID, 1, &(fifo->rd_count));
		fifo->rd_count &= 0xFFF;
	}
	if ((request & FIFO_WR_COUNT)){
		user_gpio_read(FIFO_DATA_DEVICE_ID, 2, &(fifo->wr_count));
		fifo->wr_count &= 0xFFF;
	}


	return XST_SUCCESS;
}
//Resets the FIFO and Halts execution until Reset is successful
int user_gpio_reset_fifo(void){
	FifoParams fifo;
	int i;

	if(user_gpio_write_bit(FIFO_RESET_DEVICE_ID, 1, FIFO_RESET, 0) != XST_SUCCESS) return XST_FAILURE;

	for(i = 0; i < 10000; i++);
	if(user_gpio_write_bit(FIFO_RESET_DEVICE_ID, 1, FIFO_RESET, 1) != XST_SUCCESS) return XST_FAILURE;

	if(user_gpio_get_fifo(&fifo, FIFO_RD_COUNT | FIFO_WR_COUNT) != XST_SUCCESS) return XST_FAILURE;


	return XST_SUCCESS;
}

char user_gpio_fifo_isEmpty(void){
	u32 rd, wr;

	user_gpio_read(FIFO_DATA_DEVICE_ID, 1, &rd);
	rd &= 0xFFF;

	user_gpio_read(FIFO_DATA_DEVICE_ID, 2, &wr);
	wr &= 0xFFF;

	return (rd >= wr);
}

//Checks fifo for the programmable empty flag
char user_gpio_fifo_isProgEmpty(int nChannel){
	char empty = 1;

	if(user_gpio_read_bit(FIFO_SIGNALS_DEVICE_ID, 2, (u32)FIFO_EMPTY_BIT, &empty) != XST_SUCCESS) return XST_FAILURE;

	return empty;
}

char user_gpio_fifo_is_ovf(void){
	u32 reg;

	user_gpio_read(FIFO_SIGNALS_DEVICE_ID, 2, &reg);
	return (reg & FIFO_OVF_BIT);
}


//Enables GPIO Interrupts
void user_gpio_interrupts_enable(char enable, char channel1, char channel2){
	u32 mask = 0;

	if(channel1 == GPIO_ENABLE){
		mask |= XGPIO_IR_CH1_MASK;
	}
	if(channel1 == GPIO_ENABLE){
		mask |= XGPIO_IR_CH2_MASK;
	}

	if(enable == GPIO_ENABLE){
		XGpio_InterruptEnable(&(gpio[INTERRUPT_DEVICE_ID]), mask);
		XGpio_InterruptGlobalEnable(&(gpio[INTERRUPT_DEVICE_ID]));
	}else{
		XGpio_InterruptDisable(&(gpio[INTERRUPT_DEVICE_ID]), mask);
		XGpio_InterruptGlobalDisable(&(gpio[INTERRUPT_DEVICE_ID]));
	}

}

//Performs Hardware reset to the traffic generator
int user_gpio_reset_trafgen(void){
	if(user_gpio_write_bit(TRAFGEN_RESET_DEVICE_ID, 1, TG_RESET, 0) != XST_SUCCESS) return XST_FAILURE;
	if(user_gpio_write_bit(TRAFGEN_RESET_DEVICE_ID, 1, TG_RESET, 1) != XST_SUCCESS) return XST_FAILURE;

	return XST_SUCCESS;
}

int user_gpio_get_cnt_ovf(u32 *ovf_cnt){
	return user_gpio_read(OVF_CNT_DEVICE_ID, 1, ovf_cnt);
}

int user_gpio_set_cnt_ovf(char request){
	u32 enable = COUNTER_OVF_CE;
	u32 reset = COUNTER_OVF_SR;

	user_gpio_write_bit(COUNTER_SIGNALS_ID , 1, reset, request & COUNTER_SRESET);
	user_gpio_write_bit(COUNTER_SIGNALS_ID , 1, enable, request & COUNTER_ENABLE);

	return XST_SUCCESS;
}

