#ifndef _MESSAGE_BUFFERS_H_
#define _MESSAGE_BUFFERS_H_
/* messageBuffers.h */


void messageBuffers_init(void);

MESSAGE_STRUCT *messageBuffers_allocate(void);
 	
void messageBuffers_release(MESSAGE_STRUCT *m);

int messageBuffers_freeCnt(void); 

int messageBuffers_totalCnt(void);

#endif
