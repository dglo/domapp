/*
    Authors: Chuck McParland, John Jacobsen
    Start Date: May 4, 1999
    Modifications by John Jacobsen 2004 
                  to implement configurable engineering events
    Modifications by Jacobsen 2005 to support production (domapp) FPGA
*/

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "hal/DOM_MB_domapp.h"
#include "hal/DOM_MB_hal.h"

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

/* Externally available pedestal waveforms */
extern unsigned short atwdpedavg[2][4][128];
extern unsigned short fadcpedavg[256];
extern USHORT atwdThreshold[2][4], fadcThreshold;
extern USHORT pulser_rate;
extern int pulser_running;

/* LC mode, defined via slow control */
extern UBYTE LCmode;

/* define size of data buffer in message */
#define DATA_BUFFER_LEN MAXDATA_VALUE
 
/* defines for engineering events */
#define ENG_EVENT_FID 0x2; /* Use format 2, with LC enable bits set */

/* defines for ATWD readout */
#define ATWD_TIMEOUT_COUNT 4000
#define ATWD_TIMEOUT_USEC 5

/* global storage and functions */
extern UBYTE FPGA_trigger_mode;
extern int FPGA_ATWD_select;

/* local functions, data */
extern UBYTE DOM_state;
extern UBYTE DOM_config_access;
extern UBYTE DOM_status;
extern UBYTE DOM_cmdSource;
extern ULONG DOM_constraints;
extern char *DOM_errorString;
extern USHORT atwdpedavg[2][4][ATWDCHSIZ];
extern USHORT fadcpedavg[FADCSIZ];
extern int SW_compression;
extern int SW_compression_fmt;

int nTrigsReadOut = 0;

// routines used for generating engineering events
UBYTE *ATWDShortMove(USHORT *data, UBYTE *buffer, int count);
UBYTE *FADCMove(USHORT *data, UBYTE *buffer, int count);
UBYTE *TimeMove(UBYTE *buffer, unsigned long long time);

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
  DOM_state = DOM_FB_RUN_IN_PROGRESS;

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
  mprintf("Started flasher board run!!! bright=%hu window=%hu delay=%h "
	  "mask=%hu rateRequest=%hu",
	  bright, window, delay, mask, rate, realRate);
  nTrigsReadOut = 0;
  return 1;
}

inline BOOLEAN FBRunIsInProgress(void) { return DOM_state==DOM_FB_RUN_IN_PROGRESS; }

void setSPETrigMode(void) {
  /* Allow for SPE data to be acquired, or periodic forced triggers */
  hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_SPE
			       | HAL_FPGA_DOMAPP_TRIGGER_FORCED);
  hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FORCED);
}

void setSPEPulserTrigMode(void) {
  /* Disallow periodic forced triggers.  Collect SPEs simulated by
     on-board front-end pulser */
  hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_SPE);
  hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FE_PULSER);
}

void setFBTrigMode(void) {
  hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_FLASHER);
  hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FLASHER);
}

void setPeriodicForcedTrigMode(void) {
  hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_FORCED);
  hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FORCED);
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

  if(DOM_state!=DOM_IDLE) {
    mprintf("beginRun: ERROR: DOM not in idle state, DOM_state=%d", DOM_IDLE);
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

  if(pulser_running) {
    pulser_running = 0;
    if(FPGA_trigger_mode == TEST_DISC_TRIG_MODE) {
      setSPEPulserTrigMode();
    } else {
      mprintf("WARNING: pulser running but trigger mode (%d) is disallowed. "
	      "Won't allow triggers!", FPGA_trigger_mode);
    }
    mprintf("Setting pulser rate to %d.", pulser_rate);
    hal_FPGA_DOMAPP_cal_pulser_rate(pulser_rate);

  } else if(newRunState == DOM_FB_RUN_IN_PROGRESS) {
    /* For now, only allow triggers on flasher board firing, not on SPE disc. */
    setFBTrigMode();
    /* Rate is set by beginFBRun */
  } else {
    /* Default mode */
    switch(FPGA_trigger_mode) { // This gets set by a DATA_ACCESS message
    case TEST_DISC_TRIG_MODE: setSPETrigMode();  break; // can change when pulser starts
    case CPU_TRIG_MODE: 
    default:                  setPeriodicForcedTrigMode(); break;
    }
    hal_FPGA_DOMAPP_cal_pulser_rate(pulser_rate);
  }
  
  //hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_FORCED);

  hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_REPEAT);    
  hal_FPGA_DOMAPP_daq_mode(HAL_FPGA_DOMAPP_DAQ_MODE_ATWD_FADC);
  hal_FPGA_DOMAPP_atwd_mode(HAL_FPGA_DOMAPP_ATWD_MODE_TESTING);
  hal_FPGA_DOMAPP_enable_atwds(HAL_FPGA_DOMAPP_ATWD_A|HAL_FPGA_DOMAPP_ATWD_B);
  hal_FPGA_DOMAPP_lbm_mode(HAL_FPGA_DOMAPP_LBM_MODE_WRAP);
  //hal_FPGA_DOMAPP_lbm_mode(HAL_FPGA_DOMAPP_LBM_MODE_STOP);
  if(compressionMode == CMP_NONE) {
    hal_FPGA_DOMAPP_compression_mode(HAL_FPGA_DOMAPP_COMPRESSION_MODE_OFF);
  } else if(compressionMode == CMP_RG) {
    hal_FPGA_DOMAPP_compression_mode(HAL_FPGA_DOMAPP_COMPRESSION_MODE_ON);
  } else {
    mprintf("beginRun: ERROR: invalid compression mode given (%d)", (int) compressionMode);
    return FALSE;
  }

  dsc_hal_do_LC_settings(); /* See domSControl.c */

  hal_FPGA_DOMAPP_rate_monitor_enable(HAL_FPGA_DOMAPP_RATE_MONITOR_SPE|
  				      HAL_FPGA_DOMAPP_RATE_MONITOR_MPE);

  hal_FPGA_DOMAPP_enable_daq(); /* <-- Can get triggers NOW */
  return TRUE;
}

int endFBRun(void) { return endRun(); }

int endRun(void) { /* End either a "regular" or flasher run */
  if(DOM_state != DOM_RUN_IN_PROGRESS && DOM_state != DOM_FB_RUN_IN_PROGRESS) {
    mprintf("Can't stop run, DOM state=%d!", DOM_state);
    return FALSE;
  }
  
  mprintf("Disabling FPGA internal data acquisition... LBM=%u",
	  hal_FPGA_DOMAPP_lbm_pointer());
  hal_FPGA_DOMAPP_disable_daq();
  hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_OFF);
  hal_FB_set_brightness(0);
  hal_FB_disable();
  mprintf("Ended run (run type=%s)", DOM_state==DOM_FB_RUN_IN_PROGRESS?"flasher":"normal");
  DOM_state=DOM_IDLE;
  return TRUE;
}

inline BOOLEAN runIsInProgress(void) { return DOM_state==DOM_RUN_IN_PROGRESS; }

void initFillMsgWithData(void) { }

int isDataAvailable() {
  return ( (hal_FPGA_DOMAPP_lbm_pointer()-lbmp) & ~FPGA_DOMAPP_LBM_BLOCKMASK );
}

/* Stolen from Arthur: access lookback event at idx, but changed to byte sense */
unsigned char *lbmEvent(unsigned idx) {
   return
     ((unsigned char *) hal_FPGA_DOMAPP_lbm_address()) + (idx & WHOLE_LBM_MASK);
}

/* Stolen from Arthur: but changed EVENT_LEN to EVENT_SIZE */
unsigned nextEvent(unsigned idx) {
   return idx + HAL_FPGA_DOMAPP_LBM_EVENT_SIZE;
}

static unsigned nextValidBlock(unsigned ptr) {
  return ((ptr) & ~FPGA_DOMAPP_LBM_BLOCKMASK);
}

static int haveOverflow(unsigned lbmp) {
  if( (hal_FPGA_DOMAPP_lbm_pointer()-lbmp) > WHOLE_LBM_MASK ) {
    mprintf("LBM OVERFLOW!!! hal_FPGA_DOMAPP_lbm_pointer=0x%08lx lbmp=0x%08lx",
	    hal_FPGA_DOMAPP_lbm_pointer(), lbmp);
    return 1;
  }
  return 0;
}

int engEventSize(void) { return 1550; } // Make this smarter

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
    mprintf("WARNING: formatDomappEngEvent, trigger bits indicate ATWD size(%u) != 3... "
	    "stamp 0x%04lx%08lx  tlo=0x%04x lbmp=0x%08lx hal_lbm=0x%08lx "
	    "thi=0x%08lx trig=0x%08lx tdead=0x%04x",
	    atwdsize, (unsigned long) ((stamp>>32)&0xFFFFFFFF), 
	    (unsigned long) (stamp&0xFFFFFFFF), hdr->tlo, lbmp, 
	    hal_FPGA_DOMAPP_lbm_pointer(), hdr->thi, hdr->trigbits, hdr->tdead);
  }

  // Skip over length
  msgp += 2;
  *msgp++ = 0x0;
  *msgp++ = ENG_EVENT_FID; // Event ID
  *msgp++ = (hdr->trigbits >> 16) & 0x01; // "MiscBits"
  *msgp++ = FlashADCLen;
  *msgp++ = (ATWDChMask[1]<<4) | ATWDChMask[0];
  *msgp++ = (ATWDChMask[3]<<4) | ATWDChMask[2];

  int source = hdr->trigbits & 0xFFFF;
  UBYTE trigmask;

  // This is a bit hacked, but the event format only allows one trigger type at a time
  int hadWarning = 0;
  if(source == HAL_FPGA_DOMAPP_TRIGGER_FORCED) { 
    trigmask = 1;
  } else if(source == HAL_FPGA_DOMAPP_TRIGGER_SPE || 
	    source == HAL_FPGA_DOMAPP_TRIGGER_MPE ||
	    source == (HAL_FPGA_DOMAPP_TRIGGER_SPE|HAL_FPGA_DOMAPP_TRIGGER_MPE)) {
    trigmask = 2;
  } else if(source == HAL_FPGA_DOMAPP_TRIGGER_FLASHER) {
    trigmask = 3;
  } else {
    if(!hadWarning) {
      hadWarning++;
      mprintf("formatDomappEngEvent: Disallowed source bits from trigger info in event (source=0x%04x)",
	      source);
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
  }

  if(hdr->trigbits & 1<<18) {
    int ich;
    for(ich = 0; ich < 4; ich++) {
      msgp = ATWDShortMove((USHORT *) (e+0x210+ich*0x100) , msgp, ATWDChLen[ich]);
    }
  } else {
    mprintf("WARNING: ATWD data MISSING from raw event!!!");
  }

  int nbytes = msgp-m0;

  formatShort(nbytes, m0);
  return nbytes;
}

struct rgevt { unsigned long word0, word1, word2, word3; };

int nextRGEventSize(unsigned mylbmp) {
  struct rgevt * hdr = (struct rgevt *) lbmEvent(mylbmp);
  return hdr->word1 & 0x7FF;
}

unsigned char * RGHitBegin(unsigned mylbmp) {
  struct rgevt * hdr = (struct rgevt *) lbmEvent(mylbmp);
  return (unsigned char *) &(hdr->word1);
}

int formatRGEvent(UBYTE * msgp, unsigned mylbmp) {
  int nbytes = nextRGEventSize(mylbmp);
  memcpy(msgp, RGHitBegin(mylbmp), nbytes);
  return nbytes;
}

int isCompressBitSet(unsigned mylbmp) {
  struct rgevt * hdr = (struct rgevt *) lbmEvent(mylbmp);
  return (hdr->word0 >> 31) & 0x01;
}

unsigned short RGTimeMSB16(unsigned mylbmp) {
  struct rgevt * hdr = (struct rgevt *) lbmEvent(mylbmp);
  return hdr->word0 & 0xFFFF;
}

void mShowHdr(unsigned mylbmp) {
  struct rgevt * hdr = (struct rgevt *) lbmEvent(mylbmp);
  mprintf("lbmp=%u C=%d siz=%d TS16=0x%04x w0=0x%08lx w1=0x%08lx w2=0x%08lx w3=0x%08lx",
	  mylbmp, isCompressBitSet(mylbmp), nextRGEventSize(mylbmp), RGTimeMSB16(mylbmp),
	  hdr->word0, hdr->word1, hdr->word2, hdr->word3);
}

#ifdef SNSIM
#  warning supernova implemented via SIMULATION (hal not ready)
#  define sn_ready sim_hal_FPGA_DOMAPP_sn_ready
#  define sn_event sim_hal_FPGA_DOMAPP_sn_event

int sim_hal_FPGA_DOMAPP_sn_ready(void) { return 1; }
int sim_hal_FPGA_DOMAPP_sn_event(SNEvent *evt) {
  static unsigned char icount = 0;
  evt->ticks  = hal_FPGA_DOMAPP_get_local_clock();
  evt->counts = icount++;
  return 0;
}

#else
#  define sn_ready hal_FPGA_DOMAPP_sn_ready
#  define sn_event hal_FPGA_DOMAPP_sn_event
#endif

int fillMsgWithSNData(UBYTE *msgBuffer, int bufsiz) {
  UBYTE *p  = msgBuffer;
  UBYTE *p0 = msgBuffer;
  static SNEvent sn;
  static SNEvent * psn = &sn;
  static int saved_bin = 0;

  p += 2; // Skip length portion, fill later

# define NCUR() ((int) (p - msgBuffer))
# define STD_DT 65536
# define bytelimit(counts) ((unsigned char) ((counts) > 255 ? 255 : (counts)))

  if(saved_bin) {
    saved_bin = 0;
  } else {
    doPong(28);
    if(!sn_ready()) {
      doPong(29);
      return 0;
    }
    doPong(30);
    if(sn_event(psn)) {
      doPong(31);
      mprintf("fillMsgWithSNData: WARNING: hal_FPGA_DOMAPP_sn_event failed!");
      return 0;
    }
    doPong(32);
  }

  /* By now, psn is either our saved_bin or a new event */
  unsigned long long t0 = psn->ticks;
  p    = TimeMove(p, t0);
  *p++ = bytelimit(psn->counts);

  while(1) {
    if(!sn_ready()) break;
    if(NCUR()+1 > bufsiz) break;

    /* Get next event; may have to save it for later if delta-t != STD_DT */
    if(sn_event(psn)) {
      mprintf("fillMsgWithSNData: WARNING: hal_FPGA_DOMAPP_sn_event failed!");
      break;
    }
    if((psn->ticks - t0) != STD_DT) {
      saved_bin = 1;
      break;
    }
    t0 = psn->ticks;
    *p++ = bytelimit(psn->counts);
  }

  formatShort(NCUR(), p0);
  return NCUR();
}

int fillMsgWithRGData(UBYTE *msgBuffer, int bufsiz) {
  UBYTE *p  = msgBuffer;
  UBYTE *p0 = msgBuffer;
# define NCUR() ((int) (p - msgBuffer))

  // Before forming block, guarantee a hit is there.
  if(!isDataAvailable()) return 0; 

  if(!isCompressBitSet(lbmp)) {
    mprintf("WARNING: dataAccessRoutines: fillMsgWithRGData: have hit data "
	    "but compress bit is not set!");
    mShowHdr(lbmp);
    return 0;
  }

  unsigned short tshi = RGTimeMSB16(lbmp); 
  p += 2; // Skip length portion, fill later
  formatShort(tshi, p);
  p += 2;
  
  while(1) {
    if(RGTimeMSB16(lbmp) != tshi) break;

    if(!isDataAvailable()) break;

    if(!isCompressBitSet(lbmp)) {
      mprintf("WARNING: dataAccessRoutines: fillMsgWithRGData: have hit data "
	      "but compress bit is not set!  Skipping hit...");
      // Skip this event
      lbmp = nextEvent(lbmp);
    }

    if(bufsiz - NCUR() < nextRGEventSize(lbmp)) break;
    if(haveOverflow(lbmp)) {
      lbmp = nextValidBlock(hal_FPGA_DOMAPP_lbm_pointer());
      //mprintf("Reset LBM pointer, hal_FPGA_DOMAPP_lbm_pointer=0x%08lx lbmp=0x%08lx",
      //        hal_FPGA_DOMAPP_lbm_pointer(), lbmp);
      break;
    }
    
    //mShowHdr(lbmp);
    // if we get here, we have room for the formatted engineering event
    // and one or more hits are available
    p += formatRGEvent(p, lbmp);  // Fill data into user's message buffer,
				  //   advance both message pointer...
    lbmp = nextEvent(lbmp);       //          ... and LBM pointer
    nTrigsReadOut++;
  }

  formatShort(NCUR(), p0); // Finally, fill length of block and return
  //mprintf("fillMsgWithRGData: Total block length is %d", NCUR());
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
      return NCUR();
    }
    
    // if we get here, we have room for the formatted engineering event
    // and one or more hits are available
    p += formatDomappEngEvent(p, lbmp);  // Fill data into user's message buffer,
					 //   advance both message pointer...
    lbmp = nextEvent(lbmp);              //          ... and LBM pointer
    nTrigsReadOut++;
  }
}
 
int fillMsgWithData(UBYTE *msgBuffer, int bufsiz, UBYTE format, UBYTE compression) {
  if(format == FMT_ENG && compression == CMP_NONE) 
    return fillMsgWithEngData(msgBuffer, bufsiz);
  if(format == FMT_RG  && compression == CMP_RG)
    return fillMsgWithRGData(msgBuffer, bufsiz);
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
	*buffer++ = *(ptr+1);
	*buffer++ = *ptr;
	ptr += 2;
    }
    return buffer;
}


UBYTE *FADCMove(USHORT *data, UBYTE *buffer, int count) {
    int i;
    UBYTE *ptr = (UBYTE *)data;

    for(i = 0; i < count; i++) {
	*buffer++ = *(ptr+1);
	*buffer++ = *ptr;
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


