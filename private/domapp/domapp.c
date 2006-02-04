/*
  domapp - IceCube DOM Application program for use with 
           "Real"/"domapp" FPGA
           J. Jacobsen (jacobsen@npxdesigns.com), Chuck McParland
  $Date: 2005-12-23 21:08:42 $
  $Revision: 1.35 $
*/

#include <unistd.h> /* Needed for read/write */
#include <string.h>

// DOM-related includes
#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_domapp.h"
#include "message.h"
#include "moniDataAccess.h"
#include "expControl.h"
#include "dataAccess.h"
#include "dataAccessRoutines.h"
#include "moniDataAccess.h"
#include "msgHandler.h"
#include "domSControl.h"

#define STDIN  0
#define STDOUT 1

#define MIN_HW_IVAL (FPGA_HAL_TICKS_PER_SEC+100)

/* routines to handle send and receive of messages through stdin/out */
int getmsg(char * halmsg, char * message);
void putmsg(char * message);

/* packet driver counters, etc. For compatibility purposes only */
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

/* Monitoring buffer, static allocation: */
UBYTE monibuf[MONI_CIRCBUF_RECS * MONI_REC_SIZE];

char halmsg[MAX_TOTAL_MESSAGE];
int  halmsg_remain = 0;

int main(void) {
  char message[MAX_TOTAL_MESSAGE];

  unsigned long long t_hw_last, t_cf_last, tcur;
  unsigned long long moni_hardware_interval, moni_config_interval;

  t_hw_last = t_cf_last = hal_FPGA_DOMAPP_get_local_clock();

  /* Start up monitoring system -- do this before other *Init()'s because
     they may want to insert monitoring information */
  moniInit(monibuf, MONI_MASK);
  moniRunTests();
  
  /* manually init all the domapp components */
  msgHandlerInit();
  domSControlInit();
  expControlInit();
  dataAccessInit();

  halDisableAnalogMux(); /* John Kelley suggests explicitly disabling this by default */
  
  halEnableBarometer(); /* Increases power consumption slightly but 
			   enables power to be read out */
  halStartReadTemp();
  USHORT temperature = 0; // Chilly

  for (;;) {    
    /* Insert periodic monitoring records */

    tcur = hal_FPGA_DOMAPP_get_local_clock();      

    /* Guarantee that the HW monitoring records occur no faster than at a
       rate specified by MIN_HW_IVAL -- this guarantees a unique SPE/MPE measurement
       each record: */
    moni_hardware_interval = moniGetHdwrIval();
    if(moni_hardware_interval < MIN_HW_IVAL) moni_hardware_interval = MIN_HW_IVAL;
    moni_config_interval = moniGetConfIval();

    long long dthw = tcur-t_hw_last;    
    long long dtcf = tcur-t_cf_last;

    /* Hardware monitoring */
    if(   moni_hardware_interval > 0 
       && (dthw < 0 || dthw > moni_hardware_interval)) {
      /* Update temperature if it's done; start next one */
      if(halReadTempDone()) {
	temperature = halFinishReadTemp();
	halStartReadTemp();
      }
      moniInsertHdwrStateMessage(tcur, temperature, 
				 hal_FPGA_DOMAPP_spe_rate_immediate(),
				 hal_FPGA_DOMAPP_mpe_rate_immediate());
      t_hw_last = tcur;
    }
    
    /* Software monitoring */
    if(moni_config_interval > 0 && (dtcf < 0 || dtcf > moni_config_interval)) {
      moniInsertConfigStateMessage(tcur);
      t_cf_last = tcur;
    }

    /* Check for new message */
    if(getmsg(halmsg, message)) {
      msgHandler((MESSAGE_STRUCT *) message);
      putmsg(message);
    }
  } /* for(;;) */
}

void putmsg(char *buf) {
  int len = Message_dataLen((MESSAGE_STRUCT *) buf);
  if(len > MAXDATA_VALUE) return;
  int nw = len + MSG_HDR_LEN;
  hal_FPGA_send(0, nw, buf);
}

static int receive(char *b) {
  int nr, type;
  hal_FPGA_receive(&type, &nr, b);
  return (nr > 0)? nr : 0;
}

int getmsg(char *halmsg,    /* HAL message buffer updated as needed */
	   char *msgbuf) {  /* Message data to fill; both buffers >= MAX_TOTAL_MESSAGE */
  static int halmsg_remain = 0; /* Remaining unparsed data already read from HAL */
  static int ipos          = 0; /* Position in current domapp msg buffer */
  static int haloff        = 0; /* Position in current HAL msg buffer */
  /* Try to fill data from HAL to build message.
     Return true if message successfully built.  Otherwise, retry.
     Assumes most msg reads from HAL are domapp-message-aligned,
     so that in case of corrupted message buffer, we can just toss
     HAL message data until an intact packet arrives. */

  /* Wait for data to parse */
  if(! halmsg_remain) {
    if(!hal_FPGA_msg_ready()) return 0;
    halmsg_remain = receive(halmsg);
    if(! halmsg_remain) return 0;
    haloff = 0;
  }

  /* Wait for header */
  int needed = MSG_HDR_LEN-ipos;
  if(needed > 0) {
    int tocopy = (halmsg_remain > needed) ? needed : halmsg_remain;
    memcpy(msgbuf+ipos, halmsg+haloff, tocopy);
    ipos += tocopy;
    halmsg_remain -= tocopy;
    haloff += tocopy;
  }
  
  if(ipos < MSG_HDR_LEN) return 0; /* No header yet -- can't proceed until more data */

  int len = Message_dataLen((MESSAGE_STRUCT *) msgbuf);

  if(len < 0 || len > MAXDATA_VALUE) { /* Toss corrupt message */
    mprintf("getmsg: data length (%d) too long!", len);
    halmsg_remain = 0;
    ipos = 0;
    haloff = 0;
    return 0;
  }

  if(len == 0) {
    ipos = 0;
    return 1;
  }

  /* Do same thing for message payload we did for header */
  if(! halmsg_remain) return 0; 
  needed = len + MSG_HDR_LEN - ipos; /* Number of bytes needed past header */
  if(needed > 0) {
    int tocopy = (halmsg_remain > needed) ? needed : halmsg_remain;
    memcpy(msgbuf+ipos, halmsg+haloff, tocopy);
    ipos += tocopy;
    halmsg_remain -= tocopy;
    haloff += tocopy;
  }

  if(ipos >= MSG_HDR_LEN + len) { /* Don't need '>' but be safe */
    ipos = 0;
    return 1;
  } else {
    return 0;
  }
}
 
