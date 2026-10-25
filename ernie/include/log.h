/*
 * The syscon's log (app/log.c): counters of events kept in the NVS's syscon area, one record
 * each, behind a header. An event is queued by logEvent and counted by logUpdate in task 2;
 * the host reads them with sceSysconGetLogInfo (0x0086), sceSysconLogStart (0x0087) and
 * sceSysconLogReadData (0x0088).
 */
#ifndef ERNIE_LOG_H
#define ERNIE_LOG_H

#define LOG_VERSION		3	/* the records' layout */
#define LOG_HEADER		0	/* the header's record */
#define LOG_EVENTS		3	/* the events queued */

/* the header's state */
#define LOG_STATE_UNLOADED	(-2)	/* not read from the NVS yet (logInit) */
#define LOG_STATE_ERASED	(-1)
#define LOG_STATE_IDLE		0
#define LOG_STATE_RUNNING	1

/* the header's mode */
#define LOG_MODE_STOPPED	0x00
#define LOG_MODE_STARTED	0xaa
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
};

extern void logQueueInit(void);
extern void logInit(void);
extern void logUpdate(void);
extern void logEvent(unsigned int event);
extern int logGetInfo(struct LogInfo *info);
extern int logStart(void);
extern int logStop(void);
extern int logReadData(unsigned int id, void *data, unsigned char *size);
extern int logReadOffset(unsigned int offset, unsigned char size, void *data);
extern int logClear(unsigned int id);
extern int logClearIfStopped(void);
extern int logSetVersion(unsigned int version);
extern int logReset(void);
extern int logWriteHeader(struct LogHeader *header);
extern int logRead(unsigned int id, void *data, unsigned char *size);
extern int logWrite(unsigned int id, void *data);
extern int logNvsRead(unsigned int offset, unsigned char size, void *data);
extern int logNvsWrite(unsigned int offset, unsigned char size, void *data);
extern int logClearAll(void);
extern unsigned int logRecordCount(void);

#endif
