/* DOMSControl.c */
/*
Author: Chuck McParland
Start Date: May 4, 1999
Description:
	DOM Slow Control service thread.  
	Performs all "standard" DOM service functions.
Last Modification:
*/

/* system include files */
#include <pthread.h>

/* DOM-related includes */
#include "domapp_common/DOMtypes.h"
#include "message/message.h"
#include "slowControl/domSControl.h"
#include "domapp_common/slowControl.h"
#include "domapp_common/messageAPIstatus.h"
#include "domapp_common/commonServices.h"
#include "domapp_common/commonMessageAPIstatus.h"
#include "slowControl/DSCmessageAPIstatus.h"
#include "hal/DOM_MB_hal_simul.h"
#include "domapp_common/DOMstateInfo.h"

/* extern functions */
extern void formatLong(ULONG value, UBYTE *buf);
extern void formatShort(USHORT value, UBYTE *buf);
extern ULONG unformatLong(UBYTE *buf);
extern USHORT unformatShort(UBYTE *buf);
extern UBYTE DOM_state;
extern UBYTE DOM_config_access;

/* local functions, data */
USHORT PMT_HV_max=PMT_HV_DEFAULT_MAX;

/* external mutex for slow control access */
pthread_mutex_t SlowControlMutex;

/* slow control resource lock */
// RESOURCE SC_lock;

/* message  queue identifies */
extern int SC;	
extern int SD;

/* struct that contains common service info for
	this service. */
COMMON_SERVICE_INFO domsc;

/* Slow Control Entry Point */
void *DOMSControl(void *dummy) {

    MESSAGE_STRUCT *M;
    UBYTE *data;
    int tempInt;
    UBYTE tmpByte;
    UBYTE *tmpPtr;
    USHORT tmpShort;
    USHORT PMT_HVreq;
    int i;

    domsc.state=SERVICE_ONLINE;
    domsc.lastErrorID=COMMON_No_Errors;
    domsc.lastErrorSeverity=INFORM_ERROR;
    domsc.majorVersion=DSC_MAJOR_VERSION;
    domsc.minorVersion=DSC_MINOR_VERSION;
    strcpy(domsc.lastErrorStr,DSC_ERS_NO_ERRORS);
    domsc.msgReceived=0;
    domsc.msgRefused=0;
    domsc.msgProcessingErr=0;


    /* init pointer to resource lock */
    pthread_mutex_lock(&SlowControlMutex);


    for(;;) {
	/* blocking wait for a message in our stack. */
	Message_receive(&M,SC);
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
		pthread_mutex_lock(&SlowControlMutex);
		for(i=0;i<MAX_NUM_ADCS;i++) {
		    formatShort(halReadADC(i),tmpPtr);
		    tmpPtr+=sizeof(USHORT);
		}
		pthread_mutex_unlock(&SlowControlMutex);
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
		    pthread_mutex_lock(&SlowControlMutex);
		    formatShort(halReadADC(tmpByte),data);
		    pthread_mutex_unlock(&SlowControlMutex);
		    Message_setDataLen(M,DSC_READ_ONE_ADC_LEN);
		    Message_setStatus(M,SUCCESS);
		}
		break;
	    case DSC_READ_ALL_DACS:
		tmpPtr=data;
		/* lock, access and unlock */
		pthread_mutex_lock(&SlowControlMutex);
		for(i=0;i<MAX_NUM_DACS;i++) {
		    formatShort(halReadDAC(i),tmpPtr);
		    tmpPtr+=sizeof(USHORT);
		}
		pthread_mutex_unlock(&SlowControlMutex);
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
		    pthread_mutex_lock(&SlowControlMutex);
		    formatShort(halReadDAC(tmpByte),data);
		    pthread_mutex_unlock(&SlowControlMutex);
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
		    pthread_lock_mutex(&SlowControlMutex);
		    halWriteDAC(tmpByte,unformatShort(&data[2]));
		    pthread_unlock_mutex(&SlowControlMutex);
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
		else if(!testDOMconstraints(DOM_CONSTRAINT_NO_HV_CHANGE)){
		    /* format up failure response */
		    Message_setDataLen(M,0);
		    domsc.msgProcessingErr++;
		    strcpy(domsc.lastErrorStr,DSC_VIOLATES_CONSTRAINTS);
		    domsc.lastErrorID=DSC_violates_constraints;
		    domsc.lastErrorSeverity=WARNING_ERROR;
		    Message_setStatus(M,SERVICE_SPECIFIC_ERROR|WARNING_ERROR);
		    break;
		}
		Message_setDataLen(M,0);
		Message_setStatus(M,SUCCESS);
		break;

	    case DSC_ENABLE_PMT_HV:
		if(DOM_config_access==0){
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
		pthread_lock_mutex(&SlowControlMutex);
		halEnablePMT_HV();
		pthread_lock_mutex(&SlowControlMutex);
		Message_setDataLen(M,0);
		Message_setStatus(M,SUCCESS);
		break;
	    case DSC_DISABLE_PMT_HV:
		/* lock, access and unlock */
		pthread_lock_mutex(&SlowControlMutex);
		halDisablePMT_HV();
		pthread_unlock_mutex(&SlowControlMutex);
		/* format up success response */
		Message_setDataLen(M,0);
		Message_setStatus(M,SUCCESS);
		break;
	    case DSC_SET_PMT_HV_LIMIT:
		/* store maximum value */
		PMT_HV_max=unformatShort(data);
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
		pthread_lock_mutex(&SlowControlMutex);
		data[0]=halPMT_HVisEnabled();
		data[1]=0;
		formatShort(halReadPMT_HV(),&data[2]);
		pthread_lock_mutex(&SlowControlMutex);
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
	    Message_send(M,SD);
	}

}

