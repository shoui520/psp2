/*
 * The shell's start: the time it started, the framework and the start job that loads the system
 * modules and starts the shell's services, then the cold-boot plugin and the framework's run.
 * When the job is done the start's error, if any, is shown, else the system plugins are loaded.
 * The allocator given to bXCe takes paf's heap. The file's and every name here are ours (no
 * record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string>
#include <framework.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <common/main_thread_call_list.h>
#include <memory/heap_allocator.h>
#include <dir/dir.h>
#include <system/system.h>
#include <kernel/threadmgr.h>
#include <kernel/processmgr.h>
#include <kernel/sysmem.h>
#include <libkernel/libkernel.h>
#include <libmd5.h>
#include <activity_db/activity_db.h>
#include <activity_db/activity_db_internal.h>
#include <appmgr/appmgr.h>
#include <pfsmgr/pfsmgr.h>
#include <regmgr/regmgr.h>
#include <vshbridge/vshbridge.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <shell_svc/shellsvc.h>
#include <shell_svc/shell_util.h>
#include <bXCe/bxce_internal.h>

#include "boot.h"
#include "shell.h"
#include "src/livespace_db/lsdb_keys.h"
#include "src/np/rt/np_rt_core.h"
#include "src/np/polling/np_polling.h"
#include "src/np/imps/np_imps_core.h"
#include "src/np/imps/np_imps_svc.h"
#include "src/np/roster/np_roster.h"
#include "src/np/commerce2/np_commerce2_core.h"

using namespace paf;

/* the application data mounted at start */
#define SHELL_APPDATA_MOUNT_ID		105

/* ux0:data's mode */
#define SHELL_DATA_DIR_MODE		0x186

/* the start ends here when the id storage does not say test mode and the kit is not a test
   kit (value only: no name known) */
#define SHELL_START_ERROR_NOT_TEST	0x80AF0002

/* the common GUI dialog value set after the start (value only) */
#define SHELL_COMMON_GUI_DIALOG_VALUE	38

/* paf's model type of the PlayStation TV (value only) */
#define SHELL_MODEL_TYPE_1		1

static SceUInt64 s_startTime = 0;
static int s_startupError = 0;

/* bXCe's memory: paf's heap */
class ShellXmlAllocator : public bXCe::MemAllocator {
public:
	ShellXmlAllocator()
	{
	}

	virtual ~ShellXmlAllocator()
	{
	}

	virtual void *allocate(size_t size, void *userData)
	{
		(void)userData;
		return sce_paf_malloc(size);
	}

	virtual void deallocate(void *ptr, void *userData)
	{
		sce_paf_free(ptr);
	}
};

/* the message the error dialog is given (empty) */
static wstring s_errorMessage;
static ShellXmlAllocator s_xmlAllocator;
static ScePfsRndDriveId s_appDataMountPoint;

/* the services' start, run by the job queue */
class ShellStartJob : public job::JobItem {
public:
	ShellStartJob() : job::JobItem(NULL, NULL)
	{
	}

	virtual ~ShellStartJob()
	{
	}

	virtual int Run();
	virtual void Finish(int result);
};

extern int shellSvcServerInit(void);
extern int consoleInfoFileWriteInt64(int index, SceUInt64 value, int mode);

/* the shell service's server and the shell utility's events */
__attribute__((noinline)) static int shellSvcStart(void)
{
	shellSvcServerInit();
	shellSvcServerStart();
	return SceShellSvc_E91AEAD4();
}

int main(int argc, char *argv[])
{
	s_startTime = SceThreadmgr_47F6DE49();
#if defined(FW_TYPE_CEX)
	sceClibPrintf("[Shell]: vsh/release_branches/release_03.650 r142704 (build#757166)\n");
#elif FW_VERSION == 0x03630011
	sceClibPrintf("[Shell]: vsh/release_branches/release_03.630 r142609 (build#756499)\n");
#else
	sceClibPrintf("[Shell]: vsh/release_branches/release_03.650 r142704 (build#757167)\n");
#endif
	sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
	s_startupError = shellStartupInit();
	shellFrameworkCreate();
	shellLsdbRegCallbackCreate();
	shellMtpRegCallbackCreate();

	{
		ShellStartJob *item = new ShellStartJob();
		common::SharedPtr<job::JobItem> job(item);

		job::JobQueue::default_queue->Enqueue(job);
		job::JobQueue::CheckAllQueues();
	}

	g_shellFramework->LoadCommonResourceSync(NULL);
	g_shellFramework->InitCommonSE();
	shellBootLoadColdbootPlugin();
	shellFrameworkRun();
	return 0;
}

int shellStartAppDataMount(void)
{
	return sceAppMgrAppDataMount(SHELL_APPDATA_MOUNT_ID, &s_appDataMountPoint);
}

int shellStartAppDataUmount(void)
{
	return sceAppMgrUmount(s_appDataMountPoint.drive_id);
}

SceUInt64 shellStartGetTime(void)
{
	return s_startTime;
}

/* run on the main thread after the start: callbacks, and the shell utility's events */
static void shellStartPoll(void *userdata)
{
	sceKernelCheckCallback();
	SceShellUtilUketorne_FDC48BBE();
}

/* what the start does only on a model other than the PlayStation TV (nothing in this build) */
__attribute__((noinline)) static void shellStartHandheldInit(void)
{
}

int ShellStartJob::Run()
{
	int ret = shellModulesLoad();

	if (ret != 0)
		return ret;

	if (shellStartupNeedsInitialSetup()) {
		Dir::Create("ur0:app");
		Dir::Create("ur0:shell");
		Dir::Create("ur0:shell/tabpaper");
		consoleInfoFileWriteInt64(0, (SceUInt64)-1, 0);
	}
	Dir::Create("ux0:data", SHELL_DATA_DIR_MODE);
	shellStartAppDataMount();
	shellClosedCaptionMigrate();
	shellSvcStart();
	sceShellUtilInitEvents(1);
	ScebXCe_EB4E6C8D(&s_xmlAllocator, NULL);
	shellDbCheckAll();
	shellNetStart();
	shellLsdbStart();
	shellTriggerMgrStart();
	shellNpStart();
	shellActivityDbOpen();
	shellShutterSoundInit();
	shellNpServicesStart();
	shellImeServiceStart();
	if (system::Model::GetType() != SHELL_MODEL_TYPE_1)
		shellStartHandheldInit();
	shellNpNotifyStart();
	shellMtpStart();
	shellMtpMarlinStart();
	shellNoticeServiceStart();
	shellIncomingDialogServerCreate();
	ScePafToplevel_397DD062(g_shellFramework, "vs0:data/internal/effects", 7);
	if (shellIsComTestMode(NULL) == 0 && vshSblUtMgrHasComTestFlag() == 0)
		return SHELL_START_ERROR_NOT_TEST;
	shellDatabaseCheckRecovery();
	shellThemeLoad();
	return ret;
}

void ShellStartJob::Finish(int result)
{
	if (result != 0)
		s_startupError = result;
	SceCommonGuiDialog_5057376C(SHELL_COMMON_GUI_DIALOG_VALUE);
	shellStateWatcherCreate();
	shellAccessibilityMgrCreate();
	shellHandlerTableCreate();
	shellUtilServerMgrCreate();
	common::MainThreadCallList::Register(shellStartPoll, NULL);
	if (s_startupError == 0) {
		shellMainThreadPollerCreate();
		shellBootStartSystemEntry();
	} else {
		int type = SHELL_ERROR_DIALOG_START;

		if (sceRegMgrIsBlueScreen(s_startupError) != 0)
			type = SHELL_ERROR_DIALOG_BLUE_SCREEN;
		shellErrorDialogShow(type, s_startupError, &s_errorMessage, 0);
	}
	sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
}

/* --------------------------------------------------------------------------
 * Shell NP subsystem startup & WebAPI context (0x81003D20..0x81004586)
 * -------------------------------------------------------------------------- */

struct NpBaseUrlCacheAllocator;
struct NpCommonPs4Allocator;

extern "C" {
int sceSblQafMgrIsAllowNpTest(void);
int SceNpCommon_A385C8CD(SceSize stackSize, int priority, SceSize poolSize, void *poolBase);
int SceNpCommon_A7EC5CDB(SceSize stackSize, int priority);
int SceNpCommonPs4_2D7AF34A(SceSize poolSize);
NpBaseUrlCacheAllocator *SceNpCommonPs4_03C9ACE2(void);
NpCommonPs4Allocator *SceNpCommonPs4_53484B79(void);
int SceNpCommonPs4_33B5FE83(void);
int SceNpManager_C89E0B94(int option);
int SceNpManager_77DE17D4(const void *signInCallbacks);
int SceNpActivityNet_348268A0(void *(*allocFn)(SceSize), void (*freeFn)(void *));
int SceNpWebApi_E83CE7F1(const int *errorFilter);
int SceNpWebApi_E8FBA9B8(SceSize poolSize, const char *name);
int SceNpWebApi_4699EF17(const void *commId, const void *commPassphrase);
}

extern int npCommonServiceLoadAccountId(void);
extern int npAuthCoreInit(int mode, SceSize stackSize, int priority);
extern int npScoreIpcServerStart(void);
extern int npTusIpcServerStart(void);
extern int npManagerCoreInit(void);
extern int npSnsFbServerStart(void);
extern int npSnsFbServerSuspend(SceUID pid);
extern int npUpdateClockInit(void);

extern int npImpsSvcInit(void *heapMem, SceSize heapSize, const NpImpsSvcInitParam *param);
extern int npImpsSvcTerm(void);
extern int npImpsUtilInit(void);
extern int npImpsUtilTerm(void);
extern int npImpsUtilNotifyProcessEvent(int eventType, SceUID processId, int appId);
extern int npImpsUtilIsInitialized(void);
extern int npImpsNotifInit(void);
extern int npImpsNotifTerm(void);
extern int npAsmServiceCacheInit(NpBaseUrlCacheAllocator *allocator);
extern void npAsmServiceCacheTerm(void);
extern int sceNpPushInit(void);
extern int sceNpPushTerm(void);
extern int sceNpPushStart(void);
namespace sce {
namespace Np {
namespace Push {
int InitService(void);
int TermService(void);
}
}
}
extern int npAsmVshInit(NpCommonPs4Allocator *allocator);
extern void npAsmVshTerm(void);
extern int npAsmVshIpcInit(void);
extern void npAsmVshIpcTerm(void);
extern int npAsmVshUnregisterActiveTitleId(SceUID appId);
extern int npCommunityInitBackend(void);
extern int npAuthServerInit(void);
extern int npManagerIpcServerInit(void);
extern int npRtSignalingServerInit(void);
extern int npJgaMsgServerInit(void);
extern int npTrophyServerInit(SceSize poolSize);
extern int npTrophyServerSuspend(SceUID pid);
extern int npPartyVshInit(void);
extern int npSnsFbUtilInit(void);
extern int npSnsFbUtilTerm(void);
extern int npUtilityProfileInitWithAllocator(void *(*allocFn)(SceSize), void (*freeFn)(void *));
extern int npCommerce2SessionManagerInit(SceSize stackSize, int priority);
extern int npCommonServiceIsSignedIn(int *outSignedIn);
extern int npOAuthInit(void);
extern int npGamePresenceInit(void);
extern int npServiceInit(void);
extern int npIpc2ServerStart(void);
extern void npAccountNotifyBootSignIn(void);
extern int nsxPushInfoEnqueueJob(void);
extern int npSignInDialogStartModeA(int *outReqId, int dialogParam, SceUID pid);
extern int npSignInDialogStartModeB(int *outReqId, int dialogParam, SceUID pid);
extern int npSignInDialogStartModeC(int *outReqId, int dialogParam, SceUID pid);
extern int npSignInDialogGetStatus(int dialogReqId);
extern int npSignInDialogAbort(int dialogReqId);
extern int npSignInDialogGetResult(int dialogReqId, int *outResult);

static const int kNpWebApiErrorNotInitialized = -2141902335;	/* 0x80552E01 */
static const int kNpImpsErrorNoMemory = -2141905919;		/* 0x80552001 */
static const int kNpImpsSvcErrorNoMemory = -2141906431;		/* 0x80551E01 */

static const SceUInt8 kShellNpWebApiPassphrase[128] = {
	0x90, 0xA3, 0x2F, 0x18, 0x98, 0x8B, 0x68, 0xC1, 0x8E, 0xB4, 0x48, 0x79, 0xBF, 0x90, 0xF8, 0xAE,
	0x7F, 0xF0, 0x96, 0xAE, 0x1E, 0x33, 0x99, 0x43, 0xB5, 0x9C, 0x3D, 0xC2, 0x75, 0x1C, 0x43, 0x5F,
	0x04, 0x48, 0x48, 0x72, 0xE7, 0xA8, 0x21, 0xA4, 0x25, 0x46, 0xD9, 0x89, 0x48, 0xA5, 0xF6, 0xB8,
	0x01, 0x1B, 0x28, 0x2F, 0xA2, 0x89, 0x5E, 0xD2, 0x8B, 0x66, 0x88, 0x85, 0x7E, 0xB7, 0xE1, 0xEA,
	0x17, 0x36, 0x53, 0x46, 0xA5, 0xA2, 0x21, 0xFA, 0x4D, 0xAE, 0x02, 0x96, 0x78, 0x5C, 0x82, 0x5B,
	0x28, 0x59, 0xCA, 0xF1, 0x1E, 0x0D, 0x92, 0xA3, 0xA7, 0x15, 0x17, 0xDE, 0x04, 0x4B, 0x81, 0x40,
	0x17, 0x43, 0xC1, 0xD6, 0x8E, 0x48, 0x16, 0xD4, 0x17, 0x58, 0x0D, 0x15, 0xF0, 0x48, 0x3F, 0xBF,
	0xD1, 0x35, 0xEC, 0x33, 0x78, 0x08, 0xA1, 0x95, 0xFB, 0x41, 0x19, 0xDD, 0xDF, 0x91, 0xDE, 0x1E
};

static const SceUInt8 kAllowedNpEnvDigests[4][16] = {
	{ 0xB7, 0x98, 0x34, 0xED, 0x98, 0x44, 0xEE, 0xC0, 0x31, 0xED, 0xB3, 0xF1, 0xE4, 0x7F, 0x79, 0x86 },
	{ 0x51, 0x31, 0xF0, 0xEF, 0xE7, 0x47, 0xEC, 0xBE, 0x73, 0x8E, 0x71, 0xD8, 0x37, 0x57, 0xD5, 0x61 },
	{ 0xF0, 0xBC, 0x02, 0x51, 0xCE, 0x21, 0xB1, 0x50, 0x23, 0x1F, 0x5D, 0xF9, 0xEA, 0x77, 0x42, 0x1F },
	{ 0xAA, 0x02, 0xF0, 0x9F, 0xE3, 0xA3, 0xA3, 0x89, 0xCE, 0xE6, 0xB4, 0xAA, 0xDB, 0xE7, 0x60, 0xEE }
};

struct ShellNpSignInCallbackTable {
	int headerWord;
	int optionFlags;
	int (*startModeA)(int *outReqId, int dialogParam, SceUID pid);
	int (*startModeB)(int *outReqId, int dialogParam, SceUID pid);
	int (*startModeC)(int *outReqId, int dialogParam, SceUID pid);
	int (*getStatus)(int dialogReqId);
	int (*abort)(int dialogReqId);
	int (*getResult)(int dialogReqId, int *outResult);
};

static const ShellNpSignInCallbackTable kShellNpSignInCallbacks = {
	0,
	0,
	npSignInDialogStartModeA,
	npSignInDialogStartModeB,
	npSignInDialogStartModeC,
	npSignInDialogGetStatus,
	npSignInDialogAbort,
	npSignInDialogGetResult
};

static void *s_npImpsSvcHeapMem = NULL;
static void *s_npImpsUtilHeapMem = NULL;
static memory::HeapAllocator *s_npImpsUtilAllocator = NULL;
static SceUID s_npSystemMemBlock = -1;
static int s_npAccountCallbackId = -1;
static int s_npWebApiLibCtx = 0;
static bool s_npWasSignedIn = false;
static SceUInt8 s_npEnvMd5Digest[16];
static char s_npEnvSaltBuf[128];
static char s_npWebApiPoolName[64];

static void *shellNpPafMalloc(SceSize size)
{
	return sce_paf_malloc(size);
}

static void shellNpPafFree(void *ptr)
{
	sce_paf_free(ptr);
}

/* (0x81003D20) */
int shellNpGetWebApiLibCtx(int *outLibCtx)
{
	if (s_npWebApiLibCtx == 0)
		return kNpWebApiErrorNotInitialized;
	*outLibCtx = s_npWebApiLibCtx;
	return 0;
}

/* (0x810044B0) */
static void shellNpOnProcessCreate(int event, const void *info, SceSize size, void *arg)
{
	(void)size;
	(void)arg;
	if (event == 3 && info != NULL) {
		const SceUID *ids = (const SceUID *)info;
		if (!shellBootIsLaunchRestricted())
			npImpsUtilNotifyProcessEvent(1, ids[0], (int)ids[1]);
	}
}

/* (0x810044CE) */
static void shellNpOnProcessExit(int event, const void *info, SceSize size, void *arg)
{
	(void)size;
	(void)arg;
	if (event == 0 && info != NULL) {
		const SceUID *ids = (const SceUID *)info;
		if (!shellBootIsLaunchRestricted()) {
			npImpsUtilNotifyProcessEvent(2, ids[0], (int)ids[1]);
			npImpsUtilIsInitialized();
		}
		npAsmVshUnregisterActiveTitleId(ids[1]);
	}
}

/* (0x810044F8) */
static void shellNpOnProcessSuspend(int event, const void *info, SceSize size, void *arg)
{
	(void)size;
	(void)arg;
	if (event == 6 && info != NULL) {
		const SceUID *ids = (const SceUID *)info;
		npTrophyServerSuspend(ids[0]);
		npSnsFbServerSuspend(ids[0]);
	}
}

/* (0x8100452C) */
static void shellNpOnAccountEvent(int eventType, void *userArg)
{
	(void)userArg;
	if (eventType == 1) {
		ActivityDbSyncHandle syncHandle;
		syncHandle.ClearWaitStatus();
	} else if (eventType == 0 && !s_npWasSignedIn) {
		s_npWasSignedIn = true;
		nsxPushInfoEnqueueJob();
	}
}

/* (0x81003D3E) */
static int shellNpInitImpsAndPush(void)
{
	NpRtCoreInitParam rtParam;
	rtParam.mainThread.priority = 0x10000100;
	rtParam.mainThread.stackSize = 0x4000;
	rtParam.mainThread.cpuAffinityMask = 0;
	rtParam.recvThread.priority = 0x10000100;
	rtParam.recvThread.stackSize = 0x4000;
	rtParam.recvThread.cpuAffinityMask = 0;

	NpPollingThreadParam pollingParam = { 0x10000100, 0x4000, 0 };
	NpPollingThreadParam impsCoreParam = { 0x10000100, 0x4000, 0 };
	NpPollingThreadParam impsCtlParam = { 0x10000100, 0x4000, 0 };

	int ret = npRtCoreInit(&rtParam);
	if (ret < 0)
		return ret;

	ret = npBasicInit();
	if (ret >= 0) {
		ret = npPollingInit(&pollingParam);
		if (ret >= 0) {
			ret = npImpsCoreInit(&impsCoreParam);
			if (ret >= 0) {
				ret = npImpsCtlInit(&impsCtlParam);
				if (ret >= 0) {
					NpRosterInitParam rosterParam;
					rosterParam.allocFn = sce_paf_malloc;
					rosterParam.freeFn = sce_paf_free;
					ret = npRosterInit(&rosterParam);
					if (ret >= 0) {
						s_npImpsSvcHeapMem = shellNpFriendMalloc(0x64000U);
						if (s_npImpsSvcHeapMem != NULL) {
							NpImpsSvcInitParam svcParam;
							svcParam.cacheThreadPriority = 0x10000100;
							svcParam.cacheThreadStackSize = 0x4000;
							svcParam.ctxMgrThreadPriority = 0x10000100;
							svcParam.ctxMgrThreadStackSize = 0x2000;
							ret = npImpsSvcInit(s_npImpsSvcHeapMem, 0x64000U, &svcParam);
							if (ret >= 0) {
								s_npImpsUtilHeapMem = shellNpFriendMalloc(0x8000U);
								if (s_npImpsUtilHeapMem != NULL) {
									s_npImpsUtilAllocator = new memory::HeapAllocator(
										s_npImpsUtilHeapMem, 0x8000U,
										"SceNpImpsUtilityVsh", NULL);
									if (s_npImpsUtilAllocator != NULL) {
										ret = npImpsUtilInit();
										if (ret >= 0) {
											ret = npImpsNotifInit();
											if (ret >= 0) {
												ret = SceNpCommonPs4_2D7AF34A(53248);
												if (ret >= 0) {
													ret = npAsmServiceCacheInit(SceNpCommonPs4_03C9ACE2());
													if (ret >= 0) {
														ret = sceNpPushInit();
														if (ret >= 0) {
															ret = sce::Np::Push::InitService();
															if (ret >= 0) {
																ret = npAsmVshInit(SceNpCommonPs4_53484B79());
																if (ret >= 0) {
																	ret = npAsmVshIpcInit();
																	if (ret >= 0) {
																		ret = npImpsCtlStartDefault();
																		if (ret >= 0)
																			return 0;
																		npAsmVshIpcTerm();
																	}
																	npAsmVshTerm();
																}
																sce::Np::Push::TermService();
															}
															sceNpPushTerm();
														}
														npAsmServiceCacheTerm();
													}
													SceNpCommonPs4_33B5FE83();
												}
												npImpsNotifTerm();
											}
											npImpsUtilTerm();
										}
										delete s_npImpsUtilAllocator;
										s_npImpsUtilAllocator = NULL;
									} else {
										ret = kNpImpsErrorNoMemory;
									}
									shellNpFriendFree(s_npImpsUtilHeapMem);
									s_npImpsUtilHeapMem = NULL;
								} else {
									ret = kNpImpsErrorNoMemory;
								}
								npImpsSvcTerm();
							}
							shellNpFriendFree(s_npImpsSvcHeapMem);
							s_npImpsSvcHeapMem = NULL;
						} else {
							ret = kNpImpsSvcErrorNoMemory;
						}
						npRosterTerm();
					}
					npImpsCtlTerm();
				}
				npImpsCoreTerm();
			}
			npPollingTerm();
		}
		npBasicTerm();
	}
	npRtCoreTerm();
	return ret;
}

/* (0x81003F90) */
int shellNpStart(void)
{
	void *poolBase = NULL;
	s_npSystemMemBlock = sceKernelAllocMemBlock("SceNpSystem", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
						    0x14E000U, NULL);
	sceKernelGetMemBlockBase(s_npSystemMemBlock, &poolBase);

	char commId[16];
	SceUInt8 passphrase[128];
	sce_paf_memset(commId, 0, sizeof(commId));
	sce_paf_memcpy(commId, "NPXS19999_00", 13);
	sce_paf_memset(passphrase, 0, sizeof(passphrase));
	sce_paf_memcpy(passphrase, kShellNpWebApiPassphrase, sizeof(passphrase));

	char npEnv[20];
	if (sceRegMgrGetKeyStr("/CONFIG/NP", "env", npEnv, 17) >= 0 &&
	    sceSblQafMgrIsAllowNpTest() != 1) {
		sceClibMemset(s_npEnvSaltBuf, 0, sizeof(s_npEnvSaltBuf));
		sceClibStrncpy(s_npEnvSaltBuf, "frontier", sizeof(s_npEnvSaltBuf));
		SceSize remaining = 120U;
		SceSize envLen = sceClibStrnlen(npEnv, 17U);
		bool envValid = false;
		if (envLen > 0U && envLen <= 16U) {
			while (sceClibStrnlen(s_npEnvSaltBuf, sizeof(s_npEnvSaltBuf)) < 100U &&
			       remaining != 0U) {
				sceClibStrncat(s_npEnvSaltBuf, npEnv, remaining);
				remaining = (remaining > envLen) ? (remaining - envLen) : 0U;
			}
			sceMd5Digest(s_npEnvSaltBuf, sizeof(s_npEnvSaltBuf), s_npEnvMd5Digest);

			if (vshSblUtMgrHasStoreFlag() != 0) {
				envValid =
					sceClibMemcmp(s_npEnvMd5Digest, kAllowedNpEnvDigests[0], 16U) == 0 ||
					sceClibMemcmp(s_npEnvMd5Digest, kAllowedNpEnvDigests[1], 16U) == 0 ||
					sceClibMemcmp(s_npEnvMd5Digest, kAllowedNpEnvDigests[2], 16U) == 0 ||
					sceClibMemcmp(s_npEnvMd5Digest, kAllowedNpEnvDigests[3], 16U) == 0;
			} else if (vshSblAimgrIsTest() == 1 || vshSblAimgrIsTool() == 1 ||
				   vshSblAimgrIsDEX() == 1) {
				envValid =
					sceClibMemcmp(s_npEnvMd5Digest, kAllowedNpEnvDigests[0], 16U) == 0 ||
					sceClibMemcmp(s_npEnvMd5Digest, kAllowedNpEnvDigests[1], 16U) == 0 ||
					sceClibMemcmp(s_npEnvMd5Digest, kAllowedNpEnvDigests[2], 16U) == 0;
			} else {
				envValid =
					sceClibMemcmp(s_npEnvMd5Digest, kAllowedNpEnvDigests[0], 16U) == 0;
			}
		}
		if (!envValid) {
			sceClibStrncpy(npEnv, "invalid", 16U);
			sceRegMgrSetKeyStr("/CONFIG/NP", "env", npEnv, 17);
		}
	}

	if (ScebXCe_EF4AF9F9(0, NULL) < 0 ||
	    SceNpCommon_A385C8CD(0x4000, 0x10000100, 0x14E000, poolBase) < 0 ||
	    npCommonServiceLoadAccountId() < 0 ||
	    npAuthCoreInit(0, 0x8000, 0x10000114) < 0 ||
	    npCommunityInitBackend() < 0 ||
	    npScoreIpcServerStart() < 0 ||
	    npTusIpcServerStart() < 0 ||
	    npAuthServerInit() < 0 ||
	    npManagerCoreInit() < 0 ||
	    npManagerIpcServerInit() < 0 ||
	    shellNpFriendMemInit(0x80000U) < 0 ||
	    SceNpCommon_A7EC5CDB(0x4000, 0x10000100) < 0 ||
	    shellNpInitImpsAndPush() < 0 ||
	    npRtSignalingServerInit() < 0 ||
	    npJgaMsgServerInit() < 0 ||
	    SceNpManager_C89E0B94(0) < 0 ||
	    npTrophyServerInit(0x80000U) < 0 ||
	    npPartyVshInit() < 0 ||
	    npSnsFbUtilInit() < 0)
		return 0;

	if (npSnsFbServerStart() < 0) {
		npSnsFbUtilTerm();
		return 0;
	}

	if (npUtilityProfileInitWithAllocator(shellNpPafMalloc, shellNpPafFree) < 0 ||
	    SceNpActivityNet_348268A0(shellNpPafMalloc, shellNpPafFree) < 0 ||
	    npCommerce2CoreInit() < 0 ||
	    npCommerce2SessionManagerInit(0x2000, 0x10000114) < 0)
		return 0;

	if (SceShell_83D16CA8(3, shellNpOnProcessCreate, NULL) < 0)
		return 0;
	if (SceShell_83D16CA8(0, shellNpOnProcessExit, NULL) < 0) {
		SceShell_E3D3A162(3, shellNpOnProcessCreate, NULL);
		return 0;
	}
	if (SceShell_83D16CA8(6, shellNpOnProcessSuspend, NULL) < 0) {
		SceShell_E3D3A162(0, shellNpOnProcessExit, NULL);
		SceShell_E3D3A162(3, shellNpOnProcessCreate, NULL);
		return 0;
	}

	if (s_npAccountCallbackId == -1) {
		int cbId = npAccountRegisterNetCallback(shellNpOnAccountEvent, NULL);
		if (cbId >= 0) {
			s_npAccountCallbackId = cbId;
			int signedIn = 0;
			npCommonServiceIsSignedIn(&signedIn);
			s_npWasSignedIn = (signedIn != 0);

			if (SceNpManager_77DE17D4(&kShellNpSignInCallbacks) >= 0 &&
			    npUpdateClockInit() >= 0 &&
			    npOAuthInit() >= 0 &&
			    npGamePresenceInit() >= 0) {
				int errorFilter[2] = { -2130688633, -2130688511 };
				if (SceNpWebApi_E83CE7F1(errorFilter) >= 0) {
					int ctxId = SceNpWebApi_E8FBA9B8(0x2000, s_npWebApiPoolName);
					if (ctxId >= 0) {
						s_npWebApiLibCtx = ctxId;
						if (SceNpWebApi_4699EF17(commId, passphrase) >= 0 &&
						    npServiceInit() >= 0 &&
						    npIpc2ServerStart() >= 0 &&
						    sceNpPushStart() >= 0) {
							npAccountNotifyBootSignIn();
						}
					}
				}
			}
		}
	}
	return 0;
}
