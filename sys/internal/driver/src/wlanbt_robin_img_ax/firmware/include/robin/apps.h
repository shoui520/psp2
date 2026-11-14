/*
 * Frames an access point keeps for peers in power save (0x12918..0x131d4):
 * each dozing peer has a queue, and one for its delivery-enabled (U-APSD)
 * access categories; group-addressed frames wait for the DTIM beacon. The
 * stations share 22 entries. The names are inferred from use; see
 * wlan/apps.c.
 */
#ifndef ROBIN_APPS_H
#define ROBIN_APPS_H

#include <stdint.h>
#include "robin/buffer.h"
#include "robin/conn.h"
#include "robin/list.h"

#define WLAN_AP_PS_ENTRIES		22

/* One kept frame. */
struct WlanApPsEntry {
	struct RobinListNode node;	/* in the free or the used list */
	OsaBuffer *buffer;
	struct WlanApPsEntry *next;
	struct WlanApPsEntry *prev;
	struct WlanApPsQueue *queue;
};

_Static_assert(sizeof(struct WlanApPsEntry) == 24, "power-save entry");

/* How the entries are shared (0x54764). */
struct WlanApPsLimits {
	uint8_t reportFull;		/* 1: report a full peer to the host and keep the frame */
	uint8_t maxBuffered;		/* entries the stations share, at most 22 */
	uint8_t stations;		/* peers that have dozed since they associated */
	uint8_t quota;			/* entries one peer may hold */
};

/* The host's command that reads or sets the limits. */
struct WlanApPsBufferConfig {
	uint8_t action[2];		/* 1: set */
	uint8_t reportFull;
	uint8_t maxBuffered;
};

extern struct WlanApPsLimits wlanApPsLimits;

int wlanApPsHasBuffered(const struct WlanConnectionRole *role);
int wlanApPsBufferCommand(struct WlanConnection *connection, struct WlanApPsBufferConfig *config);
void wlanApPsResetGroup(struct WlanConnectionInfo *info);
void wlanApPsFreeEntry(struct WlanApPsEntry *entry);
int wlanApPsUnlink(struct WlanApPsEntry *entry);
void wlanApPsDrop(struct WlanApPsEntry *entry);
void wlanApPsFlushGroup(struct WlanConnection *connection);
void wlanApPsResetUapsd(struct WlanConnection *connection);
void wlanApPsResetStation(struct WlanConnection *connection);
int wlanApPsInit(void);
void wlanApPsFlushUapsd(struct WlanConnection *connection);
void wlanApPsStationGone(struct WlanConnection *connection);
int wlanApPsReportFull(struct WlanConnection *connection);
void wlanApPsCheckRoom(struct WlanConnection *connection);
int wlanApPsSendNext(struct WlanConnection *connection, struct WlanApPsQueue *queue, int more);
void wlanApPsSendAll(struct WlanConnection *connection, struct WlanApPsQueue *queue);
void wlanApPsSendGroup(struct WlanConnection *connection, int dtim);
void wlanApPsSetState(struct WlanConnection *connection, int powerSave);
OsaBuffer *wlanApPsCopyMgmtFrame(struct WlanConnection *connection, OsaBuffer *buffer);
void wlanApPsInsert(struct WlanApPsEntry *entry, struct WlanApPsEntry *after);
int wlanApPsAcPrecedes(unsigned int category, unsigned int other);
struct WlanApPsEntry *wlanApPsFindSlot(struct WlanApPsEntry *entry, unsigned int category);
int wlanApPsAcDeliveryEnabled(uint8_t flags, unsigned int category);
struct WlanApPsEntry *wlanApPsAllocEntry(void);
int wlanApPsQueueFrame(struct WlanConnection *connection, OsaBuffer *buffer);
int wlanApPsUnderQuota(struct WlanConnection *connection);
int wlanApPsMayBuffer(struct WlanConnection *connection, OsaBuffer *buffer);
int wlanTxHoldForPeer(OsaBuffer *buffer, struct WlanConnection *connection);
int wlanTxPeerHolding(struct WlanConnection *connection);

struct WlanRxStatus;
int wlanApPsPsPoll(struct WlanConnection *connection);
void wlanApPsStartServicePeriod(struct WlanConnection *connection, unsigned int tid);
void wlanApPsTrigger(struct WlanConnection *connection, const struct WlanRxStatus *status);
void wlanApPsTxDone(struct WlanConnection *connection, const uint8_t *data);

#endif /* ROBIN_APPS_H */
