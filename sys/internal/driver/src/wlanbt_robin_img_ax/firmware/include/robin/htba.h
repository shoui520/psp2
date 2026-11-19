/*
 * 802.11n in the connections' management: the HT elements a connection keeps,
 * the Block Ack action frames and the host commands that start and end a
 * Block Ack session (HostCmd_CMD_11N_ADDBA_REQ 0xce, _ADDBA_RSP 0xcf,
 * _DELBA 0xd0), the Block Ack timeout table, and the 20/40 BSS coexistence
 * frames (wlan/htba.c, wlan/baaction.c). Marvell published no source for it;
 * the names say what the code does and the file names are inferred.
 */
#ifndef ROBIN_HTBA_H
#define ROBIN_HTBA_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/ie.h"
#include "robin/protection.h"
#include "robin/softtimer.h"
#include "robin/stream.h"

/* Action frame categories and the Block Ack action codes (802.11 7.3.1.11, 7.4.1). */
enum {
	WLAN_ACTION_CATEGORY_BLOCK_ACK = 3,
	WLAN_ACTION_CATEGORY_PUBLIC = 4,
};

enum {
	WLAN_BA_ACTION_ADDBA_REQUEST,
	WLAN_BA_ACTION_ADDBA_RESPONSE,
	WLAN_BA_ACTION_DELBA,
};

#define WLAN_STATUS_UNSPECIFIED_FAILURE	1
#define WLAN_STATUS_REQUEST_DECLINED	37

/* The host events of a Block Ack session (Marvell's EVENT_ADDBA, EVENT_DELBA and EVENT_BA_STREAM_TIMEOUT). */
enum {
	WLAN_EVENT_ADDBA = 0x33,
	WLAN_EVENT_DELBA = 0x34,
	WLAN_EVENT_BA_STREAM_TIMEOUT = 0x37,
};

/* The host's command responses carry the command code with bit 15 set. */
#define WLAN_HOST_RESPONSE_ADDBA_REQUEST	0x80ce
#define WLAN_HOST_RESPONSE_DELBA		0x80d0
#define WLAN_HOST_RESPONSE_COEXISTENCE		0x80e9

/* A command's response header, and the body that follows it. */
struct WlanHostResponse {
	uint8_t command[2];
	uint8_t size[2];
	uint8_t sequence[2];
	uint8_t result[2];
	uint8_t body[];
};

#define WLAN_HOST_RESPONSE_HEADER	8

/*
 * The bodies of the commands. The parameter set of a session is 16 bits: bit 0
 * A-MSDU, bit 1 immediate Block Ack, bits 2-5 the TID, bits 6-15 the buffer
 * size. A result of 1 is a failure.
 */
struct HostCmdAddbaRequest {
	uint8_t result;
	uint8_t peer[6];
	uint8_t dialogToken;
	uint8_t paramSet[2];
	uint8_t timeout[2];
	uint8_t startSequence[2];	/* the starting sequence number of the window, in bits 4-15 */
};

_Static_assert(sizeof(struct HostCmdAddbaRequest) == 14, "ADDBA request");

struct HostCmdAddbaResponse {
	uint8_t result;
	uint8_t peer[6];
	uint8_t dialogToken;
	uint8_t status[2];
	uint8_t paramSet[2];
	uint8_t timeout[2];
	uint8_t startSequence[2];
};

_Static_assert(sizeof(struct HostCmdAddbaResponse) == 16, "ADDBA response");

/* The parameter set of a DELBA: bit 11 the initiator, bits 12-15 the TID. */
struct HostCmdDelba {
	uint8_t result;
	uint8_t peer[6];
	uint8_t paramSet[2];
	uint8_t reason[2];
};

_Static_assert(sizeof(struct HostCmdDelba) == 11, "DELBA");

/* The ADDBA request waiting for its response, and the connection it was sent on. */
struct WlanAddbaPending {
	struct HostCmdAddbaRequest request;
	uint8_t reserved14[2];
	struct WlanConnection *connection;
};

/* The DELBA waiting for its transmission, and the connection it was sent on. */
struct WlanDelbaPending {
	struct HostCmdDelba delba;
	uint8_t reserved11;
	struct WlanConnection *connection;
};

/* A Block Ack timeout the table keeps (44 bytes). */
struct WlanBaTimeout {
	uint32_t active;
	struct WlanSoftTimer timer;
	uint8_t peer[6];
	uint8_t tid;
	uint8_t initiator;		/* 1: this station is the originator */
	uint16_t timeout;		/* in units of 1024 microseconds */
	uint8_t reserved38[2];
	struct WlanConnection *connection;
};

_Static_assert(sizeof(struct WlanBaTimeout) == 44, "Block Ack timeout");

/* The cells of the Block Ack timeout table (0x553e4): its size, the table and the session hooks. */
extern uint8_t wlanBaTimeoutCount;
extern struct WlanBaTimeout *wlanBaTimeoutTable;
extern int (*wlanTxBaDeleteHook)(struct WlanConnection *connection, const uint8_t *peer, unsigned int tid);
extern int (*wlanRxBaDeleteHook)(const uint8_t *peer, unsigned int tid);
extern int (*wlanTxBaAddHook)(struct WlanConnection *connection, const struct WlanBaRequest *request);

/* The HT configuration the host set (HostCmd_CMD_11N_CFG 0xcd): the HT capability and the transmit information. */
extern uint8_t wlanHtTxCapability[2];
extern uint8_t wlanHtTxInfo[2];

/* The Block Ack state and the HT elements kept in SQRAM and in the ITCM. */
extern uint8_t wlanAddbaDialogToken;			/* SQRAM 0xc0011bc0: the ADDBA request waiting for its response */
extern uint8_t wlanHtCoexIe[3];				/* SQRAM 0xc0011bc5: the 20/40 BSS coexistence element */
extern uint32_t wlanDelbaCount;				/* SQRAM 0xc0011bc8: DELBA timers that ran */
extern uint8_t wlanHtOperationIe[24];			/* SQRAM 0xc0031f00: the HT operation element of the BSS */
extern uint8_t wlanHtOverlapScanIe[16];			/* SQRAM 0xc0031f18: the overlapping BSS scan parameters */

/* The rate selection's HT state (not converted yet): greenfield frames and the channel width in use. */
void wlanRateGreenfieldSet(struct WlanConnection *connection, int update, int greenfield);	/* 0x3d712 */
void wlanRateChannelWidthSet(struct WlanConnection *connection, int update, int width);		/* 0x3cfb2 */

void wlanBaTimeoutCallback(uint32_t entry);					/* 0x30385 */

const uint8_t *wlanConnectionHtOperation(struct WlanConnection *connection);	/* 0x301b0 */
const uint8_t *wlanConnectionHtCoexistence(struct WlanConnection *connection);	/* 0x30200 */
const uint8_t *wlanConnectionHtOverlapScan(struct WlanConnection *connection);	/* 0x30246 */
int wlanHtGreenfieldWanted(struct WlanConnection *connection);			/* 0x3028c */
int wlanHtShortGiWanted(struct WlanConnection *connection, unsigned int rate);	/* 0x302ce */
int wlanHtChannelWidth(struct WlanConnection *connection);			/* 0x302fc */
uint32_t wlanTxBaTimeoutShift(uintptr_t owner);					/* 0x3031a */
int wlanConnectionIsHt(struct WlanConnection *connection);			/* 0x30324 */
int wlanConnectionHtTransmitsBa(struct WlanConnection *connection);		/* 0x3032c */
int wlanHtElementGet(struct WlanConnection *connection, int elementId, void *destination); /* 0x3033e */
void wlanBaActionFrameAlloc(struct WlanConnection *connection, OsaBuffer **buffer,
			    const uint8_t *peer);				/* 0x303a6 */
int wlanBssCoexFrameSend(struct WlanConnection *connection, const uint8_t *body,
			 unsigned int length);					/* 0x303e2 */
void wlanBaAddbaResponseReceived(struct WlanConnection *connection, int result,
				 const uint8_t *peer, uint8_t *response);	/* 0x30438 */
void wlanBaDelbaTimeout(uint32_t record);					/* 0x304e5 */
void wlanBaAddbaTimeout(uint32_t record);					/* 0x30521 */
int wlanBaAllowedForTid(struct WlanConnection *connection, uint8_t tid);	/* 0x30552 */
int hostCmd00ce(uint8_t *body, unsigned int length);				/* 0x3058e */
int hostCmd00cf(uint8_t *body, unsigned int length);				/* 0x306ca */
int hostCmd00d0(uint8_t *body, unsigned int length);				/* 0x30728 */
int hostCmd00e9(struct WlanConnection *connection, uint8_t *body, unsigned int length); /* 0x307d2 */
void wlanHtBeaconApply(struct WlanConnection *connection, const WlanIePointers *ies,
		       const uint8_t *source);					/* 0x307e0 */
void hostCmd00cdSet(struct WlanConnection *connection, uint8_t *body);		/* 0x3089c */
void hostCmd00cdGet(uint8_t *body);						/* 0x308d4 */
OsaBuffer *wlanHostMessageAlloc(struct WlanConnection *connection);	/* 0x34c94 */
int wlanHostMessagePost(OsaBuffer *message);				/* 0x34d80 */
void wlanBaSessionsDelete(struct WlanConnection *connection, const uint8_t *peer);	/* 0x308e8 */
void wlanHtElementsReset(struct WlanConnection *connection);			/* 0x30940 */
int wlanHtElementsInit(void);							/* 0x30998 */
int wlanHtInitStageRegister(void);						/* 0x309ac */
void wlanBaActionSent(OsaBuffer *frame, int success);				/* 0x309c2 */
int wlanBssCoexFrameReceived(struct WlanConnection *connection, const uint8_t *body,
			     const uint8_t *frame, int unused);			/* 0x30a20 */
int wlanBaActionReceived(struct WlanConnection *connection, const uint8_t *body,
			 const uint8_t *frame, int unused);			/* 0x30b82 */
void wlanBaTimeoutOpsInit(void);						/* 0x30c0e */
void wlanTxRingMovePending(unsigned int ring, unsigned int target);		/* 0x30eec */
void wlanTxRingMoveToWindow(unsigned int target, unsigned int ring, unsigned int type,
			    const uint8_t *address);				/* 0x30efe */

/* Whether a connection's peer may use greenfield frames, from the HT capability and operation elements. */
int wlanHtGreenfieldAllowed(const uint8_t *htCapability, const uint8_t *htOperation);	/* 0x4ab84 */
int wlanHtShortGiAllowed(const uint8_t *htCapability, unsigned int rate, int shortGi20,
			 int shortGi40);					/* 0x4aba0 */
int wlanHtElementCopy(int elementId, void *destination, const uint8_t *htCapability,
		      uint8_t *htOperation, const uint8_t *coexistence, int unused,
		      const uint8_t *overlapScan);				/* 0x4abce */
struct WlanBaTimeout *wlanBaTimeoutFree(void);					/* 0x4ac3c */
void wlanBaTimeoutStart(struct WlanConnection *connection, unsigned int timeout,
			const uint8_t *peer, unsigned int tid, unsigned int initiator,
			void (*callback)(uint32_t));				/* 0x4ac66 */
void wlanBaTimeoutExpired(struct WlanConnection *connection, const uint8_t *record,
			  struct WlanTxBaSession *txSessions,
			  struct WlanRxBaSession *rxSessions,
			  void (*callback)(uint32_t));				/* 0x4acbc */
int wlanBaTimeoutCancel(const uint8_t *peer, unsigned int tid, unsigned int initiator);	/* 0x4ad3a */
/* A Block Ack parameter set (802.11 ADDBA and DELBA), passed by value. */
struct WlanBaParamSet {
	uint16_t amsdu : 1;
	uint16_t policy : 1;
	uint16_t tid : 4;
	uint16_t bufferSize : 10;
};

/* The same where a frame or command carries it, unaligned. */
struct WlanBaParamSetUnaligned {
	struct WlanBaParamSet value;
} __attribute__((packed));

void wlanBaDelbaBuild(OsaBuffer *frame, struct WlanBaParamSet paramSet, uint16_t reason);	/* 0x4ad9a */
void wlanBaAddbaDeclineBuild(OsaBuffer *frame, const uint8_t *request);		/* 0x4adc4 */
void wlanBaAddbaEventPost(const uint8_t *peer, const uint8_t *request,
			  struct WlanConnection *connection);			/* 0x4adfe */
void wlanBaDelbaReceived(struct WlanConnection *connection, const uint8_t *peer,
			 const uint8_t *body);					/* 0x4ae48 */
void wlanHostAddbaReplyBuild(OsaBuffer *message, int result, const uint8_t *peer,
			     const uint8_t *body, uint16_t status);		/* 0x4aecc */
void wlanBaAddbaAccepted(struct WlanConnection *connection, unsigned int startSequence,
			 unsigned int timeout, struct WlanBaParamSet paramSet, const uint8_t *peer,
			 struct WlanTxBaSession *txSessions, void (*callback)(uint32_t)); /* 0x4af2a */
void wlanHostDelbaReplyBuild(OsaBuffer *message, struct WlanConnection *connection, const uint8_t *body, int success,
			     const uint8_t *peer);				/* 0x4afb0 */
void wlanBaAddbaRequestBuild(OsaBuffer *frame, unsigned int startSequence,
			     const struct HostCmdAddbaRequest *request);	/* 0x4b002 */
int wlanBaAddbaRequestCheck(const uint8_t *request, struct WlanTxBaSession *txSessions,
			    unsigned int tid);					/* 0x4b064 */
void wlanBaAddbaResponseBuild(OsaBuffer *frame, const struct HostCmdAddbaResponse *response); /* 0x4b098 */
void wlanBaRxRequestFromResponse(struct WlanBaRequest *request, const uint8_t *response); /* 0x4b0d8 */
void wlanHtProtectionObserve(WlanProtection *protection, const WlanIePointers *ies,
			     const uint8_t *source, const uint8_t *bssid, int32_t timeout); /* 0x4b124 */
void wlanHtElementsClear(uint8_t *htCapability, uint8_t *htOperation, uint8_t *coexistence,
			 uint8_t *address, uint8_t *overlapScan);		/* 0x4b19e */
uint8_t wlanBaDelbaCheck(int notify, const uint8_t *body, struct WlanConnection *connection,
			 const uint8_t *peer, struct WlanSoftTimer *timer);		/* 0x4b1e4 */
int wlanBaIsDelba(const uint8_t *body);						/* 0x4b25a */
int wlanBssCoexIsFrame(const uint8_t *body);					/* 0x4b26e */
void wlanHostCoexistenceReplyBuild(OsaBuffer *message, struct WlanConnection *connection, const uint8_t *body); /* 0x4b282 */
int wlanBaAddbaTimeoutResponse(uint8_t *response, const uint8_t *record, uint8_t *token); /* 0x4b2be */
void wlanBssCoexFrameBuild(OsaBuffer *frame, const uint8_t *body, unsigned int length); /* 0x4b2ea */
void wlanBaRxResponseApply(const uint8_t *response, int update, struct WlanRxBaSession *sessions,
			   struct WlanConnection *connection, void (*callback)(uint32_t)); /* 0x4b328 */

#endif /* ROBIN_HTBA_H */
