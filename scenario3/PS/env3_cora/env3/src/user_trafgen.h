/*
 * user_trafgen.h
 *
 *  Created on: Mar 31, 2020
 *      Author: gerard
 */

#ifndef SRC_USER_TRAFGEN_H_
#define SRC_USER_TRAFGEN_H_

#include "xparameters.h"
#include "xil_types.h"
#include "xtrafgen.h"
#include "xil_exception.h"

#include "user_gpio.h"


#define TG_DEV_ID XPAR_AXI_TRAFFIC_GEN_0_DEVICE_ID
#define TG_DATA_WIDTH 4


/*
 * Set Mode Constants
 */
#define MODE_PEAKS 	0x1
#define MODE_CONST 	0x2
#define MODE_RAND  	0x4

#define SUBMODE_LEN 	0x10
#define SUBMODE_DELAY 	0x20

/*
 * Get Status Constants
 */

#define TG_DONE 		0x1
#define TG_COUNT 		0x2
#define TG_ERR_COUNT 	0x4
#define TG_ERR_STATUS 	0x8
#define TG_ALL 		0xF

#define TG_ENABLE 	0x1
#define TG_DISABLE 	0x0

#define TG_OK 1
#define TG_KO 0


#define TG_DELAY_CONST 0// 1 Clock Between transfers in constant mode

typedef struct{
	char done;
	u32 count;
	u32 error_count;
	char error_status;
}TrafGenState;

/*
 * Initialises the Traffic generator to Streaming and Master Only
 */
int user_trafgen_init(void);

/*
 * Sets The Traffic Generator Mode
 * @params: mode: MODE_X | SUBMODE_Y | OPTION_Z
 *
 * MODE = MODE_PEAKS, mode: MODE_PEAKS | SUBMODE_LEN and or SUBMODE_DELAY
 * MODE = MODE_RAND, mode: MODE_RAND | SUBMODE_LEN and or SUBMODE_DELAY
 * MODE = MODE_CONST, mode: MODE_CONST
 */
int user_trafgen_set_mode(u32 nBytes, u16 mode, u32 length, u16 delay, u32 *bytes_configured);

//Enables or disables the traffic generator
int user_trafgen_enable(char enable);

//Fills the TrafGenState structure with the options demanded in the option variable
int user_trafgen_get_state(char option, TrafGenState *state);

//Configures the traffic generator to produce bursts of data at regular intervals selected through the mode parameter
int trafgen_mode_peaks(u32 nBytes, char mode, u32 length, u16 delay, u32 *bytes_configured);
//Configures the traffic generator to data bursts of random length, delay, or both.
int trafgen_mode_rand(u32 nBytes, char mode, u32 length, u16 delay, u32 *bytes_configured);
//Resets Random length and delay to 0
void trafgen_reset_rand(void);
//Checks for errors and returns if there was any error
int trafgen_error_check(void);
//Sets transfer length for Traffic Generator, length <= 3 Bytes
int trafgen_set_length(u32 length_in);
//Checks if the traffic generator is done
char user_trafgen_isDone(void);
//Sets the programmable delay between packets for the traffic generator
void trafgen_set_delay(u16 delay);
//Sets the transfer count for the traffic generator
void trafgen_set_tcnt(u16 tcnt);
//Performs Hardware Reset to the traffic generator
int user_trafgen_reset(void);


#endif /* SRC_USER_TRAFGEN_H_ */
