/*
 * The IBSS start and join entry code of the management layer (wlan/mgmtentry.c, the firmware's
 * placeholder names "macMgmt*Entry"): the beacon and probe response templates of a Bluetooth AMP
 * link, the host's security elements of an IBSS, and the IBSS's element buffer. The file name is
 * inferred; no source file name survives.
 */
#ifndef ROBIN_MGMTENTRY_H
#define ROBIN_MGMTENTRY_H

#include <stdint.h>
#include "robin/adhoc.h"
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/esa.h"
#include "robin/ie.h"
#include "robin/txring.h"

extern struct WlanIeBuffer wlanIbssIeBuffer;				/* 0x55170: the host's extra elements of an IBSS */
extern struct WlanHtTlvElements wlanIbssTlvElements;			/* 0x57890 */

/* The supplicant the security elements are handed to: supplicantIsEnabled() and supplicantSetAssocRsn() (0x54e74, 0x54e78; macMgmtSyncEntry()). */
extern int (*wlanSecurityQueryHook)(void);
extern void (*wlanSecurityRecordHook)(SecurityMode_t mode, Cipher_t *mcast, Cipher_t *ucast,
				      AkmSuite_t *akmList, IEEEtypes_RSNCapability_t *rsnCapability,
				      Cipher_t *groupManagementCipher);
extern uint32_t wlanIbssPowerLimit;					/* 0x5516c: dBm, 255: none */

/* The completion macMgmtQueryEntry() and macMgmtResetEntry() are given: the answer to the host's command. */
struct WlanHostCompletion {
	uint8_t reserved0[8];
	uint8_t sequence;		/* the command's sequence number */
	uint8_t reserved9[3];
	uint8_t status;			/* 0: the command succeeded */
};

/* wlan/hostcmdtlv.c */
int wlanHtTlvApply(struct WlanHtTlvElements *elements, struct WlanIeBuffer *ies, uint8_t *tlv);	/* 0x2a9c8 */
int wlanHostTlvResolve(struct WlanIeBuffer *ies, uint8_t *tlv, uint8_t *channels);		/* 0x2aa78 */
void wlanHostResponseBuild(OsaBuffer *buffer, unsigned int command, unsigned int status,
			   unsigned int sequence);						/* 0x2a8d8 */

void wlanHostResponse8040Build(OsaBuffer *buffer, unsigned int result);				/* 0x2a9aa */
unsigned int wlanAdHocStartParse(WlanAdHocStart *start, uint8_t *command, const uint8_t *defaultRates,
				 uint16_t *tlvLength, uint16_t defaultBeaconPeriod);		/* 0x2a910 */
unsigned int wlanAdHocJoinParse(WlanAdHocBssDesc *join, uint8_t *command, uint16_t *tlvLength);	/* 0x2a976 */

void macMgmtProcessEntry(struct WlanConnection *connection, unsigned int subtype, OsaBuffer *buffer,
			 struct WlanMacTxRequest *request);			/* 0x3600a */
void macMgmtUpdateEntry(uintptr_t argument);					/* 0x36236 */
void macMgmtConfigureEntry(uint8_t *capability, uint8_t *information, uint8_t *coexistence,
			   uint8_t *obssScan, uint8_t *reserved);		/* 0x36420 */
int macMgmtCheckEntry(struct WlanConnection *connection, uint8_t *tlv, uint8_t *channels); /* 0x3642a */
int macMgmtValidateEntry(struct WlanConnection *connection, const uint8_t *request,
				unsigned int command);				/* 0x36464 */
void macMgmtPrepareEntry(void);							/* 0x3649e */
void macMgmtDispatchEntry(const uint8_t *rsn, const uint8_t *wpa, uint8_t *ucast, uint8_t *mcast,
			  SecurityMode_t *mode, int unused, int skipHeader);		/* 0x364be */
void macMgmtParseEntry(int unused, const uint8_t *wpa, const uint8_t *rsn, struct WlanIeBuffer *ies,
		       uint8_t *present, uint8_t *mcast, uint8_t *ucast);	/* 0x3652a */

int macMgmtQueryEntry(struct WlanConnection *connection, const struct WlanHostCompletion *completion); /* 0x3691c */
int macMgmtResetEntry(struct WlanConnection *connection, const struct WlanHostCompletion *completion); /* 0x3699a */
void macMgmtSyncEntry(void);							/* 0x36a1c */
int macMgmtApplyEntry(void);							/* 0x36a2a */
void macMgmtResolveEntry(struct WlanConnection *connection, unsigned int channel, int limited); /* 0x36a32 */
void macMgmtScheduleEntry(struct WlanConnection *connection);			/* 0x36a56 */
void macMgmtNotifyEntry(struct WlanConnection *connection);			/* 0x36a78 */
int hostCmd002bStart(struct WlanConnection *connection, uint8_t *command);	/* 0x365aa */
int hostCmd002cJoin(struct WlanConnection *connection, uint8_t *command);	/* 0x3675e */

#endif /* ROBIN_MGMTENTRY_H */
