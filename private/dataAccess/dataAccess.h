/* dataAccess.h */
#ifndef _DATA_ACCESS_
#define _DATA_ACCESS_

/* Header file for defines, structs, etc. for */
/* the Data Access Service. */

/* Data Access version info. */
#define DAC_MAJOR_VERSION 2
#define DAC_MINOR_VERSION 0

/* maximum length of Data Access last */
/* error string. */
#define DAC_ERROR_STR_LEN 80

/* Data Access error strings */
/* for DAC_No_Errors */
#define DAC_ERS_NO_ERRORS "DAC: No errors."
/* for DAC_Bad_Msg_Subtype */
#define DAC_ERS_BAD_MSG_SUBTYPE "DAC: Bad msg subtype."

/* data access entry point */
void *dataAccess(void *arg);

#endif
