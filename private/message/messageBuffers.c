/* message.c */

#include "domapp_common/DOMtypes.h"
#include "domapp_common/packetFormatInfo.h"
#include "domapp_common/messageAPIstatus.h"
#include "message/message.h"
#include "message/messageBuffers.h"

/* for use under cygwin and linux */
#include <pthread.h>

/* declare how many message and data buffers we will create */
#define MAX_MSG 16
	
/* static storage for message headers and data buffers */

MESSAGE_STRUCT *msgFreeList[MAX_MSG];
MESSAGE_STRUCT msgHdr[MAX_MSG];
UBYTE msgData[MAX_MSG][MAXDATA_VALUE];
int nextMsgFree=0;
int lastMsgFree=0;
int freeListCorrupt=0;
pthread_mutex_t msgBuffersMutex=PTHREAD_MUTEX_INITIALIZER;

void messageBuffers_init()
{
    int i;

    pthread_mutex_lock(&msgBuffersMutex);

    for(i=0;i<MAX_MSG;i++) {
	msgFreeList[i]=&msgHdr[i];
	msgHdr[i].data=&msgData[i][0];
    }
    nextMsgFree=0;
    lastMsgFree=0;

    pthread_mutex_unlock(&msgBuffersMutex);
}

MESSAGE_STRUCT *messageBuffers_allocate()
{
    
    MESSAGE_STRUCT *m;

    pthread_mutex_lock(&msgBuffersMutex);

    m=msgFreeList[nextMsgFree];
    if(m!=0) {
	m->head.hd.dlenHI=0;
	m->head.hd.dlenLO=0;
	msgFreeList[nextMsgFree]=0;
	nextMsgFree++;
	if(nextMsgFree>=MAX_MSG) {
	    nextMsgFree=0;
	}
    }

    pthread_mutex_unlock(&msgBuffersMutex);
    return m;
}
 	
void messageBuffers_release(MESSAGE_STRUCT *m)
{

    pthread_mutex_lock(&msgBuffersMutex);

    if(msgFreeList[lastMsgFree]==0) {
	msgFreeList[lastMsgFree]=m;
	lastMsgFree++;
	if(lastMsgFree>=MAX_MSG) {
	    lastMsgFree=0;
	}
    }
    else {
    	freeListCorrupt++;
    }
    pthread_mutex_unlock(&msgBuffersMutex);
}

int messageBuffers_freeCnt() {
    int count;

    pthread_mutex_lock(&msgBuffersMutex);

    if(nextMsgFree < lastMsgFree) {
	count = lastMsgFree-nextMsgFree;
    }
    else if(nextMsgFree > lastMsgFree) {
	count = MAX_MSG-nextMsgFree+lastMsgFree;
    }
    else {
    	if(msgFreeList[nextMsgFree] == 0) {
	    count = 0;
    	}
	else {
	    count = MAX_MSG;
	}
    }
    /* release the lock */
    pthread_mutex_unlock(&msgBuffersMutex);
    return count;
    
}

int messageBuffers_totalCnt() {
    return MAX_MSG;
}
