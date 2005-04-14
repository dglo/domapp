/* DOMdata.h */

#ifndef _DOMDATA_
#define _DOMDATA_

#define FID_LEN 4
#define TRIG_LEN 8
#define ATWD_LO_LEN 128
#define ATWD_HI_LEN 256
#define ATWD0 0
#define ATWD1 1

#define ATWDCHSIZ 128
#define FADCSIZ   256

/* data pattern of 0's */
#define ZERO 0

/* data pattern of incrementing ints
(e.g. {0,1,2,3,...}).  Pattern NOT
formated in any way (e.g. 8 bit to 16 bit
representations for high resolution data). */
#define RAW_CNT 1

/* data pattern of incrementing ints
(e.g. {0,1,2,3,...}). */
#define FMT_CNT 2

/* clock data 32 Mhtz clock -about 10 bins per
cycle */
#define CLK_32MHTZ 3

/* real data taken from a random location in the
file named in the struct */
#define REAL_DATA 4

/* read data taken from a random location in the
file named in the struct DIVIDED BY 10 */
#define REAL_DATA_DIV10 5

/* read test data from the ATWD */
#define ATWD_TEST_DATA 6

/* read test data from the slow adc */
#define SLOWADC_TEST_DATA 7

/* read test data from the communications ADC */
#define COMADC_TEST_DATA 8

typedef struct {
	BOOLEAN hi;
	BOOLEAN lo;
	BYTE	contents;
} ATWD_DESC;

#define ATWD0_LO_PRES 1
#define ATWD0_HI_PRES 2
#define ATWD1_LO_PRES 4
#define ATWD1_HI_PRES 8
#define ATWD2_LO_PRES 0x10
#define ATWD2_HI_PRES 0x20
#define ATWD3_LO_PRES 0x40
#define ATWD3_HI_PRES 0x80

typedef struct {
	BYTE len;
	BYTE contents;
} SLOWADC_DESC;

typedef struct {
	ULONG time;
	int energy;
	BYTE coinc;
	BYTE quality;
	BOOLEAN SPE;
} TRIGGER_DESC;

#define SPE_MASK 0
#define MSPE_MASK 0x10

typedef struct {
	TRIGGER_DESC trig;
	ATWD_DESC atwd0;
	ATWD_DESC atwd1;
	ATWD_DESC atwd2;
	ATWD_DESC atwd3;
	SLOWADC_DESC adc;
} DOM_DATA;

#endif
