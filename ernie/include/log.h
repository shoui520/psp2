/*
 * The syscon's log (app/log.c): counters of events kept in the NVS's syscon area, one record
 * each, behind a header. An event is queued by logEvent and counted by logUpdate in task 2;
 * the host reads them with sceSysconGetLogInfo (0x0086), sceSysconLogStart (0x0087) and
 * sceSysconLogReadData (0x0088).
 */
#ifndef ERNIE_LOG_H
#define ERNIE_LOG_H

#include "ernie.h"
#include "board.h"

#if SOURCE_0103
/* the slim's, a constant of the version block (0xf40, app/version.c) */
extern const unsigned int logVersion;
#define LOG_VERSION		logVersion
#elif FW_SINCE(0x00090903)
#define LOG_VERSION		3	/* the records' layout */
#else
#define LOG_VERSION		2
#endif
#define LOG_HEADER		0	/* the header's record */
#if BOARD_DOL1002
#define LOG_IDS			0x2b	/* DOL-1002's: the records' ids are below it */
#define LOG_EVENTS		5	/* the events queued */
#else
#if SOURCE_0103
#define LOG_IDS			0x1e	/* the slim's: the records' ids are below it */
#endif
#define LOG_EVENTS		3	/* the events queued */
#endif

/* the header's state */
#define LOG_STATE_UNLOADED	(-2)	/* not read from the NVS yet (logInit) */
#define LOG_STATE_ERASED	(-1)
#define LOG_STATE_IDLE		0
#define LOG_STATE_RUNNING	1

/* the header's mode */
#define LOG_MODE_STOPPED	0x00
#define LOG_MODE_STARTED	0xaa
#define LOG_MODE_HALTED		0x11	/* the slim's: stopped by command 0x0089 */
#define LOG_MODE_NONE		0xff	/* not read yet: events are queued */

struct LogHeader {
	signed char	state;		/* LOG_STATE_* */
	unsigned char	mode;		/* LOG_MODE_* */
	unsigned int	version;	/* LOG_VERSION */
};

/* sceSysconGetLogInfo's */
struct LogInfo {
	unsigned int	version;
	signed char	state;
	unsigned char	mode;
	unsigned long	reserved;
};

/* a record: where its counter is in the NVS's syscon area */
struct LogRecord {
	unsigned int	id;
	unsigned int	offset;
	unsigned char	size;
#if BOARD_DOL1001
	unsigned char	kind;		/* LOG_KIND_*: what an event does to it */
#endif
};

#if BOARD_DOL1001
/* DOL-1001's record kinds */
#define LOG_KIND_COUNT		1	/* a counter: an event adds one */
#define LOG_KIND_SET		2	/* an event's value replaces it */
#define LOG_KIND_OR		3	/* an event's value is or'ed in */
#endif

extern void logQueueInit(void);
extern void logInit(void);
extern void logUpdate(void);
#if BOARD_DOL1001
/* DOL-1001's events give their record a value: counted (0), set or or'ed by the record's kind */
extern void logEventValue(unsigned int event, long value);
#define logEvent(event)		logEventValue((event), 0)
#else
extern void logEvent(unsigned int event);
#endif
extern int logGetInfo(struct LogInfo *info);
extern int logStart(void);
extern int logStop(void);
#if SOURCE_0103
extern int logHalt(void);
/* the slim's: the log to mode (LOG_MODE_*), asked by source (0: logUpdate, 1: the host's
   commands, 2: serial1's 0x0151 and 0x0152) */
extern int logSetMode(unsigned char mode, unsigned char source);
#endif
extern int logReadData(unsigned int id, void *data, unsigned char *size);
extern int logReadOffset(unsigned int offset, unsigned char size, void *data);
extern int logClear(unsigned int id);
extern int logClearIfStopped(void);
#if !BOARD_DOL1002
extern int logSetVersion(unsigned int version);
#endif
extern int logReset(void);
#if !BOARD_DOL1002
extern int logWriteHeader(struct LogHeader *header);
#endif
#if BOARD_DOL1001
extern int logRead(unsigned int id, void *data, unsigned char *size, unsigned char *kind);
#else
extern int logRead(unsigned int id, void *data, unsigned char *size);
#endif
extern int logWrite(unsigned int id, void *data);
extern int logNvsRead(unsigned int offset, unsigned char size, void *data);
#if FW_SINCE(0x00090903)
extern int logNvsWrite(unsigned int offset, unsigned char size, void *data);
#endif
extern int logClearAll(void);
extern unsigned int logRecordCount(void);

#endif
