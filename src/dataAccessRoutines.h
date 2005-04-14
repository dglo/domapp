/** @file dataAccessRoutines.h */
#ifndef _DATA_ACCESS_ROUTINES_
#define _DATA_ACCESS_ROUTINES_

#define TBIT(a) (1<<(a))
#define TRIG_UNKNOWN_MODE TBIT(7)
#define TRIG_LC_UPPER_ENA TBIT(6)
#define TRIG_LC_LOWER_ENA TBIT(5)
#define TRIG_FB_RUN       TBIT(4)
BOOLEAN beginRun(void); 

BOOLEAN endRun(void);

BOOLEAN forceRunReset(void);

void initFillMsgWithData(void);

BOOLEAN checkDataAvailable(void);

int fillMsgWithData(UBYTE *msgBuffer);

void initFormatEngineeringEvent(UBYTE, UBYTE, UBYTE);

void startLBMTriggers();
void bufferLBMTriggers(void);
void insertTestEvents(void);

#endif
