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
#include "hal/DOM_FPGA_domapp_regs.h"

#include "message.h"
#include "moniDataAccess.h"
#include "dataAccessRoutines.h"
#include "domSControl.h"
#include "slowControl.h"
#include "messageAPIstatus.h"
#include "commonServices.h"
#include "commonMessageAPIstatus.h"
#include "domSControlRoutines.h"
#include "DSCmessageAPIstatus.h"
#include "DOMstateInfo.h"

/* Exported variables */
UBYTE SNRequestMode;
unsigned SNRequestDeadTime;
int SNRequested = 0;

/* extern functions */
extern void formatLong(ULONG value, UBYTE *buf);
extern void formatShort(USHORT value, UBYTE *buf);
extern ULONG unformatLong(UBYTE *buf);
extern USHORT unformatShort(UBYTE *buf);
extern void updateTriggerModes(void);
extern UBYTE DOM_state;
extern UBYTE DOM_config_access;

/* global storage */
extern UBYTE FPGA_trigger_mode;
extern int FPGA_ATWD_select;

/* local functions, data */
USHORT PMT_HV_max          = PMT_HV_DEFAULT_MAX;
int   pulser_running       = 0;
USHORT pulser_rate         = 1; /* By default, now take forced triggers at 1 Hz */
UBYTE selected_mux_channel = 0;
ULONG deadTime             = 100;
UBYTE LCmode               = 0;
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
#define MAXDISTNS 3175
UBYTE LCtype               = LC_TYPE_HARD;
UBYTE LCtx                 = LC_TX_BOTH;
UBYTE LCsrc                = 0;
UBYTE LCspan               = 1;
USHORT LCupLengths[4];
USHORT LCdnLengths[4];
UBYTE LClengthsSet         = 0;
//ULONG up_pre_ns, up_post_ns, dn_pre_ns, dn_post_ns;
#define LC_WIN_DEFAULT 200
ULONG pre_ns               = LC_WIN_DEFAULT;
ULONG post_ns              = LC_WIN_DEFAULT;

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

void set_HAL_lc_mode() {
  mprintf("set_HAL_lc_mode(LCmode=%d, LCtype=%d)", LCmode, LCtype);
  if(LCmode == LC_MODE_NONE) {
    hal_FPGA_DOMAPP_lc_mode(HAL_FPGA_DOMAPP_LC_MODE_OFF);
  } else if(LCmode == LC_MODE_SLC_ONLY) {
    hal_FPGA_DOMAPP_lc_mode(HAL_FPGA_DOMAPP_LC_MODE_SOFT);
  } else {
    switch(LCtype) {
    case LC_TYPE_NONE:   hal_FPGA_DOMAPP_lc_mode(HAL_FPGA_DOMAPP_LC_MODE_OFF);    break;
    case LC_TYPE_FLABBY: hal_FPGA_DOMAPP_lc_mode(HAL_FPGA_DOMAPP_LC_MODE_FLABBY); break;
    case LC_TYPE_SOFT:   hal_FPGA_DOMAPP_lc_mode(HAL_FPGA_DOMAPP_LC_MODE_SOFT);   break;
    case LC_TYPE_HARD:
    default:             hal_FPGA_DOMAPP_lc_mode(HAL_FPGA_DOMAPP_LC_MODE_HARD);   break;
    }
  }
}

void setLCmodeAndTx() {
  /* Enables both triggering by and TX of LC signals */

  mprintf("setLCmodeAndTx(LCmode=%d, LCtx=%d)", LCmode, LCtx);

  set_HAL_lc_mode(); /* DISABLES LC if LCmode == 0 */

  unsigned txbits;
  unsigned rxbits;
  switch(LCmode) { 
  case LC_MODE_NONE:
    rxbits = 0;
    break;
  case LC_MODE_UP:
    rxbits = HAL_FPGA_DOMAPP_LC_ENABLE_RCV_UP;
    break;
  case LC_MODE_DN:
    rxbits = HAL_FPGA_DOMAPP_LC_ENABLE_RCV_DOWN;
    break;
  case LC_MODE_SLC_ONLY:
    rxbits = 0; /* SLC-only means TX LC signals but don't require them for triggers (no RX) */
    break;
  case LC_MODE_UP_AND_DOWN:
    rxbits = HAL_FPGA_DOMAPP_LC_ENABLE_RCV_UP 
      |      HAL_FPGA_DOMAPP_LC_ENABLE_RCV_DOWN
      |      HAL_FPGA_DOMAPP_LC_ENABLE_RECV_UP_AND_DOWN;
    break;
  default:
  case LC_MODE_BOTH:
    rxbits = HAL_FPGA_DOMAPP_LC_ENABLE_RCV_UP | HAL_FPGA_DOMAPP_LC_ENABLE_RCV_DOWN;
    break;
  }

  switch(LCtx) {
  case LC_TX_UP:
    txbits = HAL_FPGA_DOMAPP_LC_ENABLE_SEND_UP;
    break;
  case LC_TX_DN:
    txbits = HAL_FPGA_DOMAPP_LC_ENABLE_SEND_DOWN;
    break;
  case LC_TX_NONE:
    txbits = 0;
    break;
  case LC_TX_BOTH:
  default:
    txbits = HAL_FPGA_DOMAPP_LC_ENABLE_SEND_UP | HAL_FPGA_DOMAPP_LC_ENABLE_SEND_DOWN;
    break;
  }

  mprintf("Sending 0x%08x to hal_FPGA_DOMAPP_lc_enable", txbits|rxbits);
  hal_FPGA_DOMAPP_lc_enable( txbits | rxbits );
}

void updateLCsrc(void) {
  mprintf("updateLCsrc(%d)", LCsrc);
  if(LCsrc == LC_SRC_MPE) 
    hal_FPGA_DOMAPP_lc_disc_mpe();
  else
    hal_FPGA_DOMAPP_lc_disc_spe();
}

void updateLCspan(void) { 
  mprintf("updateLCspan(%d)", LCspan);
  hal_FPGA_DOMAPP_lc_span(LCspan); 
}

int updateLCwindows(void) {
  mprintf("updateLCwindows(pre=%d, post=%d)", pre_ns, post_ns);
  if(hal_FPGA_DOMAPP_lc_windows(pre_ns, post_ns)) { 
    mprintf("WARNING: hal_FPGA_DOMAPP_lc_windows failed (pre=%d, post=%d), probably bad args", 
	    pre_ns, post_ns);
    pre_ns = post_ns = LC_WIN_DEFAULT;
    return 1;
  }
  return 0;
}

void updateLClengths(void) {
  if(LClengthsSet) {
    mprintf("updateLClengths UP %d %d %d %d DN %d %d %d %d", 
	    LCupLengths[0], LCupLengths[1], LCupLengths[2], LCupLengths[3],
	    LCdnLengths[0], LCdnLengths[1], LCdnLengths[2], LCdnLengths[3]);
    int ispan; for(ispan=0; ispan < 4; ispan++) {
      hal_FPGA_DOMAPP_lc_length_up(ispan, LCupLengths[ispan]);
      hal_FPGA_DOMAPP_lc_length_down(ispan, LCdnLengths[ispan]);
    }
  } else {
    //mprintf("updateLClengths: lengths not set, skipping");
  }
}

void dsc_hal_disable_LC_completely(void) {
  //mprintf("dsc_hal_disable_LC_completely ... disabling LC");
  hal_FPGA_DOMAPP_lc_mode(HAL_FPGA_DOMAPP_LC_MODE_OFF);
  hal_FPGA_DOMAPP_lc_enable(0);
}

void dsc_hal_do_LC_settings(void) {
  //mprintf("dsc_hal_do_LC_settings ... setting up LC");
  setLCmodeAndTx();
  updateLCsrc();
  updateLCspan();
  updateLClengths();
  updateLCwindows();
}

int doStartSN(UBYTE mode, unsigned deadtime) {
  if(hal_FPGA_DOMAPP_sn_dead_time(deadtime)) {
    mprintf("ERROR: hal_FPGA_DOMAPP_sn_dead_time FAILED!\n");
    return -1;
  }
  hal_FPGA_DOMAPP_sn_mode(mode ? HAL_FPGA_DOMAPP_SN_MODE_MPE : HAL_FPGA_DOMAPP_SN_MODE_SPE);
  return 0;
}

void doStopSN(void) {
  hal_FPGA_DOMAPP_sn_mode(HAL_FPGA_DOMAPP_SN_MODE_OFF);
}

#define DOERROR(errstr, errnum, errtype)                       \
   do {                                                        \
      domsc.msgProcessingErr++;                                \
      strcpy(domsc.lastErrorStr,(errstr));                     \
      domsc.lastErrorID=(errnum);                              \
      domsc.lastErrorSeverity=errtype;                         \
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|errtype);     \
      Message_setDataLen(M,0);                                 \
   } while(0)

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
      DOERROR(DSC_ILLEGAL_ADC_CHANNEL, DSC_Illegal_ADC_Channel, FATAL_ERROR);
    } else {
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
      DOERROR(DSC_ILLEGAL_DAC_CHANNEL, DSC_Illegal_DAC_Channel, FATAL_ERROR);
    } else {
      formatShort(halReadDAC(tmpByte),data);
      Message_setDataLen(M,DSC_READ_ONE_DAC_LEN);
      Message_setStatus(M,SUCCESS);
    }
    break;
  case DSC_WRITE_ONE_DAC:
    tmpByte=data[0];
    if(Message_dataLen(M)!=DSC_WRITE_ONE_DAC_REQ_LEN){
      DOERROR(DSC_ERS_BAD_MSG_FORMAT, COMMON_Bad_Msg_Format, FATAL_ERROR);
    } else if(!testDOMconstraints(DOM_CONSTRAINT_NO_DAC_CHANGE)){
      DOERROR(DSC_VIOLATES_CONSTRAINTS, DSC_violates_constraints, WARNING_ERROR);
    } else if(tmpByte>=MAX_NUM_DACS) {
      DOERROR(DSC_ILLEGAL_DAC_CHANNEL, DSC_Illegal_DAC_Channel, FATAL_ERROR);
    } else {
      halWriteDAC(tmpByte,unformatShort(&data[2]));
      moniInsertSetDACMessage(hal_FPGA_DOMAPP_get_local_clock(),
			      tmpByte, 
			      unformatShort(&data[2]));
      Message_setStatus(M,SUCCESS);
      Message_setDataLen(M,0);
    }
    break;
  case DSC_SET_PMT_HV:
    /* save anode and dynode values for next msg */
    PMT_HVreq=unformatShort(data);
    /* check that requests are not over current limits */
    if(PMT_HVreq > PMT_HV_max) {
      DOERROR(DSC_PMT_HV_REQUEST_TOO_HIGH, DSC_PMT_HV_request_too_high, FATAL_ERROR);
      break;
    } else if((!testDOMconstraints(DOM_CONSTRAINT_NO_HV_CHANGE))
	    || DOM_state==DOM_FB_RUN_IN_PROGRESS){
      DOERROR(DSC_VIOLATES_CONSTRAINTS, DSC_violates_constraints, WARNING_ERROR);
      break;
    }
    halWriteBaseDAC(PMT_HVreq);
    moniInsertSetPMT_HV_Message(hal_FPGA_DOMAPP_get_local_clock(),PMT_HVreq);
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;

  case DSC_ENABLE_PMT_HV:
    if(!testDOMconstraints(DOM_CONSTRAINT_NO_HV_CHANGE)){
      DOERROR(DSC_VIOLATES_CONSTRAINTS, DSC_violates_constraints, WARNING_ERROR);
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
    formatShort(domappReadBaseADC(),&data[0]);
    formatShort(halReadBaseDAC(),&data[2]);
    Message_setDataLen(M,DSC_QUERY_PMT_HV_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_READ_ONE_ADC_REPT:
    tmpByte=data[0];
    if(tmpByte>=MAX_NUM_ADCS) {
      DOERROR(DSC_ILLEGAL_ADC_CHANNEL, DSC_Illegal_ADC_Channel, FATAL_ERROR);
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
    if(selected_mux_channel == 0xFF) {
      halDisableAnalogMux(); /* Per John Kelley request */
    } else {
      halSelectAnalogMuxInput(selected_mux_channel);
    }
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
    double setRate = hal_FPGA_DOMAPP_cal_pulser_rate((double) pulser_rate);
    mprintf("Setting pulser rate to %hu Hz (rounded by HAL to %d Hz; CAL_CONTROL=0x%08lx))", 
	    pulser_rate, (int) setRate, FPGA(CAL_CONTROL));
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_GET_PULSER_RATE:
    formatShort(pulser_rate, &data[0]);
    Message_setDataLen(M,DSC_GET_PULSER_RATE_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_SET_PULSER_ON:
    if(FPGA_trigger_mode != SPE_DISC_TRIG_MODE && FPGA_trigger_mode != MPE_DISC_TRIG_MODE) {
      DOERROR(DSC_VIOLATES_CONSTRAINTS, DSC_violates_constraints, WARNING_ERROR);
      break;
    }
    mprintf("Turned on front-end pulser");
    pulser_running = TRUE;
    updateTriggerModes();
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;

  case DSC_SET_PULSER_OFF:
    pulser_running = FALSE;

    if(FPGA_trigger_mode != SPE_DISC_TRIG_MODE && FPGA_trigger_mode != MPE_DISC_TRIG_MODE) {
      DOERROR(DSC_VIOLATES_CONSTRAINTS, DSC_violates_constraints, WARNING_ERROR);
      break;
    }
    /* Go back to normal SPE mode */
    updateTriggerModes();
    mprintf("Turned off front-end pulser");
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_PULSER_RUNNING:
    data[0] = pulser_running;
    Message_setDataLen(M,DSC_PULSER_RUNNING_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_GET_RATE_METERS:
    formatLong((ULONG)hal_FPGA_DOMAPP_spe_rate_immediate(),
	       &data[0]);
    formatLong((ULONG)hal_FPGA_DOMAPP_mpe_rate_immediate(),
	       &data[4]);
    Message_setDataLen(M,DSC_GET_RATE_METERS_LEN);
    Message_setStatus(M,SUCCESS);
    break;  
  case DSC_SET_SCALER_DEADTIME:
    { 
      int dt = unformatLong(data);
      if(dt < 100 || dt > 102400) {
	DOERROR("Bad value for scaler deadtime", DSC_bad_deadtime, FATAL_ERROR);
      } else {
	deadTime = dt;
	hal_FPGA_DOMAPP_rate_monitor_deadtime((int)deadTime);
	mprintf("Set scaler deadtime to %d nsec.", dt);
	Message_setDataLen(M,0);
	Message_setStatus(M,SUCCESS);
      }
    }
    break;
  case DSC_GET_SCALER_DEADTIME:
    formatLong(deadTime,&data[0]);
    Message_setDataLen(M,DSC_GET_SCALER_DEADTIME_LEN);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_SET_LOCAL_COIN_MODE:
    if(data[0] > MAX_LC_MODE) {
      DOERROR(DSC_ILLEGAL_LC_MODE, DSC_Illegal_LC_Mode, FATAL_ERROR);
      break;
    } 
    LCmode = data[0];
    setLCmodeAndTx();
    moniInsertLCModeChangeMessage(hal_FPGA_DOMAPP_get_local_clock(), 
				  LCmode);
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_GET_LOCAL_COIN_MODE:
    data[0] = LCmode;
    Message_setDataLen(M,1);
    Message_setStatus(M,SUCCESS);
    break;
  case DSC_SET_LOCAL_COIN_WINDOW:
    pre_ns   = unformatLong(&data[0]);
    post_ns  = unformatLong(&data[4]);
    Message_setDataLen(M,0);
    
    if(updateLCwindows()) {
      pre_ns = post_ns = LC_WIN_DEFAULT;
      DOERROR(DSC_LC_WINDOW_FAIL, DSC_LC_Window_Fail, FATAL_ERROR);
    } else {
      Message_setStatus(M,SUCCESS);
      moniInsertLCWindowChangeMessage(hal_FPGA_DOMAPP_get_local_clock(),
				      pre_ns, post_ns);
    }		 
    break;

  case DSC_GET_LOCAL_COIN_WINDOW:
    formatLong(pre_ns,  &data[0]);
    formatLong(post_ns, &data[4]);
    Message_setDataLen(M,8);
    Message_setStatus(M,SUCCESS);
    break;

  case DSC_SET_LC_TYPE:
    Message_setDataLen(M,0);
    { 
      UBYTE lct = data[0];
      if(lct != LC_TYPE_SOFT && lct != LC_TYPE_HARD && lct != LC_TYPE_FLABBY) {
	DOERROR("Invalid local coincidence type", DSC_LC_Bad_Type, FATAL_ERROR);
      } else {
	LCtype = lct;
	set_HAL_lc_mode();
	Message_setStatus(M,SUCCESS);
      }
    }
    break;

  case DSC_GET_LC_TYPE:
    data[0] = LCtype;
    Message_setDataLen(M,1);
    Message_setStatus(M,SUCCESS);
    break;

  case DSC_SET_LC_TX:
    {
      UBYTE lctx = data[0];
      if(lctx != LC_TX_NONE && lctx != LC_TX_UP && lctx != LC_TX_DN && lctx != LC_TX_BOTH) {
	DOERROR("Invalid local coincidence transmit (TX) setting", DSC_LC_Bad_Tx, FATAL_ERROR);
      } else {
        LCtx = lctx;
	setLCmodeAndTx();
        Message_setStatus(M,SUCCESS);
	Message_setDataLen(M,0);
      }
    }
    break;

  case DSC_GET_LC_TX:
    data[0] = LCtx;
    Message_setDataLen(M,1);
    Message_setStatus(M,SUCCESS);
    break;

  case DSC_SET_LC_SRC:
    {
      UBYTE lcs = data[0];
      if(lcs != LC_SRC_SPE && lcs != LC_SRC_MPE) {
	DOERROR("Invalid local coincidence source (SPE/MPE)", DSC_LC_Bad_Src, FATAL_ERROR);
      } else {
        LCsrc = lcs;
	updateLCsrc();
        Message_setStatus(M,SUCCESS);
	Message_setDataLen(M,0);
      }
    }
    break;

  case DSC_GET_LC_SRC:
    data[0] = LCsrc;
    Message_setDataLen(M,1);
    Message_setStatus(M,SUCCESS);
    break;

  case DSC_SET_LC_SPAN:
    {
      UBYTE lcsp = data[0];
      if(lcsp < 1 || lcsp > 4) {
	DOERROR("Invalid local coincidence span number (1..4)", DSC_LC_Bad_Span, FATAL_ERROR);
      } else {
        LCspan = lcsp;
	updateLCspan();
	Message_setDataLen(M,0);
        Message_setStatus(M,SUCCESS);
      }
    }
    break;

  case DSC_GET_LC_SPAN:
    data[0] = LCspan;
    Message_setDataLen(M,1);
    Message_setStatus(M,SUCCESS);
    break;

  case DSC_SET_LC_CABLE_LEN:
    Message_setDataLen(M,0);
    Message_setStatus(M,SUCCESS);
    LClengthsSet = 1;
    { 
      int ispan; for(ispan = 0; ispan < 4; ispan++) {
	USHORT iup = unformatShort(data+ispan*2);
	USHORT idn = unformatShort(data+8+ispan*2);
	if(iup > MAXDISTNS || idn > MAXDISTNS) {
	  DOERROR("Invalid local coincidence cable length, must be < 25*127", DSC_LC_Bad_Len,
		  FATAL_ERROR);
	  LClengthsSet = 0; /* Changed our minds! */
	  break;
	}
	LCupLengths[ispan] = iup;
	LCdnLengths[ispan] = idn;
	updateLClengths();
      }
    }
    break;
  case DSC_GET_LC_CABLE_LEN:
    if(!LClengthsSet) { /* Don't allow this if lengths aren't set! */
      DOERROR("Local coincidence lengths NOT previously set by software -- "
	      "do DSC_SET_LC_CABLE_LEN first", DSC_LC_Len_Not_Set, FATAL_ERROR);
      break;
    }
    {
      int ispan; for(ispan = 0; ispan < 4; ispan++) {
	formatShort(LCupLengths[ispan], data+ispan*2);
	formatShort(LCdnLengths[ispan], data+8+ispan*2);
      }
    }
    Message_setDataLen(M,16);
    Message_setStatus(M,SUCCESS);
    break;

  case DSC_ENABLE_SN: 
    { 
      if(DOM_state != DOM_IDLE) {
	DOERROR("DSC_ENABLE_SN: ERROR: DOM not in idle state!", DSC_SN_DataTakingInProgress,
		FATAL_ERROR);
	break;
      }
      unsigned deadtime = unformatLong(data);
      UBYTE mode        = data[4];
      if(mode != 0 && mode != 1) {
	DOERROR("DSC_ENABLE_SN: Mode must be 0 (SPE) or 1 (MPE)",
		DSC_Bad_SN_Mode, FATAL_ERROR);
	break;
      }
      if(deadtime < 6400 || deadtime > 512000) {
	DOERROR("DSC_ENABLE_SN: Deadtime must be < 6400 or > 512000",
		DSC_Bad_SN_Deadtime, FATAL_ERROR);
	break;
      }

      /* Enable SN request so SN is started when run is started */
      SNRequestMode     = mode;
      SNRequestDeadTime = deadtime;
      SNRequested       = 1;
      mprintf("Requested supernova data collection for next run (deadtime=%d, mode=%s)",
	      deadtime, mode?"MPE":"SPE");
      Message_setStatus(M,SUCCESS);
      Message_setDataLen(M,0);
    }
    break;

  case DSC_DISABLE_SN:
    mprintf("Removed request for supernova data taking");
    SNRequested = 0;
    // Explicitly disable supernova in case data taking is [pathologically] still in progress:
    doStopSN();
    Message_setStatus(M,SUCCESS);
    Message_setDataLen(M,0);
    break;

  case DSC_SET_CHARGE_STAMP_TYPE:
    {
      UBYTE mode      = data[0]; /* 0=ATWD 1=FADC */
      if(mode != 0 && mode != 1) {
	mprintf("ERROR: charge stamp mode is 0x%02x!", mode);
	DOERROR("bad charge stamp mode", 0, 0);
	break;
      }
      UBYTE chsel     = data[1]; /* 0=auto 1=fixed channel selection 
				    (ATWD only; ignored for FADC)  */
      if(chsel != 0 && chsel != 1) {
	mprintf("ERROR: channel selection arg is 0x%02x!", chsel);
	DOERROR("bad channel selection arg", 0, 0);
	break;
      }
      UBYTE ch        = data[2]; /* If fixed channel selection, which byte?
				    (ATWD only; ignored for FADC) */
      /* data[3] is spare */
      USHORT thresh   = unformatShort(&data[4]); /* Threshold for threshold-bin 
						    selection (ATWD only) */
      mprintf("Set charge stamp type: mode=%d chsel=%d ch=%d thresh=%d",
	      mode, chsel, ch, thresh);
    }
    Message_setStatus(M,SUCCESS);
    Message_setDataLen(M,0);
    break;

  default:
    DOERROR(DSC_ERS_BAD_MSG_SUBTYPE, COMMON_Bad_Msg_Subtype, WARNING_ERROR);
    break;
  }
}

