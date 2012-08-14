#include <string.h>

#include "hal/DOM_MB_types.h"
#include "hal/DOM_MB_fpga.h"
#include "hal/DOM_MB_domapp.h"
#include "hal/DOM_FPGA_comm.h"

#include "message.h"
#include "interval.h"
#include "commonServices.h"
#include "dataAccess.h"
#include "dataAccessRoutines.h"
#include "DACmessageAPIstatus.h"
#include "messageAPIstatus.h"

// defined in dataAccess.c
extern UBYTE dataFormat;
extern UBYTE compMode;

UBYTE interval_initialized;
UBYTE interval_state;
unsigned long long interval_starttime;

#define INTERVAL (FPGA_HAL_TICKS_PER_SEC)
#define SN_SEND 1
#define MONI_SEND 2


void interval_init() {
  interval_initialized = FALSE;
  interval_starttime = 0;
  interval_state = MONI_SEND;
}

void interval_start() {
  interval_initialized = TRUE;
  interval_starttime = hal_FPGA_DOMAPP_get_local_clock();
  interval_state = MONI_SEND;
}


void interval_force_stop() {
  interval_initialized = FALSE;
}

int interval_service(MESSAGE_STRUCT *M) {
  unsigned long long cur_time;
  UBYTE *data;
  int tmpInt;
  
  if(interval_initialized) {
    cur_time = hal_FPGA_DOMAPP_get_local_clock();
    data = Message_getData(M);
    
    if((cur_time - interval_starttime) < INTERVAL) {
      // if there is data send that..
      if( (tmpInt = fillMsgWithData(data, MAXDATA_VALUE, dataFormat, compMode))) {
        Message_setType(M, DATA_ACCESS);
        Message_setSubtype(M, DATA_ACC_GET_DATA);
        Message_setDataLen(M, tmpInt);
        Message_setStatus(M, SUCCESS);
	
        return 1;
      }
    } else {
      // time is up for the interval.
      // we have to send monitoring data
      // and supernova data
      if(interval_state == MONI_SEND) {
        // send a moni message
        // sets the length and status
        Message_setType(M, DATA_ACCESS);
        Message_setSubtype(M, DATA_ACC_GET_NEXT_MONI_REC );
	
        fillMsgWithMoniData(M);
        interval_state = SN_SEND;
        return 1;
      } else if (interval_state == SN_SEND) {
        tmpInt = fillMsgWithSNData(data, MAXDATA_VALUE);
        
        // the runcore method in data collector class of the 
        // string hub code iterated until it gets a supernova 
        // data packet with something in it.  ie tmpInt>0
        // only send a message to the surface if we have
        // a message with something in it.
        
        if(tmpInt>0) {
	  
          Message_setType(M, DATA_ACCESS);
          Message_setSubtype(M, DATA_ACC_GET_SN_DATA);
          
          Message_setDataLen(M, tmpInt);
          Message_setStatus(M, SUCCESS);
          
          interval_initialized=FALSE;
          interval_state = MONI_SEND;
          interval_starttime = 0;
          return 1;
        } 
      }
    }
  }
  return 0;
}
