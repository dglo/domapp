// DATAmessagerAPIstatus.h 

// This file contains subtype values for the 
// DAQ<=>DOM messaging API used by the Data
// Access service.  
//
// March 30, 1999
// Chuck McParland


#ifndef _DATA_MESSAGE_API_STATUS_
#define _DATA_MESSAGE_API_STATUS_


// message API subtype values. This field is not
// declared as an enum because each service declares
// values in addion to those in MessageAPIstatus.h.
// Since enums are typedefs, odd compiler problems 
// will ensue.  
//
// Note 255 is the maximum value available for this field. 
#define	DATA_ACC_DATA_AVAIL 10
#define	DATA_ACC_GET_NEXT_BUF 11

// These are Data Access specific return message
// formats and values.  In most cases, they are formatted
// as byte values to eliminate any questions of
// "endian-ness".  Note that remote object accesses
// (subtype=REMOTE_OBJECT_REF) will have a format
// determined by that additional protocol.  These
// formats describe "all the rest".

// Response to: 
//	subType: DATA_ACC_GET_NEWEST_EVENT
//			 DATA_ACC_GET_OLDEST_EVENT
//			 DATA_ACC_GET_ANY_EVENT
//			 DATA_ACC_GET_THIS_EVENT
// Passed values:
//	none
// Returned values in data portion of message:
//	BOOLEAN dataAvailable
// Size of returned values in data portion:
//	UBYTE
#define	DAC_data_avail 1

// Response to: 
//	subType: EXPCONTROL_DIS_TRIG 
// Returned values in data portion of message:
//	none
// Size of returned values in data portion:
//	determined by header 

#endif


