/*
 * shell_util_server.h: shared internal declarations for SceShell's ShellUtil lock manager
 * (0x81248A10-0x812499F5), ShellUtil IPMI server manager (0x812499F6-0x8124D61B), and
 * strong dialog state watcher (0x8124D61C-0x8124DFFF).
 */
#ifndef _SHELL_UTIL_SERVER_H
#define _SHELL_UTIL_SERVER_H

#include <scetypes.h>
#include <std/list>
#include <std/map>
#include <std/string>
#include <common/shared_ptr.h>
#include <thread/thread.h>
#include <ipmi/session.h>
#include <common_gui_dialog/common_gui_dialog.h>

/* SceShellUtil error codes */
enum {
	SHELL_UTIL_ERR_NOT_READY		= -2146428928,	/* 0x80101800 */
	SHELL_UTIL_ERR_INVALID_PARAM		= -2146428924,	/* 0x80101804 */
	SHELL_UTIL_ERR_NO_MEMORY		= -2146428923,	/* 0x80101805 */
	SHELL_UTIL_ERR_NOT_LOCKED		= -2146428922,	/* 0x80101806 */
	SHELL_UTIL_ERR_PERMISSION_DENIED	= -2146428921,	/* 0x80101807 */
	SHELL_UTIL_ERR_URI_NOT_FOUND		= -2146428911,	/* 0x80101811 */
	SHELL_UTIL_ERR_DEFERRED			= -2136014815,	/* 0x80AF0021 */
	SHELL_UTIL_ERR_NOT_INITIALIZED		= -2136014847,	/* 0x80AF0001 */
	SHELL_UTIL_ERR_NOT_FOUND		= -2136014811,	/* 0x80AF0025 */
	SHELL_UTIL_ERR_INVALID_ID		= -2136014705,	/* 0x80AF008F */
};

/* Lock bit mask constants (12 managed lock slots, 0x001..0x800) */
enum {
	SHELL_LOCK_SLOT_COUNT			= 12,
	SHELL_LOCK_ALL_MASK			= 0xFFF,
	SHELL_LOCK_PS_BUTTON_SLOT		= 11,
};

typedef int (*ShellLockHandlerFn)(SceUID clientPid);

struct ShellLockPendingOp {
	virtual ~ShellLockPendingOp() {}

	int operation;
	SceUInt32 lockMask;
	IPMI::Session *session;
	int requestId;
};

struct ShellLockSlot {
	ShellLockSlot() : active(false), totalCount(0), lockFn(NULL), unlockFn(NULL) {}
	virtual ~ShellLockSlot() {}

	bool active;
	int totalCount;
	paf::map<IPMI::Session *, int> sessionRefs;
	ShellLockHandlerFn lockFn;
	ShellLockHandlerFn unlockFn;
};

class ShellLockManager {
public:
	ShellLockManager();
	virtual ~ShellLockManager();

	int Lock(SceUInt32 lockMask, IPMI::Session *session, int requestId);
	int Unlock(SceUInt32 lockMask, IPMI::Session *session, int requestId);
	int UnlockAll(SceUInt32 lockMask, IPMI::Session *session, int requestId);
	int OnAsyncStepCompleted();
	int OnPsButtonPressed();
	int OnAppExited(SceUID processId);

private:
	int m_pendingAsyncCount;
	paf::list<ShellLockPendingOp> m_pendingOps;
	ShellLockSlot m_slots[SHELL_LOCK_SLOT_COUNT];
};

/* Singleton accessors for ShellLockManager */
ShellLockManager *shellHandlerTableGetInstance(void);
SceUInt32 shellHandlerTableGetAllMask(void);

/* Async IPMI completion helper (0x8124D420) */
int shellUtilSendAsyncResponse(IPMI::Session *session, int requestId, int result);

/* ShellUtil IPMI server dispatchers (0x8124BAC8, 0x8124BADC, 0x8124BAEC) */
int shellSvcDisp_0C_Sync(IPMI::Session *session, SceUInt32 methodId,
	const IPMI::DataInfo *inList, SceUInt32 inCount,
	IPMI::BufferInfo *outList, SceUInt32 outCount);
int shellSvcDisp_0C_Async(IPMI::Session *session, SceUInt32 requestId, SceUInt32 methodId,
	const IPMI::DataInfo *inList, SceUInt32 inCount);
void shellSvcDisp_0C_Disconn(IPMI::Session *session);

/* Direct shell entrypoints into ShellUtilServerMgr */
int shellShutdownRequest(IPMI::Session *session, int flags);
int shellColdResetRequest(IPMI::Session *session, int flags);
int shellUtilShowErrorDialog(IPMI::Session *session, int errorType, int errorCode, int flags);
int shellSystemNoticeRequest(IPMI::Session *session, int requestId, const char *themePath);

/* Strong dialog state watcher entrypoints (0x8124D61C..0x8124D69A) */
int shellStateWatcherOpenDialog(CommonGuiLayoutDialogParam *param, bool playSound, int level);
int shellErrorDialogOpenParam(CommonGuiMessageDialogParam *param, bool playSound, int level);
int shellStateWatcherOpenListDialog(CommonGuiListDialogParam *param, bool playSound, int level);
int shellStateWatcherCloseDialog(int dialogId);

#endif /* _SHELL_UTIL_SERVER_H */
