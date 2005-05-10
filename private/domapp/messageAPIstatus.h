
/* messageAPIstatus.h */

/* This file contains status, type and subtype
   values for the DAQ<=>DOM messaging API.  As new
   services and status values are created, they
   should be added to this file.

   March 30, 1999
   Chuck McParland */

#ifndef _MESSAGE_API_STATUS_
#define _MESSAGE_API_STATUS_

/* message API return status values are formatted by
   or-ing the appropriate status value with the
   appropriate logging request value and the 
   appropriate error severity value.
   e.g. BYTE status=(UNKNOWN_SERVER|LOG_REQUEST)
  		|FATAL_ERROR;
   Since all services share the same error "space",
   errors should be kept fairly generic.  Detailed
   information about a particular service's error
   can be obtained by a message query to that service
   (see SubTypes).
  
   Note that only 31 status values are available. */

/* This was broken - you can't OR these together 
   if they aren't powers of 2!!! --JEJ
   enum Status {
   SUCCESS=1,
   UNKNOWN_SERVER,
   SERVER_STACK_FULL,
   UNKNOWN_SUBTYPE,
   SERVER_PROTOCOL_ERROR,
   DATA_NOT_FOUND,
   OBJECT_NOT_FOUND,
   SERVICE_SPECIFIC_ERROR,
   LAST_STATUS=31
   };
*/

#define BIT(n) (1<<(n))
/* Can only have 8 of these, status is 8 bits */
#define SUCCESS                BIT(0)
#define UNKNOWN_SERVER         BIT(1)
#define SERVER_STACK_FULL      BIT(2)
#define UNKNOWN_SUBTYPE        BIT(3)
#define SERVER_PROTOCOL_ERROR  BIT(4)
#define DATA_NOT_FOUND         BIT(5)
#define OBJECT_NOT_FOUND       BIT(6)
#define SERVICE_SPECIFIC_ERROR BIT(7)

/* logging request bit masks */
#define NO_LOG_REQUEST 0x00
#define LOG_REQUEST 0x80
/* error severity bit masks */
#define FATAL_ERROR 0x60
#define SEVERE_ERROR 0x40
#define WARNING_ERROR 0x20
#define INFORM_ERROR 0x00

/* message API type values. 
   Note 255 values are available for this field. */ 
enum Type {
	MESSAGE_HANDLER=1,
	DOM_SLOW_CONTROL,
	DATA_ACCESS,
	EXPERIMENT_CONTROL,
	TEST_MANAGER
};

/* message API subtype values. In principle, it is
   possible to multiply allocate values in this field
   since it is only interpreted by the service
   addressed by the Type field. This field is not
   declared as an enum because each service (see
   Type above) will declare additional values in 
   a separate include file.  Since enums are typedefs,
   odd compiler problems will ensue.  These values
   will be valid for ALL services so they are defined
   here. 
  
   Note 255 values are available for this field. */
#define	GET_SERVICE_STATE 1
#define	GET_LAST_ERROR_ID 2
#define	GET_LAST_ERROR_STR 3
#define GET_SERVICE_VERSION_INFO 4
#define CLEAR_LAST_ERROR 5
#define GET_SERVICE_STATS 6
#define	REMOTE_OBJECT_REF 7
#define GET_SERVICE_SUMMARY 8

#endif
