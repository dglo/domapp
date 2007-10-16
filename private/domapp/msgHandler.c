/* msgHandler.c 
   This service reads a message from a fifo
   and puts it onto the destination stack
   described by message type field
   If destination cannot be determined, put
   message back to sender ( marked undeliverable)
*/ 

#include "hal/DOM_MB_types.h"
#include "hal/DOM_MB_hal.h"

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "message.h"
#include "msgHandler.h"
#include "domSControl.h"
#include "expControl.h"
#include "moniDataAccess.h"
#include "dataAccess.h"
#include "messageAPIstatus.h"
#include "commonServices.h"
#include "commonMessageAPIstatus.h"
#include "versions.h"
#include "MSGHANDLERmessageAPIstatus.h"

/* extern functions */
extern void   formatLong(ULONG value, UBYTE *buf);
extern USHORT unformatShort(UBYTE *buf);
extern ULONG  unformatLong(UBYTE *buf);


ULONG SlowCntStackOvfl = 0;
ULONG DataAccStackOvfl = 0;
ULONG ExpCntStackOvfl  = 0;
ULONG SenderStackOvfl  = 0;


/* packet driver counters, etc. */
extern ULONG PKTrecv;
extern ULONG PKTsent;
extern ULONG NoStorage;
extern ULONG FreeListCorrupt;
extern ULONG PKTbufOvr;
extern ULONG PKTbadFmt;
extern ULONG PKTspare;

extern ULONG MSGrecv;
extern ULONG MSGsent;
extern ULONG tooMuchData;
extern ULONG IDMismatch;
extern ULONG CRCproblem;

extern unsigned long msgs;
extern unsigned long loops;

/* struct that contains common service info for
	this service. */
COMMON_SERVICE_INFO msgHand;

void msgHandlerInit(void) {
  msgHand.state = SERVICE_ONLINE;
  msgHand.lastErrorID = COMMON_No_Errors;
  msgHand.lastErrorSeverity = INFORM_ERROR;
  msgHand.majorVersion = MSGHANDLER_MAJOR_VERSION;
  msgHand.minorVersion = MSGHANDLER_MINOR_VERSION;
  strcpy(msgHand.lastErrorStr,MSGHAND_ERS_NO_ERRORS);
  msgHand.msgReceived = 0;
  msgHand.msgRefused = 0;
  msgHand.msgProcessingErr = 0;
} 

void msgHandler(MESSAGE_STRUCT *M) {
  UBYTE *data, *tmpPtr;
  
  switch ( Message_getType(M) ) {
    
  case DOM_SLOW_CONTROL:
    domSControl(M);
    break;
    
  case DATA_ACCESS: 
    dataAccess(M);
    break;
    
  case EXPERIMENT_CONTROL:
    expControl(M);
    break;

  case MESSAGE_HANDLER:
    /* get address of data portion. */
    /* Receiver ALWAYS links a message */
    /* to a valid data buffer-even */ 
    /* if it is empty. */
    data = Message_getData(M);
    msgHand.msgReceived++;
    switch(Message_getSubtype(M)) {

      /* Manditory Service SubTypes */
    case GET_SERVICE_STATE:
      /* get current state of Message Handler */
      data[0] = msgHand.state;
      Message_setDataLen(M,GET_SERVICE_STATE_LEN);
      Message_setStatus(M,SUCCESS);
      break;
    case GET_LAST_ERROR_ID:
      /* get the ID of the last error encountered */
      data[0] = msgHand.lastErrorID;
      data[1] = msgHand.lastErrorSeverity;
      Message_setDataLen(M,GET_LAST_ERROR_ID_LEN);
      Message_setStatus(M,SUCCESS);
      break;
    case GET_SERVICE_VERSION_INFO:
      /* get the major and minor version of this */
      /*	Message Handler */
      data[0] = msgHand.majorVersion;
      data[1] = msgHand.minorVersion;
      Message_setDataLen(M,GET_SERVICE_VERSION_INFO_LEN);
      Message_setStatus(M,SUCCESS);
      break;
    case GET_SERVICE_STATS:
      /* get standard service statistics for */
      /*	the Message Handler */
      formatLong(msgHand.msgReceived,&data[0]);
      formatLong(msgHand.msgRefused,&data[4]);
      formatLong(msgHand.msgProcessingErr,&data[8]);
      Message_setDataLen(M,GET_SERVICE_STATS_LEN);
      Message_setStatus(M,SUCCESS);
      break;
    case GET_LAST_ERROR_STR:
      /* get error string for last error encountered */
      strcpy(data,msgHand.lastErrorStr);
      Message_setDataLen(M,strlen(msgHand.lastErrorStr)); 
      Message_setStatus(M,SUCCESS);
      break;
    case CLEAR_LAST_ERROR:
      /* reset both last error ID and string */
      msgHand.lastErrorID = COMMON_No_Errors;
      msgHand.lastErrorSeverity = INFORM_ERROR;
      strcpy(msgHand.lastErrorStr,
	     MSGHAND_ERS_NO_ERRORS);
      Message_setDataLen(M,0);
      Message_setStatus(M,SUCCESS);
      break;
    case REMOTE_OBJECT_REF:
      /* remote object reference */
      /*	TO BE IMPLEMENTED..... */
      msgHand.msgProcessingErr++;
      strcpy(msgHand.lastErrorStr,
	     MSGHAND_ERS_BAD_MSG_SUBTYPE);
      msgHand.lastErrorID = COMMON_Bad_Msg_Subtype;
      msgHand.lastErrorSeverity = WARNING_ERROR;
      Message_setDataLen(M,0);
      Message_setStatus(M,
			UNKNOWN_SUBTYPE|WARNING_ERROR);
      break;
    case GET_SERVICE_SUMMARY:
      /* init a temporary buffer pointer */
      tmpPtr = data;
      /* get current state of Message Handler */
      *tmpPtr++ = msgHand.state;
      /* get the ID of the last error encountered */
      *tmpPtr++ = msgHand.lastErrorID;
      *tmpPtr++ = msgHand.lastErrorSeverity;
      /* get the major and minor version of this */
      /*	Message Handler */
      *tmpPtr++ = msgHand.majorVersion;
      *tmpPtr++ = msgHand.minorVersion;
      /* get standard service statistics for */
      /*	the Message Handler */
      formatLong(msgHand.msgReceived,tmpPtr);
      tmpPtr += sizeof(ULONG);
      formatLong(msgHand.msgRefused,tmpPtr);
      tmpPtr += sizeof(ULONG);
      formatLong(msgHand.msgProcessingErr,tmpPtr);
      tmpPtr += sizeof(ULONG);
      /* get error string for last error encountered */
      strcpy(tmpPtr,msgHand.lastErrorStr);

      Message_setDataLen(M,(int)(tmpPtr-data)+
			 strlen(msgHand.lastErrorStr));
      Message_setStatus(M,SUCCESS);
      break;

      /* Message Handler specific SubTypes: */

    case MSGHAND_GET_DOM_VER:
      /* get the major and minor version of this */
      /*	DOM hardware */
      data[0] = 0;
      data[1] = 1;
      Message_setDataLen(M,
			 MSGHAND_GET_DOM_VERSION_INFO_LEN);
      Message_setStatus(M,SUCCESS);
      break;
    case MSGHAND_GET_DOM_ID:
      /* get the id of this DOM hardware */
      strcpy(data,halGetBoardID());
      Message_setDataLen(M,
			 MSGHAND_GET_DOM_ID_LEN);
      Message_setStatus(M,SUCCESS);
      break;
    case MSGHAND_GET_DOM_NAME:
      /* get given name of this DOM hardware */
      /* for now, we just return the ID */
      strcpy(data,halGetBoardID());
      Message_setDataLen(M,strlen(halGetBoardID()));
      Message_setStatus(M,SUCCESS);
      break;
    case MSGHAND_GET_ATWD_ID:
      /* get ID's of installed ATWD's */
      formatLong(1234,&data[0]);
      formatLong(1235,&data[4]);
      Message_setDataLen(M,MSGHAND_GET_ATWD_ID_LEN);
      Message_setStatus(M,SUCCESS);
      break;
    case MSGHAND_GET_PKT_STATS:
      /* init a temporary buffer pointer */
      tmpPtr = data;
      /* get packet driver statistics */
      formatLong(PKTrecv,tmpPtr);
      tmpPtr += sizeof(ULONG);
      formatLong(PKTsent,tmpPtr);
      tmpPtr += sizeof(ULONG);
      formatLong(NoStorage,tmpPtr);
      tmpPtr += sizeof(ULONG);
      formatLong(FreeListCorrupt,tmpPtr);
      tmpPtr += sizeof(ULONG);
      formatLong(PKTbufOvr,tmpPtr);
      tmpPtr += sizeof(ULONG);					
      formatLong(PKTbadFmt,tmpPtr);
      tmpPtr += sizeof(ULONG);
      formatLong(PKTspare,tmpPtr);
      tmpPtr += sizeof(ULONG);
      Message_setDataLen(M,MSGHAND_GET_PKT_STATS_LEN);
      Message_setStatus(M,SUCCESS);
      break;
    case MSGHAND_GET_MSG_STATS:
      /* init a temporary buffer pointer */
      tmpPtr = data;
      /* get packet driver statistics */
      formatLong(msgs, tmpPtr);
      tmpPtr += sizeof(ULONG);
      formatLong(loops, tmpPtr);
      tmpPtr += sizeof(ULONG);
      msgs = loops = 0;
      Message_setDataLen(M,8);
      Message_setStatus(M,SUCCESS);
      break;
    case MSGHAND_CLR_PKT_STATS:
      /* reset packet level stats */
      PKTrecv = 0;
      PKTsent = 0;
      NoStorage = 0;
      FreeListCorrupt = 0;
      PKTbufOvr = 0;
      PKTbadFmt = 0;
      PKTspare = 0;
      Message_setDataLen(M,0);
      Message_setStatus(M,SUCCESS);
      break;
    case MSGHAND_CLR_MSG_STATS:
      /* reset message level stats */
      MSGrecv = 0;
      MSGsent = 0;
      tooMuchData = 0;
      IDMismatch = 0;
      CRCproblem = 0;
      Message_setDataLen(M,0);
      Message_setStatus(M,SUCCESS);
      break;
    case MSGHAND_ECHO_MSG:
      /* echo the incoming message */
      Message_setDataLen(M,Message_dataLen(M));
      Message_setStatus(M,SUCCESS);
      break;
    case MSGHAND_REBOOT_CPU_FLASH:
      /* hal reboot */
      halSetFlashBoot();
      halBoardReboot();
      Message_setStatus(M,SUCCESS);
      Message_setDataLen(M,0);
      break;
    case MSGHAND_GET_DOMAPP_RELEASE:
      {
	Message_setStatus(M,SUCCESS);
	char relstr[] = "DOM-MB-";
#define STR(a) #a
#define STRING(a) STR(a)
	char bldstr[] = STRING(ICESOFT_BUILD);
	int  irel = strlen(relstr);
	int  brel = strlen(bldstr); // From versions.h
	if(irel+brel <= MAXDATA_VALUE) {
	  memcpy(data, relstr, irel);
	  memcpy(data+irel, bldstr, brel);
	  Message_setDataLen(M, irel+brel);
	} else {
	  Message_setDataLen(M, 0);
	}
      }
      break;
      /*----------------------------------- */
      /* unknown service request (i.e. message */
      /*	subtype), respond accordingly */
    default:
      msgHand.msgRefused++;
      strcpy(msgHand.lastErrorStr,
	     MSGHAND_ERS_BAD_MSG_SUBTYPE);
      msgHand.lastErrorID = COMMON_Bad_Msg_Subtype;
      msgHand.lastErrorSeverity = WARNING_ERROR;
      Message_setStatus(M, UNKNOWN_SUBTYPE|WARNING_ERROR);
      break;
    }
    break;

  default:
    msgHand.msgProcessingErr++;
    strcpy(msgHand.lastErrorStr,
	   MSGHAND_UNKNOWN_SERVER);
    msgHand.lastErrorID = MSGHAND_unknown_server;
    Message_setDataLen(M,0);
    Message_setStatus(M,
		      UNKNOWN_SERVER|WARNING_ERROR);
    break;
  }
}
