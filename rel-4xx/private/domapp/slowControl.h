/* slowControl.h */

/* 29 Nov 1999
	-changed id's for SPE and MSPE disc dacs
*/

#ifndef _SLOWCONTROL_
#define _SLOWCONTROL_


#define MAX_NUM_ADCS 8
/* ADC channel nemonics */
#define ADC_VOLTAGE_SUM
#define ADC_5V_POWER_SUPPLY
#define ADC_PRESSURE
#define ADC_5V_CURRENT
#define ADC_3_3V_CURRENT
#define ADC_2_5V_CURRENT
#define ADC_1_8V_CURRENT
#define ADC_MINUS_5V_CURRENT

#define MAX_NUM_DACS 16
/* DAC channel nemonics */
#define DAC_ATWD0_TRIGGER_BIAS
#define DAC_ATWD0_RAMP_TOP
#define DAC_ATWD0_RAMP_RATE
#define DAC_ATWD_ANALOG_REF
#define DAC_ATWD1_TRIGGER_BIAS
#define DAC_ATWD1_RAMP_TOP
#define DAC_ATWD1_RAMP_RATE
#define DAC_PMT_FE_PEDESTAL
#define DAC_MULTIPLE_SPE_THRESH
#define DAC_SINGLE_SPE_THRESH
#define DAC_LED_BRIGHTNESS
#define DAC_FAST_ADC_REF
#define DAC_INTERNAL_PULSER
#define DAC_FE_AMP_LOWER_CLAMP
#define DAC_SPARE_DAC_0
#define DAC_SPARE_DAC_1

#endif
