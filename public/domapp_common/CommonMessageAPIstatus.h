/* commonMessageAPIstatus.h */

/* This file contains common response messages and 
   formats.

   March 30, 1999
   Chuck McParland */


#ifndef _COMMON_MESSAGE_API_STATUS_
#define _COMMON_MESSAGE_API_STATUS_


/* Common message subtypes are:
   GET_SERVICE_STATE
   GET_LAST_ERROR_ID
   GET_LAST_ERROR_STR
   CLEAR_LAST_ERROR

   Response to: 
  	subType: GET_SERVICE_STATE
   Passed values:
  	none
   Size of passed values:
  	0  
   Returned values in data portion of message:
  	BYTE currentState
   Size of returned values in data portion: */
#define GET_SERVICE_STATE_LEN 1
/* Legal values:  */
#define SERVICE_OFFLINE 0
#define SERVICE_ONLINE 1
#define SERVICE_BUSY 2
#define SERVICE_ERROR 3


/* Response to: 
  	subType: GET_LAST_ERROR_ID
   Passed values:
  	none
   Size of passed values:
  	0  
   Returned values in data portion of message:
  	UBYTE errorSeverity
  	UBYTE errorValue
   Size of returned values in data portion: */
#define GET_LAST_ERROR_ID_LEN 2
/* Legal values: 
  	errorSeverity:
		same values as message status mask.
		see "messageAPIstatus.h" i.e.:
		#define FATAL_ERROR 0x60
		#define SEVERE_ERROR 0x40
		#define WARNING_ERROR 0x20
		#define INFORM_ERROR 0x00
	errorValue: 
		error values defined by individual service
		include files (e.g. EXPmessageAPIstatus.h) */
#define	COMMON_No_Errors 1
#define	COMMON_Bad_Msg_Subtype 2
#define COMMON_Bad_Msg_Format 3

/* Response to: 
	subType: GET_LAST_ERROR_STR 
   Passed values:
  	none
   Size of passed values:
  	0 
   Returned values in data portion of message:
  	char[] text_string
   Size of returned values in data portion:
  	strLen(text_string)+1;
   Legal values:
		legal values defined by individual service
		include files (e.g. ExpControl.h) */

/* Response to: 
	subType: GET_SERVICE_VERSION_INFO
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	UBYTE majorVersion
	UBYTE minorVersion
   Size of returned values in data portion: */
#define GET_SERVICE_VERSION_INFO_LEN 2

/* Response to: 
	subType: GET_SERVICE_STATS
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
    All ULONGs are in LITTLE ENDIAN format.
	ULONG msgReceived;
    ULONG msgRefused;
    ULONG msgProcessingErr;
   Size of returned values in data portion: */
#define GET_SERVICE_STATS_LEN 12

#endif
