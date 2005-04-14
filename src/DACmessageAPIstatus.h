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
#define DATA_ACC_TEST_SW_COMP      15 /* Inject software compression test pattern data */
#define DATA_ACC_SET_BASELINE_THRESHOLD 16
#define DATA_ACC_GET_BASELINE_THRESHOLD 17
#define DATA_ACC_SET_SW_DATA_COMPRESSION 18
#define DATA_ACC_GET_SW_DATA_COMPRESSION 19
#define DATA_ACC_SET_SW_DATA_COMPRESSION_FORMAT 20
#define DATA_ACC_GET_SW_DATA_COMPRESSION_FORMAT 21
#define DATA_ACC_RESET_LBM 22
#define DATA_ACC_GET_FB_SERIAL 23 /* out: UB*n serial ID */

// define service specific error values
#define DAC_Data_Overrun        4
#define DAC_Moni_Not_Init       5
#define DAC_Moni_Overrun        6
#define DAC_Moni_Badstat        7
#define DAC_Bad_Compr_Format    8
#define DAC_Bad_Argument        9
#define DAC_Cant_Get_FB_Serial 10
#define DAC_Cant_Enable_FB     11

/* These are Data Access specific return message
   formats and values.  In most cases, they are formatted
   as byte values to eliminate any questions of
   "endian-ness".  Note that remote object accesses
   (subtype=REMOTE_OBJECT_REF) will have a format
   determined by that additional protocol.  These
   formats describe "all the rest".
*/

/* Response to: 
	subType: DATA_ACC_DATA_AVAIL
Passed values:
	none
Returned values in data portion of message:
	BOOLEAN dataAvailable
Size of returned values in data portion:
	UBYTE DATA_AVAILABLE */
#define	DAC_ACC_DATA_AVAIL_LEN 1

/* Response to: 
	subType: DATA_GET_DATA
Passed values:
	none
Returned values in data portion of message:
	data buffer, if available
Size of returned values in data portion:
	data length encoded in message header */

/*
 
Response to:
        subType: DATA_ACC_GET_NEXT_MONI_REC
Passed values:
        none
Returned values in data portion of message:
        BOOLEAN dataAvailable.  If 0, nothing else will be in message.  
	If 1, the rest of the message contains the monitoring record.
*/


/*

Response to:
        subType: DATA_ACC_SET_MONI_IVAL
Passed values:
        ULONG hware_interval
        ULONG config_interval
Returned values in data portion of message:
        none 

*/

/* 
Response to
        subtype: DATA_ACC_SET_ENG_FMT
Passed values:
        UBYTE FADC_SAMP_CNT
        UBYTE ATWD01_MASK
        UBYTE ATWD23_MASK
Returned values in data portion of message: 
        none
*/


#endif


