/* message.h */
#ifndef _MESSAGE_H_
#define _MESSAGE_H_

/* defines for message header struct and field
extraction routines. 
*/



#define MESSAGE_FLAG_VALUE 1
#define PACKET_SIZE_VALUE 8 
#define MAX_TOTAL_MESSAGE 4092
#define MAXDATA_VALUE (MAX_TOTAL_MESSAGE - MSG_HDR_LEN)

/* private per instance data */
#define MSG_HDR_LEN 8
typedef struct {
  union HEAD {
    struct HD {
      UBYTE mt;
      UBYTE mst;
      UBYTE dlenHI;
      UBYTE dlenLO;
      UBYTE res[2];
      UBYTE msgID;
      UBYTE status;
    } hd;
    UBYTE h[MSG_HDR_LEN];
  } head;  
  UBYTE data[MAXDATA_VALUE];
} MESSAGE_STRUCT;

#define MEM_ERROR 0
#define MEM_CREATE_ERROR -1
#define MEM_SUCCESS 1

/* create functions */
void Message_init(MESSAGE_STRUCT *msgStruct);

/* set/get functions */
UBYTE Message_getType(MESSAGE_STRUCT *msgStruct); 
UBYTE Message_getSubtype(MESSAGE_STRUCT *msgStruct); 
UBYTE Message_getStatus(MESSAGE_STRUCT *msgStruct);
UBYTE* Message_getData(MESSAGE_STRUCT *msgStruct);
int	Message_dataLen(MESSAGE_STRUCT *msgStruct);
void  Message_setType(MESSAGE_STRUCT *msgStruct,
	UBYTE t); 
void  Message_setSubtype(MESSAGE_STRUCT *msgStruct,
	UBYTE st); 
void  Message_setData(MESSAGE_STRUCT *msgStruct,
	UBYTE *d, int size); 
void Message_setDataLen(MESSAGE_STRUCT *msgStruct,
	int l);  
void  Message_setStatus (MESSAGE_STRUCT *msgStruct,
	UBYTE status); 

int Message_createQueue(int q);
/* send message to recepient */
int Message_send(MESSAGE_STRUCT *msgStruct, int q);	
int Message_forward(MESSAGE_STRUCT *msgStruct, int q);	
int Message_receive(MESSAGE_STRUCT **msgStruct, int q);


#endif
