/* DOMSControl.h */
#ifndef _DOMSCONTROL_
#define _DOMSCONTROL_

/* Header file for defines, structs, etc. for
   the Data Access Service. */

/* Data Access version info. */
#define DSC_MAJOR_VERSION 2
#define DSC_MINOR_VERSION 0 

/* maximum length of Data Access last 
   error string. */
#define DSC_ERROR_STR_LEN 80

/* default maximums for PMT HV anode and dynode */
#define PMT_HV_DEFAULT_MAX 600

/* Data Access error strings */
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

/* domSControl entry point */
void *domSControl(void *arg);

#endif
