/*
Author: Chuck McParland
Start Date: May 4, 1999
Description:
	Helper routines for DOM Experiment Control 
	service thread.
Modifications by John Jacobsen 2004 to implement configurable engineering events
*/
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// DOM-related includes
#include "hal/DOM_MB_fpga.h"
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
#include "DOMdataCompression.h"
#include "expControl.h"
#include "EXPmessageAPIstatus.h"
#include "DSCmessageAPIstatus.h"

/* Simulated waveform */
USHORT simspe[128] = {
  0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   2,
  6,  14,  25,  35,  40,  35,  25,  14,
  6,   2,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0,
  0,   0,   0,   0,   0,   0,   0,   0
};

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

// local functions, data
ULONG bufferWraps     = 0;
ULONG bufferOverrun   = 0;
ULONG eventsDiscarded = 0;
ULONG numPatEvts      = 0;
ULONG numCPUEvts      = 0;
int nDOMRunTriggers   = 0;
int nDOMRawTriggers   = 0; /* Include triggers which don't have local coin */

UBYTE lclEventCopy[sizeof(lbmRec)];

// routines used for generating engineering events
UBYTE *ATWDByteMove(USHORT *data, UBYTE *buffer, int count);
UBYTE *ATWDShortMove(USHORT *data, UBYTE *buffer, int count);
UBYTE *FADCMove(USHORT *data, UBYTE *buffer, int count);
UBYTE *TimeMove(UBYTE *buffer, unsigned long long time);
void formatEngineeringEvent(UBYTE *buffer, unsigned long long time);
void getPatternEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC);
BOOLEAN getCPUEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC);
BOOLEAN getTestDiscEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC);

/** Set by initFormatEngineeringEvent: */
/* UBYTE ATWDCh0Mask; */
/* UBYTE ATWDCh1Mask; */
/* UBYTE ATWDCh2Mask; */
/* UBYTE ATWDCh3Mask; */
UBYTE ATWDChMask[4];

/* int ATWDCh0Len; */
/* int ATWDCh1Len; */
/* int ATWDCh2Len; */
/* int ATWDCh3Len; */
int ATWDChLen[4];

/* BOOLEAN ATWDCh0Byte; */
/* BOOLEAN ATWDCh1Byte; */
/* BOOLEAN ATWDCh2Byte; */
/* BOOLEAN ATWDCh3Byte; */
BOOLEAN ATWDChByte[4];

/* USHORT *ATWDCh0Data; */
/* USHORT *ATWDCh1Data; */
/* USHORT *ATWDCh2Data; */
/* USHORT *ATWDCh3Data; */
USHORT *ATWDChData[4];

USHORT *FlashADCData;

UBYTE FlashADCLen;
UBYTE MiscBits = 0;
UBYTE Spare = 0;

USHORT Channel0Data[ATWDCHSIZ];
USHORT Channel1Data[ATWDCHSIZ];
USHORT Channel2Data[ATWDCHSIZ];
USHORT Channel3Data[ATWDCHSIZ];
USHORT FADCData[FADCSIZ];

lbmRec LBMHit;

#define TIMELBM
#undef TIMELBM
#ifdef TIMELBM
#define TBEG(b) bench_start(b)
#define TEND(b) bench_end(b)
#define TSHOW(a,b) bench_show(a,b)
static bench_rec_t bformat, breadout, bbuffer, bstarttrig, bcompress;
#else
#define TBEG(b)
#define TEND(b)
#define TSHOW(a,b)
#endif

#define DOCH0
#undef  DOCH0

BOOLEAN beginFBRun(USHORT bright, USHORT window, USHORT delay, USHORT mask, USHORT rate) {
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
  err = hal_FB_enable(&config_t, &valid_t, &reset_t);
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
  hal_FB_set_rate(rate);

  /* Convert launch delay from ns to FPGA units */
  int delay_i = (delay / 25) - 2;
  delay_i = (delay_i > 0) ? delay_i : 0;
  hal_FPGA_TEST_set_atwd_LED_delay(delay_i); 

  hal_FPGA_TEST_start_FB_flashing();

  mprintf("Started flasher board run!!! bright=%hu window=%hu delay=%hu mask=%hu rate=%hu",
	  bright, window, delay, mask, rate);
  nDOMRunTriggers = 0;
  startLBMTriggers(); /* Triggers can start happening NOW */

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
  nDOMRunTriggers = 0;
  if(DOM_state!=DOM_IDLE) {
    return FALSE;
  }
  else {
    DOM_state=DOM_RUN_IN_PROGRESS;
    mprintf("Started run!");
    startLBMTriggers(); /* Triggers can start happening NOW */
    return TRUE;
  }
}

BOOLEAN endRun() {
    if(DOM_state!=DOM_RUN_IN_PROGRESS) {
	return FALSE;
    }
    else {
	DOM_state=DOM_IDLE;
	mprintf("Ended run after %d triggers (%d pre-LC triggers)", nDOMRunTriggers,
		nDOMRawTriggers);
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

BOOLEAN checkDataAvailable() {

    return TRUE;
}

int fillMsgWithDataPoll(UBYTE *msgBuffer) {

  //ULONG FPGAeventIndex;
  BOOLEAN done;
  UBYTE *dataBufferPtr = msgBuffer;
  UBYTE *tmpBufferPtr;

  // start things off
  done = FALSE;
  // notify compression routines that we are starting a new buffer
  dataBufferPtr=startDOMdataBuffer(msgBuffer);

  while(!done) {
    switch (FPGA_trigger_mode) {
    case TEST_PATTERN_TRIG_MODE:
      getPatternEvent(Channel0Data, Channel1Data,
		      Channel2Data, Channel3Data, FADCData);
      break;
    case CPU_TRIG_MODE:
      if(!getCPUEvent(Channel0Data, Channel1Data,
		      Channel2Data, Channel3Data, FADCData)) {
	done = TRUE;
      }
      break;
    case TEST_DISC_TRIG_MODE:
      if(!getTestDiscEvent(Channel0Data, Channel1Data,
			   Channel2Data, Channel3Data, FADCData)) {
	done = TRUE;
      }
      break;
    default:
      getPatternEvent(Channel0Data, Channel1Data,
		      Channel2Data, Channel3Data, FADCData);
      break;
    }


    if(!done) {
      //fprintf(stderr,"dataAccessRoutines: formatting event\r\n");
      formatEngineeringEvent(lclEventCopy, hal_FPGA_TEST_get_local_clock());
      //offer it to the data compression routine
      tmpBufferPtr = addAndCompressEvent(lclEventCopy, dataBufferPtr, SW_compression);
      if(tmpBufferPtr == 0) {
	//ran out of room
	done = TRUE;
      }
      else {
	//accept it
	dataBufferPtr = tmpBufferPtr;
      }
    }
  }

  return (int)(dataBufferPtr-msgBuffer);
}

int fillMsgWithData(UBYTE *msgBuffer) {
    BOOLEAN done;
    UBYTE *dataBufferPtr = msgBuffer;
    UBYTE *tmpBufferPtr;

    // start things off
    done = FALSE;
    // notify compression routines that we are starting a new buffer
    dataBufferPtr=startDOMdataBuffer(msgBuffer);

    while(!done) {
      if(LBMQisEmpty()) {
	done = TRUE;
      } else {
	maybeGetLBMQ((lbmRec *) lclEventCopy);
      }
      
      if(!done) {
	//offer it to the data compression routine
	TBEG(bcompress);
	tmpBufferPtr = addAndCompressEvent(lclEventCopy, dataBufferPtr, SW_compression);
	TEND(&bcompress);
	if(tmpBufferPtr == NULL) {
	  //ran out of room
	  done = TRUE;
	} else {
	  //accept it
	  dataBufferPtr = tmpBufferPtr;
	  acceptLBMRec();
	}
      }
    }
    return (int)(dataBufferPtr-msgBuffer);
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

/*     //t.time = hal_FPGA_TEST_get_local_clock(); /\* Old code didn't use latched ATWD time *\/ */
/*     if(useLatched) { */
/*       if(FPGA_ATWD_select == 0) { */
/* 	t.time = hal_FPGA_TEST_get_atwd0_clock(); */
/*       } else { */
/* 	t.time = hal_FPGA_TEST_get_atwd1_clock(); */
/*       } */
/*     } else { */
/*       t.time = hal_FPGA_TEST_get_local_clock(); */
/*     } */

    for(i = 0; i < 6; i++) {
      *buffer++ = t.timeBytes[5-i];
    }

    return buffer;
}

void initFormatEngineeringEvent(UBYTE fadc_samp_cnt_arg, 
				UBYTE atwd01_mask_arg,
				UBYTE atwd23_mask_arg) {
  int ich;
#define BSIZ 1024
  //UBYTE buf[BSIZ]; int n;
  //unsigned long long time;
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
  MiscBits = 0;
  Spare = 0;
  
  ATWDChData[0] = Channel0Data;
  ATWDChData[1] = Channel1Data;
  ATWDChData[2] = Channel2Data;
  ATWDChData[3] = Channel3Data;
  FlashADCData = FADCData;
}

void formatEngineeringEvent(UBYTE *event, unsigned long long time) {
  UBYTE *beginOfEvent=event;
  USHORT length;
  int ich;
  
  //  skip over event length
  event += 2;
  
  //  fill in the event ID
  *event++ = 0x0;
  *event++ = ENG_EVENT_FID;
  
  //  various header info
  *event++ = MiscBits;
  *event++ = FlashADCLen;
  
  //  ATWD masks
  *event++ = (ATWDChMask[1]<<4) | ATWDChMask[0];
  *event++ = (ATWDChMask[3]<<4) | ATWDChMask[2];
  
  /* Trigger mask */
  UBYTE trigmask;
  switch(FPGA_trigger_mode) {
  case TEST_PATTERN_TRIG_MODE:
    trigmask = 0; break;
  case CPU_TRIG_MODE:
    trigmask = 1; break;
  case TEST_DISC_TRIG_MODE:
    trigmask = 2; break;
  case FB_TRIG_MODE: 
    trigmask = 3; break;
  default:
    trigmask = 0; trigmask |= TRIG_UNKNOWN_MODE; break;
  }
  if(LCmode == 1 || LCmode == 2) trigmask |= TRIG_LC_UPPER_ENA;
  if(LCmode == 1 || LCmode == 3) trigmask |= TRIG_LC_LOWER_ENA;
  if(FBRunIsInProgress()) trigmask |= TRIG_FB_RUN;

  *event++ = trigmask;

  //  spare byte
  *event++ = Spare;
  
  //  insert the time
  event = TimeMove(event, time);
  
  //  do something with flash data
  if(SW_compression) {
    event = FADCMove_PedSubtract_RoadGrade(FlashADCData, event, (int)FlashADCLen);
  } else {
    event = FADCMove(FlashADCData, event, (int)FlashADCLen);
  }

  //  now the ATWD data
  for(ich = 0; ich < 4; ich++) {
    if(SW_compression) {
      event = ATWDShortMove_PedSubtract_RoadGrade(ATWDChData[ich], MiscBits & 0x01, ich,
						  event, ATWDChLen[ich]);
    } else {
      event = ATWDShortMove(ATWDChData[ich], event, ATWDChLen[ich]);
    }
  }
  length = (USHORT)((event - beginOfEvent) & 0xffff);
  *beginOfEvent++ = (length >> 8) & 0xff;
  *beginOfEvent++ = length & 0xff;
} 


void getPatternEvent(USHORT *Ch0Data, USHORT *Ch1Data,
        USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC) {
    int i;
    MiscBits = 0;
    for(i = 0; i < ATWDCHSIZ; i++) {
	Ch0Data[i] = i;
  	Ch1Data[i] = ATWDCHSIZ - i;
	Ch2Data[i] = i;
	Ch3Data[i] = ATWDCHSIZ - i;
    }

    for(i = 0; i < FADCSIZ; i++) {
	FADC[i] = i;
    }
}


void getSPEPatternEvent(USHORT *Ch0Data, USHORT *Ch1Data,
			USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC) {
  int i;
  MiscBits = 0;
  int ichip = MiscBits & 0x01;
  for(i = 0; i < ATWDCHSIZ; i++) {
    Ch0Data[i] = simspe[i] + atwdpedavg[ichip][0][i];
    Ch1Data[i] = atwdpedavg[ichip][1][i];
    Ch2Data[i] = atwdpedavg[ichip][2][i];
    Ch3Data[i] = atwdpedavg[ichip][3][i];
  }

  for(i = 0; i < FADCSIZ; i++) {
    FADC[i] = fadcpedavg[i];
  }
}

BOOLEAN getCPUEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC) {
    int i;
    UBYTE trigger_mask;
    
    //fprintf(stderr, "getCPUEvent\r\n");

    if(FPGA_ATWD_select == 0) {
	trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD0|
	    HAL_FPGA_TEST_TRIGGER_FADC;
    }
    else {
	trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD1|
	    HAL_FPGA_TEST_TRIGGER_FADC;
    }
    hal_FPGA_TEST_trigger_forced(trigger_mask);

    /* poison the atwd and fadc data to make sure we're reading
     * the real thing!
     *
     * this is for testing only!
     */
    if (Ch0Data!=NULL) memset(Ch0Data, 0xaa, ATWDCHSIZ*sizeof(short));
    if (Ch1Data!=NULL) memset(Ch1Data, 0xbb, ATWDCHSIZ*sizeof(short));
    if (Ch2Data!=NULL) memset(Ch2Data, 0xcc, ATWDCHSIZ*sizeof(short));
    if (Ch3Data!=NULL) memset(Ch3Data, 0xdd, ATWDCHSIZ*sizeof(short));
    if (FADC!=NULL) memset(FADC, 0xee, FlashADCLen*sizeof(short));

    for(i = 0; i < ATWD_TIMEOUT_COUNT; i++) {
	if(hal_FPGA_TEST_readout_done(trigger_mask)) {
	    //fprintf(stderr,"readout ready\r\n");
	    if(FPGA_ATWD_select == 0) {
	        MiscBits = 0;
	    	hal_FPGA_TEST_readout(Ch0Data, Ch1Data,
		    Ch2Data, Ch3Data, 0, 0, 0, 0, ATWDCHSIZ, 
		    FADC, (int)FlashADCLen, trigger_mask);
	    }
	    else {
	        MiscBits = 1;
	    	hal_FPGA_TEST_readout(0, 0, 0, 0, Ch0Data, Ch1Data,
		    Ch2Data, Ch3Data, ATWDCHSIZ, 
		    FADC, (int)FlashADCLen, trigger_mask);
	    }
	    return TRUE;
	}
	else {
	    halUSleep(ATWD_TIMEOUT_USEC);
	}
    }
    return FALSE;
}

BOOLEAN getTestDiscEvent(USHORT *Ch0Data, USHORT *Ch1Data,
	USHORT *Ch2Data, USHORT *Ch3Data, USHORT *FADC) {
    int i;
    UBYTE trigger_mask;
    
    if(FPGA_ATWD_select == 0) {
	trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD0|
	    HAL_FPGA_TEST_TRIGGER_FADC;
    }
    else {
	trigger_mask = HAL_FPGA_TEST_TRIGGER_ATWD1|
	    HAL_FPGA_TEST_TRIGGER_FADC;
    }
    
    if(LCmode > 0) {
      hal_FPGA_TEST_trigger_disc_lc(trigger_mask);
    } else {
      hal_FPGA_TEST_trigger_disc(trigger_mask);
    }

    for(i = 0; i < ATWD_TIMEOUT_COUNT; i++) {
	if(hal_FPGA_TEST_readout_done(trigger_mask)) {
	    if(FPGA_ATWD_select == 0) {
	        MiscBits = 0;
	    	hal_FPGA_TEST_readout(Channel0Data, Channel1Data,
		    Channel2Data, Channel3Data, 0, 0, 0, 0, ATWDCHSIZ, 
		    FADC, (int)FlashADCLen, trigger_mask);
	    }
	    else {
	        MiscBits = 1;
	    	hal_FPGA_TEST_readout(0, 0, 0, 0, Channel0Data, Channel1Data,
		    Channel2Data, Channel3Data, ATWDCHSIZ, 
		    FADC, (int)FlashADCLen, trigger_mask);
	    }
	    return TRUE;
	}
	else {
	    halUSleep(ATWD_TIMEOUT_USEC);
	}
    }
    return FALSE;
}

void startLBMTriggers(void) {
  if(FBRunIsInProgress()) {
    UBYTE trigger_mask = 
      (FPGA_ATWD_select ? HAL_FPGA_TEST_TRIGGER_ATWD1 : HAL_FPGA_TEST_TRIGGER_ATWD0);
    hal_FPGA_TEST_trigger_LED(trigger_mask);
  } else if(runIsInProgress()) {
    UBYTE trigger_mask = HAL_FPGA_TEST_TRIGGER_FADC | 
      (FPGA_ATWD_select ? HAL_FPGA_TEST_TRIGGER_ATWD1 : HAL_FPGA_TEST_TRIGGER_ATWD0);
    
    switch (FPGA_trigger_mode) {
    case CPU_TRIG_MODE:
      hal_FPGA_TEST_trigger_forced(trigger_mask);
      break;
    case TEST_DISC_TRIG_MODE:
      if(LCmode > 0) {
	hal_FPGA_TEST_trigger_disc_lc(trigger_mask);
      } else {
	hal_FPGA_TEST_trigger_disc(trigger_mask);
      }
      break;
    case TEST_PATTERN_TRIG_MODE:
    default: /* Do nothing */ 
      break;
    }
  } else {
    /* Invalid run type.  Do nothing */
  }
}

void insertPedestalOnlyEvent(void) {
  int isamp, ichip=0;
  for(isamp = 0; isamp < ATWDCHSIZ; isamp++) {
    Channel0Data[isamp] = atwdpedavg[ichip][0][isamp];
    Channel1Data[isamp] = atwdpedavg[ichip][1][isamp];
    Channel2Data[isamp] = atwdpedavg[ichip][2][isamp];
    Channel3Data[isamp] = atwdpedavg[ichip][3][isamp];
  }

  for(isamp = 0; isamp < FADCSIZ; isamp++) {
    FADCData[isamp] = fadcpedavg[isamp];
  }

  formatEngineeringEvent((unsigned char *) &LBMHit, hal_FPGA_TEST_get_local_clock());
  putLBMQ(&LBMHit);
  if(LBMQisBlown()) {
    mprintf("Lookback memory is blown (%d entries).  Resetting read pointer", nLBMQEntries());
    LBMResetOverflow();
  }
}

void insertFADCSquarePulse(USHORT peak) {
  int isamp, ichip=0;
  for(isamp = 0; isamp < ATWDCHSIZ; isamp++) {
    Channel0Data[isamp] = atwdpedavg[ichip][0][isamp];
    Channel1Data[isamp] = atwdpedavg[ichip][1][isamp];
    Channel2Data[isamp] = atwdpedavg[ichip][2][isamp];
    Channel3Data[isamp] = atwdpedavg[ichip][3][isamp];
  }
  for(isamp = 0; isamp < FADCSIZ; isamp++) {
    FADCData[isamp] = fadcpedavg[isamp];
    if(isamp>5&&isamp<50) FADCData[isamp]+=peak;
  }

  formatEngineeringEvent((unsigned char *) &LBMHit, hal_FPGA_TEST_get_local_clock());
  putLBMQ(&LBMHit);
  if(LBMQisBlown()) {
    mprintf("Lookback memory is blown (%d entries).  Resetting read pointer", nLBMQEntries());
    LBMResetOverflow();
  }
}


void insertATWDSquarePulse(int ichip, int ichan, USHORT peak) {
  int isamp;
  for(isamp = 0; isamp < ATWDCHSIZ; isamp++) {
    Channel0Data[isamp] = atwdpedavg[ichip][0][isamp];
    Channel1Data[isamp] = atwdpedavg[ichip][1][isamp];
    Channel2Data[isamp] = atwdpedavg[ichip][2][isamp];
    Channel3Data[isamp] = atwdpedavg[ichip][3][isamp];
    if(ichan==0 && isamp>5 && isamp<20) Channel0Data[isamp]+=peak;
    if(ichan==1 && isamp>5 && isamp<20) Channel1Data[isamp]+=peak;
    if(ichan==2 && isamp>5 && isamp<20) Channel2Data[isamp]+=peak;
    if(ichan==3 && isamp>5 && isamp<20) Channel3Data[isamp]+=peak;
/*     if(ichan==0 && isamp == 6) mprintf("ichan=%d isamp=%d peak=%d ped=%d data=%d", */
/* 				       ichan,isamp,peak,atwdpedavg[ichip][0][isamp], */
/* 				       Channel0Data[isamp]); */
  }
  for(isamp = 0; isamp < FADCSIZ; isamp++) {
    FADCData[isamp] = fadcpedavg[isamp];
  }

  formatEngineeringEvent((unsigned char *) &LBMHit, hal_FPGA_TEST_get_local_clock());
  putLBMQ(&LBMHit);
  if(LBMQisBlown()) {
    mprintf("Lookback memory is blown (%d entries).  Resetting read pointer", nLBMQEntries());
    LBMResetOverflow();
  }  
}


void insertTestEvents(void) {
  insertPedestalOnlyEvent();
  insertFADCSquarePulse(666);
  insertATWDSquarePulse(0,1,666);
  insertATWDSquarePulse(0,2,666);
  insertATWDSquarePulse(0,3,666); 
  nDOMRunTriggers+=5;
}


void bufferLBMTriggers(void) {
  unsigned long long time;
  int gottrig = 0;
  //unsigned long tsr0, tsr1;

  /* Do nothing unless run in progress */
  if((!runIsInProgress()) && (!FBRunIsInProgress())) return; 

  UBYTE trigger_mask;
  if (FBRunIsInProgress()) {
      trigger_mask =
          (FPGA_ATWD_select ? HAL_FPGA_TEST_TRIGGER_ATWD1 : HAL_FPGA_TEST_TRIGGER_ATWD0);
  } else {
      trigger_mask = HAL_FPGA_TEST_TRIGGER_FADC | 
          (FPGA_ATWD_select ? HAL_FPGA_TEST_TRIGGER_ATWD1 : HAL_FPGA_TEST_TRIGGER_ATWD0);
  }

  /* Read out data */
  switch (FPGA_trigger_mode) {
  case CPU_TRIG_MODE:
  case TEST_DISC_TRIG_MODE:
  case FB_TRIG_MODE:
    /* Bail if nothing available */
    //tsr0 = (FPGA(TEST_SIGNAL_RESPONSE));
    if(!hal_FPGA_TEST_readout_done(trigger_mask)) break;
    if(!hal_FPGA_TEST_readout_done(trigger_mask)) break; /* Do this twice to deal with
							    firmware issues in pre-300 SW */
    //tsr1 = FPGA(TEST_SIGNAL_RESPONSE);
    //mprintf("tsr0 0x%08lx tsr1 0x%08lx trigger_mask 0x%02x", tsr0, tsr1, trigger_mask);
    nDOMRawTriggers++;
    nDOMRunTriggers++;
    if(FPGA_ATWD_select == 0) {
      MiscBits = 0;
      TBEG(breadout);
#ifdef DOCH0
      hal_FPGA_TEST_readout(Channel0Data, 0, 0, 0,
			    0, 0, 0, 0, ATWDCHSIZ, 0, (int)FlashADCLen, trigger_mask);
#else
      hal_FPGA_TEST_readout(Channel0Data, Channel1Data, Channel2Data, Channel3Data, 
			    0, 0, 0, 0, ATWDCHSIZ, FADCData, (int)FlashADCLen, trigger_mask);
#endif
      TEND(&breadout);
    } else {
      TBEG(breadout);
#ifdef DOCH0
      hal_FPGA_TEST_readout(0, 0, 0, 0, Channel0Data, 0, 0, 0,
			    ATWDCHSIZ, 0, (int)FlashADCLen, trigger_mask);
#else
      hal_FPGA_TEST_readout(0, 0, 0, 0, Channel0Data, Channel1Data, Channel2Data, Channel3Data,
			    ATWDCHSIZ, FADCData, (int)FlashADCLen, trigger_mask);
#endif
      TEND(&breadout);
    }
    gottrig = 1;
    numCPUEvts++;
    if(!(numCPUEvts%1000)) {
      mprintf("Got CPU or Disc. trigger %d (nLBM=%d head=%d tail=%d)", 
	      numCPUEvts, nLBMQEntries(), _getLBMQHead(), _getLBMQTail());
      TSHOW(&bformat,    "Format engineering event");
#ifdef DOCH0
      TSHOW(&breadout,   "Read out data from HAL (Ch0 only)");
#else
      TSHOW(&breadout,   "Read out data from HAL");
#endif
      TSHOW(&bbuffer,    "Buffer data in LBM");
      TSHOW(&bstarttrig, "Enable trigger");
      TSHOW(&bcompress,  "Add and Compress");
    }
    break;

  case TEST_PATTERN_TRIG_MODE:
  default:
    nDOMRunTriggers++;
    MiscBits = 0;
#define DOSPESIM
#undef DOSPESIM
#ifdef DOSPESIM
    getSPEPatternEvent(Channel0Data, Channel1Data,
                    Channel2Data, Channel3Data, FADCData);
#else
    getPatternEvent(Channel0Data, Channel1Data,
                    Channel2Data, Channel3Data, FADCData);
#endif
    gottrig = 1;
    numPatEvts++;
    if(!(numPatEvts%1000)) mprintf("Got test pattern event %d (nLBM=%d head=%d tail=%d)",
				   numPatEvts, nLBMQEntries(), _getLBMQHead(), _getLBMQTail());
    break;
  }
  if(gottrig) { 
    /* Timestamp it */
    if(FPGA_trigger_mode == TEST_DISC_TRIG_MODE) {
      if(FPGA_ATWD_select == 0) 
	time = hal_FPGA_TEST_get_atwd0_clock();
      else 
	time = hal_FPGA_TEST_get_atwd1_clock();
    } else {
      time = hal_FPGA_TEST_get_local_clock();
    }
    /* Start next trigger */
    TBEG(bstarttrig);
    startLBMTriggers(); 
    TEND(&bstarttrig);

    /* Format engineering event */
    TBEG(bformat);
    formatEngineeringEvent((unsigned char *) &LBMHit, time);
    TEND(&bformat);

    /* Buffer it */
    TBEG(bbuffer);
    putLBMQ(&LBMHit);
    TEND(&bbuffer);

    if(LBMQisBlown()) {
      mprintf("Lookback memory is blown (%d entries).  Resetting read pointer", nLBMQEntries());
      LBMResetOverflow();
    }
  }
}

