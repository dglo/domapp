/**
 * @mainpage domapp - DOM Application program
 * @author Chuck McParland originally, now updated and maintained by 
 * J. Jacobsen (jacobsen@npxdesigns.com)
 *
 * $Date: 2005-04-14 21:30:45 $
 */

/**
 * @file domapp.c
 * 
 * Domapp main loop.  Dispacher for routines to handle messages,
 * triggering, monitoring events, pedestal runs.
 * 
 * $Revision: 1.3 $
 * $Author: jacobsen $ Based on original code by Chuck McParland
 * $Date: 2005-04-14 21:30:45 $
*/

#include <unistd.h> /* Needed for read/write */
#include <stdio.h>  /* snprintf */
#include <stdlib.h> /* Needed for lbm.h */
#include <string.h> /* "              " */

// DOM-related includes
#include "hal/DOM_MB_hal.h"
#include "lbm.h"
#include "message.h"
#include "moniDataAccess.h"
#include "expControl.h"
#include "dataAccess.h"
#include "dataAccessRoutines.h"
#include "moniDataAccess.h"
#include "msgHandler.h"
#include "domSControl.h"

/** fds index for stdin, stdout */
#define STDIN  0
#define STDOUT 1

/** Code for scalar averaging */
#define FPGA_CLOCK_FREQ    40000000 /* 40 MHz */
#define FPGA_CLOCKS_PER_MS (FPGA_CLOCK_FREQ/1000)
#define SCALAR_PERIOD_MS   102
#define SCALAR_PERIOD_CLKS (SCALAR_PERIOD_MS*FPGA_CLOCKS_PER_MS)
#define SCALAR_PERIODS_PER_AVERAGE 9

/** routines to handle send and receive of messages through stdin/out */
int getmsg(char *);
void putmsg(char *);

/** packet driver counters, etc. For compatibility purposes only */
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
  unsigned long long t_scalar_last;

  t_scalar_last = t_hw_last = t_cf_last = hal_FPGA_TEST_get_local_clock();
  
  ///* init messageBuffers - now use single static message buffer */
  //messageBuffers_init();
  
  /* Start up monitoring system -- do this before other *Init()'s because
     they may want to insert monitoring information */
  moniInit(monibuf, MONI_MASK);
  moniRunTests();
  //moniTestAllMonitorRecords();

  /* manually init all the domapp components */
  msgHandlerInit();
  domSControlInit();
  expControlInit();
  dataAccessInit();
  
  /* Get buffer, temporary replacement for lookback mem. */
  if(lbm_init()) {
    mprintf("Malloc of LBM buffer failed");
  }
  int numTriggerChecks = 0;
  
  halEnableBarometer(); /* Increases power consumption slightly but 
			   enables power to be read out */
  halStartReadTemp();
  USHORT temperature = 0; // Chilly

  long spe_sum = 0, mpe_sum = 0;
  int numscalars = 0, quorum = 0;

  for (;;) {
    
    /* Insert periodic monitoring records */
    tcur = hal_FPGA_TEST_get_local_clock();      
    moni_hardware_interval = moniGetHdwrIval();
    moni_config_interval   = moniGetConfIval();

    long long dthw = tcur-t_hw_last;    
    long long dtcf = tcur-t_cf_last;
    long long dtsc = tcur-t_scalar_last;

    if(!quorum && dtsc > SCALAR_PERIOD_CLKS) { /* Handle scalar averaging */
      t_scalar_last = tcur;
      int spe = hal_FPGA_TEST_get_spe_rate();
      int mpe = hal_FPGA_TEST_get_mpe_rate();
      if(spe >= 0 && mpe >= 0) {
	numscalars++;
	spe_sum += spe;
	mpe_sum += mpe;
	if(numscalars >= SCALAR_PERIODS_PER_AVERAGE) {
	  quorum = 1;
	}
      }
    }

    /* Hardware monitoring */
    if(moni_hardware_interval > 0 && (dthw < 0 || dthw > moni_hardware_interval)) {
      /* Update temperature if it's done; start next one */
      if(halReadTempDone()) {
	temperature = halFinishReadTemp();
	halStartReadTemp();
      }
      moniInsertHdwrStateMessage(tcur, temperature, quorum?spe_sum:0, quorum?mpe_sum:0);
      quorum = numscalars = spe_sum = mpe_sum = 0;
      t_hw_last = tcur;
    }
    
    /* Software monitoring */
    if(moni_config_interval > 0 && (dtcf < 0 || dtcf > moni_config_interval)) {
      moniInsertConfigStateMessage(tcur);
      t_cf_last = tcur;
    }
    
    /* Check for new message */
    if(halIsInputData()) {
      if(getmsg(message)) msgHandler((MESSAGE_STRUCT *) message); 
      putmsg(message);
    } else if(lbm_ok()) {
      numTriggerChecks++;
      bufferLBMTriggers();
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

