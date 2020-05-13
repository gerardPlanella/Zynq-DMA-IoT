/*
 * user_trafgen.c
 *
 *  Created on: Mar 31, 2020
 *      Author: gerard
 */

#include "user_trafgen.h"

XTrafGen trafgen;

/*
 * Initialises the Traffic generator to Streaming and Master Only
 */
int user_trafgen_init(void){
	XTrafGen_Config *config;
	int status;

	user_trafgen_reset();

	config = XTrafGen_LookupConfig(TG_DEV_ID);
	if(!config){
		xil_printf("[TRAF_GEN] Error finding configuration\r\n");
		return XST_FAILURE;
	}

	status = XTrafGen_CfgInitialize(&trafgen, config, config->BaseAddress);
	if(status != XST_SUCCESS){
		xil_printf("[TRAF_GEN] Initialisation failed\r\n");
		return XST_FAILURE;
	}

	if(trafgen.OperatingMode != XTG_MODE_STREAMING){
		xil_printf("[TRAF_GEN] Invalid operating mode\r\n");
		return XST_FAILURE;
	}

	//Enable the Error Status bit
	XTrafGen_WriteReg(trafgen.Config.BaseAddress, 0x74, 0x1);

	return XST_SUCCESS;
}

/*
 * Sets The Traffic Generator Mode
 * @params: mode: MODE_X | SUBMODE_X | OPTION_X
 *
 * Except for MODE = MODE_RAND, mode: MODE_RAND | SUBMODE_X (You can use two submodes for the RAND)
 */
int user_trafgen_set_mode(u32 nBytes, u16 mode, u32 length, u16 delay, u32 *bytes_configured){
	char tgmode 	=(char)(mode & 0xF);

	user_trafgen_enable(TG_DISABLE);
	trafgen_reset_rand();

	switch(tgmode){
		case MODE_PEAKS:
			if(trafgen_mode_peaks(nBytes, mode, length, delay, bytes_configured) != XST_SUCCESS){
				xil_printf("[TG] Peaks mode setup failed\r\n");
				return XST_FAILURE;
			}
			break;
		case MODE_CONST:
			if(trafgen_mode_peaks(nBytes, mode | SUBMODE_DELAY, length, TG_DELAY_CONST, bytes_configured) != XST_SUCCESS){
				xil_printf("[TG] Constant mode setup failed\r\n");
				return XST_FAILURE;
			}
			break;
		case MODE_RAND:
			if(trafgen_mode_rand(nBytes, mode, length, delay, bytes_configured) != XST_SUCCESS){
				xil_printf("[TG] Random mode setup failed\r\n");
				return XST_FAILURE;
			}
			break;

		default:
			return XST_FAILURE;
	}
	return XST_SUCCESS;
}

int trafgen_mode_peaks(u32 nBytes, char mode, u32 length, u16 delay, u32 *bytes_configured){
	u16 length_opt = 0;
	u32 delay_opt = 0;
	u16 trans_cnt = 0;
	u64 calculated = 0;

	if((mode & SUBMODE_LEN)){
		length_opt = length;
	}

	if((mode & SUBMODE_DELAY)){
		delay_opt = delay;
	}

	trans_cnt = (nBytes / (length_opt * 4));
	calculated = (trans_cnt * length * 4);

	xil_printf("Length %u, TCNT %u, Wanted Bytes %u Total Bytes %u\n\r",length_opt,  trans_cnt, nBytes, calculated);

	if(trafgen_set_length(length_opt) != XST_SUCCESS){
		xil_printf("[TG SET MODE] Length %u exceeds limit of 3 bytes\r\n", length);
		return XST_FAILURE;
	}

	trafgen_set_delay(delay_opt);
	trafgen_set_tcnt(trans_cnt);

	*bytes_configured = (trans_cnt * length) << (TG_DATA_WIDTH/2);

	return XST_SUCCESS;
}



int trafgen_mode_rand(u32 nBytes, char mode, u32 length, u16 delay, u32 *bytes_configured){
	u32 reg;
	u32 tcnt;

	reg = XTrafGen_ReadReg(trafgen.Config.BaseAddress,XTG_STREAM_CFG_OFFSET);

	if((mode & SUBMODE_LEN)){
		reg |= XTG_STREAM_CFG_RANDL_MASK;
		tcnt = 0;//Infinite transfers
		*bytes_configured = nBytes;
	}else{
		if(trafgen_set_length(length) != XST_SUCCESS){
			xil_printf("[TG SET MODE] Length %u exceeds limit of 3 bytes\r\n", length);
		}
		tcnt = (nBytes / (length << (TG_DATA_WIDTH/2)));
		*bytes_configured = (tcnt * length) << (TG_DATA_WIDTH/2);
	}
	if((mode & SUBMODE_DELAY)){
		reg |= XTG_STREAM_CFG_RANDLY_MASK;
	}else{
		trafgen_set_delay( delay);
	}

	XTrafGen_WriteReg(trafgen.Config.BaseAddress, XTG_STREAM_CFG_OFFSET, reg);
	trafgen_set_tcnt(tcnt);

	reg = XTrafGen_GetStreamingTransLen((&trafgen));

	return XST_SUCCESS;
}


//Enables or disables the traffic generator
int user_trafgen_enable(char enable){
	if(enable == TG_ENABLE){
		XTrafGen_StreamEnable(&trafgen);
		return XST_SUCCESS;
	}else if(enable == TG_DISABLE){
		XTrafGen_StreamDisable(&trafgen);
		return XST_SUCCESS;
	}
	return XST_FAILURE;
}

//Fills the TrafGenState structure with the options demanded in the option variable
int user_trafgen_get_state(char option, TrafGenState *state){
	if((option & TG_DONE)){
		state->done = user_trafgen_isDone();
	}
	if((option & TG_COUNT)){
		state->count = XTrafGen_ReadReg(trafgen.Config.BaseAddress, 0x3C);
	}
	if((option & TG_ERR_COUNT)){
		state->error_count = (XTrafGen_ReadReg(trafgen.Config.BaseAddress, 0x7C) & 0xFFFF);
	}
	if((option & TG_ERR_STATUS)){
		state->error_status = (char)(XTrafGen_ReadReg(trafgen.Config.BaseAddress, 0x70) & 0x1);
	}

	return XST_SUCCESS;
}

//Checks if the traffic generator is done
char user_trafgen_isDone(void){
	u32 reg;

	reg = (XTrafGen_ReadReg(trafgen.Config.BaseAddress,XTG_STREAM_CNTL_OFFSET) & XTG_STREAM_CNTL_TD_MASK);
	return (char)(reg >> (XTG_STREAM_CNTL_TD_SHIFT));
}

//Checks for errors and returns if there was any error
int trafgen_error_check(void){
	char error = (char)(XTrafGen_ReadReg(trafgen.Config.BaseAddress, 0x70) & 0x1);
	if(error) return TG_KO;
	return TG_OK;
}

void trafgen_reset_rand(void){
	u32 reg;

	reg = XTrafGen_ReadReg(trafgen.Config.BaseAddress, XTG_STREAM_CFG_OFFSET);
	reg &=( ~(XTG_STREAM_CFG_RANDL_MASK | XTG_STREAM_CFG_RANDLY_MASK));

	XTrafGen_WriteReg(trafgen.Config.BaseAddress, XTG_STREAM_CFG_OFFSET, reg);
}

int trafgen_set_length(u32 length_in){
	u32 reg, upper, lower, length;

	//Traffic generator lengths start at 0 = 1 beat
	if(length_in) length = length_in - 1;

	//Length Register limit of 3 bytes
	if(length & 0xFF000000) return XST_FAILURE;

	upper = (length >> 16) & 0xFF;
	lower = length & 0xFFFF;

	reg = XTrafGen_ReadReg(trafgen.Config.BaseAddress, XTG_STREAM_TL_OFFSET);
	reg &= (~lower);
	reg |= lower;
	XTrafGen_WriteReg(trafgen.Config.BaseAddress, XTG_STREAM_TL_OFFSET, reg);

	//We now write to the Extended Length register
	reg = XTrafGen_ReadReg(trafgen.Config.BaseAddress, 0x50);
	reg &= (~upper);
	reg |= upper;
	XTrafGen_WriteReg(trafgen.Config.BaseAddress, 0x50, reg);

	return XST_SUCCESS;
}


void trafgen_set_delay(u16 delay){
	u32 reg;

	reg = XTrafGen_ReadReg(trafgen.Config.BaseAddress, XTG_STREAM_CFG_OFFSET);
	reg &= (~XTG_STREAM_CFG_PDLY_MASK);
	reg |=((delay << XTG_STREAM_CFG_PDLY_SHIFT) & XTG_STREAM_CFG_PDLY_MASK);

	XTrafGen_WriteReg(trafgen.Config.BaseAddress, XTG_STREAM_CFG_OFFSET, reg);
}

void trafgen_set_tcnt(u16 tcnt){
	u32 reg;

	reg = XTrafGen_ReadReg(trafgen.Config.BaseAddress, XTG_STREAM_TL_OFFSET);
	reg &= (~XTG_STREAM_TL_TCNT_MASK);
	reg |= ((tcnt << XTG_STREAM_TL_TCNT_SHIFT) & XTG_STREAM_TL_TCNT_MASK);

	XTrafGen_WriteReg(trafgen.Config.BaseAddress, XTG_STREAM_TL_OFFSET, reg);
}

//Performs Hardware Reset to the traffic generator
int user_trafgen_reset(void){
	return user_gpio_reset_trafgen();
}
