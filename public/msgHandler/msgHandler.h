/* msgHandler.h */
#ifndef _MSGHANDLER_
#define _MSGHANDLER_

/* 
   Header file for defines, structs, etc. for
   the message handler Service. */

/* message handler version info. */
#define MSGHANDLER_MAJOR_VERSION 3
#define MSGHANDLER_MINOR_VERSION 2
/* minor version 1	23 Nov 1999
	added MSGHAND_GET_MEMORY_CONTENTS subtype to fetch byte and long word data by
		address.
   Minor Version 2 	8 Dec 1999
	added code to reboot into DOMboot and/or on-chip boot
*/
 
/* maximum length of Test Manager last
   error string. */
#define MSGHANDLER_ERROR_STR_LEN 80

/* message handler error strings.  Even error
  	strings common to all services are listed here
	so we can tag them with the name of the service
	that generated them. */
/* for COMMON_No_Errors */
#define MSGHAND_ERS_NO_ERRORS "msgHnd: No errors."
/* for COMMON_Bad_Msg_Subtyp */
#define MSGHAND_ERS_BAD_MSG_SUBTYPE "msgHnd: Bad msg subtype."
/* for COMMON_Bad_Msg_Format */
#define MSGHAND_ERS_BAD_MSG_FORMAT "msgHnd: Bad msg format."
/* for MSGHAND_unknown_server */
#define MSGHAND_UNKNOWN_SERVER "msgHnd: Unknown server."
/* for MSGHAND_server_stack_full */
#define MSGHAND_SERVER_STACK_FULL "msgHnd: Server stack full."
/* for MSGHAND_bad_parameter */
#define MSGHAND_BAD_PARAMETER "msgHnd: Bad paramaeter or parameter value."

/* define msgHandler entry point */
void *msgHandler(void *arg);

#endif
