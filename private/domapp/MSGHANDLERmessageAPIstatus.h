/* MSGHANDLERmessagerAPIstatus.h  */
#ifndef _MSHHAND_MESSAGE_API_STATUS_H_
#define _MSHHAND_MESSAGE_API_STATUS_H_

/* This file contains subtype values for the 
   DAQ<=>DOM messaging API used by the Message
   Handler service.  

   Chuck McParland */

/* message API subtype values. This field is not
   declared as an enum because each service declares
   values in addion to those in messageAPIstatus.h.
   Since enums are typedefs, odd compiler problems 
   will ensue.  

   Note 255 is the maximum value available for this field.  */
#define	MSGHAND_GET_DOM_ID 10
#define	MSGHAND_GET_DOM_NAME 11
#define	MSGHAND_GET_DOM_VER 12
#define MSGHAND_GET_PKT_STATS 13
#define MSGHAND_GET_MSG_STATS 14
#define MSGHAND_CLR_PKT_STATS 15
#define MSGHAND_CLR_MSG_STATS 16
#define	MSGHAND_GET_ATWD_ID 17
#define MSGHAND_ECHO_MSG 18
#define MSGHAND_ACCESS_MEMORY_CONTENTS 20
#define MSGHAND_REBOOT_CPU_FLASH 23
#define MSGHAND_GET_DOMAPP_RELEASE 24

/* message handler specific errors */
#define MSGHAND_unknown_server 4
#define MSGHAND_server_stack_full 5
#define MSGHAND_bad_parameter 6

/* These are Message Handler specific return message
   formats and values.  In most cases, they are formatted
   as byte values to eliminate any questions of
   "endian-ness".  Note that remote object accesses
   (subtype=REMOTE_OBJECT_REF) will have a format
   determined by that additional protocol.  These
   formats describe "all the rest". */

/* Response to: 
	subType: MSGHAND_GET_DOM_ID
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	ULONG DOM ID number
   Size of returned values in data portion: */
#define MSGHAND_GET_DOM_ID_LEN 12

/* Response to: 
	subType: MSGHAND_GET_DOM_NAME
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	char[] text_string
   Size of returned values in data portion: 
	length of name string */

/* Response to: 
	subType: MSGHAND_GET_DOM_VERSION_INFO
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	UBYTE majorVersion
	UBYTE minorVersion
   Size of returned values in data portion: */
#define MSGHAND_GET_DOM_VERSION_INFO_LEN 2

/* Response to: 
	subType: MSGHAND_GET_PKT_STATS
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	ULONG PKTrecv
	ULONG PKTsent
	ULONG NoStorage
	ULONG FreeListCorrupt
	ULONG PKTbufOvr
	ULONG PKTbadFmt
	ULONG PKTspare
	
   Size of returned values in data portion: */
#define MSGHAND_GET_PKT_STATS_LEN 7*4

/* Response to: 
	subType: MSGHAND_GET_MSG_STATS
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	ULONG MSGrecv
	ULONG MSGsent
	ULONG tooMuchData
	ULONG IDMismatch
	ULONG CRCproblem
	
   Size of returned values in data portion: */
#define MSGHAND_GET_MSG_STATS_LEN 5*4

/* Response to: 
	subType: MSGHAND_CLR_PKT_STATS
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	none
   Size of returned values in data portion: 
	0 	*/

/* Response to: 
	subType: MSGHAND_CLR_PMSG_STATS
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	none
   Size of returned values in data portion: 
	0 	*/

/* Response to: 
	subType: MSGHAND_GET_ATWD_ID
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	ULONG ATWD0_ID
	ULONG ATWD1_ID
   Size of returned values in data portion: */
#define MSGHAND_GET_ATWD_ID_LEN 8	

/* Response to: 
	subType: MSGHAND_ACCESS_MEMORY_CONTENTS 
   Passed values:
	UBYTE byte (==0) or long (==1)
	UBYTE read (==0) or write (==1)
	USHORT numReads (must be 1 for writes)
	ULONG address (in application mode)
   Size of passed values: */
#define MSGHAND_ACCESS_MEMORY_CONTENTS_REQ_LEN 8
/* Returned values in data portion
	bytes or longs read
   Size of returned values in data portion: 
	number of bytes read */

/* Response to: 
	subType: MSGHANDR_REBOOT_FLASH 
   Passed values:
	none
   Size of passed values: 
	0
   Returned values in data portion
	none
   Size of returned values in data portion: 
	none */

/* Response to:
        subType: MSGHAND_GET_DOMAPP_RELEASE
   Passed values:
        none
   Size of passed values:
        0
   Returned values in data portion of message:
        Character string w/o null termination
   Size of returned values in data portion:
        variable 
*/

#endif
