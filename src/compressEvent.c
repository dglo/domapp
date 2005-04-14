/*
 * compressEvent.c
 * 
 * Compress an engineering format event using pedestal
 * subtraction, road grading, and run-length compression.
 *
 * John Kelley
 * UW-Madison
 * September 2004
 */
#include <stdio.h>
#include <stdlib.h>

/* For clock: */
/* #include "hal/DOM_FPGA_regs.h" */
#include "hal/DOM_MB_types.h"
#include "engFormat.h"
#include "compressEvent.h"
#include "moniDataAccess.h"

/* Externally available pedestal waveforms */
extern unsigned short atwdpedavg[2][4][128];
extern unsigned short fadcpedavg[255];

/* Road grader thresholds for each ATWD and channel, and FADC */
USHORT atwdThreshold[2][4] = {
  {DEFAULT_ATWD_THRESH,DEFAULT_ATWD_THRESH,DEFAULT_ATWD_THRESH,DEFAULT_ATWD_THRESH},
  {DEFAULT_ATWD_THRESH,DEFAULT_ATWD_THRESH,DEFAULT_ATWD_THRESH,DEFAULT_ATWD_THRESH}
};
USHORT fadcThreshold = DEFAULT_FADC_THRESH;

/*****************************************************************/
/*
 * putBit
 *
 * Put a single bit into the output buffer, keeping track
 * of what word and bit we're on.  Takes care of zeroing
 * out new words.
 */
void putBit(USHORT val, ULONG *buf_out, USHORT *word_p, USHORT *bit_p) {

    /* Zero out a new word */
    if (*bit_p == 0)
        buf_out[*word_p] = 0;

    /* OR in the bit, starting at the LSB position in the output word */
    if (val)
        buf_out[*word_p] |= (0x1U << *bit_p);

    /* Increment bits but max out at 31 */
    (*bit_p)++;
    if (*bit_p == 32) {
        *bit_p = 0;
        (*word_p)++;
    }
}

/*****************************************************************/
/*
 * putVal
 *
 * Put a value onto the output buffer.  Zeros are compressed
 * to a single bit, and non-zero values add a non-zero indicator
 * bit.  
 *
 */
void putVal(USHORT val, ULONG *buf_out, USHORT *word_p, USHORT *bit_p) {

    USHORT mask;

    if (val == 0) {
        putBit(0, buf_out, word_p, bit_p);
    }
    else {
        /* Add on the non-zero bit */
        putBit(1, buf_out, word_p, bit_p);

        /* Add on the bits, LSB first */
        for(mask = 0x1; mask <= (0x1 << (COMPRESS_NUMBITS-1)); mask <<= 1) {
            if (val & mask)
                putBit(1, buf_out, word_p, bit_p);
            else
                putBit(0, buf_out, word_p, bit_p);
        }
        
    }
}

/*****************************************************************/
/*
 * compressWaveform
 *
 * Implements K. Helbing's run-length compression on a waveform
 * of N-bit values (as USHORTs).  The input values should 
 * already be road-graded, if desired.  The output is compressed into
 * a series of words as ULONGs.  Returns the length of the output
 * buffer in words used.
 *
 */
USHORT compressWaveform(USHORT *buf_in, USHORT len_in, ULONG *buf_out) {

    int i;
    int err;
    USHORT bit, word, runlen;

    err = bit = word = runlen = 0;

    if ((buf_in == NULL) || (buf_out == NULL) || (len_in <= 0))
        return 0;

    for (i = 1; i <= len_in; i++) {

        /* Check to see if run of values continues */
        /* NB: run lengths greater than N-bit data value not supported */
        if ((i < len_in) && (buf_in[i] == buf_in[i-1])) {
            runlen++;
        }
        /* Otherwise encode previous value and run length */
        else { 

            /* Put the data value and run length into the buffer */
            putVal(buf_in[i-1], buf_out, &word, &bit);
            putVal(runlen, buf_out, &word, &bit);

            /* Reset run length */
            runlen = 0;
        }
    }

    /* Round up if some bits are used in final word */
    if (bit > 0)
        word++;

    return word;
}

/*****************************************************************/
/* 
 * roadGrade
 *
 * Clamp all values in the input buffer at or below a given threshold
 * to zero (absolute value!).  Modifies buffer.
 */
void roadGrade(USHORT *buf, USHORT len, USHORT thresh) {

    int i;
    for (i = 0; i < len; i++)
        buf[i] = (abs(buf[i]) <= thresh) ? 0 : buf[i];

}

/*****************************************************************/
/* 
 * setATWDRoadGradeThreshold
 * 
 * Set the road grader threshold for the given atwd and ch.
 */
void setATWDRoadGradeThreshold(USHORT threshold, int atwd, int ch) {

    if (((atwd == 0) || (atwd == 1)) && (ch >= 0) && (ch <= 3))
        atwdThreshold[atwd][ch] = threshold;
    //mprintf("atwdthreshold[%d][%d] = %hu", atwd, ch, threshold);

}

void setFADCRoadGradeThreshold(USHORT threshold) {
    //mprintf("fadcthreshold = %hu", threshold);
    fadcThreshold = threshold;
}

/*****************************************************************/
/* 
 * getATWDRoadGradeThreshold
 * 
 * Get the road grader threshold for the given atwd and ch.
 */
USHORT getATWDRoadGradeThreshold(int atwd, int ch) {

    if (((atwd == 0) || (atwd == 1)) && (ch >= 0) && (ch <= 3))
        return atwdThreshold[atwd][ch];
    else
        return 0;
}

USHORT getFADCRoadGradeThreshold(void) {
    return fadcThreshold;
}

/*****************************************************************/
/* 
 * pedestalSub
 *
 * Subtract pedestal values from an input buffer (must be the same
 * length!).  Also inverts waveform (pulses are negative-going!) 
 * and sets negative values to zero.
 * 
 * Modifies input buffer.
 */
void pedestalSub(USHORT *databuf, USHORT *pedbuf, USHORT len) {

    /* Signed! */
    short val;
    int i;

    for (i = 0; i < len; i++) {
      val = databuf[i] - pedbuf[i];
      /* Invert and set negative values to zero */
      databuf[i] = (val > 0) ? val : 0;
    }
}

/*****************************************************************/
/*
 * compressHeader 
 *
 * Translate an engineering format header into a compressed 
 * event header.
 *
 */
void compressHeader(const UBYTE *header_in, ULONG *header_out) {

    /* WORD 0 */
    /* 0 - Set compression bit */
    header_out[0] = 0x1;

    /* 11:1 - length, leave zero for now */

    /* 12 - FADC available? */
    header_out[0] |= isEngFADCAvailable(header_in) ? 0x1000 : 0;

    /* 13 - ATWD available? */
    header_out[0] |= isEngATWDAvailable(header_in) ? 0x2000 : 0;

    /* 15:14 - channel information */
    /* FIX ME: what if I have a different comb. of channels? */
    /* 
     * 00 : ch0
     * 01 : ch0 + ch1
     * 10 : ch0 + ch1 + ch2
     * 11 : ch0 + ch1 + ch2 + ch3
     */
    if (getEngChannelCount(header_in) > 0)
        header_out[0] |= ((getEngChannelCount(header_in) - 1) & 0x3) << 14; 

    /* 16 - ATWD0/1 */
    header_out[0] |= (getEngATWDNumber(header_in) & 0x1) << 16;

    /* 18:17 - LC, leave zero for now */
    
    /* 31:19 - trigger information */
    header_out[0] |= (getEngTriggerFlag(header_in) << 19);

    /* WORD 1 */
    /* 31:0 - lower 32b of timestamp */
    header_out[1] = getEngTimestampLo(header_in);
}

/*****************************************************************/
/* 
 * setEventLength
 *
 * Call after all waveform compression to set the total length of a
 * compressed event in the header, in bytes.  Should include header
 * as well.
 *
 */
void setEventLength(ULONG *header, USHORT len) {

    /* 11:1 - length in bytes, including header */
    header[0] |= ((ULONG)(len & 0x7ff)) << 1;
}

/*****************************************************************/
/* 
 * compressEvent
 * 
 * Given a pointer to a engineering format event, go through the 
 * following steps:
 *  - change the header to a compressed event header
 *  - subtract pedestals from waveforms, if available
 *  - road-grade ("zero suppress") the waveforms
 *  - run-length compress the waveforms
 *
 * Returns length of compressed event, in *words*.
 */
USHORT compressEvent(const UBYTE *buf_in, ULONG *buf_out) {

    USHORT word_idx_out = 0;

    const UBYTE *data;
    USHORT waves[768];

    USHORT data_len;
    USHORT len_in = 0;
    UBYTE data_sz;
    int ch, i;

    /* Read and compress the header */
    compressHeader(buf_in, buf_out);

    word_idx_out += 2;

    if ((data = getEngFADCData(buf_in)) != NULL) {

        data_len = getEngFADCCount(buf_in);
        
        /* Copy to working buffer in SRAM */
        /* Note that FADC data is always size short */
        for (i = 0; i < data_len; i++) {
            waves[len_in+i] = ((data[i*2]&0xFF)<<8)|data[i*2+1];
        }

        /* Subtract "pedestal" (really a baseline) */
        //this is now done in dataAccessRoutines.c
        //pedestalSub(waves+len_in, fadcpedavg, data_len);

        /* Road-grades the input at the threshold level */
        /* A threshold of zero totally disables this */
        //this is now done in dataAccessRoutines.c
        //if (fadcThreshold > 0)
        //  roadGrade(waves+len_in, data_len, fadcThreshold);

        /* Fill samples we have no data for with zero */
        for (i = data_len; i < 255; i++)
            waves[len_in+i] = 0;

        len_in += 255;
    }


    /* Compress all ATWD channels available */
    int atwd = getEngATWDNumber(buf_in);
    for (ch = 0; ch < 4; ch++) {
        
        if ((data = getEngChannelData(buf_in, ch)) != NULL) {
            
            data_len = getEngSampleCount(buf_in, ch);
            data_sz = getEngDataSize(buf_in, ch);

            /* Copy to working buffer in SRAM */
            for (i = 0; i < data_len; i++) {
                //waves[len_in+i] = *(USHORT *)(&data[i*data_sz]);
                waves[len_in+i] = ((data[i*2]&0xFF)<<8)|data[i*2+1];                
            }

            /* Subtract pedestal values */
            //this is now done in dataAccessRoutines.c
            //pedestalSub(waves+len_in, atwdpedavg[atwd][ch], data_len);
            
            /* Road-grades the input at the threshold level */
            /* A threshold of zero totally disables this */
            //this is now done in dataAccessRoutines.c
            //if (atwdThreshold[atwd][ch] > 0)
            //    roadGrade(waves+len_in, data_len, atwdThreshold[atwd][ch]);
            
            /* Fill samples we have no data for with zero */
            for (i = data_len; i < 128; i++)
                waves[len_in+i] = 0;

            len_in += 128;            
        }
    }

    /* Compress the waveforms, all at once */
    word_idx_out += compressWaveform(waves, len_in, buf_out+word_idx_out);

    /* Update length in header */
    setEventLength(buf_out, word_idx_out * 4);
    /* Return number of word used in the output buffer */
    return word_idx_out;
}
