
/**
 * @mainpage domapp socket based simulation program
 * $Author: jacobsen $
 * $Date: 2003-02-22 01:01:12 $
 *
 * @section ration Rationale
 *
 * This code provides low level initialization and communications
 * code that allow execution of the DOM application within the 
 * either the Linux or Cygwin environment.  This particular verstion
 * uses stdin and stdout to perform all message passing communications.
 * It is intended to be run by the simboot program and, therefore, will
 * have these file descriptors mapped to a pre-established IP socket.
 *
 * @section details Implementation details
 *
 * This implementation uses the standard ipcmsg messaging package to
 * simulate the use of shared message queues between the various threads
 * of the DOM application.  This facility will be replaced by a similar
 * mechanism native to the DOM MB operating system (Nucleus).
 *
 * @subsection lang Language
 *
 * For DOM application compatibility, this code is written in C. 
 *
 */

/**
 * @file domappSocket.c
 * 
 * This file contains low level initialization routines used to
 * settup and manage the environment used in simulating execution of
 * the DOM application on other platforms.
 *
 * $Revision: 1.1 $
 * $Author: jacobsen $
 * $Date: 2003-02-22 01:01:12 $
*/

/** system include files */
#include <pthread.h>
#include <sys/socket.h>
#include <stdio.h>

/** project include files */
#include "msgHandler/msgHandlerTest.h"
#include "domapp_common/DOMtypes.h"
#include "domapp_common/packetFormatInfo.h"
#include "domapp_common/messageAPIstatus.h"
#include "domapp_common/commonMessageAPIstatus.h"
#include "message/message.h"
#include "message/messageBuffers.h"
#include "msgHandler/msgHandler.h"
#include "msgHandler/MSGHANDLERmessageAPIstatus.h"
	
/** fds index for stdin, stdout */
#define STDIN 0
#define STDOUT 1

/** error reporting flags, mostly just for information */
#define ERROR -1
#define COM_ERROR -2

/** maximum values for some MESSAGE_STRUCT fields */
#define MAX_TYPE 255
#define MAX_SUBTYPE 255
#define MAX_STATUS 255

/** timeout values for select timer struct */
#define TIMEOUT_10MSEC 10000
#define TIMEOUT_100MSEC 100000

/** ipcmsg key values used in creating DOM application msg queues */
#define RD_QUEUE 0
#define SD_QUEUE 1
#define SC_QUEUE 2
#define EC_QUEUE 3
#define DA_QUEUE 4
#define TM_QUEUE 5

/** thread variable for msgHandler */
void *msgHandlerThread(void *arg);

/** routines to handle send and receive of messages through stdin/out */
int recvMsg(void);
int sendMsg(void);

/** message queue ids for DOM application thread use */
int RD;
int SD;
int SC;
int EC;
int DA;
int TM;

/** packet driver counters, etc. For compatibility purposes only */
ULONG PKTrecv;
ULONG PKTsent;
ULONG NoStorage;
ULONG FreeListCorrupt;
ULONG PKTbufOvr;
ULONG PKTbadFmt;
ULONG PKTspare;

ULONG MSGrecv;
ULONG MSGsent;
ULONG tooMuchData;
ULONG IDMismatch;
ULONG CRCproblem;

/* initialize a mutex for the msgHandler */
pthread_mutex_t msgHandlerMutex=PTHREAD_MUTEX_INITIALIZER;

/**
 * Main entry that starts the DOM application off.  Should
 * never return unless error encountered.
 *
 * @param integer ID to be used as DOM ID for this simulation.
 *	If not present default value of 1234 is used.
 *
 * @return ERROR or COM_ERROR, no non-error returns possible.
 *
 * @see domappFile.c for file based version of same program.
*/

int main(int argc, char* argv[]) {

    /** storage */
    char *errorMsg;
    MESSAGE_STRUCT *recvBuffer;
    MESSAGE_STRUCT *sendBuffer;
    int i;
    int j;
    int k;
    int dataLen;
    int send;
    int receive;
    pthread_t msgHandlerID;
    int domID;
    int port;
    fd_set fds;
    struct timeval timeout;
    int nready;

    /* standard hello msg, will remove eventually */
    fprintf(stderr,"domapp: argc=%d, argv[0]=%s\n\r", argc, argv[0]);

    /* read args and configure */
    if (argc >= 1) {
        sscanf(argv[0], "%i", &domID);
    }
    else {
        // must have id and port to run w/o console
  	domID=1234;
    }

    /* show what dom we're running as */
    fprintf(stderr,"domapp: executing as DOM #%d\n\r",domID);

    /* init messageBuffers */
    messageBuffers_init();

    /* create message queues for msgHandler */
    RD = Message_createQueue(RD_QUEUE);
    if (RD < 0) {
	errorMsg="domapp: cannot create message queue RD";
	fprintf(stderr,"%s\n\r",errorMsg);
	return ERROR;
    }

    SD = Message_createQueue(SD_QUEUE);
    if (SD < 0) {
	errorMsg="domapp: cannot create message queue SD";
	fprintf(stderr,"%s\n\r",errorMsg);
	return ERROR;
    }

    SC = Message_createQueue(SC_QUEUE);
    if (SC < 0) {
	errorMsg="domapp: cannot create message queue SC";
	fprintf(stderr,"%s\n\r",errorMsg);
	return ERROR;
    }

    EC = Message_createQueue(EC_QUEUE);
    if (EC < 0) {
	errorMsg="domapp: cannot create message queue EC";
	fprintf(stderr,"%s\n\r",errorMsg);
	return ERROR;
    }

    DA = Message_createQueue(DA_QUEUE);
    if (DA < 0) {
	errorMsg="domapp: cannot create message queue DA";
	fprintf(stderr,"%s\n\r",errorMsg);
	return ERROR;
    }

    TM = Message_createQueue(TM_QUEUE);
    if (TM < 0) {
	errorMsg="domapp: cannot create message queue TM";
	fprintf(stderr,"%s\n\r",errorMsg);
	return ERROR;
    }

    i = pthread_create(&msgHandlerID, NULL, msgHandler, 0);

    fprintf(stderr, "Read to go\n");

    /* set up the select so that we can do read timeouts */
    FD_ZERO(&fds);
    FD_SET(STDIN, &fds);
    /* set up timeout */
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT_100MSEC;

    for (;;) {
	nready = select(FD_SETSIZE, &fds, (fd_set *)0, (fd_set *)0, 
	    &timeout);

	/* always reset timeout value to larger value */
	timeout.tv_usec = TIMEOUT_100MSEC;
	if (nready < 0) {
	    fprintf(stderr, "domapp: com error on read\n");
	    return COM_ERROR;
	}

	/* see if we have anything to read */
	if (nready > 0) {
	    fprintf(stderr, "domapp: performing read\n");
	    if (FD_ISSET(STDIN, &fds)) {
	    	if (recvMsg() < 0) {
		    /* error reported from receive */
		    break;
		}
	    }
	}
	  
	/* see if msgHandler has something to send out */
	else {
	    fprintf(stderr, "domapp: performing write\n");
	    if (sendMsg() < 0) {
	        /* error reported from send */
	        /* try a reconnect */
	        break;
	    }
	}


    }

    /* this is really bad, nothing left to do but bail */
    errorMsg="domapp: lost socket connection";
    fprintf(stderr,"%s\n\r",errorMsg);
    return 0;
}


/**
 * receive a complete message, place it into a message buffer
 * and queue it up to the msgHandler for processing.
 *
 * @return 0 if message properly handled, other wise error from
 *	communications routine.
 *
 * @see sendMsg() complimentary function.
*/

int recvMsg() {
    /** length of entire message to be received */
    long msgLength;
    /** number of bytes to be received for a given msg part */
    int targetLength;
    /** recv sts */
    int sts;
    /** pointer to message header struct */
    MESSAGE_STRUCT *recvBuffer_p;
    /* pointer to message data buffer */
    UBYTE *dataBuffer_p;
    /* misc buffer pointer */
    UBYTE *buffer_p;

    /* get the byte length of the entire message */
    targetLength = sizeof(long);
    buffer_p = (char *)&msgLength;

    /* read until we have entire length value */
    while (targetLength != 0) {

        sts = recv(STDIN, buffer_p, targetLength, 0);
	if (sts <0) {
	    return sts;
	}

	targetLength -= sts;
	if(targetLength == 0) {
	    break;
	}
	buffer_p += sts;
    }
    
    fprintf(stderr, "domapp: starting receive of message of length=%d\n",
	msgLength);

    /* check for illegal length value */
    if (msgLength > (sizeof(MESSAGE_STRUCT) + MAXDATA_VALUE)) {
	return ERROR;
    }
    
    /* receive the message header */
    recvBuffer_p = messageBuffers_allocate();
    if (recvBuffer_p == NULL) {
	return ERROR;
    }
    /* save data buffer address for  this message buffer */
    dataBuffer_p = Message_getData(recvBuffer_p);

    targetLength = sizeof(MESSAGE_STRUCT);
    buffer_p = (char *)recvBuffer_p;

    /* read until we have entire message header */
    while (targetLength != 0) {

        sts = recv(STDIN, buffer_p, targetLength, 0);
	if (sts < 0) {
  	    /* patch up the message buffer and release it */
	    Message_setData(recvBuffer_p, dataBuffer_p, MAXDATA_VALUE);
	    messageBuffers_release(recvBuffer_p);
	    return sts;
	}

	targetLength -= sts;
	if(targetLength == 0) {
	    break;
	}
	buffer_p += sts;
    }
    

    fprintf(stderr, "domapp: header received, starting data portion of length=%d\n",
   	Message_dataLen(recvBuffer_p));

    /* check internal message header length */
    if ((msgLength - sizeof(MESSAGE_STRUCT) -
	Message_dataLen(recvBuffer_p)) != 0) {

	/* patch up message buffer and release it */
	Message_setData(recvBuffer_p, dataBuffer_p, MAXDATA_VALUE);
	messageBuffers_release(recvBuffer_p);
	return ERROR;
    }


    /* receive the optional message data portion */
    targetLength = Message_dataLen(recvBuffer_p);
    buffer_p = dataBuffer_p;

    while (targetLength != 0) {

        sts = recv(STDIN, buffer_p, targetLength, 0);
	if (sts < 0) {
  	    /* patch up the message buffer and release it */
	    Message_setData(recvBuffer_p, dataBuffer_p, MAXDATA_VALUE);
	    messageBuffers_release(recvBuffer_p);
	    return sts;
	}

	targetLength -= sts;
	if(targetLength == 0) {
	    break;
	}
	buffer_p += sts;
    }
    

    /* send it off to the msgHandler */
    Message_send(recvBuffer_p, RD);

    return 0;
}

/**
 * Check msgHandler message queue for outgoing messages. If
 * one is present, assemble message and send it.
 *
 * @return 0  no errors, otherwise return value of low level 
 * 	communications routine.
 *
 * @see recvMsg()
*/


int sendMsg() {
    int sts;
    long sendLen;
    MESSAGE_STRUCT *sendBuffer_p;

    if(Message_receive_nonblock(&sendBuffer_p, SD) > 0) {
	fprintf(stderr, "domapp: have a message to send\n");
	sendLen = sizeof(long) + sizeof(MESSAGE_STRUCT) +
	    (long)Message_dataLen(sendBuffer_p);

	/* send out the message length */
	fprintf(stderr, "domapp: sent message length=%d\n", sendLen);
        sts = send(STDOUT, &sendLen, sizeof(long), 0);
	if(sts < 0) {
	    return sts;
	}

	/* send out the message header */
	fprintf(stderr, "domapp: sent message header\n");
	sts = send(STDOUT, sendBuffer_p, sizeof(MESSAGE_STRUCT), 0);
	if(sts < 0) {
	    return sts;
	}

	/* send out the optional data body, so length could be 0 */
	fprintf(stderr, "domapp: sent data portion of %d bytes\n",
	    Message_dataLen(sendBuffer_p));
	sts = send(STDOUT, Message_getData(sendBuffer_p), 
	    Message_dataLen(sendBuffer_p), 0);

	/* always delete the message buffer-even if there was a com error */
    	messageBuffers_release(sendBuffer_p);

	if(sts < 0) {
	    return sts;
	}
    }
    else {
	fprintf(stderr, "domapp: sendMsg: nothing to send\n");
	return 0;
    }
}

