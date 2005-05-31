/*
  domapp - IceCube DOM Application program for use with 
           "Real"/"domapp" FPGA
           J. Jacobsen (jacobsen@npxdesigns.com), Chuck McParland
  $Date: 2005-05-26 03:45:19 $
  $Revision: 1.29 $
*/

#include <unistd.h> /* Needed for read/write */

// DOM-related includes
#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_domapp.h"
//#include "lbm.h"
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
int getmsg(char *);
void putmsg(char *);

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
    if(halIsInputData() && getmsg(message)) {
      msgHandler((MESSAGE_STRUCT *) message);
      putmsg(message);
    }

  } /* for(;;) */
}

void putmsg(char *buf) {
  int len = Message_dataLen((MESSAGE_STRUCT *) buf);
  if(len > MAXDATA_VALUE) return;
  int nw = len + MSG_HDR_LEN;
  write(STDOUT, buf, nw);
}

int getmsg(char *buf) {
  int nh = read(STDIN, buf, MSG_HDR_LEN);
  if(nh != MSG_HDR_LEN) return 0;
  int len = Message_dataLen((MESSAGE_STRUCT *) buf);
  if(len > MAXDATA_VALUE) return 0;
  if(len == 0) return 1;
  int np = read(STDIN, buf+nh, len);
  if(np != len) return 0;
  return 1;
}

