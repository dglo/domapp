/** 
 * @author John Jacobsen, John J. IT Svcs., for LBNL/IceCube
 * @author john@johnj.com
 */

#include "message.h"

#ifndef _GENERIC_MSG_SEND_RECV_H_
#define _GENERIC_MSG_SEND_RECV_H_

#define GMSR_FILE     1 /**< Indicates a file descriptor is a char device file */
#define GMSR_SOCKET   2 /**< Indicates a file descriptor is a socket */
#define GMSR_EOF     -1 /**< Indicates EOF condition */
#define GMSR_FDERR   -2 /**< Indicates an invalide file descriptor type */
#define GMSR_SHORTRD -3 /**< Indicates short read */
#define GMSR_BADDATA -4 /**< Indicates corrupt message or other bad data */

 
int gmsr_get_file_descriptor_mode(int filedes);

int gmsr_recvMessageFromFile(int filedes,
			     MESSAGE_STRUCT *recvBuffer_p,
			     char *recvData, int verbose);

int gmsr_recvMessageFromSocket(int filedes,
			       MESSAGE_STRUCT *recvBuffer_p,
			       char *recvData);

int gmsr_recvMessageGeneric(int filedes,
			    MESSAGE_STRUCT *recvBuffer_p,
			    char *recvData, int verbose);

int gmsr_sendMessageGeneric(int filedes,
			    MESSAGE_STRUCT *sendBuffer_p);

#endif
