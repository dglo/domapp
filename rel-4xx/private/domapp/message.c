/* message.c */

#include "DOMtypes.h"
#include "messageAPIstatus.h"
#include "message.h"
#include "string.h"

const int messageFlag= MESSAGE_FLAG_VALUE;
/* Maximum messagelength in byte */

/* Default Constructor empty ignore message */
void Message_init(MESSAGE_STRUCT *msg) {
  msg->head.hd.mt     = 0; 	      
  msg->head.hd.dlenLO = 0;
  msg->head.hd.dlenHI = 0;
  memset(msg->data, 0, MAXDATA_VALUE);
}

/* structs and storage for messaging system */
#define MESSAGE_QUEUE_SIZE  32
#define NUM_MESSAGE_QUEUES  16
#define RECEIVER_WAITING    1
#define NO_RECEIVER_WAITING 0

typedef struct {
    int receiverWaiting;
    MESSAGE_STRUCT *queue[MESSAGE_QUEUE_SIZE];
    int nextElemPut;
    int nextElemGet;
} MESSAGE_QUEUE_LIST;

MESSAGE_QUEUE_LIST msgQueueList[NUM_MESSAGE_QUEUES];

/* public functions */

UBYTE Message_getType(MESSAGE_STRUCT *msgStruct) { return msgStruct->head.hd.mt; }

UBYTE Message_getSubtype(MESSAGE_STRUCT *msgStruct) { return msgStruct->head.hd.mst; }

UBYTE Message_getStatus(MESSAGE_STRUCT *msgStruct) { return msgStruct->head.hd.status; }

UBYTE* Message_getData(MESSAGE_STRUCT *msgStruct) { return msgStruct->data; }

int Message_dataLen(MESSAGE_STRUCT *msgStruct) { 
  return (msgStruct->head.hd.dlenLO + 
	  (msgStruct->head.hd.dlenHI << 8) );
}

void Message_setType(MESSAGE_STRUCT *msgStruct,UBYTE t) { msgStruct->head.hd.mt= t; }

void Message_setSubtype(MESSAGE_STRUCT *msgStruct, UBYTE st) { msgStruct->head.hd.mst= st; }

void Message_setStatus(MESSAGE_STRUCT *msgStruct, UBYTE status) {
  msgStruct->head.hd.status= status;
}

void Message_setData(MESSAGE_STRUCT *msgStruct,
		     UBYTE *d, int l) {
  int len = l > MAXDATA_VALUE ? MAXDATA_VALUE : l;
  memcpy(msgStruct->data, d, len);
  msgStruct->head.hd.dlenLO= len & 0xff;
  msgStruct->head.hd.dlenHI= ( len >> 8) & 0xff;
}

void Message_setDataLen(MESSAGE_STRUCT *msgStruct,
	int l) {
 if(l > MAXDATA_VALUE) {
    l=MAXDATA_VALUE;
 }
 msgStruct->head.hd.dlenLO= l & 0xff;
 msgStruct->head.hd.dlenHI= ( l >> 8) & 0xff;
}

/* create a cygwin message queue */
int Message_createQueue(int q) {
    /* check for illegal queue number */
    if(q >= NUM_MESSAGE_QUEUES) {
	return MEM_CREATE_ERROR;
    }
    /* init the struct areas for this queue */

    msgQueueList[q].nextElemPut = 0;
    msgQueueList[q].nextElemGet = 0;
    msgQueueList[q].receiverWaiting = NO_RECEIVER_WAITING;
    return q;
}

/* send/receive message */

int Message_send(MESSAGE_STRUCT *msg,
	int q)
{
    // check for full state
    if(msgQueueList[q].nextElemPut > msgQueueList[q].nextElemGet) {
	if(msgQueueList[q].nextElemPut != (MESSAGE_QUEUE_SIZE-1)) {
	    if(msgQueueList[q].nextElemGet == 0) {
		// we're full
	 	return MEM_ERROR;
	    }
	}
    }
    else if(msgQueueList[q].nextElemPut < msgQueueList[q].nextElemGet) {
	if(msgQueueList[q].nextElemGet-msgQueueList[q].nextElemPut >= 1) {
	    // we're full
	    return MEM_ERROR;
   	}
    }
    else {
	// copy message into queue and adjust pointers
	msgQueueList[q].queue[msgQueueList[q].nextElemPut] = msg;
	// bump pointer
	msgQueueList[q].nextElemPut++;
	if(msgQueueList[q].nextElemPut >= MESSAGE_QUEUE_SIZE) {
	    msgQueueList[q].nextElemPut = 0;
	}
    }
    return MEM_SUCCESS;
}

int Message_forward(MESSAGE_STRUCT *msgStruct,
	int queue)
{
    return Message_send(msgStruct,queue);
}

int Message_receive(MESSAGE_STRUCT **msg,
	int q)
{

    // loop til non empty
    while (msgQueueList[q].nextElemGet == msgQueueList[q].nextElemPut) {
	// we wait
	msgQueueList[q].receiverWaiting = RECEIVER_WAITING;
    }

    *msg = msgQueueList[q].queue[msgQueueList[q].nextElemGet];
    msgQueueList[q].nextElemGet++;
    if(msgQueueList[q].nextElemGet >= MESSAGE_QUEUE_SIZE) {
	msgQueueList[q].nextElemGet = 0;
    }
    // now release the lock and return
    msgQueueList[q].receiverWaiting = NO_RECEIVER_WAITING;
    return MEM_SUCCESS;
}

int Message_receive_nonblock(MESSAGE_STRUCT **msg,
	int q)
{
    
    // see if we are empty
    if(msgQueueList[q].nextElemGet == msgQueueList[q].nextElemPut) {
 	return MEM_ERROR;
    }

    *msg = msgQueueList[q].queue[msgQueueList[q].nextElemGet];
    msgQueueList[q].nextElemGet++;
    if(msgQueueList[q].nextElemGet >= MESSAGE_QUEUE_SIZE) {
	msgQueueList[q].nextElemGet = 0;
    }
    return MEM_SUCCESS;
}

