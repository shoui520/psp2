/*
 * The TSPEC sessions of a connection (wlan/tspec.c): the host asks for a traffic stream with
 * HostCmd 0x6e, the request goes out as an ADDTS action frame in a time slot of its own, the answer
 * is told to the host, and the admitted streams give their medium time to the queues. The names of
 * the firmware's placeholders ("p2pDirect*Entry") are kept; the comments say what the code does.
 */
#ifndef ROBIN_TSPEC_H
#define ROBIN_TSPEC_H

#include <stdint.h>
#include "robin/wmm.h"

struct WlanConnection;
struct WlanTxStatus;

/* WlanTspecFlags: the request is sent whether or not the access category needs admission control. */
#define WLAN_TSPEC_SEND_ALWAYS	0x02

/* The ADDTS request waiting for its time slot, and the slot's handle (0x59d44, 96 bytes). */
struct WlanAddtsPending {
	uint32_t slot;			/* wlanSlotSubmit()'s handle of the request in the slot scheduler; 0: none */
	uint8_t dialogToken;		/* the ADDTS request's dialog token; never 0 */
	uint8_t pending;		/* the TSPEC below is still to be sent */
	uint8_t tspec[55];
	uint8_t reserved61[3];
	struct WlanConnection *connection;
	uint8_t tailLength;
	uint8_t tail[27];
};

_Static_assert(sizeof(struct WlanAddtsPending) == 96, "pending ADDTS request");
_Static_assert(offsetof(struct WlanAddtsPending, connection) == 64, "pending ADDTS connection");
_Static_assert(offsetof(struct WlanAddtsPending, tailLength) == 68, "pending ADDTS tail length");

extern struct WlanAddtsPending wlanAddtsPending;			/* 0x59d44 */
extern uint8_t wlanTspecFlags;						/* 0x55414 */

int p2pDirectComputeEntry(OsaBuffer **buffer);					/* 0x415ce */
int p2pDirectBuildEntry(const uint8_t *tspec, int mode);			/* 0x41436 */
int p2pDirectQueryEntry(const uint8_t *tspec, unsigned int length, const uint8_t *tail,
			unsigned int tailLength);				/* 0x4160c */
int p2pDirectResetEntry(void);							/* 0x416a0 */
int p2pDirectSyncEntry(int count);						/* 0x416d8 */
int p2pDirectApplyEntry(void);							/* 0x4172a */
int p2pDirectResolveEntry(const uint8_t *response, int length);		/* 0x4173e */
int p2pDirectScheduleEntry(struct WlanConnection *connection, const uint8_t *source,
			   const uint8_t *destination, const uint8_t *body);	/* 0x417a4 */
int p2pDirectFormatEntry(struct WlanConnection *connection, const uint8_t *body, const uint8_t *frame);	/* 0x41894 */
unsigned int p2pDirectTrackEntry(void);					/* 0x4190a */
unsigned int p2pDirectManageEntry(void);					/* 0x41940 */
int p2pDirectAcquireEntry(void);						/* 0x4196a */
int p2pDirectReleaseEntry(void);						/* 0x419a6 */
int p2pDirectSetupEntry(uint32_t handle, int event, void (*completion)(int result));	/* 0x419d4 */
int p2pDirectClearEntry(uint32_t handle, const struct WlanTxStatus *status, const OsaBuffer *buffer,
			uint32_t second, uint32_t third,
			int (*result)(const uint8_t *body, int length));		/* 0x41a5c */
void p2pDirectVerifyEntry(struct WlanConnection *connection);		/* 0x41bf2 */
int p2pDirectSelectEntry(unsigned int category);				/* 0x41c72 */
int p2pDirectAdjustEntry(unsigned int tid);					/* 0x41c88 */

#endif
