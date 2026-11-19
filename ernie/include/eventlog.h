/*
 * USS-1001's (BOARD_USS1001) event log: the last EVENT_LOG_ENTRIES events in the RAM a warm start
 * keeps (app/eventlog.c), each with the clock's low word, two codes and up to four bytes; the
 * host starts, stops, clears and reads it.
 */
#ifndef ERNIE_EVENTLOG_H
#define ERNIE_EVENTLOG_H

#include "ernie.h"

#define EVENT_LOG_ENTRIES	512
#define EVENT_LOG_DATA		4

/* eventLogMode */
#define EVENT_LOG_STOPPED	0x55
#define EVENT_LOG_STARTED	0xaa

/* what the functions return */
#define EVENT_LOG_OK		0
#define EVENT_LOG_NO_BUFFER	1
#define EVENT_LOG_NO_ENTRY	2
#define EVENT_LOG_BUSY		3

struct EventLogEntry {
	unsigned int	clock;
	unsigned char	kind;
	unsigned char	code;
	unsigned char	data[EVENT_LOG_DATA];
};

/* the data of the events the modules record, four bytes each (in eventlog.c's data) */
/* the data of a charge event: four bytes, or the slim's two words */
union EventLogData {
	unsigned char	byte[4];
	unsigned int	word[2];
};

extern union EventLogData eventLogCharge;
extern union EventLogData eventLogTask1;
extern unsigned char eventLogInput[4];
extern unsigned char eventLogCommand[4];
extern unsigned char eventLogData5[4];

extern void eventLogInit(void);
extern int eventLogRecord(unsigned char kind, unsigned char code, unsigned char *data, unsigned char size);
extern int eventLogClear(void);
extern int eventLogStart(void);
extern int eventLogStop(void);
extern int eventLogGetInfo(unsigned char *version, unsigned char *mode, unsigned int *count);
extern int eventLogRead(unsigned int index, struct EventLogEntry *entry);

#endif
