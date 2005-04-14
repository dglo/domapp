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
#include "message.h"
#include "dataAccess.h"
#include "messageAPIstatus.h"
#include "commonServices.h"
#include "commonMessageAPIstatus.h"
#include "DACmessageAPIstatus.h"
#include "dataAccessRoutines.h"
#include "DOMdataCompression.h"
#include "moniDataAccess.h"
#include "compressEvent.h"
#include "DOMstateInfo.h"
#include "DSCmessageAPIstatus.h"
#include "lbm.h"

/* extern functions */
extern void formatLong(ULONG value, UBYTE *buf);

/* external data */
extern UBYTE DOM_state;

/* global storage */
UBYTE FPGA_trigger_mode=CPU_TRIG_MODE;
int FPGA_ATWD_select   = 0;
int SW_compression     = 0;
int SW_compression_fmt = 0;
/* Format masks: default, and configured by request */
#define DEF_FADC_SAMP_CNT  255
#define DEF_ATWD01_MASK    0xFF
#define DEF_ATWD23_MASK    0xFF

/* struct that contains common service info for
	this service. */
COMMON_SERVICE_INFO datacs;

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
    initDOMdataCompression(MAXDATA_VALUE);

}

/* data access  Entry Point */
void dataAccess(MESSAGE_STRUCT *M) {
    char * idptr;
    UBYTE *data;
    int tmpInt;
    //UBYTE tmpByte;
    UBYTE *tmpPtr;
    //USHORT tmpShort;
    MONI_STATUS ms;
    unsigned long long moniHdwrIval, moniConfIval;
    struct moniRec aMoniRec;
    int total_moni_len, moniBytes, len;
    int ichip, ich;
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
      
    case REMOTE_OBJECT_REF:
      /* remote object reference */
      /*	TO BE IMPLEMENTED..... */
      datacs.msgProcessingErr++;
      strcpy(datacs.lastErrorStr, DAC_ERS_BAD_MSG_SUBTYPE);
      datacs.lastErrorID = COMMON_Bad_Msg_Subtype;
      datacs.lastErrorSeverity = WARNING_ERROR;
      Message_setDataLen(M, 0);
      Message_setStatus(M,
			UNKNOWN_SUBTYPE|WARNING_ERROR);
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
      data[0] = checkDataAvailable();
      Message_setStatus(M, SUCCESS);
      Message_setDataLen(M, DAC_ACC_DATA_AVAIL_LEN);
      break;
      
      /*  check for available data */ 
    case DATA_ACC_GET_DATA:
      // try to fill in message buffer with waveform data
      tmpInt = fillMsgWithData(data);
      Message_setDataLen(M, tmpInt);
      Message_setStatus(M, SUCCESS);
      break;
      
    case DATA_ACC_RESET_LBM:
      emptyLBMQ();
      Message_setDataLen(M, 0);
      Message_setStatus(M, SUCCESS);
      break;

      /* JEJ: Deal with configurable intervals for monitoring events */
    case DATA_ACC_SET_MONI_IVAL:
      moniHdwrIval = FPGA_HAL_TICKS_PER_SEC * unformatLong(Message_getData(M));
      moniConfIval = FPGA_HAL_TICKS_PER_SEC * unformatLong(Message_getData(M)+sizeof(ULONG));

      /* Set to one second minimum - redundant w/ message units in seconds, of course */
      if(moniHdwrIval != 0 && moniHdwrIval < FPGA_HAL_TICKS_PER_SEC) 
	moniHdwrIval = FPGA_HAL_TICKS_PER_SEC;

      if(moniConfIval != 0 && moniConfIval < FPGA_HAL_TICKS_PER_SEC)
        moniConfIval = FPGA_HAL_TICKS_PER_SEC;

      moniSetIvals(moniHdwrIval, moniConfIval);
      Message_setDataLen(M, 0);
      Message_setStatus(M, SUCCESS);

      mprintf("MONI SET IVAL REQUEST hw=%ldE6 cf=%ldE6 CPU TICKS/RECORD",
	      (long) (moniHdwrIval/1000000), (long) (moniConfIval/1000000));

      break;
      
      /* JEJ: Deal with requests for monitoring events */
    case DATA_ACC_GET_NEXT_MONI_REC:
      moniBytes = 0;
      while(1) {
	/* Get moni record */
	ms = moniFetchRec(&aMoniRec);

	if(ms == MONI_NOTINITIALIZED) {
	  datacs.msgRefused++;
	  strcpy(datacs.lastErrorStr, DAC_MONI_NOT_INIT);
	  datacs.lastErrorID = DAC_Moni_Not_Init;
	  datacs.lastErrorSeverity = SEVERE_ERROR;
	  Message_setDataLen(M, 0);
	  Message_setStatus(M, SEVERE_ERROR);
	  break;
	} else if(ms == MONI_WRAPPED || ms == MONI_OVERFLOW) {
	  Message_setDataLen(M, 0);
	  strcpy(datacs.lastErrorStr, DAC_MONI_OVERFLOW);
	  datacs.lastErrorID = DAC_Moni_Overrun;
	  datacs.lastErrorSeverity = WARNING_ERROR;
	  Message_setDataLen(M, 0);
	  Message_setStatus(M, WARNING_ERROR);
	  break;
	} else if(ms == MONI_NODATA 
		  || moniBytes + aMoniRec.dataLen + 10 > MAXDATA_VALUE) {
	  /* If too many bytes, current record will be tossed.  Pick it up
	     at next message request */
	  
	  /* We're done, package reply up and send it on its way */
	  Message_setDataLen(M, moniBytes);
	  Message_setStatus(M, SUCCESS);
	  break;
	} else if(ms == MONI_OK) {
	  /* Not done.  Add record and iterate */
	  moniAcceptRec();
	  total_moni_len = aMoniRec.dataLen + 10; /* Total rec length */

	  /* Format header */
	  formatShort(total_moni_len, data);
	  formatShort(aMoniRec.fiducial.fstruct.moniEvtType, data+2);
	  formatTime(aMoniRec.time, data+4);

	  /* Copy payload */
	  len = (aMoniRec.dataLen > MAXMONI_DATA) ? MAXMONI_DATA : aMoniRec.dataLen;
	  memcpy(data+10, aMoniRec.data, len);

	  moniBytes += total_moni_len;
	  data += total_moni_len;
	} else {
	  Message_setDataLen(M, 0);
	  strcpy(datacs.lastErrorStr, DAC_MONI_BADSTAT);
	  datacs.lastErrorID = DAC_Moni_Badstat;
	  datacs.lastErrorSeverity = SEVERE_ERROR;
	  Message_setDataLen(M, 0);
	  Message_setStatus(M, SEVERE_ERROR);
	  break;
	}
      } /* while(1) */
      break; /* from switch */
      
    case DATA_ACC_SET_ENG_FMT:
      Message_setDataLen(M, 0);
      Message_setStatus(M, SUCCESS);
      initFormatEngineeringEvent(data[0], data[1], data[2]);
      break;
      /*----------------------------------- */
      /* unknown service request (i.e. message */
      /*	subtype), respond accordingly */

    case DATA_ACC_TEST_SW_COMP:
      insertTestEvents();
      Message_setDataLen(M, 0);
      Message_setStatus(M, SUCCESS);
      break;

    case DATA_ACC_SET_BASELINE_THRESHOLD:
      setFADCRoadGradeThreshold(unformatShort(data));
      for(ichip=0;ichip<2;ichip++) {
	for(ich=0; ich<4; ich++) {
	  setATWDRoadGradeThreshold(unformatShort(data // base pointer
						  + 2  // skip fadc value
						  + ichip*8 // ATWD0 or 1
						  + ich*2   // select channel
						  ), ichip, ich);
	}
      }
      
      Message_setDataLen(M, 0);
      Message_setStatus(M, SUCCESS);
      break;

    case DATA_ACC_GET_BASELINE_THRESHOLD:
      formatShort(getFADCRoadGradeThreshold(), data);
      for(ichip=0;ichip<2;ichip++) {
        for(ich=0; ich<4; ich++) {
	  formatShort(getATWDRoadGradeThreshold(ichip, ich),
		      data // base pointer
		      + 2  // skip fadc value
		      + ichip*8 // ATWD0 or 1
		      + ich*2   // select channel
		      );
	}
      }
      Message_setDataLen(M, 18);
      Message_setStatus(M, SUCCESS);
      break;

    case DATA_ACC_SET_SW_DATA_COMPRESSION:
      if(data[0] != 0 && data[0] != 1) {
	datacs.lastErrorID = DAC_Bad_Argument;
        strcpy(datacs.lastErrorStr, DAC_ERS_BAD_ARGUMENT);
        datacs.lastErrorSeverity = WARNING_ERROR;
	Message_setDataLen(M, 0);
        Message_setStatus(M, WARNING_ERROR);
	break;
      }
      SW_compression = data[0];
      Message_setDataLen(M, 0);
      Message_setStatus(M, SUCCESS);
      break;

    case DATA_ACC_GET_SW_DATA_COMPRESSION:
      data[0] = SW_compression ? 1 : 0;
      Message_setDataLen(M, 1);
      Message_setStatus(M, SUCCESS);
      break;
      
    case DATA_ACC_SET_SW_DATA_COMPRESSION_FORMAT:
      Message_setDataLen(M, 0);
      if(data[0] != 0) {
	/* 0 is only supported format at the moment */
	datacs.lastErrorID = DAC_Bad_Compr_Format;
	strcpy(datacs.lastErrorStr, DAC_ERS_BAD_COMPR_FORMAT);
	datacs.lastErrorSeverity = WARNING_ERROR;
	Message_setStatus(M, WARNING_ERROR);
	break;
      } 
      SW_compression_fmt = (int) data[0];
      Message_setStatus(M, SUCCESS);
      break;

    case DATA_ACC_GET_SW_DATA_COMPRESSION_FORMAT:
      data[0] = (UBYTE) SW_compression_fmt;
      Message_setDataLen(M, 1);
      Message_setStatus(M, SUCCESS);
      break;

    case DATA_ACC_GET_FB_SERIAL:
      wasEnabled = hal_FB_isEnabled();
      Message_setDataLen(M, 0);
      if(!wasEnabled) {
	if(hal_FB_enable(&config, &valid, &reset)) {
	  datacs.lastErrorID = DAC_Cant_Enable_FB;
	  strcpy(datacs.lastErrorStr, DAC_CANT_ENABLE_FB);
	  datacs.lastErrorSeverity = WARNING_ERROR;
	  Message_setStatus(M, WARNING_ERROR);
	  hal_FB_disable();
	  break;
	}
      }
      if(hal_FB_get_serial(&idptr)) {
	datacs.lastErrorID = DAC_Cant_Get_FB_Serial;
	strcpy(datacs.lastErrorStr, DAC_CANT_GET_FB_SERIAL);
	datacs.lastErrorSeverity = WARNING_ERROR;
	Message_setStatus(M, WARNING_ERROR);
	if(!wasEnabled) hal_FB_disable();
	break;
      } else {
	memcpy(data, idptr, strlen(idptr));
	Message_setDataLen(M, strlen(idptr));
      }
      if(!wasEnabled) hal_FB_disable();
      Message_setStatus(M, SUCCESS);
      break;

    default:
      datacs.msgRefused++;
      strcpy(datacs.lastErrorStr, DAC_ERS_BAD_MSG_SUBTYPE);
      datacs.lastErrorID = COMMON_Bad_Msg_Subtype;
      datacs.lastErrorSeverity = WARNING_ERROR;
      Message_setDataLen(M, 0);
      Message_setStatus(M,
			UNKNOWN_SUBTYPE|WARNING_ERROR);
      break;
      
    } /* Switch message subtype */
} /* dataAccess subroutine */

