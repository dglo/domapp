/*
  Original Author: Chuck McParland
  Extensions by John Jacobsen (jacobsen@npxdesigns.com)
  Start Date: May 4, 1999
  DOM Experiment Control service to handle
  special run-related functions.  Performs all 
  "standard" DOM service functions.
*/

#include <string.h>
#include <math.h>

/* DOM-related includes */
#include "hal/DOM_MB_types.h"
#include "hal/DOM_MB_hal.h"
#include "hal/DOM_MB_domapp.h"
#include "hal/DOM_FPGA_domapp_regs.h"

#include "DOMtypes.h"
#include "DOMdata.h"
#include "message.h"
#include "expControl.h"
#include "messageAPIstatus.h"
#include "commonServices.h"
#include "commonMessageAPIstatus.h"
#include "EXPmessageAPIstatus.h"
#include "DOMstateInfo.h"
#include "moniDataAccess.h"
#include "dataAccessRoutines.h"
#include "domSControl.h"
#include "unitTests.h"

#include <stdio.h>

/* extern functions */
extern void formatLong(ULONG value, UBYTE *buf);
extern UBYTE   compMode;
extern unsigned nextEvent(unsigned idx);
/* local functions, data */
UBYTE DOM_state;
UBYTE DOM_config_access;
UBYTE DOM_status;
UBYTE DOM_cmdSource;
ULONG DOM_constraints;
char DOM_errorString[DOM_STATE_ERROR_STR_LEN];

/* struct that contains common service info for
	this service. */
COMMON_SERVICE_INFO expctl;

/* Pedestal pattern data */
int pedestalsAvail = 0;
ULONG  atwdpedsum[2][4][ATWDCHSIZ];
USHORT atwdpedavg[2][4][ATWDCHSIZ];
ULONG  fadcpedsum[FADCSIZ];
USHORT fadcpedavg[FADCSIZ];
ULONG npeds0, npeds1, npedsadc;

/* Actual data to be read out */
USHORT atwddata[2][4][ATWDCHSIZ];
USHORT fadcdata[FADCSIZ];

#define ATWD_TIMEOUT_COUNT 4000
#define ATWD_TIMEOUT_USEC 5

#define MAXPEDGOAL 1000 /* MAX # of ATWD triggers (FADCs are twice this) */

void zeroPedestals(void) {
  memset((void *) fadcpedsum, 0, FADCSIZ * sizeof(ULONG));
  memset((void *) fadcpedavg, 0, FADCSIZ * sizeof(USHORT));
  memset((void *) atwdpedsum, 0, 2*4*ATWDCHSIZ*sizeof(ULONG));
  memset((void *) atwdpedavg, 0, 2*4*ATWDCHSIZ*sizeof(USHORT));

  int iatwd; for(iatwd=0;iatwd<2;iatwd++) {
    int ich; for(ich=0; ich<4; ich++) {
      hal_FPGA_DOMAPP_pedestal(iatwd, ich, atwdpedavg[iatwd][ich]);
    }
  }
  pedestalsAvail = 0;
  npeds0 = npeds1 = npedsadc = 0;
}

void dumpRegs(void) {
#define dumpReg(a) (mprintf("%20s=0x%08x", #a, FPGA(a)))
  dumpReg(TRIGGER_SOURCE);
  dumpReg(TRIGGER_SETUP);
  dumpReg(DAQ);
  dumpReg(LBM_CONTROL);
  dumpReg(LBM_POINTER);
  dumpReg(DOM_STATUS);
  dumpReg(SYSTIME_LSB);
  dumpReg(SYSTIME_MSB);
  dumpReg(LC_CONTROL);
  dumpReg(CAL_CONTROL);
  dumpReg(CAL_TIME);
  dumpReg(CAL_LAUNCH);
  dumpReg(CAL_LAST_FLASH_LSB);
  dumpReg(CAL_LAST_FLASH_MSB);
  dumpReg(RATE_CONTROL);
  dumpReg(RATE_SPE);
  dumpReg(RATE_MPE);
  dumpReg(SN_CONTROL);
  dumpReg(SN_DATA);
  dumpReg(INT_EN);
  dumpReg(INT_MASK);
  dumpReg(INT_ACK);
  dumpReg(FL_CONTROL);
  dumpReg(FL_STATUS);
  dumpReg(COMP_CONTROL);
  dumpReg(ATWD_A_01_THRESHOLD);
  dumpReg(ATWD_A_23_THRESHOLD);
  dumpReg(ATWD_B_01_THRESHOLD);
  dumpReg(ATWD_B_23_THRESHOLD);
  dumpReg(PONG);
  dumpReg(FW_DEBUGGING);
  dumpReg(R2R_LADDER);
  dumpReg(ATWD_PEDESTAL);
}

void zeroLBM(void) {
  memset(hal_FPGA_DOMAPP_lbm_address(), 0, WHOLE_LBM_MASK+1);
}

void moniATWDPeds(int iatwd, int ich, int ntrials, int pavg, USHORT * avgs) {
  /* Put ASCII record of pedestals used into monitoring stream.
     There will be 8 records in all (2 ATWDs x 4 channels)
     Pedestal pattern occupies up to N slots in the form "1024 " (4 digits + space) 
     N = 128 * 5 = 640 */

  unsigned char   pedstr[MAXMONI_DATA];
  unsigned char * this = pedstr;

  int is; 

  this += snprintf(this, MAXMONI_DATA-((int) (this-pedstr)), 
		   "PED-ATWD %c CH %d--%d trials, bias %d:",
		   'A'+iatwd, ich, ntrials, pavg);

  for(is = 0; is < ATWDCHSIZ; is++) {
    this += snprintf(this, MAXMONI_DATA-((int) (this-pedstr)), " %hd", avgs[is]);
    if(((int) (this-pedstr)) >= MAXMONI_DATA) break; /* Should never happen */
  }
  mprintf(pedstr);
}

int isContaminated(USHORT *p1, USHORT *p2){
    /* Compare two averaged waveforms, p1 and p2, to assess whether one or both are 
       light-contaminated.  Also check for baseline shift. */

    /* Autocorrelation and baseline shift parameters */
    const int LAG=1;
    const float MAX_C0_CL_RATIO = 3.0; 
    const float MIN_C0 = 10.0;
    const float MAX_BL_SIGMA = 4.0;

    int i,sum=0;
    int a=0, b=0, c=0, d=0;
    int contaminated=0;

    if ((p1 == NULL) || (p2 == NULL)) {
        mprintf("isContaminated: ERROR -- NULL waveform data");
        return 0;
    }

    /* Compute the autocorrelation of the difference of the two waveforms at 
       lags 0 and lag in a single pass through the data. 
       The autocorrelation is a useful measure of light contamination because 
       it will be large at non-zero lags when the difference of waveforms 
       deviates from the baseline in a correlated way from bin to bin, such as 
       having a pulse */
    for(i=0; i<LAG; i++){
        int w=(int)p2[i]-(int)p1[i];
        sum+=w;
        a+=w*w;
        b+=2*w;
    }
    for(i=LAG; i<ATWDCHSIZ; i++){
        int w=(int)p2[i]-(int)p1[i];
        sum+=w;
        a+=w*w;
        b+=2*w;
        c+=w*((int)p2[i-LAG] - (int)p1[i-LAG]);
        d+=w+((int)p2[i-LAG] - (int)p1[i-LAG]);
    }
    float avg=sum/(float)ATWDCHSIZ;
    /* autocorrelation at the non-zero lag */
    float autoCL = c - (d*avg) + (ATWDCHSIZ-LAG)*avg*avg;
    /* autocorrelation at lag 0 */
    float autoC0 = a - (b*avg) + ATWDCHSIZ*avg*avg;

    /* standard deviation of difference */
    float stddev = sqrt(autoC0/ATWDCHSIZ);

    /* significance of non-zero baseline shift */
    float blShift = 0;
    if (stddev > 0)
        blShift = fabs(avg/stddev);

    /* there is a large shift in the average baselines */
    float c0_cl_ratio;
    if (blShift >= MAX_BL_SIGMA) {
        mprintf("isContaminated: baseline shift of %.1f counts is %.1f sigma from zero", 
                avg, blShift); 
        contaminated = 1;
    }
    else {
        /* if there was no correlation at the non-zero lag, 
           the autocorrelation length is short and we assume everything is fine */
        if (autoCL==0.0) {
            contaminated = 0;
        }
        else{            
            /* the autocorrelation at lag 0 is always non-negative, 
               but at other lags it may be negative */
            if(autoCL<0.0)
                autoCL=-autoCL;
            /* if the autocorrelation at lag L is too large relative to the 
               autocorrelation at lag 0 there is probably a substantial departure
               from baseline */
            c0_cl_ratio = autoC0/autoCL;
            contaminated = (c0_cl_ratio < MAX_C0_CL_RATIO) && (autoC0 > MIN_C0);
            if (contaminated)
                mprintf("isContaminated: pulse detected in baseline (autoCL %.2f autoC0 %.2f ratio %.2f)", autoCL, autoC0, c0_cl_ratio);
        }
    }
    return contaminated;
}

BOOLEAN pedestalEventOk(unsigned char *e, int resetEventWarnings) {
    /* Sanity check on events collected during a pedestal run.  */
    /* Returns TRUE if all checks pass, FALSE otherwise.  Argument */
    /* is a pointer to the event in the LBM. */
   
    static int didUncompressedWarning = 0;  
    static int didATWDSizeWarning     = 0;
    static int didMissingFADCWarning  = 0;
    static int didMissingATWDWarning  = 0;
    
    if (e == NULL) {
        mprintf("pedestalEventOk: WARNING: trying to check a NULL event!");
        dumpRegs();
        return FALSE;
    }

    if (resetEventWarnings)
        didUncompressedWarning = didATWDSizeWarning = didMissingFADCWarning = didMissingATWDWarning = 0;
    
    struct tstevt * hdr = (struct tstevt *) e;
    struct tstwords {
        unsigned long w0, w1, w2, w3;
    } * words = (struct tstwords *) e;
    unsigned atwdsize = (hdr->trigbits>>19)&0x3;
    unsigned long daq = FPGA(DAQ);

    if (words->w0>>31) {
        if(!didUncompressedWarning) {
            didUncompressedWarning++;
            mprintf("pedestalEventOk: WARNING: trying to collect waveforms for pedestals but "
                    "got COMPRESSED data!  w0=0x%08x w1=0x%08x w2=0x%08x w3=0x%08x DAQ=0x%08x",
                    words->w0, words->w1, words->w2, words->w3, daq);
            dumpRegs();
        }
        return FALSE;
    } 
    if (atwdsize != 3) {
        if(!didATWDSizeWarning) {
            didATWDSizeWarning++;
            mprintf("pedestalEventOk: WARNING: atwdsize=%d should be 3!  "
                    "w0=0x%08x w1=0x%08x w2=0x%08x w3=0x%08x DAQ=0x%08x",
                    atwdsize, words->w0, words->w1, words->w2, words->w3, daq);
            dumpRegs();
        }
        return FALSE;
    }    
    /* Check valid FADC data present */
    if (!(hdr->trigbits & 1<<17)) {
        if(!didMissingFADCWarning) {
            didMissingFADCWarning++;
            mprintf("pedestalEventOk: WARNING: NO FADC data present in event!  trigbits=0x%08x", 
                    hdr->trigbits);
            dumpRegs();
        }
        return FALSE;
    }    
    /* Check valid ATWD data present */
    if (!(hdr->trigbits & 1<<18)) {
        if(!didMissingATWDWarning) {
            didMissingATWDWarning++;
            mprintf("pedestalEventOk: WARNING: NO ATWD data present in event!  trigbits=0x%08x", 
                    hdr->trigbits);
            dumpRegs();
        }
        return FALSE;
    }
    return TRUE;
}

int pedestalRun(ULONG ped0goal, ULONG ped1goal, ULONG pedadcgoal,
                BOOLEAN setbias, USHORT *biases) {
    /* return 0 if pedestal run succeeds, else error.  If setbias is TRUE,
       program in the given bias offsets to the baselines; otherwise, dynamically
       determine the correct offset. */
    
    mprintf("Starting pedestal run...");
    
    dsc_hal_disable_LC_completely();
    hal_FPGA_DOMAPP_disable_daq();
    hal_FPGA_DOMAPP_lbm_reset();
    halUSleep(1000); /* make sure atwd is done... */
    hal_FPGA_DOMAPP_lbm_reset();
    zeroLBM();
    unsigned lbmp = hal_FPGA_DOMAPP_lbm_pointer();
    hal_FPGA_DOMAPP_trigger_source(HAL_FPGA_DOMAPP_TRIGGER_FORCED);
    hal_FPGA_DOMAPP_cal_source(HAL_FPGA_DOMAPP_CAL_SOURCE_FORCED);
    hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_FORCED);
    hal_FPGA_DOMAPP_daq_mode(HAL_FPGA_DOMAPP_DAQ_MODE_ATWD_FADC);
    hal_FPGA_DOMAPP_atwd_mode(HAL_FPGA_DOMAPP_ATWD_MODE_TESTING);
    hal_FPGA_DOMAPP_lc_mode(HAL_FPGA_DOMAPP_LC_MODE_OFF);
    hal_FPGA_DOMAPP_lbm_mode(HAL_FPGA_DOMAPP_LBM_MODE_WRAP);
    hal_FPGA_DOMAPP_compression_mode(HAL_FPGA_DOMAPP_COMPRESSION_MODE_OFF);

    hal_FPGA_DOMAPP_rate_monitor_enable(0);
    int dochecks               = 1;
    int didMissedTrigWarning   = 0;
    int resetEventWarnings     = 1;
    npeds0 = npeds1 = npedsadc = 0;            
    pedestalsAvail             = 0;

    /* Last pedestal averages for light-contamination / consistency check */
    USHORT lastPedestal[2][4][ATWDCHSIZ];

    int maxcontaminated = 10; /* Maximum trials to get a non-light-contaminated average */

    int avgtrial=0;
    int avgdone=0;
    int iatwd, ich, isamp;
    int numMissedTriggers0, numMissedTriggers1;

    /* Acquisition of the average pedestal. We check a pair of averages after the fact for light
       contamination using the autocorrelation of their difference.  If no contamination is 
       detected, the last average is used. */ 
    for (avgtrial=0; !avgdone && avgtrial<maxcontaminated; avgtrial++) {

        zeroPedestals();
        numMissedTriggers0 = numMissedTriggers1 = 0;

        for(iatwd=0;iatwd<2;iatwd++) {

            hal_FPGA_DOMAPP_enable_atwds(iatwd==0?HAL_FPGA_DOMAPP_ATWD_A:HAL_FPGA_DOMAPP_ATWD_B);
            hal_FPGA_DOMAPP_enable_daq(); 

            int numTrigs = (iatwd==0 ? ped0goal : ped1goal);      
            int extraTrigs = 10; /* Pre-scan ATWD before collecting pedestals for average */
            int npeds=0;

            int trial,
                maxtrials = 10,
                trialtime = 40, /* usec */
                done      = 0;
    
            int it; for(it=0; it<numTrigs+extraTrigs; it++) {
                hal_FPGA_DOMAPP_cal_launch();
                done = 0;
                for(trial=0; !done && trial<maxtrials; trial++) {
                    halUSleep(trialtime); /* Give FPGA time to write LBM without CPU hogging the bus */
                    if(hal_FPGA_DOMAPP_lbm_pointer() >= lbmp+FPGA_DOMAPP_LBM_BLOCKSIZE) done = 1;
                }

                if(!done) {
                    if (iatwd==0) numMissedTriggers0++; else numMissedTriggers1++;
                    if(!didMissedTrigWarning) {
                        didMissedTrigWarning++;
                        mprintf("pedestalRun: WARNING: missed one or more calibration triggers for ATWD %d! "
                                "lbmp=0x%08x fpga_ptr=0x%08x", iatwd, lbmp, hal_FPGA_DOMAPP_lbm_pointer());
                        dumpRegs();
                    }
                    continue;
                }

                /* Check that the event is sane, if checks are enabled */
                unsigned char * e = lbmEvent(lbmp);
                if (dochecks && !pedestalEventOk(e, resetEventWarnings)) {
                    lbmp = nextEvent(lbmp);
                    resetEventWarnings = 0;
                    if (iatwd==0) numMissedTriggers0++; else numMissedTriggers1++;
                    continue;                    
                }

                /* Ignore otherwise successful prescan triggers */ 
                if (it < extraTrigs) {
                    lbmp = nextEvent(lbmp);
                    continue;
                }

                /* Pull out FADC and ATWD data */
                unsigned short * fadc = (unsigned short *) (e+0x10);
                unsigned short * atwd = (unsigned short *) (e+0x210);

                /* Count the waveforms into the running sums */
                for(ich=0; ich<4; ich++)
                    for(isamp=0; isamp<ATWDCHSIZ; isamp++) 
                        atwdpedsum[iatwd][ich][isamp] += atwd[ATWDCHSIZ*ich + isamp] & 0x3FF;
                
                for(isamp=0; isamp<FADCSIZ; isamp++) fadcpedsum[isamp] += fadc[isamp] & 0x3FF; 
                
                if(iatwd==0) npeds0++; else npeds1++;
                npedsadc++;
                lbmp = nextEvent(lbmp);
            } /* loop over triggers */

            npeds = iatwd==0 ? npeds0 : npeds1;
            for(ich=0; ich<4; ich++) {
                for(isamp=0; isamp<ATWDCHSIZ; isamp++) {
                    if(npeds > 0) {
                        /* Calculate round-to-nearest average with integer math */
                        atwdpedavg[iatwd][ich][isamp] = (atwdpedsum[iatwd][ich][isamp] + (npeds/2))/npeds;
                    } else {
                        atwdpedavg[iatwd][ich][isamp] = 0;
                    }
                }
            }
        } /* ATWD Loop */

        /* Now check waveforms for light contamination and baseline consistency */
        if (avgtrial > 0) {
            int contaminated = 0;
            if ((npeds0 != 0) && (npeds1 != 0)) {
                for (iatwd=0; iatwd<2; iatwd++) {                
                    /* Check channels 0, 1, and 2 */
                    for (ich=0; ich<3; ich++) {
                        contaminated = contaminated || 
                            isContaminated(atwdpedavg[iatwd][ich], lastPedestal[iatwd][ich]);
                    }
                }
                avgdone = !contaminated;
            }
            
            if (!avgdone) {
                mprintf("pedestalRun: WARNING: pedestal average contaminated; trying again (trial %d)", avgtrial);
                halUSleep(1000); /* settle down a bit */
            }
        }

        /* Save averages for contamination / consistency check */
        for (iatwd=0; iatwd<2; iatwd++) 
            for (ich=0; ich<4; ich++)
                for(isamp=0; isamp<ATWDCHSIZ; isamp++)
                    lastPedestal[iatwd][ich][isamp] = atwdpedavg[iatwd][ich][isamp];

    } /* Light contamination loop */

    if (!avgdone)
        mprintf("pedestalRun: WARNING: continuing with possibly light-contaminated pedestal average!");

    /* Now program the results into the FPGA */
    for(iatwd=0; iatwd<2; iatwd++) {
        int npeds = iatwd==0 ? npeds0 : npeds1;
        int numMissedTriggers = iatwd==0 ? numMissedTriggers0 : numMissedTriggers1;

        for(ich=0; ich<4; ich++) {
            /* 
             * Take out any average of the average pedestal - really we
             * only want to reduce the ATWD fingerprint and are not too
             * concerned about lingering constant biases (actually we
             * need this bias to keep the output from underflowing and
             * clipping.).
             * JEJ 6/3/2011: allow programmable average/bias to be set from surface
             */
            int i, pavg = 0;
            if(setbias && ich<3) {
                pavg = biases[iatwd*3+ich];
            } else { /* Channel 3 (clock/mux) bias not programmable */
                for (i = 0; i < ATWDCHSIZ; i++) pavg += atwdpedavg[iatwd][ich][i];
                pavg /= ATWDCHSIZ;
            }
            for (i = 0; i < ATWDCHSIZ; i++) atwdpedavg[iatwd][ich][i] -= pavg;
        
            /* Program ATWD pedestal pattern into FPGA */
            hal_FPGA_DOMAPP_pedestal(iatwd, ich, atwdpedavg[iatwd][ich]);
            moniATWDPeds(iatwd, ich, npeds, pavg, atwdpedavg[iatwd][ich]);
        }    
        for(isamp=0; isamp<FADCSIZ; isamp++) {
            if(npedsadc > 0) {
                fadcpedavg[isamp] = fadcpedsum[isamp]/npedsadc;
            } else {
                fadcpedavg[isamp] = 0;
            }
            /* Currently there is no setting of FADC pedestals in the FPGA! */
        }               
        mprintf("Number of pedestal triggers for ATWD %d is %d (%s, missed triggers=%d)", iatwd, npeds,
                dochecks ? "checks were enabled" : "WARNING: checks were DISABLED", numMissedTriggers);
    }

    hal_FPGA_DOMAPP_disable_daq();
    hal_FPGA_DOMAPP_cal_mode(HAL_FPGA_DOMAPP_CAL_MODE_OFF);

    if(npeds0 > 0 && npeds1 > 0 && npedsadc > 0) pedestalsAvail = 1;
    
    return 0;
}

/* Exp Control  Entry Point */
void expControlInit(void) {
    expctl.state=SERVICE_ONLINE;
    expctl.lastErrorID=COMMON_No_Errors;
    expctl.lastErrorSeverity=INFORM_ERROR;
    expctl.majorVersion=EXP_MAJOR_VERSION;
    expctl.minorVersion=EXP_MINOR_VERSION;
    strcpy(expctl.lastErrorStr,EXP_ERS_NO_ERRORS);
    expctl.msgReceived=0;
    expctl.msgRefused=0;
    expctl.msgProcessingErr=0;

    /* get DOM state lock and initialize state info */
    DOM_state=DOM_IDLE;
    DOM_config_access=DOM_CONFIG_CHANGES_ALLOWED;
    DOM_status=DOM_STATE_SUCCESS;
    DOM_cmdSource=EXPERIMENT_CONTROL;
    DOM_constraints=DOM_CONSTRAINT_NO_CONSTRAINTS;
    strcpy(DOM_errorString,DOM_STATE_STR_STARTUP);

    zeroPedestals();
}      

#define DOERROR(errstr, errnum, errtype)                       \
   do {                                                        \
      expctl.msgProcessingErr++;                               \
      strcpy(expctl.lastErrorStr,(errstr));                    \
      expctl.lastErrorID=(errnum);                             \
      expctl.lastErrorSeverity=errtype;                        \
      Message_setStatus(M,SERVICE_SPECIFIC_ERROR|errtype);     \
      Message_setDataLen(M,0);                                 \
   } while(0)

void expControl(MESSAGE_STRUCT *M) {
    UBYTE *data;
    UBYTE *tmpPtr;
    
    /* get address of data portion. */
    /* Receiver ALWAYS links a message */
    /* to a valid data buffer-even */ 
    /* if it is empty. */
    data=Message_getData(M);
    expctl.msgReceived++;
    switch ( Message_getSubtype(M) ) {
      /* Manditory Service SubTypes */
    case GET_SERVICE_STATE:
      /* get current state of Test Manager */
      data[0]=expctl.state;
      Message_setDataLen(M,GET_SERVICE_STATE_LEN);
      Message_setStatus(M,SUCCESS);
      break;
      
    case GET_LAST_ERROR_ID:
      /* get the ID of the last error encountered */
      data[0]=expctl.lastErrorID;
      data[1]=expctl.lastErrorSeverity;
      Message_setDataLen(M,GET_LAST_ERROR_ID_LEN);
      Message_setStatus(M,SUCCESS);
      break;
      
    case GET_SERVICE_VERSION_INFO:
      /* get the major and minor version of this */
      /*	Test Manager */
      data[0]=expctl.majorVersion;
      data[1]=expctl.minorVersion;
      Message_setDataLen(M,GET_SERVICE_VERSION_INFO_LEN);
      Message_setStatus(M,SUCCESS);
      break;
      
    case GET_SERVICE_STATS:
      /* get standard service statistics for */
      /*	the Test Manager */
      formatLong(expctl.msgReceived,&data[0]);
      formatLong(expctl.msgRefused,&data[4]);
      formatLong(expctl.msgProcessingErr,&data[8]);
      Message_setDataLen(M,GET_SERVICE_STATS_LEN);
      Message_setStatus(M,SUCCESS);
      break;
      
    case GET_LAST_ERROR_STR:
      /* get error string for last error encountered */
      strcpy(data,expctl.lastErrorStr);
      Message_setDataLen(M,strlen(expctl.lastErrorStr));
      Message_setStatus(M,SUCCESS);
      break;
      
    case CLEAR_LAST_ERROR:
      /* reset both last error ID and string */
      expctl.lastErrorID=COMMON_No_Errors;
      expctl.lastErrorSeverity=INFORM_ERROR;
      strcpy(expctl.lastErrorStr,EXP_ERS_NO_ERRORS);
      Message_setDataLen(M,0);
      Message_setStatus(M,SUCCESS);
      break;
      
    case GET_SERVICE_SUMMARY:
      /* init a temporary buffer pointer */
      tmpPtr=data;
      /* get current state of slow control */
      *tmpPtr++=expctl.state;
      /* get the ID of the last error encountered */
      *tmpPtr++=expctl.lastErrorID;
      *tmpPtr++=expctl.lastErrorSeverity;
      /* get the major and minor version of this */
      /*	exp control */
      *tmpPtr++=expctl.majorVersion;
      *tmpPtr++=expctl.minorVersion;
      /* get standard service statistics for */
      /*	the exp control */
      formatLong(expctl.msgReceived,tmpPtr);
      tmpPtr+=sizeof(ULONG);
      formatLong(expctl.msgRefused,tmpPtr);
      tmpPtr+=sizeof(ULONG);
      formatLong(expctl.msgProcessingErr,tmpPtr);
      tmpPtr+=sizeof(ULONG);
      /* get error string for last error encountered */
      strcpy(tmpPtr,expctl.lastErrorStr);
      Message_setDataLen(M,(int)(tmpPtr-data)+
			 strlen(expctl.lastErrorStr));
      Message_setStatus(M,SUCCESS);
      break;
      
      /* begin run */ 
    case EXPCONTROL_BEGIN_RUN:
      turnOffFlashers();
      if (!beginRun(compMode, DOM_RUN_IN_PROGRESS)) {
	DOERROR(EXP_CANNOT_BEGIN_RUN, EXP_Cannot_Begin_Run, SEVERE_ERROR);
	break;
      }
      Message_setStatus(M,SUCCESS);
      Message_setDataLen(M,0);
      break;
      
    case EXPCONTROL_BEGIN_FB_RUN:
      {
	USHORT bright=0, window=0, mask=0, rate=0;
	short delay=0;
	bright = unformatShort(&data[0]);
	window = unformatShort(&data[2]);
	delay  = unformatShort(&data[4]);
	if(delay < -200 || delay > 175) {
	  DOERROR("EXPCONTROL_BEGIN_FB_RUN: Flasher delay must be -200 to 175!",
		  EXP_Bad_FB_Delay, SEVERE_ERROR);
	  break;
	}
	mask   = unformatShort(&data[6]);
	rate   = unformatShort(&data[8]);
	if (!beginFBRun(compMode, bright, window, delay, mask, rate)) {
	  DOERROR(EXP_CANNOT_BEGIN_FB_RUN, EXP_Cannot_Begin_FB_Run, SEVERE_ERROR);
	  break;
	}
	Message_setStatus(M,SUCCESS);
	Message_setDataLen(M,0);
	break;
      }
      /* end run */ 

    case EXPCONTROL_CHANGE_FB_SETTINGS: 
      {
	USHORT bright=0, window=0, mask=0, rate=0;
	short delay=0;
	bright = unformatShort(&data[0]);
	window = unformatShort(&data[2]);
	delay  = unformatShort(&data[4]);
	if(delay < -200 || delay > 175) {
	  DOERROR("EXPCONTROL_CHANGE_FB_SETTINGS: Flasher delay must be -200 to 175!",
		  EXP_Bad_FB_Delay, SEVERE_ERROR);
	  break;
	}
	mask   = unformatShort(&data[6]);
	rate   = unformatShort(&data[8]);
	if (!changeFBsettings(bright, window, delay, mask, rate)) {
	  DOERROR("EXPCONTROL_CHANGE_FB_SETTINGS: Could not change flasherboard settings",
		  EXP_Cannot_Change_FB_Settings, SEVERE_ERROR);
	  break;
	}
	Message_setStatus(M,SUCCESS);
	Message_setDataLen(M,0);
	break;
      }

    case EXPCONTROL_END_RUN:
      if (!endRun()) {
	DOERROR(EXP_CANNOT_END_RUN, EXP_Cannot_End_Run, SEVERE_ERROR);
	break;
      }
      Message_setStatus(M,SUCCESS);
      Message_setDataLen(M,0);
      break;

    case EXPCONTROL_END_FB_RUN:
      if (!endFBRun()) {
	DOERROR(EXP_CANNOT_END_FB_RUN, EXP_Cannot_End_FB_Run, SEVERE_ERROR);
	break;
      }
      Message_setStatus(M,SUCCESS);
      Message_setDataLen(M,0);
      break;
      
    /* get run state */
    case EXPCONTROL_GET_DOM_STATE:
      data[0]=DOM_state;
      data[1]=DOM_config_access;
      data[2]=DOM_status;
      data[3]=DOM_cmdSource;
      formatLong(DOM_constraints,&data[4]);
      strcpy(&data[8],DOM_errorString);
      Message_setStatus(M,SUCCESS);
      Message_setDataLen(M,strlen(DOM_errorString)+8);
      break;
      
    case EXPCONTROL_DO_PEDESTAL_COLLECTION: 
      {
	ULONG ped0goal   = unformatLong(&data[0]);
	ULONG ped1goal   = unformatLong(&data[4]);
	ULONG pedadcgoal = unformatLong(&data[8]);
	BOOLEAN setbias = FALSE;
	USHORT biases[6];
	if(Message_dataLen(M) > 12) {
	  setbias = TRUE;
	  int i; for(i=0; i<6; i++) {
	    biases[i] = unformatShort(&data[i*2+12]);
	  }
	}
	zeroPedestals();
	if(ped0goal   > MAXPEDGOAL ||
	   ped1goal   > MAXPEDGOAL ||
	   pedadcgoal > ped0goal + ped1goal) {
	  DOERROR(EXP_TOO_MANY_PEDS, EXP_Too_Many_Peds, SEVERE_ERROR);
	  break;
	}
	if(pedestalRun(ped0goal, ped1goal, pedadcgoal, setbias, biases)) {
	  DOERROR(EXP_PEDESTAL_RUN_FAILED, EXP_Pedestal_Run_Failed, SEVERE_ERROR);
	  break;
	}
#ifdef  DEBUGLBM
#warning DEBUGLBM set!
	if(pedestalRun(ped0goal, ped1goal, pedadcgoal, setbias, biases)) {
	  DOERROR(EXP_PEDESTAL_RUN_FAILED, EXP_Pedestal_Run_Failed, SEVERE_ERROR);
	  break;
	}
#endif

	Message_setDataLen(M,0);
	Message_setStatus(M,SUCCESS);
	break;
      }

    case EXPCONTROL_GET_NUM_PEDESTALS:
      formatLong(npeds0, &data[0]);
      formatLong(npeds1, &data[4]);
      formatLong(npedsadc, &data[8]);
      Message_setStatus(M,SUCCESS);
      Message_setDataLen(M,12);
      break;

    case EXPCONTROL_GET_PEDESTAL_AVERAGES:
      if(!pedestalsAvail) {
	DOERROR(EXP_PEDESTALS_NOT_AVAIL, EXP_Pedestals_Not_Avail, SEVERE_ERROR);
	break;
      }
      /* else we're good to go */
      int ichip, ich, isamp;
      int of = 0;
      for(ichip=0;ichip<2;ichip++) 
	for(ich=0;ich<4;ich++) 
	  for(isamp=0;isamp<ATWDCHSIZ;isamp++) {
	    formatShort(atwdpedavg[ichip][ich][isamp], data+of);
	    of += 2;
	  }
      for(isamp=0;isamp<FADCSIZ;isamp++) {
	formatShort(fadcpedavg[isamp], data+of);
	of += 2;
      }
      Message_setDataLen(M,of);
      Message_setStatus(M,SUCCESS);
      break;

    case EXPCONTROL_RUN_UNIT_TESTS:
      if(unit_tests()) {
	Message_setStatus(M,SUCCESS);
      } else {
	DOERROR("Unit test failure", EXP_Unit_Test_Failure, WARNING_ERROR);
      }
      break;

    default:
      DOERROR(EXP_ERS_BAD_MSG_SUBTYPE, COMMON_Bad_Msg_Subtype, WARNING_ERROR);
      break;      
    }
}

