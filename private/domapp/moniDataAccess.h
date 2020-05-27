/* moniDataAccess.h
 * Header file for store/fetch operations on monitoring data
 * Part of dataAccess thread
 * John Jacobsen, jacobsen@npxdesigns.com, for LBNL
 * May, 2003
 * $Id: moniDataAccess.h,v 1.7.4.11 2007-10-29 21:59:50 jacobsen Exp $
 */

#ifndef _MONI_DATA_ACCESS_
#define _MONI_DATA_ACCESS_

#define MONI_TIME_LEN 6
#define MAXMONI_RECSIZE 1024
#define MAXMONI_DATA MAXMONI_RECSIZE-4-MONI_TIME_LEN 

struct moniRec {
  USHORT dataLen;    /* Number of bytes in data portion */
  UBYTE moniEvtType; /* One of MONI_TYPE... */
  UBYTE spare;       /* For alignment */
  unsigned long long time; /* Make this explicitly 64 bits; use only 48 */
  UBYTE data[MAXMONI_DATA];  /* Payload data */
};

/* IMPORTANT NOTE: all data structs to be put in the data portion of monitor
   records MUST be less than MAXMONI_DATA bytes in length!!! */

#define HARDWARE_STATE_EVENT_VERSION 0x01

/* WARNING: These must match values in DataAccGetMonitorMsg.java! */
#define MONI_TYPE_HDWR_STATE_MSG     0xC8
#define MONI_TYPE_CONF_STATE_MSG     0xC9
#define MONI_TYPE_CONF_STATE_CHG_MSG 0xCA
#define MONI_TYPE_LOG_MSG            0xCB
#define MONI_TYPE_GENERIC_MSG        0xCC


/* Should be defined by the HAL, but if not, do it here: */
#ifndef FPGA_HAL_TICKS_PER_SEC 
#define FPGA_HAL_TICKS_PER_SEC 40000000
#endif

struct moniConfig {
  /* ALL SHORT FIELDS MUST BE FILLED BIG-ENDIAN!!! i.e. use moniBEShort() */
  UBYTE  config_event_version;  /* 1 */
  UBYTE  spare0;                /* 2 */
  USHORT hw_config_len;         /* 4 */
  UBYTE  dom_mb_id[6];          /* 10 */
  USHORT spare1; /* skip over next two bytes to align. */ /* 12 */
  UBYTE  hw_base_id[8];         /* 20 */
  USHORT fpga_build_num;        /* 22 */
  USHORT sw_config_len;         /* 24 */
  USHORT dom_mb_sw_build_num;   /* 26 */
  UBYTE  msg_hdlr_major;         
  UBYTE  msg_hdlr_minor;
  UBYTE  exp_ctrl_major;
  UBYTE  exp_ctrl_minor;
  UBYTE  slo_ctrl_major;
  UBYTE  slo_ctrl_minor;
  UBYTE  data_acc_major;
  UBYTE  data_acc_minor;       /* 34 */
  USHORT daq_config_len;       /* 36 */
  ULONG  trig_config_info;     /* 40 */
  ULONG  atwd_readout_info;    /* 44 */
};

struct moniHardware {
  /* ALL SHORT FIELDS MUST BE FILLED BIG-ENDIAN!!! i.e. use moniBEShort() */
  UBYTE STATE_EVENT_VERSION;
  UBYTE spare;
  USHORT ADC_VOLTAGE_SUM;
  USHORT ADC_5V_POWER_SUPPLY;
  USHORT ADC_PRESSURE;
  USHORT ADC_5V_CURRENT;
  USHORT ADC_3_3V_CURRENT;
  USHORT ADC_2_5V_CURRENT;
  USHORT ADC_1_8V_CURRENT;
  USHORT ADC_MINUS_5V_CURRENT;
  USHORT DAC_ATWD0_TRIGGER_BIAS;
  USHORT DAC_ATWD0_RAMP_TOP;
  USHORT DAC_ATWD0_RAMP_RATE;
  USHORT DAC_ATWD_ANALOG_REF;
  USHORT DAC_ATWD1_TRIGGER_BIAS;
  USHORT DAC_ATWD1_RAMP_TOP;
  USHORT DAC_ATWD1_RAMP_RATE;
  USHORT DAC_PMT_FE_PEDESTAL;
  USHORT DAC_MULTIPLE_SPE_THRESH;
  USHORT DAC_SINGLE_SPE_THRESH;
  USHORT DAC_LED_BRIGHTNESS;
  USHORT DAC_FAST_ADC_REF;
  USHORT DAC_INTERNAL_PULSER;
  USHORT DAC_FE_AMP_LOWER_CLAMP;
  USHORT DAC_FL_REF;
  USHORT DAC_MUX_BIAS;
  USHORT PMT_BASE_HV_SET_VALUE;
  USHORT PMT_BASE_HV_MONITOR_VALUE;
  USHORT DOM_MB_TEMPERATURE;
  ULONG  SPE_RATE;
  ULONG  MPE_RATE;
};

/** The circular buffer will be 
   MONI_CIRCBUF_RECS * sizeof(struct moniRec) bytes.
   MUST BE POWER OF TWO!!! */
#define MONI_CIRCBUF_RECS 1024

#define MONI_MASK (MONI_CIRCBUF_RECS-1)

/* Return type for consumer function */
typedef enum {
  MONI_OK       = 1,
  MONI_NODATA   = 2,
  MONI_WRAPPED  = 3,
  MONI_OVERFLOW = 4,
  MONI_NOTINITIALIZED = 5,
  MONI_ERROR    = 6
} MONI_STATUS;

typedef enum {
  CHARGE_STAMP_ATWD = 0,
  CHARGE_STAMP_FADC = 1,
} CHARGE_STAMP_MODE_TYPE;

typedef enum {
  CHARGE_STAMP_AUTO    = 0,
  CHARGE_STAMP_BY_CHAN = 1,
} CHARGE_STAMP_SEL_TYPE;


/* Prototypes */

void moniRunTests(void);
unsigned long long moniGetHdwrIval(void);
unsigned long long moniGetConfIval(void);
unsigned long long moniGetFastIval(void);

void moniInit(UBYTE *bufBaseAddr, 
	      int mask);                      /* Initializes circular buffer 
						 pointers */

void moniZeroIndices(void);                   /* Resets buffer, assuming moniInit
						 was called at some point first */

int moniHaveData(void);                       /* Any records in buffer? */

void moniInsertRec(struct moniRec *m);        /* Producer function */

MONI_STATUS moniFetchRec(struct moniRec *m);  /* Consumer function 
						 (data access thread) */

void moniAcceptRec(void);	              /* accept previously fetched record */


/* The following functions use moniInsertRec to insert data */

void moniInsertHdwrStateMessage(unsigned long long time, USHORT temperature, 
				long spe_sum, long mpe_sum);
/* Timestamped rec. of all DACs, ADCs, etc. hardware info on board */

void moniInsertConfigStateMessage(unsigned long long time);
/* DOM Configuration state message */

void moniInsertDiagnosticMessage(char *msg, unsigned long long time, int len);
/* Text string */

void mprintf(char *arg, ...);

void moniInsertGenericMessage(UBYTE *msg, unsigned long long time, int len);
/* Free format message type */

void moniInsertFlasherData(unsigned long long time);  
/* Flasher data (specify argument type later) */

#define NUM_HIST_BINS 128 /* Cannot exceed (MAXMONI_RECSIZE-20)/N where N is the max. number of ASCII
			     characters needed to represent 10 bits per bin, e.g. "1023 " requires 5 
			     chars, so 128 is fine */

void moniZeroAllChargeStampHistos(void);

void moniZeroChargeStampHisto(unsigned short *h, unsigned *entries, int nsamp);

void moniInsertChargeStampHistos(unsigned short *h, unsigned entries,
				 CHARGE_STAMP_MODE_TYPE mode, int ichip, int ichan, int nsamp);

void moniDoChargeStampHistos(void);

/* Configuration state change messages */
void moniInsertSetDACMessage(unsigned long long time, UBYTE dacID, unsigned short dacVal);
void moniInsertSetPMT_HV_Message(unsigned long long time, unsigned short hv);
void moniInsertSetPMT_HV_Limit_Message(unsigned long long time, unsigned short limit);
void moniInsertEnablePMT_HV_Message(unsigned long long time);
void moniInsertDisablePMT_HV_Message(unsigned long long time);
void moniInsertLCModeChangeMessage(unsigned long long time, UBYTE mode);
void moniInsertLCWindowChangeMessage(unsigned long long time,
                                     ULONG pre_ns, ULONG post_ns);
void moniInsertSelfLCModeChangeMessage(unsigned long long time, UBYTE mode);
void moniInsertSelfLCWindowChangeMessage(unsigned long long time, ULONG self_ns);

void moniTestAllMonitorRecords(void);         
/* Generate one of each record */

/* The following are utility functions used by the monitoring code per se */

int  moniGetElementLen(void);                 
/* Returns length of monitoring data */

#endif
