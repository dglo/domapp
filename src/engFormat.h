/*
 * engFormat.h
 *
 * John Kelley
 * UW-Madison, Aug 2004
 */

#include "hal/DOM_MB_types.h"

USHORT getHeaderLength(const UBYTE *buf);
USHORT getEngEventLength(const UBYTE *buf);

BOOLEAN isEngATWDAvailable(const UBYTE *buf);
UBYTE getEngATWDNumber(const UBYTE *buf);
BOOLEAN isEngChannelAvailable(const UBYTE *buf, int ch);
UBYTE getEngChannelCount(const UBYTE *buf);
UBYTE getEngDataSize(const UBYTE *buf, int ch);
USHORT getEngSampleCount(const UBYTE *buf, int ch);
BOOLEAN isEngFADCAvailable(const UBYTE *buf);
USHORT getEngFADCCount(const UBYTE *buf);
ULONG getEngTimestampLo(const UBYTE *buf);
USHORT getEngTimestampHi(const UBYTE *buf);
UBYTE getEngTriggerFlag(const UBYTE *buf);
UBYTE *getEngChannelData(const UBYTE *buf, int ch);
UBYTE *getEngFADCData(const UBYTE *buf);

void setEngATWDNumber(UBYTE *buf, int atwd);
void setEngChannelAvailable(UBYTE *buf, int ch);
void setEngDataSize(UBYTE *buf, int ch, UBYTE sz);
void setEngSampleCount(UBYTE *buf, int ch, USHORT cnt);
void setEngFADCCount(UBYTE *buf, UBYTE cnt);
void setEngTimestampLo(UBYTE *buf, ULONG time);    
void setEngTimestampHi(UBYTE *buf, ULONG time);    
void setEngTriggerFlag(UBYTE *buf, UBYTE flag);
void setEngEventLength(UBYTE *buf, USHORT len);
void setEngEventFormat(UBYTE *buf);

