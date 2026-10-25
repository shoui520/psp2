/*
 * The syscon's log: a counter for each of LOG_EVENTS' kinds of events, kept in the NVS's
 * syscon area (logRecords), behind a header record. logEvent queues an event, and
 * logUpdate, called by task 2 at each tick, counts it while the host has started the log
 * (LOG_MODE_STARTED); it also takes the header in at the first call and moves the records
 * of an older layout to this one's.
 */
#include <string.h>

#include "kernel.h"
#include "log.h"
#include "nvs.h"
#include "nvsreq.h"

static int logQueuePush(unsigned int event);
static int logQueuePop(unsigned int *event);

/* a record's counter, or the header */
union LogValue {
	struct LogHeader header;
	unsigned long	value;
	unsigned int	word;
};

static const struct LogRecord logRecords[] = {
	{ 0x00, 0x00, 4 },	/* the header */
	{ 0x01, 0x1c, 4 },
	{ 0x02, 0x20, 4 },
	{ 0x03, 0x08, 1 },
	{ 0x04, 0x09, 1 },
	{ 0x05, 0x0e, 2 },
	{ 0x06, 0x0b, 1 },
	{ 0x07, 0x0c, 1 },
	{ 0x08, 0x0d, 1 },
	{ 0x09, 0x24, 2 },
	{ 0x0a, 0x26, 2 },
	{ 0x0b, 0x04, 2 },
	{ 0x0c, 0x11, 1 },
	{ 0x0d, 0x12, 1 },
	{ 0x0e, 0x13, 1 },
	{ 0x0f, 0x14, 1 },
	{ 0x10, 0x15, 1 },
	{ 0x11, 0x16, 1 },
	{ 0x12, 0x17, 1 },
	{ 0x13, 0x18, 1 },
	{ 0x14, 0x19, 1 },
	{ 0x15, 0x1a, 1 },
	{ 0x16, 0x1b, 1 },
	{ 0x17, 0x06, 2 },
	{ 0x18, 0x10, 1 },
};

static struct LogHeader logHeader;
static struct {
	unsigned int	events[LOG_EVENTS];
	unsigned char	head;		/* the first event's */
	unsigned char	count;
} logQueue;

void logQueueInit(void)
{
	unsigned char i;

	for (i = 0; i < LOG_EVENTS; i++)
		logQueue.events[i] = 0xffff;
	logQueue.head = 0;
	logQueue.count = 0;
}

void logInit(void)
{
	logHeader.state = LOG_STATE_UNLOADED;
	logHeader.mode = LOG_MODE_NONE;
	logHeader.version = 0;
}

/* task 2's, at each tick */
void logUpdate(void)
{
	union LogValue record;
	union LogValue old;
	int result;
	unsigned int id;
	unsigned char size;

	if (logHeader.state == LOG_STATE_UNLOADED) {
		result = logRead(LOG_HEADER, &record, &size);
		if (result != 0)
			return;
		memcpy(&logHeader, &record, sizeof(logHeader));
	}
	if (logHeader.version < LOG_VERSION) {
		/* an older layout's records moved to this one's */
		memcpy(&record, &logHeader, sizeof(logHeader));
		result = logStop();
		if (result != 0)
			return;
		switch (logHeader.version) {
		case 0:
		case 1:
			old.value = 0;
			logNvsRead(0x04, 2, &old);
			logNvsWrite(0x1c, 4, &old);
			old.value = 0;
			logNvsRead(0x06, 2, &old);
			logNvsWrite(0x20, 4, &old);
			for (id = 3; id < 0x19; id++)
				logClear(id);
			break;
		case 2:
			old.value = 0;
			logNvsRead(0x04, 2, &old);
			logNvsWrite(0x1c, 4, &old);
			old.value = 0;
			logNvsRead(0x06, 2, &old);
			logNvsWrite(0x20, 4, &old);
			logClear(0x0b);
			logClear(0x17);
			logClear(0x18);
			logNvsRead(0x0e, 1, &old);
			logNvsWrite(0x24, 2, &old);
			logNvsRead(0x0f, 1, &old);
			logNvsWrite(0x26, 2, &old);
			old.word = 0;
			logNvsRead(0x0a, 1, &old);
			logNvsWrite(0x0e, 2, &old);
			break;
		}
		result = logWriteHeader(&record.header);
		if (result != 0)
			return;
		logSetVersion(LOG_VERSION);
	}
	switch (logHeader.mode) {
	case LOG_MODE_STOPPED:
		switch (logHeader.state) {
		case LOG_STATE_IDLE:
		case LOG_STATE_RUNNING:
			break;
		case LOG_STATE_ERASED:
			logQueueInit();
			result = logReset();
			break;
		default:
			result = logStop();
			result = logReset();
			break;
		}
		break;
	case LOG_MODE_STARTED:
		switch (logHeader.state) {
		case LOG_STATE_RUNNING:
			if (logQueuePop(&id) == 0) {
				result = logRead(id, &record, &size);
				if (result != 0)
					result = 2;
				else {
					record.value++;
					if (size == 1)
						record.value &= 0xff;
					else if (size == 2)
						record.value &= 0xffff;
					if (record.value == 0)
						result = 4;	/* the counter is full */
					else {
						result = logWrite(id, &record);
						if (result != 0)
							result = 2;
					}
				}
			}
			break;
		case LOG_STATE_ERASED:
		case LOG_STATE_IDLE:
			result = logStop();
			break;
		default:
			result = logStop();
			break;
		}
		break;
	default:
		result = logStop();
		result = logReset();
		break;
	}
}

static int logQueuePush(unsigned int event)
{
	int result;

	dis_dsp();
	if (logHeader.mode != LOG_MODE_STARTED && logHeader.mode != LOG_MODE_NONE)
		result = 5;
	else if (logQueue.head + logQueue.count < LOG_EVENTS) {
		logQueue.events[(unsigned char)(logQueue.head + logQueue.count) % LOG_EVENTS] = event;
		logQueue.count++;
		result = 0;
	} else
		result = 5;
	ena_dsp();
	return result;
}

static int logQueuePop(unsigned int *event)
{
	int result;

	dis_dsp();
	if (logQueue.count != 0) {
		*event = logQueue.events[logQueue.head];
		logQueue.head = (unsigned char)(logQueue.head + 1) % LOG_EVENTS;
		logQueue.count--;
		result = 0;
	} else
		result = 6;
	ena_dsp();
	return result;
}

void logEvent(unsigned int event)
{
	logQueuePush(event);
}

/* 0x0086 (sceSysconGetLogInfo) */
int logGetInfo(struct LogInfo *info)
{
	int result;
	struct LogHeader header;
	unsigned char size;

	result = logRead(LOG_HEADER, &header, &size);
	if (result != 0)
		result = 2;
	else {
		memcpy(&logHeader, &header, sizeof(logHeader));
		info->version = LOG_VERSION;
		info->state = logHeader.state;
		info->mode = logHeader.mode;
		info->reserved = 0;
	}
	return result;
}

/* 0x0087 (sceSysconLogStart) */
int logStart(void)
{
	int result;
	struct LogHeader header;
	unsigned char size;

	if (logHeader.mode == LOG_MODE_STARTED)
		result = 3;
	else if (logHeader.state == LOG_STATE_IDLE || logHeader.state == LOG_STATE_RUNNING) {
		logQueueInit();
		result = logRead(LOG_HEADER, &header, &size);
		if (result != 0)
			result = 2;
		else {
			header.state = LOG_STATE_RUNNING;
			header.mode = LOG_MODE_STARTED;
			result = logWriteHeader(&header);
		}
	} else
		result = 2;
	return result;
}

int logStop(void)
{
	int result;
	struct LogHeader header;
	unsigned char size;

	result = logRead(LOG_HEADER, &header, &size);
	if (result != 0)
		result = 2;
	else {
		header.mode = LOG_MODE_STOPPED;
		result = logWriteHeader(&header);
	}
	return result;
}

/* 0x0088 (sceSysconLogReadData) */
int logReadData(unsigned int id, void *data, unsigned char *size)
{
	return logRead(id, data, size);
}

int logReadOffset(unsigned int offset, unsigned char size, void *data)
{
	return logNvsRead(offset, size, data);
}

int logClear(unsigned int id)
{
	int result;
	unsigned long zero;

	result = 0;
	if (logHeader.mode == LOG_MODE_STARTED)
		result = 3;
	else {
		zero = 0;
		result = logWrite(id, &zero);
	}
	return result;
}

int logClearIfStopped(void)
{
	int result;

	result = 0;
	if (logHeader.mode == LOG_MODE_STARTED)
		result = 3;
	else
		result = logReset();
	return result;
}

int logSetVersion(unsigned int version)
{
	int result;
	struct LogHeader header;
	unsigned char size;

	result = logRead(LOG_HEADER, &header, &size);
	if (result != 0)
		result = 2;
	else {
		header.version = version;
		result = logWriteHeader(&header);
	}
	return result;
}

int logReset(void)
{
	int result;

	result = logClearAll();
	if (result != 0)
		return result;
	return logSetVersion(LOG_VERSION);
}

int logWriteHeader(struct LogHeader *header)
{
	int result;

	result = logWrite(LOG_HEADER, header);
	if (result != 0)
		result = 7;
	else
		memmove(&logHeader, header, sizeof(logHeader));
	return result;
}

int logRead(unsigned int id, void *data, unsigned char *size)
{
	int i;
	int result;

	for (i = 0; i < logRecordCount(); i++) {
		if (logRecords[i].id == id) {
			*size = logRecords[i].size;
			result = nvsRead(logRecords[i].offset, *size, data, NVS_AREA_SYSCON);
			if (result != 0) {
			}
			return result;
		}
	}
	return 1;
}

int logWrite(unsigned int id, void *data)
{
	int i;
	int result;

	for (i = 0; i < logRecordCount(); i++) {
		if (logRecords[i].id == id) {
			result = nvsWrite(logRecords[i].offset, logRecords[i].size, data,
					  NVS_AREA_SYSCON);
			if (result != 0) {
			}
			return result;
		}
	}
	return 1;
}

int logNvsRead(unsigned int offset, unsigned char size, void *data)
{
	return nvsRead(offset, size, data, NVS_AREA_SYSCON);
}

int logNvsWrite(unsigned int offset, unsigned char size, void *data)
{
	return nvsWrite(offset, size, data, NVS_AREA_SYSCON);
}

int logClearAll(void)
{
	int i;
	int result;
	unsigned long zero;

	zero = 0;
	for (i = 0; i < logRecordCount(); i++) {
		result = nvsWrite(logRecords[i].offset, logRecords[i].size,
				  (unsigned char *)&zero, NVS_AREA_SYSCON);
		if (result != 0)
			return result;
	}
	return result;
}

unsigned int logRecordCount(void)
{
	return sizeof(logRecords) / sizeof(logRecords[0]);
}
