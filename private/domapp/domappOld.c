/* domapp.c */

#include <pthread.h>
#include <sys/socket.h>
#include <stdio.h>
#include "msgHandler/msgHandlerTest.h"
#include "domapp_common/DOMtypes.h"
#include "domapp_common/packetFormatInfo.h"
#include "domapp_common/messageAPIstatus.h"
#include "domapp_common/commonMessageAPIstatus.h"
#include "message/message.h"
#include "message/messageBuffers.h"
#include "msgHandler/msgHandler.h"
#include "msgHandler/MSGHANDLERmessageAPIstatus.h"
	
#define STDIN 0
#define STDOUT 1
#define ERROR -1
#define COM_ERROR -2
#define MAX_TYPE 255
#define MAX_SUBTYPE 255
#define MAX_STATUS 255
#define TEST_QUEUE 1234
#define TIMEOUT_10MSEC 10000
#define TIMEOUT_100MSEC 100000
#define RD_QUEUE 0
#define SD_QUEUE 1
#define SC_QUEUE 2
#define EC_QUEUE 3
#define DA_QUEUE 4
#define TM_QUEUE 5

void *msgHandlerThread(void *arg);
int recvMsg(void);
int sendMsg(void);

/* storage */
char *errorMsg;
int RD;
int SD;
int SC;
int EC;
int DA;
int TM;

/* packet driver counters, etc. */
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

pthread_mutex_t msgHandlerMutex=PTHREAD_MUTEX_INITIALIZER;

/* main code that starts communications driver and then domapp */
int main(int argc, char* argv[]) {

    /* storage */
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

    fprintf(stderr,"domapp: argc=%d, argv[0]=%s\n\r", argc, argv[0]);
    /* read args and configure communications */
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
    timeout.tv_sec = 10;
    timeout.tv_usec = TIMEOUT_100MSEC;

    for (;;) {
	nready = select(FD_SETSIZE, &fds, (fd_set *)0, (fd_set *)0, 
	    &timeout);

	fprintf(stderr, "out of select, nready=%d\n",nready);

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

    errorMsg="domapp: lost socket connection";
    fprintf(stderr,"%s\n\r",errorMsg);
    return 0;
}


int recvMsg() {
    long msgLength;
    UBYTE *lengthBuf_p;
    int targetLength;
    int sts;
    MESSAGE_STRUCT *recvBuffer_p;
    UBYTE *dataBuffer_p;
    UBYTE *buffer_p;

    targetLength = sizeof(long);
    buffer_p = (char *)&msgLength;

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

