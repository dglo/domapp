/* msgHandlerTest.c */

#include <pthread.h>
#include "msgHandler/msgHandlerTest.h"
#include "domapp_common/DOMtypes.h"
#include "domapp_common/PacketFormatInfo.h"
#include "domapp_common/messageAPIstatus.h"
#include "domapp_common/commonMessageAPIstatus.h"
#include "message/message.h"
#include "message/messageBuffers.h"
#include "msgHandler/msgHandler.h"
#include "msgHandler/MSGHANDLERmessageAPIstatus.h"
	
#define ERROR -1
#define MAX_TYPE 255
#define MAX_SUBTYPE 255
#define MAX_STATUS 255
#define TEST_QUEUE 1234
#define RD_QUEUE 0
#define SD_QUEUE 1
#define SC_QUEUE 2
#define EC_QUEUE 3
#define DA_QUEUE 4
#define TM_QUEUE 5

void *msgHandlerThread(void *arg);

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

/* test entry point */
int msgHandlerTest() {

    /* storage */
    MESSAGE_STRUCT *oneBuffer;
    MESSAGE_STRUCT *twoBuffer;
    int i;
    int j;
    int k;
    int dataLen;
    int send;
    int receive;
    pthread_t msgHandlerID;

    /* init messageBuffers */
    messageBuffers_init();

    /* create message queues for msgHandler */
    RD=Message_createQueue(RD_QUEUE);
    if(RD < 0) {
	errorMsg="messageTest: cannot create message queue RD";
	return ERROR;
    }

    SD=Message_createQueue(SD_QUEUE);
    if(SD < 0) {
	errorMsg="messageTest: cannot create message queue SD";
	return ERROR;
    }

    SC=Message_createQueue(SC_QUEUE);
    if(SC < 0) {
	errorMsg="messageTest: cannot create message queue SC";
	return ERROR;
    }

    EC=Message_createQueue(EC_QUEUE);
    if(EC < 0) {
	errorMsg="messageTest: cannot create message queue EC";
	return ERROR;
    }

    DA=Message_createQueue(DA_QUEUE);
    if(DA < 0) {
	errorMsg="messageTest: cannot create message queue DA";
	return ERROR;
    }

    TM=Message_createQueue(TM_QUEUE);
    if(TM < 0) {
	errorMsg="messageTest: cannot create message queue TM";
	return ERROR;
    }

    i=pthread_create(&msgHandlerID,NULL,msgHandler,0);

    /* allocate a buffer for use during tests */
    oneBuffer=messageBuffers_allocate();
    if(oneBuffer == 0) {
	errorMsg="messageTest: unable to allocate message buffer";
	return ERROR;
    }

    Message_setType(oneBuffer,MESSAGE_HANDLER);
    Message_setSubtype(oneBuffer,GET_SERVICE_STATE);
    Message_setDataLen(oneBuffer,0);

    Message_send(oneBuffer,RD);

    for(i=0;i<10;i++) {
        receive=Message_receive(&twoBuffer,SD);
 	if(receive!=0) {
	    break;
	}
	sleep(1);
    }

    if (((Message_getStatus(twoBuffer)!=SUCCESS) ||
	(Message_dataLen(twoBuffer)!=GET_SERVICE_STATE_LEN)) ||
	(Message_getSubtype(twoBuffer)!=GET_SERVICE_STATE)) {

   	    errorMsg="msgHandlerTest: error in GET_SERVICE_STATE";
	    return ERROR;
    }

    printf("type: %d\n",Message_getType(twoBuffer));
    printf("subtype: %d\n",Message_getSubtype(twoBuffer));
    printf("status: %d\n",Message_getStatus(twoBuffer));
    printf("datalength: %d\n",Message_dataLen(twoBuffer));

    errorMsg="messageTest: success";
    return 0;
}

char *msgHandlerTest_status() {
    return errorMsg;
}

