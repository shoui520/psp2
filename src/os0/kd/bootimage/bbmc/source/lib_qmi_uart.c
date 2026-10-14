/* UART framing queue and encoded packet transport. */
#include <string.h>
#include <sce/debug.h>
#include "qmi_uart_framing.h"

static QmiUartFramingControl g_qmiUartFramingStorage;
static QmiUartFramingControl *g_qmiUartFramingControl;

static const SceKernelDebugInfo g_qmiUartReadPositionDebug
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "uart_framing/lib_qmi_uart.c",
	.line = 174,
	.func = "_getEncDataReadPosition",
#endif
};

static const SceKernelDebugInfoSource g_qmiUartReadPositionDebugSource
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&g_qmiUartReadPositionDebug, "uart_framing/lib_qmi_uart.c", 174,
	"_getEncDataReadPosition",
};

#if FW_DEBUG
static const SceKernelDebugInfo g_qmiUartAckQueueDebug
	__attribute__((section(".sce_dbginfo"), used)) = {
	.file = "uart_framing/lib_qmi_uart.c",
	.line = 948,
	.func = "_addAckNakTaskList",
};

static const SceKernelDebugInfoSource g_qmiUartAckQueueDebugSource
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&g_qmiUartAckQueueDebug, "uart_framing/lib_qmi_uart.c", 948,
	"_addAckNakTaskList",
};
#endif

/* ASCII 0x20..0x7f to six-bit values; retain the original invalid mappings. */
static const unsigned char g_qmiUartDecodeAlphabet[96] = {
	/* 0x20..0x2f */
	255, 255, 255, 62, 255, 63, 255, 255,
	255, 255, 255, 255, 255, 255, 255, 255,
	/* 0x30..0x3f */
	52, 53, 54, 55, 56, 57, 58, 59,
	60, 61, 255, 255, 255, 255, 255, 255,
	/* 0x40..0x4f */
	255, 0, 1, 2, 3, 4, 5, 6,
	7, 8, 9, 10, 11, 12, 13, 14,
	/* 0x50..0x5f */
	15, 16, 17, 18, 19, 20, 21, 22,
	23, 24, 25, 255, 255, 255, 255, 255,
	/* 0x60..0x6f */
	255, 26, 27, 28, 29, 30, 31, 32,
	33, 34, 35, 36, 37, 38, 39, 40,
	/* 0x70..0x7f */
	41, 42, 43, 44, 45, 46, 47, 48,
	49, 50, 51, 0, 0, 0, 0, 0,
};

/* Six-bit wire alphabet; '#' and '%' replace the usual last two symbols. */
static const unsigned char g_qmiUartEncodeAlphabet[64] =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789#%";

int _addAckNakTaskList(QmiUartFramingControl *control, unsigned int kind,
	unsigned int sequence)
{
	QmiUartFrameEntry *entry;
	QmiUartFrameEntry *next;
	QmiUartFrameEntry *head;
	QmiUartFrameEntry *cursor;
	QmiUartFrameEntry *prev;
	QmiUartFrameEntry *tail;
	unsigned int freeCount;

	if (control == NULL || kind - 1 > 1 || sequence > 255)
		return (int)0x80330002;
	if (control->freeCount == 0)
		return (int)0x80330400;
	entry = control->producer;
	if (kind == 1) {
		entry->buffer.acknowledgment.markerHigh = 6;
		entry->buffer.acknowledgment.markerLow = 249;
	} else {
		entry->buffer.acknowledgment.markerHigh = 21;
		entry->buffer.acknowledgment.markerLow = 234;
	}
	entry->buffer.acknowledgment.sequence = sequence;
	tail = control->tail;
	next = entry->next;
	freeCount = control->freeCount - 1;
	entry->sequence = sequence;
	entry->state = 1;
	entry->fragmentCount = 1;
	entry->kind = kind;
	entry->length = 3;
	entry->fragmentIndex = 0;
	control->producer = next;
	control->freeCount = freeCount;
	if (tail == entry)
		control->tail = entry->prev;
	head = control->head;
	if (head == entry) {
		control->head = next;
		head = next;
	}
	prev = entry->prev;
	if (prev != NULL)
		prev->next = entry->next;
	if (entry->next != NULL)
		entry->next->prev = prev;
	entry->prev = NULL;
	entry->next = NULL;
	if (next != head && head->kind - 1 <= 1) {
		cursor = head;
		for (;;) {
			cursor = cursor->next;
			if (cursor == next)
				break;
			if (cursor->kind - 1 > 1) {
				if (cursor == head)
					goto prepend;
				break;
			}
		}
		if (cursor != NULL) {
			prev = cursor->prev;
			prev->next = entry;
			entry->prev = prev;
			entry->next = cursor;
			cursor->prev = entry;
		} else {
			prev = control->tail;
			entry->prev = prev;
			prev->next = entry;
			control->tail = entry;
		}
		return 0;
	}
prepend:
	entry->next = head;
#if FW_DEBUG
	ksceKernelAssert(head != NULL, &g_qmiUartAckQueueDebug, __builtin_return_address(0));
#endif
	control->head->prev = entry;
	control->head = entry;
	return 0;
}

/* name unknown: the separate terminal path for an invalid encoded read position. */
void qmiPanicUartReadPosition(void) __attribute__((noinline, noreturn));

void qmiPanicUartReadPosition(void)
{
	ksceKernelPanic(&g_qmiUartReadPositionDebug, __builtin_return_address(0));
}

/* name unknown: apply a received ACK or NAK to queued UART fragments. */
int qmiProcessUartAcknowledgment(QmiUartFramingControl *control, unsigned int kind)
{
	QmiUartFrameEntry *entry;
	QmiUartFrameEntry *head;
	QmiUartFrameEntry *tail;
	QmiUartFrameEntry *prev;
	QmiUartFrameEntry *next;
	unsigned int sequence;
	unsigned int freeCount;
	unsigned int fragmentCount;
	unsigned int fragmentIndex;
	unsigned int nextSequence;
	unsigned int endSequence;
	int ret;

	if (kind == 0 || control == NULL)
		return (int)0x80330002;
	if (control->receiveCount <= 2)
		return 2;
	sequence = control->receiveData[(control->receiveConsumer + 2) & 2047];
	if (sequence == 255 && kind == 1)
		return (int)0x80330402;
	ret = kind == 1;
	control->lock(control->context);
	if (control->freeCount == 16 || control->head == NULL)
		goto missing;
	entry = control->head;
	if (sequence == 255) {
		while (entry->kind != 0 || entry->state == 0) {
			entry = entry->next;
			if (entry == NULL)
				goto missing;
		}
	} else {
		while (entry->kind != 0 || entry->state == 0 || entry->sequence != sequence) {
			entry = entry->next;
			if (entry == NULL)
				goto missing;
		}
	}
	control->unlock(control->context);
	if (ret != 0) {
		fragmentCount = entry->fragmentCount;
		if (fragmentCount > 1 && fragmentCount == entry->fragmentIndex + 1)
			control->multipartPending = 0;
		control->lock(control->context);
		goto release;
	}
	if (entry->attempts <= 3) {
		entry->state = 1;
		goto finish;
	}
	control->lock(control->context);
	fragmentCount = entry->fragmentCount;
	if (fragmentCount > 1) {
		fragmentIndex = entry->fragmentIndex;
		if (fragmentCount > fragmentIndex + 1) {
			sequence = entry->sequence;
			if (fragmentCount <= fragmentIndex) {
				ret = 0;
				goto unlocked;
			}
			nextSequence = sequence + 1;
			endSequence = fragmentCount + sequence - fragmentIndex;
			sequence &= 127;
			if (control->freeCount == 16)
				goto groupMissing;
			for (;;) {
				head = control->head;
				if (head == NULL)
					goto groupMissing;
				entry = head;
				while (entry->kind != 0 || entry->state == 0 ||
				       entry->sequence != sequence) {
					entry = entry->next;
					if (entry == NULL)
						goto groupMissing;
				}
				freeCount = control->freeCount;
				if (freeCount == 16)
					goto full;
				tail = control->tail;
				entry->state = 0;
				entry->kind = 0;
				entry->length = 0;
				entry->sequence = 0;
				entry->fragmentIndex = 0;
				entry->fragmentCount = 0;
				entry->attempts = 0;
				if (tail != entry) {
					prev = entry->prev;
					if (prev != NULL)
						prev->next = entry->next;
					next = entry->next;
					if (next != NULL)
						next->prev = prev;
					if (head == entry) {
						control->head = next;
						next->prev = NULL;
					}
					tail->next = entry;
					entry->prev = tail;
					entry->next = NULL;
					control->tail = entry;
				}
				if (control->producer == NULL)
					control->producer = entry;
				control->freeCount = freeCount + 1;
				if (nextSequence == endSequence) {
					ret = 0;
					goto unlocked;
				}
				sequence = nextSequence++ & 127;
				if (freeCount == 15)
					break;
			}
groupMissing:
			ret = (int)0x80330404;
			goto unlocked;
		}
	}
release:
	freeCount = control->freeCount;
	if (freeCount == 16)
		goto full;
	tail = control->tail;
	entry->state = 0;
	entry->kind = 0;
	entry->length = 0;
	entry->sequence = 0;
	entry->fragmentIndex = 0;
	entry->fragmentCount = 0;
	entry->attempts = 0;
	if (tail != entry) {
		prev = entry->prev;
		if (prev != NULL)
			prev->next = entry->next;
		next = entry->next;
		if (next != NULL)
			next->prev = prev;
		if (control->head == entry) {
			control->head = next;
			next->prev = NULL;
		}
		tail->next = entry;
		entry->prev = tail;
		entry->next = NULL;
		control->tail = entry;
	}
	if (control->producer == NULL)
		control->producer = entry;
	control->freeCount = freeCount + 1;
	ret = 0;
	goto unlocked;
full:
	ret = (int)0x80330403;
unlocked:
	control->unlock(control->context);
	goto finish;
missing:
	ret = (int)0x80330404;
	control->unlock(control->context);
finish:
	control->lock(control->context);
	control->waitingAcknowledgment = 0;
	control->unlock(control->context);
	control->receiveConsumer = (control->receiveConsumer + 3) & 2047;
	control->receiveCount -= 3;
	return ret;
}

int qmiInitializeUartFraming(unsigned int format, int (*lock)(void *),
	int (*unlock)(void *), void *context,
	int (*send)(const void *, unsigned int), unsigned int (*transmitPending)(void))
{
	unsigned int invalid = lock == NULL;

	if (format > 1)
		invalid |= 1;
	if (invalid != 0)
		return (int)0x80330002;
	if (send == NULL || unlock == NULL || transmitPending == NULL)
		return (int)0x80330002;
	if (g_qmiUartFramingControl != NULL)
		return (int)0x80330004;
	memset(&g_qmiUartFramingStorage, 0, sizeof(g_qmiUartFramingStorage));
	g_qmiUartFramingStorage.lock = lock;
	g_qmiUartFramingStorage.unlock = unlock;
	g_qmiUartFramingStorage.send = send;
	g_qmiUartFramingStorage.isTransmitPending = transmitPending;
	g_qmiUartFramingStorage.context = context;
	g_qmiUartFramingStorage.previousReceiveSequence = 255;
	g_qmiUartFramingStorage.lastReceiveSequence = 255;
	if (format != 0) {
		g_qmiUartFramingStorage.transmitStartMarker = 510;
		g_qmiUartFramingStorage.transmitEndMarker = 1275;
		g_qmiUartFramingStorage.receiveStartMarker = 765;
		g_qmiUartFramingStorage.receiveEndMarker = 1020;
	} else {
		g_qmiUartFramingStorage.transmitStartMarker = 765;
		g_qmiUartFramingStorage.transmitEndMarker = 1020;
		g_qmiUartFramingStorage.receiveStartMarker = 510;
		g_qmiUartFramingStorage.receiveEndMarker = 1275;
	}
	lock(context);
	g_qmiUartFramingStorage.entries[0].state = 0;
	g_qmiUartFramingStorage.entries[0].prev = NULL;
	g_qmiUartFramingStorage.entries[0].next = &g_qmiUartFramingStorage.entries[1];
	g_qmiUartFramingStorage.entries[1].state = 0;
	g_qmiUartFramingStorage.entries[1].prev = &g_qmiUartFramingStorage.entries[0];
	g_qmiUartFramingStorage.entries[1].next = &g_qmiUartFramingStorage.entries[2];
	g_qmiUartFramingStorage.entries[2].state = 0;
	g_qmiUartFramingStorage.entries[2].prev = &g_qmiUartFramingStorage.entries[1];
	g_qmiUartFramingStorage.entries[2].next = &g_qmiUartFramingStorage.entries[3];
	g_qmiUartFramingStorage.entries[3].state = 0;
	g_qmiUartFramingStorage.entries[3].prev = &g_qmiUartFramingStorage.entries[2];
	g_qmiUartFramingStorage.entries[3].next = &g_qmiUartFramingStorage.entries[4];
	g_qmiUartFramingStorage.entries[4].state = 0;
	g_qmiUartFramingStorage.entries[4].prev = &g_qmiUartFramingStorage.entries[3];
	g_qmiUartFramingStorage.entries[4].next = &g_qmiUartFramingStorage.entries[5];
	g_qmiUartFramingStorage.entries[5].state = 0;
	g_qmiUartFramingStorage.entries[5].prev = &g_qmiUartFramingStorage.entries[4];
	g_qmiUartFramingStorage.entries[5].next = &g_qmiUartFramingStorage.entries[6];
	g_qmiUartFramingStorage.entries[6].state = 0;
	g_qmiUartFramingStorage.entries[6].prev = &g_qmiUartFramingStorage.entries[5];
	g_qmiUartFramingStorage.entries[6].next = &g_qmiUartFramingStorage.entries[7];
	g_qmiUartFramingStorage.entries[7].state = 0;
	g_qmiUartFramingStorage.entries[7].prev = &g_qmiUartFramingStorage.entries[6];
	g_qmiUartFramingStorage.entries[7].next = &g_qmiUartFramingStorage.entries[8];
	g_qmiUartFramingStorage.entries[8].state = 0;
	g_qmiUartFramingStorage.entries[8].prev = &g_qmiUartFramingStorage.entries[7];
	g_qmiUartFramingStorage.entries[8].next = &g_qmiUartFramingStorage.entries[9];
	g_qmiUartFramingStorage.entries[9].state = 0;
	g_qmiUartFramingStorage.entries[9].prev = &g_qmiUartFramingStorage.entries[8];
	g_qmiUartFramingStorage.entries[9].next = &g_qmiUartFramingStorage.entries[10];
	g_qmiUartFramingStorage.entries[10].state = 0;
	g_qmiUartFramingStorage.entries[10].prev = &g_qmiUartFramingStorage.entries[9];
	g_qmiUartFramingStorage.entries[10].next = &g_qmiUartFramingStorage.entries[11];
	g_qmiUartFramingStorage.entries[11].state = 0;
	g_qmiUartFramingStorage.entries[11].prev = &g_qmiUartFramingStorage.entries[10];
	g_qmiUartFramingStorage.entries[11].next = &g_qmiUartFramingStorage.entries[12];
	g_qmiUartFramingStorage.entries[12].state = 0;
	g_qmiUartFramingStorage.entries[12].prev = &g_qmiUartFramingStorage.entries[11];
	g_qmiUartFramingStorage.entries[12].next = &g_qmiUartFramingStorage.entries[13];
	g_qmiUartFramingStorage.entries[13].state = 0;
	g_qmiUartFramingStorage.entries[13].prev = &g_qmiUartFramingStorage.entries[12];
	g_qmiUartFramingStorage.entries[13].next = &g_qmiUartFramingStorage.entries[14];
	g_qmiUartFramingStorage.entries[14].state = 0;
	g_qmiUartFramingStorage.entries[14].prev = &g_qmiUartFramingStorage.entries[13];
	g_qmiUartFramingStorage.entries[14].next = &g_qmiUartFramingStorage.entries[15];
	g_qmiUartFramingStorage.entries[15].state = 0;
	g_qmiUartFramingStorage.entries[15].prev = &g_qmiUartFramingStorage.entries[14];
	g_qmiUartFramingStorage.entries[15].next = NULL;
	g_qmiUartFramingStorage.head = &g_qmiUartFramingStorage.entries[0];
	g_qmiUartFramingStorage.tail = &g_qmiUartFramingStorage.entries[15];
	g_qmiUartFramingStorage.producer = &g_qmiUartFramingStorage.entries[0];
	g_qmiUartFramingStorage.freeCount = 16;
	g_qmiUartFramingStorage.unlock(g_qmiUartFramingStorage.context);
	g_qmiUartFramingControl = &g_qmiUartFramingStorage;
	g_qmiUartFramingStorage.enabled = 1;
	return 0;
}

int qmiFinalizeUartFraming(void)
{
	QmiUartFramingControl *control = g_qmiUartFramingControl;

	if (control == NULL)
		return (int)0x80330003;
	control->entries[0].state = 0;
	control->entries[1].state = 0;
	control->entries[2].state = 0;
	control->entries[3].state = 0;
	control->entries[4].state = 0;
	control->entries[5].state = 0;
	control->entries[6].state = 0;
	control->entries[7].state = 0;
	control->entries[8].state = 0;
	control->entries[9].state = 0;
	control->entries[10].state = 0;
	control->entries[11].state = 0;
	control->entries[12].state = 0;
	control->entries[13].state = 0;
	control->entries[14].state = 0;
	control->entries[15].state = 0;
	memset(&g_qmiUartFramingStorage, 0, sizeof(g_qmiUartFramingStorage));
	g_qmiUartFramingControl = NULL;
	return 0;
}

int qmiSuspendUartFraming(void)
{
	QmiUartFramingControl *control = g_qmiUartFramingControl;

	if (control == NULL)
		return (int)0x80330003;
	control->entries[0].state = 0;
	control->entries[1].state = 0;
	control->entries[2].state = 0;
	control->entries[3].state = 0;
	control->entries[4].state = 0;
	control->entries[5].state = 0;
	control->entries[6].state = 0;
	control->entries[7].state = 0;
	control->entries[8].state = 0;
	control->entries[9].state = 0;
	control->entries[10].state = 0;
	control->entries[11].state = 0;
	control->entries[12].state = 0;
	control->entries[13].state = 0;
	control->entries[14].state = 0;
	control->entries[15].state = 0;
	return 0;
}

int qmiResumeUartFraming(void)
{
	if (g_qmiUartFramingControl == NULL)
		return (int)0x80330003;
	g_qmiUartFramingStorage.lock(g_qmiUartFramingStorage.context);
	g_qmiUartFramingStorage.entries[0].state = 0;
	g_qmiUartFramingStorage.entries[0].prev = NULL;
	g_qmiUartFramingStorage.entries[0].next = &g_qmiUartFramingStorage.entries[1];
	g_qmiUartFramingStorage.entries[1].state = 0;
	g_qmiUartFramingStorage.entries[1].prev = &g_qmiUartFramingStorage.entries[0];
	g_qmiUartFramingStorage.entries[1].next = &g_qmiUartFramingStorage.entries[2];
	g_qmiUartFramingStorage.entries[2].state = 0;
	g_qmiUartFramingStorage.entries[2].prev = &g_qmiUartFramingStorage.entries[1];
	g_qmiUartFramingStorage.entries[2].next = &g_qmiUartFramingStorage.entries[3];
	g_qmiUartFramingStorage.entries[3].state = 0;
	g_qmiUartFramingStorage.entries[3].prev = &g_qmiUartFramingStorage.entries[2];
	g_qmiUartFramingStorage.entries[3].next = &g_qmiUartFramingStorage.entries[4];
	g_qmiUartFramingStorage.entries[4].state = 0;
	g_qmiUartFramingStorage.entries[4].prev = &g_qmiUartFramingStorage.entries[3];
	g_qmiUartFramingStorage.entries[4].next = &g_qmiUartFramingStorage.entries[5];
	g_qmiUartFramingStorage.entries[5].state = 0;
	g_qmiUartFramingStorage.entries[5].prev = &g_qmiUartFramingStorage.entries[4];
	g_qmiUartFramingStorage.entries[5].next = &g_qmiUartFramingStorage.entries[6];
	g_qmiUartFramingStorage.entries[6].state = 0;
	g_qmiUartFramingStorage.entries[6].prev = &g_qmiUartFramingStorage.entries[5];
	g_qmiUartFramingStorage.entries[6].next = &g_qmiUartFramingStorage.entries[7];
	g_qmiUartFramingStorage.entries[7].state = 0;
	g_qmiUartFramingStorage.entries[7].prev = &g_qmiUartFramingStorage.entries[6];
	g_qmiUartFramingStorage.entries[7].next = &g_qmiUartFramingStorage.entries[8];
	g_qmiUartFramingStorage.entries[8].state = 0;
	g_qmiUartFramingStorage.entries[8].prev = &g_qmiUartFramingStorage.entries[7];
	g_qmiUartFramingStorage.entries[8].next = &g_qmiUartFramingStorage.entries[9];
	g_qmiUartFramingStorage.entries[9].state = 0;
	g_qmiUartFramingStorage.entries[9].prev = &g_qmiUartFramingStorage.entries[8];
	g_qmiUartFramingStorage.entries[9].next = &g_qmiUartFramingStorage.entries[10];
	g_qmiUartFramingStorage.entries[10].state = 0;
	g_qmiUartFramingStorage.entries[10].prev = &g_qmiUartFramingStorage.entries[9];
	g_qmiUartFramingStorage.entries[10].next = &g_qmiUartFramingStorage.entries[11];
	g_qmiUartFramingStorage.entries[11].state = 0;
	g_qmiUartFramingStorage.entries[11].prev = &g_qmiUartFramingStorage.entries[10];
	g_qmiUartFramingStorage.entries[11].next = &g_qmiUartFramingStorage.entries[12];
	g_qmiUartFramingStorage.entries[12].state = 0;
	g_qmiUartFramingStorage.entries[12].prev = &g_qmiUartFramingStorage.entries[11];
	g_qmiUartFramingStorage.entries[12].next = &g_qmiUartFramingStorage.entries[13];
	g_qmiUartFramingStorage.entries[13].state = 0;
	g_qmiUartFramingStorage.entries[13].prev = &g_qmiUartFramingStorage.entries[12];
	g_qmiUartFramingStorage.entries[13].next = &g_qmiUartFramingStorage.entries[14];
	g_qmiUartFramingStorage.entries[14].state = 0;
	g_qmiUartFramingStorage.entries[14].prev = &g_qmiUartFramingStorage.entries[13];
	g_qmiUartFramingStorage.entries[14].next = &g_qmiUartFramingStorage.entries[15];
	g_qmiUartFramingStorage.entries[15].state = 0;
	g_qmiUartFramingStorage.entries[15].prev = &g_qmiUartFramingStorage.entries[14];
	g_qmiUartFramingStorage.entries[15].next = NULL;
	g_qmiUartFramingStorage.head = &g_qmiUartFramingStorage.entries[0];
	g_qmiUartFramingStorage.tail = &g_qmiUartFramingStorage.entries[15];
	g_qmiUartFramingStorage.producer = &g_qmiUartFramingStorage.entries[0];
	g_qmiUartFramingStorage.freeCount = 16;
	g_qmiUartFramingStorage.receiveProducer = 0;
	g_qmiUartFramingStorage.receiveConsumer = 0;
	g_qmiUartFramingStorage.transmitPending = 0;
	g_qmiUartFramingStorage.waitingAcknowledgment = 0;
	g_qmiUartFramingStorage.multipartPending = 0;
	g_qmiUartFramingStorage.bufferedInput = 0;
	g_qmiUartFramingStorage.receiveCount = 0;
	g_qmiUartFramingStorage.previousReceiveSequence = 255;
	g_qmiUartFramingStorage.lastReceiveSequence = 255;
	g_qmiUartFramingStorage.unlock(g_qmiUartFramingStorage.context);
	return 0;
}

int qmiQueueUartFrame(const void *data, unsigned int length)
{
	QmiUartFramingControl *control;
	QmiUartFrameEntry *entry;
	QmiUartDataHeader *header;
	const unsigned char *input;
	const unsigned char *end;
	unsigned char *encoded;
	unsigned int fragments;
	unsigned int fragment = 0;
	unsigned int offset = 0;
	unsigned int remaining = length;
	unsigned int chunk = length;
	unsigned int nextRemaining;
	unsigned int encodedLength;
	unsigned int nextEncoded;
	unsigned int mask;
	unsigned int symbol;
	unsigned int shift;
	unsigned int i;
	unsigned short endMarker;
	unsigned char bitPosition;
	unsigned char carry;
	unsigned char nextCarry;
	unsigned char checksum;
	int emitsSecond;
	int ret;

	if (length == 0 || data == NULL)
		return (int)0x80330002;
	if (g_qmiUartFramingControl == NULL)
		return (int)0x80330003;
	fragments = length / 768;
	if (768 * fragments != length)
		fragments++;
	g_qmiUartFramingControl->lock(g_qmiUartFramingControl->context);
	control = g_qmiUartFramingControl;
	if (fragments > control->freeCount) {
		control->unlock(control->context);
		return (int)0x80330400;
	}
	if (fragments == 0)
		goto full;
	for (;;) {
		if (remaining <= 768) {
			nextRemaining = 0;
		} else {
			nextRemaining = remaining - 768;
			chunk = 768;
		}
		input = (const unsigned char *)data + offset;
		if (input == NULL || chunk - 1 >= 768) {
			ret = (int)0x80330002;
			goto out;
		}
		if (control->freeCount == 0)
			goto full;
		endMarker = control->transmitEndMarker;
		entry = control->producer;
		entry->length = sizeof(entry->buffer);
		header = &entry->buffer.data.header;
		header->startMarkerLow = control->transmitStartMarker;
		header->fragmentIndex = fragment;
		header->fragmentCount = fragments;
		header->startMarkerHigh = control->transmitStartMarker >> 8;
		header->sequence = control->transmitSequence;
		encoded = entry->buffer.data.encoded;
		encodedLength = 0;
		carry = 0;
		bitPosition = 7;
		end = input + chunk;
		do {
			bitPosition = (bitPosition - 6) & 7;
			nextEncoded = encodedLength + 1;
			mask = 255 >> (7 - bitPosition);
			emitsSecond = bitPosition > 4;
			symbol = *input >> (bitPosition + 1);
			shift = 5 - bitPosition;
			if (bitPosition > 4)
				bitPosition = 7;
			symbol = (unsigned char)(carry | symbol);
			carry = 0;
			encoded[encodedLength] = g_qmiUartEncodeAlphabet[symbol];
			encodedLength += 2;
			nextCarry = (*input++ & mask) << shift;
			if (!emitsSecond)
				encodedLength = nextEncoded;
			if (emitsSecond)
				encoded[nextEncoded] = g_qmiUartEncodeAlphabet[nextCarry];
			else
				carry = nextCarry;
		} while (input != end);
		if (bitPosition != 7)
			encoded[encodedLength++] = g_qmiUartEncodeAlphabet[carry];
		header->lengthLow = encodedLength + 6;
		header->lengthHigh = (unsigned short)(encodedLength + 6) >> 8;
		checksum = 0;
		for (i = 0; i < encodedLength + sizeof(*header); i++)
			checksum ^= entry->buffer.bytes[i];
		encoded[encodedLength] = checksum;
		encoded[encodedLength + 2] = endMarker;
		encoded[encodedLength + 1] = endMarker >> 8;
		entry->length = encodedLength + 10;
		entry->state = 1;
		entry->fragmentCount = header->fragmentCount;
		entry->kind = 0;
		entry->sequence = header->sequence;
		entry->fragmentIndex = header->fragmentIndex;
		offset += chunk;
		control->producer = entry->next;
		control->transmitSequence = (control->transmitSequence + 1) & 127;
		control->freeCount--;
		if (++fragment == fragments)
			break;
		chunk = nextRemaining;
		remaining = nextRemaining;
	}
	ret = 0;
	goto out;
full:
	ret = (int)0x80330400;
out:
	control->unlock(control->context);
	return ret;
}

void qmiPumpUartFrames(void)
{
	QmiUartFramingControl *control;
	QmiUartFrameEntry *entry;
	QmiUartFrameEntry *next;
	QmiUartFrameEntry *prev;
	QmiUartFrameEntry *tail;
	QmiUartFrameEntry *selected = NULL;
	unsigned int length;
	unsigned int kind;
	unsigned int state;
	unsigned int attempts;
	unsigned int freeCount;
	int clearWaiting;

	if (g_qmiUartFramingControl == NULL || g_qmiUartFramingControl->enabled == 0)
		return;
	length = g_qmiUartFramingControl->isTransmitPending();
	if (length != 0)
		return;
	g_qmiUartFramingControl->transmitPending = 0;
	g_qmiUartFramingControl->lock(g_qmiUartFramingControl->context);
	control = g_qmiUartFramingControl;
	entry = control->head;
	if (entry == NULL || entry->state == 0) {
		g_qmiUartFramingControl->unlock(g_qmiUartFramingControl->context);
		return;
	}
	for (; entry != NULL && entry->state != 0; entry = next) {
		next = entry->next;
		kind = entry->kind;
		state = entry->state;
		clearWaiting = 0;
		if (kind == 0) {
			if (state == 2) {
				entry->state = 3;
				entry->acknowledgmentTicks = 50;
				control->waitingAcknowledgment = 1;
				continue;
			}
			if (state == 3) {
				entry->acknowledgmentTicks--;
				if (entry->acknowledgmentTicks > 0)
					continue;
				attempts = entry->attempts;
				if (attempts == 4) {
					clearWaiting = 1;
					goto release;
				}
			} else if (state == 1 && control->transmitPending == 0 &&
				   control->waitingAcknowledgment == 0) {
				attempts = entry->attempts;
				if (attempts == 4)
					goto release;
			} else {
				continue;
			}
			selected = entry;
			entry->state = 2;
			entry->attempts = attempts + 1;
			length = entry->length;
			control->transmitPending = 1;
			if (entry->fragmentCount > 1)
				control->multipartPending = 1;
		} else if (kind - 1 <= 1) {
			if (state == 2)
				goto release;
			if (state == 1 && control->transmitPending == 0) {
				selected = entry;
				length = entry->length;
				entry->state = 2;
				entry->attempts++;
				control->transmitPending = 1;
			}
		}
		continue;
release:
		freeCount = control->freeCount;
		if (freeCount != 16) {
			tail = control->tail;
			entry->state = 0;
			entry->kind = 0;
			entry->length = 0;
			entry->sequence = 0;
			entry->fragmentIndex = 0;
			entry->fragmentCount = 0;
			entry->attempts = 0;
			if (entry != tail) {
				prev = entry->prev;
				if (prev != NULL) {
					prev->next = entry->next;
					next = entry->next;
				}
				if (next != NULL)
					next->prev = prev;
				if (entry == control->head) {
					control->head = next;
					next->prev = NULL;
				}
				tail->next = entry;
				entry->prev = tail;
				entry->next = NULL;
				control->tail = entry;
			}
			if (control->producer == NULL)
				control->producer = entry;
			control->freeCount = freeCount + 1;
		}
		if (clearWaiting)
			control->waitingAcknowledgment = 0;
	}
	control->unlock(control->context);
	if (selected != NULL)
		g_qmiUartFramingControl->send(selected, length);
}

int qmiDecodeUartFrame(const unsigned char *data, unsigned int length,
	void **packet, unsigned int *packetLength)
{
	QmiUartFramingControl *control;
	const unsigned char *end;
	unsigned char *decoded;
	unsigned int count;
	unsigned int consumer;
	unsigned int producer;
	unsigned int startMarker;
	unsigned int endMarker;
	unsigned int marker;
	unsigned int first;
	unsigned int last;
	unsigned int wireLength;
	unsigned int totalLength;
	unsigned int sequence = 0;
	unsigned int checksum;
	unsigned int encodedLength;
	unsigned int groups;
	unsigned int group;
	unsigned int encodedOffset;
	unsigned int decodedLength;
	unsigned int remainder;
	unsigned int value;
	unsigned int i;
	int advance;
	int ret;
	int continueBuffered = 0;

	if (length == 0 || data == NULL || packetLength == NULL || packet == NULL)
		return (int)0x80330002;
	control = g_qmiUartFramingControl;
	if (control == NULL)
		return (int)0x80330003;
	*packet = NULL;
	*packetLength = 0;
	if (control->bufferedInput != 0) {
		count = control->receiveCount;
		control->bufferedInput = 0;
	} else {
		count = control->receiveCount + length;
		if (count > 2048)
			return (int)0x80330405;
		producer = control->receiveProducer;
		end = data + length;
		for (;;) {
			control->receiveProducer = producer + 1;
			control->receiveData[producer] = *data++;
			control->receiveProducer = (producer + 1) & 2047;
			if (data == end)
				break;
			producer = (producer + 1) & 2047;
		}
		control->receiveCount = count;
	}
	if (count <= 1)
		return 2;
	consumer = control->receiveConsumer;
	startMarker = control->receiveStartMarker;
	first = control->receiveData[consumer & 2047];
	marker = control->receiveData[(consumer + 1) & 2047] | (first << 8);
	if (marker != startMarker) {
		if (marker == 0x06f9) {
			ret = qmiProcessUartAcknowledgment(control, 1);
			goto result;
		}
		if (marker == 0x15ea) {
			ret = qmiProcessUartAcknowledgment(control, 2);
			goto result;
		}
		if (first == startMarker) {
			advance = -1;
			goto resynchronized;
		}
		endMarker = control->receiveEndMarker;
		if (first == endMarker) {
			advance = 1;
			goto resynchronized;
		}
		advance = 0;
		marker = control->receiveData[consumer & 2047];
		last = marker;
		for (;;) {
			advance++;
			marker = (unsigned short)(marker << 8);
			if ((unsigned int)advance == count)
				break;
			if (advance == 2048)
				goto panic;
			last = control->receiveData[(consumer + advance) & 2047];
			marker |= last;
			if (marker == startMarker || marker == 0x06f9 || marker == 0x15ea) {
				advance--;
				goto resynchronized;
			}
			if (marker == endMarker) {
				advance++;
				goto resynchronized;
			}
		}
		if (last == 6 || last == 21 || last == startMarker >> 8 ||
		    last == endMarker >> 8)
			advance--;
		control->receiveConsumer = (consumer + advance) & 2047;
		control->receiveCount = count - advance;
		ret = 0;
		goto buffered;
resynchronized:
		control->receiveConsumer = (consumer + advance) & 2047;
		control->receiveCount = count - advance;
		control->lock(control->context);
		sequence = 255;
		goto negativeAcknowledgment;
	}
	if (count <= 3)
		return 2;
	wireLength = control->receiveData[(consumer + 3) & 2047] |
		(control->receiveData[(consumer + 2) & 2047] << 8);
	if (wireLength > 1036) {
		control->receiveConsumer = (consumer + 2) & 2047;
		control->receiveCount = count - 2;
		ret = (int)0x80330406;
		goto result;
	}
	totalLength = wireLength + 4;
	if (totalLength > count)
		return 2;
	sequence = control->receiveData[(consumer + 4) & 2047];
	endMarker = control->receiveData[(consumer + wireLength + 3) & 2047] |
		(control->receiveData[(consumer + wireLength + 2) & 2047] << 8);
	if (control->receiveEndMarker != endMarker) {
		control->receiveCount = count - 2;
		control->receiveConsumer = (consumer + 2) & 2047;
		ret = (int)0x80330407;
		goto result;
	}
	checksum = 0;
	i = 0;
	do {
		checksum ^= control->receiveData[(consumer + i++) & 2047];
	} while (wireLength + 1 > i);
	if (control->receiveData[(wireLength + 1 + consumer) & 2047] != checksum) {
		control->receiveConsumer = (totalLength + consumer) & 2047;
		control->receiveCount = count - totalLength;
		control->lock(control->context);
negativeAcknowledgment:
		ret = _addAckNakTaskList(g_qmiUartFramingControl, 2, sequence);
		g_qmiUartFramingControl->unlock(g_qmiUartFramingControl->context);
		goto result;
	}
	encodedLength = wireLength - 6;
	if (encodedLength == 0) {
		ret = (int)0x80330002;
		goto consumed;
	}
	if (3 * encodedLength > 3075) {
		ret = (int)0x80330405;
		goto consumed;
	}
	decoded = control->decodedData;
	groups = encodedLength >> 2;
	for (group = 0; group != groups; group++) {
		encodedOffset = 4 * group;
		value = g_qmiUartDecodeAlphabet[
			(int)control->receiveData[(control->receiveConsumer + 7 + encodedOffset) & 2047] - 32] << 18;
		if (group == 510)
			goto panic;
		value |= g_qmiUartDecodeAlphabet[
			(int)control->receiveData[(control->receiveConsumer + 8 + encodedOffset) & 2047] - 32] << 12;
		value |= g_qmiUartDecodeAlphabet[
			(int)control->receiveData[(control->receiveConsumer + 10 + encodedOffset) & 2047] - 32];
		value |= g_qmiUartDecodeAlphabet[
			(int)control->receiveData[(control->receiveConsumer + 9 + encodedOffset) & 2047] - 32] << 6;
		decoded[3 * group + 2] = value;
		decoded[3 * group] = value >> 16;
		decoded[3 * group + 1] = value >> 8;
	}
	encodedOffset = 4 * group;
	decodedLength = 3 * group;
	remainder = encodedLength - encodedOffset;
	if (remainder == 1) {
		decoded[decodedLength++] = 4 * g_qmiUartDecodeAlphabet[
			(int)control->receiveData[(control->receiveConsumer + 7 + encodedOffset) & 2047] - 32];
	} else if (remainder == 2 || remainder == 3) {
		if (encodedOffset == 2040)
			qmiPanicUartReadPosition();
		value = g_qmiUartDecodeAlphabet[
			(int)control->receiveData[(control->receiveConsumer + 7 + encodedOffset) & 2047] - 32] << 18;
		value |= g_qmiUartDecodeAlphabet[
			(int)control->receiveData[(control->receiveConsumer + 8 + encodedOffset) & 2047] - 32] << 12;
		if (remainder == 3)
			value |= g_qmiUartDecodeAlphabet[
				(int)control->receiveData[(control->receiveConsumer + 9 + encodedOffset) & 2047] - 32] << 6;
		decoded[decodedLength++] = value >> 16;
		if (remainder == 3)
			decoded[decodedLength++] = value >> 8;
	}
	control->decodedLength = decodedLength;
	control->receiveConsumer = (control->receiveConsumer + totalLength) & 2047;
	control->receiveCount -= totalLength;
	*packet = decoded;
	*packetLength = decodedLength;
	control->lock(control->context);
	ret = _addAckNakTaskList(g_qmiUartFramingControl, 1, sequence);
	g_qmiUartFramingControl->unlock(g_qmiUartFramingControl->context);
	control = g_qmiUartFramingControl;
	if (sequence == control->previousReceiveSequence || sequence == control->lastReceiveSequence) {
		ret = (int)0x80330401;
		*packet = NULL;
		*packetLength = 0;
	} else {
		continueBuffered = ret == 0 || (unsigned int)(ret - (int)0x80330406) <= 1;
	}
	control->previousReceiveSequence = control->lastReceiveSequence;
	control->lastReceiveSequence = sequence;
	goto completed;
consumed:
	control->receiveConsumer = (totalLength + consumer) & 2047;
	control->receiveCount = count - totalLength;
result:
	continueBuffered = ret == 0 || (unsigned int)(ret - (int)0x80330406) <= 1;
completed:
	if (!continueBuffered)
		return ret;
	control = g_qmiUartFramingControl;
buffered:
	if (control->receiveCount != 0) {
		control->bufferedInput = 1;
		return 1;
	}
	return ret;
panic:
	ksceKernelPanic(&g_qmiUartReadPositionDebug, __builtin_return_address(0));
}

int qmiIsUartFrameQueueEmpty(void)
{
	if (g_qmiUartFramingControl == NULL)
		return 0;
	return g_qmiUartFramingControl->freeCount == 16;
}



