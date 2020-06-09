/** @file dataAccessRoutines.h */
#ifndef _DATA_ACCESS_ROUTINES_
#define _DATA_ACCESS_ROUTINES_

#define TBIT(a) (1<<(a))
#define TRIG_UNKNOWN_MODE TBIT(7)
#define TRIG_LC_UPPER_ENA TBIT(6)
#define TRIG_LC_LOWER_ENA TBIT(5)
#define TRIG_FB_RUN       TBIT(4)

enum {
  CMP_NONE  = 0,
  CMP_RG    = 1,
  CMP_DELTA = 2
};

enum {
  FMT_ENG   = 0,
  FMT_RG    = 1,
  FMT_DELTA = 2
};

enum {
  F_MONI_RATE_HLC = 0,
  F_MONI_RATE_SLC = 1
};

/* LBM stuff */
#define FPGA_DOMAPP_LBM_BLOCKSIZE 2048 /* BYTES not words */
#define FPGA_DOMAPP_LBM_BLOCKMASK (FPGA_DOMAPP_LBM_BLOCKSIZE-1)
#define MIN_LBM_BIT_DEPTH     8        /* This is somewhat arbitrary */
#define ACTUAL_LBM_BIT_DEPTH  24       /* Size is 2**24-1 = 16MB */
#define DEFAULT_LBM_BIT_DEPTH ACTUAL_LBM_BIT_DEPTH /* Used to be 2**21-1 = 2 MB */
#define WHOLE_LBM_MASK ((1<<ACTUAL_LBM_BIT_DEPTH)-1)

/* PMT HV / Flasherboard interlocks */
#define MAX_HVADC_OFF 5
#define MAX_FB_BRIGHTNESS_HV_ON 35
#define MAX_FB_WIDTH_HV_ON 30

#define FPGA_LBM_WRITE_BIT_DEPTH 28
/* FPGA write address space is 28 bits, i.e. the write counter wraps
   at 1<<28.  Note that this is larger than the available physical
   address space. */
#define LBM_WRITE_POINTER_MASK ((1<<FPGA_LBM_WRITE_BIT_DEPTH)-1)

struct tstevt {
  unsigned short tlo;
  unsigned short one;
  unsigned long  thi;
  unsigned long  trigbits;
  unsigned short tdead;
};

void initFillMsgWithData(void);
int isDataAvailable(unsigned, unsigned, unsigned);
int fillMsgWithData(UBYTE *msgBuffer, int bsize, UBYTE format, UBYTE compression);
int fillMsgWithSNData(UBYTE *msgBuffer, int bsize);
int fillMsgWithMoniData(MESSAGE_STRUCT *M);
void initFormatEngineeringEvent(UBYTE, UBYTE, UBYTE);
int  beginRun(UBYTE compressionMode, UBYTE newRunState);
int  beginFBRun(UBYTE compressionMode, USHORT bright, USHORT window, 
		short delay, USHORT mask, USHORT rate);
int  changeFBsettings(USHORT bright, USHORT window, 
		      short delay, USHORT mask, USHORT rate);
void turnOffFlashers(void);
int  endRun(void);
int  endFBRun(void);
unsigned getLastHitCount(void);
unsigned getLastHitCountNoReset(void);
USHORT domappReadBaseADC(void);
unsigned long long domappHVSerialRaw(void);
unsigned char *lbmEvent(unsigned idx);
int countMsgWithDeltaData(UBYTE *msgBuffer, int bufsiz);
int countMsgWithEngData(UBYTE *msgBuffer, int bufsiz);
int countMsgWithData(UBYTE *msgBuffer, int bufsiz, UBYTE format, UBYTE compression);
int domappDecodeTriggerMode(UBYTE trigger_mode);
#endif
