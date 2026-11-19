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

#if BOARD_DOL1001
static int logQueuePush(unsigned int event, long value);
static int logQueuePop(unsigned int *event, long *value);
#else
static int logQueuePush(unsigned int event);
static int logQueuePop(unsigned int *event);
#endif

#if BOARD_DOL1001
/* a read that does not ask for the record's kind */
#define LOG_READ(id, data, size)	logRead((id), (data), (size), NULL)
#if BOARD_DOL1002
static void logMigrate(unsigned int version);
/* the header written, and taken in when written */
#define LOG_WRITE_HEADER(result, header)				\
	do {								\
		(result) = logNvsWrite(LOG_HEADER, sizeof(struct LogHeader), (header)); \
		if ((result) == 0)					\
			memmove(&logHeader, (header), sizeof(logHeader)); \
	} while (0)
#endif
#else
#define LOG_READ(id, data, size)	logRead((id), (data), (size))
#endif

/* a record's counter, or the header */
union LogValue {
	struct LogHeader header;
	unsigned long	value;
	unsigned int	word;
};

static TABLE_CONST struct LogRecord logRecords[] = {
#if BOARD_DOL1001
	{ 0x00, 0x00, 4, LOG_KIND_SET },	/* the header */
	{ 0x01, 0x1c, 4, LOG_KIND_COUNT },
	{ 0x02, 0x20, 4, LOG_KIND_COUNT },
	{ 0x04, 0x09, 1, LOG_KIND_COUNT },
	{ 0x05, 0x0e, 2, LOG_KIND_COUNT },
	{ 0x07, 0x0c, 1, LOG_KIND_COUNT },
	{ 0x08, 0x0d, 1, LOG_KIND_COUNT },
	{ 0x09, 0x24, 2, LOG_KIND_COUNT },
	{ 0x0a, 0x26, 2, LOG_KIND_COUNT },
	{ 0x0b, 0x04, 2, LOG_KIND_COUNT },
	{ 0x0c, 0x11, 1, LOG_KIND_COUNT },
	{ 0x0f, 0x14, 1, LOG_KIND_COUNT },
	{ 0x10, 0x15, 1, LOG_KIND_COUNT },
	{ 0x11, 0x16, 1, LOG_KIND_COUNT },
	{ 0x12, 0x17, 1, LOG_KIND_COUNT },
	{ 0x13, 0x18, 1, LOG_KIND_COUNT },
	{ 0x14, 0x19, 1, LOG_KIND_COUNT },
	{ 0x15, 0x1a, 1, LOG_KIND_COUNT },
	{ 0x16, 0x1b, 1, LOG_KIND_COUNT },
	{ 0x17, 0x06, 2, LOG_KIND_COUNT },
	{ 0x18, 0x10, 1, LOG_KIND_COUNT },
	{ 0x19, 0x28, 1, LOG_KIND_COUNT },
	{ 0x1b, 0x12, 2, LOG_KIND_COUNT },
	{ 0x1c, 0x0a, 1, LOG_KIND_SET },	/* the temperature's peak (app/thermal.c) */
	{ 0x1d, 0x2a, 1, LOG_KIND_COUNT },
#if BOARD_DOL1002
	{ 0x1e, 0x40, 4, LOG_KIND_OR },
	{ 0x1f, 0x44, 2, LOG_KIND_OR },
	{ 0x20, 0x46, 2, LOG_KIND_SET },
	{ 0x21, 0x48, 1, LOG_KIND_COUNT },
	{ 0x22, 0x49, 1, LOG_KIND_SET },
	{ 0x27, 0x2b, 1, LOG_KIND_COUNT },
	{ 0x28, 0x2c, 1, LOG_KIND_SET },
	{ 0x29, 0x4e, 1, LOG_KIND_COUNT },
	{ 0x2a, 0x50, 2, LOG_KIND_SET },
#endif
#elif FW_SINCE(0x00090903)
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
#if SOURCE_0103
	{ 0x19, 0x28, 1 },
	{ 0x1a, 0x29, 1 },
	{ 0x1d, 0x2a, 1 },
#endif
#else
	{ 0x00, 0x00, 4 },	/* the header */
	{ 0x01, 0x04, 2 },
	{ 0x02, 0x06, 2 },
	{ 0x03, 0x08, 1 },
	{ 0x04, 0x09, 1 },
	{ 0x05, 0x0a, 1 },
	{ 0x06, 0x0b, 1 },
	{ 0x07, 0x0c, 1 },
	{ 0x08, 0x0d, 1 },
	{ 0x09, 0x0e, 1 },
	{ 0x0a, 0x0f, 1 },
	{ 0x0b, 0x10, 1 },
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
#endif
};

static struct LogHeader logHeader;
#if BOARD_DOL1001
/* DOL-1001's event: a record and the value it is given */
struct LogEvent {
	unsigned int	id;
	long		value;
};
#endif

static struct {
#if BOARD_DOL1001
	struct LogEvent	events[LOG_EVENTS];
#else
	unsigned int	events[LOG_EVENTS];
#endif
	unsigned char	head;		/* the first event's */
	unsigned char	count;
} logQueue;

void logQueueInit(void)
{
	unsigned char i;

	for (i = 0; i < LOG_EVENTS; i++)
#if BOARD_DOL1001
	{
		logQueue.events[i].id = 0xffff;
		logQueue.events[i].value = 0;
	}
#else
		logQueue.events[i] = 0xffff;
#endif
	logQueue.head = 0;
	logQueue.count = 0;
}

void logInit(void)
{
	logHeader.state = LOG_STATE_UNLOADED;
	logHeader.mode = LOG_MODE_NONE;
	logHeader.version = 0;
}

#if SOURCE_0103
/* the header taken in (at the first call, first, which then starts the log), and the records
   of an older layout moved to this one's */
static void logLoad(unsigned char first)
{
	union LogValue record;
#if !BOARD_DOL1002
	union LogValue old;
#endif
	int result;
#if !BOARD_DOL1002
	unsigned int id;
#endif
	unsigned char size;

	if (first == 1) {
		logHeader.state = LOG_STATE_UNLOADED;
		logHeader.mode = LOG_MODE_NONE;
		logHeader.version = 0;
	}
	if (logHeader.state == LOG_STATE_UNLOADED) {
		result = LOG_READ(LOG_HEADER, &record, &size);
		if (result != 0)
			return;
		memcpy(&logHeader, &record, sizeof(logHeader));
		if (logHeader.mode == LOG_MODE_NONE) {
			result = (unsigned char)logSetMode(LOG_MODE_STOPPED, 0);
			if (result != 0)
				return;
			result = logReset();
			if (result != 0)
				return;
		}
	}
	if (logHeader.version < LOG_VERSION) {
		memcpy(&record, &logHeader, sizeof(logHeader));
		result = (unsigned char)logSetMode(LOG_MODE_STOPPED, 0);
		if (result != 0)
			return;
#if BOARD_DOL1002
		logMigrate(logHeader.version);
		record.header.version = LOG_VERSION;
		result = logNvsWrite(LOG_HEADER, sizeof(struct LogHeader), &record);
		if (result != 0)
			return;
		memmove(&logHeader, &record, sizeof(logHeader));
#else
		switch (logHeader.version) {
#if BOARD_DOL1001
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			logClear(0x19);
			logClear(0x1b);
			logClear(0x1c);
			break;
		case 8:
			logClear(0x1d);
			break;
#else
		case 0:
		case 1:
			old.value = 0;
			logNvsRead(0x04, 2, &old);
			logNvsWrite(0x1c, 4, &old);
			old.value = 0;
			logNvsRead(0x06, 2, &old);
			logNvsWrite(0x20, 4, &old);
			for (id = 3; id < 0x1b; id++)
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
			logClear(0x19);
			logClear(0x1a);
			break;
		case 3:
		case 4:
		case 5:
			logClear(0x19);
			logClear(0x1a);
			break;
		case 6:
		case 7:
		case 8:
			logClear(0x1d);
			break;
#endif
		}
		result = logWriteHeader(&record.header);
		if (result != 0)
			return;
		logSetVersion(LOG_VERSION);
#endif
	}
	if (first == 1)
		logSetMode(LOG_MODE_STARTED, 0);
}

/* task 2's, at each tick */
void logUpdate(void)
{
	union LogValue record;
#if BOARD_DOL1001
	long value;		/* the event's */
#endif
	int result;
	unsigned int id;
	unsigned char size;
#if BOARD_DOL1001
	unsigned char kind;
#endif
#if BOARD_DOL1002
	unsigned char n;
#endif

	if (logHeader.state == LOG_STATE_UNLOADED)
		logLoad(1);
	else
		logLoad(0);
	switch (logHeader.mode) {
	case LOG_MODE_STOPPED:
		switch (logHeader.state) {
		case LOG_STATE_IDLE:
		case LOG_STATE_RUNNING:
			break;
		case LOG_STATE_ERASED:
			logQueueInit();
			result = logReset();
			if (result != 0)
				return;
			break;
		default:
			result = (unsigned char)logSetMode(LOG_MODE_STOPPED, 0);
			if (result != 0)
				return;
			result = logReset();
			if (result != 0)
				return;
			break;
		}
		break;
	case LOG_MODE_STARTED:
		switch (logHeader.state) {
		case LOG_STATE_RUNNING:
#if BOARD_DOL1002
			/* the queue's events, all of them at each tick */
			for (n = 0; n < LOG_EVENTS; n++) {
				if (logQueuePop(&id, &value) != 0)
					break;
				result = logRead(id, &record, &size, &kind);
				if (result != 0) {
					result = 2;
					break;
				}
				if (kind == LOG_KIND_COUNT)
					record.value++;
				else if (kind == LOG_KIND_SET)
					record.value = value;
				else if (kind == LOG_KIND_OR)
					record.value |= value;
				else {
					result = 2;
					break;
				}
				if (size == 1)
					record.value &= 0xff;
				else if (size == 2)
					record.value &= 0xffff;
				if (record.value == 0 && kind == LOG_KIND_COUNT) {
					result = 4;	/* the counter is full */
					break;
				}
				result = logWrite(id, &record);
				if (result != 0) {
					result = 2;
					break;
				}
			}
#elif BOARD_DOL1001
			if (logQueuePop(&id, &value) == 0) {
				result = logRead(id, &record, &size, &kind);
				if (result != 0)
					result = 2;
				else {
					if (kind == LOG_KIND_COUNT)
						record.value++;
					else if (kind == LOG_KIND_SET)
						record.value = value;
					else if (kind == LOG_KIND_OR)
						record.value |= value;
					else {
						result = 2;
						goto done;
					}
					if (size == 1)
						record.value &= 0xff;
					else if (size == 2)
						record.value &= 0xffff;
					if (record.value == 0 && kind == LOG_KIND_COUNT)
						result = 4;	/* the counter is full */
					else {
						result = logWrite(id, &record);
						if (result != 0)
							result = 2;
					}
				}
			done:
				;
			}
#else
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
#endif
			break;
		case LOG_STATE_ERASED:
		case LOG_STATE_IDLE:
			result = (unsigned char)logSetMode(LOG_MODE_STOPPED, 0);
			if (result != 0)
				return;
			break;
		default:
			result = (unsigned char)logSetMode(LOG_MODE_STOPPED, 0);
			if (result != 0)
				return;
			break;
		}
		break;
	case LOG_MODE_HALTED:
		break;
	default:
		result = (unsigned char)logSetMode(LOG_MODE_STOPPED, 0);
		if (result != 0)
			return;
		result = logReset();
		if (result != 0)
			return;
		break;
	}
}
#else
/* task 2's, at each tick */
void logUpdate(void)
{
	union LogValue record;
#if FW_SINCE(0x00090903)
	union LogValue old;
#endif
	int result;
	unsigned int id;
	unsigned char size;

	if (logHeader.state == LOG_STATE_UNLOADED) {
		result = LOG_READ(LOG_HEADER, &record, &size);
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
#if FW_SINCE(0x00090903)
			old.value = 0;
			logNvsRead(0x04, 2, &old);
			logNvsWrite(0x1c, 4, &old);
			old.value = 0;
			logNvsRead(0x06, 2, &old);
			logNvsWrite(0x20, 4, &old);
			for (id = 3; id < 0x19; id++)
				logClear(id);
#else
			for (id = 3; id < 0x17; id++)
				logClear(id);
#endif
			break;
#if FW_SINCE(0x00090903)
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
#endif
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
#endif

#if BOARD_DOL1001
static int logQueuePush(unsigned int event, long value)
{
	int result;
	unsigned char i;

	dis_dsp();
#if BOARD_DOL1002
	if (logHeader.mode == LOG_MODE_STOPPED)
		result = 5;
	else if (logQueue.count < LOG_EVENTS) {
#else
	if (logHeader.mode != LOG_MODE_STARTED && logHeader.mode != LOG_MODE_NONE)
		result = 5;
	else if (logQueue.head + logQueue.count < LOG_EVENTS) {
#endif
		i = (unsigned char)(logQueue.head + logQueue.count) % LOG_EVENTS;
		logQueue.events[i].id = event;
		logQueue.events[i].value = value;
		logQueue.count++;
		result = 0;
	} else
		result = 5;
	ena_dsp();
	return result;
}

static int logQueuePop(unsigned int *event, long *value)
{
	int result;

	dis_dsp();
	if (logQueue.count != 0) {
		*event = logQueue.events[logQueue.head].id;
		*value = logQueue.events[logQueue.head].value;
		logQueue.head = (unsigned char)(logQueue.head + 1) % LOG_EVENTS;
		logQueue.count--;
		result = 0;
	} else
		result = 6;
	ena_dsp();
	return result;
}

void logEventValue(unsigned int event, long value)
{
	logQueuePush(event, value);
}
#else
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
#endif

/* 0x0086 (sceSysconGetLogInfo) */
int logGetInfo(struct LogInfo *info)
{
	int result;
	struct LogHeader header;
	unsigned char size;

	result = LOG_READ(LOG_HEADER, &header, &size);
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

#if SOURCE_0103
int logStart(void)
{
	int result;
	struct LogHeader header;

	if (logHeader.state == LOG_STATE_IDLE || logHeader.state == LOG_STATE_RUNNING) {
#if BOARD_DOL1002
		if (logHeader.mode == LOG_MODE_STOPPED)
			logQueueInit();
		header.state = LOG_STATE_RUNNING;
		header.mode = LOG_MODE_STARTED;
		header.version = logHeader.version;
		LOG_WRITE_HEADER(result, &header);
#else
		logQueueInit();
		header.state = LOG_STATE_RUNNING;
		header.mode = LOG_MODE_STARTED;
		header.version = logHeader.version;
		result = logWriteHeader(&header);
#endif
	} else
		result = 2;
	return result;
}

int logHalt(void)
{
	int result;
	struct LogHeader header;

	if (logHeader.state == LOG_STATE_IDLE || logHeader.state == LOG_STATE_RUNNING) {
#if BOARD_DOL1002
		if (logHeader.mode == LOG_MODE_STOPPED)
			logQueueInit();
		header.state = logHeader.state;
		header.mode = LOG_MODE_HALTED;
		header.version = logHeader.version;
		LOG_WRITE_HEADER(result, &header);
#else
		logQueueInit();
		header.state = logHeader.state;
		header.mode = LOG_MODE_HALTED;
		header.version = logHeader.version;
		result = logWriteHeader(&header);
#endif
	} else
		result = 2;
	return result;
}

int logStop(void)
{
	int result;
	struct LogHeader header;

	header.state = logHeader.state;
	header.mode = LOG_MODE_STOPPED;
	header.version = logHeader.version;
#if BOARD_DOL1002
	LOG_WRITE_HEADER(result, &header);
#else
	result = logWriteHeader(&header);
#endif
	return result;
}

/* 0x0087 (sceSysconLogStart) and 0x0089 */
int logSetMode(unsigned char mode, unsigned char source)
{
	int result;
	struct LogHeader header;
	unsigned char size;

	result = 0;
	result = LOG_READ(LOG_HEADER, &header, &size);
	if (result != 0)
		result = 2;
	else {
		switch (mode) {
		case LOG_MODE_STOPPED:
			result = logStop();
			break;
		case LOG_MODE_STARTED:
			switch (logHeader.mode) {
			case LOG_MODE_STARTED:
				result = 3;
				break;
			case LOG_MODE_STOPPED:
				if (source == 1 || source == 2)
					result = logStart();
				else
					result = 8;
				break;
			case LOG_MODE_HALTED:
				result = logStart();
				break;
			default:
				result = 8;
				break;
			}
			break;
		case LOG_MODE_HALTED:
			switch (logHeader.mode) {
			case LOG_MODE_STARTED:
				result = 3;
				break;
			case LOG_MODE_STOPPED:
				result = logHalt();
				break;
			case LOG_MODE_HALTED:
				result = 0;
				break;
			default:
				result = 8;
				break;
			}
			break;
		default:
			result = 8;
			break;
		}
	}
	return result;
}
#else
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
		result = LOG_READ(LOG_HEADER, &header, &size);
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

	result = LOG_READ(LOG_HEADER, &header, &size);
	if (result != 0)
		result = 2;
	else {
		header.mode = LOG_MODE_STOPPED;
		result = logWriteHeader(&header);
	}
	return result;
}
#endif

/* 0x0088 (sceSysconLogReadData) */
int logReadData(unsigned int id, void *data, unsigned char *size)
{
	return LOG_READ(id, data, size);
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

#if BOARD_DOL1002
int logReset(void)
{
	int result;
	struct LogHeader header;

	result = logClearAll();
	if (result != 0)
		return result;
	header.state = LOG_STATE_IDLE;
	header.mode = LOG_MODE_STOPPED;
	header.version = LOG_VERSION;
	LOG_WRITE_HEADER(result, &header);
	return result;
}
#else
int logSetVersion(unsigned int version)
{
	int result;
	struct LogHeader header;
	unsigned char size;

	result = LOG_READ(LOG_HEADER, &header, &size);
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
#endif

#if BOARD_DOL1001
int logRead(unsigned int id, void *data, unsigned char *size, unsigned char *kind)
#else
int logRead(unsigned int id, void *data, unsigned char *size)
#endif
{

#if SOURCE_0103
	unsigned int i;
#else
	int i;
#endif
	int result;

	for (i = 0; i < logRecordCount(); i++) {
		if (logRecords[i].id == id) {
			*size = logRecords[i].size;
#if BOARD_DOL1001
			if (kind != NULL)
				*kind = logRecords[i].kind;
#endif
			result = nvsRead(logRecords[i].offset, *size, data, NVS_AREA_SYSCON);
			if (result != 0) {
			}
			return result;
		}
	}
#if SOURCE_0103
	if (id < LOG_IDS) {
		/* a record this layout does not keep reads as 0 */
		*size = 1;
#if BOARD_DOL1001
		memset(data, 0, *size);
#else
		*(unsigned char *)data = 0;
#endif
		return 0;
	}
#endif
	return 1;
}

int logWrite(unsigned int id, void *data)
{

#if SOURCE_0103
	unsigned int i;
#else
	int i;
#endif
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
#if SOURCE_0103
	if (id < LOG_IDS)
		return 0;
#endif
	return 1;
}

int logNvsRead(unsigned int offset, unsigned char size, void *data)
{
	return nvsRead(offset, size, data, NVS_AREA_SYSCON);
}

#if FW_SINCE(0x00090903)
int logNvsWrite(unsigned int offset, unsigned char size, void *data)
{
	return nvsWrite(offset, size, data, NVS_AREA_SYSCON);
}
#endif

int logClearAll(void)
{

#if SOURCE_0103
	unsigned int i;
#else
	int i;
#endif
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

#if BOARD_DOL1002
/* the records a layout older than version did not keep cleared: from the first new one on */
#define LOG_CLEAR_FROM(first)						\
	do {								\
		zero = 0;						\
		for (id = (first); id < LOG_IDS; id++)			\
			logWrite(id, &zero);				\
	} while (0)

static void logMigrate(unsigned int version)
{
	unsigned long zero;
	unsigned int id;

	switch (version) {
	case 3:
	case 4:
	case 5:
		LOG_CLEAR_FROM(0x19);
		break;
	case 6:
	case 7:
		LOG_CLEAR_FROM(0x1b);
		break;
	case 8:
	case 9:
		LOG_CLEAR_FROM(0x1d);
		break;
	case 0x10:
	case 0x11:
		LOG_CLEAR_FROM(0x1e);
		break;
	case 0x12:
		LOG_CLEAR_FROM(0x21);
		break;
	case 0x13:
		LOG_CLEAR_FROM(0x29);
		break;
	}
}
#endif
