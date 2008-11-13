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

/* LBM stuff */
#define FPGA_DOMAPP_LBM_BLOCKSIZE 2048 /* BYTES not words */
#define FPGA_DOMAPP_LBM_BLOCKMASK (FPGA_DOMAPP_LBM_BLOCKSIZE-1)
#define MIN_LBM_BIT_DEPTH     8        /* This is somewhat arbitrary */
#define DEFAULT_LBM_BIT_DEPTH 21       /* 2MB */
#define ACTUAL_LBM_BIT_DEPTH  24       /* Size is 2**24-1 = 16MB */
#define WHOLE_LBM_MASK ((1<<ACTUAL_LBM_BIT_DEPTH)-1)

struct tstevt {
  unsigned short tlo;
  unsigned short one;
  unsigned long  thi;
  unsigned long  trigbits;
  unsigned short tdead;
};

void initFillMsgWithData(void);
int isDataAvailable(void);
int fillMsgWithData(UBYTE *msgBuffer, int bsize, UBYTE format, UBYTE compression);
int fillMsgWithSNData(UBYTE *msgBuffer, int bsize);
void initFormatEngineeringEvent(UBYTE, UBYTE, UBYTE);
int  beginRun(UBYTE compressionMode, UBYTE newRunState);
int  beginFBRun(UBYTE compressionMode, USHORT bright, USHORT window, 
		short delay, USHORT mask, USHORT rate);
void turnOffFlashers(void);
int  endRun(void);
int  endFBRun(void);
unsigned getLastHitCount(void);
unsigned getLastHitCountNoReset(void);

USHORT domappReadBaseADC(void);
unsigned long long domappHVSerialRaw(void);
unsigned char *lbmEvent(unsigned idx);
#endif
