/*
// PcaketFormatInfo.h 

// This file contains information about 
// the "on the wire" packet format that
// eventually makes it's way up to the 
// application.  
// In particular, it contains values for
// the "hidden" flag word that accompanies
// each packet and is passed between the 
// application and the FPGA fifo hardware
// on both the DAQ and DOM sides.  It also
// contains (future) values for the error
// status byte that is returned when a packet
// is dequeued.
// 
//
// March 30, 1999
// Chuck McParland
*/

#ifndef _PACKET_FMT_INFO_
#define _PACKET_FMT_INFO_

/* Legal symbols for header byte */
#define FL_RESET 0
#define FL_TRIG 1
#define FL_BEG_MSG 2
#define FL_MSG 3
#define FL_END_MSG 4
#define FL_STATUS 5
#define FL_CMD_BEG 6

#define FL_FORCED 1

#endif
