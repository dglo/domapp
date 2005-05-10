/**
 *   @file genericMsgSendRecv.c
 * Methods to send and receive messages either over a socket or 
 * using the device driver files
 * $Revision: 1.2 $
 * @author John Jacobsen, John J. IT Svcs, for LBNL and IceCube
 * Parts of this are based on code by Chuck McParland
 * $Date: 2005-04-28 16:52:29 $
 */

#include <string.h>
#include <unistd.h>

/** project include files */
#include "msgHandlerTest.h"
#include "DOMtypes.h"
#include "messageAPIstatus.h"
#include "commonMessageAPIstatus.h"
#include "message.h"
#include "messageBuffers.h"
#include "msgHandler.h"
#include "MSGHANDLERmessageAPIstatus.h"
#include "genericMsgSendRecv.h"

#define pfprintf(...)  /* Use this to comment out debug lines */

#if defined (LINUX) || defined (CYGWIN)
int gmsr_get_file_descriptor_mode(int filedes) {
  /** Determine mode from file descriptor. 
      @param filedes The File Descriptor we're interested in 
      @return GMSR_FILE if (device) file, GMSR_SOCKET if network socket,
              GMSR_FDERR if something else, or can't stat().
      @author Jacobsen
  */

  struct stat filestat;

  if(fstat(filedes, &filestat)) {
    return GMSR_FDERR;
  } else {
    if(S_ISCHR(filestat.st_mode)) {
      /* it's a (char) device driver file */
      return GMSR_FILE;
    } else if(S_ISSOCK(filestat.st_mode)) {
      /* It's a socket */
      return GMSR_SOCKET;
    } else {
      /* Something else */
      return GMSR_FDERR;
    }
  }
}


int gmsr_recvMessageFromFile(int filedes,
			     MESSAGE_STRUCT *recvBuffer_p,
			     char *recvData,
			     int verbose) {
  /** Receive a message using the device driver file <filedes>.
      Just uses a single read call.
      @param recvBuffer_p  pointer to message data to get
      @param filedes       file descriptor to for device file to read from
      @return number of bytes received, or a negative error condition
      @author Jacobsen
  */
  static char sub[] = "gmsr_recvMessageFromFile";
  static char tmpbuf[MAXDATA_VALUE + MSG_HDR_LEN];
  UBYTE * message_data;
  int nread;
  int toCopy;

  pfprintf(stderr,"%s: Trying to receive a message.\n",sub);
  /* Get bytes from driver using read() */
  if(verbose) printf("read+\n");
  nread = read(filedes, tmpbuf, MAXDATA_VALUE + MSG_HDR_LEN);
  if(verbose) printf("read-\n");
  if(nread <= 0) {
    //pfprintf(stderr,"%s: Read failed (%d).\n",sub,nread);
    return GMSR_SHORTRD;
  } 

  if(verbose) printf("memcpy+\n");
  /* Copy header portion */
  memcpy(recvBuffer_p, tmpbuf, MSG_HDR_LEN);
  if(verbose) printf("memcpy-\n");
  /* Copy data portion */
  toCopy = Message_dataLen(recvBuffer_p);
  if(toCopy > MAXDATA_VALUE) toCopy = MAXDATA_VALUE;
  memcpy(recvData, tmpbuf+MSG_HDR_LEN, toCopy);
  Message_setData(recvBuffer_p, recvData, nread - MSG_HDR_LEN);

  return nread;
}

int gmsr_recvMessageFromSocket(int filedes,
			       MESSAGE_STRUCT *recvBuffer_p,
			       char *recvData) {
  /** Socket message receiver.
      @param recvBuffer_p  pointer to message data to get
      @param filedes       file descriptor for socket to read from
      @param recvData      Array to store message data, if any
      @return number of bytes received, or a negative error condition
      @author Jacobsen, with some code from McP.
  */
  
  static char sub[] = "gmsr_recvMessageFromSocket";
  long msgLength;
  int targetLength;
  int sts;
  int recvBytes;
  UBYTE *buffer_p;

  pfprintf(stderr,"%s: Trying to receive a message.\n",sub);
  
  targetLength = MSG_HDR_LEN;
  buffer_p = (char *) recvBuffer_p;

  /* read until we have entire message header */
  while (targetLength != 0) {
   
    sts = recv(filedes, buffer_p, targetLength, 0);
    pfprintf(stderr, "%s: Status of header read is %d.\n",sub,sts);
    if(sts <= 0) {
      return sts == 0 ? GMSR_EOF : sts;
    }

    targetLength -= sts;
    if(targetLength == 0) {
      break;
    }
    buffer_p += sts;
  }

  if(Message_dataLen(recvBuffer_p) > MAXDATA_VALUE) {
    pfprintf(stderr, "%s: bad message length in header (%d bytes).\n",
	    sub, Message_dataLen(recvBuffer_p));
    return GMSR_BADDATA;
  }

  pfprintf(stderr, "%s: header received, starting data portion of length=%d\n",
	  sub,
	  Message_dataLen(recvBuffer_p));

  /* receive the optional message data portion */
  targetLength = Message_dataLen(recvBuffer_p);
  buffer_p     = recvData;
  recvBytes    = 0;
  
  while (targetLength != 0) {
      
    pfprintf(stderr,"%s: reading data portion now.\n",sub);
    sts = recv(filedes, buffer_p, targetLength, 0);
    pfprintf(stderr, "%s: Status of data portion read is %d.\n",sub,sts);
    if(sts <= 0) {
      return sts == 0 ? GMSR_EOF : sts;
    }
    
    targetLength -= sts;
    recvBytes += sts;

    if(targetLength == 0) {
      break;
    }
    buffer_p += sts;
  }
  
  Message_setData(recvBuffer_p, recvData, recvBytes);
  msgLength = Message_dataLen(recvBuffer_p);
  return msgLength;

}

int gmsr_recvMessageGeneric(int filedes,
			    MESSAGE_STRUCT *recvBuffer_p,
			    char *recvData,
			    int verbose) {
  /** Generic message receiver (uses either socket I/O or file I/O).
      Currently reads the whole thing at once, whether socket or device file.

      @param recvBuffer_p  pointer to message data to get
      @param filedes       file descriptor to either device file or socket to read from
      @return number of bytes received, or a negative error condition
      @author Jacobsen
  */
  static char sub[] = "gmsr_recvMessageGeneric";
  int tmp;
  int targetLength;
  int sts;
  int fmode;

  fmode = gmsr_get_file_descriptor_mode(filedes);

  switch(fmode) {
  case GMSR_SOCKET: 
    return gmsr_recvMessageFromSocket(filedes, recvBuffer_p, recvData);
    break;
  case GMSR_FILE:
    return gmsr_recvMessageFromFile(filedes, recvBuffer_p, recvData, verbose);
    break;
  default:
    return GMSR_FDERR;
    break;
  }
}
#endif

int gmsr_sendMessageGeneric(int filedes,
			    MESSAGE_STRUCT *sendBuffer_p) {
  /** Generic message sender (uses either socket I/O or file I/O).

      Currently writes the whole thing at once, whether socket or device file.

      @param sendBuffer_p  pointer to message to send
      @param filedes       file descriptor to either device file or socket to send on
      @return number of bytes sent.
      @author Jacobsen
   */
  long sendlen;
  int wrote;
  char txbuf[MAXDATA_VALUE + MSG_HDR_LEN]; // + sizeof(long)];

  if(sendBuffer_p == 0) {
    return 0; /* Bad source pointer --> no data sent */
  }

  pfprintf(stderr,"Entered, sendBuffer_p == %p\n",sendBuffer_p);
  sendlen = //sizeof(long) + 
    MSG_HDR_LEN + (long) Message_dataLen(sendBuffer_p);


  pfprintf(stderr,"memcpy\n");

  /* Copy message header to tx buffer */
  memcpy(txbuf, // + sizeof(long), 
	 sendBuffer_p, MSG_HDR_LEN);

  /* Copy message data, if any */
  if(Message_dataLen(sendBuffer_p) > 0) {
    memcpy(txbuf // + sizeof(long) 
	   + MSG_HDR_LEN, 
	   Message_getData(sendBuffer_p), Message_dataLen(sendBuffer_p));
  }


  pfprintf(stderr,"write.\n");

  wrote = write(filedes, txbuf, sendlen);

  return wrote;
}
