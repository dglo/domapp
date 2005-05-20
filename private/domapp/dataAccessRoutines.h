/** @file dataAccessRoutines.h */
#ifndef _DATA_ACCESS_ROUTINES_
#define _DATA_ACCESS_ROUTINES_

#define TBIT(a) (1<<(a))
#define TRIG_UNKNOWN_MODE TBIT(7)
#define TRIG_LC_UPPER_ENA TBIT(6)
#define TRIG_LC_LOWER_ENA TBIT(5)
#define TRIG_FB_RUN       TBIT(4)

#define CMP_NONE  0
#define CMP_RG    1

#define FMT_ENG 0
#define FMT_RG  1

/* LBM stuff */
#define FPGA_DOMAPP_LBM_BLOCKSIZE 2048 /* BYTES not words */
#define FPGA_DOMAPP_LBM_BLOCKMASK (FPGA_DOMAPP_LBM_BLOCKSIZE-1)
#define WHOLE_LBM_MASK ((1<<24)-1)

struct tstevt {
  unsigned short tlo;
  unsigned short one;
  unsigned long  thi;
  unsigned long  trigbits;
  unsigned short tdead;
};

BOOLEAN beginRun(UBYTE compression); 
BOOLEAN endRun(void);
BOOLEAN forceRunReset(void);
void initFillMsgWithData(void);
int isDataAvailable(void);
int fillMsgWithData(UBYTE *msgBuffer, int bsize, UBYTE format, UBYTE compression);
void initFormatEngineeringEvent(UBYTE, UBYTE, UBYTE);
void startLBMTriggers(void);
void bufferLBMTriggers(void);
void insertTestEvents(void);
void setSPETrigMode(void);
void setSPEPulserTrigMode(void);
void setFBTrigMode(void);
void setPeriodicForcedTrigMode(void);
USHORT domappReadBaseADC(void);
unsigned long long domappHVSerialRaw(void);
unsigned char *lbmEvent(unsigned idx);
#endif
