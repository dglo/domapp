/* message.c */

#include "domapp_common/DOMtypes.h"
#include "domapp_common/PacketFormatInfo.h"
#include "domapp_common/MessageAPIstatus.h"
#include "message/message.h"
#include "message/messageBuffers.h"
	
#define ERROR -1

/* storage */
char *errorMsg;

/* test entry point */
int messageBuffersTest() {

    /* storage */
    int i;
    int maxNumBufs;
    int allocateCnt;
    MESSAGE_STRUCT *oneBuffer;
    MESSAGE_STRUCT *buffers[16];

    /* init messageBuffers */
    messageBuffers_init();

    /* find out how many buffers we have */
    maxNumBufs=messageBuffers_totalCnt();

    /* storage for array of message buffer pointers */
    //buffers=(MESSAGE_STRUCT **)malloc(maxNumBufs*sizeof(MESSAGE_STRUCT *));

    /* allocate a buffer and check that we can return it */
    if(messageBuffers_totalCnt() != messageBuffers_freeCnt()) {
	errorMsg="messageBuffersTest: total and free count not equal at init";
	return ERROR;
    }

    oneBuffer=messageBuffers_allocate();
    if(messageBuffers_freeCnt() != (messageBuffers_totalCnt()-1)) {
	errorMsg="messageBuffersTest: allocate did not reduce buffer count";
	return ERROR;
    }
    
    messageBuffers_release(oneBuffer);
    if(messageBuffers_freeCnt() != messageBuffers_totalCnt()) {
	errorMsg="messageBuffersTest: release did not return buffer to pool";
	return ERROR;
    }

    /* allocate all buffers and see if count tracks */ 
    allocateCnt=0;
    while(messageBuffers_freeCnt() != 0) {
	buffers[allocateCnt]=messageBuffers_allocate();
	allocateCnt++;
    }

    if(allocateCnt != messageBuffers_totalCnt()) {
	errorMsg="messageBuffersTest: total allocated buffer count error";
	return ERROR;
    }

    /* release buffers and see if count tracks  correctly */
    while(messageBuffers_freeCnt() != messageBuffers_totalCnt()) {
	messageBuffers_release(buffers[allocateCnt]);
	allocateCnt--;
    }

    if(allocateCnt != 0) {
	errorMsg="messageBuffersTest: total released buffer count error";
	return ERROR;
    }

    errorMsg="messageBuffersTest: success";
    return 0;

}

char *messageBuffersTest_status() {
    return errorMsg;
}

