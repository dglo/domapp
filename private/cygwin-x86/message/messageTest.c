/* messageTest.c */

#include "domapp_common/DOMtypes.h"
#include "domapp_common/PacketFormatInfo.h"
#include "domapp_common/MessageAPIstatus.h"
#include "message/message.h"
#include "message/messageBuffers.h"
	
#define ERROR -1
#define MAX_TYPE 255
#define MAX_SUBTYPE 255
#define MAX_STATUS 255
#define TEST_QUEUE 1234

/* storage */
char *errorMsg;

/* test entry point */
int messageTest() {

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

    /* init messageBuffers */
    messageBuffers_init();

    /* allocate a buffer for use */
    oneBuffer=messageBuffers_allocate();
    if(oneBuffer == 0) {
	errorMsg="messageTest: unable to allocate message buffer";
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
			"messageTest: type set/read error";
		    return ERROR;
		}
		if(Message_getSubtype(oneBuffer) != subType) {
		    errorMsg=
			"messageTest: subtype set/read error";
		    return ERROR;
		}
		if(Message_getStatus(oneBuffer) != status) {
		    errorMsg=
			"messageTest: status set/read error";
		    return ERROR;
		}
	    }
	}
    }

    /* check data length settings */
    for(dataLen=0;dataLen<=MAXDATA_VALUE;dataLen++) {
	Message_setDataLen(oneBuffer,dataLen);
 	if(Message_dataLen(oneBuffer) != dataLen) {
	    errorMsg="messageTest: data length set/read error";
	    return ERROR;
	}
    }

    /* check for behavior at edge of permissible data length */
    Message_setDataLen(oneBuffer,MAXDATA_VALUE+1);
    if(Message_dataLen(oneBuffer) != MAXDATA_VALUE) {
	errorMsg="messageTest: MAXDATA_VALUE data length set/read error";
	return ERROR;
    }

    /* create a message queue */
    queue=Message_createQueue(TEST_QUEUE);
    if(queue < 0) {
	errorMsg="messageTest: cannot create message queue";
	return ERROR;
    }

    /* check that we can return from non blocking receive */
    receive=Message_receive_nonblock(oneBuffer,queue);
    if (receive != -1) {
	errorMsg=
	"messageTest: incorrect return from non blocking call on empty queue";
	return ERROR;
    }

    /* send a message to the above queue */
    send=Message_send(oneBuffer,queue);

    /* receive message from the above queue */
    receive=Message_receive(&twoBuffer,queue);

    /* check for correct message */
    if(oneBuffer != twoBuffer) {
	errorMsg="messageTest: incorrect send/receive message pair";
  	return ERROR;
    }

    /* check that we can return from non blocking receive */
    receive=Message_receive_nonblock(oneBuffer,queue);
    if (receive != -1) {
	errorMsg=
	"messageTest: incorrect return from non blocking call on empty queue";
	return ERROR;
    }

    /* done--no detected errors */
    errorMsg="messageTest: success";
    return 0;

}

char *messageTest_status() {
    return errorMsg;
}

