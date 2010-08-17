/* DACmessageAPIstatus.h */
#ifndef _DATA_MESSAGE_API_STATUS_H_
#define _DATA_MESSAGE_API_STATUS_H_

// This file contains subtype values for the 
// DAQ<=>DOM messaging API used by the Data
// Access service.  
//
// March 30, 1999
// Chuck McParland

/* 
   Mods by J. Jacobsen 2004 to add messages to retrieve 
   monitoring messages from domapp.
*/


// message API subtype values. This field is not
// declared as an enum because each service declares
// values in addion to those in MessageAPIstatus.h.
// Since enums are typedefs, odd compiler problems 
// will ensue.  
//
// Note 255 is the maximum value available for this field. 
#define	DATA_ACC_DATA_AVAIL        10
#define	DATA_ACC_GET_DATA          11
#define DATA_ACC_GET_NEXT_MONI_REC 12
#define DATA_ACC_SET_MONI_IVAL     13 /* Set monitoring interval for 
					 hardware and configuration snapshot
					 records */
#define DATA_ACC_SET_ENG_FMT       14

/* #define DATA_ACC_TEST_SW_COMP   15 /\* Inject software compression test pattern data *\/ */

#define DATA_ACC_SET_BASELINE_THRESHOLD 16 
#define DATA_ACC_GET_BASELINE_THRESHOLD 17 

/* #define DATA_ACC_SET_SW_DATA_COMPRESSION 18 */
/* #define DATA_ACC_GET_SW_DATA_COMPRESSION 19 */
/* #define DATA_ACC_SET_SW_DATA_COMPRESSION_FORMAT 20 */
/* #define DATA_ACC_GET_SW_DATA_COMPRESSION_FORMAT 21 */

#define DATA_ACC_RESET_LBM            22
#define DATA_ACC_GET_FB_SERIAL        23 /* out: UB*n serial ID */
#define DATA_ACC_SET_DATA_FORMAT      24
#define DATA_ACC_GET_DATA_FORMAT      25
#define DATA_ACC_SET_COMP_MODE        26
#define DATA_ACC_GET_COMP_MODE        27
#define DATA_ACC_GET_SN_DATA          28
#define DATA_ACC_RESET_MONI_BUF       29
#define DATA_ACC_MONI_AVAIL           30
#define DATA_ACC_GET_NUMOVERFLOWS     31
#define DATA_ACC_SET_LBM_BIT_DEPTH    32
#define DATA_ACC_GET_LBM_SIZE         33
#define DATA_ACC_HISTO_CHARGE_STAMPS  34
#define DATA_ACC_SELECT_ATWD          35
#define DATA_ACC_GET_F_MONI_RATE_TYPE 36
#define DATA_ACC_SET_F_MONI_RATE_TYPE 37
#define DATA_ACC_GET_LBM_PTRS         38

// define service specific error values
#define DAC_Data_Overrun        4
#define DAC_Moni_Not_Init       5
#define DAC_Moni_Overrun        6
#define DAC_Moni_Badstat        7
#define DAC_Bad_Compr_Format    8
#define DAC_Bad_Argument        9
#define DAC_Cant_Get_FB_Serial 10
#define DAC_Cant_Enable_FB     11
#define DAC_SN_Not_Running     12
#define DAC_Bad_Lbm_Depth      13

#define	DAC_ACC_DATA_AVAIL_LEN 1

#endif


