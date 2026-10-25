/*
 * The serial link's request (app/serialreq.c): request 0x20 lets the host unlock and lock the
 * command link on UART3, restart it, and read and write mode 1's buffer (serial1Buffer),
 * sending what it writes down the link as a frame 0x8310.
 */
#ifndef ERNIE_SERIALREQ_H
#define ERNIE_SERIALREQ_H

#include "host.h"

#define SERIAL_REQUEST_FRAME	0x8310	/* the frame carrying the buffer's bytes */

extern struct HostRequest serialRequest;

extern void serialRequestInit(void);
extern void serialRequestReset(void);
extern void serialRequestNotify(void);

#endif
