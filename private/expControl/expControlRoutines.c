// ExpControlRoutines.c //
/*
Author: Chuck McParland
Start Date: May 4, 1999
Description:
	Helper routines for DOM Experiment Control 
	service thread.
Last Modification:
*/

/* system includes */
#include <pthread.h>

// DOM-related includes
#include "domapp_common/DOMtypes.h"
#include "domapp_common/DOMstateInfo.h"
#include "message/message.h"
#include "expControl/expControl.h"
#include "expControl/EXPmessageAPIstatus.h"

/* local functions, data */
extern UBYTE DOM_state;
extern UBYTE DOM_config_access;
extern UBYTE DOM_status;
extern UBYTE DOM_cmdSource;
extern ULONG DOM_constraints;
extern char *DOM_errorString;

/* external mutex for DOM state access */
extern pthread_mutex_t DOMstateMutex;

// local functions, data


BOOLEAN beginRun() {
    if(DOM_state!=DOM_IDLE) {
	return FALSE;
    }
    else {
	pthread_mutex_lock(&DOMstateMutex);
	DOM_state=DOM_RUN_IN_PROGRESS;
	pthread_mutex_unlock(&DOMstateMutex);
	return TRUE;
    }
}

BOOLEAN endRun() {
    if(DOM_state!=DOM_RUN_IN_PROGRESS) {
	return FALSE;
    }
    else {
	pthread_mutex_lock(&DOMstateMutex);
	DOM_state=DOM_IDLE;
	pthread_mutex_unlock(&DOMstateMutex);
	return TRUE;
    }
}

BOOLEAN forceRunReset() {
    pthread_mutex_lock(&DOMstateMutex);
    DOM_state=DOM_IDLE;
    pthread_mutex_unlock(&DOMstateMutex);
    return TRUE;
}


