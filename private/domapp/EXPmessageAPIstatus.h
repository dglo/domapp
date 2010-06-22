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
#define	EXPCONTROL_GET_DOM_STATE 15

/* New messages: see domapp api document: */
#define EXPCONTROL_DO_PEDESTAL_COLLECTION 16 /* in: UL UL UL */
#define EXPCONTROL_GET_NUM_PEDESTALS 19 /* out: UL UL UL */
#define EXPCONTROL_GET_PEDESTAL_AVERAGES 20 /* out: UH:128*9 */
#define EXPCONTROL_BEGIN_FB_RUN 27 /* in: US, US, S, US, US */
#define EXPCONTROL_END_FB_RUN   28 /* No args */
#define EXPCONTROL_CHANGE_FB_SETTINGS 29 /* in: US, US, S, US, US */
#define EXPCONTROL_RUN_UNIT_TESTS 30

#define	EXP_Cannot_Start_Trig 3
#define	EXP_Cannot_Stop_Trig 4
#define	EXP_Cannot_Begin_Run 5
#define	EXP_Cannot_End_Run 6
#define	EXP_Cannot_Reset_Run_State 7
#define EXP_Cannot_Begin_FB_Run 8
#define EXP_Cannot_End_FB_Run 9
#define EXP_Pedestal_Run_Failed 8
#define EXP_Too_Many_Peds       9
#define EXP_Pedestals_Not_Avail 10
#define EXP_Bad_FB_Delay        11
#define EXP_Cannot_Change_FB_Settings 12
#define EXP_Unit_Test_Failure 13
#endif
