/* DSCmessagerAPIstatus.h */

/* This file contains subtype values for the 
   DAQ<=>DOM messaging API used by the Slow
   Control service.  

   March 7, 2003
   Chuck McParland */


#ifndef _DSC_MESSAGE_API_STATUS_
#define _DSC_MESSAGE_API_STATUS_


/* message API subtype values. This field is not
   declared as an enum because each service declares
   values in addion to those in MessageAPIstatus.h.
   Since enums are typedefs, odd compiler problems 
   will ensue.  

   Note 255 is the maximum value available for this field. */ 
#define	DSC_READ_ALL_ADCS 10
#define	DSC_READ_ONE_ADC 11
#define DSC_READ_ALL_DACS 12
#define DSC_WRITE_ONE_DAC 13
#define DSC_SET_PMT_HV 14
#define DSC_ENABLE_PMT_HV 16
#define DSC_DISABLE_PMT_HV 18
#define DSC_QUERY_PMT_HV 22
#define DSC_READ_ONE_ADC_REPT 27
#define DSC_GET_PMT_HV_LIMIT 28
#define DSC_SET_PMT_HV_LIMIT 29
#define DSC_READ_ONE_DAC 30


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
	UBYTE TRUE/FALSE
	UBYTE spare
	USHORT PMT_ADC
	USHORT PMT_DAC
   Size of returned values in data portion: */ 
#define DSC_QUERY_PMT_HV_LEN 	6	

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

#endif
