/*
 * The IBSS start and join entry code of the management layer (wlan/mgmtentry.c, the firmware's
 * placeholder names "macMgmt*Entry"): the beacon and probe response templates of a Bluetooth AMP
 * link, the host's security elements of an IBSS, and the IBSS's element buffer. The file name is
 * inferred; no source file name survives.
 */
#ifndef ROBIN_MGMTENTRY_H
#define ROBIN_MGMTENTRY_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/ie.h"
#include "robin/txring.h"

extern struct WlanIeBuffer wlanIbssIeBuffer;				/* 0x55170: the host's extra elements of an IBSS */
extern struct WlanHtTlvElements wlanIbssTlvElements;			/* 0x57890 */

/* The hooks of the security elements, called through (0x54e74, 0x54e78). */
extern int (*wlanSecurityQueryHook)(void);
extern void (*wlanSecurityRecordHook)(uint32_t mode, uint8_t *mcast, uint8_t *ucast, uint8_t *akmList,
				      uint32_t first, uint32_t second);

/* wlan/hostcmdtlv.c */
int wlanHtTlvApply(struct WlanHtTlvElements *elements, struct WlanIeBuffer *ies, uint8_t *tlv);	/* 0x2a9c8 */
int wlanHostTlvResolve(struct WlanIeBuffer *ies, uint8_t *tlv, uint8_t *channels);		/* 0x2aa78 */
void wlanHostResponseBuild(OsaBuffer *buffer, unsigned int command, unsigned int status,
			   unsigned int sequence);						/* 0x2a8d8 */

void macMgmtProcessEntry(struct WlanConnection *connection, unsigned int subtype, OsaBuffer *buffer,
			 struct WlanMacTxRequest *request);			/* 0x3600a */
void macMgmtUpdateEntry(uintptr_t argument);					/* 0x36236 */
void macMgmtConfigureEntry(uint8_t *capability, uint8_t *information, uint8_t *coexistence,
			   uint8_t *obssScan, uint8_t *reserved);		/* 0x36420 */
int macMgmtCheckEntry(struct WlanConnection *connection, uint8_t *tlv, uint8_t *channels); /* 0x3642a */
OsaBuffer *macMgmtValidateEntry(struct WlanConnection *connection, const uint8_t *request,
				unsigned int command);				/* 0x36464 */
void macMgmtPrepareEntry(void);							/* 0x3649e */
void macMgmtDispatchEntry(const uint8_t *rsn, const uint8_t *wpa, uint8_t *ucast, uint8_t *mcast,
			  uint8_t *mode, int unused, int skipHeader);		/* 0x364be */
void macMgmtParseEntry(int unused, const uint8_t *wpa, const uint8_t *rsn, struct WlanIeBuffer *ies,
		       uint8_t *present, uint8_t *mcast, uint8_t *ucast);	/* 0x3652a */

#endif /* ROBIN_MGMTENTRY_H */
