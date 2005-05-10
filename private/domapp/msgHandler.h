/* msgHandler.h */
#ifndef _MSGHANDLER_H_
#define _MSGHANDLER_H_

/* 
   Header file for defines, structs, etc. for
   the message handler Service. */

/* message handler version info. */
#define MSGHANDLER_MAJOR_VERSION 10
#define MSGHANDLER_MINOR_VERSION 1
/* major version 10	10 May 2003
	beginning of icecube domapp code chain, version 10 coded as
	single threaded version for test purposes.
*/
/* minor version 1	5 July 2003
	added command to access hal reboot code.
*/
 
/* maximum length of msgHandler last error string */
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
void msgHandlerInit(void);
void msgHandler(MESSAGE_STRUCT *M);

#endif
