
/* messageAPIstatus.h */

/* This file contains status, type and subtype
   values for the DAQ<=>DOM messaging API.  As new
   services and status values are created, they
   should be added to this file.

   March 30, 1999
   Chuck McParland
   Modified (fixed) J. Jacobsen 2005-2006 */

#ifndef _MESSAGE_API_STATUS_
#define _MESSAGE_API_STATUS_

/* message API return status values are formatted by
   or-ing the appropriate status value with the
   appropriate logging request value and the 
   appropriate error severity value.
   e.g. BYTE status=(UNKNOWN_SERVER|FATAL_ERROR);
   Since all services share the same error "space",
   errors should be kept fairly generic.  Detailed
   information about a particular service's error
   can be obtained by a message query to that service
   (see SubTypes). */
  
#define BIT(n) (1<<(n))
/* Can only have 8 of these, status is 8 bits */
#define SUCCESS                BIT(0)  /* 0x01 */
#define UNKNOWN_SERVER         BIT(1)  /* 0x02 */
#define INFORM_ERROR           BIT(2)  /* 0x04 */
#define UNKNOWN_SUBTYPE        BIT(3)  /* 0x08 */
#define FATAL_ERROR            BIT(4)  /* 0x10 */
#define WARNING_ERROR          BIT(5)  /* 0x20 */
#define SEVERE_ERROR           BIT(6)  /* 0x40 */
#define SERVICE_SPECIFIC_ERROR BIT(7)  /* 0x80 */

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
