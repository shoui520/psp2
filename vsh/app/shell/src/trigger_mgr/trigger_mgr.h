/*
 * trigger_mgr.h: SceTriggerMgr public shell interface (0x812A4876-0x812A5885).
 * Manages background periodic, time-of-day, and daily timers for applications
 * and built-in shell services (SHELL0001..SHELL0006), persisted to
 * ur0:shell/trigger_table.bin and exposed over SceShellSvc IPMI channel 0x00100000.
 */
#ifndef SHELL_TRIGGER_MGR_H
#define SHELL_TRIGGER_MGR_H

#include <scetypes.h>

namespace IPMI {
class Session;
struct DataInfo;
struct BufferInfo;
}

typedef void (*ShellTriggerServiceCallback)(bool enabled, void *userData);
typedef void (*ShellTriggerCtrlCallback)(void *userData);

/* Lifecycle and service control (0x812A4876, 0x812A4A3A, 0x812A4B48, 0x812A4C58) */
int shellTriggerMgrInit(void);
int shellTriggerMgrResume(ShellTriggerCtrlCallback callback, void *userData);
int shellTriggerMgrSuspend(ShellTriggerCtrlCallback callback, void *userData);
int shellTriggerMgrSvcCtrl(int command, int arg);
void shellTriggerTableReset(void);

/* Built-in service registration and toggle notification (0x812A4C6C-0x812A509E) */
int shellTriggerMgrRegisterService(const char *serviceId);
int shellTriggerMgrUnregisterService(const char *serviceId);
int shellTriggerMgrSetServiceEnabled(const char *serviceId, bool enabled);
int shellTriggerMgrIsServiceEnabled(const char *serviceId, bool *outEnabled);
int shellTriggerMgrSetServiceCallback(const char *serviceId,
				      ShellTriggerServiceCallback callback,
				      void *userData);
int shellTriggerMgrClearServiceCallback(const char *serviceId);

/* SceShellSvc IPMI channel 0x00100000..0x0010FFFF dispatchers (0x812A581C-0x812A5840) */
int shellSvcDisp_10_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount,
			 IPMI::BufferInfo *outList, SceUInt32 outCount);
int shellSvcDisp_10_Async(IPMI::Session *session, SceUInt32 requestId,
			  SceUInt32 methodId, const IPMI::DataInfo *inList,
			  SceUInt32 inCount);
void shellSvcDisp_10_Disconn(IPMI::Session *session);

#endif
