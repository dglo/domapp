/*
Author: Chuck McParland
Start Date: May 4, 1999
Description:
	DOM Experiment Control service thread to handle
	special run-related functions.  Performs all 
	"standard" DOM service functions.
Modification: 1/05/04 Jacobsen :-- add monitoring functionality
Modification: 1/19/04 Jacobsen :-- add configurable engineering event
Modification: 5/10/04 Jacobsen :-- put more than one monitoring rec. per request msg
*/

/* system include files */
#if defined (CYGWIN) || defined (LINUX)
#include <stdio.h>
#endif

#include <string.h>
#include <stdio.h> /* snprintf */
/* DOM-related includes */
#include "hal/DOM_MB_types.h"
#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_fpga.h"
#include "hal/DOM_MB_domapp.h"

#include "message.h"
#include "expControl.h"
#include "dataAccess.h"
#include "messageAPIstatus.h"
#include "commonServices.h"
#include "commonMessageAPIstatus.h"
#include "DACmessageAPIstatus.h"
#include "dataAccessRoutines.h"
//#include "DOMdataCompression.h"
#include "moniDataAccess.h"
//#include "compressEvent.h"
#include "domSControl.h"
#include "DOMstateInfo.h"
#include "DSCmessageAPIstatus.h"

extern void formatLong(ULONG value, UBYTE *buf);
extern UBYTE DOM_state;
extern USHORT pulser_rate;
USHORT atwdRGthresh[2][4], fadcRGthresh;

UBYTE FPGA_trigger_mode = CPU_TRIG_MODE;
UBYTE dataFormat        = FMT_ENG;
UBYTE compMode          = CMP_NONE;
int FPGA_ATWD_select    = 0;
int SW_compression      = 0;
int SW_compression_fmt  = 0;
int numOverflows        = 0;
int atwdSelect          = HAL_FPGA_DOMAPP_ATWD_A|HAL_FPGA_DOMAPP_ATWD_B;
unsigned long sw_lbm_mask = (1<<DEFAULT_LBM_BIT_DEPTH)-1;

/* Format masks: default, and configured by request */
#define DEF_FADC_SAMP_CNT  255
#define DEF_ATWD01_MASK    0xFF
#define DEF_ATWD23_MASK    0xFF

/* struct that contains common service info for
	this service. */
COMMON_SERVICE_INFO datacs;

/* Monitoring intervals */

extern unsigned long long moniHdwrIval, 
                          moniConfIval, 
                          moniFastIval,
                          moniHistoIval;
extern unsigned short     histoPrescale;


/* data access  Entry Point */

void dataAccessInit(void) {
  //MESSAGE_STRUCT *m;

    datacs.state = SERVICE_ONLINE;
    datacs.lastErrorID = COMMON_No_Errors;
    datacs.lastErrorSeverity = INFORM_ERROR;
    datacs.majorVersion = DAC_MAJOR_VERSION;
    datacs.minorVersion = DAC_MINOR_VERSION;
    strcpy(datacs.lastErrorStr, DAC_ERS_NO_ERRORS);
    datacs.msgReceived = 0;
    datacs.msgRefused = 0;
    datacs.msgProcessingErr = 0;

    /* now initialize the data filling routines */
    initFillMsgWithData();
    initFormatEngineeringEvent(DEF_FADC_SAMP_CNT, DEF_ATWD01_MASK, DEF_ATWD23_MASK);
    //initDOMdataCompression(MAXDATA_VALUE);

    /* Even though pulser isn't running, set rate appropriately for heartbeat events */
    hal_FPGA_DOMAPP_cal_pulser_rate((double) pulser_rate);
}

#define DOERROR(errstr, errnum, errtype)                       \
   do {                                                        \
      datacs.msgProcessingErr++;                               \
      strcpy(datacs.lastErrorStr,(errstr));                    \
      datacs.lastErrorID=(errnum);                             \
      datacs.lastErrorSeverity=errtype;                        \
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|errtype);     \
      Message_setDataLen(M,0);                                 \
   } while(0)

void dataAccess(MESSAGE_STRUCT *M) {
    char * idptr;
    UBYTE *data;
    int tmpInt;
    UBYTE *tmpPtr;
    MONI_STATUS ms;
    struct moniRec aMoniRec;
    int total_moni_len, moniBytes, len;
    int config, valid, reset; /* For hal_FB_enable */
    int wasEnabled;
    /* get address of data portion. */
    /* Receiver ALWAYS links a message */
    /* to a valid data buffer-even */ 
    /* if it is empty. */
    data = Message_getData(M);
    datacs.msgReceived++;
    switch ( Message_getSubtype(M) ) {
      /* Manditory Service SubTypes */
    case GET_SERVICE_STATE:
      /* get current state of Data Access */
      data[0] = datacs.state;
      Message_setDataLen(M, GET_SERVICE_STATE_LEN);
      Message_setStatus(M, SUCCESS);
      break;
      
    case GET_LAST_ERROR_ID:
      /* get the ID of the last error encountered */
      data[0] = datacs.lastErrorID;
      data[1] = datacs.lastErrorSeverity;
      Message_setDataLen(M, GET_LAST_ERROR_ID_LEN);
      Message_setStatus(M, SUCCESS);
      break;
      
    case GET_SERVICE_VERSION_INFO:
      /* get the major and minor version of this */
      /*	Data Access */
      data[0] = datacs.majorVersion;
      data[1] = datacs.minorVersion;
      Message_setDataLen(M, GET_SERVICE_VERSION_INFO_LEN);
      Message_setStatus(M, SUCCESS);
      break;
      
    case GET_SERVICE_STATS:
      /* get standard service statistics for */
      /*	the Data Access */
      formatLong(datacs.msgReceived, &data[0]);
      formatLong(datacs.msgRefused, &data[4]);
      formatLong(datacs.msgProcessingErr, &data[8]);
      Message_setDataLen(M, GET_SERVICE_STATS_LEN);
      Message_setStatus(M, SUCCESS);
      break;
      
    case GET_LAST_ERROR_STR:
      /* get error string for last error encountered */
      strcpy(data,datacs.lastErrorStr);
      Message_setDataLen(M, strlen(datacs.lastErrorStr));
      Message_setStatus(M, SUCCESS);
      break;
      
    case CLEAR_LAST_ERROR:
      /* reset both last error ID and string */
      datacs.lastErrorID = COMMON_No_Errors;
      datacs.lastErrorSeverity = INFORM_ERROR;
      strcpy(datacs.lastErrorStr, DAC_ERS_NO_ERRORS);
      Message_setDataLen(M, 0);
      Message_setStatus(M, SUCCESS);
      break;
      
    case GET_SERVICE_SUMMARY:
      /* init a temporary buffer pointer */
      tmpPtr = data;
      /* get current state of slow control */
      *tmpPtr++ = datacs.state;
      /* get the ID of the last error encountered */
      *tmpPtr++ = datacs.lastErrorID;
      *tmpPtr++ = datacs.lastErrorSeverity;
      /* get the major and minor version of this */
      /*	exp control */
      *tmpPtr++ = datacs.majorVersion;
      *tmpPtr++ = datacs.minorVersion;
      /* get standard service statistics for */
      /*	the exp control */
      formatLong(datacs.msgReceived, tmpPtr);
      tmpPtr += sizeof(ULONG);
      formatLong(datacs.msgRefused, tmpPtr);
      tmpPtr += sizeof(ULONG);
      formatLong(datacs.msgProcessingErr, tmpPtr);
      tmpPtr += sizeof(ULONG);
      /* get error string for last error encountered */
      strcpy(tmpPtr, datacs.lastErrorStr);
      Message_setDataLen(M, (int)(tmpPtr-data) +
			 strlen(datacs.lastErrorStr));
      Message_setStatus(M, SUCCESS);
      break;
      
      /*-------------------------------*/
      /* Data Access specific SubTypes */
      
      /*  check for available data */ 
    case DATA_ACC_DATA_AVAIL:
      data[0] = isDataAvailable();
      Message_setStatus(M, SUCCESS);
      Message_setDataLen(M, DAC_ACC_DATA_AVAIL_LEN);
      break;
      
      /*  check for available data */ 
    case DATA_ACC_GET_DATA:
      // try to fill in message buffer with waveform data
      tmpInt = fillMsgWithData(data, MAXDATA_VALUE, dataFormat, compMode);
      Message_setDataLen(M, tmpInt);
      Message_setStatus(M, SUCCESS);
      break;
      
      /* Deal with configurable intervals for monitoring events; pick up "fast" rate if available;
         convert to clock ticks when needed. */
    case DATA_ACC_SET_MONI_IVAL:
      moniHdwrIval = unformatLong(Message_getData(M));
      if(moniHdwrIval < FPGA_HAL_TICKS_PER_SEC) 
	moniHdwrIval *= FPGA_HAL_TICKS_PER_SEC;

      moniConfIval = unformatLong(Message_getData(M)+sizeof(ULONG));
      if(moniConfIval < FPGA_HAL_TICKS_PER_SEC) 
	moniConfIval *= FPGA_HAL_TICKS_PER_SEC;

      if(Message_dataLen(M) >= (3 * sizeof(ULONG))) {
	moniFastIval = unformatLong(Message_getData(M)+2*sizeof(ULONG));
	if(moniFastIval < FPGA_HAL_TICKS_PER_SEC) 
	  moniFastIval *= FPGA_HAL_TICKS_PER_SEC;
      }
      Message_setDataLen(M, 0);
      Message_setStatus(M, SUCCESS);

      mprintf("MONI SET IVAL REQUEST hw=%ldE6 cf=%ldE6 fast=%ldE6 CPU TICKS/RECORD",
	      (long) (moniHdwrIval/1000000), (long) (moniConfIval/1000000),
	      (long) (moniFastIval/1000000));

      break;
      
    case DATA_ACC_GET_NEXT_MONI_REC:
      moniBytes = 0;
      int done  = 0;

      while(!done) {
	ms = moniFetchRec(&aMoniRec);

	if(moniBytes + aMoniRec.dataLen + 10 > MAXDATA_VALUE) {
	  /* Can't fit any more data */
	  Message_setDataLen(M, moniBytes);
          Message_setStatus(M, SUCCESS);
          break;
	}

	switch(ms) {
	case MONI_NOTINITIALIZED:
	  DOERROR(DAC_MONI_NOT_INIT, DAC_Moni_Not_Init, SEVERE_ERROR);
	  done = 1;
	  break;
	case MONI_WRAPPED:
 	case MONI_OVERFLOW:
	  DOERROR(DAC_MONI_OVERFLOW, DAC_Moni_Overrun, WARNING_ERROR);
	  done = 1;
	  break;
	case MONI_NODATA:
	  Message_setDataLen(M, moniBytes);
	  Message_setStatus(M, SUCCESS);
	  done = 1;
	  break;
	case MONI_OK:
	  /* Not done.  Add record and iterate */
	  moniAcceptRec();
	  total_moni_len = aMoniRec.dataLen + 10; /* Total rec length */
	  /* Format header */
	  formatShort(total_moni_len, data);
	  unsigned short mt = aMoniRec.moniEvtType;
	  formatShort(mt, data+2);
	  formatTime(aMoniRec.time, data+4);
	  /* Copy payload */
	  len = (aMoniRec.dataLen > MAXMONI_DATA) ? MAXMONI_DATA : aMoniRec.dataLen;
	  memcpy(data+10, aMoniRec.data, len);
	  moniBytes += total_moni_len;
	  data += total_moni_len;
	  break;
	case MONI_ERROR:
	default:
	  DOERROR(DAC_MONI_BADSTAT, DAC_Moni_Badstat, SEVERE_ERROR);
	  done = 1;
	  break;
	} /* inner switch */
      }   /* while(!done) */
      break; /* from outer switch */

    case DATA_ACC_RESET_MONI_BUF:
      moniZeroIndices();
      Message_setDataLen(M, 0);
      Message_setStatus(M, SUCCESS);
      break;

    case DATA_ACC_MONI_AVAIL:
      Message_setDataLen(M, 1);
      data[0] = moniHaveData();
      Message_setStatus(M, SUCCESS);
      break;
	
    case DATA_ACC_SET_ENG_FMT:
      Message_setDataLen(M, 0);
      Message_setStatus(M, SUCCESS);
      initFormatEngineeringEvent(data[0], data[1], data[2]);
      break;
    
    case DATA_ACC_SET_DATA_FORMAT:
      if(data[0] != FMT_ENG && data[0] != FMT_RG && data[0] != FMT_DELTA) {
	DOERROR(DAC_ERS_BAD_ARGUMENT, DAC_Bad_Argument, SEVERE_ERROR);
	break;
      }
      dataFormat = data[0];
      mprintf("Set data format to %d", dataFormat);

      Message_setDataLen(M, 0);
      Message_setStatus(M, SUCCESS);
      break;
      
    case DATA_ACC_GET_DATA_FORMAT:
      data[0] = dataFormat;
      Message_setDataLen(M, 1);
      Message_setStatus(M, SUCCESS);
      break;

    case DATA_ACC_SET_COMP_MODE:
      if(data[0] != CMP_NONE && data[0] != CMP_RG && data[0] != CMP_DELTA) {
	DOERROR(DAC_ERS_BAD_ARGUMENT, DAC_Bad_Argument, SEVERE_ERROR);
        break;
      }
      compMode = data[0];
      mprintf("Set compression mode to %d", compMode);

      Message_setDataLen(M, 0);
      Message_setStatus(M, SUCCESS);
      break;

    case DATA_ACC_GET_COMP_MODE:
      data[0] = compMode;
      Message_setDataLen(M, 1);
      Message_setStatus(M, SUCCESS);
      break;

    case DATA_ACC_GET_FB_SERIAL:
      wasEnabled = hal_FB_isEnabled();
      Message_setDataLen(M, 0);
      if(!wasEnabled) {
	if(hal_FB_enable(&config, &valid, &reset, DOM_FPGA_DOMAPP)) {
	  DOERROR(DAC_CANT_ENABLE_FB, DAC_Cant_Enable_FB, WARNING_ERROR);
	  hal_FB_disable();
	  break;
	}
      }
      if(hal_FB_get_serial(&idptr)) {
	DOERROR(DAC_CANT_GET_FB_SERIAL, DAC_Cant_Get_FB_Serial, WARNING_ERROR);
	if(!wasEnabled) hal_FB_disable();
	break;
      } else {
	memcpy(data, idptr, strlen(idptr));
	Message_setDataLen(M, strlen(idptr));
      }
      if(!wasEnabled) hal_FB_disable();
      Message_setStatus(M, SUCCESS);
      break;

    case DATA_ACC_GET_SN_DATA: 
      { 
	int nb = fillMsgWithSNData(data, MAXDATA_VALUE);
	Message_setDataLen(M, nb);
	Message_setStatus(M, SUCCESS);
	break;
      }
      break;

    case DATA_ACC_GET_NUMOVERFLOWS:
      formatLong(numOverflows, data);
      Message_setDataLen(M, 4);
      Message_setStatus(M, SUCCESS);
      break;

    case DATA_ACC_SET_LBM_BIT_DEPTH:
      {
	unsigned char bits = data[0];
	if(bits < MIN_LBM_BIT_DEPTH || bits > ACTUAL_LBM_BIT_DEPTH) {
	  DOERROR(DAC_BAD_LBM_DEPTH, DAC_Bad_Lbm_Depth, WARNING_ERROR);
	  break;
	}
	sw_lbm_mask = (1<<bits)-1;
      }
      break;

    case DATA_ACC_GET_LBM_SIZE:
      formatLong(sw_lbm_mask+1, data);
      Message_setDataLen(M, 4);
      Message_setStatus(M, SUCCESS);
      break;

    case DATA_ACC_HISTO_CHARGE_STAMPS:
      {
	
	moniHistoIval = unformatLong(Message_getData(M));
	if(moniHistoIval < FPGA_HAL_TICKS_PER_SEC)
	  moniHistoIval *= FPGA_HAL_TICKS_PER_SEC;
	histoPrescale = unformatShort(Message_getData(M)+sizeof(ULONG));
		
	if(moniHistoIval>0) { 
	  mprintf("Histogramming charge stamps with ival=%u, prescale=%hu",
		  moniHistoIval, histoPrescale);
	} else {
	  mprintf("Histogramming DISABLED");
	}
	
	Message_setDataLen(M, 0);
	Message_setStatus(M, SUCCESS);
      }
      break;

    case DATA_ACC_SELECT_ATWD:
      {
	int mode = (int) data[0];

	if(mode == 0) {
	  atwdSelect = HAL_FPGA_DOMAPP_ATWD_A;
	} else if(mode == 1) {
	  atwdSelect = HAL_FPGA_DOMAPP_ATWD_B;
	} else if(mode == 2) {
	  atwdSelect = HAL_FPGA_DOMAPP_ATWD_A|HAL_FPGA_DOMAPP_ATWD_B;
	} else {
	  DOERROR(DAC_ERS_BAD_ARGUMENT, DAC_Bad_Argument, SEVERE_ERROR);
	  break;
	}
	mprintf("ATWD chips to enable: A=%s B=%s", 
		(atwdSelect&HAL_FPGA_DOMAPP_ATWD_A)?"YES":"no",
		(atwdSelect&HAL_FPGA_DOMAPP_ATWD_B)?"YES":"no");

	Message_setDataLen(M, 0);
	Message_setStatus(M, SUCCESS);
      }
      break;
      
    default:
      datacs.msgRefused++;
      DOERROR(DAC_ERS_BAD_MSG_SUBTYPE, COMMON_Bad_Msg_Subtype, WARNING_ERROR);
      break;
      
    } /* Switch message subtype */
} /* dataAccess subroutine */

