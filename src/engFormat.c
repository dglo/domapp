/*
 * engFormat.c
 *
 * John Kelley
 * UW-Madison, Aug 2004
 */

#include <stdlib.h>

#include "hal/DOM_MB_types.h"

/*****************************************************************/
/* 
 * Header read functions
 * 
 * Provides an interface to information contained in the engineering
 * format header.
 */

/*
 * getEngHeaderLength
 *
 * Just provide a clean interface to get the length of the header,
 * in bytes.
 */
USHORT getEngHeaderLength(const UBYTE *buf) {return 16;}

/*
 * getEngEventLength
 *
 * Get total length of engineering event, including the header.
 */
USHORT getEngEventLength(const UBYTE *buf) {
    return *((USHORT *)buf);
}

/*
 * isEngATWDAvailable
 *
 * Returns TRUE if any ATWD data is available.
 */
BOOLEAN isEngATWDAvailable(const UBYTE *buf) {
    return ((buf[6] & 0x01) || (buf[6] & 0x10) ||
            (buf[7] & 0x01) || (buf[7] & 0x10));
}

/*
 * getEngATWDNumber
 *
 * Returns which ATWD (0/1) recorded this event.
 */
UBYTE getEngATWDNumber(const UBYTE *buf) {
    return (buf[4] & 0x1);
}

/*
 * isEngChannelAvailable
 *
 * Returns TRUE if data is available for the specified 
 * ATWD channel.
 */
BOOLEAN isEngChannelAvailable(const UBYTE *buf, int ch) {
    BOOLEAN avail;

    if (!isEngATWDAvailable(buf))
        avail = FALSE;
    else {
        switch (ch) {
        case 0:
            avail = ((buf[6] & 0x01) != 0);
            break;
        case 1:
            avail = ((buf[6] & 0x10) != 0);
            break;
        case 2:
            avail = ((buf[7] & 0x01) != 0);
            break;
        case 3:
            avail = ((buf[7] & 0x10) != 0);
            break;
        default:
            avail = FALSE;
        }
    }

    return avail;
}

/*
 * getEngChannelCount
 *
 * Somewhat redundant -- get total number of ATWD channels
 * available.
 */
UBYTE getEngChannelCount(const UBYTE *buf) {

    UBYTE cnt = 0;
    int ch;

    for (ch = 0; ch < 4; ch++) {
        if (isEngChannelAvailable(buf, ch))
            cnt++;
    }
    return cnt;
}

/*
 * getEngDataSize
 *
 * Returns the size of the ATWD data for a given 
 * channel, in number of bytes.
 */
UBYTE getEngDataSize(const UBYTE *buf, int ch) {
    
    UBYTE sz;

    switch (ch) {
    case 0:
        sz = (buf[6] & 0x02) ? 2 : 1;
        break;
    case 1:
        sz = (buf[6] & 0x20) ? 2 : 1;
        break;
    case 2:
        sz = (buf[7] & 0x02) ? 2 : 1;
        break;
    case 3:
        sz = (buf[7] & 0x20) ? 2 : 1;
        break;
    default:
        sz = 0;
    }
    
    return sz;
}

/*
 * getEngSampleCount
 * 
 * Returns the number of samples recorded for a
 * particular ATWD channel.
 */
USHORT getEngSampleCount(const UBYTE *buf, int ch) {

    USHORT cnt;

    switch (ch) {
    case 0:
        cnt = (((buf[6] & 0x0c) >> 2) + 1) * 32;
        break;
    case 1:
        cnt = (((buf[6] & 0xc0) >> 6) + 1) * 32;
        break;
    case 2:
        cnt = (((buf[7] & 0x0c) >> 2) + 1) * 32;
        break;
    case 3:
        cnt = (((buf[7] & 0xc0) >> 6) + 1) * 32;
        break;
    default:
        cnt = 0;
    }
    return cnt;
}

/*
 * isEngFADCAvailable
 *
 * Returns true if FADC data is in the event.
 */
BOOLEAN isEngFADCAvailable(const UBYTE *buf) {
    return (buf[5] != 0);
}

/*
 * getEngFADCCount
 *
 * Returns the number of samples of FADC data present.
 */
USHORT getEngFADCCount(const UBYTE *buf) {
    return (USHORT)buf[5];
}

/*
 * getEngTimestampLo
 *
 * Returns the least-significant 32b of the
 * timestamp.
 */

ULONG swapLong(const UBYTE *buf) {
  return (((buf[0]) << 24)
          |((buf[1]) << 16)
          |((buf[2]) << 8)
          |((buf[3]) << 0));
}

USHORT swapShort(const UBYTE *buf) {
    return (((buf[0]) << 8)
            |((buf[1]) << 0));
}

ULONG getEngTimestampLo(const UBYTE *buf) {
    return swapLong(buf+12);
}

/*
 * getEngTimestampHi
 *
 * Returns the most-significant 16b of the timestamp.
 */
USHORT getEngTimestampHi(const UBYTE *buf) {
  //return *((USHORT *)(buf+10));
  return swapShort(buf+10);
}

/*
 * getEngTriggerFlag
 *
 * Returns the event trigger flag field from the header.
 */
UBYTE getEngTriggerFlag(const UBYTE *buf) {
    return buf[8];
}

/*****************************************************************/
/* 
 * Data functions
 * 
 * Provides an interface to obtain pointers to data within an 
 * engineering event.
 */

/*
 * getEngChannelData
 *
 * Get pointer to ATWD data of a given channel.
 */
const UBYTE *getEngChannelData(const UBYTE *buf, int ch) {

    int idx = getEngHeaderLength(buf);

    /* ATWD data comes after FADC data (short) */
    idx += getEngFADCCount(buf) * 2;

    if (isEngChannelAvailable(buf, ch))
        return (buf + idx +
                (ch*getEngSampleCount(buf, ch)*getEngDataSize(buf, ch)));
    else 
        return NULL;
}

/*
 * getEngFADCData
 *
 * Get pointer to FADC data of a given channel, if it exists.
 */
const UBYTE *getEngFADCData(const UBYTE *buf) {

    /* FADC data comes first after header */
    if (isEngFADCAvailable(buf))
        return (buf + getEngHeaderLength(buf));       
    else 
        return NULL;
}

/*****************************************************************/
/*
 * Header write functions
 *
 * Construct the engineering event header using these functions.
 *
 */

/*
 * setEngATWDNumber
 *
 * Sets which ATWD (0/1) recorded this event.
 */
void setEngATWDNumber(UBYTE *buf, int atwd) {
    buf[4] |= atwd & 0x1;
}

/*
 * setEngChannelAvailable
 *
 * Sets if a particular channel of ATWD data is available.
 *
 */
void setEngChannelAvailable(UBYTE *buf, int ch) {
    switch (ch) {
    case 0:
        buf[6] |= 0x01;
        break;
    case 1:
        buf[6] |= 0x10;
        break;
    case 2:
        buf[7] |= 0x01;
        break;
    case 3:
        buf[7] |= 0x10;
        break;
    }
}

/*
 * setEngDataSize
 *
 * Sets the size of the ATWD data for a given 
 * channel, in number of bytes.
 */
void setEngDataSize(UBYTE *buf, int ch, UBYTE sz) {
    
    /* Size should be 2 or 1 bytes */
    if (sz == 2) {
        switch (ch) {
        case 0:
            buf[6] |= 0x02;
            break;
        case 1:
            buf[6] |= 0x20;
            break;
        case 2:
            buf[7] |= 0x02;
            break;
        case 3:
            buf[7] |= 0x20;
            break;
        }
    }
}

/*
 * setEngSampleCount
 * 
 * Sets the number of samples recorded for a
 * particular ATWD channel.
 */
void setEngSampleCount(UBYTE *buf, int ch, USHORT cnt) {

    /* Number of 32-count blocks */
    /* Make sure it's only 2 bits, too */
    UBYTE blks = ((cnt / 32) - 1) & 0x3;

    switch (ch) {
    case 0:
        buf[6] |= blks << 2;
        break;
    case 1:
        buf[6] |= blks << 6;
        break;
    case 2:
        buf[7] |= blks << 2;
        break;
    case 3:
        buf[7] |= blks << 6;
        break;
    }
}

/*
 * setEngFADCCount
 *
 * Sets the number of samples of FADC data present.
 */
void setEngFADCCount(UBYTE *buf, UBYTE cnt) {
    buf[5] = cnt;
}

/*
 * setEngTimestampLo
 *
 * Sets the least-significant 32b of the
 * timestamp.  Must unfortunately swap yet again (JEJ)
 */
void setEngTimestampLo(UBYTE *buf, ULONG time_lo) {   
#ifdef LE
  *((ULONG *)(buf+12)) = time_lo;
#else
  int of    = 12;
  buf[of+3] = ((time_lo >> 0)  & 0xFF);
  buf[of+2] = ((time_lo >> 8)  & 0xFF);
  buf[of+1] = ((time_lo >> 16) & 0xFF);
  buf[of+0] = ((time_lo >> 24) & 0xFF);
#endif
}

/*
 * setEngTimestampHi
 *
 * Sets the most-significant 16b of the timestamp.
 */
void setEngTimestampHi(UBYTE *buf, USHORT time_hi) {    
#ifdef LE
  *((USHORT *)(buf+10)) = time_hi;
#else
  int of = 10;
  buf[of+1] = ((time_hi >> 0) & 0xFF);
  buf[of+0] = ((time_hi >> 8) & 0xFF);
#endif
}

/*
 * setEngTriggerFlag
 *
 * Sets the event trigger flag field in the header.
 */
void setEngTriggerFlag(UBYTE *buf, UBYTE flag) {
    buf[8] = flag;
}

/*
 * setEngEventLength
 *
 * Sets the event length, including the header, in bytes.
 *
 */
void setEngEventLength(UBYTE *buf, USHORT len) {
#ifdef LE
    *((USHORT *)buf) = len;
#else
    buf[1] = ((len >> 0) & 0xFF);
    buf[0] = ((len >> 8) & 0xFF);
#endif
}

/*
 * setEngEventFormat
 *
 * Sets event format to BIG ENDIAN (JEJ)
 *
 */
void setEngEventFormat(UBYTE *buf) {
    buf[2] = 0x00;
    buf[3] = 0x01;
}
