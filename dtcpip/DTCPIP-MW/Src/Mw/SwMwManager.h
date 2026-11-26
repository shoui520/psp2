/*
 * The DTCP manager object (SwMwManager.cpp): the session lifecycle the manager dispatch
 * drives, the keybin and record setup, and the checked status helper.
 */

#pragma once

#include <scetypes.h>

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
int SwMwManagerThreadCreate(struct SwMwManagerThread *thread, int (*entry)(void *),
			    void *argument, int type);
int SwMwManagerSessionStart(struct SwMwManagerSinkSlots *slots, int mode);
int SwMwContentSupportCreate(struct SwMwContentSupport **support);
int SwMwManagerRegisterMoveSink(struct SwMwManagerSinkSlots *slots, int sink);
int SwMwManagerUnregisterMoveSink(struct SwMwManagerSinkSlots *slots, int sink);
}
