/*
 * The four WLAN control tasks ("CB Proc", "MAC Tx", "MAC Tx Notify", "MAC Mgmt"),
 * their message queues and the staged initialisation they hang off.
 */
#ifndef ROBIN_WLANTASK_H
#define ROBIN_WLANTASK_H

#include <stdint.h>
#include "robin/sync.h"
#include "robin/threadx.h"

struct WlanConnection;

/* One entry of the post-initialisation chain; the chain is run by the control task. */
struct WlanInitStage {
	int (*run)(void);
	struct WlanInitStage *next;
};

/*
 * Handles the management layer uses to reach the queues of the other tasks.
 * Each member points to the handle variable, not to the object.
 */
/* The control object of a ROM queue, as the firmware reserves it (56 bytes). */
typedef struct { uint32_t words[14]; } WlanQueueObject;

struct WlanTaskHandles {
	struct OsaEventFlags **mgmtEventFlags;
	struct OsaMessageQueue **mgmtSmeQueue;
	struct OsaMessageQueue **cbSmeQueue;
	struct OsaMessageQueue **timerQueue;
};

extern struct OsaMessageQueue *cbProcSmeQueue;
extern struct OsaMessageQueue *cbProcQueue;
extern TX_THREAD *cbProcTask;
extern struct OsaSemaphore *cbProcSemaphore;
extern struct OsaMessageQueue *macTxMgmtQueue;
extern struct OsaMessageQueue *uapGroupRekeyQueue;
extern TX_THREAD *macTxTask;
extern struct OsaEventFlags *macTxEventFlags;
extern TX_THREAD *macTxNotifyTask;
extern struct OsaEventFlags *macTxNotifyEventFlags;
extern TX_THREAD *macMgmtTask;
extern struct OsaEventFlags *macMgmtEventFlags;
extern struct OsaMessageQueue *macMgmt80211Queue;
extern struct OsaMessageQueue *macMgmtSmeQueue;
extern struct OsaMessageQueue *timerCbQueue;

/* The events of the MAC management task (macMgmtTaskMain()), and the event flags modes. */
#define MGMT_EVENT_RING_A	0x001
#define MGMT_EVENT_RING_B	0x002
#define MGMT_EVENT_COMMANDS	0x004
#define MGMT_EVENT_LINK_CHECK	0x008
#define MGMT_EVENT_SLOT_POLL	0x010
#define MGMT_EVENT_TIMERS	0x020
#define MGMT_EVENT_SLOT_RUN	0x040
#define MGMT_EVENT_SLOT_TICK	0x080
#define MGMT_EVENT_RING_C	0x200
#define MGMT_EVENT_PEER_CHECK	0x800
#define MGMT_EVENT_POST_RECEIVED	0x4000
#define MGMT_EVENT_POST_RELEASE	0x8000

#define MGMT_EVENT_SET_OR	7
#define MGMT_EVENT_WAIT_OR_CLEAR	8

extern struct WlanTaskHandles wlanTaskHandles;				/* 0x57814 */
extern WlanQueueObject macMgmtLinkCheckQueue;				/* 0x5772c */
extern WlanQueueObject macMgmtPeerCheckQueue;				/* 0x577a0 */

void macMgmtTaskMain(ULONG input);					/* 0x342a8 */
int wlanLinkCheckPost(struct WlanConnection *connection);		/* 0x347be */

void wlanSuspendTasks(int raise);
void wlanResumeTasks(void);
extern struct WlanInitStage *wlanInitStageHead;		/* the chain of stages, run again after a reset */
extern struct WlanInitStage *wlanInitStageTail;
int wlanInitStageRegister(struct WlanInitStage *stage);
int wlanTaskHandlesInit(const struct WlanTaskHandles *handles);
int cbProcCreateTask(void);
int cbProcStartTask(void);
int macTxCreateTask(void);
int macTxStartTask(void);
int macTxSelectState(void);
int macTxNotifyCreateTask(void);
int macTxNotifyStartTask(void);
int macMgmtCreateTask(void);
int macMgmtStartTask(void);
int cbProcPostInitCreateTasks(void);
int cbProcPostInitStartTasks(void);
int macMgmtFlushQueues(void);

/* Not converted yet; provisional names. */
void wlanMacResume(unsigned int reason);					/* 0x1c800 */
void wlanMacFlush(void);							/* 0x1c840 */

#endif /* ROBIN_WLANTASK_H */
