/* EXPmessagerAPIstatus.h */
#ifndef _EXP_MESSAGE_API_STATUS_H_
#define _EXP_MESSAGE_API_STATUS_H_

/* This file contains subtype values for the 
   DAQ<=>DOM messaging API used by the Experiment
   Control service.  

   Chuck McParland */

/* message API subtype values. This field is not
   declared as an enum because each service declares
   values in addion to those in MessageAPIstatus.h.
   Since enums are typedefs, odd compiler problems 
   will ensue.  

   Note 255 is the maximum value available for this field.  */
#define	EXPCONTROL_ENA_TRIG 10
#define	EXPCONTROL_DIS_TRIG 11
#define	EXPCONTROL_BEGIN_RUN 12
#define	EXPCONTROL_END_RUN 13
#define EXPCONTROL_FORCE_RUN_RESET 14
#define	EXPCONTROL_GET_DOM_STATE 15

/* New messages: see domapp api document: */
#define EXPCONTROL_DO_PEDESTAL_COLLECTION 16 /* in: UL UL UL */
#define EXPCONTROL_GET_NUM_PEDESTALS 19 /* out: UL UL UL */
#define EXPCONTROL_GET_PEDESTAL_AVERAGES 20 /* out: UH:128*9 */
#define EXPCONTROL_BEGIN_FB_RUN 27 /* Out: US US US US US */
#define EXPCONTROL_END_FB_RUN   28 /* No args */

/* These are Experiment Control specific return message
   formats and values.  In most cases, they are formatted
   as byte values to eliminate any questions of
   "endian-ness".  Note that remote object accesses
   (subtype=REMOTE_OBJECT_REF) will have a format
   determined by that additional protocol.  These
   formats describe "all the rest". */

/* Response to: 
  	subType: EXPCONTROL_ENA_TRIG
   Passed values:
	none
   Size of passed values:
  	0 
   Returned values in data portion of message:
  	none
   Size of returned values in data portion:
  	0
   Last Error ID for failed invocation: */
#define	EXP_Cannot_Start_Trig 3

/* Response to: 
	subType: EXPCONTROL_DIS_TRIG 
   Passed values:
  	none
   Size of passed values:
	0 
   Returned values in data portion of message:
	none
   Size of returned values in data portion:
	0
   Last Error ID for failed invocation: */
#define	EXP_Cannot_Stop_Trig 4

/* Response to: 
	subType: EXPCONTROL_BEGIN_RUN
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	none
   Size of returned values in data portion:
	0
   Last Error ID for failed invocation: */
#define	EXP_Cannot_Begin_Run 5


/* Response to: 
	subType: EXPCONTROL_END_RUN
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	none
   Size of returned values in data portion:
	0
   Last Error ID for failed invocation: */
#define	EXP_Cannot_End_Run 6

/* Response to: 
	subType: EXPCONTROL_FORCE_RUN_RESET
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	none
   Size of returned values in data portion:
	0
   Last Error ID for failed invocation: */
#define	EXP_Cannot_Reset_Run_State 7

#define EXP_Cannot_Begin_FB_Run 8
#define EXP_Cannot_End_FB_Run 9

/* Response to: 
	subType: EXPCONTROL_GET_DOM_STATE
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	UBYTE DOM_state
	UBYTE DOM_status
	UBYTE DOM_cmdSource
	UBYTE spare
	ULONG DOM_constraints
	char DOM_errorString
   Size of returned values in data portion: 
	length of errorString + 8
   Legal values: */


/* More error conditions */

#define EXP_Pedestal_Run_Failed 8
#define EXP_Too_Many_Peds       9
#define EXP_Pedestals_Not_Avail 10
#endif
