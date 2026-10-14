/* Application abort handling and coredump controls. */

#include <sce/debug.h>
#include <sce/barrier.h>
#include <stdint.h>
#include <psp2kern/kernel/threadmgr.h>

extern int sceAppMgrDump(void);
extern int dumpBgmState(void);
extern int appmgrDebugConsoleInitialize(void);
extern int handleAbortedApp(SceSize args, void *argp);
extern int appmgrProcessDebugSuspendHandler(SceUID pid, SceUID threadId,
					  int reason, uintptr_t arg);
extern int SceThreadmgrForDriver_5311CFB5(uintptr_t hook, uintptr_t arg);

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
	if (ksceKernelSetEventFlag(g_appmgrAbortEventFlag, 1) != 0) {
#if FW_DEBUG
		sceAppMgrDump();
		dumpBgmState();
		SCE_DBG_ASSERT_CHECK_FUNC(0, 940, "_sceAppMgrNotifyAppMgrHandler");
#else
		ksceKernelPanic(&notifyAbortPanicInfo, __builtin_return_address(0));
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
		ksceKernelPanic(&(panicInfo), __builtin_return_address(0)); \
} while (0)
#endif

int _sceAppMgrAbortHandlerInitialize(void)
{
	int result = appmgrDebugConsoleInitialize();

	if (result < 0)
		return result;
	result = ksceKernelCreateEventFlag("SceAppMgrAbort", 0, 0, NULL);
	if (result < 0)
		return result;
	g_appmgrAbortEventFlag = result;
	result = ksceKernelCreateEventFlag("SceAppMgrCoredump", 0, 0, NULL);
	if (result < 0) {
		result = ksceKernelDeleteEventFlag(g_appmgrAbortEventFlag);
		ABORT_INITIALIZE_CHECK(result, 1032, deleteAbortAfterCreatePanicInfo);
		return result;
	}
	g_appmgrCoredumpEventFlag = result;
	g_appmgrShellCrashed = 0;
	dmb();
	result = ksceKernelCreateThread("SceAppMgrAbortHandler", handleAbortedApp,
				       31, 4096, 0, 0x80000, NULL);
	if (result < 0) {
		result = ksceKernelDeleteEventFlag(g_appmgrAbortEventFlag);
		ABORT_INITIALIZE_CHECK(result, 1052, deleteAbortPanicInfo);
		result = ksceKernelDeleteEventFlag(g_appmgrCoredumpEventFlag);
		ABORT_INITIALIZE_CHECK(result, 1054, deleteCoredumpPanicInfo);
		return result;
	}
	g_appmgrAbortThreadId = result;
	result = ksceKernelStartThread(result, 0, NULL);
	ABORT_INITIALIZE_CHECK(result, 1059, startAbortPanicInfo);
	result = SceThreadmgrForDriver_5311CFB5(
		(uintptr_t)appmgrProcessDebugSuspendHandler, 0);
	ABORT_INITIALIZE_CHECK(result, 1063, registerAbortPanicInfo);
	return 0;
}

#undef ABORT_INITIALIZE_CHECK
