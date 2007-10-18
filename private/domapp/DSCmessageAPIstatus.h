/* DSCmessageAPIstatus.h */
#ifndef _DSC_MESSAGE_API_STATUS_H_
#define _DSC_MESSAGE_API_STATUS_H_

/* This file contains subtype values for the 
   DAQ<=>DOM messaging API used by the Slow
   Control service.  

   March 7, 2003
   Chuck McParland 

   Mods by Jacobsen in 2004 to accommodate new messages */

   

/* message API subtype values. This field is not
   declared as an enum because each service declares
   values in addion to those in MessageAPIstatus.h.
   Since enums are typedefs, odd compiler problems 
   will ensue.  

   Note 255 is the maximum value available for this field. */ 
#define	DSC_READ_ALL_ADCS         10
#define	DSC_READ_ONE_ADC          11
#define DSC_READ_ALL_DACS         12
#define DSC_WRITE_ONE_DAC         13
#define DSC_SET_PMT_HV            14
#define DSC_ENABLE_PMT_HV         16
#define DSC_DISABLE_PMT_HV        18
#define DSC_QUERY_PMT_HV          22
#define DSC_READ_ONE_ADC_REPT     27
#define DSC_GET_PMT_HV_LIMIT      28
#define DSC_SET_PMT_HV_LIMIT      29
#define DSC_READ_ONE_DAC          30
#define DSC_SET_TRIG_MODE         31
#define DSC_GET_TRIG_MODE         32
#define DSC_SELECT_ATWD           33
#define DSC_WHICH_ATWD            34
#define DSC_MUX_SELECT            35
#define DSC_WHICH_MUX             36
#define DSC_SET_PULSER_RATE       37
#define DSC_GET_PULSER_RATE       38
#define DSC_SET_PULSER_ON         39
#define DSC_SET_PULSER_OFF        40
#define DSC_PULSER_RUNNING        41
#define DSC_GET_RATE_METERS       42
#define DSC_SET_SCALER_DEADTIME   43
#define DSC_GET_SCALER_DEADTIME   44
#define DSC_SET_LOCAL_COIN_MODE   45
#define DSC_GET_LOCAL_COIN_MODE   46
#define DSC_SET_LOCAL_COIN_WINDOW 47
#define DSC_GET_LOCAL_COIN_WINDOW 48
#define DSC_SET_LC_TYPE           49
#define DSC_GET_LC_TYPE           50
#define DSC_SET_LC_TX             51
#define DSC_GET_LC_TX             52
#define DSC_SET_LC_SRC            53
#define DSC_GET_LC_SRC            54
#define DSC_SET_LC_SPAN           55
#define DSC_GET_LC_SPAN           56
#define DSC_SET_LC_CABLE_LEN      57
#define DSC_GET_LC_CABLE_LEN      58
#define DSC_ENABLE_SN             59
#define DSC_DISABLE_SN            60
#define DSC_SET_CHARGE_STAMP_TYPE 61

/* Slow Control specific Last error ID values */
#define DSC_Failed_Challenge 4
#define DSC_Illegal_ADC_Channel 5
#define DSC_Illegal_DAC_Channel 6
#define DSC_FPGA_not_loaded 7
#define DSC_FPGA_file_not_found 8
#define DSC_FPGA_wrong_file_type 9
#define DSC_FPGA_load_failed 10
#define DSC_FPGA_already_loaded 11
#define DSC_PMT_HV_request_too_high 12
#define DSC_bad_DOM_state 13
#define DSC_unknown_error 14
#define DSC_bgnd_task_unresponsive 15
#define DSC_FPGA_config_error 16
#define DSC_bad_task_cmd 17
#define DSC_violates_constraints 18
#define DSC_rate_meters_unresponsive 19
#define DSC_bad_flasher_param 20
#define DSC_Illegal_LC_Mode 21
#define DSC_LC_Window_Fail 22
#define DSC_LC_Bad_Type 33
#define DSC_LC_Bad_Tx 34
#define DSC_LC_Bad_Src 35
#define DSC_LC_Bad_Span 36
#define DSC_LC_Bad_Len 37
#define DSC_LC_Len_Not_Set 38
#define DSC_bad_deadtime 39
#define DSC_SN_Already_Running 40
#define DSC_Bad_SN_Mode 41
#define DSC_Bad_SN_Deadtime 42
#define DSC_SN_Not_Running 43
#define DSC_SN_Start_Failed 44
#define DSC_SN_DataTakingInProgress 45

/* These are DOM Slow Control specific return message
   formats and values.  In most cases, they are formatted
   as byte values to eliminate any questions of
   "endian-ness".  Note that remote object accesses
   (subtype=REMOTE_OBJECT_REF) will have a format
   determined by that additional protocol.  These
   formats describe "all the rest". */

/* Response to: 
	subType: DSC_READ_ALL_ADCS
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	USHORT ADC_VOLTAGE_SUM
	USHORT ADC_5V_POWER_SUPPLY
	USHORT ADC_PRESSURE
	USHORT ADC_5V_CURRENT
	USHORT ADC_3_3V_CURRENT
	USHORT ADC_2_5V_CURRENT
	USHORT ADC_1_8V_CURRENT
	USHORT ADC_MINUS_5V_CURRENT
   Size of returned values in data portion: */
#define DSC_READ_ALL_ADCS_LEN 16

/* Response to: 
	subType: DSC_READ_ONE_ADC
   Passed values:
	UBYTE ADC ID
   Size of passed values: */
#define DSC_READ_ONE_ADC_REQ_LEN 1  
/* Returned values in data portion of message:
	USHORT ADC value
   Size of returned values in data portion: */
#define DSC_READ_ONE_ADC_LEN 2
/* Last Error ID for failed invocation 
#define DSC_Illegal_ADC_Channel 5 */

/* Response to: 
	subType: DSC_READ_ALL_DACS
   Passed values:
	none
   Size of passed values:
	0  
   Returned values in data portion of message:
	USHORT DAC_ATWD0_TRIGGER_BIAS
	USHORT DAC_ATWD0_RAMP_TOP
	USHORT DAC_ATWD0_RAMP_RATE
	USHORT DAC_ATWD_ANALOG_REF
	USHORT DAC_ATWD1_TRIGGER_BIAS
	USHORT DAC_ATWD1_RAMP_TOP
	USHORT DAC_ATWD1_RAMP_RATE
	USHORT DAC_PMT_FE_PEDESTAL
	USHORT DAC_MULTIPLE_SPE_THRESH
	USHORT DAC_SINGLE_SPE_THRESH
	USHORT DAC_LED_BRIGHTNESS
	USHORT DAC_FAST_ADC_REF
	USHORT DAC_INTERNAL_PULSER
	USHORT DAC_FE_AMP_LOWER_CLAMP
	USHORT DAC_SPARE_DAC_0
	USHORT DAC_SPARE_DAC_1
   Size of returned values in data portion: */
#define DSC_READ_ALL_DACS_LEN 32

/* Response to: 
	subType: DSC_WRITE_ONE_DAC
   Passed values:
	UBYTE DAC ID
	UBYTE spare	
	USHORT DAC value
   Size of passed values: */
#define DSC_WRITE_ONE_DAC_REQ_LEN 4  
/* Returned values in data portion of message:
	none
   Size of returned values in data portion:
	0
   Last Error ID for failed invocation 
#define DSC_Illegal_DAC_Channel 6 */

/* Response to: 
	subType: DSC_SET_PMT_HV
   Passed values:
	USHORT ADC_PMT_HV
   Size of passed values: */
#define DSC_SET_PMT_HV_REQ_LEN 2  
/* Returned values in data portion of message:
	none
   Size of returned values in data portion: 
	0	*/

/* Response to: 
	subType: DSC_ENABLE_PMT_HV
   Passed values:
	none
   Size of passed values: 
	0
   Returned values in data portion of message:
	none
   Size of returned values in data portion: 
	0	*/

/* Response to: 
	subType: DSC_DISABLE_PMT_HV
   Passed values:
	none
   Size of passed values: 
	0
   Returned values in data portion of message:
	none
   Size of returned values in data portion: 
	0				*/

/* Response to: 
	subType: DSC_QUERY_PMT_HV
   Passed values:
	none
   Size of passed values: 
	0
   Returned values in data portion of message:
	USHORT PMT_ADC
	USHORT PMT_DAC
   Size of returned values in data portion: */ 
#define DSC_QUERY_PMT_HV_LEN 	4

/* Response to: 
	subType: DSC_READ_ONE_ADC_REPT
   Passed values:
	UBYTE ADC ID
	UBYTE msec between reads
   Size of passed values: */
#define DSC_READ_ONE_ADC_REPT_REQ_LEN 2  
/* Returned values in data portion of message:
	UBYTE ADC ID
	UBYTE msec between reads
	USHORT Number of Samples
	ARRAY OF USHORT ADC values
Size of returned values in data portion: 
	computed */
/* Last Error ID for failed invocation 
#define DSC_Illegal_ADC_Channel 5 */

/* Response to: 
	subType: DSC_SET_PMT_HV_LIMIT
   Passed values:
	USHORT PMT_DAC_MAX
   Size of passed values: */
#define DSC_SET_PMT_HV_LIMIT_REQ_LEN 2  
/* Returned values in data portion of message:
	none
   Size of returned values in data portion: 
	0 */

/* Response to: 
	subType: DSC_GET_PMT_HV_LIMIT
   Passed values:
	none
   Size of passed values: 
	0 */
/* Returned values in data portion of message:
	USHORT PMT_DAC_MAX
   Size of returned values in data portion: */
#define DSC_GET_PMT_HV_LIMIT_LEN 2 
	
/* Response to: 
	subType: DSC_READ_ONE_DAC
   Passed values:
	UBYTE DAC ID
   Size of passed values: */
#define DSC_READ_ONE_DAC_REQ_LEN 1  
/* Returned values in data portion of message:
	USHORT DAC value
   Size of returned values in data portion: */
#define DSC_READ_ONE_DAC_LEN 2
/* Last Error ID for failed invocation 
#define DSC_Illegal_DAC_Channel 5 */

/* Response to:
	subType: DSC_SET_TRIG_MODE
   Passed values:
        UBYTE TRIGGER_MODE
	values: */
#define TEST_PATTERN_TRIG_MODE 0
#define CPU_TRIG_MODE          1
#define SPE_DISC_TRIG_MODE     2
#define FB_TRIG_MODE           3
#define MPE_DISC_TRIG_MODE     4

/* Size of passed values: */
#define DSC_SET_TRIG_MODE_REQ_LEN 1
/* Returned values in data portion of message:
        none
   Size of returned values in data portion: 
	0 */

/* Response to:
        subType: DSC_GET_TRIG_MODE
   Passed values:
        UBYTE TRIGGER_MODE
	see DSC_SET_TRIG_MODE
   Size of passed values: */
/* Returned values in data portion of message:
        UBYTE TRIGGER_MODE
   Size of returned values in data portion: */
#define DSC_GET_TRIG_MODE_LEN 1
 
/* Response to:
        subType: DSC_SELECT_ATWD
   Passed values:
        UBYTE ATWD
   Size of passed values: */
/* Returned values in data portion of message:
	none
   Size of returned values in data portion: */

/* Response to:
        subType: DSC_WHICH_ATWD
   Passed values:
	none
   Size of passed values: */
/* Returned values in data portion of message:
        UBYTE ATWD
   Size of returned values in data portion: */
#define DSC_WHICH_ATWD_LEN 1

/* Response to:
        subType: DSC_MUX_SELECT
   Passed values:
	UBYTE MUX_CHANNEL
   Size of passed values: */
#define DSC_MUX_SELECT_REQ_LEN 1
/* Returned values in data portion of message:
        none
   Size of returned values in data portion: */

/* Response to:
        subType: DSC_WHICH_MUX
   Passed values:
	none
   Size of passed values: */
/* Returned values in data portion of message:
        UBYTE MUX_CHANNEL
   Size of returned values in data portion: */
#define DSC_WHICH_MUX_LEN 1

/* Response to:
        subType: DSC_SET_PULSER_RATE
   Passed values:
	USHORT PULSER_RATE
   Size of passed values: */
#define DSC_SET_PULSER_RATE_REQ_LEN 2
/* Returned values in data portion of message:
        none
   Size of returned values in data portion: */

/* Response to:
        subType: DSC_GET_PULSER_RATE
   Passed values:
	none
   Size of passed values: */
/* Returned values in data portion of message:
	USHORT PULSER_RATE
   Size of returned values in data portion: */
#define DSC_GET_PULSER_RATE_LEN 2

/* Response to:
        subType: DSC_SET_PULSER_ON
   Passed values:
	none
   Size of passed values: */
/* Returned values in data portion of message:
        nono
   Size of returned values in data portion: */

/* Response to:
        subType: DSC_SET_PULSER_OFF
   Passed values:
	none
   Size of passed values: */
/* Returned values in data portion of message:
   Size of returned values in data portion: */

/* Response to:
        subType: DSC_PULSER_RUNNING
   Passed values:
	none
   Size of passed values: */
/* Returned values in data portion of message:
        UBYTE PULSER_RUNNING
   Size of returned values in data portion: */
#define DSC_PULSER_RUNNING_LEN 1

/* Response to:
        subType: DSC_GET_RATE_METERS
   Passed values:
	none
   Size of passed values: */
/* Returned values in data portion of message:
        ULONG SPE_RATE_PER_TENTH_SEC
        ULONG MPE_RATE_PER_TENTH_SEC
   Size of returned values in data portion: */
#define DSC_GET_RATE_METERS_LEN 8

/* Response to:
        subType: DSC_SET_SCALER_DEADTIME
   Passed values:
        ULONG SCALER_DEADTIME
   Returned values: 
        None
*/

/* Response to: 
         subType: DSC_GET_SCALER_DEADTIME
    Passed values: 
         none 
    Size of passed values: */
/* Returned values in data portion of message:
         ULONG SCALER_DEADTIME
    Size of returned values in data portion: */
#define DSC_GET_SCALER_DEADTIME_LEN 4

#endif
