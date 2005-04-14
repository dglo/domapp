/* DOMstateInfo.h */

#ifndef _DOMSTATEINFO_
#define _DOMSTATEINFO_

#define DOM_IDLE 0
#define DOM_RUN_IN_PROGRESS 1
#define DOM_RUN_ENDING 2
#define DOM_RUN_ERROR 3
#define DOM_TEST_IN_PROGRESS 4
#define DOM_TEST_ENDING 5
#define DOM_TEST_ERROR 6
#define DOM_FPGA_LOAD_IN_PROGRESS 7
#define DOM_PEDESTAL_COLLECTION_IN_PROGRESS 8
#define DOM_FB_RUN_IN_PROGRESS 9
#define DOM_CONFIG_CHANGES_ALLOWED 0
#define DOM_CONFIG_CHANGES_DISALLOWED 1

/* maximum length of Experiment Control last */
/* error string. */
#define DOM_STATE_ERROR_STR_LEN 80
/* error strings */
#define DOM_STATE_STR_STARTUP "DOM: normal boot."

/* DOM state status values */
#define DOM_STATE_SUCCESS 0

/* bit mask values for contraint vector */
#define DOM_CONSTRAINT_NO_CONSTRAINTS 0x0
#define DOM_CONSTRAINT_NO_FPGA_CHANGE 0x1
#define DOM_CONSTRAINT_NO_HV_CHANGE 0x2
#define DOM_CONSTRAINT_NO_DAC_CHANGE 0x4
#define DOM_CONSTRAINT_NO_TESTS 0x8
#define DOM_CONSTRAINT_NO_FLASHER 0x10

#endif



