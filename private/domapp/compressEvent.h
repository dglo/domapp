/*
 * compressEvent
 *
 * Header file for software compression.
 */

/* Number of bits per value in the input buffer */
#define COMPRESS_NUMBITS     10

/* Prototypes */
USHORT compressEvent(const UBYTE *buf_in, ULONG *buf_out);

void setATWDRoadGradeThreshold(USHORT threshold, int atwd, int ch);
void setFADCRoadGradeThreshold(USHORT threshold);

#define DEFAULT_ATWD_THRESH 10
#define DEFAULT_FADC_THRESH 10

USHORT getATWDRoadGradeThreshold(int atwd, int ch);
USHORT getFADCRoadGradeThreshold(void);
