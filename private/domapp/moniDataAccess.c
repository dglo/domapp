/* 
 * moniDataAccess.c
 * Routines to store and fetch monitoring data from a circular buffer
 * John Jacobsen, npxdesigns.com for IceCube
 * May, 2003
 * $Id: moniDataAccess.c,v 1.14.4.2 2007-08-08 03:04:57 jacobsen Exp $
 */


#include "hal/DOM_MB_types.h"
#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_domapp.h"
#include "message.h"
#include "messageAPIstatus.h"
#include "commonServices.h"
#include "DSCmessageAPIstatus.h"
#include "moniDataAccess.h"
#include "dataAccessRoutines.h"
#include "dataAccess.h"
#include "domSControl.h"
#include "expControl.h"
#include "msgHandler.h"
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

static UBYTE *moniBaseAddr;
static int   moniMask;
static int   moniInitialized = 0;
static ULONG moniReadIndex, moniWriteIndex;
static int   moniCounterWraps;
static int   moniOverruns;
static BOOLEAN moniProvisionalRecFetch = FALSE;
static int   moniIsInTrouble = 0;
static unsigned long long moniHdwrIval=0, moniConfIval=0, moniFastIval=0;


int moniGetElementLen(void)  { return sizeof(struct moniRec);  }
void moniZeroIndices(void)   { moniReadIndex = moniWriteIndex = 0; }
void moniIncWriteIndex(void) { moniWriteIndex++; }

unsigned long long moniGetHdwrIval(void) { return moniHdwrIval; }
unsigned long long moniGetConfIval(void) { return moniConfIval; }
unsigned long long moniGetFastIval(void) { return moniFastIval; }

void moniSetIvals(unsigned long long mhi, unsigned long long mci, unsigned long long mfi) {
  moniHdwrIval = mhi;
  moniConfIval = mci;
  moniFastIval = mfi;
}

UBYTE * moniGetWriteBufferAddr(void) {
  return moniBaseAddr + (sizeof(struct moniRec) * (moniWriteIndex & moniMask));
}

UBYTE * moniBufAddr(long idx) {
  return moniBaseAddr + (sizeof(struct moniRec) * (idx & moniMask));
}


void moniInit(UBYTE *bufBaseAddr, int mask) {
  moniBaseAddr = bufBaseAddr;
  moniMask     = mask;

  moniZeroIndices();

  moniCounterWraps = 0;
  moniOverruns     = 0;

  moniSetIvals(0,0,0); /* The idea here is that no monitoring occurs 
			  if these values aren't set to something != 0 */

  moniInitialized = 1;
}


void moniInsertRec(struct moniRec *m) {
  if(!moniInitialized) return;
  if(moniIsInTrouble) return;

  memcpy(moniGetWriteBufferAddr(), m, sizeof(struct moniRec));
  moniIncWriteIndex();
}

int moniHaveData(void) {
  return ((moniWriteIndex - moniReadIndex) != 0);
}

MONI_STATUS moniFetchRec(struct moniRec *m) {
  /* If this function returns MONI_OK, m is filled with 
     a proper record.  Contents pointed to by m are undefined
     otherwise. */

  if(!moniInitialized) return MONI_NOTINITIALIZED;
  if(moniIsInTrouble)  return MONI_ERROR;

  /* Have new data? */
  if((moniWriteIndex - moniReadIndex) == 0)
    return MONI_NODATA;

  /* Look for overflow - if so, give oldest that fits */
  if((moniWriteIndex - moniReadIndex) > MONI_CIRCBUF_RECS)
    moniReadIndex = moniWriteIndex - MONI_CIRCBUF_RECS; 

  /* Good to go.  Copy it. */
  memcpy(m, moniBufAddr(moniReadIndex), sizeof(struct moniRec));

  moniProvisionalRecFetch = TRUE;
  return MONI_OK;
}

void moniAcceptRec(void) {
  if(moniProvisionalRecFetch) {
    moniReadIndex++;
    moniProvisionalRecFetch = FALSE;
  }
}
  

/* Type 2 - event log tagged with ASCII
   text string */
void moniInsertDiagnosticMessage(char *msg, unsigned long long time, int len) {
  struct moniRec mr;
  mr.dataLen = (len > MAXMONI_DATA) ? MAXMONI_DATA : len;
  mr.moniEvtType = MONI_TYPE_LOG_MSG;
  mr.time = time;
  memcpy(mr.data, msg, mr.dataLen);
  moniInsertRec(&mr);
}

void mprintf(char *fmt, ...) {
  unsigned char buf[MAXMONI_DATA]; /* Message portion only! */
  va_list ap;
  unsigned long long time = hal_FPGA_DOMAPP_get_local_clock();
  va_start(ap, fmt);
  int n = vsnprintf(buf, MAXMONI_DATA, fmt, ap);
  va_end(ap);
  moniInsertDiagnosticMessage(buf, time, n);
}

static inline USHORT moniBEShort(unsigned short x) {
  return (   ((x >> 8) & 0xFF) | ((x & 0xFF) << 8) );
}

static inline ULONG moniBELong(ULONG x) {
  return (   (((x >> 24) & 0xFF) << 0)  |
	     (((x >> 16) & 0xFF) << 8)  |
	     (((x >>  8) & 0xFF) << 16) |
	     (((x >>  0) & 0xFF) << 24));
}


static inline unsigned long long moniBELongLong(unsigned long long x) {
  return (   (((x >> 56) & 0xFF) << 0)  |
             (((x >> 48) & 0xFF) << 8)  |
             (((x >> 40) & 0xFF) << 16) |
             (((x >> 32) & 0xFF) << 24) |
             (((x >> 24) & 0xFF) << 32) |
             (((x >> 16) & 0xFF) << 40) |
             (((x >>  8) & 0xFF) << 48) |
             (((x >>  0) & 0xFF) << 56));
}

void moniFillBogusConfigStateMessage(struct moniConfig * mc) {
  int itest;
  itest = 1;
  mc->config_event_version = itest++;
  mc->spare0               = itest++;
  mc->hw_config_len        = moniBEShort(itest++);
  memcpy(mc->dom_mb_id,"ABCDEF",6);
  mc->spare1               = moniBEShort(itest++);
  memcpy(mc->hw_base_id, "GHIJKLMN", 8);
  //mc->hw_base_id           = moniBELongLong(itest++);
  mc->fpga_build_num       = moniBEShort(itest++);
  mc->sw_config_len        = moniBEShort(itest++);
  mc->dom_mb_sw_build_num  = moniBEShort(itest++);
  mc->msg_hdlr_major       = itest++;
  mc->msg_hdlr_minor       = itest++;
  mc->exp_ctrl_major       = itest++;
  mc->exp_ctrl_minor       = itest++;
  mc->slo_ctrl_major       = itest++;
  mc->slo_ctrl_minor       = itest++;
  mc->data_acc_major       = itest++;
  mc->data_acc_minor       = itest++;
  mc->daq_config_len       = moniBEShort(itest++);
  mc->trig_config_info     = moniBELong(itest++);
  mc->atwd_readout_info    = moniBELong(itest++);
}

void swapOrder(unsigned char *dst, unsigned char *src, int nbytes) {
  /* Swap byte order of two entities of same byte width */
  int ib;
  for(ib=0;ib<nbytes;ib++) {
    dst[ib] = src[nbytes-ib-1];
  }
}

void moniInsertConfigStateMessage(unsigned long long time) {
  struct moniRec mr;
  struct moniConfig mc;
  int test = FALSE;
  unsigned long long boardID, HVID;

  if(test) {
    /* Fill record with bogus values for testing */
    moniFillBogusConfigStateMessage(&mc);
  } else {
    mc.config_event_version = 0;
    mc.spare0               = 0;
    mc.hw_config_len        = moniBEShort(18);

    boardID = halGetBoardIDRaw();
    HVID    = domappHVSerialRaw();

    swapOrder(mc.hw_base_id, (void *) &HVID, 8);
    swapOrder(mc.dom_mb_id, (void *) &boardID, 6);

    mc.spare1               = moniBEShort(0);         
    mc.fpga_build_num       = moniBEShort(hal_FPGA_query_build());
    mc.sw_config_len        = moniBEShort(10);
    mc.dom_mb_sw_build_num  = moniBEShort(ICESOFT_BUILD); /* From hal/dom-ws */
    mc.msg_hdlr_major       = MSGHANDLER_MAJOR_VERSION;
    mc.msg_hdlr_minor       = MSGHANDLER_MINOR_VERSION;
    mc.exp_ctrl_major       = EXP_MAJOR_VERSION;
    mc.exp_ctrl_minor       = EXP_MINOR_VERSION;
    mc.slo_ctrl_major       = DSC_MAJOR_VERSION;
    mc.slo_ctrl_minor       = DSC_MINOR_VERSION;
    mc.data_acc_major       = DAC_MAJOR_VERSION;
    mc.data_acc_minor       = DAC_MINOR_VERSION;
    mc.daq_config_len       = moniBEShort(8);
    mc.trig_config_info     = moniBELong(0);
    mc.atwd_readout_info    = moniBELong(0);
  }


  // skip:
  mr.dataLen = sizeof(mc);
  mr.moniEvtType = MONI_TYPE_CONF_STATE_MSG;
  mr.time = time;
  memcpy(mr.data, (UBYTE *) &mc, sizeof(mc));
  moniInsertRec(&mr);
}


void moniFillBogusHdwrStateMessage(struct moniHardware *mh) {
  int itest = 1;
  mh->STATE_EVENT_VERSION    = itest++;
  mh->spare                  = itest++;

  mh->ADC_VOLTAGE_SUM        = moniBEShort(itest++);                         
  mh->ADC_5V_POWER_SUPPLY    = moniBEShort(itest++);
  mh->ADC_PRESSURE           = moniBEShort(itest++);
  mh->ADC_5V_CURRENT         = moniBEShort(itest++);
  mh->ADC_3_3V_CURRENT       = moniBEShort(itest++);
  mh->ADC_2_5V_CURRENT       = moniBEShort(itest++);
  mh->ADC_1_8V_CURRENT       = moniBEShort(itest++);
  mh->ADC_MINUS_5V_CURRENT   = moniBEShort(itest++);

  mh->DAC_ATWD0_TRIGGER_BIAS  =  moniBEShort(itest++);
  mh->DAC_ATWD0_RAMP_TOP      =  moniBEShort(itest++);
  mh->DAC_ATWD0_RAMP_RATE     =  moniBEShort(itest++);
  mh->DAC_ATWD_ANALOG_REF     =  moniBEShort(itest++);
  mh->DAC_ATWD1_TRIGGER_BIAS  =  moniBEShort(itest++);
  mh->DAC_ATWD1_RAMP_TOP      =  moniBEShort(itest++);
  mh->DAC_ATWD1_RAMP_RATE     =  moniBEShort(itest++);
  mh->DAC_PMT_FE_PEDESTAL     =  moniBEShort(itest++);
  mh->DAC_MULTIPLE_SPE_THRESH =  moniBEShort(itest++);
  mh->DAC_SINGLE_SPE_THRESH   =  moniBEShort(itest++);
  mh->DAC_LED_BRIGHTNESS      =  moniBEShort(itest++);
  mh->DAC_FAST_ADC_REF        =  moniBEShort(itest++);
  mh->DAC_INTERNAL_PULSER     =  moniBEShort(itest++);
  mh->DAC_FE_AMP_LOWER_CLAMP  =  moniBEShort(itest++);
  mh->DAC_FL_REF              =  moniBEShort(itest++);
  mh->DAC_MUX_BIAS            =  moniBEShort(itest++);

  mh->PMT_BASE_HV_SET_VALUE     = moniBEShort(itest++);
  mh->PMT_BASE_HV_MONITOR_VALUE = moniBEShort(itest++);
  mh->DOM_MB_TEMPERATURE        = moniBEShort(itest++);
  mh->SPE_RATE                  = moniBELong(itest++);
  mh->MPE_RATE                  = moniBELong(itest++);
}

/* Type MONI_TYPE_HDWR_STATE_MSG - log dom state message */
void moniInsertHdwrStateMessage(unsigned long long time, USHORT temperature, 
				long spe_sum, long mpe_sum) {
  struct moniRec mr;
  struct moniHardware mh;
  int test = FALSE;

  if(test) {
    moniFillBogusHdwrStateMessage(&mh);
  } else {    
    mh.STATE_EVENT_VERSION       = HARDWARE_STATE_EVENT_VERSION;
    mh.spare                     = 0;
    mh.ADC_VOLTAGE_SUM           = moniBEShort(halReadADC(DOM_HAL_ADC_VOLTAGE_SUM));
    mh.ADC_5V_POWER_SUPPLY       = moniBEShort(halReadADC(DOM_HAL_ADC_5V_POWER_SUPPLY));
    mh.ADC_PRESSURE              = moniBEShort(halReadADC(DOM_HAL_ADC_PRESSURE));
    mh.ADC_5V_CURRENT            = moniBEShort(halReadADC(DOM_HAL_ADC_5V_CURRENT));
    mh.ADC_3_3V_CURRENT          = moniBEShort(halReadADC(DOM_HAL_ADC_3_3V_CURRENT));
    mh.ADC_2_5V_CURRENT          = moniBEShort(halReadADC(DOM_HAL_ADC_2_5V_CURRENT));
    mh.ADC_1_8V_CURRENT          = moniBEShort(halReadADC(DOM_HAL_ADC_1_8V_CURRENT));
    mh.ADC_MINUS_5V_CURRENT      = moniBEShort(halReadADC(DOM_HAL_ADC_MINUS_5V_CURRENT));
    mh.DAC_ATWD0_TRIGGER_BIAS    = moniBEShort(halReadDAC(DOM_HAL_DAC_ATWD0_TRIGGER_BIAS));
    mh.DAC_ATWD0_RAMP_TOP        = moniBEShort(halReadDAC(DOM_HAL_DAC_ATWD0_RAMP_TOP));
    mh.DAC_ATWD0_RAMP_RATE       = moniBEShort(halReadDAC(DOM_HAL_DAC_ATWD0_RAMP_RATE));
    mh.DAC_ATWD_ANALOG_REF       = moniBEShort(halReadDAC(DOM_HAL_DAC_ATWD_ANALOG_REF));
    mh.DAC_ATWD1_TRIGGER_BIAS    = moniBEShort(halReadDAC(DOM_HAL_DAC_ATWD1_TRIGGER_BIAS));
    mh.DAC_ATWD1_RAMP_TOP        = moniBEShort(halReadDAC(DOM_HAL_DAC_ATWD1_RAMP_TOP));
    mh.DAC_ATWD1_RAMP_RATE       = moniBEShort(halReadDAC(DOM_HAL_DAC_ATWD1_RAMP_RATE));
    mh.DAC_PMT_FE_PEDESTAL       = moniBEShort(halReadDAC(DOM_HAL_DAC_PMT_FE_PEDESTAL));
    mh.DAC_MULTIPLE_SPE_THRESH   = moniBEShort(halReadDAC(DOM_HAL_DAC_MULTIPLE_SPE_THRESH));
    mh.DAC_SINGLE_SPE_THRESH     = moniBEShort(halReadDAC(DOM_HAL_DAC_SINGLE_SPE_THRESH));
    mh.DAC_LED_BRIGHTNESS        = moniBEShort(halReadDAC(DOM_HAL_DAC_LED_BRIGHTNESS));
    mh.DAC_FAST_ADC_REF          = moniBEShort(halReadDAC(DOM_HAL_DAC_FAST_ADC_REF));
    mh.DAC_INTERNAL_PULSER       = moniBEShort(halReadDAC(DOM_HAL_DAC_INTERNAL_PULSER));
    mh.DAC_FE_AMP_LOWER_CLAMP    = moniBEShort(halReadDAC(DOM_HAL_DAC_FE_AMP_LOWER_CLAMP));
    mh.DAC_FL_REF                = moniBEShort(halReadDAC(DOM_HAL_DAC_FL_REF));
    mh.DAC_MUX_BIAS              = moniBEShort(halReadDAC(DOM_HAL_DAC_MUX_BIAS));
    mh.PMT_BASE_HV_SET_VALUE     = moniBEShort(halReadBaseDAC());
    mh.PMT_BASE_HV_MONITOR_VALUE = moniBEShort(domappReadBaseADC());
    mh.DOM_MB_TEMPERATURE        = moniBEShort(temperature);
    mh.SPE_RATE                  = moniBELong(spe_sum);
    mh.MPE_RATE                  = moniBELong(mpe_sum);
  }
  mr.dataLen = sizeof(mh);
  mr.moniEvtType = MONI_TYPE_HDWR_STATE_MSG;
  mr.time = time;
  memcpy(mr.data, (UBYTE *) &mh, sizeof(mh));
  moniInsertRec(&mr);
}


void moniInsertSetDACMessage(unsigned long long time, UBYTE dacID, unsigned short dacVal) {
  struct moniRec mr;
  mr.dataLen = 6;
  mr.moniEvtType = MONI_TYPE_CONF_STATE_CHG_MSG;
  mr.time = time;
  mr.data[0] = DOM_SLOW_CONTROL;
  mr.data[1] = DSC_WRITE_ONE_DAC;
  mr.data[2] = dacID;
  mr.data[3] = 0; /* spare */
  formatShort(dacVal, &(mr.data[4]));
  moniInsertRec(&mr);
}


void moniInsertSetPMT_HV_Message(unsigned long long time, unsigned short hv) {
  struct moniRec mr;
  mr.dataLen = 4;
  mr.moniEvtType = MONI_TYPE_CONF_STATE_CHG_MSG;
  mr.time = time;
  mr.data[0] = DOM_SLOW_CONTROL;
  mr.data[1] = DSC_SET_PMT_HV;
  formatShort(hv, &(mr.data[2]));
  moniInsertRec(&mr);
}


void moniInsertSetPMT_HV_Limit_Message(unsigned long long time, unsigned short limit) {
  struct moniRec mr;
  mr.dataLen = 4;
  mr.moniEvtType = MONI_TYPE_CONF_STATE_CHG_MSG;
  mr.time = time;
  mr.data[0] = DOM_SLOW_CONTROL;
  mr.data[1] = DSC_SET_PMT_HV_LIMIT;
  formatShort(limit, &(mr.data[2]));
  moniInsertRec(&mr);
}


void moniInsertEnablePMT_HV_Message(unsigned long long time) {
  struct moniRec mr;
  mr.dataLen = 2;
  mr.moniEvtType = MONI_TYPE_CONF_STATE_CHG_MSG;
  mr.time = time;
  mr.data[0] = DOM_SLOW_CONTROL;
  mr.data[1] = DSC_ENABLE_PMT_HV;
  moniInsertRec(&mr);
}


void moniInsertDisablePMT_HV_Message(unsigned long long time) {
  struct moniRec mr;
  mr.dataLen = 2;
  mr.moniEvtType = MONI_TYPE_CONF_STATE_CHG_MSG;
  mr.time = time;
  mr.data[0] = DOM_SLOW_CONTROL;
  mr.data[1] = DSC_DISABLE_PMT_HV;
  moniInsertRec(&mr);
}



void moniInsertLCModeChangeMessage(unsigned long long time, UBYTE mode) {
  struct moniRec mr;
  mr.dataLen = 3;
  mr.moniEvtType = MONI_TYPE_CONF_STATE_CHG_MSG;
  mr.time = time;
  mr.data[0] = DOM_SLOW_CONTROL;
  mr.data[1] = DSC_SET_LOCAL_COIN_MODE;
  mr.data[2] = mode;
  moniInsertRec(&mr);
}


void moniInsertLCWindowChangeMessage(unsigned long long time,
                                     ULONG pre_ns, ULONG post_ns) {
  struct moniRec mr;
  mr.dataLen = 2+4*sizeof(ULONG);
  mr.moniEvtType = MONI_TYPE_CONF_STATE_CHG_MSG;
  mr.time = time;
  mr.data[0] = DOM_SLOW_CONTROL;
  mr.data[1] = DSC_SET_LOCAL_COIN_WINDOW;
  formatLong(pre_ns,  &(mr.data[2]));  /* This is a bit ugly, but       */
  formatLong(post_ns, &(mr.data[6]));  /* keeps the old format intact   */
  formatLong(pre_ns,  &(mr.data[10])); /* which does not easily support */
  formatLong(post_ns, &(mr.data[14])); /* new versions                  */

  moniInsertRec(&mr);
}

/* unsigned long long moniGetTimeAsUnsigned(void) {  */
/*   /\* Fix signed-like peculiarity in HAL *\/ */
/*   //return hal_FPGA_DOMAPP_get_local_clock() & 0xFFFFFFFF; */
/*   return hal_FPGA_DOMAPP_get_local_clock(); */
/* } */

void moniPuts(char *s) {
  unsigned long long time;
  time = hal_FPGA_DOMAPP_get_local_clock();
  moniInsertDiagnosticMessage(s, time, strlen(s));
}

void moniRunTests() {
  /* Do not perform other monitoring operations while this
     test is running. */
  unsigned long long time;
#define BSIZ 512
  char buf[BSIZ];

  struct moniRec mr;
  MONI_STATUS ms;
  int NSMALL=5;
  int irec;
  char  *msg = "a0a1a2a3a4a5a6a7a8a9"
    "b0b1b2b3b4b5b6b7b8b9"
    "c0c1c2c3c4c5c6c7c8c9"
    "d0d1d2d3d4d5d6d7d8d9";
  int msglen = 2*10*4;
  int n;

  //n = snprintf(buf, BSIZ, "STARTING SELF TEST");
    
  time = hal_FPGA_DOMAPP_get_local_clock();

  /* Make sure buffer empty */
  ms = moniFetchRec(&mr);
  if(ms != MONI_NODATA) {
    moniPuts("MONI SELF TEST FAILURE(1)");
    moniIsInTrouble = 1;
    return;
  }

  /* Insert and fetch a message */
  moniInsertDiagnosticMessage("MONI1", time, 5);
  ms = moniFetchRec(&mr);
  if(ms != MONI_OK) {
    moniPuts("MONI SELF TEST FAILURE(2)");
    moniIsInTrouble = 1;
    return;
  }
  moniAcceptRec();

  /* Make sure buffer empty */
  ms = moniFetchRec(&mr);
  if(ms != MONI_NODATA) {
    moniPuts("MONI SELF TEST FAILURE(3)");
    moniIsInTrouble = 1;
    return;
  }

  /* Fill small set of records */
  for(irec=0; irec < NSMALL; irec++) {
    time = hal_FPGA_DOMAPP_get_local_clock();
    moniInsertDiagnosticMessage(msg,time,strlen(msg));
  }

  /* try to get it back */
  for(irec=0; irec < NSMALL; irec++) {
    ms = moniFetchRec(&mr);
    moniAcceptRec();

    if(ms != MONI_OK) { 
      moniPuts("MONI SELF TEST FAILURE(4)");
      moniIsInTrouble = 1;
      return;
    }

    if(mr.dataLen != msglen) {
      moniPuts("MONI SELF TEST FAILURE(5)");
      moniIsInTrouble = 1;
      return;
    }

    if(memcmp(mr.data, msg, msglen)) {
      moniPuts("MONI SELF TEST FAILURE(6)");
      moniIsInTrouble = 1;
      return;
    }

  }

  /* Make sure buffer is again empty */
  ms = moniFetchRec(&mr);
  if(ms != MONI_NODATA) {
    moniPuts("MONI SELF TEST FAILURE(7)");
    moniIsInTrouble = 1;
    return;
  }
  moniAcceptRec();

  /* Blow the buffer so that some records are lost */
  for(irec=0; irec < MONI_CIRCBUF_RECS+10; irec++) {
    n = snprintf(buf, BSIZ, "the infinite poetry of monitor records(%d)", irec);
    moniInsertDiagnosticMessage(buf, time, n);
  }

  /* Buffer has been blown now.  
     Current semantics are just to throw old data away.
     Should have exactly MONI_CIRCBUF_RECS-1 records now. */
  for(irec=0; irec < MONI_CIRCBUF_RECS; irec++) {
    ms = moniFetchRec(&mr);
    if(ms != MONI_OK) {
      //moniZeroIndices();
      n = snprintf(buf, BSIZ, "MONI SELF TEST FAILURE(8) - irec = %d", irec);
      moniInsertDiagnosticMessage(buf, time, n);
      moniIsInTrouble = 1;
      return;
    }
    moniAcceptRec();
  }

  /* Next read should be empty */
  ms = moniFetchRec(&mr);
  if(ms != MONI_NODATA) {
    moniPuts("MONI SELF TEST FAILURE(9)");
    moniIsInTrouble = 1;
    return;
  }


  /* Insert and fetch a message, again */
  moniInsertDiagnosticMessage("MONI1", time, 5);
  ms = moniFetchRec(&mr);
  if(ms != MONI_OK) {
    moniPuts("MONI SELF TEST FAILURE(10)");
    moniIsInTrouble = 1;
    return;
  }
  moniAcceptRec();


  /* Make sure buffer empty */
  ms = moniFetchRec(&mr);
  if(ms != MONI_NODATA) {
    moniPuts("MONI SELF TEST FAILURE(11)");
    moniIsInTrouble = 1;
    return;
  }


  /* store and fetch a bunch of times */
  for(irec=0; irec < MONI_CIRCBUF_RECS*2; irec++) {
    time = hal_FPGA_DOMAPP_get_local_clock();
    moniInsertDiagnosticMessage(msg,time,strlen(msg));
    ms = moniFetchRec(&mr);
    if(ms != MONI_OK && ms != MONI_WRAPPED && ms != MONI_OVERFLOW) {
      moniZeroIndices();
      moniPuts("MONI SELF TEST FAILURE(12)");
      moniIsInTrouble = 1;
      return;
    }

    moniAcceptRec();
  }

  // success:
  /* Indicate success */
  moniZeroIndices();
  moniInsertDiagnosticMessage("MONI SELF TEST OK", time, 17);

  /* These all seem to work: 
     moniInsertSetDACMessage(hal_FPGA_DOMAPP_get_local_clock(), 1, 2);
     moniInsertSetPMT_HV_Message(hal_FPGA_DOMAPP_get_local_clock(), 3);
     moniInsertSetPMT_HV_Limit_Message(hal_FPGA_DOMAPP_get_local_clock(), 4);
     moniInsertEnablePMT_HV_Message(hal_FPGA_DOMAPP_get_local_clock());
     moniInsertDisablePMT_HV_Message(hal_FPGA_DOMAPP_get_local_clock());
  */

  return;

}


