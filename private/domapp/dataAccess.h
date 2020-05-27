/* dataAccess.h */
#ifndef _DATA_ACCESS_H_
#define _DATA_ACCESS_H_

/* Header file for defines, structs, etc. for */
/* the Data Access Service. */

/* Data Access version info. */
#define DAC_MAJOR_VERSION 10
#define DAC_MINOR_VERSION 2
/* major version 10	10 May 2003
	beginning of icecube domapp code chain.  Version 10 implemented
	as single thread test version.
*/
/* minor version 1	5 July 2003
	added monitoring code infrastructure
   minor version 2     14 Jan. 2004 updated moni infrastructure JJ
*/

/* maximum length of Data Access last error string */
#define DAC_ERROR_STR_LEN 80

/* Data Access error strings */
/* for DAC_No_Errors */
#define DAC_ERS_NO_ERRORS "DAC: No errors."
/* for DAC_Bad_Msg_Subtype */
#define DAC_ERS_BAD_MSG_SUBTYPE  "DAC: Bad msg subtype."
#define DAC_DATA_OVERRUN         "DAC: Readout buffer data overrun."
#define DAC_MONI_NOT_INIT        "DAC: Monitoring not initialized"
#define DAC_MONI_OVERFLOW        "DAC: Monitoring buffer overflowed"
#define DAC_MONI_BADSTAT         "DAC: Monitoring: bad status"
#define DAC_ERS_BAD_COMPR_FORMAT "DAC: Compression: bad format specifier"
#define DAC_ERS_BAD_ARGUMENT     "DAC: Bad message argument"
#define DAC_CANT_ENABLE_FB       "DAC: Can't enable flasher board"
#define DAC_CANT_GET_FB_SERIAL   "DAC: Can't get flasher board serial number"
#define DAC_BAD_LBM_DEPTH        "DAC: LBM depth out of range"
#define DAC_SN_NOT_INIT          "DAC: Supernova not initialized"

/* define ALLOCATE_READOUT_BUFFER if you want dataAccess
   to allocate memory to be used for FPGA simulation.
   Otherwise define READOUT_BASE_ADDR to the real address
 */
#define ALLOCATE_READOUT_BUFFER

/* define the size of the memory buffer shared between the
   FPGA data engine and domapp. Format is a bit mask that 
   matches the field size of the memory buffer length.  MUST
   be a power of 2!  I.e. mask of 0x1ff means that the FPGA
   will rotate through a set of 512 buffers that are, as defined
   by the FPGA code, to be some fixed length.  At the moment, this
   length is set to 2048
 */
#define FPGA_MEMORY_MASK 0x1ff

/* data access entry point */
void dataAccessInit(void);
int dataAccess(MESSAGE_STRUCT *M);

int data_access_unit_tests(void);
int fillMsgWithMoniData(MESSAGE_STRUCT *M);
#endif
