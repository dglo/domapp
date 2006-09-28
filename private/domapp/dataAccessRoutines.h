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
#define WHOLE_LBM_MASK ((1<<24)-1)
#define SW_LBM_MASK ((1<<21)-1) /* Throttle back lookback memory to prevent floods of data in DAQ */

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
int  endRun(void);
int  endFBRun(void);

USHORT domappReadBaseADC(void);
unsigned long long domappHVSerialRaw(void);
unsigned char *lbmEvent(unsigned idx);
#endif
