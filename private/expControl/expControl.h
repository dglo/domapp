/* ExpControl.h */
#ifndef _EXPCONTROL_
#define _EXPCONTROL_

/* Header file for defines, structs, etc. for */
/* the Experiment Control Service. */

/* Experiment Control version info. */
#define EXP_MAJOR_VERSION 2
#define EXP_MINOR_VERSION 0

/* maximum length of Experiment Control last */
/* error string. */
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
/* for EXP_Cannot_Reset_Run_State */
#define EXP_CANNOT_RESET_RUN_STATE "Exp: Cannot reset run state."

/* expControl entry point */
void *expControl(void *arg);

#endif
