
/*
Author: Chuck McParland
Start Date: May 4, 1999
Description:
	DOM Experiment Control service thread to handle
	special run-related functions.  Performs all 
	"standard" DOM service functions.
Last Modification:
*/

/* system include files */
#include <pthread.h>

/* DOM-related includes */
#include "domapp_common/DOMtypes.h"
#include "message/message.h"
#include "expControl/expControl.h"
#include "domapp_common/messageAPIstatus.h"
#include "domapp_common/commonServices.h"
#include "domapp_common/commonMessageAPIstatus.h"
#include "expControl/EXPmessageAPIstatus.h"
#include "domapp_common/DOMstateInfo.h"

/* extern functions */
extern void formatLong(ULONG value, UBYTE *buf);
extern BOOLEAN beginRun(void);
extern BOOLEAN endRun(void);
extern BOOLEAN forceRunReset(void);

/* message queue identifiers */
extern int EC;
extern int SD;

/* external mutex for DOM state access */
pthread_mutex_t DOMstateMutex;

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

/* DOM state information resource lock */
//RESOURCE DOM_state_lock;

/* Exp Control  Entry Point */
void *expControl(void *dummy) {

    MESSAGE_STRUCT *M;
    UBYTE *data;
    int tempInt;
    UBYTE tmpByte;
    UBYTE *tmpPtr;
    USHORT tmpShort;

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
    pthread_mutex_lock(&DOMstateMutex);
    DOM_state=DOM_IDLE;
    DOM_config_access=DOM_CONFIG_CHANGES_ALLOWED;
    DOM_status=DOM_STATE_SUCCESS;
    DOM_cmdSource=EXPERIMENT_CONTROL;
    DOM_constraints=DOM_CONSTRAINT_NO_CONSTRAINTS;
    strcpy(DOM_errorString,DOM_STATE_STR_STARTUP);
    pthread_mutex_unlock(&DOMstateMutex);

    for(;;) {
	/* blocking wait for a message in our stack. */
	Message_receive(&M,EC);
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

	    /* get run state */
	    case EXPCONTROL_GET_DOM_STATE:
	        pthread_mutex_lock(&DOMstateMutex);
		data[0]=DOM_state;
		data[1]=DOM_config_access;
		data[2]=DOM_status;
		data[3]=DOM_cmdSource;
		formatLong(DOM_constraints,&data[4]);
		strcpy(&data[8],DOM_errorString);
		pthread_mutex_unlock(&DOMstateMutex);
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
	    Message_send(M,SD);
	}

}

