/*
 * SceDeci4pTsmp: the "TSMP" DECI4P protocol of the development host's target
 * manager. The host reads and writes registry settings, the setting UI file
 * and event log XML, the software information other modules report and the
 * activation; the target pushes registry changes as they happen.
 *
 * Received packets arrive through SceDeci4pDfMgr into the receive buffer and
 * are handled by the main thread; every reply is built in the one send
 * buffer. A second thread synchronises the settings once at boot.
 */

#pragma once

#include <psp2kern/types.h>
#include <psp2kern/kernel/threadmgr/fast_mutex.h>
#include <deci4p_dfmgr/deci4p.h>
#include <stddef.h>

#define TSMP_BUF_SIZE		0x11000
#define TSMP_DATA_MAX		0x10000		/* XML bytes per packet */
#define TSMP_PACKET_MAX		0x1001c		/* largest accepted request */
#define TSMP_SWINFO_ID_MAX	0x20
#define TSMP_SWINFO_LEN_MAX	0x80

/* main thread event flag */
#define TSMP_EVENT_RECV		0x001		/* a packet is in the receive buffer */
#define TSMP_EVENT_SENT		0x002		/* the send buffer is free again */
#define TSMP_EVENT_REG		0x010		/* registry notifications queued */
#define TSMP_EVENT_SETTING_XML	0x020		/* setting UI file to send */
#define TSMP_EVENT_EVENTLOG_XML	0x040		/* event log XML to send */
#define TSMP_EVENT_SWINFO	0x080		/* software information queued */
#define TSMP_EVENT_TICK		0x100		/* send the current time */
#define TSMP_EVENT_REG_BEGIN	0x200		/* registry synchronisation starts */
#define TSMP_EVENT_REG_END	0x400		/* ... and ends */
#define TSMP_EVENT_SYNC_TERM	0x800		/* the sync thread is done */
#define TSMP_EVENT_ALL		0xff1		/* everything but TSMP_EVENT_SENT */

/* sync thread event flag */
#define TSMP_SYNC_START		0x001		/* the host sent its setting sync request */
#define TSMP_SYNC_REG_DONE	0x010
#define TSMP_SYNC_SETTING_DONE	0x020
#define TSMP_SYNC_EVENTLOG_DONE	0x040
#define TSMP_SYNC_TICK_DONE	0x100
#define TSMP_SYNC_REG_END_DONE	0x200

/* system status */
#define TSMP_STATUS_SENDING	0x001		/* a reply is with SceDeci4pDfMgr */
#define TSMP_STATUS_SYNCING	0x010		/* the sync thread runs */
#define TSMP_STATUS_SETTING_XML	0x020		/* setting UI file chunks pending */
#define TSMP_STATUS_EVENTLOG_XML 0x040		/* event log chunks pending */
#define TSMP_STATUS_SYNC_REQ	0x100		/* setting sync requested by the host: */
#define TSMP_STATUS_SYNC_REG	0x200		/*   registry */
#define TSMP_STATUS_SYNC_SETTING 0x400		/*   setting UI file */
#define TSMP_STATUS_SYNC_EVENTLOG 0x800		/*   event log */

/* send / receive buffer status */
#define TSMP_BUF_BUSY		0x1
#define TSMP_BUF_FILLED		0x2
#define TSMP_BUF_HEADER		0x4

/* reply result codes */
#define TSMP_RESULT_OK			0
#define TSMP_RESULT_REG_NOT_FOUND	2
#define TSMP_RESULT_REG_ERROR		7
#define TSMP_RESULT_BAD_COMMAND		8
#define TSMP_RESULT_BAD_SIZE		9
#define TSMP_RESULT_FAILED		0x2008
#define TSMP_RESULT_ACT_EXPIRED		0x200a
#define TSMP_RESULT_ACT_INVALID		0x200c
#define TSMP_RESULT_ACT_UNSUPPORTED	0x200d
#define TSMP_RESULT_ACT_BAD_DATA	0x200e
#define TSMP_RESULT_REG_DENIED		0x2013

typedef struct TsmpHeader {
	SceDeci4pHeader hdr;
	SceUInt8 type;
	SceUInt8 code;
	SceUInt16 count;			/* entries that follow */
	SceUInt32 id[2];			/* echoed from the request */
} TsmpHeader;

/* a TSMP packet: the header, a result or entry count, then the body */
typedef struct TsmpPacket {
	TsmpHeader hdr;
	union {
		SceUInt32 result;		/* replies: TSMP_RESULT_* */
		SceUInt16 entries;		/* notifications: entries in data */
	};
	SceUInt32 data[];
} TsmpPacket;

/* a chunk of a file the target sends (setting UI file, event log) */
typedef struct TsmpFileChunk {
	TsmpHeader hdr;
	SceUInt32 result;
	SceSize offset;				/* of this chunk in the file */
	SceSize size;				/* of this chunk */
	SceSize total;				/* of the file */
	SceUInt8 data[];
} TsmpFileChunk;

_Static_assert(sizeof(TsmpHeader) == 0x1c, "TsmpHeader size");
_Static_assert(sizeof(TsmpPacket) == 0x20, "TsmpPacket size");
_Static_assert(sizeof(TsmpFileChunk) == 0x2c, "TsmpFileChunk size");

typedef struct TsmpBuffer {
	SceKernelFastMutex lock;
	SceUID block;
	SceUInt32 status;			/* TSMP_BUF_* */
	SceSize length;				/* bytes to send / received */
	void *base;
} TsmpBuffer;

/* a registry key/value or software information entry */
typedef struct TsmpNotification {
	SceSize size;				/* sizeof(TsmpNotification) */
	char name[0x100];
	SceUInt16 type;				/* TSMP_VALUE_* */
	SceUInt16 length;			/* value bytes, strings with the NUL */
	SceUInt32 value;			/* TSMP_VALUE_INT */
	char data[0x7c0];
} TsmpNotification;

#define TSMP_VALUE_INT		0
#define TSMP_VALUE_STR		1
#define TSMP_VALUE_BIN		2

typedef struct TsmpNotificationNode {
	TsmpNotification ntf;
	struct TsmpNotificationNode *next;
} TsmpNotificationNode;

typedef struct TsmpNotificationQueue {
	SceKernelFastMutex lock;
	TsmpNotificationNode *head;
	TsmpNotificationNode *tail;
	int count;
	int peak;
	SceUID heap;
} TsmpNotificationQueue;

/* SceRegMgrForDebugger transfer of a large file (setting UI file, event log) */
typedef struct TsmpRegFileRequest {
	SceSize size;				/* sizeof(TsmpRegFileRequest) */
	SceUInt32 flags;
	void *buf;
	SceSize bufSize;
	SceSize total;
	SceUInt32 reserved[3];
} TsmpRegFileRequest;

/* SceRegMgrForDebugger set / get of one key */
typedef struct TsmpRegRequest {
	SceSize size;				/* sizeof(TsmpRegRequest) */
	const char *name;
	SceUInt16 type;				/* 0 int, 1 string, 2 binary */
	SceUInt16 length;
	union {
		SceUInt32 value;
		const void *data;
	};
} TsmpRegRequest;

/* SceRegMgrForDebugger write of the setting UI file */
typedef struct TsmpRegWriteRequest {
	SceSize size;				/* sizeof(TsmpRegWriteRequest) */
	const void *data;
	SceSize length;
	SceUInt32 reserved;
} TsmpRegWriteRequest;

typedef struct TsmpState {
	SceKernelFastMutex statusLock;
	int handle;				/* sceDfMgrOpen */
	SceUID evf;				/* TSMP_EVENT_* */
	SceUID syncEvf;				/* TSMP_SYNC_* */
	SceUID mainThread;
	SceUID syncThread;
	SceUID miscHeap;
	SceUInt32 status;			/* TSMP_STATUS_* */
	TsmpBuffer send;
	TsmpBuffer recv;
	TsmpNotificationQueue registry;
	TsmpNotificationQueue swInfo;
	SceSize fileSize;			/* setting UI file / event log buffer */
	SceUID fileBlock;
	void *fileBase;
	TsmpRegFileRequest fileRequest;
	SceSize fileSent;
	TsmpNotification regEntry;		/* filled by the registry for the callback */
	TsmpNotification current;		/* last popped notification */
} TsmpState;

_Static_assert(sizeof(TsmpHeader) == 0x1c, "TSMP header");
_Static_assert(sizeof(TsmpNotification) == 0x8cc, "TSMP notification");
_Static_assert(offsetof(TsmpState, send) == 0x5c, "TSMP send buffer");
_Static_assert(offsetof(TsmpState, registry) == 0xfc, "TSMP registry queue");
_Static_assert(offsetof(TsmpState, fileSize) == 0x1a4, "TSMP file");
_Static_assert(offsetof(TsmpState, regEntry) == 0x1d4, "TSMP registry entry");
_Static_assert(sizeof(TsmpState) == 0x136c, "TSMP state");
