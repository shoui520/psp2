/*
 * Creation of the WLAN control tasks and their queues, and the chain of
 * initialisation stages. The filename is inferred; the task and queue names
 * are the firmware's own strings.
 */
#include <stddef.h>
#include "robin/conn.h"
#include "robin/wlantask.h"
#include "robin/osa.h"
#include "robin/rom.h"
#include "robin/sync.h"
#include "robin/task.h"

enum {
	WLAN_QUEUE_MESSAGE = 4,
	WLAN_QUEUE_CLASS = 12,
	WLAN_TASK_PRIORITY_MGMT = 12,
	WLAN_TASK_PRIORITY_TX = 11,
};

/* Control objects the ROM kernel fills in; sizes are what the firmware reserves. */
typedef struct { uint32_t words[14]; } WlanQueueObject;
typedef struct { uint32_t words[8]; } WlanEventFlagsObject;
typedef struct { uint32_t words[7]; } WlanSemaphoreObject;

struct OsaMessageQueue *cbProcSmeQueue __attribute__((section(".itcm.bss")));
struct OsaMessageQueue *cbProcQueue __attribute__((section(".itcm.bss")));
TX_THREAD *cbProcTask __attribute__((section(".itcm.bss")));
struct OsaSemaphore *cbProcSemaphore __attribute__((section(".itcm.bss")));
struct OsaMessageQueue *macTxMgmtQueue __attribute__((section(".itcm.bss")));
struct OsaMessageQueue *uapGroupRekeyQueue __attribute__((section(".itcm.bss")));
TX_THREAD *macTxTask __attribute__((section(".itcm.bss")));
struct OsaEventFlags *macTxEventFlags __attribute__((section(".itcm.bss")));
TX_THREAD *macTxNotifyTask __attribute__((section(".dtcm.bss")));
struct OsaEventFlags *macTxNotifyEventFlags __attribute__((section(".dtcm.bss")));
TX_THREAD *macMgmtTask __attribute__((section(".itcm.bss")));
struct OsaEventFlags *macMgmtEventFlags __attribute__((section(".itcm.bss")));
struct OsaMessageQueue *macMgmt80211Queue __attribute__((section(".itcm.bss")));
struct OsaMessageQueue *macMgmtSmeQueue __attribute__((section(".itcm.bss")));
struct OsaMessageQueue *timerCbQueue __attribute__((section(".itcm.bss")));

static WlanQueueObject cbProcSmeQueueObject __attribute__((section(".itcm.bss")));
static WlanQueueObject cbProcQueueObject __attribute__((section(".itcm.bss")));
static struct TX_THREAD_STRUCT cbProcTaskObject __attribute__((section(".itcm.bss")));
static WlanSemaphoreObject cbProcSemaphoreObject __attribute__((section(".itcm.bss")));
static uint8_t cbProcStack[1700] __attribute__((section(".dtcm.bss")));
static WlanQueueObject macTxMgmtQueueObject __attribute__((section(".itcm.bss")));
static WlanQueueObject uapGroupRekeyQueueObject __attribute__((section(".itcm.bss")));
static WlanEventFlagsObject macTxEventFlagsObject __attribute__((section(".itcm.bss")));
static struct TX_THREAD_STRUCT macTxTaskObject __attribute__((section(".itcm.bss")));
static uint8_t macTxStack[1900] __attribute__((section(".dtcm.bss")));
static WlanEventFlagsObject macTxNotifyEventFlagsObject __attribute__((section(".dtcm.bss")));
static struct TX_THREAD_STRUCT macTxNotifyTaskObject __attribute__((section(".dtcm.bss")));
static uint8_t macTxNotifyStack[1200] __attribute__((section(".dtcm.bss")));
static WlanQueueObject macMgmt80211QueueObject __attribute__((section(".itcm.bss")));
static WlanQueueObject macMgmtSmeQueueObject __attribute__((section(".itcm.bss")));
static WlanQueueObject timerCbQueueObject __attribute__((section(".itcm.bss")));
static WlanEventFlagsObject macMgmtEventFlagsObject __attribute__((section(".itcm.bss")));
static struct TX_THREAD_STRUCT macMgmtTaskObject __attribute__((section(".itcm.bss")));
static uint8_t macMgmtStack[1700] __attribute__((section(".dtcm.bss")));
/* Two single-word queues of the management task, with their 24-byte storage. */
static WlanQueueObject macMgmtMaskQueueA __attribute__((section(".itcm.bss")));
static uint8_t macMgmtMaskStorageA[24] __attribute__((section(".itcm.bss")));
static WlanQueueObject macMgmtMaskQueueB __attribute__((section(".itcm.bss")));
static uint8_t macMgmtMaskStorageB[24] __attribute__((section(".itcm.bss")));

static struct WlanTaskHandles wlanTaskHandles __attribute__((section(".itcm.bss")));
static struct WlanInitStage *wlanInitStageHead __attribute__((section(".itcm.rw")));
static struct WlanInitStage *wlanInitStageTail __attribute__((section(".itcm.rw")));
static struct WlanInitStage macTxStage __attribute__((section(".itcm.bss")));
static struct WlanInitStage macMgmtFlushStage __attribute__((section(".itcm.bss")));

/* Unconverted pieces. */
int macTxParseState(void);			/* 0x9b20, init stage of the transmit task */
void macTxNotifyParseState(void);		/* 0x2fbcc */
void macTxQueryQueue(int value, int apply);	/* 0x10068 */
void macTxResetQueue(int value, int apply);	/* 0x1007a */
void macTxComputeQueue(void);			/* 0x10056 */
void macTxTaskMain(ULONG input);		/* 0x9e3c */
void macMgmtTaskMain(ULONG input);		/* 0x342a8 */
void macTxNotifyTaskMain(ULONG input);		/* 0x2f42c */
void cbProcTaskMain(ULONG input);		/* 0x1aeae */
int macMgmtConfigureBuffer(void);		/* 0x37d74 */
int uapSqramValidateContext(void);		/* SQRAM via ROM veneer 0x52eb4 */
void wlanPsConfigureEntry(void);		/* 0x16a4c */
void macMgmtRecordContext(void);		/* 0x35fca */
void macTxNotifyVerifyFrame(void);		/* 0x33abc */
int tx_queue_create_words(void *queue, void *name, unsigned int words, void *start, unsigned int size);

/* Appends a stage to the chain unless it is empty or already present. */
int wlanInitStageRegister(struct WlanInitStage *stage)
{
	struct WlanInitStage *entry;

	if (stage == NULL || stage->run == NULL)
		return 1;
	for (entry = wlanInitStageHead; entry != NULL; entry = entry->next) {
		if (entry == stage)
			return 1;
	}
	if (wlanInitStageHead != NULL)
		wlanInitStageTail->next = stage;
	else
		wlanInitStageHead = stage;
	wlanInitStageTail = stage;
	stage->next = NULL;
	return 0;
}

/* Drops everything queued for the management task and resets the queue selectors. */
int macMgmtFlushQueues(void)
{
	void *buffer;
	uintptr_t message = 0;

	osaEventFlagsSet(*wlanTaskHandles.mgmtEventFlags, 0, 5);
	while (osaMsgQueueReceive(*wlanTaskHandles.mgmtSmeQueue, (uintptr_t *)&buffer, 4, 0) != 15)
		osa_BufferFree(buffer);
	while (osaMsgQueueReceive(*wlanTaskHandles.timerQueue, &message, 4, 0) != 15)
		;
	macTxQueryQueue(255, 0);
	macTxResetQueue(255, 0);
	macTxComputeQueue();
	return 0;
}

/* Publishes the queue handles of the control and management tasks. */
int wlanTaskHandlesInit(const struct WlanTaskHandles *handles)
{
	wlanTaskHandles = *handles;
	macMgmtRecordContext();
	macTxNotifyVerifyFrame();
	macMgmtFlushStage.run = macMgmtFlushQueues;
	return wlanInitStageRegister(&macMgmtFlushStage);
}

int cbProcCreateTask(void)
{
	if (macMgmtConfigureBuffer())
		return 1;
	if (uapSqramValidateContext() != 0)
		return 1;
	cbProcSmeQueue = (struct OsaMessageQueue *)&cbProcSmeQueueObject;
	osaMsgQueueCreate(&cbProcSmeQueue, "CBProcSMEMsgQ", WLAN_QUEUE_MESSAGE, 6, WLAN_QUEUE_CLASS);
	cbProcQueue = (struct OsaMessageQueue *)&cbProcQueueObject;
	osaMsgQueueCreate(&cbProcQueue, "CBProcMsgQ", WLAN_QUEUE_MESSAGE, 10, WLAN_QUEUE_CLASS);
	__aeabi_memset4(cbProcStack, sizeof(cbProcStack), 0xb1);
	cbProcTask = &cbProcTaskObject;
	if (osaCreateTask(&cbProcTask, cbProcStack, sizeof(cbProcStack), WLAN_TASK_PRIORITY_MGMT,
			  "CB Proc", cbProcTaskMain, 0) != 0)
		return 1;
	wlanPsConfigureEntry();
	return 0;
}

int cbProcStartTask(void)
{
	osaTaskResume(cbProcTask);
	cbProcSemaphore = (struct OsaSemaphore *)&cbProcSemaphoreObject;
	return osaSemaphoreInit(&cbProcSemaphore, 1, WLAN_QUEUE_CLASS);
}

/* Queues of the transmit task: the management frame queue and the group rekey queue. */
void macTxSelectState(void)
{
	/* The caller tests the result of the first creation. */
	if (osaMsgQueueCreate(&macTxMgmtQueue, "TxMgmt80211MsgQ", WLAN_QUEUE_MESSAGE, 6,
			      WLAN_QUEUE_CLASS) == 0)
		osaMsgQueueCreate(&uapGroupRekeyQueue, "UapGrpRekeyTimeOutMsgQ", WLAN_QUEUE_MESSAGE,
				  6, WLAN_QUEUE_CLASS);
}

int macTxCreateTask(void)
{
	macTxMgmtQueue = (struct OsaMessageQueue *)&macTxMgmtQueueObject;
	uapGroupRekeyQueue = (struct OsaMessageQueue *)&uapGroupRekeyQueueObject;
	macTxSelectState();
	macTxEventFlags = (struct OsaEventFlags *)&macTxEventFlagsObject;
	osaEventFlagsInit(&macTxEventFlags);
	macTxNotifyParseState();
	__aeabi_memset4(macTxStack, sizeof(macTxStack), 0xc1);
	macTxTask = &macTxTaskObject;
	if (osaCreateTask(&macTxTask, macTxStack, sizeof(macTxStack), WLAN_TASK_PRIORITY_TX,
			  "MAC Tx", macTxTaskMain, 0) != 0)
		return 1;
	macTxStage.run = macTxParseState;
	wlanInitStageRegister(&macTxStage);
	return 0;
}

int macTxStartTask(void)
{
	return osaTaskResume(macTxTask);
}

int macTxNotifyCreateTask(void)
{
	macTxNotifyEventFlags = (struct OsaEventFlags *)&macTxNotifyEventFlagsObject;
	osaEventFlagsInit(&macTxNotifyEventFlags);
	__aeabi_memset4(macTxNotifyStack, sizeof(macTxNotifyStack), 0xd1);
	macTxNotifyTask = &macTxNotifyTaskObject;
	return osaCreateTask(&macTxNotifyTask, macTxNotifyStack, sizeof(macTxNotifyStack),
			     WLAN_TASK_PRIORITY_TX, "MAC Tx Notify", macTxNotifyTaskMain, 0) != 0;
}

int macTxNotifyStartTask(void)
{
	return osaTaskResume(macTxNotifyTask);
}

int macMgmtCreateTask(void)
{
	timerCbQueue = (struct OsaMessageQueue *)&timerCbQueueObject;
	osaMsgQueueCreate(wlanTaskHandles.timerQueue, "TimerCbMsgQ", WLAN_QUEUE_MESSAGE, 18,
			  WLAN_QUEUE_CLASS);
	macMgmt80211Queue = (struct OsaMessageQueue *)&macMgmt80211QueueObject;
	osaMsgQueueCreate(&macMgmt80211Queue, "MacMgmt80211MsgQ", WLAN_QUEUE_MESSAGE, 6,
			  WLAN_QUEUE_CLASS);
	macMgmtSmeQueue = (struct OsaMessageQueue *)&macMgmtSmeQueueObject;
	osaMsgQueueCreate(wlanTaskHandles.mgmtSmeQueue, "MacMgmtSMEMsgQ", WLAN_QUEUE_MESSAGE, 6,
			  WLAN_QUEUE_CLASS);
	tx_queue_create((TX_QUEUE *)&macMgmtMaskQueueA, NULL, 1, macMgmtMaskStorageA,
			sizeof(macMgmtMaskStorageA));
	tx_queue_create((TX_QUEUE *)&macMgmtMaskQueueB, NULL, 1, macMgmtMaskStorageB,
			sizeof(macMgmtMaskStorageB));
	macMgmtEventFlags = (struct OsaEventFlags *)&macMgmtEventFlagsObject;
	osaEventFlagsInit(wlanTaskHandles.mgmtEventFlags);
	__aeabi_memset4(macMgmtStack, sizeof(macMgmtStack), 0xa1);
	macMgmtTask = &macMgmtTaskObject;
	return osaCreateTask(&macMgmtTask, macMgmtStack, sizeof(macMgmtStack),
			     WLAN_TASK_PRIORITY_MGMT, "MAC Mgmt", macMgmtTaskMain, 0) != 0;
}

int macMgmtStartTask(void)
{
	return osaTaskResume(macMgmtTask);
}

/* Creates the four control tasks; returns 1 if any creation failed. */
int cbProcPostInitCreateTasks(void)
{
	if (macTxCreateTask() != 0)
		return 1;
	if (macTxNotifyCreateTask())
		return 1;
	if (macMgmtCreateTask())
		return 1;
	if (cbProcCreateTask() != 0)
		return 1;
	return 0;
}

int cbProcPostInitStartTasks(void)
{
	macMgmtStartTask();
	cbProcStartTask();
	macTxStartTask();
	macTxNotifyStartTask();
	return 0;
}

/* Priority the "MAC Mgmt" task had before it was raised, 0xff when it was not. */
static uint8_t wlanSavedMgmtPriority __attribute__((section(".dtcm.rw"))) = 0xff;

/*
 * Suspends the four WLAN tasks except the caller. With raise set and only
 * the "other" role active, the "MAC Mgmt" task is first given priority 13.
 */
void wlanSuspendTasks(int raise)
{
	TX_THREAD *current;

	osaTaskGetCurrent(&current);
	if (current != macTxTask)
		osaTaskSuspend(macTxTask);
	if (current != macTxNotifyTask)
		osaTaskSuspend(macTxNotifyTask);
	if (wlanSavedMgmtPriority == 0xff && wlanRoleOtherOnly() && raise)
		osaTaskSetPriority(macMgmtTask, 13, &wlanSavedMgmtPriority);
	if (current != macMgmtTask)
		osaTaskSuspend(macMgmtTask);
	if (current != cbProcTask)
		osaTaskSuspend(cbProcTask);
}

void wlanResumeTasks(void)
{
	uint8_t previous;

	osaTaskResume(macTxTask);
	osaTaskResume(macTxNotifyTask);
	osaTaskResume(macMgmtTask);
	osaTaskResume(cbProcTask);
	if (wlanSavedMgmtPriority != 0xff) {
		osaTaskSetPriority(macMgmtTask, wlanSavedMgmtPriority, &previous);
		wlanSavedMgmtPriority = 0xff;
	}
}
