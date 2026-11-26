/*
 * The DTCP manager object (SwMwManager.cpp): the session lifecycle the manager dispatch
 * drives, the keybin and record setup, and the checked status helper.
 */

#pragma once

#include <scetypes.h>

/* the session object the manager keeps in its first word (the SWS content session's
 * library class): start, close, the record write and exchange, and the three interface
 * operations the middleware does not call (their names are ours) */
struct SwMwManagerObject {
	virtual void destroy();				/* 0x00 */
	virtual void dispose();				/* 0x04 */
	virtual int start();				/* 0x08 */
	virtual void close();				/* 0x0c */
	virtual void discard();				/* 0x10 */
	virtual void notify();				/* 0x14 */
	virtual void poll();				/* 0x18 */
	virtual int write(int value, int size);		/* 0x1c */
	virtual int exchange(int size, void *buffer, int *value, void *owner);	/* 0x20 */
};

/* the activation worker's state: its flag, the worker's status and the activation result */
struct SwMwManagerActivation {
	unsigned char state;		/* 0x00 */
	unsigned char padding[3];	/* 0x01 */
	int status;			/* 0x04 */
	int result;			/* 0x08 */
};

/* the parameter table worker's buffer: 612 bytes, its state flag and its status */
struct SwMwManagerParamTbl {
	unsigned char data[612];	/* 0x000 */
	unsigned char state;		/* 0x264 */
	unsigned char padding[3];	/* 0x265 */
	int status;			/* 0x268 */
};

/* one of the manager's threads: its entry, its id once started and its argument */
struct SwMwManagerThread {
	int (*entry)(void *argument);	/* 0x00 */
	SceUID id;			/* 0x04 */
	void *argument;			/* 0x08 */
};

/* the device's two 35-entry session tables (the manager keeps its sinks and move sinks) */
struct SwMwManagerSinkSlots {
	int reserved;		/* 0x000 */
	int sinks[35];		/* 0x004: the sinks (0x81005060) */
	int moveSinks[35];	/* 0x090: the move sinks (0x810050FC/0x8100514A) */
	int starting;		/* 0x11C: the mode of the start in flight */
	void *support;		/* 0x120: the content support (0x81013790) */
	void *content;		/* 0x124: the content object (the streams' manager) */
	void *manager;		/* 0x128: the manager object */
};

/* the certification descriptor the manager create takes (0x81002de2): the template
 * pointer, the device certificate's three bytes at +4, the template type at +0x88 and its
 * length at +0x8c */
struct SwMwManagerCertificate {
	void *templatePtr;		/* 0x000 */
	unsigned char device[3];	/* 0x004 */
	unsigned char padding[129];	/* 0x007 */
	int type;			/* 0x088 */
	int size;			/* 0x08c */
};

/* the manager object: its first word, the device slots the session start uses, then the
 * key bin, the started flag, the four record handles and the two parameters */
struct SwMwManager {
	struct SwMwManagerObject *object;	/* 0x000 */
	struct SwMwManagerSinkSlots slots;	/* 0x004 */
	int keyBin;				/* 0x130 */
	int started;				/* 0x134 */
	int recordA;				/* 0x138 */
	int recordB;				/* 0x13C */
	int recordC;				/* 0x140 */
	int recordD;				/* 0x144 */
	int padding[2];				/* 0x148 */
	int parameter0;				/* 0x150 */
	int parameter1;				/* 0x154 */
	int tail[4];				/* 0x158: the base's own fields */
};


/* the SWS content support singleton's interface the start builds (0x81013790's class) */
struct SwMwContentSupport {
	virtual void destroy();				/* 0x00 */
	virtual void dispose();				/* 0x04 */
	virtual int createContent(void **content);	/* 0x08 */
	virtual void release();				/* 0x0c */
	virtual int createManager(void **manager);	/* 0x10 */
};

extern "C" {
int SwMwManagerMapStatus(int status);
int SwMwManagerCancel(void);
int SwMwManagerCheckActivation(void);
int SwMwManagerJoinThread(struct SwMwManagerThread *thread);
int SwMwManagerStartActivation(void);
int SwMwManagerGetActivationResult(int *result);
int SwMwManagerActivationThread(void *argument);
int SwMwManagerParamTblThread(void *argument);
int SwMwManagerDownloadParamTbl(void);
int SwMwManagerEnsureSession(struct SwMwManager *manager, int mode);
int SwMwManagerCreate(struct SwMwManager **out, const int *config,
		      struct SwMwManagerCertificate *certificate, int argument);
int SwMwManagerTeardown(struct SwMwManager *manager);
int SwMwManagerWriteRecord(struct SwMwManager *manager, int value, int size);
int SwMwManagerWriteRecordDone(struct SwMwManager *manager, int value, int size, int *done);
int SwMwManagerExchangeRecord(struct SwMwManager *manager, void *buffer, unsigned int size,
			      int *value);
int SwMwManagerThreadCreate(struct SwMwManagerThread *thread, int (*entry)(void *),
			    void *argument, int type);
int SwMwManagerSessionStart(struct SwMwManagerSinkSlots *slots, int mode);
int SwMwContentSupportCreate(struct SwMwContentSupport **support);
int SwMwManagerRegisterMoveSink(struct SwMwManagerSinkSlots *slots, int sink);
int SwMwManagerUnregisterMoveSink(struct SwMwManagerSinkSlots *slots, int sink);
}
