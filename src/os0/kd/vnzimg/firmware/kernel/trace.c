/*
 * The trace: a record of kernel events (thread switches, creation, deletion)
 * and of the application's own, per core, in a buffer the ARM side gives.
 *
 * Each core writes its share of the buffer as an array of records; a share
 * either wraps (VK_TRACE_WRAP) or stops the trace when it is full.
 */
#include "kernel.h"

#define VK_TRACE_CONTROL_SIZE	0x240	/* the control block and the cores' headers */
#define VK_TRACE_WRAP		1

/* a core's share of the trace buffer (0x40 bytes) */
typedef struct VkTraceShare {
	VkTraceRecord *records;
	uint32_t size;
	uint32_t next;			/* the next record */
	uint32_t capacity;		/* records */
	uint32_t fixed;			/* the size was set (vkTraceSetShare) */
	uint32_t wraps;
	uint32_t reserved18[10];
} VkTraceShare;

/* the trace's control block, at the start of the buffer */
typedef struct VkTraceControl {
	volatile uint8_t *lock;		/* a lock page */
	void *buffer;
	uint32_t bufferSize;
	VkTraceShare *shares;		/* VNZ_CORES of them */
	uint32_t sharesSize;
	void *records;
	uint32_t recordsSize;
	uint32_t cores;
	uint32_t mode;			/* VK_TRACE_WRAP: wrap when full */
	uint32_t sequence;		/* of vkTraceStart */
} VkTraceControl;

VkTraceControl *g_vkTrace VK_SHARED;
uint16_t g_vkTraceMask VK_SHARED;	/* the event classes traced */
uint16_t g_vkTraceEnabled VK_SHARED;
uint32_t g_vkTraceFull VK_SHARED;	/* a share filled up: the trace stopped */

void vkTraceInitialize(uint32_t primary)
{
	if (primary) {
		g_vkTraceFull = 0;
		g_vkTraceEnabled = 0;
		g_vkTraceMask = 0;
		g_vkTrace = NULL;
	}
}

/*
 * Append a record to a core's share. Returns 0, or -2 when the trace has
 * stopped.
 */
static int32_t traceRecord(uint32_t index, uint32_t timestamp, uint32_t coreId,
	uint32_t event, uint32_t argument, uint32_t argument2, uint32_t argument3)
{
	VkTraceShare *share;
	VkTraceRecord *record;
	uint32_t psw, next;

	if (g_vkTraceFull == 1)
		return -2;

	share = &g_vkTrace->shares[index];
	psw = vnzReadControl(VNZ_CR_PSW);
	vnzDisableInterrupts();
	record = &share->records[share->next];
	record->timestamp = timestamp;
	record->coreId = coreId;
	record->event = event;
	record->argument = argument;
	record->argument2 = argument2;
	record->argument3 = argument3;

	next = share->next + 1;
	if (next < share->capacity) {
		share->next = next;
	} else if (g_vkTrace->mode == VK_TRACE_WRAP) {
		share->next = 0;
		share->wraps++;
	} else {
		share->next = next;
		g_vkTraceFull = 1;
	}
	vnzWriteControl(VNZ_CR_PSW, psw);
	return 0;
}

/* Record an event on this core. */
int32_t vkTraceWrite(uint32_t event, uint32_t argument, uint32_t argument2, uint32_t argument3)
{
	uint32_t timestamp, coreId;
	int32_t result;

	if (vnzReadControl(VNZ_CR_PSW) & VNZ_PSW_IEC)
		vkDisableDispatch();
	timestamp = vnzRegister(VNZ_FREE_RUNNING_TIMER);
	coreId = (int32_t)vnzReadControl(VNZ_CR_ID) >> 16;
	result = traceRecord(coreId & 7, timestamp, coreId, event, argument, argument2, argument3);
	if (vnzReadControl(VNZ_CR_PSW) & VNZ_PSW_IEC)
		vkEnableDispatch();
	return result;
}

/* Record a kernel event when its class is traced. */
void vkTrace(uint32_t event, uint32_t argument, uint32_t argument2, uint32_t argument3,
	uint32_t class)
{
	if (g_vkTraceMask & class)
		vkTraceWrite(event, argument, argument2, argument3);
}

uint32_t vkTraceMask(void)
{
	return g_vkTraceMask;
}

/* Record one of the application's events (classes 0x01-0xff, events below 0x100). */
int32_t vkTraceUser(uint32_t class, uint32_t event, uint32_t argument, uint32_t argument2,
	uint32_t argument3)
{
	if ((class & 0xff00) || event >= 0x100)
		return -1;
	if (class & vkTraceMask())
		return vkTraceWrite(event, argument, argument2, argument3);
	return 0;
}

/*
 * Set up the trace in buffer (size bytes). The cores share what follows the
 * control block equally. Returns 0, or -1 when the buffer is too small or there
 * is no lock page.
 */
int32_t vkTraceSetup(void *buffer, uint32_t size, uint32_t mode)
{
	VkTraceControl *control = buffer;
	VkTraceShare *share;
	uint8_t *records;
	uint32_t shareSize, i;

	if (size < VK_TRACE_CONTROL_SIZE)
		return -1;
	control->lock = vkLockPageAllocate();
	if (control->lock == NULL)
		return -1;

	control->recordsSize = size - VK_TRACE_CONTROL_SIZE;
	shareSize = (control->recordsSize >> 3) & ~0x3f;
	control->buffer = buffer;
	control->bufferSize = size;
	control->mode = mode;
	control->shares = (VkTraceShare *)((uint8_t *)buffer + 0x40);
	control->sharesSize = 0x200;
	records = (uint8_t *)control->shares + 0x200;
	control->cores = VNZ_CORES;
	control->records = records;
	control->sequence = 0;

	for (i = 0; i < VNZ_CORES; i++) {
		share = &control->shares[i];
		share->records = (VkTraceRecord *)records;
		records += shareSize;
		share->size = shareSize;
		share->next = 0;
		share->fixed = 0;
		share->wraps = 0;
		share->capacity = shareSize / sizeof(VkTraceRecord);
	}
	g_vkTraceFull = 0;
	g_vkTrace = control;
	return 0;
}

/*
 * Give a core a share of its own size; the cores whose share was not set split
 * the rest. Returns 0, or -1 when the trace is running or there is not enough.
 */
int32_t vkTraceSetShare(uint32_t coreId, uint32_t size)
{
	VkTraceControl *control = g_vkTrace;
	VkTraceShare *shares;
	uint32_t index, fixedSize, fixedCount, left, others, otherCapacity, i;
	uint8_t *records;

	if (control == NULL || control->sequence)
		return -1;

	index = coreId - VNZ_FIRST_CORE_ID;
	shares = control->shares;
	fixedSize = 0;
	fixedCount = 0;
	for (i = 0; i < VNZ_CORES; i++) {
		if (shares[i].fixed == 1 && i != index) {
			fixedCount++;
			fixedSize += shares[i].size;
		}
	}

	size &= ~0x3f;
	left = control->recordsSize - fixedSize;
	if (left < size)
		return -1;
	left -= size;
	fixedCount++;

	shares[index].size = size;
	shares[index].capacity = size / sizeof(VkTraceRecord);
	shares[index].fixed = 1;

	others = VNZ_CORES - fixedCount;
	otherCapacity = 0;
	if (others) {
		size = (left / others) & ~0x3f;
		otherCapacity = size / sizeof(VkTraceRecord);
	}

	records = control->records;
	for (i = 0; i < VNZ_CORES; i++) {
		if (shares[i].fixed == 0) {
			shares[i].size = size;
			shares[i].capacity = otherCapacity;
		}
		shares[i].records = (VkTraceRecord *)records;
		records += shares[i].size;
	}
	return 0;
}

/* Start tracing the event classes in mask; every core's share gets a start record. */
int32_t vkTraceStart(uint32_t mask)
{
	VkTraceControl *control = g_vkTrace;
	uint32_t timestamp = vnzRegister(VNZ_FREE_RUNNING_TIMER);
	uint32_t sequence = control->sequence + 1;
	uint32_t i;

	for (i = 0; i < VNZ_CORES; i++)
		if (traceRecord(i, timestamp, VNZ_FIRST_CORE_ID + i, VK_TRACE_START,
		    sequence, 0, 0) == -2)
			return -2;
	control->sequence = sequence;
	g_vkTraceEnabled = 1;
	g_vkTraceMask = mask;
	return 0;
}

/* Stop tracing; every core's share gets a stop record, this core's first. */
int32_t vkTraceStop(void)
{
	uint32_t timestamp, sequence, coreId, index, i;
	int32_t result = 0;

	g_vkTraceEnabled = 0;
	g_vkTraceMask = 0;
	timestamp = vnzRegister(VNZ_FREE_RUNNING_TIMER);
	sequence = g_vkTrace->sequence;
	vkDisableDispatch();
	coreId = vkCurrentCoreId();
	index = coreId - VNZ_FIRST_CORE_ID;
	traceRecord(index, timestamp, coreId, VK_TRACE_STOP, sequence, 0, 0);
	for (i = 0; i < VNZ_CORES; i++) {
		if (i == index)
			continue;
		result = traceRecord(i, timestamp, VNZ_FIRST_CORE_ID + i, VK_TRACE_STOP,
			sequence, 0, 0);
		if (result == -2)
			break;
	}
	vkEnableDispatch();
	return result;
}

/* Let go of the buffer. */
void vkTraceRelease(void)
{
	vkLockPageFree(g_vkTrace->lock);
	g_vkTrace = NULL;
}

/* The trace buffer and its size. */
void *vkTraceBuffer(uint32_t *size)
{
	VkTraceControl *control = g_vkTrace;

	if (control == NULL) {
		*size = 0;
		return NULL;
	}
	*size = control->bufferSize;
	return control->buffer;
}
