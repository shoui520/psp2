/*
 * OSA core: adapters between the OSA calling conventions and the ThreadX
 * services in the Mask ROM, the idle thread, and the version banner text.
 * The filename is inferred; no source filename survives.
 */
#include <stddef.h>
#include "robin/osa.h"
#include "robin/power.h"
#include "robin/rom.h"
#include "robin/sys.h"

/* ThreadX status 10 is TX_QUEUE_EMPTY and 16 TX_NO_MEMORY; others fold into 1. */
int osaTranslateStatus(int status)
{
	if (status == 0)
		return 0;
	if (status == 10)
		return OSA_QUEUE_EMPTY;
	if (status == 16)
		return OSA_NO_MEMORY;
	return OSA_ERROR;
}

/* Set to 1 by osaInit() once the workspace exists. */
uint8_t osaWorkspaceReady __attribute__((section(".dtcm.rw")));
struct OsaWorkspace *osaWorkspaceHandle __attribute__((section(".dtcm.rw")));
struct OsaWorkspace osaWorkspace __attribute__((section(".dtcm.bss")));

/* Returns 1 when the workspace has already been initialised. */
int osaGetWorkspace(struct OsaWorkspace **workspace)
{
	osaWorkspaceHandle = &osaWorkspace;
	*workspace = &osaWorkspace;
	if (osaWorkspaceReady)
		return 1;
	return 0;
}

void *osaClearWorkspace(void *workspace)
{
	__aeabi_memclr(workspace, sizeof(struct OsaWorkspace));
	return workspace;
}

int osaCreateThread(TX_THREAD *thread, void *stack, ULONG stackSize,
		    UINT priority, CHAR *name, void (*entry)(ULONG), ULONG input)
{
	int status;

	status = tx_thread_create(thread, name, entry, input, stack, stackSize,
				  priority, priority, 0, TX_DONT_START);
	if (status != 0)
		return osaTranslateStatus(status);
	return status;
}

/* Releases a byte or block pool allocation; the workspace is not consulted. */
int osaFreeMemory(struct OsaWorkspace *workspace, uint32_t type, void *memory)
{
	int status;

	if (type == OSA_POOL_BYTE)
		status = tx_byte_release(memory);
	else
		status = tx_block_release(memory);
	if (status != 0)
		return osaTranslateStatus(status);
	return status;
}

int osaAllocateMemory(struct OsaWorkspace *workspace, uint32_t type, uint32_t size,
		      void **memory, uint32_t wait)
{
	int status;

	if (type == OSA_POOL_BYTE)
		status = tx_byte_allocate((TX_BYTE_POOL *)workspace, memory, size, wait);
	else
		status = tx_block_allocate((TX_BLOCK_POOL *)workspace, memory, wait);
	if (status != 0)
		return osaTranslateStatus(status);
	return status;
}

/*
 * Message queues are created with one-word messages over a byte-pool
 * allocation of messageSize * capacity bytes; a failed creation hangs.
 */
int osaCreateMessageQueue(struct OsaMessageQueue *queue, const char *name,
			  uint32_t messageSize, uint32_t capacity,
			  void *storage, uint32_t allocationClass)
{
	uint32_t size = messageSize * capacity;
	void *memory = storage;
	int status;

	if (storage == NULL) {
		status = osaAllocateMemory(osaWorkspaceHandle, OSA_POOL_BYTE, size,
					   &memory, 0);
		if (status != 0) {
			osaFreeMemory(osaWorkspaceHandle, OSA_POOL_BYTE, memory);
			return (uint8_t)status;
		}
	}
	if (tx_queue_create((TX_QUEUE *)queue, (CHAR *)name, 1, memory, size) != 0)
		for (;;)
			;
	return 0;
}

/* Sends without waiting; the timeout argument is not used. */
int osaSendMessageQueue(struct OsaMessageQueue *queue, uint32_t messageSize,
			uintptr_t message, uint32_t timeout)
{
	return osaTranslateStatus(tx_queue_send((TX_QUEUE *)queue, &message,
						TX_NO_WAIT));
}

int osaReceiveMessageQueue(struct OsaMessageQueue *queue, uintptr_t *message,
			   uint32_t messageSize, uint32_t timeout)
{
	return osaTranslateStatus(tx_queue_receive((TX_QUEUE *)queue, message,
						   timeout));
}

/* Name shared by the event flag groups created through OSA. */
static const char *osaObjectName __attribute__((section(".itcm.rw"))) = "OSA";

int osaCreateEventFlags(struct OsaEventFlags *flags)
{
	return osaTranslateStatus(tx_event_flags_create(
		(TX_EVENT_FLAGS_GROUP *)flags, (CHAR *)osaObjectName));
}

/* Modes 5 and 6 mean AND, 7 and 8 OR; the set option has no clear variants. */
int osaSetEventFlags(struct OsaEventFlags *flags, uint32_t events, uint32_t mode)
{
	if (mode == 5 || mode == 6)
		mode = TX_AND;
	else if (mode == 7 || mode == 8)
		mode = TX_OR;
	return osaTranslateStatus(tx_event_flags_set(
		(TX_EVENT_FLAGS_GROUP *)flags, events, mode));
}

/* Reads the current flag bits of a group into *flags. */
int osaGetEventFlagsState(TX_EVENT_FLAGS_GROUP *group, uint32_t *flags)
{
	CHAR *name;
	TX_THREAD *firstSuspended;
	ULONG suspendedCount;
	TX_EVENT_FLAGS_GROUP *nextGroup;

	return osaTranslateStatus(tx_event_flags_info_get(group, &name, (ULONG *)flags,
							  &firstSuspended,
							  &suspendedCount,
							  &nextGroup));
}

/*
 * Mode 5 is AND, 6 AND with clear, 7 OR and 8 OR with clear; other values
 * go to ThreadX unchanged. A wait that is aborted or finds no events still
 * reports the current flag bits.
 */
int osaWaitEventFlags(struct OsaEventFlags *flags, uint32_t requested,
		      uint32_t mode, uint32_t *actual, uint32_t timeout)
{
	ULONG got = 0;
	int status;

	switch (mode) {
	case 5:
		mode = TX_AND;
		break;
	case 6:
		mode = TX_AND_CLEAR;
		break;
	case 7:
		mode = TX_OR;
		break;
	case 8:
		mode = TX_OR_CLEAR;
		break;
	}
	status = tx_event_flags_get((TX_EVENT_FLAGS_GROUP *)flags, requested, mode,
				    &got, timeout);
	*actual = got;
	if (status == 26 || status == 7) {
		osaGetEventFlagsState((TX_EVENT_FLAGS_GROUP *)flags, actual);
		if (status == 26)
			return 2;
		return 33;
	}
	if (status == 0)
		return 0;
	return osaTranslateStatus(status);
}

int osaCreateSemaphore(struct OsaSemaphore *semaphore, uint32_t count)
{
	return osaTranslateStatus(tx_semaphore_create(
		(TX_SEMAPHORE *)semaphore, "OSA_Sema", count));
}

int osaWaitSemaphore(struct OsaSemaphore *semaphore, uint32_t timeout)
{
	return osaTranslateStatus(tx_semaphore_get((TX_SEMAPHORE *)semaphore,
						   timeout));
}

int osaReleaseSemaphore(struct OsaSemaphore *semaphore)
{
	return osaTranslateStatus(tx_semaphore_put((TX_SEMAPHORE *)semaphore));
}

/* Pool objects are anonymous: the name is a pointer to an empty string. */
static char osaPoolName[4] __attribute__((section(".itcm.rw")));

int osaCreatePool(void *pool, uint32_t type, void *start, uint32_t size,
		  uint32_t blockSize)
{
	int status;

	if (type == OSA_POOL_BYTE)
		status = tx_byte_pool_create((TX_BYTE_POOL *)pool, osaPoolName,
					     start, size);
	else
		status = tx_block_pool_create((TX_BLOCK_POOL *)pool, osaPoolName,
					      blockSize, start, size);
	if (status != 0)
		return osaTranslateStatus(status);
	return status;
}

int osaDeletePool(void *pool, uint32_t type)
{
	int status;

	if (type == OSA_POOL_BYTE)
		status = tx_byte_pool_delete((TX_BYTE_POOL *)pool);
	else
		status = tx_block_pool_delete((TX_BLOCK_POOL *)pool);
	if (status != 0)
		return osaTranslateStatus(status);
	return status;
}

/* Functions run on every pass of the idle thread. */
struct OsaIdleHook *osaIdleHooks __attribute__((section(".itcm.rw")));

/* The idle thread: prepare the power state, then poll the hooks forever. */
static void osaIdleTask(ULONG input)
{
	struct OsaIdleHook *hook;

	powerIdlePrepare();
	mdevIoctl(btSdioDev, 0xd, 0);
	for (;;) {
		for (hook = osaIdleHooks; hook != NULL; hook = hook->next) {
			if (hook->function != NULL)
				hook->function();
		}
		powerIdleStep();
	}
}

static TX_THREAD osaIdleThread __attribute__((section(".itcm.bss")));
static uint8_t osaIdleStack[0x1cc] __attribute__((section(".itcm.bss")));

/* Returns 1 if the idle thread could not be created. */
int osaCreateIdleThread(void)
{
	osaIdleHooks = NULL;
	if ((uint8_t)tx_thread_create(&osaIdleThread, "Idle", osaIdleTask, 0,
				      osaIdleStack, sizeof(osaIdleStack), 30, 30, 0,
				      TX_DONT_START) != 0)
		return 1;
	return 0;
}

int osaStartIdleThread(void)
{
	return tx_thread_resume(&osaIdleThread);
}

/* Adds a hook to the idle list unless it is already linked. */
int osaAddIdleHook(struct OsaIdleHook *hook)
{
	struct OsaIdleHook *entry;
	UINT posture;

	posture = sysIrqDisable();
	for (entry = osaIdleHooks; entry != NULL; entry = entry->next) {
		if (entry == hook)
			return sysIrqRestore(posture);
	}
	hook->next = osaIdleHooks;
	osaIdleHooks = hook;
	return sysIrqRestore(posture);
}

/* Lowercase hexadecimal digit of a nibble. */
char osaHexDigit(uint32_t nibble)
{
	if (nibble >= 10)
		return nibble + 'a' - 10;
	return nibble + '0';
}

/* Writes the two hexadecimal digits of a byte, high nibble first. */
void osaFormatHexByte(uint32_t value, char *output)
{
	output[0] = osaHexDigit(value >> 4);
	output[1] = osaHexDigit(value & 0xf);
}

/*
 * Searches a chain of type/length/value elements for one of the given type.
 * The remaining length is reduced by the length byte of the element reached
 * after the step, as the original does.
 */
const uint8_t *osaFindTlv(const uint8_t *data, int type, int length)
{
	while (length >= 2) {
		if (*data == type)
			return data;
		data += data[1] + 2;
		length = length - data[1] - 2;
	}
	return NULL;
}

/* Driver registrations run once at start-up; the names below are provisional. */
int btSdioHciScheduleHeader(void);	/* 0x00026224 */
int btSdioHciAdjustState(void);		/* 0x00020e2c */
int fmCoexFilterBuffer(void);		/* 0x0004cee0 */
int fmCoexHandlePacket(void);		/* 0x0004cf4c */
int btSbcUpdateState(void);		/* 0x0004d548 */
int btSbcBuildEntry(void);		/* 0x0004f490 */
int btSdioHciFormatBuffer(void);	/* 0x0002366c */
int ampPalBuildState(void);		/* 0x00002782 */

void osaInitSubsystems(void)
{
	btSdioHciScheduleHeader();
	btSdioHciAdjustState();
	fmCoexFilterBuffer();
	fmCoexHandlePacket();
	btSbcUpdateState();
	btSbcBuildEntry();
	btSdioHciFormatBuffer();
	ampPalBuildState();
}
