/*
    Authors: Chuck McParland, John Jacobsen
    Start Date: May 4, 1999
    Modifications by John Jacobsen 2004 
                  to implement configurable engineering events
    Modifications by Jacobsen 2005 to support production (domapp) FPGA
    Modifications by Jacobsen 2006 to implement delta-compression and many other features
*/

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "hal/DOM_MB_domapp.h"
#include "hal/DOM_MB_hal.h"
#include "hal/DOM_FPGA_domapp_regs.h"

#include "moniDataAccess.h"
#include "commonServices.h"
#include "DOMtypes.h"
#include "DOMdata.h"
#include "DOMstateInfo.h"
#include "message.h"
#include "dataAccessRoutines.h"
#include "DSCmessageAPIstatus.h"
#include "domSControl.h"
#include "expControl.h"

extern USHORT pulser_rate;
extern int pulser_running;
extern UBYTE SNRequestMode;
extern unsigned SNRequestDeadTime;
extern int SNRequested;
extern int numOverflows;
extern unsigned long sw_lbm_mask;

extern UBYTE LCmode; /* From domSControl.c */
extern UBYTE fMoniRateType; /* From dataAccess.c */

/* define size of data buffer in message */
#define DATA_BUFFER_LEN MAXDATA_VALUE
 
/* defines for engineering events */
#define ENG_EVENT_FID 0x2; /* Use format 2, with LC enable bits set */

/* defines for ATWD readout */
#define ATWD_TIMEOUT_COUNT 4000
#define ATWD_TIMEOUT_USEC 5

extern UBYTE FPGA_trigger_mode;
extern int FPGA_ATWD_select;
extern UBYTE DOM_state;
extern int atwdSelect;

int nTrigsReadOut = 0;

// routines used for generating engineering events
UBYTE *ATWDShortMove(USHORT *data, UBYTE *buffer, int count);
UBYTE *FADCMove(USHORT *data, UBYTE *buffer, int count);
UBYTE *TimeMove(UBYTE *buffer, unsigned long long time);

/* Histogramming */
extern unsigned long long     moniHistoIval;
extern unsigned short         histoPrescale;
extern CHARGE_STAMP_MODE_TYPE chargeStampMode;
extern CHARGE_STAMP_SEL_TYPE  chargeStampChanSel;
extern UBYTE                  chargeStampChannel;
extern unsigned short         ATWDchargeStampHistos[2][2][NUM_HIST_BINS];
extern unsigned               ATWDchargeStampEntries[2][2];
extern unsigned short         FADCchargeStampHistos[NUM_HIST_BINS];
extern unsigned               FADCchargeStampEntries;
#define doChargeStampHisto(a) (moniHistoIval > 0) /* Do histo if interval is set */

/** Set by initFormatEngineeringEvent: */
UBYTE ATWDChMask[4];
int ATWDChLen[4];
BOOLEAN ATWDChByte[4];
USHORT *ATWDChData[4];
USHORT *FlashADCData;

UBYTE FlashADCLen;

USHORT Channel0Data[ATWDCHSIZ];
USHORT Channel1Data[ATWDCHSIZ];
USHORT Channel2Data[ATWDCHSIZ];
USHORT Channel3Data[ATWDCHSIZ];
USHORT FADCData[FADCSIZ];

unsigned lbmp;
unsigned hitCounter;

#define TIMELBM
#undef TIMELBM
#ifdef TIMELBM
# define TBEG(b) bench_start(b)
# define TEND(b) bench_end(b)
# define TSHOW(a,b) bench_show(a,b)
static bench_rec_t bformat, breadout, bbuffer, bcompress;
#else
# define TBEG(b)
# define TEND(b)
# define TSHOW(a,b)
#endif

int hvOffCheckFails(void) {
#define MAXHVOFFADC 5
  USHORT hvadc = domappReadBaseADC();
  if(hvadc > MAXHVOFFADC) {
    mprintf("Can't start flasher board run: DOM HV ADC=%hu.", hvadc);
    return 1;
  }
  return 0;
}

int fbSetup(USHORT bright, USHORT window, short delay, USHORT mask, USHORT rate) {
  int err, config_t, valid_t, reset_t;
  err = hal_FB_enable(&config_t, &valid_t, &reset_t, DOM_FPGA_DOMAPP);
  if (err != 0) {
    switch(err) {
    case FB_HAL_ERR_CONFIG_TIME:
      mprintf("Error: flasherboard configuration time too long");
      return 1;
    case FB_HAL_ERR_VALID_TIME:
      mprintf("Error: flasherboard clock validation time too long");
      return 1;
    default:
      mprintf("Error: unknown flasherboard enable failure");
      return 1;
    }
  }
  halSelectAnalogMuxInput(DOM_HAL_MUX_FLASHER_LED_CURRENT);  
  hal_FB_set_brightness((UBYTE) bright);
  hal_FB_set_pulse_width((UBYTE) window);
  hal_FB_enable_LEDs(mask);

  /* Find first LED for MUXer */
  int iled;
  UBYTE firstled=0;
#define N_LEDS 12
  for(iled = 0; iled < N_LEDS; iled++) {
    if((mask >> iled) & 1) {
      firstled = iled;
      break;
    }
  }
  hal_FB_select_mux_input(DOM_FB_MUX_LED_1 + firstled);  
  return 0;
}

inline BOOLEAN FBRunIsInProgress(void) { return DOM_state==DOM_FB_RUN_IN_PROGRESS; }

int changeFBsettings(USHORT bright, USHORT window,
		     short delay, USHORT mask, USHORT rate) {
  /* Change flasher-board settings 'on-the-fly' so FB doesn't have to
     be turned off (danger of large, extended afterbursts) */
  if(!FBRunIsInProgress()) { 
    mprintf("changeFBsettings: Flasher board run not in progress!");
    return 0;
  }

  if(fbSetup(bright, window, delay, mask, rate)) {
    mprintf("changeFBsettings: ERROR: fbSetup failed!");
    return 0;
  }

  hal_FPGA_DOMAPP_cal_atwd_offset(delay);
  double realRate = hal_FPGA_DOMAPP_FB_set_rate((double) rate);
  mprintf("Changed flasher board settings!!! bright=%hu window=%hu delay=%d "
          "mask=%hu rateRequest=%hu realRate=%d",
          bright, window, (int) delay, mask, rate, (int) realRate);
  return 1;
}

int beginFBRun(UBYTE compressionMode, USHORT bright, USHORT window, 
	       short delay, USHORT mask, USHORT rate) {

  if(hvOffCheckFails()) {
    mprintf("beginFBRun: ERROR: hvOffCheckFails!\n");
    return 0;
  }

  if(DOM_state!=DOM_IDLE) {
    mprintf("Can't start flasher board run: DOM_state=%d.", DOM_state);
    return 0;
  }

  halPowerDownBase(); /* Just to be sure, turn off HV */

  if(fbSetup(bright, window, delay, mask, rate)) {
    mprintf("beginFBRun: ERROR: fbSetup failed!");
    return 0;
  }
  hal_FPGA_DOMAPP_cal_atwd_offset(delay);
  double realRate = hal_FPGA_DOMAPP_FB_set_rate((double) rate);

  if(! beginRun(compressionMode, DOM_FB_RUN_IN_PROGRESS)) {
    mprintf("beginFBRun: ERROR: beginRun failed!");
    return 0;
  }
  mprintf("Started flasher board run!!! bright=%hu window=%hu delay=%d "
	  "mask=%hu rateRequest=%hu realRate=%d",
	  bright, window, (int) delay, mask, rate, (int) realRate);
  nTrigsReadOut = 0;
  return 1;
}


void updateTriggerModes(void) {
  if(pulser_running) {
    /* Only SPE or MPE disc modes allowed w/ pulser */
    if(FPGA_trigger_mode == SPE_DISC_TRIG_MODE) {
      hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_SPE);
      hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FE_PULSER);
      mprintf("Setting pulser rate to %d.", pulser_rate);
      hal_FPGA_DOMAPP_cal_pulser_rate(pulser_rate);
    } else if(FPGA_trigger_mode == MPE_DISC_TRIG_MODE) {
      hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_MPE);
      hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FE_PULSER);
      mprintf("Setting pulser rate to %d.", pulser_rate);
      hal_FPGA_DOMAPP_cal_pulser_rate(pulser_rate);
    } else {
      mprintf("WARNING: pulser running but trigger mode (%d) is disallowed. "
	      "Won't allow triggers!", FPGA_trigger_mode);
    }
  } else if(DOM_state == DOM_FB_RUN_IN_PROGRESS) {
    /* For now, only allow triggers on flasher board firing, not on SPE disc. */
    /* Rate is set by beginFBRun */
    hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_FLASHER);
    hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FLASHER);
  } else {
    /* Default modes - no pulser or flasher */
    switch(FPGA_trigger_mode) { // This gets set by a DATA_ACCESS message
    case SPE_DISC_TRIG_MODE:
      hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_SPE
				   | HAL_FPGA_DOMAPP_TRIGGER_FORCED);
      break;
    case MPE_DISC_TRIG_MODE:
      hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_MPE
				   | HAL_FPGA_DOMAPP_TRIGGER_FORCED);
      break;
    case CPU_TRIG_MODE: 
    default:
        hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_FORCED);
	break;
    }
    hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FORCED);
    hal_FPGA_DOMAPP_cal_pulser_rate(pulser_rate);
  }
}

USHORT domappReadBaseADC() { 
  /* When the lines below are uncommented, shut off triggers first because HV read 
     crosstalks into ATWDs (or their inputs) */
  //hal_FPGA_DOMAPP_disable_daq();
  USHORT adc = halReadBaseADC();
  //hal_FPGA_DOMAPP_enable_daq();
  return adc;
}

unsigned long long domappHVSerialRaw(void) {
  /* Ditto as above: Kael seems to think we also may need to disable triggers when reading 
     out the PMT base ID */
  //hal_FPGA_DOMAPP_disable_daq();
  unsigned long long s = halHVSerialRaw();
  //hal_FPGA_DOMAPP_enable_daq();
  return s;
}

int beginRun(UBYTE compressionMode, UBYTE newRunState) {
  nTrigsReadOut = 0;
  numOverflows  = 0;
  hitCounter    = 0;

  if(DOM_state!=DOM_IDLE) {
    mprintf("beginRun: ERROR: DOM not in idle state (%d), DOM_state=%d", DOM_IDLE, DOM_state);
    return FALSE;
  }
  if(newRunState != DOM_RUN_IN_PROGRESS && newRunState != DOM_FB_RUN_IN_PROGRESS) {
    mprintf("beginRun: ERROR: newRunState=%d, must be %d or %d", newRunState, 
	    DOM_RUN_IN_PROGRESS, DOM_FB_RUN_IN_PROGRESS);
    return 0;
  }

  DOM_state=newRunState;
  mprintf("Starting run (type=%d)", newRunState);
  hal_FPGA_DOMAPP_disable_daq();
  hal_FPGA_DOMAPP_lbm_reset();
  halUSleep(10000); /* make sure atwd is done... */
  hal_FPGA_DOMAPP_lbm_reset();
  lbmp = hal_FPGA_DOMAPP_lbm_pointer();

  updateTriggerModes();

  hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_REPEAT);    
  hal_FPGA_DOMAPP_daq_mode(HAL_FPGA_DOMAPP_DAQ_MODE_ATWD_FADC);
  hal_FPGA_DOMAPP_enable_atwds(atwdSelect);
  hal_FPGA_DOMAPP_lbm_mode(HAL_FPGA_DOMAPP_LBM_MODE_WRAP);

  switch(compressionMode) {
  case CMP_NONE:
    hal_FPGA_DOMAPP_atwd_mode(HAL_FPGA_DOMAPP_ATWD_MODE_TESTING);
    hal_FPGA_DOMAPP_compression_mode(HAL_FPGA_DOMAPP_COMPRESSION_MODE_OFF);
    break;
  case CMP_DELTA:
    hal_FPGA_DOMAPP_atwd_mode(HAL_FPGA_DOMAPP_ATWD_MODE_BEACON);
    hal_FPGA_DOMAPP_compression_mode(HAL_FPGA_DOMAPP_COMPRESSION_MODE_ON);
    hal_FPGA_DOMAPP_set_delta_compression_all_avail();
    mprintf("beginRun: COMP_CONTROL=0x%08x DAQ=0x%08x ICETOP_CONTROL=0x%08x", 
	    FPGA(COMP_CONTROL), FPGA(DAQ), FPGA(ICETOP_CONTROL)); 
    break;
  default:
    mprintf("beginRun: ERROR: invalid compression mode given (%d)", (int) compressionMode);
    return FALSE;
  }

  dsc_hal_do_LC_settings(); /* See domSControl.c */
  unsigned masks = 
    HAL_FPGA_DOMAPP_RATE_MONITOR_SPE              |
    HAL_FPGA_DOMAPP_RATE_MONITOR_MPE              |
    HAL_FPGA_DOMAPP_RATE_MONITOR_DEADTIME_ATWD_A  |
    HAL_FPGA_DOMAPP_RATE_MONITOR_DEADTIME_ATWD_B;

  hal_FPGA_DOMAPP_rate_monitor_enable(masks);

  if(SNRequested && doStartSN(SNRequestMode, SNRequestDeadTime)) {
    mprintf("beginRun: ERROR: couldn't start requested supernova datataking.\n");
    return FALSE;
  }

  hal_FPGA_DOMAPP_enable_daq(); /* <-- Can get triggers NOW */
  mprintf("RUN STARTED: DAQ=0x%08x LC_CONTROL=0x%08x", FPGA(DAQ), FPGA(LC_CONTROL));

  return TRUE;
}

void turnOffFlashers(void) {
  hal_FB_set_brightness(0);
  hal_FB_enable_LEDs(0);
  halUSleep(100*1000); /* 100 msec */
  hal_FB_disable();
}

int endFBRun(void) { return endRun(); }

int endRun(void) { /* End either a "regular" or flasher run */
  if(DOM_state != DOM_RUN_IN_PROGRESS && DOM_state != DOM_FB_RUN_IN_PROGRESS) {
    mprintf("Can't stop run, DOM state=%d!", DOM_state);
    return FALSE;
  }
  
  mprintf("Disabling FPGA internal data acquisition... LBM=%u, firmware dbg regi=0x%08x",
	  hal_FPGA_DOMAPP_lbm_pointer(), FPGA(FW_DEBUGGING));
  hal_FPGA_DOMAPP_disable_daq();
  hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_OFF);
  doStopSN();
  turnOffFlashers();
  mprintf("Ended run (run type=%s)", DOM_state==DOM_FB_RUN_IN_PROGRESS?"flasher":"normal");
  DOM_state=DOM_IDLE;
  return TRUE;
}

inline BOOLEAN runIsInProgress(void) { return DOM_state==DOM_RUN_IN_PROGRESS; }

unsigned getLastHitCount(void) {
  unsigned h = hitCounter;
  hitCounter = 0;
  return h;
}

unsigned getLastHitCountNoReset(void) { return hitCounter; }

void initFillMsgWithData(void) { }

int isDataAvailable() {
  return ( (hal_FPGA_DOMAPP_lbm_pointer()-lbmp) & ~FPGA_DOMAPP_LBM_BLOCKMASK );
}

/* Access lookback event at idx, but changed to byte sense */
inline unsigned char *lbmEvent(unsigned idx) {
   return
     ((unsigned char *) hal_FPGA_DOMAPP_lbm_address()) + (idx & WHOLE_LBM_MASK);
}

inline unsigned nextEvent(unsigned idx) {
  return idx + HAL_FPGA_DOMAPP_LBM_EVENT_SIZE;
}

inline static unsigned nextValidBlock(unsigned ptr) {
  return ((ptr) & ~FPGA_DOMAPP_LBM_BLOCKMASK);
}

static int haveOverflow(unsigned lbmp) {
  //if( (hal_FPGA_DOMAPP_lbm_pointer()-lbmp) > WHOLE_LBM_MASK ) {
  if( (hal_FPGA_DOMAPP_lbm_pointer()-lbmp) > sw_lbm_mask) {
    mprintf("LBM OVERFLOW!!! hal_FPGA_DOMAPP_lbm_pointer=0x%08lx lbmp=0x%08lx",
	    hal_FPGA_DOMAPP_lbm_pointer(), lbmp);
    numOverflows++;
    return 1;
  }
  return 0;
}

inline int engEventSize(void) { return 1550; } // Make this smarter

inline int deltaEventSize(void) { return engEventSize(); } // Make this MUCH smarter

void mDumpEngHeader(struct tstevt * hdr) {
    mprintf("tlo=0x%04x lbmp=0x%08lx hal_lbm=0x%08lx "
	    "thi=0x%08lx trig=0x%08lx tdead=0x%04x",
	    hdr->tlo, lbmp, hal_FPGA_DOMAPP_lbm_pointer(), 
	    hdr->thi, hdr->trigbits, hdr->tdead);
}
  
int formatDomappEngEvent(UBYTE * msgp, unsigned lbmp) {
  unsigned char * e = lbmEvent(lbmp);
  unsigned char * m0 = msgp;
  struct tstevt * hdr = (struct tstevt *) e;
  unsigned long long stamp = hdr->tlo | (((unsigned long long) hdr->thi) << 16);

  /*********************************************************************************/
  /* mprintf("stamp 0x%04lx%08lx  tlo=0x%04x lbmp=0x%08lx hal_lbm=0x%08lx "	   */
  /* 	  "thi=0x%08lx trig=0x%08lx tdead=0x%04x",				   */
  /* 	  (unsigned long) ((stamp>>32)&0xFFFFFFFF), 				   */
  /* 	  (unsigned long) (stamp&0xFFFFFFFF),					   */
  /* 	  hdr->tlo, lbmp, hal_FPGA_DOMAPP_lbm_pointer(), 			   */
  /* 	  hdr->thi, hdr->trigbits, hdr->tdead);					   */
  /*********************************************************************************/
  
  
  /***************************************************************************************/
  /* Only HAL_FPGA_DOMAPP_ATWD_MODE_TESTING is allowed when formatDomappEngEvent 	 */
  /* is called.  So atwdsize must be 3.  This was seen not to be the case early		 */
  /* in testing, so we check for it every time.						 */
  /***************************************************************************************/
  
  unsigned atwdsize = (hdr->trigbits>>19)&0x3;
  if(atwdsize != 3) {
    mprintf("WARNING: formatDomappEngEvent, trigger bits indicate ATWD size(%u) != 3... ", atwdsize);
    mDumpEngHeader(hdr);
  }

  // Skip over length
  msgp += 2;
  *msgp++ = 0x0;
  *msgp++ = ENG_EVENT_FID; // Event ID
  *msgp++ = (hdr->trigbits >> 16) & 0x01; // "MiscBits"
  *msgp++ = FlashADCLen;
  *msgp++ = (ATWDChMask[1]<<4) | ATWDChMask[0];
  *msgp++ = (ATWDChMask[3]<<4) | ATWDChMask[2];

  int source = hdr->trigbits & 0xFF;
  UBYTE trigmask;

  // This is a bit hacked, but the event format only allows one trigger type at a time
  int hadWarning = 0;
  if(source == HAL_FPGA_DOMAPP_TRIGGER_FORCED) { /* Flasher or Disc. take precedence */
    trigmask = 1;
  } else if(source & HAL_FPGA_DOMAPP_TRIGGER_FLASHER) {
    trigmask = 3;
  } else if(source & (HAL_FPGA_DOMAPP_TRIGGER_SPE | HAL_FPGA_DOMAPP_TRIGGER_MPE)) {
    trigmask = 2;
  } else {
    if(!hadWarning) {
      hadWarning++;
      mprintf("WARNING: formatDomappEngEvent: Disallowed source bits from trigger "
	      "info in event (source=0x%04x)", source);
      mDumpEngHeader(hdr);
    }
    trigmask = TRIG_UNKNOWN_MODE;
  }
  if(hdr->trigbits & 1<<24) trigmask |= TRIG_LC_LOWER_ENA;
  if(hdr->trigbits & 1<<25) trigmask |= TRIG_LC_UPPER_ENA;

  if(FBRunIsInProgress()) trigmask |= TRIG_FB_RUN;
  *msgp++ = trigmask;
  *msgp++ = 0;               // Spare
  msgp = TimeMove(msgp, stamp); // Timestamp

  //mprintf("Got timestamp 0x%04lx%08lx",  
  //        (unsigned long) ((stamp>>32)&0xFFFFFFFF), (unsigned long) (stamp&0xFFFFFFFF));

  if(hdr->trigbits & 1<<17) {
    msgp = FADCMove((USHORT *) (e+0x10), msgp, (int) FlashADCLen);
  } else {
    mprintf("WARNING: FADC data MISSING from raw event!!!");
    mDumpEngHeader(hdr);
  }

  if(hdr->trigbits & 1<<18) {
    int ich;
    for(ich = 0; ich < 4; ich++) {
      msgp = ATWDShortMove((USHORT *) (e+0x210+ich*0x100) , msgp, ATWDChLen[ich]);
    }
  } else {
    mprintf("WARNING: ATWD data MISSING from raw event!!!");
    mDumpEngHeader(hdr);
  }

  int nbytes = msgp-m0;

  formatShort(nbytes, m0);
  return nbytes;
}

struct deltaHit {
  unsigned long word0;
  unsigned long word1;
};

inline unsigned short getDeltaHitTMSB(unsigned lbmp) {
  return ((struct deltaHit *) lbmEvent(lbmp))->word0 & 0xFFFF;
}

inline unsigned short getDeltaHitSize(unsigned lbmp) {
  return ((struct deltaHit *) lbmEvent(lbmp))->word1 & 0x07FF; /* 11 bit size word */
}

inline unsigned char * getDeltaHitStart(unsigned lbmp) {
  return ((unsigned char *) lbmEvent(lbmp)) + 4; /* Skip WORD0 domapp header word */
}

void histoChargeStamp(unsigned char * hitbuf) {
  if(chargeStampMode == CHARGE_STAMP_FADC && histoPrescale > 0) { /* FADC mode */
    unsigned word3 = ((unsigned *) (hitbuf+8))[0];
    //mprintf("histoChargeStamp: word3=0x%08x", word3);
    unsigned isHi = (word3 & 0x800000)>>31;
    //mprintf("histoChargeStamp: isHi: %u", isHi);
    unsigned peakCount = (word3 >> 9) & 0x1FF;
    //mprintf("histoChargeStamp: raw peakCount: %u", peakCount);
    if(isHi) peakCount *= 2;
    //mprintf("histoChargeStamp: adjusted peakCount: %u", peakCount);
    unsigned bin = peakCount / histoPrescale;
    if(bin > (NUM_HIST_BINS-1)) bin = NUM_HIST_BINS-1;
    //mprintf("histoChargeStamp: bin=%u", bin);
    FADCchargeStampHistos[bin] ++;
    FADCchargeStampEntries++;
  } else if(chargeStampMode == CHARGE_STAMP_ATWD) {   /* ATWD/IceTop mode */
    unsigned word1  = ((unsigned *) hitbuf)[0];
    unsigned word3  = ((unsigned *) (hitbuf+8))[0];
    unsigned charge = word3 & 0x00001FFFF;
    unsigned bin    = charge / histoPrescale;
    if(bin > (NUM_HIST_BINS-1)) bin = NUM_HIST_BINS-1;
    int chan        = (int) ((word3 >> 17) & 0x3);
    int aorb        = (word1 >> 11) & 0x1;
    if(chan > 1) { /* Deal w/ overflows per Paul Sullivan */
      chan = 1;
      bin  = NUM_HIST_BINS-1;
    }
    ATWDchargeStampHistos[aorb][chan][bin] ++;
    ATWDchargeStampEntries[aorb][chan]++;
  } else {
    /* Do nothing if mode is wrong (can't happen due to check in domSControl.c) */
  }
}

static int formatDomappDeltaEvent(UBYTE * msgp, unsigned lbmp, unsigned short tmsb, 
				  int doHdr, int* isHeaderOnly) {
  unsigned char * m0 = msgp;

  if(doHdr) {
    msgp += 2;                 /* Skip length, fill in later */
    *msgp++ = 0;               /* spare */
    *msgp++ = 0x90;            /* delta compression header byte */

    formatShort(tmsb, msgp);
    msgp += 2;

    *msgp++ = 0;               /* spare */
    *msgp++ = 0;               /* spare */
  }

  unsigned short hitsize = getDeltaHitSize(lbmp);
  *isHeaderOnly = (hitsize == 12);
  memcpy(msgp, getDeltaHitStart(lbmp), hitsize);
  msgp += hitsize;
  if(doChargeStampHisto()) histoChargeStamp(getDeltaHitStart(lbmp));
  return msgp-m0;
}

int fillMsgWithDeltaData(UBYTE *msgBuffer, int bufsiz) {
  /* Fill until MSBs of time stamp change; add header if needed */
  UBYTE *p     = msgBuffer;
# define NCUR() ((int) (p - msgBuffer))

  int doHdr = 1;
  unsigned short lastMsb = 0;
  int ret = 0;
  int isHeaderOnly = 0;
  while(1) {
    if(!isDataAvailable()) {
      ret = NCUR();
      break;
    }

    if(haveOverflow(lbmp)) break; /* Break here to keep hitCounter, etc. correct */

    /* Stop if not enough room for hit - if data is corrupt due 
       to LBM overflow, the whole thing gets tossed below. */
    unsigned short hsiz = getDeltaHitSize(lbmp);
    if(bufsiz - NCUR() < hsiz) {
      ret = NCUR();
      break;
    }
    unsigned short tmsb = getDeltaHitTMSB(lbmp);
    if(doHdr) lastMsb = tmsb;
    if(lastMsb != tmsb) {
      ret = NCUR();
      break;
    }
    p += formatDomappDeltaEvent(p, lbmp, tmsb, doHdr, &isHeaderOnly);
    doHdr = 0;
    lbmp = nextEvent(lbmp);
    nTrigsReadOut++;
    /* Count the hit only if we're recording all SLC, or if we're recording
       HLC and the hit is not an SLC hit */
    if(fMoniRateType == F_MONI_RATE_SLC || !isHeaderOnly) hitCounter++;
  }
  /* If overflow has occurred, we can't trust the data for this cycle */
  if(haveOverflow(lbmp)) {
    lbmp = nextValidBlock(hal_FPGA_DOMAPP_lbm_pointer());
    return 0;
  } 
  formatShort(NCUR(), msgBuffer);
  return ret;
}

extern unsigned long hal_get_interrupt_enables(void);

int checkTime(unsigned long long t, unsigned long long tblock,
	      unsigned long long * tprev, unsigned dt) {
  static int virgin = 1;
  if(virgin) { virgin = 0; *tprev = t; return 0; }
  int err = 0;
  char * why = "";
  if(((long long) t) - ((long long) *tprev) > dt) { /* Gap in time */
    why = "SN time gap";
    err = 1;
  } else if(((long long) t) - ((long long) *tprev) < dt) { /* Time too short or out of order */
    why = "SN time out of order: ";
    err = 1;
  }
  if(err) {
    mprintf("%s: t=%lu tprev=%lu tblock=%lu dt=%ld dtblock=%ld irqEnable=%lx", why,
	    (unsigned long) t, (unsigned long) *tprev, (unsigned long) tblock, 
	    (long) (t-*tprev), (long) (t-tblock), FPGA(INT_EN));
  }
  *tprev = t;
  return err;
}

int fillMsgWithSNData(UBYTE *msgBuffer, int bufsiz) {
  UBYTE *p  = msgBuffer;
  UBYTE *p0 = msgBuffer;
  static SNEvent sn;
  static SNEvent * psn = &sn;
  static int saved_bin = 0;
  static unsigned long long tprev = 0;
  p += 2; // Skip length portion, fill later
  formatShort(300, p); // Add format ID
  p += 2;

# define NCUR() ((int) (p - msgBuffer))
# define STD_DT 65536
# define bytelimit(counts) ((unsigned char) ((counts) > 255 ? 255 : (counts)))

  if(!saved_bin) {
    if(!hal_FPGA_DOMAPP_sn_ready()) {
      return 0;
    }
    if(hal_FPGA_DOMAPP_sn_event(psn)) {
      mprintf("fillMsgWithSNData: WARNING: hal_FPGA_DOMAPP_sn_event failed (HAL overflow)!");
    }
  }

  /* By now, psn is either our saved_bin or a new event */
  unsigned long long t0 = psn->ticks;
  if(saved_bin) {
    saved_bin = 0;
  } else {
    checkTime(t0, t0, &tprev, STD_DT);
  }
  p    = TimeMove(p, t0);
  *p++ = bytelimit(psn->counts);

  while(1) {
    if(!hal_FPGA_DOMAPP_sn_ready()) break;
    if(NCUR()+1 > bufsiz) break;

    /* Get next event; may have to save it for later if delta-t != STD_DT */
    if(hal_FPGA_DOMAPP_sn_event(psn)) {
      mprintf("fillMsgWithSNData: WARNING: hal_FPGA_DOMAPP_sn_event failed (HAL overflow)!");
    }
    if(checkTime(psn->ticks, t0, &tprev, STD_DT)) {
      saved_bin = 1;
      break;
    }      
    *p++ = bytelimit(psn->counts);
  }

  formatShort(NCUR(), p0);
  return NCUR();
}

int fillMsgWithEngData(UBYTE *msgBuffer, int bufsiz) {
  UBYTE *p     = msgBuffer;
# define NCUR() ((int) (p - msgBuffer))
  while(1) {
    if(!isDataAvailable()) return NCUR();
    if(bufsiz - NCUR() < engEventSize()) return NCUR(); 
    if(haveOverflow(lbmp)) {
      lbmp = nextValidBlock(hal_FPGA_DOMAPP_lbm_pointer());
      //mprintf("Reset LBM pointer, hal_FPGA_DOMAPP_lbm_pointer=0x%08lx lbmp=0x%08lx",
      //        hal_FPGA_DOMAPP_lbm_pointer(), lbmp);
      return 0; // Data is compromised -- kill it
    }
    
    // if we get here, we have room for the formatted engineering event
    // and one or more hits are available
    p += formatDomappEngEvent(p, lbmp);  // Fill data into user's message buffer,
					 //   advance both message pointer...
    lbmp = nextEvent(lbmp);              //          ... and LBM pointer
    nTrigsReadOut++;
    hitCounter++;
  }
}


int fillMsgWithData(UBYTE *msgBuffer, int bufsiz, UBYTE format, UBYTE compression) {
  if(format == FMT_ENG && compression == CMP_NONE) 
    return fillMsgWithEngData(msgBuffer, bufsiz);
  if(format == FMT_DELTA && compression == CMP_DELTA)
    return fillMsgWithDeltaData(msgBuffer, bufsiz);
  mprintf("dataAccess: fillMsgWithData: WARNING: invalid format/compression combo!  "
	  "format=%d compression=%d", (int) format, (int) compression);
  return 0;
}

UBYTE *ATWDByteMoveFromBottom(USHORT *data, UBYTE *buffer, int count) {
  /** Old version of ATWDByteMove, which assumes waveform is sorted from early
      time to later */
    int i;

    for(i = 0; i < count; i++) {
	*buffer++ = (UBYTE)(*data++ & 0xff);
    }
    return buffer;
}

UBYTE *ATWDShortMoveFromBottom(USHORT *data, UBYTE *buffer, int count) {
  /** Old version of ATWDShortMove, which assumes waveform is sorted from early
      time to later */
    int i;
    UBYTE *ptr = (UBYTE *)data;

    for(i = 0; i < count; i++) {
	*buffer++ = *(ptr+1);
	*buffer++ = *ptr;
	ptr += 2;
    }
    return buffer;
}


UBYTE *ATWDByteMove(USHORT *data, UBYTE *buffer, int count) {
  /** Assumes earliest samples are last in the array */
    int i;

    data += ATWDCHSIZ-count;

    if(count > ATWDCHSIZ || count <= 0) return buffer;
    for(i = ATWDCHSIZ-count; i < ATWDCHSIZ; i++) {
	*buffer++ = (UBYTE)(*data++ & 0xff);
    }
    return buffer;
}


UBYTE *ATWDShortMove(USHORT *data, UBYTE *buffer, int count) {
  /** Assumes earliest samples are last in the array */
    int i;

    UBYTE *ptr = (UBYTE *)data;

    ptr += (ATWDCHSIZ-count)*2;

    if(count > ATWDCHSIZ || count <= 0) return buffer; 
    for(i = ATWDCHSIZ - count; i < ATWDCHSIZ; i++) {
      *buffer++ = (*(ptr+1))&0x03;  /* Keep a total */
      *buffer++ = *ptr;             /*  of 10 bits  */
      ptr += 2;
    }
    return buffer;
}


UBYTE *FADCMove(USHORT *data, UBYTE *buffer, int count) {
    int i;
    UBYTE *ptr = (UBYTE *)data;

    for(i = 0; i < count; i++) {
      *buffer++ = (*(ptr+1))&0x03;  /* Keep a total */
      *buffer++ = *ptr;             /*  of 10 bits  */
      ptr+=2;
    }
    return buffer;
}

UBYTE *TimeMove(UBYTE *buffer, unsigned long long time) {
    int i;
    union DOMtime {unsigned long long time;
	UBYTE timeBytes[8];};
    union DOMtime t;

    t.time = time;

    for(i = 0; i < 6; i++) {
      *buffer++ = t.timeBytes[5-i];
    }

    return buffer;
}

void initFormatEngineeringEvent(UBYTE fadc_samp_cnt_arg, 
				UBYTE atwd01_mask_arg,
				UBYTE atwd23_mask_arg) {
  /* NEEDED for variable engineering events */
  int ich;
#define BSIZ 1024
  /* Endianness set to agree w/ formatEngineeringEvent */
  ATWDChMask[0] = atwd01_mask_arg & 0xF;
  ATWDChMask[1] = (atwd01_mask_arg >> 4) & 0xF;
  ATWDChMask[2] = atwd23_mask_arg & 0xF;
  ATWDChMask[3] = (atwd23_mask_arg >> 4) & 0xF;

  for(ich = 0; ich < 4; ich++) {
    switch(ATWDChMask[ich]) {
    case 1: //0b0001:
      ATWDChLen[ich]  = 32;
      ATWDChByte[ich] = TRUE;
      break;
    case 5: //0b0101:
      ATWDChLen[ich]  = 64;
      ATWDChByte[ich] = TRUE;
      break;
    case 9: //0b1001:
      ATWDChLen[ich]  = 16;
      ATWDChByte[ich] = TRUE;
      break;
    case 13: //0b1101:
      ATWDChLen[ich]  = 128;
      ATWDChByte[ich] = TRUE;
      break;
    case 3: //0b0011:
      ATWDChLen[ich]  = 32;
      ATWDChByte[ich] = FALSE;
      break;
    case 7: //0b0111:
      ATWDChLen[ich]  = 64;
      ATWDChByte[ich] = FALSE;
      break;
    case 11: //0b1011:
      ATWDChLen[ich]  = 16;
      ATWDChByte[ich] = FALSE;
      break;
    case 15: //0b1111:
      ATWDChLen[ich]  = 128;
      ATWDChByte[ich] = FALSE;
      break;
    default:
      ATWDChLen[ich]  = 0;
      ATWDChByte[ich] = TRUE;
      break;
    }
  }

  FlashADCLen = fadc_samp_cnt_arg;
  
  ATWDChData[0] = Channel0Data;
  ATWDChData[1] = Channel1Data;
  ATWDChData[2] = Channel2Data;
  ATWDChData[3] = Channel3Data;
  FlashADCData = FADCData;
}

int data_access_unit_tests(void) {
  return 1;
}

