#ifndef _MESSAGE_H_
#define _MESSAGE_H_


/* private per instance data */
typedef struct {
	union HEAD{
	 struct HD {
	  UBYTE mt;
	  UBYTE mst;
	  UBYTE dlenHI;
	  UBYTE dlenLO;
	  UBYTE res[2];
	  UBYTE msgID;
	  UBYTE status;
	 } hd;
	 UBYTE h[8];
	} head;
  
  UBYTE *data;
} MESSAGE_STRUCT;

#define MESSAGE_FLAG_VALUE 1
#define PACKET_SIZE_VALUE 8 
#define MAXDATA_VALUE 4096

#define FPGA_TRIG_FIFO 0
#define FPGA_CMD_FIFO 1
#define FPGA_DATA_FIFO 2

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
