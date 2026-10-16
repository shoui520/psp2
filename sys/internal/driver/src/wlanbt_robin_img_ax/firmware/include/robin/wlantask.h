/*
 * The four WLAN control tasks ("CB Proc", "MAC Tx", "MAC Tx Notify", "MAC Mgmt"),
 * their message queues and the staged initialisation they hang off.
 */
#ifndef ROBIN_WLANTASK_H
#define ROBIN_WLANTASK_H

#include <stdint.h>
#include "robin/sync.h"
#include "robin/threadx.h"

/* One entry of the post-initialisation chain; the chain is run by the control task. */
struct WlanInitStage {
	int (*run)(void);
	struct WlanInitStage *next;
};

/*
 * Handles the management layer uses to reach the queues of the other tasks.
 * Each member points to the handle variable, not to the object.
 */
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

void wlanSuspendTasks(int raise);
void wlanResumeTasks(void);
int wlanInitStageRegister(struct WlanInitStage *stage);
int wlanTaskHandlesInit(const struct WlanTaskHandles *handles);
int cbProcCreateTask(void);
int cbProcStartTask(void);
int macTxCreateTask(void);
int macTxStartTask(void);
void macTxSelectState(void);
int macTxNotifyCreateTask(void);
int macTxNotifyStartTask(void);
int macMgmtCreateTask(void);
int macMgmtStartTask(void);
int cbProcPostInitCreateTasks(void);
int cbProcPostInitStartTasks(void);
int macMgmtFlushQueues(void);

#endif /* ROBIN_WLANTASK_H */
