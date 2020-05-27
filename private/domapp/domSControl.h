/* DOMSControl.h */
#ifndef _DOMSCONTROL_H_
#define _DOMSCONTROL_H_

/* Header file for defines, structs, etc. for
   the Slow Control Service. */

int doStartSN(UBYTE mode, unsigned deadtime);
void doStopSN();

/* Slow Control version info. */
#define DSC_MAJOR_VERSION 10
#define DSC_MINOR_VERSION 2
/* major version 10	10 May 2003
	beginning icecube domapp code chain, version 10 coded as
	single threaded version for test purposes.
*/
/* minor version 1	5 July	2003
	added pulser and mux routines
*/
/* minor version 2     18 May 2018
        added self-LC routines
*/

/* maximum length of slow control last error string */
#define DSC_ERROR_STR_LEN 80

/* default maximums for PMT HV anode and dynode */
#define PMT_HV_DEFAULT_MAX 4095

/* Enum types for local coincidence */
typedef enum {
  LC_MODE_NONE        = 0, 
  LC_MODE_BOTH        = 1,
  LC_MODE_UP          = 2,
  LC_MODE_DN          = 3,
  LC_MODE_UP_AND_DOWN = 4,
  LC_MODE_SLC_ONLY    = 5,
} LC_MODE_T;
#define MAX_LC_MODE LC_MODE_SLC_ONLY /* Must agree with highest LC_MODE_T */

typedef enum {
  SELF_LC_MODE_NONE=0,
  SELF_LC_MODE_SPE=1,
  SELF_LC_MODE_MPE=2
} SELF_LC_MODE_T;
#define MAX_SELF_LC_MODE SELF_LC_MODE_MPE

typedef enum {
  LC_TYPE_NONE=0,
  LC_TYPE_SOFT=1, 
  LC_TYPE_HARD=2,
  LC_TYPE_FLABBY=3 
} LC_TYPE_T;
typedef enum {
  LC_TX_NONE=0,
  LC_TX_UP  =1,
  LC_TX_DN  =2,
  LC_TX_BOTH=3 
} LC_TX_T;
typedef enum {
  LC_SRC_SPE = 0,
  LC_SRC_MPE = 1 
} LC_SRC_T;

/* Slow Control error strings */
/* for COMMON_No_Errors */
#define DSC_ERS_NO_ERRORS "DSC: No errors."
/* for COMMON_Bad_Msg_Subtype */
#define DSC_ERS_BAD_MSG_SUBTYPE "DSC: Bad msg subtype."
/* for COMMON_Bad_Msg_Format */
#define DSC_ERS_BAD_MSG_FORMAT "DSC: Bad msg format."
/* for DSC_Failed_Challenge */
#define DSC_FAILED_CHALLENGE "DSC: Failed verify challenge."
/* for DSC_Illegal_ADC_Channel */
#define DSC_ILLEGAL_ADC_CHANNEL "DSC: Illegal ADC channel."
/* for DSC_Illegal_DAC_Channel */
#define DSC_ILLEGAL_DAC_CHANNEL "DSC: Illegal DAC channel."
/* for DSC_FPGA_not_loaded */
#define DSC_FPGA_NOT_LOADED "DSC: FPGA not loaded."
/* for DSC_FPGA_file_not_found */
#define DSC_FPGA_FILE_NOT_FOUND "DSC: FPGA file not found."
/* for DSC_FPGA_wrong_file_type */
#define DSC_FPGA_WRONG_FILE_TYPE "DSC: FPGA wrong file type."
/* for DSC_FPGA_load_failed */
#define DSC_FPGA_LOAD_FAILED "DSC: FPGA load failed."
/* for DSC_FPGA_already_loaded */
#define DSC_FPGA_ALREADY_LOADED "DSC: FPGA already loaded."
/* for DSC_PMT_HV_request_too_high */
#define DSC_PMT_HV_REQUEST_TOO_HIGH "DSC: PMT HV request (anode or dynode) too high."
/* for DSC_bad_DOM_state */
#define DSC_BAD_DOM_STATE "DSC: DOM in wrong state for request."
/* for DSC_unknown_error */
#define DSC_UNKNOWN_ERROR "DSC: Unknown error encountered."
/* for DSC_bgnd_task_unresponsive */
#define DSC_BGND_TASK_UNRESPONSIVE "DSC: Background task unresponsive."
/* for DSC_FPGA_config_error */
#define DSC_FPGA_CONFIG_ERROR "DSC: Error in configuring FPGA."
/* for DSC_bad_task_cmd */
#define DSC_BAD_TASK_CMD "DSC: Bad background task cmd."
/* for DSC_violates_constraints */
#define DSC_VIOLATES_CONSTRAINTS "DSC: Operation violates present constraints."
/*for DSC_rate_meters_unresponsive */
#define DSC_RATE_METERS_UNRESPONSIVE "DSC: Rate meters unresponsive."
/* for DSC_bad_flasher_param */
#define DSC_BAD_FLASHER_PARAM "DSC: Bad flasher parameter."

#define DSC_ILLEGAL_LC_MODE "DSC: Illegal Local Coincidence Mode"
#define DSC_LC_WINDOW_FAIL "DSC: Failed to set local coin. windows (check values!)"

/* domSControl entry point */
void domSControlInit(void);
void domSControl(MESSAGE_STRUCT *M);

/* New LC config functions */
void updateLCtx(void);
void updateLCsrc(void);
void updateLCspan(void);
void updateLClengths(void);
int  updateLCwindows(void);

void dsc_hal_disable_LC_completely(void);
void dsc_hal_do_LC_settings(void);

int dsc_isSNrunning(void);

#endif
