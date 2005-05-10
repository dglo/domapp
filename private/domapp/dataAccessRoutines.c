/*
    Authors: Chuck McParland, John Jacobsen
    Start Date: May 4, 1999
    Modifications by John Jacobsen 2004 
                  to implement configurable engineering events
    Modifications by Jacobsen 2005 to support production FPGA
*/

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// DOM-related includes
#include "hal/DOM_MB_fpga.h"
#include "hal/DOM_MB_domapp.h"
#include "hal/DOM_FPGA_domapp_regs.h"
#include "hal/DOM_MB_pld.h"
#include "hal/DOM_FPGA_regs.h"
#include "hal/DOM_MB_types.h"
#include "hal/DOM_MB_hal.h"

#include "moniDataAccess.h"
#include "commonServices.h"
#include "DOMtypes.h"
#include "DOMdata.h"
#include "DOMstateInfo.h"
#include "lbm.h"
#include "message.h"
#include "dataAccessRoutines.h"
#include "DSCmessageAPIstatus.h"

/* Externally available pedestal waveforms */
extern unsigned short atwdpedavg[2][4][128];
extern unsigned short fadcpedavg[256];
extern USHORT atwdThreshold[2][4], fadcThreshold;

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

int   nTrigsReadOut   = 0;

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
#define TBEG(b) bench_start(b)
#define TEND(b) bench_end(b)
#define TSHOW(a,b) bench_show(a,b)
static bench_rec_t bformat, breadout, bbuffer, bcompress;
#else
#define TBEG(b)
#define TEND(b)
#define TSHOW(a,b)
#endif


BOOLEAN beginFBRun(USHORT bright, USHORT window, USHORT delay, USHORT mask, USHORT rate) {
#warning all this flasher stuff is old and needs to be redone
#define MAXHVOFFADC 5
  USHORT hvadc = halReadBaseADC();
  if(hvadc > MAXHVOFFADC) {
    mprintf("Can't start flasher board run: DOM HV ADC=%hu.", hvadc);
    return FALSE;
  }

  if(DOM_state!=DOM_IDLE) {
    mprintf("Can't start flasher board run: DOM_state=%d.", DOM_state);
    return FALSE;
  }

  halPowerDownBase(); /* Just to be sure, turn off HV */

  DOM_state = DOM_FB_RUN_IN_PROGRESS;
  int err, config_t, valid_t, reset_t;
  err = hal_FB_enable(&config_t, &valid_t, &reset_t, DOM_FPGA_DOMAPP);
  if (err != 0) {
    switch(err) {
    case FB_HAL_ERR_CONFIG_TIME:
      mprintf("Error: flasherboard configuration time too long");
      return FALSE;
    case FB_HAL_ERR_VALID_TIME:
      mprintf("Error: flasherboard clock validation time too long");
      return FALSE;
    default:
      mprintf("Error: unknown flasherboard enable failure");
      return FALSE;
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
  hal_FPGA_TEST_FB_set_rate(rate);

  /* Convert launch delay from ns to FPGA units */
  int delay_i = (delay / 25) - 2;
  delay_i = (delay_i > 0) ? delay_i : 0;
  hal_FPGA_TEST_set_atwd_LED_delay(delay_i); 

  hal_FPGA_TEST_start_FB_flashing();

  mprintf("Started flasher board run!!! bright=%hu window=%hu delay=%hu mask=%hu rate=%hu",
	  bright, window, delay, mask, rate);
  nTrigsReadOut = 0;
  return TRUE;
}

BOOLEAN endFBRun() {
  if(DOM_state!=DOM_FB_RUN_IN_PROGRESS) {
    mprintf("Can't stop flasher board run: DOM_state=%d.", DOM_state);
    return FALSE;
  }
  hal_FPGA_TEST_stop_FB_flashing();
  hal_FB_set_brightness(0);
  hal_FB_disable();
  DOM_state = DOM_IDLE;
  mprintf("Stopped flasher board run.");
  return TRUE;
}

inline BOOLEAN FBRunIsInProgress(void) { return DOM_state==DOM_FB_RUN_IN_PROGRESS; }

BOOLEAN beginRun() {
  nTrigsReadOut = 0;

  if(DOM_state!=DOM_IDLE) {
    return FALSE;
  } else {
    DOM_state=DOM_RUN_IN_PROGRESS;
    mprintf("Started run!");
    hal_FPGA_DOMAPP_disable_daq();
    halUSleep(1000); /* make sure atwd is done... */
    hal_FPGA_DOMAPP_lbm_reset();
    lbmp = hal_FPGA_DOMAPP_lbm_pointer();

    switch(FPGA_trigger_mode) { // This gets set by a DATA_ACCESS message
    case TEST_DISC_TRIG_MODE:
      hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_SPE);
      break;
    case FB_TRIG_MODE:
      hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_FLASHER);
      break;
    case CPU_TRIG_MODE: 
    default:
      hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_FORCED);
      break;
    }

    
    hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_REPEAT);
    //hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_FORCED);
    hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FORCED);
    hal_FPGA_DOMAPP_daq_mode(HAL_FPGA_DOMAPP_DAQ_MODE_ATWD_FADC);
    hal_FPGA_DOMAPP_atwd_mode(HAL_FPGA_DOMAPP_ATWD_MODE_TESTING);
    hal_FPGA_DOMAPP_cal_pulser_rate(10.0);
    hal_FPGA_DOMAPP_enable_atwds(HAL_FPGA_DOMAPP_ATWD_A|HAL_FPGA_DOMAPP_ATWD_B);
    hal_FPGA_DOMAPP_lc_mode(HAL_FPGA_DOMAPP_LC_MODE_OFF);
    hal_FPGA_DOMAPP_lbm_mode(HAL_FPGA_DOMAPP_LBM_MODE_WRAP);
    hal_FPGA_DOMAPP_lbm_mode(HAL_FPGA_DOMAPP_LBM_MODE_STOP);
    hal_FPGA_DOMAPP_compression_mode(HAL_FPGA_DOMAPP_COMPRESSION_MODE_OFF);
    hal_FPGA_DOMAPP_rate_monitor_enable(0);

    hal_FPGA_DOMAPP_enable_daq(); /* <-- Can get triggers NOW */

    /******************************************/
    /* int i;				      */
    /* for(i=0;i<400;i++) {		      */
    /*   halUSleep(2000);		      */
    /*   hal_FPGA_DOMAPP_cal_launch();	      */
    /* }    				      */
    /******************************************/

    return TRUE;
  }
}

BOOLEAN endRun() {
    if(DOM_state!=DOM_RUN_IN_PROGRESS) {
	return FALSE;
    }
    else {
	DOM_state=DOM_IDLE;
	mprintf("Disabling FPGA internal data acquisition... LBM=%u",
		hal_FPGA_DOMAPP_lbm_pointer());
	hal_FPGA_DOMAPP_disable_daq();
	mprintf("Ended run");
	return TRUE;
    }
}

BOOLEAN forceRunReset() {
    DOM_state=DOM_IDLE;
    return TRUE;
}

inline BOOLEAN runIsInProgress(void) { return DOM_state==DOM_RUN_IN_PROGRESS; }

void initFillMsgWithData(void) {
}


#define FPGA_DOMAPP_LBM_BLOCKSIZE 2048 /* BYTES not words */
#define FPGA_DOMAPP_LBM_BLOCKMASK (FPGA_DOMAPP_LBM_BLOCKSIZE-1)
#define WHOLE_LBM_MASK ((1<<24)-1)

int isDataAvailable() {
  return ( (hal_FPGA_DOMAPP_lbm_pointer()-lbmp) & ~FPGA_DOMAPP_LBM_BLOCKMASK );
}

/* Stolen from Arthur: access lookback event at idx, but changed to byte sense */
static inline unsigned char *lbmEvent(unsigned idx) {
   return
     ((unsigned char *) hal_FPGA_DOMAPP_lbm_address()) + (idx & WHOLE_LBM_MASK);
}

/* Stolen from Arthur: but changed EVENT_LEN to EVENT_SIZE */
static inline unsigned nextEvent(unsigned idx) {
   return (idx + HAL_FPGA_DOMAPP_LBM_EVENT_SIZE) & HAL_FPGA_DOMAPP_LBM_MASK;
}

static unsigned nextValidBlock(unsigned ptr) {
  return ((ptr) & ~FPGA_DOMAPP_LBM_BLOCKMASK);
}

static int haveOverflow(unsigned lbmp) {
  if( ((hal_FPGA_DOMAPP_lbm_pointer()-lbmp)&HAL_FPGA_DOMAPP_LBM_MASK) > WHOLE_LBM_MASK ) {
    mprintf("LBM OVERFLOW!!! hal_FPGA_DOMAPP_lbm_pointer=0x%08lx lbmp=0x%08lx",
	    hal_FPGA_DOMAPP_lbm_pointer(), lbmp);
    return 1;
  }
  return 0;
}

int engEventSize(void) { return 1550; } // Make this smarter

struct tstevt {
  unsigned short tlo;
  unsigned short one;
  unsigned long  thi;
  unsigned long  trigbits;
  unsigned short tdead;
};

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

  //mprintf("Got timestamp 0x%04lx%08lx",  (unsigned long) ((stamp>>32)&0xFFFFFFFF), (unsigned long) (stamp&0xFFFFFFFF));

  if(hdr->trigbits & 1<<17) {
    msgp = FADCMove((USHORT *) (e+0x10), msgp, (int) FlashADCLen);
  } else {
    mprintf("FADC data MISSING from raw event!!!");
  }

  if(hdr->trigbits & 1<<18) {
    int ich;
    for(ich = 0; ich < 4; ich++) {
      msgp = ATWDShortMove((USHORT *) (e+0x210+ich*0x100) , msgp, ATWDChLen[ich]);
    }
  } else {
    mprintf("ATWD data MISSING from raw event!!!");
  }

  int nbytes = msgp-m0;

  formatShort(nbytes, m0);
  return nbytes;
}


int fillMsgWithData(UBYTE *msgBuffer, int bufsiz) {
  BOOLEAN done;
  UBYTE *p = msgBuffer;
  done = FALSE;
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
    p += formatDomappEngEvent(p, lbmp);  /* Advance both message pointer */
    lbmp = nextEvent(lbmp);              /* ... and LBM pointer */
    nTrigsReadOut++;
  }
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


UBYTE *ATWDShortMove_PedSubtract_RoadGrade(USHORT *data, int iatwd, int ich,
					   UBYTE *buffer, int count) {
  /** Assumes earliest samples are last in the array */
  int i;
  for(i = ATWDCHSIZ - count; i < ATWDCHSIZ; i++) {
    int pedsubtracted = data[i] - atwdpedavg[iatwd][ich][i];
    USHORT chopped = (pedsubtracted > atwdThreshold[iatwd][ich]) ? pedsubtracted : 0;
    //mprintf("ATWDShortMove... i=%d data=%d pedsubtracted=%d chopped=%d",
    //        i, data[i], pedsubtracted, chopped);
    *buffer++ = (chopped >> 8)&0xFF;
    *buffer++ = chopped & 0xFF;
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


UBYTE *FADCMove_PedSubtract_RoadGrade(USHORT *data, UBYTE *buffer, int count) {
  int i;
  for(i = 0; i < count; i++) {
    int subtracted = data[i]-fadcpedavg[i];
    USHORT graded = (subtracted > fadcThreshold) ? subtracted : 0;
    //mprintf("FADCMove_... subtracted=%d graded=%d",subtracted, graded);
    *buffer++ = (graded >> 8) & 0xFF;
    *buffer++ = graded & 0xFF;
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


