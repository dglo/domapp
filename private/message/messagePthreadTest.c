/* messagePthreadTest.c */

#include "domapp_common/DOMtypes.h"
#include "domapp_common/packetFormatInfo.h"
#include "domapp_common/messageAPIstatus.h"
#include "message/message.h"
#include "message/messageBuffers.h"
	
/* include for cygwin messge passing fcns */
#include <pthread.h>
#include <unistd.h>

#define ERROR -1
#define MAX_TYPE 255
#define MAX_SUBTYPE 255
#define MAX_STATUS 255
#define TEST_QUEUE 1

/* storage */
char *errorMsg;

/* test structures used for interthread testing */
    int threadCounter;
    int msgPutCounter;
    int msgGetCounter;

/* thread prototypes */
    void *AThread(void *arg);
    void *BThread(void *arg);
    void *ACVThreadTest(void *arg);
    void *BCVThreadTest(void *arg);
    void *AmsgTest(void *arg);
    void *BmsgTest(void *arg);

/* thread ID's */
    pthread_t AThreadID;
    pthread_t BThreadID;
    pthread_t ACVThreadTestID;
    pthread_t BCVThreadTestID;
    pthread_t AmsgTestID;
    pthread_t BmsgTestID;

/* thread status */

/* thread mutexes */
    pthread_mutex_t threadTestMutex = PTHREAD_MUTEX_INITIALIZER;

/* needed mutex and condition variables */
    pthread_cond_t threadTestCV = PTHREAD_COND_INITIALIZER;
    

/* test entry point */
int messagePthreadTest() {

    /* storage */
    MESSAGE_STRUCT *oneBuffer;
    MESSAGE_STRUCT *twoBuffer;
    int i;
    int j;
    int k;
    UBYTE type;
    UBYTE subType;
    UBYTE status;
    int dataLen;
    int queue;
    int send;
    int receive;
    int numPasses;

    /* run the mutex test threads to see if there is any
     chance that the message routines will work. */
    threadCounter = 0;
    i = pthread_create(&AThreadID,NULL,AThread,0);
    i = pthread_create(&BThreadID,NULL,BThread,0);

    pthread_join(AThreadID,NULL);
    pthread_join(BThreadID,NULL);

    if(threadCounter != 2) {
  	errorMsg = "messagePthreadTest: basic pthread test failure";
    	return ERROR;
    }

    /* now test out condition variables */
    threadCounter = 0;
    i = pthread_create(&ACVThreadTestID,NULL,ACVThreadTest,0);
    i = pthread_create(&BCVThreadTestID,NULL,BCVThreadTest,0);

    pthread_join(ACVThreadTestID,NULL);
    pthread_join(ACVThreadTestID,NULL);

    if(threadCounter != 2) {
  	errorMsg = 
	    "messagePthreadTest: basic pthread condition variable test failure";
   	return ERROR;
    }

    /* init messageBuffers */
    messageBuffers_init();

    /* allocate a buffer for use */
    oneBuffer=messageBuffers_allocate();
    if(oneBuffer == 0) {
	errorMsg="messagePthreadTest: unable to allocate message buffer";
	return ERROR;
    }

    
    /* loop through message struct values, set and check various combos */
    for(i=0;i<=MAX_TYPE;i++) {
   	type=(UBYTE)i;
        Message_setType(oneBuffer,type);

	for(j=0;j<=MAX_SUBTYPE;j++) {
	    subType=(UBYTE)j;
	    Message_setSubtype(oneBuffer,subType);

	    for(k=0;k<=MAX_STATUS;k++) {
		status=(UBYTE)k;
		Message_setStatus(oneBuffer,status);

		if(Message_getType(oneBuffer) != type) {
		    errorMsg=
			"messagePthreadTest: type set/read error";
		    return ERROR;
		}
		if(Message_getSubtype(oneBuffer) != subType) {
		    errorMsg=
			"messagePthreadTest: subtype set/read error";
		    return ERROR;
		}
		if(Message_getStatus(oneBuffer) != status) {
		    errorMsg=
			"messagePthreadTest: status set/read error";
		    return ERROR;
		}
	    }
	}
    }

    /* check data length settings */
    for(dataLen=0;dataLen<=MAXDATA_VALUE;dataLen++) {
	Message_setDataLen(oneBuffer,dataLen);
 	if(Message_dataLen(oneBuffer) != dataLen) {
	    errorMsg="messagePthreadTest: data length set/read error";
	    return ERROR;
	}
    }

    /* check for behavior at edge of permissible data length */
    Message_setDataLen(oneBuffer,MAXDATA_VALUE+1);
    if(Message_dataLen(oneBuffer) != MAXDATA_VALUE) {
	errorMsg="messagePthreadTest: MAXDATA_VALUE data length set/read error";
	return ERROR;
    }

    /* create a message queue */
    queue=Message_createQueue(TEST_QUEUE);

    if(queue < 0) {
	errorMsg="messagePthreadTest: cannot create message queue";
	return ERROR;
    }

    /* check that we can return from non blocking receive */
    receive=Message_receive_nonblock(oneBuffer,queue);
    if (receive != -1) {
	errorMsg=
	"messagePthreadTest: incorrect return from non blocking call on empty queue";
	return ERROR;
    }

    /* send a message to the above queue */
    send=Message_send(oneBuffer,queue);

    /* receive message from the above queue */
    receive=Message_receive(&twoBuffer,queue);

    /* check for correct message */
    if(oneBuffer != twoBuffer) {
	errorMsg="messagePthreadTest: incorrect send/receive message pair";
  	return ERROR;
    }

    /* check that we can return from non blocking receive */
    receive=Message_receive_nonblock(oneBuffer,queue);
    if (receive != -1) {
	errorMsg=
	"messagePthreadTest: incorrect return from non blocking call on empty queue";
	return ERROR;
    }

    /* put and get messages from multiple threads with 
     random delays for realism. */
    msgPutCounter = 0;
    msgGetCounter = 0;
    numPasses=1024;
    i = pthread_create(&AmsgTestID,NULL,AmsgTest,&numPasses);
    i = pthread_create(&BmsgTestID,NULL,BmsgTest,&numPasses);

    pthread_join(AmsgTestID,NULL);
    pthread_join(BmsgTestID,NULL);

    if(msgPutCounter != msgGetCounter) {
  	errorMsg = "messagePthreadTest: threaded msg queue test failed";
    	return ERROR;
    }

    /* done--no detected errors */
    errorMsg="messagePthreadTest: success";
    return 0;

}

char *messagePthreadTest_status() {
    return errorMsg;
}

/* simple thread sequencing test */
void *AThread(void *arg) {
    // initialize counter
    threadCounter=1;
    // sleep for a bit
    usleep(1000);
}

void *BThread(void *arg) {
    // sleep for a bit
    usleep(10000);
    // change counter
    threadCounter=2;
}

/* simple thread condition variable test */
void *ACVThreadTest(void *arg) {
    // lock the mutex associated with the condition variable
    pthread_mutex_lock(&threadTestMutex);

    // loop to eliminate false triggers 
    while (threadCounter == 0) {
	// sleep until signaled
 	pthread_cond_wait(&threadTestCV, &threadTestMutex);
    }

    threadCounter=2;
    // release the lock, we're done
    pthread_mutex_unlock(&threadTestMutex);
}

void *BCVThreadTest(void *arg) {
    // wait for a bit
    usleep(10000);
    pthread_mutex_lock(&threadTestMutex);
    // change the value
    threadCounter=1;
    pthread_cond_signal(&threadTestCV);
    // release the lock
    pthread_mutex_unlock(&threadTestMutex);
}

void *AmsgTest(void *arg) {
    int sendSts;
    MESSAGE_STRUCT *putBuffer;
    UBYTE *data;
    int *numPasses=(int *)arg;

    msgPutCounter = 0;
    while (msgPutCounter < *numPasses) {
    	/* allocate a buffer for use */
    	putBuffer = messageBuffers_allocate();

    	if(putBuffer != 0) {
	    /* get address of data portion of message */
	    data = Message_getData(putBuffer);
	    /* insert a counter */
	    *(int *)data = msgPutCounter;
	    /* now insert the message into the queue */
    	    sendSts = Message_send(putBuffer,TEST_QUEUE);
	    if(sendSts == MEM_ERROR) {
		messageBuffers_release(putBuffer);
	    }
	    else {
		msgPutCounter++;
		//printf("wrote msg %d\n",msgPutCounter);
	    }
	}
	/* sleep for a while */
	/* this is a millisec because anything less in Cygwin
	  appears to turn into a nop. And, since threads don't
	  appear to round robin very well, the second thread 
	  starves out and the test runs forever. */
	usleep(1000);
    }
}

void *BmsgTest(void *arg) {
    int recvSts;
    MESSAGE_STRUCT *getBuffer;
    UBYTE *data;
    int msgSequence;
    int *numPasses=(int *)arg;

    msgGetCounter = 0;

    while (msgGetCounter < *numPasses) {
	/* try to get a message */
	recvSts = Message_receive(&getBuffer,TEST_QUEUE);

	/* we should never get an error return */
	if(recvSts == MEM_ERROR) {
	    return;
	}
	else {
	    /* point into the data portion of the message */
	    data = Message_getData(getBuffer);
	    /* get the sequence number inserted by writing thread */
	    msgSequence = *(int *)data;
	    /* make sure its correct */
	    if(msgSequence != msgGetCounter) {
		return;
	    }
	    messageBuffers_release(getBuffer);
	    msgGetCounter++;
	}
    }
}
