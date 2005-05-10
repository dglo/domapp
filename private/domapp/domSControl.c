/* domSControl.c */

/*
Author: Chuck McParland
Start Date: May 4, 1999
Description:
	DOM Slow Control service thread.  
	Performs all "standard" DOM service functions.
Last Modification:
Jan. 14 '04 Jacobsen -- add monitoring actions for state change operations
*/

#include <string.h>

/* DOM-related includes */
#include "hal/DOM_MB_types.h"
#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_domapp.h"
#include "hal/DOM_FPGA_regs.h"

#include "message.h"
#include "moniDataAccess.h"

#include "domSControl.h"
#include "slowControl.h"
#include "messageAPIstatus.h"
#include "commonServices.h"
#include "commonMessageAPIstatus.h"
#include "domSControlRoutines.h"
#include "DSCmessageAPIstatus.h"
#include "DOMstateInfo.h"

/* extern functions */
extern void formatLong(ULONG value, UBYTE *buf);
extern void formatShort(USHORT value, UBYTE *buf);
extern ULONG unformatLong(UBYTE *buf);
extern USHORT unformatShort(UBYTE *buf);
extern UBYTE DOM_state;
extern UBYTE DOM_config_access;

/* global storage */
extern UBYTE FPGA_trigger_mode;
extern int FPGA_ATWD_select;

/* local functions, data */
USHORT PMT_HV_max=PMT_HV_DEFAULT_MAX;
BOOLEAN pulser_running = FALSE;
USHORT pulser_rate = 0;
UBYTE selected_mux_channel = 0;
ULONG deadTime = 0;
ULONG up_pre_ns, up_post_ns, dn_pre_ns, dn_post_ns;
UBYTE LCmode = 0;

/* struct that contains common service info for
	this service. */
COMMON_SERVICE_INFO domsc;

void domSControlInit(void) {
    domsc.state=SERVICE_ONLINE;
    domsc.lastErrorID=COMMON_No_Errors;
    domsc.lastErrorSeverity=INFORM_ERROR;
    domsc.majorVersion=DSC_MAJOR_VERSION;
    domsc.minorVersion=DSC_MINOR_VERSION;
    strcpy(domsc.lastErrorStr,DSC_ERS_NO_ERRORS);
    domsc.msgReceived=0;
    domsc.msgRefused=0;
    domsc.msgProcessingErr=0;
}

void domSControl(MESSAGE_STRUCT *M) {
#define BSIZ 1024
  //  char buf[BSIZ]; int n;
  //  unsigned long long time;

  UBYTE *data;
  UBYTE tmpByte;
  UBYTE *tmpPtr;
  USHORT tmpShort;
  USHORT PMT_HVreq;
  int i;


  /* get address of data portion. */
  /* Receiver ALWAYS links a message 
     to a valid data buffer-even 
     if it is empty. */
  data=Message_getData(M);
  domsc.msgReceived++;
  switch (Message_getSubtype(M)) {
    /* Manditory Service SubTypes */
  case GET_SERVICE_STATE:
    /* get current state of Slow Control */
    data[0]=domsc.state;
    Message_setDataLen(M,GET_SERVICE_STATE_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case GET_LAST_ERROR_ID:
    /* get the ID of the last error encountered */
    data[0]=domsc.lastErrorID;
    data[1]=domsc.lastErrorSeverity;
    Message_setDataLen(M,GET_LAST_ERROR_ID_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case GET_SERVICE_VERSION_INFO:
    /* get the major and minor version of this
       Slow Control */
    data[0]=domsc.majorVersion;
    data[1]=domsc.minorVersion;
    Message_setDataLen(M,GET_SERVICE_VERSION_INFO_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case GET_SERVICE_STATS:
    /* get standard service statistics for
       the Slow Control */
    formatLong(domsc.msgReceived,&data[0]);
    formatLong(domsc.msgRefused,&data[4]);
    formatLong(domsc.msgProcessingErr,&data[8]);
    Message_setDataLen(M,GET_SERVICE_STATS_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case GET_LAST_ERROR_STR:
    /* get error string for last error encountered */
    strcpy(data,domsc.lastErrorStr);
    Message_setDataLen(M,strlen(domsc.lastErrorStr));
    Message_setStatus(M,SUCCESS);
    break;
  case CLEAR_LAST_ERROR:
    /* reset both last error ID and string */
    domsc.lastErrorID=COMMON_No_Errors;
    domsc.lastErrorSeverity=INFORM_ERROR;
    strcpy(domsc.lastErrorStr,DSC_ERS_NO_ERRORS);
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case REMOTE_OBJECT_REF:
    /* remote object reference
       TO BE IMPLEMENTED..... */
    domsc.msgProcessingErr++;
    strcpy(domsc.lastErrorStr,DSC_ERS_BAD_MSG_SUBTYPE);
    domsc.lastErrorID=COMMON_Bad_Msg_Subtype;
    domsc.lastErrorSeverity=WARNING_ERROR;
    Message_setDataLen(M,0);
    Message_setStatus(M,
		      UNKNOWN_SUBTYPE|WARNING_ERROR);
    break;
  case GET_SERVICE_SUMMARY:
    /* init a temporary buffer pointer */
    tmpPtr=data;
    /* get current state of slow control */
    *tmpPtr++=domsc.state;
    /* get the ID of the last error encountered */
    *tmpPtr++=domsc.lastErrorID;
    *tmpPtr++=domsc.lastErrorSeverity;
    /* get the major and minor version of this */
    /*	slow control */
    *tmpPtr++=domsc.majorVersion;
    *tmpPtr++=domsc.minorVersion;
    /* get standard service statistics for */
    /*	the slow control */
    formatLong(domsc.msgReceived,tmpPtr);
    tmpPtr+=sizeof(ULONG);
    formatLong(domsc.msgRefused,tmpPtr);
    tmpPtr+=sizeof(ULONG);
    formatLong(domsc.msgProcessingErr,tmpPtr);
    tmpPtr+=sizeof(ULONG);
    /* get error string for last error encountered */
    strcpy(tmpPtr,domsc.lastErrorStr);
    Message_setDataLen(M,(int)(tmpPtr-data)+
		       strlen(domsc.lastErrorStr));
    Message_setStatus(M,SUCCESS);
    break;
    /*-------------------------------
      Slow Control specific SubTypes */

  case DSC_READ_ALL_ADCS:
    tmpPtr=data;
    /* lock, access and unlock */
    for(i=0;i<MAX_NUM_ADCS;i++) {
      formatShort(halReadADC(i),tmpPtr);
      tmpPtr+=sizeof(USHORT);
    }
    /* format up success response */
    Message_setDataLen(M,DSC_READ_ALL_ADCS_LEN);
    Message_setStatus(M,SUCCESS);
    break;

  case DSC_READ_ONE_ADC:
    tmpByte=data[0];
    if(tmpByte>=MAX_NUM_ADCS) {
      /* format up failure response */
      domsc.msgProcessingErr++;
      Message_setDataLen(M,0);
      strcpy(domsc.lastErrorStr,DSC_ILLEGAL_ADC_CHANNEL);
      domsc.lastErrorID=DSC_Illegal_ADC_Channel;
      domsc.lastErrorSeverity=FATAL_ERROR;
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|FATAL_ERROR);
    }
    else {
      /* format up success response */
      /* lock, access and unlock */
      formatShort(halReadADC(tmpByte),data);
      Message_setDataLen(M,DSC_READ_ONE_ADC_LEN);
      Message_setStatus(M,SUCCESS);
    }
    break;
  case DSC_READ_ALL_DACS:
    tmpPtr=data;
    /* lock, access and unlock */
    for(i=0;i<MAX_NUM_DACS;i++) {
      formatShort(halReadDAC(i),tmpPtr);
      tmpPtr+=sizeof(USHORT);
    }
    /* format up success response */
    Message_setDataLen(M,DSC_READ_ALL_DACS_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_READ_ONE_DAC:
    tmpByte=data[0];
    if(tmpByte>=MAX_NUM_DACS) {
      /* format up failure response */
      domsc.msgProcessingErr++;
      Message_setDataLen(M,0);
      strcpy(domsc.lastErrorStr,DSC_ILLEGAL_DAC_CHANNEL);
      domsc.lastErrorID=DSC_Illegal_DAC_Channel;
      domsc.lastErrorSeverity=FATAL_ERROR;
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|FATAL_ERROR);
    }
    else {
      /* format up success response */
      /* lock, access and unlock */
      formatShort(halReadDAC(tmpByte),data);
      Message_setDataLen(M,DSC_READ_ONE_DAC_LEN);
      Message_setStatus(M,SUCCESS);
    }
    break;
  case DSC_WRITE_ONE_DAC:
    tmpByte=data[0];
    if(Message_dataLen(M)!=DSC_WRITE_ONE_DAC_REQ_LEN){
      /* format up failure response */
      domsc.msgProcessingErr++;
      strcpy(domsc.lastErrorStr,DSC_ERS_BAD_MSG_FORMAT);
      domsc.lastErrorID=COMMON_Bad_Msg_Format;
      domsc.lastErrorSeverity=WARNING_ERROR;
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|WARNING_ERROR);
    }
    else if(!testDOMconstraints(DOM_CONSTRAINT_NO_DAC_CHANGE)){
      /* format up failure response */
      domsc.msgProcessingErr++;
      strcpy(domsc.lastErrorStr,DSC_VIOLATES_CONSTRAINTS);
      domsc.lastErrorID=DSC_violates_constraints;
      domsc.lastErrorSeverity=WARNING_ERROR;
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|WARNING_ERROR);
    }
    else if(tmpByte>=MAX_NUM_DACS) {
      /* format up failure response */
      domsc.msgProcessingErr++;
      strcpy(domsc.lastErrorStr,DSC_ILLEGAL_DAC_CHANNEL);
      domsc.lastErrorID=DSC_Illegal_DAC_Channel;
      domsc.lastErrorSeverity=FATAL_ERROR;
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|FATAL_ERROR);
    }
    else {
      /* format up success response */
      /* lock, access and unlock */
      halWriteDAC(tmpByte,unformatShort(&data[2]));
      moniInsertSetDACMessage(hal_FPGA_DOMAPP_get_local_clock(),
			      tmpByte, 
			      unformatShort(&data[2]));
      Message_setStatus(M,SUCCESS);
    }
    Message_setDataLen(M,0);
    break;
  case DSC_SET_PMT_HV:
    /* save anode and dynode values for next msg */
    PMT_HVreq=unformatShort(data);
    /* check that requests are not over current limits */
    if(PMT_HVreq > PMT_HV_max) {
      domsc.msgProcessingErr++;
      domsc.lastErrorID=DSC_PMT_HV_request_too_high;
      strcpy(domsc.lastErrorStr,
	     DSC_PMT_HV_REQUEST_TOO_HIGH);
      domsc.lastErrorSeverity=FATAL_ERROR;
      Message_setDataLen(M,0);
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|FATAL_ERROR);
      break;
    }
    else if((!testDOMconstraints(DOM_CONSTRAINT_NO_HV_CHANGE))
	    || DOM_state==DOM_FB_RUN_IN_PROGRESS){
      /* format up failure response */
      Message_setDataLen(M,0);
      domsc.msgProcessingErr++;
      strcpy(domsc.lastErrorStr,DSC_VIOLATES_CONSTRAINTS);
      domsc.lastErrorID=DSC_violates_constraints;
      domsc.lastErrorSeverity=WARNING_ERROR;
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|WARNING_ERROR);
      break;
    }
    halWriteBaseDAC(PMT_HVreq);
    moniInsertSetPMT_HV_Message(hal_FPGA_DOMAPP_get_local_clock(),PMT_HVreq);
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;

  case DSC_ENABLE_PMT_HV:
    if(!testDOMconstraints(DOM_CONSTRAINT_NO_HV_CHANGE)){
      /* format up failure response */
      Message_setDataLen(M,0);
      domsc.msgProcessingErr++;
      strcpy(domsc.lastErrorStr,DSC_VIOLATES_CONSTRAINTS);
      domsc.lastErrorID=DSC_violates_constraints;
      domsc.lastErrorSeverity=WARNING_ERROR;
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|WARNING_ERROR);
      break;
    }
    /* lock, access and unlock */
    halPowerUpBase();
    halEnableBaseHV();
    moniInsertEnablePMT_HV_Message(hal_FPGA_DOMAPP_get_local_clock());
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_DISABLE_PMT_HV:
    /* lock, access and unlock */
    halPowerDownBase();
    moniInsertDisablePMT_HV_Message(hal_FPGA_DOMAPP_get_local_clock());		
    /* format up success response */
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_SET_PMT_HV_LIMIT:
    /* store maximum value */
    PMT_HV_max=unformatShort(data);
    moniInsertSetPMT_HV_Limit_Message(hal_FPGA_DOMAPP_get_local_clock(), PMT_HV_max);
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_GET_PMT_HV_LIMIT:
    /* fetch maximum value */
    formatShort(PMT_HV_max,data);
    Message_setDataLen(M,DSC_GET_PMT_HV_LIMIT_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_QUERY_PMT_HV:
    //data[0]=halPMT_HVisEnabled();
    data[0]=0;
    data[1]=0;
    formatShort(halReadBaseADC(),&data[2]);
    formatShort(halReadBaseDAC(),&data[4]);
    Message_setDataLen(M,DSC_QUERY_PMT_HV_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_READ_ONE_ADC_REPT:
    tmpByte=data[0];
    if(tmpByte>=MAX_NUM_ADCS) {
      /* format up failure response */
      domsc.msgProcessingErr++;
      Message_setDataLen(M,0);
      strcpy(domsc.lastErrorStr,DSC_ILLEGAL_ADC_CHANNEL);
      domsc.lastErrorID=DSC_Illegal_ADC_Channel;
      domsc.lastErrorSeverity=FATAL_ERROR;
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|FATAL_ERROR);
    }
    else {
      tmpShort=readADCrept(tmpByte,data[1],&data[2]);
      /* format up success response */
      /* readADCrept() returns number of USHORT samples */
      Message_setDataLen(M,(tmpShort*2)+4);
      Message_setStatus(M,SUCCESS);
    }
    break;
  case DSC_SET_TRIG_MODE:
    /* store trigger mode, data access routines are
       responsible for checking legal trigger mode values */
    FPGA_trigger_mode = data[0];
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_GET_TRIG_MODE:
    /* return trigger mode */
    data[0] = FPGA_trigger_mode;
    Message_setDataLen(M,DSC_GET_TRIG_MODE_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_SELECT_ATWD:
    /* store ATWD select value */
    if(data[0] == 0) {
      FPGA_ATWD_select = 0;
    }
    else {
      FPGA_ATWD_select = 1;
    }
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_WHICH_ATWD:
    /* return ATWD select value */
    data[0] = (UBYTE)FPGA_ATWD_select;
    Message_setDataLen(M,DSC_WHICH_ATWD_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_MUX_SELECT:
    /* select mux channel for ATWD channel 3 */
    selected_mux_channel = data[0];
    halSelectAnalogMuxInput(selected_mux_channel);
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_WHICH_MUX:
    /* return selected mux channel */
    data[0] = selected_mux_channel;
    Message_setDataLen(M,DSC_WHICH_MUX_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_SET_PULSER_RATE:
    pulser_rate = unformatShort(&data[0]);
    // hal set pulser rate (pulser_rate)
    hal_FPGA_TEST_set_pulser_rate(pulser_rate);
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_GET_PULSER_RATE:
    formatShort(pulser_rate, &data[0]);
    Message_setDataLen(M,DSC_GET_PULSER_RATE_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_SET_PULSER_ON:
    pulser_running = TRUE;
    // hal set pulser running
    hal_FPGA_TEST_enable_pulser();
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_SET_PULSER_OFF:
    pulser_running = FALSE;
    // hal set pulser  off
    hal_FPGA_TEST_disable_pulser();
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_PULSER_RUNNING:
    data[0] = pulser_running;
    Message_setDataLen(M,DSC_PULSER_RUNNING_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_GET_RATE_METERS:
    formatLong((ULONG)hal_FPGA_TEST_get_spe_rate(),
	       &data[0]);
    formatLong((ULONG)hal_FPGA_TEST_get_mpe_rate(),
	       &data[4]);
    Message_setDataLen(M,DSC_GET_RATE_METERS_LEN);
    Message_setStatus(M,SUCCESS);
    break;  
  case DSC_SET_SCALER_DEADTIME:
    deadTime = unformatLong(data);
    /* HAL expects an int here, but deadTime is passed as
       ULONG (unsigned) for convenience; should probably check
       for negative number, but just give to HAL as is for now; for 
       negatives HAL will just set to 50*2^15 nsec. */
    hal_FPGA_TEST_set_deadtime((int)deadTime);
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_GET_SCALER_DEADTIME:
    formatLong(deadTime,&data[0]);
    Message_setDataLen(M,DSC_GET_SCALER_DEADTIME_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_SET_LOCAL_COIN_MODE:
    Message_setDataLen(M,0);
    if(data[0] == 0) {
      LCmode = data[0];
      hal_FPGA_TEST_disable_spe_lc();
      moniInsertLCModeChangeMessage(hal_FPGA_DOMAPP_get_local_clock(), 
				    LCmode);
      Message_setStatus(M,SUCCESS);
    } else if(data[0] == 1) {
      LCmode = data[0];
      hal_FPGA_TEST_enable_spe_lc(1,1);
      moniInsertLCModeChangeMessage(hal_FPGA_DOMAPP_get_local_clock(),
				    LCmode);
      Message_setStatus(M,SUCCESS);
    } else if(data[0] == 2) { /* Upper ONLY */
      LCmode = data[0];
      hal_FPGA_TEST_enable_spe_lc(0, 1);
      moniInsertLCModeChangeMessage(hal_FPGA_DOMAPP_get_local_clock(),
				    LCmode);
      Message_setStatus(M,SUCCESS);
    } else if(data[0] == 3) { /* Lower ONLY */
      LCmode = data[0];
      hal_FPGA_TEST_enable_spe_lc(1, 0);
      moniInsertLCModeChangeMessage(hal_FPGA_DOMAPP_get_local_clock(),
				    LCmode);
      Message_setStatus(M,SUCCESS);
    } else {
      domsc.msgProcessingErr++;
      strcpy(domsc.lastErrorStr,DSC_ILLEGAL_LC_MODE);
      domsc.lastErrorID=DSC_Illegal_LC_Mode;
      domsc.lastErrorSeverity=FATAL_ERROR;
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|FATAL_ERROR);
    }
    break;
  case DSC_GET_LOCAL_COIN_MODE:
    data[0] = LCmode;
    Message_setDataLen(M,1);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_SET_LOCAL_COIN_WINDOW:
    up_pre_ns  = unformatLong(&data[0]);
    up_post_ns = unformatLong(&data[4]);
    dn_pre_ns  = unformatLong(&data[8]);
    dn_post_ns = unformatLong(&data[12]);
    Message_setDataLen(M,0);

    if(hal_FPGA_TEST_set_lc_launch_window(up_pre_ns, up_post_ns,
					  dn_pre_ns, dn_post_ns)) {
      domsc.msgProcessingErr++;
      strcpy(domsc.lastErrorStr,DSC_LC_WINDOW_FAIL);
      domsc.lastErrorID=DSC_LC_Window_Fail;
      domsc.lastErrorSeverity=FATAL_ERROR;
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|FATAL_ERROR);
    } else {
      Message_setStatus(M,SUCCESS);
      moniInsertLCWindowChangeMessage(hal_FPGA_DOMAPP_get_local_clock(),
				      up_pre_ns, up_post_ns,
				      dn_pre_ns, dn_post_ns);
    }		 
    break;
  case DSC_GET_LOCAL_COIN_WINDOW:
    formatLong(up_pre_ns,  &data[0]);
    formatLong(up_post_ns, &data[4]);
    formatLong(dn_pre_ns,  &data[8]);
    formatLong(dn_post_ns, &data[12]);
    Message_setDataLen(M,16);
    Message_setStatus(M,SUCCESS);
    break;
    /*-----------------------------------
      unknown service request (i.e. message
      subtype), respond accordingly */
  default:
    domsc.msgRefused++;
    strcpy(domsc.lastErrorStr,DSC_ERS_BAD_MSG_SUBTYPE);
    domsc.lastErrorID=COMMON_Bad_Msg_Subtype;
    domsc.lastErrorSeverity=WARNING_ERROR;
    Message_setDataLen(M,0);
    Message_setStatus(M,
		      UNKNOWN_SUBTYPE|WARNING_ERROR);
    break;
  }
}

