/* message.c */

#include "domapp_common/DOMtypes.h"
#include "domapp_common/PacketFormatInfo.h"
#include "domapp_common/MessageAPIstatus.h"
#include "message/message.h"
#include "message/messageBuffers.h"

/* declare how many message and data buffers we will create */
#define MAX_MSG 16
	
/* static storage for message headers and data buffers */

MESSAGE_STRUCT *msgFreeList[MAX_MSG];
MESSAGE_STRUCT msgHdr[MAX_MSG];
UBYTE msgData[MAX_MSG][MAXDATA_VALUE];
int nextMsgFree=0;
int lastMsgFree=0;
int freeListCorrupt=0;

void messageBuffers_init()
{
    int i;

    for(i=0;i<MAX_MSG;i++) {
	msgFreeList[i]=&msgHdr[i];
	msgHdr[i].data=&msgData[i][0];
    }
    nextMsgFree=0;
    lastMsgFree=0;
}

MESSAGE_STRUCT *messageBuffers_allocate()
{
    
    MESSAGE_STRUCT *m;

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
    return m;
}
 	
void messageBuffers_release(MESSAGE_STRUCT *m)
{

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
}

int messageBuffers_freeCnt() {
    if(nextMsgFree < lastMsgFree) {
	return (lastMsgFree-nextMsgFree);
    }
    else if(nextMsgFree > lastMsgFree) {
	return (MAX_MSG-nextMsgFree+lastMsgFree);
    }
    else {
    	if(msgFreeList[nextMsgFree] == 0) {
	    return 0;
    	}
	else {
	    return MAX_MSG;
	}
    }
}

int messageBuffers_totalCnt() {
    return MAX_MSG;
}
