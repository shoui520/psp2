/*
 * USS-1001's event log (eventlog.h): a ring of EVENT_LOG_ENTRIES entries at the RAM's kept
 * area (0xfbc00, below keepRam: a warm start keeps it), written while the host has started
 * it, read back while it is stopped, newest first.
 */
#include <string.h>

#include "ernie.h"
#include "eventlog.h"
#include "kernel.h"
#include "rtc.h"

#pragma section @@DATAL @@ELOG
static struct EventLogEntry eventLog[EVENT_LOG_ENTRIES];
#pragma section @@DATAL @@DATAL

static unsigned char eventLogVersion;
static unsigned char eventLogMode;
/* the next entry's number, and how many the ring holds */
static unsigned int eventLogNext;
static unsigned int eventLogCount;

/* the data of the events the modules record, four bytes each */
union EventLogData eventLogCharge;
unsigned char eventLogTask1[4];
unsigned char eventLogInput[4];
unsigned char eventLogCommand[4];
unsigned char eventLogData5[4];

/* eventLogRecord's entry, its clock and the byte it copies */
static unsigned int eventLogSlot;
static unsigned int eventLogClock;
static unsigned char eventLogByte;

void eventLogInit(void)
{
	eventLogVersion = 1;
	eventLogCount = 0;
	eventLogNext = 0;
	eventLogMode = EVENT_LOG_STOPPED;
}

/* an event: its kind, its code and size bytes of data (up to EVENT_LOG_DATA; none without data) */
int eventLogRecord(unsigned char kind, unsigned char code, unsigned char *data, unsigned char size)
{
	if (eventLogMode != EVENT_LOG_STARTED)
		return EVENT_LOG_BUSY;
	if (data == NULL)
		size = 0;
	if (size >= EVENT_LOG_DATA + 1)
		size = EVENT_LOG_DATA;
	dis_dsp();
	eventLogClock = (unsigned int)getClock().value;
	eventLogSlot = eventLogNext & (EVENT_LOG_ENTRIES - 1);
	eventLog[eventLogSlot].kind = kind;
	eventLog[eventLogSlot].code = code;
	eventLog[eventLogSlot].clock = eventLogClock;
	for (eventLogByte = 0; eventLogByte < EVENT_LOG_DATA; eventLogByte++)
		eventLog[eventLogSlot].data[eventLogByte] = eventLogByte < size ? data[eventLogByte] : 0;
	eventLogNext++;
	if (eventLogCount < EVENT_LOG_ENTRIES)
		eventLogCount++;
	ena_dsp();
	return EVENT_LOG_OK;
}

int eventLogClear(void)
{
	if (eventLogMode == EVENT_LOG_STARTED)
		return EVENT_LOG_BUSY;
	eventLogNext = 0;
	eventLogCount = 0;
	memset(eventLog, 0, sizeof(eventLog));
	return EVENT_LOG_OK;
}

int eventLogStart(void)
{
	if (eventLogMode == 0)
		return EVENT_LOG_BUSY;
	eventLogMode = EVENT_LOG_STARTED;
	return EVENT_LOG_OK;
}

int eventLogStop(void)
{
	if (eventLogMode == 0)
		return EVENT_LOG_BUSY;
	eventLogMode = EVENT_LOG_STOPPED;
	return EVENT_LOG_OK;
}

/* the log's version, its mode and (while stopped) its count, each where a pointer is given */
int eventLogGetInfo(unsigned char *version, unsigned char *mode, unsigned int *count)
{
	unsigned char result;

	result = EVENT_LOG_NO_BUFFER;
	if (version != NULL) {
		*version = eventLogVersion;
		result = EVENT_LOG_OK;
	}
	if (mode != NULL) {
		*mode = eventLogMode;
		result = EVENT_LOG_OK;
	}
	if (count != NULL) {
		*count = eventLogMode == EVENT_LOG_STARTED ? 0 : eventLogCount;
		result = EVENT_LOG_OK;
	}
	return result;
}

/* the index'th newest entry, while the log is stopped */
int eventLogRead(unsigned int index, struct EventLogEntry *entry)
{
	if (eventLogMode == EVENT_LOG_STARTED)
		return EVENT_LOG_BUSY;
	if (entry == NULL)
		return EVENT_LOG_NO_BUFFER;
	if (index >= eventLogCount)
		return EVENT_LOG_NO_ENTRY;
	memcpy(entry, &eventLog[(eventLogNext - index - 1) & (EVENT_LOG_ENTRIES - 1)], sizeof(struct EventLogEntry));
	return EVENT_LOG_OK;
}
