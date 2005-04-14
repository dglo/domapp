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

/* extern functions */
extern void formatLong(ULONG value, UBYTE *buf);
extern BOOLEAN beginRun(void);
extern BOOLEAN endRun(void);
extern BOOLEAN beginFBRun(USHORT bright, USHORT window, USHORT delay, USHORT mask, USHORT rate);
extern BOOLEAN endFBRun(void);
extern BOOLEAN forceRunReset(void);

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

  while(1) {
    /* Decide what to trigger.  Only ATWD0 or 1, not both.  FADC as well.
       Don't trigger any more than what was asked for.  Stop run if we've finished.
    */
    UBYTE trigger_mask = 0;
    
    if(npeds0 < ped0goal) trigger_mask |= HAL_FPGA_TEST_TRIGGER_ATWD0;
    if(npeds1 < ped1goal) trigger_mask |= HAL_FPGA_TEST_TRIGGER_ATWD1;
    if(npedsadc < pedadcgoal) trigger_mask |= HAL_FPGA_TEST_TRIGGER_FADC;
    
    if(!trigger_mask || (npeds0 >= ped0goal && npeds1 >= ped1goal && npedsadc >= pedadcgoal)) {
      pedestalsAvail = 1;
      //mprintf("pedestalRunEntryPoint cur(%d,%d,%d) reached target (%d,%d,%d) trigmask=%d", 
      //    npeds0, npeds1, npedsadc, ped0goal, ped1goal, pedadcgoal);
      return 0;
    }

    memset((void *) atwddata, 0, 2*4*ATWDCHSIZ*sizeof(USHORT));
    memset((void *) fadcdata, 0, FADCSIZ * sizeof(USHORT));

    hal_FPGA_TEST_trigger_forced(trigger_mask);
    int i;
    int done=0;
    for(i=0; !done && i<ATWD_TIMEOUT_COUNT; i++) {
      if(hal_FPGA_TEST_readout_done(trigger_mask)) done = 1;
      halUSleep(ATWD_TIMEOUT_USEC);
    }
    if(!done) { mprintf("warning: forced CPU trigger FAILED!"); return -1; }
  
    //mprintf("forced CPU trigger succeeded after %d iterations.\n", i);
    
    hal_FPGA_TEST_readout(atwddata[0][0], atwddata[0][1], atwddata[0][2], atwddata[0][3],
			  atwddata[1][0], atwddata[1][1], atwddata[1][2], atwddata[1][3],
			  ATWDCHSIZ, fadcdata, FADCSIZ, trigger_mask);

    int ichip, ich, isamp;

    for(ichip=0; ichip<2; ichip++) { /* Form up sums and averages for each ATWD */
      int thismask = ichip==0 ? HAL_FPGA_TEST_TRIGGER_ATWD0 : HAL_FPGA_TEST_TRIGGER_ATWD1;
      if(trigger_mask & thismask) {
	if(ichip==0) npeds0++; else npeds1++;
	int thispeds = ichip==0 ? npeds0 : npeds1;
	int thisgoal = ichip==0 ? ped0goal : ped1goal;
	for(ich=0; ich<4; ich++) {
	  for(isamp=0; isamp<ATWDCHSIZ; isamp++) {
	    atwdpedsum[ichip][ich][isamp] += atwddata[ichip][ich][isamp];
	    if(thispeds >= thisgoal && thisgoal > 0)
	      atwdpedavg[ichip][ich][isamp] = atwdpedsum[ichip][ich][isamp]/thisgoal;
	  }
	}
      }
    }
    
    if(trigger_mask & HAL_FPGA_TEST_TRIGGER_FADC) { /* Sums and averages for FADC */
      npedsadc++;
      for(isamp=0; isamp<FADCSIZ; isamp++) {
	fadcpedsum[isamp] += fadcdata[isamp];
	if(npedsadc >= pedadcgoal && pedadcgoal > 0) 
	  fadcpedavg[isamp] = fadcpedsum[isamp]/pedadcgoal;
      }
    }
  }
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
      if (!beginRun()) {
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
#define MAXPEDGOAL 1000
      ULONG ped0goal   = unformatLong(&data[0]);
      ULONG ped1goal   = unformatLong(&data[4]);
      ULONG pedadcgoal = unformatLong(&data[8]);
      zeroPedestals();
      Message_setDataLen(M,0);
      if(ped0goal   > MAXPEDGOAL ||
	 ped1goal   > MAXPEDGOAL ||
	 pedadcgoal > MAXPEDGOAL) {
        expctl.msgProcessingErr++;
        strcpy(expctl.lastErrorStr,EXP_TOO_MANY_PEDS);
        expctl.lastErrorID=EXP_Too_Many_Peds;
        expctl.lastErrorSeverity=SEVERE_ERROR;
        Message_setStatus(M,SERVICE_SPECIFIC_ERROR|
                          SEVERE_ERROR);

	/* Else do the run; nonzero means failure: */
      } else if(pedestalRun(ped0goal, ped1goal, pedadcgoal)) {

        expctl.msgProcessingErr++;
        strcpy(expctl.lastErrorStr,EXP_PEDESTAL_RUN_FAILED);
        expctl.lastErrorID=EXP_Pedestal_Run_Failed;
        expctl.lastErrorSeverity=SEVERE_ERROR;
        Message_setStatus(M,SERVICE_SPECIFIC_ERROR|
                          SEVERE_ERROR);
      } else {
      	Message_setStatus(M,SUCCESS);
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

