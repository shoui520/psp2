/* Application abort handling and coredump controls. */

#include <stddef.h>
#include <sysmem/debug.h>
#include <sysmem/barrier.h>
#include <stdint.h>
#include <threadmgr/threadmgr.h>
#include <appmgr/appmgr.h>

extern int dumpBgmState(void);
extern int appmgrDebugConsoleInitialize(void);
extern int handleAbortedApp(SceSize args, void *argp);
extern int appmgrProcessDebugSuspendHandler(SceUID pid, SceUID threadId,
					  int reason, uintptr_t arg);

SceUID g_appmgrAbortEventFlag;
SceUID g_appmgrCoredumpEventFlag;
SceUID g_appmgrAbortThreadId;
int g_appmgrShellCrashed;

#if FW_DEBUG
int g_fakeCoredumpStartFailure;
int g_fakeAbortReasonOne;
int g_fakeAbortCurrentProcess;
#else
int g_fakeAbortCurrentProcess;
int g_fakeCoredumpStartFailure;
int g_fakeAbortReasonOne;
#endif

#if !FW_DEBUG
static const SceKernelDebugInfo deleteAbortPanicInfo = {
	.fileHash = 0x0e824785,
	.lineHash = 0xa554bf80,
	.funcHash = 0xd2ba855d,
};
static const SceKernelDebugInfo startAbortPanicInfo = {
	.fileHash = 0x0e824785,
	.lineHash = 0xdd9b0480,
	.funcHash = 0xd2ba855d,
};
static const SceKernelDebugInfo registerAbortPanicInfo = {
	.fileHash = 0x0e824785,
	.lineHash = 0x05d80b17,
	.funcHash = 0xd2ba855d,
};
static const SceKernelDebugInfo notifyAbortPanicInfo = {
	.fileHash = 0x0e824785,
	.lineHash = 0xe06fd284,
	.funcHash = 0xa0c1f7c0,
};
static const SceKernelDebugInfo deleteAbortAfterCreatePanicInfo = {
	.fileHash = 0x0e824785,
	.lineHash = 0xf37c4d09,
	.funcHash = 0xd2ba855d,
};
static const SceKernelDebugInfo deleteCoredumpPanicInfo = {
	.fileHash = 0x0e824785,
	.lineHash = 0xb0aa5ea0,
	.funcHash = 0xd2ba855d,
};
#endif

SceUID appmgrGetAbortThreadId(void)
{
	return g_appmgrAbortThreadId;
}

int sceAppMgrSetFakeSettingBug51800ForUser(int abortCurrentProcess,
					 int forceAbortReasonOne, int failCoredumpStart)
{
#if FW_DEBUG
	SCE_DBG_PRINTF_FUNC(0, 155, "sceAppMgrSetFakeSettingBug51800ForUser",
		"sceAppMgrSetFakeSettingBug51800(%d,%d,%d) is Called\n",
		abortCurrentProcess, forceAbortReasonOne, failCoredumpStart);
#endif
	g_fakeAbortCurrentProcess = abortCurrentProcess;
	g_fakeAbortReasonOne = forceAbortReasonOne;
	g_fakeCoredumpStartFailure = failCoredumpStart;
	return 0;
}

void _sceAppMgrNotifyAppMgrHandler(void)
{
	if (sceKernelSetEventFlag(g_appmgrAbortEventFlag, 1) != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 940, "_sceAppMgrNotifyAppMgrHandler");
#else
		SceDebugForDriver_391B5B74(&notifyAbortPanicInfo, __builtin_return_address(0));
#endif
	}
}

#if FW_DEBUG
#define ABORT_INITIALIZE_CHECK(result, line, panicInfo) do { \
	if ((result) != 0) { \
		sceAppMgrDump(); \
		dumpBgmState(); \
		SCE_DBG_ASSERT_CHECK_FUNC(0, line, "_sceAppMgrAbortHandlerInitialize"); \
	} \
} while (0)
#else
#define ABORT_INITIALIZE_CHECK(result, line, panicInfo) do { \
	if ((result) != 0) \
		SceDebugForDriver_391B5B74(&(panicInfo), __builtin_return_address(0)); \
} while (0)
#endif

int _sceAppMgrAbortHandlerInitialize(void)
{
	int result = appmgrDebugConsoleInitialize();

	if (result < 0)
		return result;
	result = sceKernelCreateEventFlag("SceAppMgrAbort", 0, 0, NULL);
	if (result < 0)
		return result;
	g_appmgrAbortEventFlag = result;
	result = sceKernelCreateEventFlag("SceAppMgrCoredump", 0, 0, NULL);
	if (result < 0) {
		result = sceKernelDeleteEventFlag(g_appmgrAbortEventFlag);
		ABORT_INITIALIZE_CHECK(result, 1032, deleteAbortAfterCreatePanicInfo);
		return result;
	}
	g_appmgrCoredumpEventFlag = result;
	g_appmgrShellCrashed = 0;
	dmb();
	result = sceKernelCreateThread("SceAppMgrAbortHandler", handleAbortedApp,
				       31, 4096, 0, 0x80000, NULL);
	if (result < 0) {
		result = sceKernelDeleteEventFlag(g_appmgrAbortEventFlag);
		ABORT_INITIALIZE_CHECK(result, 1052, deleteAbortPanicInfo);
		result = sceKernelDeleteEventFlag(g_appmgrCoredumpEventFlag);
		ABORT_INITIALIZE_CHECK(result, 1054, deleteCoredumpPanicInfo);
		return result;
	}
	g_appmgrAbortThreadId = result;
	result = sceKernelStartThread(result, 0, NULL);
	ABORT_INITIALIZE_CHECK(result, 1059, startAbortPanicInfo);
	result = SceThreadmgrForDriver_5311CFB5(
		(uintptr_t)appmgrProcessDebugSuspendHandler, 0);
	ABORT_INITIALIZE_CHECK(result, 1063, registerAbortPanicInfo);
	return 0;
}

#undef ABORT_INITIALIZE_CHECK
