/* ExpControl.h */
#ifndef _EXPCONTROL_H_
#define _EXPCONTROL_H_

/* Header file for defines, structs, etc. for */
/* the Experiment Control Service. */

/* Experiment Control version info. */
#define EXP_MAJOR_VERSION 10
#define EXP_MINOR_VERSION 1
/* major version 10	10 May 2003
	beginning of icecube domapp code chain, version 10 coded as 
	single threaded version for test purposes.
*/
/* minor version 1	5 July 2003
	formatting changes only.
*/

/* maximum length of Experiment Control last error string */
#define EXP_ERROR_STR_LEN 80

/* Experiment Control error strings */
/* for EXP_No_Errors */
#define EXP_ERS_NO_ERRORS "EXP: No errors."
/* for EXP_Bad_Msg_Subtype */
#define EXP_ERS_BAD_MSG_SUBTYPE "Exp: Bad msg subtype."
/* for EXP_Cannot_Start_Trig */
#define EXP_CANNOT_START_TRIG "Exp: Cannot start trigger."
/* for EXP_Trig_Not_Active */
#define EXP_CANNOT_STOP_TRIG "Exp: Cannot stop trigger."
/* for EXP_Cannot_Begin_Run */
#define EXP_CANNOT_BEGIN_RUN "Exp: Cannot begin run."
/* for EXP_Cannot_End_Run */
#define EXP_CANNOT_END_RUN "Exp: Cannot end run."
#define EXP_CANNOT_BEGIN_FB_RUN "Exp: Cannot begin FB run."
#define EXP_CANNOT_END_FB_RUN "Exp: Cannot end FB run."

/* for EXP_Cannot_Reset_Run_State */
#define EXP_CANNOT_RESET_RUN_STATE "Exp: Cannot reset run state."

#define EXP_PEDESTAL_RUN_FAILED "Exp: Pedestal run failed."
#define EXP_TOO_MANY_PEDS "Exp: Too many pedestal triggers requested."
#define EXP_PEDESTALS_NOT_AVAIL "Exp: Pedestals not available, did you EXPCONTROL_DO_PEDESTAL_COLLECTION?"

/* expControl entry point */
void expControlInit(void);
void expControl(MESSAGE_STRUCT *M);
void pedestalRunEntryPoint(void);
void dumpRegs(void);

#endif
