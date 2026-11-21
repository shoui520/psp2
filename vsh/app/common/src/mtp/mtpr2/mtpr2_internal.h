/*
 * mtpr2_internal.h: MTPR2's classes (a header of ours: no record names one). The class
 * names, CCondvar::wait and signalAndWait, Daemon::getFileBuf, FileBuf::read and write,
 * FileHandleTable::getDaemon, FileHandleTableEntry::waitRefCountZero, the responders'
 * startDownload, receiveData, endDownload and releaseExclusiveUse and every constructor's
 * arguments are Sony's (its messages); every other name is ours (PLACEHOLDER).
 */
#ifndef _MTPR2_INTERNAL_H
#define _MTPR2_INTERNAL_H

#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>

#include "mtpr2.h"

/* what Sony's stripped messages left: an error is printed in two parts */
#define MTPR2_PRINT_ERROR()				\
	do {						\
		sce_paf_printf_dummy();			\
		sce_paf_printf_dummy();			\
	} while (0)

#define MTPR2_ASSERT(cond)				\
	do {						\
		if (!(cond)) {				\
			sce_paf_printf_dummy();		\
			while (1)			\
				;			\
		}					\
	} while (0)

#define MTPR2_WAIT_US			10000		/* PLACEHOLDER names */
#define MTPR2_DAEMON_ERROR_WAIT_US	100000
#define MTPR2_LOCK_TIMEOUT_US		1000000
#define MTPR2_FILE_BUF_LOW		0x200		/* a read wakes the daemon below it */
#define MTPR2_FILE_HANDLE		0x900DCAFE	/* the one download's handle */

namespace mtpr2 {

class Daemon;

/* a condition variable with its mutex */
class CCondvar {
public:
	CCondvar(int *error);
	~CCondvar();

	int signal(void);
	int wait(unsigned int *timeout);
	int signalAndWait(unsigned int *timeout);

	int waiters;			/* the threads in wait() */
	SceUID mutex;
	SceUID cond;
};

/* a ring buffer */
class RingBuf {
public:
	RingBuf(unsigned int size, int *error);
	~RingBuf();

	unsigned int freeSize(void);
	unsigned int dataSize(void);
	void read(void *dst, unsigned int size, unsigned int *read);
	void write(void *src, unsigned int size, unsigned int *written);

	char *buf;
	unsigned int size;
	char *writePos;
	char *readPos;
};

/* FileBuf's states */
enum FileBufStatus {
	FILE_BUF_STATUS_NONE,
	FILE_BUF_STATUS_INITIALIZED,
	FILE_BUF_STATUS_STARTED,
	FILE_BUF_STATUS_ENDED,
	FILE_BUF_STATUS_CANCELED
};

/* the downloaded file between the daemon and the reader */
class FileBuf {
public:
	FileBuf(unsigned int ringBufSize, CCondvar *cCondvar, unsigned int *timeout, int *error);
	~FileBuf();

	void start(unsigned int size);
	int read(void *buf, unsigned int size, long long offset, unsigned int *read);
	int write(void *buf, unsigned int size, long long offset, unsigned int *written);
	unsigned int freeSize(void);

	SceUInt64 offset;		/* where the next read starts */
	CCondvar *cCondvar;
	SceUID mutex;
	int status;			/* FileBufStatus */
	RingBuf *ringBuf;
	unsigned int size;		/* the file's */
	unsigned int *timeout;
};

/* a responder: what serves the file */
class MtpResponder {
public:
	MtpResponder()
	{
	}

	virtual int open(void) = 0;
	virtual ~MtpResponder()
	{
	}

	virtual int getExclusiveUse(void) = 0;
	virtual int releaseExclusiveUse(void) = 0;
	virtual int startDownload(const char *url, long long offset, int *kind,
				  unsigned int *size, unsigned int *time) = 0;
	virtual int receiveData(void *buf, unsigned int size, unsigned int *received) = 0;

	virtual int abortDownload(void)
	{
		return 0;
	}

	virtual int endDownload(void) = 0;
};

/* the host's file system */
class MtpResponderHostFs : public MtpResponder {
public:
	MtpResponderHostFs()
	{
	}

	virtual int open(void)
	{
		return 0;
	}

	virtual int getExclusiveUse(void)
	{
		return 0;
	}

	virtual int releaseExclusiveUse(void)
	{
		return 0;
	}

	virtual int startDownload(const char *url, long long offset, int *kind,
				  unsigned int *size, unsigned int *time);
	virtual int receiveData(void *buf, unsigned int size, unsigned int *received);
	virtual int endDownload(void);

	const char *path;
	SceUID fd;
	int offset;			/* PLACEHOLDER: the offset's low word, not read */
};

/* the PC's MTP connection, protocol 2 */
class MtpResponderProto2 : public MtpResponder {
public:
	MtpResponderProto2();
	virtual ~MtpResponderProto2();

	virtual int open(void)
	{
		return 0;
	}

	virtual int getExclusiveUse(void)
	{
		return 0;
	}

	virtual int releaseExclusiveUse(void)
	{
		return 0;
	}

	virtual int startDownload(const char *url, long long offset, int *kind,
				  unsigned int *size, unsigned int *time);
	virtual int receiveData(void *buf, unsigned int size, unsigned int *received);
	virtual int abortDownload(void);
	virtual int endDownload(void);

	bool failed;			/* the transfer failed */
	bool started;
	int handle;			/* the MTP object's, -1 none */
	SceInt64 offset;
	SceUInt64 remaining;		/* what is left to receive */
	bool aborted;
};

/* the daemon's states */
enum DaemonState {
	DAEMON_STATE_NONE,
	DAEMON_STATE_STARTING,
	DAEMON_STATE_RUNNING,
	DAEMON_STATE_ERROR,
	DAEMON_STATE_DONE
};

/* the responder Daemon uses */
enum DaemonProtocol {
	DAEMON_PROTOCOL_HOSTFS,
	DAEMON_PROTOCOL_PROTO2
};

/* the thread that downloads the file into its FileBuf */
class Daemon {
public:
	Daemon(CCondvar *cCondvar, const char *url, long long offset, unsigned int ringBufSize,
	       unsigned int *timeout);
	~Daemon();

	int initialize(void);
	int startResponder(void);
	int terminate(void);
	void main(void);
	int getFileBuf(FileBuf **fileBuf);
	int getFileAttributes(MTPR2FileAttributes *attr);
	int abortResponder(void);

	int state;			/* DaemonState */
	int error;
	CCondvar *cCondvar;
	int cancel;			/* 1: stop */
	FileBuf *fileBuf;
	void *buf;			/* what one receive gets */
	unsigned int bufSize;
	int protocol;			/* DaemonProtocol */
	MtpResponder *mtpResponder;
	bool registered;		/* in the FileHandleTable */
	SceInt64 offset;
	unsigned int ringBufSize;
	unsigned int *timeout;
	char url[MTPR2_URL_MAX + 1];
	unsigned int size;
	unsigned int time;
	int kind;
	SceUID thread;
};

/* the one download's handle */
class FileHandleTableEntry {
public:
	FileHandleTableEntry(Daemon *daemon, SceUID mutex, SceUID cond)
		: fileHandle(MTPR2_FILE_HANDLE), daemon(daemon), refCount(0), mutex(mutex),
		  cond(cond)
	{
	}

	int waitRefCountZero(unsigned int *timeout);

	unsigned int fileHandle;
	Daemon *daemon;
	int refCount;			/* the callers using the daemon */
	SceUID mutex;
	SceUID cond;
};

/* the downloads under way: only one */
class FileHandleTable {
public:
	FileHandleTable(int *error);
	~FileHandleTable();

	int getDaemon(unsigned int fileHandle, Daemon **daemon);
	void releaseDaemon(Daemon *daemon);
	int registerDaemon(Daemon *daemon);
	int unregisterDaemon(Daemon *daemon);
	bool findDaemon(SceUID thread, unsigned int *fileHandle);
	void cancel(void);
	bool isCanceled(void);

	FileHandleTableEntry *entries;
	SceUID mutex;
	SceUID cond;
	bool canceled;
};

}

extern mtpr2::FileHandleTable *g_fileHandleTable;	/* PLACEHOLDER name */

#endif
