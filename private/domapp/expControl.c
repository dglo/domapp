/* expControl.c */

/*
Author: Chuck McParland
Start Date: May 4, 1999
Description:
	DOM Experiment Control service thread to handle
	special run-related functions.  Performs all 
	"standard" DOM service functions.
Last Modification:
*/

#include <string.h>

/* DOM-related includes */
#include "hal/DOM_MB_types.h"
#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_domapp.h"
#include "hal/DOM_FPGA_domapp_regs.h"

#include "DOMtypes.h"
#include "DOMdata.h"
#include "message.h"
#include "expControl.h"
#include "messageAPIstatus.h"
#include "commonServices.h"
#include "commonMessageAPIstatus.h"
#include "EXPmessageAPIstatus.h"
#include "DOMstateInfo.h"
#include "moniDataAccess.h"
#include "dataAccessRoutines.h"

/* extern functions */
extern void formatLong(ULONG value, UBYTE *buf);
extern BOOLEAN beginRun(UBYTE compressionMode);
extern BOOLEAN endRun(void);
extern BOOLEAN beginFBRun(USHORT bright, USHORT window, USHORT delay, USHORT mask, USHORT rate);
extern BOOLEAN endFBRun(void);
extern BOOLEAN forceRunReset(void);
extern UBYTE   compMode;

/* local functions, data */
UBYTE DOM_state;
UBYTE DOM_config_access;
UBYTE DOM_status;
UBYTE DOM_cmdSource;
ULONG DOM_constraints;
char DOM_errorString[DOM_STATE_ERROR_STR_LEN];

/* struct that contains common service info for
	this service. */
COMMON_SERVICE_INFO expctl;

/* Pedestal pattern data */
int pedestalsAvail = 0;
ULONG  atwdpedsum[2][4][ATWDCHSIZ];
USHORT atwdpedavg[2][4][ATWDCHSIZ];
ULONG  fadcpedsum[FADCSIZ];
USHORT fadcpedavg[FADCSIZ];
ULONG npeds0, npeds1, npedsadc;

/* Actual data to be read out */
USHORT atwddata[2][4][ATWDCHSIZ];
USHORT fadcdata[FADCSIZ];

#define ATWD_TIMEOUT_COUNT 4000
#define ATWD_TIMEOUT_USEC 5

BOOLEAN beginPedestalRun(void) {
  if(DOM_state!=DOM_IDLE) {
    return FALSE;
  } else {
    DOM_state=DOM_PEDESTAL_COLLECTION_IN_PROGRESS;
    return TRUE;
  }
}

void forceEndPedestalRun(void) {
  DOM_state=DOM_IDLE;
}


void zeroPedestals() {
  memset((void *) fadcpedsum, 0, FADCSIZ * sizeof(ULONG));
  memset((void *) fadcpedavg, 0, FADCSIZ * sizeof(USHORT));
  memset((void *) atwdpedsum, 0, 2*4*ATWDCHSIZ*sizeof(ULONG));
  memset((void *) atwdpedsum, 0, 2*4*ATWDCHSIZ*sizeof(USHORT));
  pedestalsAvail = 0;
  npeds0 = npeds1 = npedsadc = 0;
}

int pedestalRun(ULONG ped0goal, ULONG ped1goal, ULONG pedadcgoal) {
  /* return 0 if pedestal run succeeds, else error */

  mprintf("Starting pedestal run...");
  hal_FPGA_DOMAPP_disable_daq();
  hal_FPGA_DOMAPP_lbm_reset();
  halUSleep(10000); /* make sure atwd is done... */
  hal_FPGA_DOMAPP_lbm_reset();
  unsigned lbmp = hal_FPGA_DOMAPP_lbm_pointer();
  hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_FORCED);
  hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FORCED);
  hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_FORCED);
  hal_FPGA_DOMAPP_daq_mode(HAL_FPGA_DOMAPP_DAQ_MODE_ATWD_FADC);
  hal_FPGA_DOMAPP_atwd_mode(HAL_FPGA_DOMAPP_ATWD_MODE_TESTING);
  hal_FPGA_DOMAPP_lc_mode(HAL_FPGA_DOMAPP_LC_MODE_OFF);
  hal_FPGA_DOMAPP_lbm_mode(HAL_FPGA_DOMAPP_LBM_MODE_WRAP);
  hal_FPGA_DOMAPP_compression_mode(HAL_FPGA_DOMAPP_COMPRESSION_MODE_OFF);
  hal_FPGA_DOMAPP_rate_monitor_enable(0);
  int dochecks               = 1;
  int didUncompressedWarning = 0;  
  int didATWDSizeWarning     = 0;
  int didMissedTrigWarning   = 0;
  int didMissingFADCWarning  = 0;
  int didMissingATWDWarning  = 0;
  int iatwd; for(iatwd=0;iatwd<2;iatwd++) {
    int numMissedTriggers = 0;
    int numTrigs = (iatwd==0 ? ped0goal : ped1goal);      
    hal_FPGA_DOMAPP_enable_atwds(iatwd==0?HAL_FPGA_DOMAPP_ATWD_A:HAL_FPGA_DOMAPP_ATWD_B);
    hal_FPGA_DOMAPP_enable_daq(); 
    int isamp;
    int it; for(it=0; it<numTrigs; it++) {
      hal_FPGA_DOMAPP_cal_launch();
      halUSleep(500);
      if(lbmp == hal_FPGA_DOMAPP_lbm_pointer()) {
	numMissedTriggers++;
	if(!didMissedTrigWarning) {
	  didMissedTrigWarning++;
	  mprintf("pedestalRun: WARNING: missed one or more calibration triggers for ATWD %d!", iatwd);
	}
	continue;
      }

      lbmp = hal_FPGA_DOMAPP_lbm_pointer();
      unsigned char * e = lbmEvent(lbmp);
      struct tstevt * hdr = (struct tstevt *) e;
      struct tstwords {
	unsigned long w0, w1, w2, w3;
      } * words = (struct tstwords *) e;
      unsigned atwdsize = (hdr->trigbits>>19)&0x3;
      unsigned long daq = FPGA(DAQ);
      if(dochecks && words->w0>>31) {
	if(!didUncompressedWarning) {
	  didUncompressedWarning++;
	  mprintf("pedestalRun: WARNING: trying to collect waveforms for pedestals but "
		  "got COMPRESSED data!  w0=0x%08x w1=0x%08x w2=0x%08x w3=0x%08x DAQ=0x%08x",
		  words->w0, words->w1, words->w2, words->w3, daq);
	}
	numMissedTriggers++;
	continue;
      } 
      if(dochecks && atwdsize != 3) {
	if(!didATWDSizeWarning) {
	  didATWDSizeWarning++;
	  mprintf("pedestalRun: WARNING: atwdsize=%d should be 3!  w0=0x%08x w1=0x%08x w2=0x%08x w3=0x%08x DAQ=0x%08x",
		  atwdsize, words->w0, words->w1, words->w2, words->w3, daq);
	}
	numMissedTriggers++;
	continue;
      }
      
      /* Check valid FADC data present */
      if(dochecks && ! (hdr->trigbits & 1<<17)) {
	if(!didMissingFADCWarning) {
	  didMissingFADCWarning++;
	  mprintf("pedestalRun: WARNING: NO FADC data present in event!  trigbits=0x%08x", hdr->trigbits);
	}
	numMissedTriggers++;
	continue;
      }

      /* Check valid ATWD data present */
      if(dochecks && ! (hdr->trigbits & 1<<18)) {
	if(!didMissingATWDWarning) {
          didMissingATWDWarning++;
          mprintf("pedestalRun: WARNING: NO ATWD data present in event!  trigbits=0x%08x", hdr->trigbits);
        }
	numMissedTriggers++;
        continue;
      }

      // pull out fadc data
      unsigned short * fadc = (unsigned short *) (e+0x10);
      unsigned short * atwd = (unsigned short *) (e+0x210);

      /* Count the waveforms into the running sums */
      int ich; for(ich=0; ich<4; ich++)
	for(isamp=0; isamp<ATWDCHSIZ; isamp++) 
	  atwdpedsum[iatwd][ich][isamp] += atwd[ATWDCHSIZ*ich + isamp] & 0x3FF; //isamp*ich+iatwd;

      for(isamp=0; isamp<FADCSIZ; isamp++) fadcpedsum[isamp] += fadc[isamp] & 0x3FF; //FADCSIZ-isamp;

      if(iatwd==0) npeds0++; else npeds1++;
      npedsadc++;

    } /* loop over triggers */


    int npeds = iatwd==0 ? npeds0 : npeds1;
    int ich; for(ich=0; ich<4; ich++) {
      for(isamp=0; isamp<ATWDCHSIZ; isamp++) {
	if(npeds > 0) {
	  atwdpedavg[iatwd][ich][isamp] = atwdpedsum[iatwd][ich][isamp]/npeds;
	} else {
	  atwdpedavg[iatwd][ich][isamp] = 0;
	}
      }
      /* Program ATWD pedestal pattern into FPGA */
      hal_FPGA_DOMAPP_pedestal(iatwd, ich, atwdpedavg[iatwd][ich]);
    }
    
    
    for(isamp=0; isamp<FADCSIZ; isamp++) {
      if(npedsadc > 0) {
	fadcpedavg[isamp] = fadcpedsum[isamp]/npedsadc;
      } else {
	fadcpedavg[isamp] = 0;
      }
      /* Currently there is no setting of FADC pedestals in the FPGA! */
    }
			   
    mprintf("Number of pedestal triggers for ATWD %d is %d (%s, missed triggers=%d)", iatwd, npeds,
	    dochecks ? "checks were enabled" : "WARNING: checks were DISABLED", numMissedTriggers);

  } /* loop over atwds */

  hal_FPGA_DOMAPP_disable_daq();
  hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_OFF);

  if(npeds0 > 0 && npeds1 > 0 && npedsadc > 0) pedestalsAvail = 1;

  return 0;
}

/* Exp Control  Entry Point */
void expControlInit(void) {
    expctl.state=SERVICE_ONLINE;
    expctl.lastErrorID=COMMON_No_Errors;
    expctl.lastErrorSeverity=INFORM_ERROR;
    expctl.majorVersion=EXP_MAJOR_VERSION;
    expctl.minorVersion=EXP_MINOR_VERSION;
    strcpy(expctl.lastErrorStr,EXP_ERS_NO_ERRORS);
    expctl.msgReceived=0;
    expctl.msgRefused=0;
    expctl.msgProcessingErr=0;

    /* get DOM state lock and initialize state info */
    DOM_state=DOM_IDLE;
    DOM_config_access=DOM_CONFIG_CHANGES_ALLOWED;
    DOM_status=DOM_STATE_SUCCESS;
    DOM_cmdSource=EXPERIMENT_CONTROL;
    DOM_constraints=DOM_CONSTRAINT_NO_CONSTRAINTS;
    strcpy(DOM_errorString,DOM_STATE_STR_STARTUP);

    zeroPedestals();
}      

void expControl(MESSAGE_STRUCT *M) {
    USHORT bright=0, window=0, delay=0, mask=0, rate=0;
    UBYTE *data;
    UBYTE *tmpPtr;
    
    /* get address of data portion. */
    /* Receiver ALWAYS links a message */
    /* to a valid data buffer-even */ 
    /* if it is empty. */
    data=Message_getData(M);
    expctl.msgReceived++;
    switch ( Message_getSubtype(M) ) {
      /* Manditory Service SubTypes */
    case GET_SERVICE_STATE:
      /* get current state of Test Manager */
      data[0]=expctl.state;
      Message_setDataLen(M,GET_SERVICE_STATE_LEN);
      Message_setStatus(M,SUCCESS);
      break;
      
    case GET_LAST_ERROR_ID:
      /* get the ID of the last error encountered */
      data[0]=expctl.lastErrorID;
      data[1]=expctl.lastErrorSeverity;
      Message_setDataLen(M,GET_LAST_ERROR_ID_LEN);
      Message_setStatus(M,SUCCESS);
      break;
      
    case GET_SERVICE_VERSION_INFO:
      /* get the major and minor version of this */
      /*	Test Manager */
      data[0]=expctl.majorVersion;
      data[1]=expctl.minorVersion;
      Message_setDataLen(M,GET_SERVICE_VERSION_INFO_LEN);
      Message_setStatus(M,SUCCESS);
      break;
      
    case GET_SERVICE_STATS:
      /* get standard service statistics for */
      /*	the Test Manager */
      formatLong(expctl.msgReceived,&data[0]);
      formatLong(expctl.msgRefused,&data[4]);
      formatLong(expctl.msgProcessingErr,&data[8]);
      Message_setDataLen(M,GET_SERVICE_STATS_LEN);
      Message_setStatus(M,SUCCESS);
      break;
      
    case GET_LAST_ERROR_STR:
      /* get error string for last error encountered */
      strcpy(data,expctl.lastErrorStr);
      Message_setDataLen(M,strlen(expctl.lastErrorStr));
      Message_setStatus(M,SUCCESS);
      break;
      
    case CLEAR_LAST_ERROR:
      /* reset both last error ID and string */
      expctl.lastErrorID=COMMON_No_Errors;
      expctl.lastErrorSeverity=INFORM_ERROR;
      strcpy(expctl.lastErrorStr,EXP_ERS_NO_ERRORS);
      Message_setDataLen(M,0);
      Message_setStatus(M,SUCCESS);
      break;
      
    case REMOTE_OBJECT_REF:
      /* remote object reference */
      /*	TO BE IMPLEMENTED..... */
      expctl.msgProcessingErr++;
      strcpy(expctl.lastErrorStr,EXP_ERS_BAD_MSG_SUBTYPE);
      expctl.lastErrorID=COMMON_Bad_Msg_Subtype;
      expctl.lastErrorSeverity=WARNING_ERROR;
      Message_setDataLen(M,0);
      Message_setStatus(M,
			UNKNOWN_SUBTYPE|WARNING_ERROR);
      break;
      
    case GET_SERVICE_SUMMARY:
      /* init a temporary buffer pointer */
      tmpPtr=data;
      /* get current state of slow control */
      *tmpPtr++=expctl.state;
      /* get the ID of the last error encountered */
      *tmpPtr++=expctl.lastErrorID;
      *tmpPtr++=expctl.lastErrorSeverity;
      /* get the major and minor version of this */
      /*	exp control */
      *tmpPtr++=expctl.majorVersion;
      *tmpPtr++=expctl.minorVersion;
      /* get standard service statistics for */
      /*	the exp control */
      formatLong(expctl.msgReceived,tmpPtr);
      tmpPtr+=sizeof(ULONG);
      formatLong(expctl.msgRefused,tmpPtr);
      tmpPtr+=sizeof(ULONG);
      formatLong(expctl.msgProcessingErr,tmpPtr);
      tmpPtr+=sizeof(ULONG);
      /* get error string for last error encountered */
      strcpy(tmpPtr,expctl.lastErrorStr);
      Message_setDataLen(M,(int)(tmpPtr-data)+
			 strlen(expctl.lastErrorStr));
      Message_setStatus(M,SUCCESS);
      break;
      
      /*-------------------------------*/
      /* Experiment Control specific SubTypes */
      
      /* enable FPGA triggers */ 
    case EXPCONTROL_ENA_TRIG:
      if (1==0) {
	/* disabled for test versions */
	expctl.msgProcessingErr++;
	strcpy(expctl.lastErrorStr,EXP_CANNOT_START_TRIG);
	expctl.lastErrorID=EXP_Cannot_Start_Trig;
	expctl.lastErrorSeverity=WARNING_ERROR;
	Message_setStatus(M,SERVICE_SPECIFIC_ERROR|
			  WARNING_ERROR);
      }
      else {
	Message_setStatus(M,SUCCESS);
      }
      Message_setDataLen(M,0);
      break;
      
      /* disable FPGA triggers */
    case EXPCONTROL_DIS_TRIG:
      if (1==0) {
	/* disabled for test versions */
	expctl.msgProcessingErr++;
	strcpy(expctl.lastErrorStr,EXP_CANNOT_STOP_TRIG);
	expctl.lastErrorID=EXP_Cannot_Stop_Trig;
	expctl.lastErrorSeverity=WARNING_ERROR;
	Message_setStatus(M,SERVICE_SPECIFIC_ERROR|
			  WARNING_ERROR);
      }
      else {
	Message_setStatus(M,SUCCESS);
      }
      Message_setDataLen(M,0);
      break;
      
      /* begin run */ 
    case EXPCONTROL_BEGIN_RUN:
      if (!beginRun(compMode)) {
	expctl.msgProcessingErr++;
	strcpy(expctl.lastErrorStr,EXP_CANNOT_BEGIN_RUN);
	expctl.lastErrorID=EXP_Cannot_Begin_Run;
	expctl.lastErrorSeverity=SEVERE_ERROR;
	Message_setStatus(M,SERVICE_SPECIFIC_ERROR|
			  SEVERE_ERROR);
      } else {
	Message_setStatus(M,SUCCESS);
      }
      Message_setDataLen(M,0);
      break;
      
    case EXPCONTROL_BEGIN_FB_RUN:
      tmpPtr = data;
      bright = unformatShort(&data[0]);
      window = unformatShort(&data[2]);
      delay  = unformatShort(&data[4]);
      mask   = unformatShort(&data[6]);
      rate   = unformatShort(&data[8]);
      if (!beginFBRun(bright, window, delay, mask, rate)) {
        expctl.msgProcessingErr++;
        strcpy(expctl.lastErrorStr,EXP_CANNOT_BEGIN_FB_RUN);
        expctl.lastErrorID=EXP_Cannot_Begin_FB_Run;
        expctl.lastErrorSeverity=SEVERE_ERROR;
        Message_setStatus(M,SERVICE_SPECIFIC_ERROR|
                          SEVERE_ERROR);
      }
      else {
        Message_setStatus(M,SUCCESS);
      }
      Message_setDataLen(M,0);
      break;

      /* end run */ 
    case EXPCONTROL_END_RUN:
      if (!endRun()) {
	expctl.msgProcessingErr++;
	strcpy(expctl.lastErrorStr,EXP_CANNOT_END_RUN);
	expctl.lastErrorID=EXP_Cannot_End_Run;
	expctl.lastErrorSeverity=SEVERE_ERROR;
	Message_setStatus(M,SERVICE_SPECIFIC_ERROR|
			  SEVERE_ERROR);
      }
      else {
	Message_setStatus(M,SUCCESS);
      }
      Message_setDataLen(M,0);
      break;

    case EXPCONTROL_END_FB_RUN:
      if (!endFBRun()) {
        expctl.msgProcessingErr++;
        strcpy(expctl.lastErrorStr,EXP_CANNOT_END_FB_RUN);
        expctl.lastErrorID=EXP_Cannot_End_FB_Run;
        expctl.lastErrorSeverity=SEVERE_ERROR;
        Message_setStatus(M,SERVICE_SPECIFIC_ERROR|
                          SEVERE_ERROR);
      }
      else {
        Message_setStatus(M,SUCCESS);
      }
      Message_setDataLen(M,0);
      break;
      
      /* get run state */
    case EXPCONTROL_GET_DOM_STATE:
      data[0]=DOM_state;
      data[1]=DOM_config_access;
      data[2]=DOM_status;
      data[3]=DOM_cmdSource;
      formatLong(DOM_constraints,&data[4]);
      strcpy(&data[8],DOM_errorString);
      Message_setStatus(M,SUCCESS);
      Message_setDataLen(M,strlen(DOM_errorString)+8);
      break;
      
      /* force run reset */ 
    case EXPCONTROL_FORCE_RUN_RESET:
      if (!forceRunReset()) {
	expctl.msgProcessingErr++;
	strcpy(expctl.lastErrorStr,EXP_CANNOT_RESET_RUN_STATE);
	expctl.lastErrorID=EXP_Cannot_Reset_Run_State;
	expctl.lastErrorSeverity=FATAL_ERROR;
	Message_setStatus(M,SERVICE_SPECIFIC_ERROR|
			  FATAL_ERROR);
      }
      else {
	Message_setStatus(M,SUCCESS);
      }
      Message_setDataLen(M,0);
      break;

    case EXPCONTROL_DO_PEDESTAL_COLLECTION:
      tmpPtr = data;
#define MAXPEDGOAL 1000 /* MAX # of ATWD triggers (FADCs are twice this) */
      ULONG ped0goal   = unformatLong(&data[0]);
      ULONG ped1goal   = unformatLong(&data[4]);
      ULONG pedadcgoal = unformatLong(&data[8]);
      zeroPedestals();
      Message_setDataLen(M,0);
      Message_setStatus(M,SUCCESS);
      if(ped0goal   > MAXPEDGOAL ||
	 ped1goal   > MAXPEDGOAL ||
	 pedadcgoal > ped0goal + ped1goal) {
        expctl.msgProcessingErr++;
        strcpy(expctl.lastErrorStr,EXP_TOO_MANY_PEDS);
        expctl.lastErrorID=EXP_Too_Many_Peds;
        expctl.lastErrorSeverity=SEVERE_ERROR;
        Message_setStatus(M,SERVICE_SPECIFIC_ERROR|
                          SEVERE_ERROR);
	break;
      }
      if(pedestalRun(ped0goal, ped1goal, pedadcgoal)) {
        expctl.msgProcessingErr++;
        strcpy(expctl.lastErrorStr,EXP_PEDESTAL_RUN_FAILED);
        expctl.lastErrorID=EXP_Pedestal_Run_Failed;
        expctl.lastErrorSeverity=SEVERE_ERROR;
        Message_setStatus(M,SERVICE_SPECIFIC_ERROR|
                          SEVERE_ERROR);
	break;
      }
      break;

    case EXPCONTROL_GET_NUM_PEDESTALS:
      formatLong(npeds0, &data[0]);
      formatLong(npeds1, &data[4]);
      formatLong(npedsadc, &data[8]);
      Message_setStatus(M,SUCCESS);
      Message_setDataLen(M,12);
      break;

    case EXPCONTROL_GET_PEDESTAL_AVERAGES:
      if(!pedestalsAvail) {
        expctl.msgProcessingErr++;
        strcpy(expctl.lastErrorStr,EXP_PEDESTALS_NOT_AVAIL);
        expctl.lastErrorID=EXP_Pedestals_Not_Avail;
        expctl.lastErrorSeverity=SEVERE_ERROR;
        Message_setStatus(M,SERVICE_SPECIFIC_ERROR|
                          SEVERE_ERROR);
	Message_setDataLen(M,0);
	break;
      }
      /* else we're good to go */
      int ichip, ich, isamp;
      int of = 0;
      for(ichip=0;ichip<2;ichip++) 
	for(ich=0;ich<4;ich++) 
	  for(isamp=0;isamp<ATWDCHSIZ;isamp++) {
	    formatShort(atwdpedavg[ichip][ich][isamp], data+of);
	    of += 2;
	  }
      for(isamp=0;isamp<FADCSIZ;isamp++) {
	formatShort(fadcpedavg[isamp], data+of);
	of += 2;
      }
      Message_setDataLen(M,of);
      Message_setStatus(M,SUCCESS);
      break;

      /*----------------------------------- */
      /* unknown service request (i.e. message */
      /*	subtype), respond accordingly */
    default:
      expctl.msgRefused++;
      strcpy(expctl.lastErrorStr,EXP_ERS_BAD_MSG_SUBTYPE);
      expctl.lastErrorID=COMMON_Bad_Msg_Subtype;
      expctl.lastErrorSeverity=WARNING_ERROR;
      Message_setDataLen(M,0);
      Message_setStatus(M,
			UNKNOWN_SUBTYPE|WARNING_ERROR);
      break;
      
    }
}

