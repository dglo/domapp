/* message.c */

#include "domapp_common/DOMtypes.h"
#include "domapp_common/PacketFormatInfo.h"
#include "domapp_common/MessageAPIstatus.h"
#include "message/message.h"

/* includes for cygwin message passing fcns */
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
 
/* defines for cygwin messaging */
#define NORMAL_MSG 1
#define WAIT 0

const int messageFlag= MESSAGE_FLAG_VALUE;
/* Maximum messagelength in byte */

/* Default Constructor empty ignore message */
void Message_init(MESSAGE_STRUCT *msg) {
  msg->head.hd.mt= 0; 	      
  msg->head.hd.dlenLO= 0;
  msg->head.hd.dlenHI= 0;
  msg->data= (UBYTE*) 0;   
}

/* cygwin msg struct to use for transfers */
struct msgbuf message;
size_t msgLen=sizeof(MESSAGE_STRUCT *);


/* public functions */

UBYTE Message_getType(MESSAGE_STRUCT *msgStruct)
{
 return msgStruct->head.hd.mt;
}

UBYTE Message_getSubtype(MESSAGE_STRUCT *msgStruct)
{
 return msgStruct->head.hd.mst;
}

UBYTE Message_getStatus(MESSAGE_STRUCT *msgStruct)
{
 return msgStruct->head.hd.status;
}

UBYTE* Message_getData(MESSAGE_STRUCT *msgStruct)
{
	return msgStruct->data;
}

int Message_dataLen(MESSAGE_STRUCT *msgStruct)
{
	return (msgStruct->head.hd.dlenLO + 
		(msgStruct->head.hd.dlenHI << 8) );
}

void Message_setType(MESSAGE_STRUCT *msgStruct,UBYTE t)
{
 msgStruct->head.hd.mt= t; 	     
}

void Message_setSubtype(MESSAGE_STRUCT *msgStruct,
	UBYTE st)
{
 msgStruct->head.hd.mst= st;
}

void Message_setStatus(MESSAGE_STRUCT *msgStruct,
	UBYTE status)
{
 msgStruct->head.hd.status= status;
}

void Message_setData(MESSAGE_STRUCT *msgStruct,
	UBYTE *d, int l)
{
 msgStruct->data = d;
 msgStruct->head.hd.dlenLO= l & 0xff;
 msgStruct->head.hd.dlenHI= ( l >> 8) & 0xff;
}

void Message_setDataLen(MESSAGE_STRUCT *msgStruct,
	int l)
{
 if(l > MAXDATA_VALUE) {
    l=MAXDATA_VALUE;
 }
 msgStruct->head.hd.dlenLO= l & 0xff;
 msgStruct->head.hd.dlenHI= ( l >> 8) & 0xff;
}

/* create a cygwin message queue */
int Message_createQueue(int q) {
    int msgflg = (IPC_CREAT | IPC_PRIVATE) | 0666;
    //int msgflg = IPC_CREAT | 0666;
    key_t key = q;
    return msgget(key,msgflg);
}

/* send/receive message */

int Message_send(MESSAGE_STRUCT *msgStruct,
	int queue)
{
    message.mtype=NORMAL_MSG;
    memcpy(&message.mtext[0],&msgStruct,msgLen);

    return msgsnd(queue,&message,msgLen,IPC_NOWAIT);
}

int Message_forward(MESSAGE_STRUCT *msgStruct,
	int queue)
{
    return Message_send(msgStruct,queue);
}

int Message_receive(MESSAGE_STRUCT **msgStruct,
	int queue)
{
    int sts;

    sts=msgrcv(queue,&message,MSGMAX,NORMAL_MSG,WAIT);

    if(sts<0) {
	return sts;
    }
    else {
	memcpy(msgStruct,&message.mtext[0],msgLen);
  	return sts;
    }
}

int Message_receive_nonblock(MESSAGE_STRUCT **msgStruct,
	int queue)
{
    int sts;

    sts=msgrcv(queue,&message,MSGMAX,NORMAL_MSG,IPC_NOWAIT);

    if(sts<=0) {
	return sts;
    }
    else {
	memcpy(msgStruct,&message.mtext[0],msgLen);
  	return sts;
    }
}

	
	
