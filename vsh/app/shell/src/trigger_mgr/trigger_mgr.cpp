/*
 * trigger_mgr.cpp: SceTriggerMgr core database, incoming dialog launcher,
 * built-in service registry, LSDB uninstall listener, and SceShellSvc IPMI
 * channel 0x00100000 dispatcher (0x812A2770-0x812A60EF, 0x814316B8, 0x81431784).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <kernel/processmgr.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <appmgr/appmgr.h>
#include <bXCe/bxce_internal.h>
#include <common/char_encoding.h>
#include <cxml/cxml.h>
#include <framework.h>
#include <plugin.h>
#include <resource/resource_obj.h>
#include <std/stdio.h>
#include <std/string.h>
#include "../../shell.h"
#include "../impose_plugin/src/incoming_dialog_server.h"
#include "../livespace_db/livespace_db_internal.h"
#include "../livespace_db/lsdb_keys.h"
#include "../power_manage_plugin/src/power_manage_internal.h"
#include "../../../game/game_plugin/game_plugin.h"
#include "trigger_mgr_internal.h"

using namespace paf;

#include <memory/heap_allocator.h>

extern "C" {
int sceAppMgrGetStatusById(SceUID processId, SceAppMgrAppStatus *outStatus);
int sceAppMgrGetStatusByName(const char *appName, SceAppMgrAppStatus *outStatus);
int SceLsdb_9B5ABE3E(AppInfoQClient *client, const paf::string *titleId);
}

/* Compile-time verification of persistent database and IPC wire sizes */
typedef char TriggerDbSizeCheck[sizeof(TriggerDatabase) == 0x14F60 ? 1 : -1];
typedef char TriggerPeriodicParamSizeCheck[sizeof(TriggerPeriodicTimerParam) == 80 ? 1 : -1];
typedef char TriggerTimeOfDayParamSizeCheck[sizeof(TriggerTimeOfDayTimerParam) == 88 ? 1 : -1];
typedef char TriggerDailyParamSizeCheck[sizeof(TriggerDailyTimerParam) == 80 ? 1 : -1];
typedef char TriggerAppInfoRespSizeCheck[sizeof(TriggerAppInfoResponse) == 1130 ? 1 : -1];
typedef char TriggerServiceInfoRespSizeCheck[sizeof(TriggerServiceInfoResponse) == 1538 ? 1 : -1];

/* Built-in system service metadata entry (0x81517B1C) */
struct BuiltinServiceMeta {
	const char *serviceId;
	SceUInt32 rcoTitleHash;
	const char *iconPath;
};

/* Runtime registration state for a built-in service (0x815C1A90, 12 bytes) */
struct BuiltinServiceRuntime {
	bool registered;
	SceUInt8 alignmentPad[3];
	ShellTriggerServiceCallback callback;
	void *userData;
};

/* Language ID -> LSDB STITLE_xx FNV-1a key entry */
struct LanguageTitleKeyEntry {
	SceUInt32 language;
	SceUInt32 lsdbKey;
};

/* Language ID -> RCO locale mapping entry */
struct LanguageLocaleEntry {
	SceUInt32 language;
	SceInt32 rcoLocale;
};

static const char *s_dbDirPath = "ur0:shell";
static const char *s_dbFilePath = "ur0:shell/trigger_table.bin";
static const char *s_builtinRcoPath = "vs0:vsh/shell/builtin_service_info.rco";

/* Titles purged when loading the database (0x81545B0C) */
static const char *const s_purgedTitles[3] = {
	"main",
	"NPXS10998",
	"NPXS10999"
};

/* Built-in service metadata table (0x81517B1C) */
static const BuiltinServiceMeta s_builtinServices[] = {
	{ "SHELL0001", 0x1A27A1E0u, "vs0:vsh/shell/updater_icon.png" },
	{ "SHELL0002", 0x766688D1u, "vs0:vsh/shell/updater_icon.png" },
	{ "SHELL0004", 0x02E6C9F9u, "vs0:app/NPXS10014/sce_sys/icon0.png" },
	{ "SHELL0005", 0x2629E98Fu, "vs0:vsh/shell/livearea_icon.png" },
	{ "SHELL0006", 0x9B68BA7Eu, "vs0:vsh/shell/livearea_icon.png" },
	{ NULL, 0u, NULL }
};

/* Display order of built-in services in getSystemTitleIdList (0x81545B18) */
static const char *const s_serviceListOrder[] = {
	"SHELL0001",
	"SHELL0005",
	"SHELL0002",
	"SHELL0006",
	"SHELL0003",
	"SHELL0004",
	NULL
};

/* Identity map from TriggerUtil language ID to PAF RCO locale (0x81517928) */
static const LanguageLocaleEntry s_languageToLocale[20] = {
	{  0,  0 }, {  1,  1 }, {  2,  2 }, {  3,  3 },
	{  4,  4 }, {  5,  5 }, {  6,  6 }, {  7,  7 },
	{  8,  8 }, {  9,  9 }, { 10, 10 }, { 11, 11 },
	{ 12, 12 }, { 13, 13 }, { 14, 14 }, { 15, 15 },
	{ 16, 16 }, { 17, 17 }, { 18, 18 }, { 19, 19 }
};

/* LSDB application attribute keys not defined in livespace_db_internal.h */
static const SceUInt32 kLsdbKeyBackgroundApp = 0xEA522351u;
static const SceUInt32 kLsdbKeyParentTitleId = 0xCEFB96FDu;

/* TriggerMgr incoming dialog XML version and limits */
static const SceInt32 kIncomingDialogSdkVersion = 0x03650011;
static const SceUInt32 kIncomingDialogTimeoutSec = 10u;
static const SceSize kIncomingDialogXmlMaxSize = 2048u;

/* Shared globals */
memory::HeapAllocator *g_triggerMgrHeap = NULL;
job::JobQueue *g_triggerMgrJobQueue = NULL;
thread::RWLock g_triggerMgrTableLock("SceTriggerMgrTable", 0);
thread::Mutex g_triggerMgrIpcLock("SceTriggerMgrIpcContextLock", 0);
TriggerDatabase g_triggerDb;

/* Module-private globals */
static thread::Mutex s_stateLock("SceTriggerMgrState", 0);
static TriggerMgrServiceState s_serviceState = TRIGGER_SVC_STATE_UNSTARTED;
static LsdbEventListener *s_lsdbListener = NULL;
static BuiltinServiceRuntime s_builtinRuntime[TRIGGER_MGR_MAX_SERVICES];
static TriggerIpcJobSlot s_ipcJobSlots[TRIGGER_MGR_MAX_IPC_JOBS];

int triggerMgrAssertFail(const char *tag, int errorCode)
{
	(void)tag;
	sceClibPrintf("[SceTriggerMgr] ASSERION FAILURE (0x%x)\n", errorCode);
	sceKernelDelayThread(100000);
	__builtin_trap();
	return errorCode;
}

void triggerMgrAssertFailNoCode(void)
{
	sceClibPrintf("[SceTriggerMgr] ASSERION FAILURE\n");
	sceKernelDelayThread(100000);
	__builtin_trap();
}

void *triggerMgrHeapAlloc(SceSize size)
{
	return operator new(size, static_cast<paf::memory::HeapAllocator *>(g_triggerMgrHeap));
}

void triggerMgrHeapFree(void *ptr)
{
	g_triggerMgrHeap->sce_paf_free(ptr);
}

/* Clears an active IPC job slot, canceling its job item (0x812A2770) */
int triggerMgrClearIpcSlot(TriggerIpcJobSlot *slot)
{
	if (slot->job.get() != NULL) {
		slot->job->Cancel();
		slot->job = common::SharedPtr<job::JobItem>();
	}
	slot->session = NULL;
	return 0;
}

/* Looks up an IPC job slot by session and requestId (0x812A27DC) */
TriggerIpcJobSlot *triggerMgrFindIpcSlot(IPMI::Session *session, SceUInt32 requestId)
{
	for (unsigned int slotIdx = 0; slotIdx < TRIGGER_MGR_MAX_IPC_JOBS; ++slotIdx) {
		if (s_ipcJobSlots[slotIdx].session == session &&
		    s_ipcJobSlots[slotIdx].requestId == requestId)
			return &s_ipcJobSlots[slotIdx];
	}
	return NULL;
}

/* Allocates a free IPC job slot (0x812A45DC) */
TriggerIpcJobSlot *triggerMgrAllocIpcSlot(IPMI::Session *session, SceUInt32 requestId,
					  common::SharedPtr<job::JobItem> job)
{
	for (unsigned int slotIdx = 0; slotIdx < TRIGGER_MGR_MAX_IPC_JOBS; ++slotIdx) {
		if (s_ipcJobSlots[slotIdx].session == NULL) {
			s_ipcJobSlots[slotIdx].session = session;
			s_ipcJobSlots[slotIdx].requestId = requestId;
			s_ipcJobSlots[slotIdx].job = job;
			return &s_ipcJobSlots[slotIdx];
		}
	}
	return NULL;
}

/* Completes a result-only async IPC job (0x812A280C) */
void triggerMgrCompleteSimpleAsyncJob(TriggerIpcJobBase *job, int result)
{
	g_triggerMgrIpcLock.Lock();
	if (!job->IsCanceled()) {
		int respErr = triggerMgrRespondResultOnly(job->m_session, job->m_requestId, result);
		if (respErr != 0)
			triggerMgrAssertFail(NULL, respErr);
		if (job->m_useCallback) {
			int notifyErr = shellSvcNotifyEvent(
				job->m_session, TRIGGER_MGR_NOTIFY_EVENT_FLAG);
			if (notifyErr != 0)
				triggerMgrAssertFail(NULL, notifyErr);
		}
		TriggerIpcJobSlot *slot = triggerMgrFindIpcSlot(job->m_session, job->m_requestId);
		if (slot == NULL)
			triggerMgrAssertFailNoCode();
		triggerMgrClearIpcSlot(slot);
	}
	g_triggerMgrIpcLock.Unlock();
}

namespace {

/* bXCe allocator backed by SceTriggerMgrHeap (0x812A5888, 0x812A589A) */
class TriggerXmlAllocator : public bXCe::MemAllocator {
public:
	virtual ~TriggerXmlAllocator();
	virtual void *allocate(size_t size, void *userData);
	virtual void deallocate(void *ptr, void *userData);
};

TriggerXmlAllocator::~TriggerXmlAllocator()
{
}

void *TriggerXmlAllocator::allocate(size_t size, void *userData)
{
	(void)userData;
	return g_triggerMgrHeap->sce_paf_malloc(size);
}

void TriggerXmlAllocator::deallocate(void *ptr, void *userData)
{
	(void)userData;
	g_triggerMgrHeap->sce_paf_free(ptr);
}

/* SAX document handler for sce_sys/incoming_dialog[_XX].xml (0x812A58AC-0x812A5BC8) */
class IncomingDialogXmlHandler : public bXCe::Sax::DocumentHandler {
public:
	enum ParseStage {
		STAGE_ROOT = 0,
		STAGE_INCOMING_DIALOG = 1,
		STAGE_TEXT_CONTENTS = 2
	};

	IncomingDialogXmlHandler(SceIncomingDialogParam *dialogParam);
	virtual ~IncomingDialogXmlHandler();
	virtual int startElement(void *userData, const char *name, int length,
				 const bXCe::AttributeList *attrList);
	virtual int endElement(void *userData, const char *name, int length);
	virtual void fatalError(void *userData, int errCode);

	SceIncomingDialogParam *m_dialogParam;
	int m_error;
	ParseStage m_stage;
};

IncomingDialogXmlHandler::IncomingDialogXmlHandler(SceIncomingDialogParam *dialogParam)
	: m_dialogParam(dialogParam),
	  m_error(0),
	  m_stage(STAGE_ROOT)
{
}

IncomingDialogXmlHandler::~IncomingDialogXmlHandler()
{
}

int IncomingDialogXmlHandler::startElement(void *userData, const char *name, int length,
					   const bXCe::AttributeList *attrList)
{
	(void)userData;
	if (m_stage == STAGE_ROOT) {
		if (sce_paf_strncmp(name, "IncomingDialog", (SceSize)length) == 0 &&
		    attrList->getLength() == 1) {
			const bXCe::Attr *attr = (const bXCe::Attr *)attrList->getAttribute(0);
			if (sce_paf_strncmp("version", attr->getName(),
					    (SceSize)attr->getNameLength()) == 0 &&
			    sce_paf_strncmp("1.5", attr->getValue(),
					    (SceSize)attr->getValueLength()) == 0) {
				m_stage = STAGE_INCOMING_DIALOG;
				return 0;
			}
		}
	} else if (m_stage == STAGE_INCOMING_DIALOG) {
		if (sce_paf_strncmp(name, "Ringtone", (SceSize)length) == 0 &&
		    attrList->getLength() == 1) {
			const bXCe::Attr *attr = (const bXCe::Attr *)attrList->getAttribute(0);
			if (sce_paf_strncmp("path", attr->getName(),
					    (SceSize)attr->getNameLength()) == 0) {
				sce_paf_strlcpy(m_dialogParam->audioPath, attr->getValue(),
						sizeof(m_dialogParam->audioPath));
				return 0;
			}
		}
		if (sce_paf_strncmp(name, "BypassPasscode", (SceSize)length) == 0 &&
		    attrList->getLength() == 0) {
			m_dialogParam->launchFlag = true;
			return 0;
		}
		if (sce_paf_strncmp(name, "TextContents", (SceSize)length) == 0 &&
		    attrList->getLength() == 0) {
			m_stage = STAGE_TEXT_CONTENTS;
			return 0;
		}
	} else if (m_stage == STAGE_TEXT_CONTENTS) {
		if (attrList->getLength() == 1) {
			const bXCe::Attr *attr = (const bXCe::Attr *)attrList->getAttribute(0);
			if (sce_paf_strncmp("string", attr->getName(),
					    (SceSize)attr->getNameLength()) == 0) {
				wstring wideText;
				common::Utf8ToUtf16(attr->getValue(), &wideText);
				bool matched = false;
				if (sce_paf_strncmp(name, "AcceptButton",
						    (SceSize)length) == 0) {
					sce_paf_wcsncpy(m_dialogParam->acceptText,
							wideText.c_str(), 31);
					matched = true;
				} else if (sce_paf_strncmp(name, "RejectButton",
							   (SceSize)length) == 0) {
					sce_paf_wcsncpy(m_dialogParam->rejectText,
							wideText.c_str(), 31);
					matched = true;
				} else if (sce_paf_strncmp(name, "MessageBox",
							   (SceSize)length) == 0) {
					sce_paf_wcsncpy(m_dialogParam->dialogText,
							wideText.c_str(), 63);
					matched = true;
				} else if (sce_paf_strncmp(name, "PostNotification",
							   (SceSize)length) == 0) {
					sce_paf_wcsncpy(m_dialogParam->noticeText,
							wideText.c_str(), 63);
					matched = true;
				}
				if (matched)
					return 0;
			}
		}
	}

	m_error = TRIGGER_MGR_ERROR_DIALOG_XML_INVALID;
	return -1;
}

int IncomingDialogXmlHandler::endElement(void *userData, const char *name, int length)
{
	(void)userData;
	if (m_stage == STAGE_INCOMING_DIALOG &&
	    sce_paf_strncmp("IncomingDialog", name, (SceSize)length) == 0) {
		m_stage = STAGE_ROOT;
	} else if (m_stage == STAGE_TEXT_CONTENTS &&
		   sce_paf_strncmp("TextContents", name, (SceSize)length) == 0) {
		m_stage = STAGE_INCOMING_DIALOG;
	}
	return 0;
}

void IncomingDialogXmlHandler::fatalError(void *userData, int errCode)
{
	(void)userData;
	m_error = errCode;
}

/* Opens the incoming dialog server UI and waits for user response (0x812A28D8) */
static int showIncomingDialog(const SceIncomingDialogParam *dialogParam)
{
	int dialogResult = TRIGGER_MGR_ERROR_INTERNAL;
	if (shellIncomingDialogServerInitializeSelf() == 0) {
		if (shellIncomingDialogServerOpenSelf(dialogParam) == 0) {
			bool waiting = true;
			while (waiting) {
				sceKernelPowerTick(
					SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
				sceKernelDelayThread(333000);
				switch (shellIncomingDialogServerGetStatusSelf()) {
				case 1:
				case 2:
					break;
				case 3:
					dialogResult = 0;
					waiting = false;
					break;
				case 4:
					dialogResult = TRIGGER_MGR_ERROR_DIALOG_REJECTED;
					waiting = false;
					break;
				case 5:
					dialogResult = TRIGGER_MGR_ERROR_DIALOG_CANCELED;
					waiting = false;
					break;
				case 6:
					dialogResult = TRIGGER_MGR_ERROR_DIALOG_TIMEOUT;
					waiting = false;
					break;
				default:
					waiting = false;
					break;
				}
			}
		}
		shellIncomingDialogServerFinishSelf();
	}
	return dialogResult;
}

static const char *getLocaleXmlSuffix(int locale)
{
	switch (locale) {
	case  0: return "_JP";
	case  1: return "_EN";
	case  2: return "_FR";
	case  3: return "_ES";
	case  4: return "_DE";
	case  5: return "_IT";
	case  6: return "_NL";
	case  7: return "_PT";
	case  8: return "_RU";
	case  9: return "_KO";
	case 10: return "_ZH_T";
	case 11: return "_ZH_S";
	case 12: return "_FI";
	case 13: return "_SV";
	case 14: return "_DA";
	case 15: return "_NO";
	case 16: return "_PL";
	case 17: return "_PT_BR";
	case 18: return "_EN_GB";
	case 19: return "_TR";
	default: return "";
	}
}

/* Reads and parses sce_sys/incoming_dialog[_XX].xml (0x812A2948) */
static int parseIncomingDialogXml(const char *mountPoint, SceIncomingDialogParam *dialogParam)
{
	char xmlPath[128];
	SceIoStat fileStat;
	const char *langSuffix = getLocaleXmlSuffix(Framework::Instance()->GetLocale());

	sce_paf_snprintf(xmlPath, sizeof(xmlPath), "%s/sce_sys/incoming_dialog%s.xml",
			 mountPoint, langSuffix);
	if (sceIoGetstat(xmlPath, &fileStat) != 0) {
		sce_paf_snprintf(xmlPath, sizeof(xmlPath), "%s/sce_sys/incoming_dialog.xml",
				 mountPoint);
		if (sceIoGetstat(xmlPath, &fileStat) != 0)
			return TRIGGER_MGR_ERROR_DIALOG_XML_NOT_FOUND;
	}

	SceSize fileSize = (SceSize)fileStat.st_size;
	if (fileSize > kIncomingDialogXmlMaxSize)
		return TRIGGER_MGR_ERROR_DIALOG_XML_TOO_LARGE;

	SceUID fd = sceIoOpen(xmlPath, SCE_O_RDONLY, 0);
	if (fd < 0)
		return TRIGGER_MGR_ERROR_INTERNAL;

	char xmlBuffer[kIncomingDialogXmlMaxSize];
	SceSSize bytesRead = sceIoRead(fd, xmlBuffer, fileSize);
	sceIoClose(fd);
	if (bytesRead < 0 || (SceSize)bytesRead < fileSize)
		return TRIGGER_MGR_ERROR_INTERNAL;

	TriggerXmlAllocator allocator;
	bXCe::Sax::Parser parser;
	IncomingDialogXmlHandler handler(dialogParam);
	bXCe::InitParameter initParam;
	initParam.allocator = &allocator;
	initParam.userData = NULL;

	if (parser.initialize(&initParam) != 0)
		return TRIGGER_MGR_ERROR_INTERNAL;

	parser.setDocumentHandler(&handler);
	parser.setResolveAttributeEntity(true);
	int parseRet = parser.parse(xmlBuffer, (int)fileSize, true);
	int finalErr = (parseRet == 0) ? handler.m_error : TRIGGER_MGR_ERROR_INTERNAL;
	parser.terminate();

	if (finalErr == 0 && sce_paf_strncmp(dialogParam->audioPath, "app0:", 5) == 0) {
		char resolvedPath[128];
		sce_paf_snprintf(resolvedPath, sizeof(resolvedPath), "%s%s",
				 mountPoint, dialogParam->audioPath + 5);
		sce_paf_strlcpy(dialogParam->audioPath, resolvedPath,
				sizeof(dialogParam->audioPath));
	}
	return finalErr;
}

/* Mounts an application's org_path directory via game_plugin (0x812A2BF0) */
static int mountAppDirectory(const char *titleId, char *outMountPoint, SceSize mountPointSize)
{
	LsdbAppInfoPtr appInfo(new LsdbAppInfo());
	AppInfoQClient lsdbClient;
	SceLsdb_A6D8772C(&lsdbClient);

	int mountErr = TRIGGER_MGR_ERROR_INTERNAL;
	if (appInfo.get() != NULL && SceLsdb_E90B81CD(&lsdbClient) == 0) {
		string titleKey(titleId != NULL ? titleId : "");
		if (SceLsdb_D239031A(&lsdbClient, &titleKey, &appInfo) != 0) {
			mountErr = TRIGGER_MGR_ERROR_APP_NOT_FOUND;
		} else {
			LsdbAppInfo::iterator orgPathIt = appInfo->find(LSDB_KEY_ORG_PATH);
			if (orgPathIt == appInfo->end()) {
				mountErr = TRIGGER_MGR_ERROR_APP_NOT_FOUND;
			} else {
				Plugin *gamePlugin = Plugin::Find("game_plugin");
				if (gamePlugin != NULL) {
					GamePluginInterface *gameIface =
						(GamePluginInterface *)gamePlugin->GetInterface(1);
					if (gameIface != NULL) {
						const char *orgPath = "";
						if (orgPathIt->second.type == LSDB_VALUE_TEXT &&
						    orgPathIt->second.text != NULL)
							orgPath = orgPathIt->second.text->c_str();
						typedef int (*MountFn)(const char *, char *, SceSize);
						MountFn mountFn =
							(MountFn)gameIface->func[GAME_PLUGIN_FUNC_APP_DIR_MOUNT];
						mountErr = mountFn(orgPath, outMountPoint,
								   mountPointSize);
					}
				}
			}
		}
	}

	SceLsdb_E2101B03(&lsdbClient);
	return mountErr;
}

static void unmountAppDirectory(const char *mountPoint)
{
	Plugin *gamePlugin = Plugin::Find("game_plugin");
	if (gamePlugin != NULL) {
		GamePluginInterface *gameIface =
			(GamePluginInterface *)gamePlugin->GetInterface(1);
		if (gameIface != NULL) {
			typedef int (*UnmountFn)(const char *);
			UnmountFn unmountFn =
				(UnmountFn)gameIface->func[GAME_PLUGIN_FUNC_APP_DIR_UNMOUNT];
			unmountFn(mountPoint);
		}
	}
}

/* Shows the incoming dialog for a foreground application trigger (0x812A2FF6) */
static int showAppIncomingDialog(const char *titleId)
{
	char mountPoint[128];
	int mountErr = mountAppDirectory(titleId, mountPoint, sizeof(mountPoint));
	if (mountErr != 0)
		return mountErr;

	SceIncomingDialogParam dialogParam;
	sce_paf_memset(&dialogParam, 0, sizeof(dialogParam));
	dialogParam.sdkVersion = kIncomingDialogSdkVersion;
	dialogParam.timeout = kIncomingDialogTimeoutSec;
	sce_paf_strlcpy(dialogParam.titleId, titleId, sizeof(dialogParam.titleId));

	int result = parseIncomingDialogXml(mountPoint, &dialogParam);
	if (result == 0)
		result = showIncomingDialog(&dialogParam);

	unmountAppDirectory(mountPoint);
	return result;
}

/* Kernel callback signaled when a launched background app reports status (0x812A349A) */
static int onAppLaunchCallback(int notifyId, int notifyCount, int notifyArg, void *common)
{
	(void)notifyId;
	(void)notifyArg;
	TriggerFireJobBase *fireJob = (TriggerFireJobBase *)common;
	fireJob->m_launchResult = notifyCount;
	fireJob->m_launchCallbackDone = true;
	return 0;
}

/* Launches a background-capable application via sceAppMgrLaunchAppByName2 (0x812A30B0) */
static int launchBackgroundApp(TriggerFireJobBase *fireJob, const char *titleId,
			       const char *launchArg, bool waitLaunchResult)
{
	AppmgrLaunchAppByNameOption optParam;
	sce_paf_memset(&optParam, 0, sizeof(optParam));
	optParam.size = sizeof(optParam);
	optParam.launchCallbackId = -1;
	optParam.evictionThresholdClass = 2;

	SceUID callbackId = -1;
	if (waitLaunchResult) {
		callbackId = sceKernelCreateCallback(
			"SceTriggerMgrAppLaunch", 0,
			(SceKernelCallbackFunction)onAppLaunchCallback, fireJob);
		if (callbackId <= 0)
			return TRIGGER_MGR_ERROR_INTERNAL;
		optParam.launchCallbackId = callbackId;
		optParam.launchMode |= 0x00200000u;
	}

	int launchErr = sceAppMgrLaunchAppByName2(titleId, launchArg, &optParam);
	if (launchErr != 0) {
		sceClibPrintf(
			"[SceTriggerMgr] *UNEXPECTED ERROR* sceAppMgrLaunchAppByName2() failed. 0x%x\n",
			launchErr);
		launchErr = TRIGGER_MGR_ERROR_INTERNAL;
	} else if (callbackId > 0) {
		while (!fireJob->m_launchCallbackDone) {
			sceKernelDelayThread(1000000);
			sceKernelCheckCallback();
		}
		launchErr = fireJob->m_launchResult;
		if (launchErr == -2147333119)
			launchErr = TRIGGER_MGR_ERROR_NO_MEMORY;
	}

	if (callbackId > 0)
		sceKernelDeleteCallback(callbackId);
	return launchErr;
}

/* Checks whether a title supports background launch in LSDB (0x812A319C) */
static bool isBackgroundCapableApp(const char *titleId)
{
	AppInfoQClient lsdbClient;
	SceLsdb_A6D8772C(&lsdbClient);
	LsdbAppInfoPtr appInfo(new LsdbAppInfo());

	SceLsdb_E90B81CD(&lsdbClient);
	string titleKey(titleId != NULL ? titleId : "");
	bool found = (SceLsdb_D239031A(&lsdbClient, &titleKey, &appInfo) == 0);
	bool isBgCapable = false;

	if (found) {
		LsdbValue &attrVal = (*appInfo)[kLsdbKeyBackgroundApp];
		if (attrVal.type == LSDB_VALUE_INTEGER && (attrVal.integer & 1) != 0)
			isBgCapable = true;
	}

	SceLsdb_E2101B03(&lsdbClient);
	return isBgCapable;
}

/* Maps language ID to LSDB localized STITLE_xx hash (0x812A3A72) */
static SceUInt32 getLocalizedStitleKey(SceUInt32 language)
{
	static const LanguageTitleKeyEntry s_stitleKeys[20] = {
		{  0, 0x11F066E9u }, {  1, 0x10F06556u }, {  2, 0x0FF063C3u }, {  3, 0x0EF06230u },
		{  4, 0x15F06D35u }, {  5, 0x14F06BA2u }, {  6, 0x13F06A0Fu }, {  7, 0x12F0687Cu },
		{  8, 0x19F07381u }, {  9, 0x18F071EEu }, { 10, 0x0BEE3AE0u }, { 11, 0x0CEE3C73u },
		{ 12, 0x0DEE3E06u }, { 13, 0x0EEE3F99u }, { 14, 0x0FEE412Cu }, { 15, 0x10EE42BFu },
		{ 16, 0x11EE4452u }, { 17, 0x12EE45E5u }, { 18, 0x13EE4778u }, { 19, 0x14EE490Bu }
	};

	for (unsigned int idx = 0; idx < 20; ++idx) {
		if (s_stitleKeys[idx].language == language)
			return s_stitleKeys[idx].lsdbKey;
	}
	return LSDB_KEY_STITLE;
}

/* Looks up a built-in service's DB record and runtime entry (0x812A41F2) */
static int findBuiltinService(const char *serviceId, TriggerServiceRecord **outRecord,
			      BuiltinServiceRuntime **outRuntime)
{
	int metaIdx = -1;
	for (int idx = 0; s_builtinServices[idx].serviceId != NULL; ++idx) {
		if (sce_paf_strncmp(s_builtinServices[idx].serviceId, serviceId,
				    TRIGGER_MGR_TITLE_ID_LEN) == 0) {
			metaIdx = idx;
			break;
		}
	}
	if (metaIdx < 0)
		return TRIGGER_MGR_ERROR_SERVICE_NOT_FOUND;

	TriggerServiceRecord *record = NULL;
	for (int dbIdx = 0; dbIdx < TRIGGER_MGR_MAX_SERVICES; ++dbIdx) {
		if (g_triggerDb.services[dbIdx].serviceId[0] != '\0' &&
		    sce_paf_strncmp(serviceId, g_triggerDb.services[dbIdx].serviceId,
				    TRIGGER_MGR_TITLE_ID_LEN) == 0) {
			record = &g_triggerDb.services[dbIdx];
			break;
		}
	}
	if (record == NULL)
		triggerMgrAssertFailNoCode();

	*outRecord = record;
	*outRuntime = &s_builtinRuntime[metaIdx];
	return 0;
}

/* Ensures all built-in service IDs exist in g_triggerDb.services (0x812A4400) */
static int ensureBuiltinServicesInDb(void)
{
	for (int metaIdx = 0; s_builtinServices[metaIdx].serviceId != NULL; ++metaIdx) {
		const char *serviceId = s_builtinServices[metaIdx].serviceId;
		TriggerServiceRecord *existing = NULL;
		for (int dbIdx = 0; dbIdx < TRIGGER_MGR_MAX_SERVICES; ++dbIdx) {
			if (g_triggerDb.services[dbIdx].serviceId[0] != '\0' &&
			    sce_paf_strncmp(serviceId, g_triggerDb.services[dbIdx].serviceId,
					    TRIGGER_MGR_TITLE_ID_LEN) == 0) {
				existing = &g_triggerDb.services[dbIdx];
				break;
			}
		}
		if (existing != NULL)
			continue;

		TriggerServiceRecord *freeSlot = NULL;
		for (int dbIdx = 0; dbIdx < TRIGGER_MGR_MAX_SERVICES; ++dbIdx) {
			if (g_triggerDb.services[dbIdx].serviceId[0] == '\0') {
				freeSlot = &g_triggerDb.services[dbIdx];
				break;
			}
		}
		if (freeSlot == NULL)
			triggerMgrAssertFailNoCode();

		sce_paf_memset(freeSlot, 0, sizeof(*freeSlot));
		sce_paf_strncpy(freeSlot->serviceId, serviceId, TRIGGER_MGR_TITLE_ID_LEN);
		if (sce_paf_strncmp(serviceId, "SHELL0006", TRIGGER_MGR_TITLE_ID_LEN) != 0)
			freeSlot->flags |= TRIGGER_APP_FLAG_ENABLED;
	}
	return 0;
}

/* Loads and validates ur0:shell/trigger_table.bin (0x812A44C4) */
static int loadDatabase(void)
{
	for (;;) {
		SceUID fd = sceIoOpen(s_dbFilePath, SCE_O_RDONLY | 0x00010000, 0);
		if (fd < 0)
			return fd;

		SceSSize bytesRead = sceIoRead(fd, &g_triggerDb, sizeof(g_triggerDb));
		bool retryRead = false;
		if (bytesRead >= 0) {
			if ((SceSize)bytesRead != sizeof(g_triggerDb)) {
				sceIoClose(fd);
				return TRIGGER_MGR_ERROR_DB_CORRUPT;
			}
		} else {
			retryRead = (bytesRead == -2147418093);
		}
		sceIoClose(fd);

		if (retryRead)
			continue;

		const SceUInt32 *words = (const SceUInt32 *)&g_triggerDb.version;
		SceUInt32 wordCount = (sizeof(g_triggerDb) / sizeof(SceUInt32)) - 1u;
		SceUInt32 computedSum = 0;
		for (SceUInt32 idx = 0; idx < wordCount; ++idx)
			computedSum += words[idx];

		if (g_triggerDb.checksum != computedSum)
			return TRIGGER_MGR_ERROR_DB_CORRUPT;

		for (int appIdx = 0; appIdx < TRIGGER_MGR_MAX_APPS; ++appIdx) {
			if (g_triggerDb.apps[appIdx].titleId[0] == '\0')
				continue;
			for (unsigned int purgeIdx = 0; purgeIdx < 3; ++purgeIdx) {
				if (sce_paf_strncmp(s_purgedTitles[purgeIdx],
						    g_triggerDb.apps[appIdx].titleId,
						    TRIGGER_MGR_TITLE_ID_LEN) == 0) {
					sce_paf_memset(&g_triggerDb.apps[appIdx], 0,
						       sizeof(TriggerAppRecord));
					break;
				}
			}
		}

		return ensureBuiltinServicesInDb();
	}
}

/* Job to query trigger policy for the calling application (0x812A356E, 0x812A38DA) */
class GetPolicyJob : public TriggerIpcJobBase {
public:
	GetPolicyJob(IPMI::Session *session, SceUInt32 requestId, bool useCallback);
	virtual ~GetPolicyJob();
	void Complete(int result, SceUInt8 policy);
	virtual int Run();
};

GetPolicyJob::GetPolicyJob(IPMI::Session *session, SceUInt32 requestId, bool useCallback)
	: TriggerIpcJobBase("SceTriggerMgrJob", session, requestId, useCallback)
{
}

GetPolicyJob::~GetPolicyJob()
{
}

void GetPolicyJob::Complete(int result, SceUInt8 policy)
{
	g_triggerMgrIpcLock.Lock();
	if (!IsCanceled()) {
		int respErr = triggerMgrRespondPolicy(m_session, m_requestId, result, &policy);
		if (respErr != 0)
			triggerMgrAssertFail(NULL, respErr);
		if (m_useCallback) {
			int notifyErr = shellSvcNotifyEvent(
				m_session, TRIGGER_MGR_NOTIFY_EVENT_FLAG);
			if (notifyErr != 0)
				triggerMgrAssertFail(NULL, notifyErr);
		}
		TriggerIpcJobSlot *slot = triggerMgrFindIpcSlot(m_session, m_requestId);
		if (slot == NULL)
			triggerMgrAssertFailNoCode();
		triggerMgrClearIpcSlot(slot);
	}
	g_triggerMgrIpcLock.Unlock();
}

int GetPolicyJob::Run()
{
	char callerTitleId[12];
	SceUID clientPid = m_session->getProcessId();
	int resolveErr = triggerMgrResolveProcessTitleId(clientPid, callerTitleId);
	if (resolveErr != 0) {
		Complete(resolveErr, 0);
		return 0;
	}

	g_triggerMgrTableLock.ReaderLock();
	int result = TRIGGER_MGR_ERROR_TIMER_NOT_FOUND;
	SceUInt8 policy = 0;
	int appIdx = triggerMgrFindAppIndex(callerTitleId);
	if (appIdx >= 0) {
		result = 0;
		policy = ((g_triggerDb.apps[appIdx].flags & TRIGGER_APP_FLAG_ENABLED) != 0)
				 ? 1u
				 : 0u;
	}
	Complete(result, policy);
	g_triggerMgrTableLock.ReaderUnlock();
	return 0;
}

/* Job to fetch localized title and icon path for an app (0x812A39A0, 0x812A3BAA, 0x812A3EA8) */
class GetAppInfoJob : public TriggerIpcJobBase {
public:
	GetAppInfoJob(IPMI::Session *session, SceUInt32 requestId,
		      const char *titleId, SceUInt32 language, bool useCallback);
	virtual ~GetAppInfoJob();
	void Complete(int result, const TriggerAppInfoResponse *appInfo);
	int FetchFromLsdb(TriggerAppInfoResponse *outInfo);
	virtual int Run();

	char m_titleId[12];
	SceUInt32 m_language;
};

GetAppInfoJob::GetAppInfoJob(IPMI::Session *session, SceUInt32 requestId,
			     const char *titleId, SceUInt32 language, bool useCallback)
	: TriggerIpcJobBase("SceTriggerMgrGetAppInfoJob", session, requestId, useCallback),
	  m_language(language)
{
	sce_paf_strncpy(m_titleId, titleId, TRIGGER_MGR_TITLE_ID_LEN);
	m_titleId[TRIGGER_MGR_TITLE_ID_LEN] = '\0';
}

GetAppInfoJob::~GetAppInfoJob()
{
}

void GetAppInfoJob::Complete(int result, const TriggerAppInfoResponse *appInfo)
{
	g_triggerMgrIpcLock.Lock();
	if (!IsCanceled()) {
		int respErr = triggerMgrRespondAppInfo(m_session, m_requestId, result, appInfo);
		if (respErr != 0)
			triggerMgrAssertFail(NULL, respErr);
		if (m_useCallback) {
			int notifyErr = shellSvcNotifyEvent(
				m_session, TRIGGER_MGR_NOTIFY_EVENT_FLAG);
			if (notifyErr != 0)
				triggerMgrAssertFail(NULL, notifyErr);
		}
		TriggerIpcJobSlot *slot = triggerMgrFindIpcSlot(m_session, m_requestId);
		if (slot == NULL)
			triggerMgrAssertFailNoCode();
		triggerMgrClearIpcSlot(slot);
	}
	g_triggerMgrIpcLock.Unlock();
}

int GetAppInfoJob::FetchFromLsdb(TriggerAppInfoResponse *outInfo)
{
	AppInfoQClient lsdbClient;
	SceLsdb_A6D8772C(&lsdbClient);
	LsdbAppInfoPtr appInfo(new LsdbAppInfo());
	LsdbIconPtr iconInfo(new LsdbIcon());

	SceLsdb_E90B81CD(&lsdbClient);
	string titleKey(m_titleId);
	int infoErr = SceLsdb_D239031A(&lsdbClient, &titleKey, &appInfo);
	int iconErr = SceLsdb_7AF9008C(&lsdbClient, &titleKey, &iconInfo, false);

	int result;
	if ((infoErr | iconErr) != 0) {
		result = TRIGGER_MGR_ERROR_APP_NOT_FOUND;
	} else {
		SceUInt32 localizedKey = getLocalizedStitleKey(m_language);
		LsdbValue &localizedVal = (*appInfo)[localizedKey];
		const char *titleUtf8 = "";
		if (localizedVal.type == LSDB_VALUE_TEXT && localizedVal.text != NULL)
			titleUtf8 = localizedVal.text->c_str();

		wstring wideTitle;
		common::Utf8ToUtf16(titleUtf8, &wideTitle);
		if (wideTitle.size() == 0) {
			LsdbValue &fallbackVal = (*appInfo)[LSDB_KEY_STITLE];
			const char *fallbackUtf8 = "";
			if (fallbackVal.type == LSDB_VALUE_TEXT && fallbackVal.text != NULL)
				fallbackUtf8 = fallbackVal.text->c_str();
			common::Utf8ToUtf16(fallbackUtf8, &wideTitle);
		}

		sce_paf_wcsncpy(outInfo->title, wideTitle.c_str(), 52);
		sce_paf_strncpy(outInfo->iconPath, iconInfo->iconPath.c_str(),
				sizeof(outInfo->iconPath));
		result = 0;
	}

	SceLsdb_DEF5C3F9(&lsdbClient);
	SceLsdb_E2101B03(&lsdbClient);
	return result;
}

int GetAppInfoJob::Run()
{
	TriggerAppInfoResponse response;
	sce_paf_memset(&response, 0, sizeof(response));

	g_triggerMgrTableLock.ReaderLock();
	int appIdx = triggerMgrFindAppIndex(m_titleId);
	if (appIdx < 0) {
		Complete(TRIGGER_MGR_ERROR_APP_NOT_FOUND, &response);
		g_triggerMgrTableLock.ReaderUnlock();
		return 0;
	}
	bool enabled = (g_triggerDb.apps[appIdx].flags & TRIGGER_APP_FLAG_ENABLED) != 0;
	g_triggerMgrTableLock.ReaderUnlock();

	int result = 0;
	if (sce_paf_strncmp(m_titleId, "NPXS10999", TRIGGER_MGR_TITLE_ID_LEN) == 0) {
		sce_paf_wcsncpy(response.title, L"DEBUGGER", 52);
	} else if (sce_paf_strncmp(m_titleId, "NPXS10998", TRIGGER_MGR_TITLE_ID_LEN) == 0) {
		sce_paf_wcsncpy(response.title, L"APP_HOME", 52);
	} else {
		result = FetchFromLsdb(&response);
	}

	if (result == 0)
		response.enabled = enabled;

	Complete(result, &response);
	return 0;
}

/* Job to fetch localized name and icon path for a built-in service (0x812A3FEA-0x812A429C) */
class GetServiceInfoJob : public TriggerIpcJobBase {
public:
	GetServiceInfoJob(IPMI::Session *session, SceUInt32 requestId,
			  const char *serviceId, SceUInt32 language, bool useCallback);
	virtual ~GetServiceInfoJob();
	void Complete(int result, const TriggerServiceInfoResponse *serviceInfo);
	int LoadFromRco(TriggerServiceInfoResponse *outInfo);
	virtual int Run();

	char m_serviceId[12];
	SceUInt32 m_language;
};

GetServiceInfoJob::GetServiceInfoJob(IPMI::Session *session, SceUInt32 requestId,
				     const char *serviceId, SceUInt32 language, bool useCallback)
	: TriggerIpcJobBase("SceTriggerMgrGetServiceInfoJob", session, requestId, useCallback),
	  m_language(language)
{
	sce_paf_strncpy(m_serviceId, serviceId, TRIGGER_MGR_TITLE_ID_LEN);
	m_serviceId[TRIGGER_MGR_TITLE_ID_LEN] = '\0';
}

GetServiceInfoJob::~GetServiceInfoJob()
{
}

void GetServiceInfoJob::Complete(int result, const TriggerServiceInfoResponse *serviceInfo)
{
	g_triggerMgrIpcLock.Lock();
	if (!IsCanceled()) {
		int respErr = triggerMgrRespondServiceInfo(
			m_session, m_requestId, result, serviceInfo);
		if (respErr != 0)
			triggerMgrAssertFail(NULL, respErr);
		if (m_useCallback) {
			int notifyErr = shellSvcNotifyEvent(
				m_session, TRIGGER_MGR_NOTIFY_EVENT_FLAG);
			if (notifyErr != 0)
				triggerMgrAssertFail(NULL, notifyErr);
		}
		TriggerIpcJobSlot *slot = triggerMgrFindIpcSlot(m_session, m_requestId);
		if (slot == NULL)
			triggerMgrAssertFailNoCode();
		triggerMgrClearIpcSlot(slot);
	}
	g_triggerMgrIpcLock.Unlock();
}

int GetServiceInfoJob::LoadFromRco(TriggerServiceInfoResponse *outInfo)
{
	resource::ResourceObj::Option rcoOpt;
	rcoOpt.load_type = 0;
	rcoOpt.allocator = g_triggerMgrHeap;
	rcoOpt.cxml_allocator_func = cxml::DefaultAllocator;

	void *rcoMem = triggerMgrHeapAlloc(sizeof(resource::ResourceObj));
	if (rcoMem == NULL)
		return TRIGGER_MGR_ERROR_NO_MEMORY;

	resource::ResourceObj *rco = new (rcoMem) resource::ResourceObj(s_builtinRcoPath, &rcoOpt);

	const BuiltinServiceMeta *meta = NULL;
	for (int idx = 0; s_builtinServices[idx].serviceId != NULL; ++idx) {
		if (sce_paf_strncmp(s_builtinServices[idx].serviceId, m_serviceId,
				    TRIGGER_MGR_TITLE_ID_LEN) == 0) {
			meta = &s_builtinServices[idx];
			break;
		}
	}

	SceInt32 rcoLocale = -1;
	for (unsigned int idx = 0; idx < 20; ++idx) {
		if (s_languageToLocale[idx].language == m_language) {
			rcoLocale = s_languageToLocale[idx].rcoLocale;
			break;
		}
	}

	ScePafResource_42E4E566(rco, rcoLocale, g_triggerMgrHeap);

	const wchar_t *nameStr = NULL;
	uint32_t nameLen = 0;
	if (meta != NULL) {
		rco->GetString(meta->rcoTitleHash, &nameStr, &nameLen);
		if (nameStr != NULL) {
			sce_paf_wcsncpy(outInfo->name, nameStr, 256);
			sce_paf_strncpy(outInfo->iconPath, meta->iconPath,
					sizeof(outInfo->iconPath));
		}
	}

	rco->~ResourceObj();
	triggerMgrHeapFree(rco);
	return 0;
}

int GetServiceInfoJob::Run()
{
	TriggerServiceInfoResponse response;
	sce_paf_memset(&response, 0, sizeof(response));

	g_triggerMgrTableLock.ReaderLock();
	TriggerServiceRecord *record = NULL;
	BuiltinServiceRuntime *runtime = NULL;
	if (findBuiltinService(m_serviceId, &record, &runtime) != 0) {
		Complete(TRIGGER_MGR_ERROR_SERVICE_NOT_FOUND, &response);
		g_triggerMgrTableLock.ReaderUnlock();
		return 0;
	}
	bool enabled = (record->flags & TRIGGER_APP_FLAG_ENABLED) != 0;
	g_triggerMgrTableLock.ReaderUnlock();

	int result = LoadFromRco(&response);
	if (result == 0)
		response.enabled = enabled;

	Complete(result, &response);
	return 0;
}

/* Job to remove all timers for an uninstalled application (0x812A434E) */
class AppUninstallJob : public job::JobItem {
public:
	AppUninstallJob(const char *titleId);
	virtual ~AppUninstallJob();
	virtual void Finish(int result);
	virtual int Run();

	char m_titleId[12];
};

AppUninstallJob::AppUninstallJob(const char *titleId)
	: job::JobItem("SceTriggerMgrAppUninstallJob", NULL)
{
	sce_paf_strncpy(m_titleId, titleId, TRIGGER_MGR_TITLE_ID_LEN);
	m_titleId[TRIGGER_MGR_TITLE_ID_LEN] = '\0';
}

AppUninstallJob::~AppUninstallJob()
{
}

void AppUninstallJob::Finish(int result)
{
	(void)result;
}

int AppUninstallJob::Run()
{
	g_triggerMgrTableLock.WriterLock();
	int appIdx = triggerMgrFindAppIndex(m_titleId);
	if (appIdx >= 0) {
		triggerPeriodicTimerCancelAllForApp(appIdx);
		triggerTimeOfDayTimerCancelAllForApp(appIdx);
		sce_paf_memset(&g_triggerDb.apps[appIdx], 0, sizeof(TriggerAppRecord));
		g_triggerMgrTableLock.WriterUnlock();

		g_triggerMgrTableLock.ReaderLock();
		triggerMgrSaveDatabase();
		g_triggerMgrTableLock.ReaderUnlock();
		return 0;
	}
	g_triggerMgrTableLock.WriterUnlock();
	return 0;
}

/* Job to start/resume or suspend the trigger manager service (0x814316B8) */
class SvcCtrlJob : public job::JobItem {
public:
	typedef void (*SvcCtrlResultCallback)(int result, void *userData);

	SvcCtrlJob(int command, SvcCtrlResultCallback callback, void *userData);
	virtual ~SvcCtrlJob();
	virtual void Finish(int result);
	virtual int Run();

	int m_command;
	SvcCtrlResultCallback m_callback;
	void *m_userData;
};

SvcCtrlJob::SvcCtrlJob(int command, SvcCtrlResultCallback callback, void *userData)
	: job::JobItem("SceTriggerMgrSvcCtrlJob", NULL),
	  m_command(command),
	  m_callback(callback),
	  m_userData(userData)
{
}

SvcCtrlJob::~SvcCtrlJob()
{
}

void SvcCtrlJob::Finish(int result)
{
	(void)result;
}

int SvcCtrlJob::Run()
{
	int result = TRIGGER_MGR_ERROR_INTERNAL;
	bool firstStart = false;

	s_stateLock.Lock();
	if (m_command == 0) {
		firstStart = (s_serviceState == TRIGGER_SVC_STATE_UNSTARTED);
		s_serviceState = TRIGGER_SVC_STATE_RUNNING;
		result = 0;
	} else if (m_command == 1) {
		s_serviceState = TRIGGER_SVC_STATE_SUSPENDED;
		result = 0;
	}
	s_stateLock.Unlock();

	if (firstStart) {
		g_triggerMgrTableLock.ReaderLock();
		triggerPeriodicTimerRegisterAll();
		triggerTimeOfDayTimerRegisterAll();
		g_triggerMgrTableLock.ReaderUnlock();
	}

	if (m_callback != NULL)
		m_callback(result, m_userData);
	return 0;
}

/* Job to persist the database asynchronously after a toggle (0x81431784) */
class SaveDatabaseJob : public job::JobItem {
public:
	SaveDatabaseJob();
	virtual ~SaveDatabaseJob();
	virtual void Finish(int result);
	virtual int Run();
};

SaveDatabaseJob::SaveDatabaseJob()
	: job::JobItem("SceTriggerMgrSavaDatabaseJob", NULL)
{
}

SaveDatabaseJob::~SaveDatabaseJob()
{
}

void SaveDatabaseJob::Finish(int result)
{
	(void)result;
}

int SaveDatabaseJob::Run()
{
	g_triggerMgrTableLock.ReaderLock();
	triggerMgrSaveDatabase();
	g_triggerMgrTableLock.ReaderUnlock();
	return 0;
}

/* Enqueues an AppUninstallJob for the given title ID (0x812A471C) */
static void enqueueAppUninstallJob(const char *titleId)
{
	void *mem = triggerMgrHeapAlloc(sizeof(AppUninstallJob));
	AppUninstallJob *job = NULL;
	if (mem != NULL)
		job = new (mem) AppUninstallJob(titleId);
	common::SharedPtr<job::JobItem> jobRef(job);
	if (job != NULL && jobRef.get() != NULL)
		g_triggerMgrJobQueue->Enqueue(jobRef);
}

/* LSDB event callback invoked when an application is uninstalled (0x812A481A) */
static void onLsdbAppEvent(int eventType, void *eventData, void *userArg)
{
	(void)userArg;
	if (eventType != 1)
		return;

	LsdbAppInfoPtr *appInfoPtr = (LsdbAppInfoPtr *)eventData;
	if (appInfoPtr == NULL || appInfoPtr->get() == NULL)
		return;

	LsdbAppInfo::iterator it = (*appInfoPtr)->find(LSDB_KEY_TITLE_ID);
	const char *titleId = "";
	if (it != (*appInfoPtr)->end() && it->second.type == LSDB_VALUE_TEXT &&
	    it->second.text != NULL)
		titleId = it->second.text->c_str();
	enqueueAppUninstallJob(titleId);
}

/* Enables or disables triggers for a service or application (0x812A4D10) */
static int setTitleTriggerEnabled(const char *titleId, bool enabled)
{
	g_triggerMgrTableLock.WriterLock();
	TriggerServiceRecord *svcRecord = NULL;
	BuiltinServiceRuntime *svcRuntime = NULL;
	if (findBuiltinService(titleId, &svcRecord, &svcRuntime) == 0) {
		if (enabled)
			svcRecord->flags |= TRIGGER_APP_FLAG_ENABLED;
		else
			svcRecord->flags &= ~TRIGGER_APP_FLAG_ENABLED;
		g_triggerMgrTableLock.WriterUnlock();

		g_triggerMgrTableLock.ReaderLock();
		if (findBuiltinService(titleId, &svcRecord, &svcRuntime) == 0) {
			if (svcRuntime->callback != NULL)
				svcRuntime->callback(enabled, svcRuntime->userData);
		}
		g_triggerMgrTableLock.ReaderUnlock();
	} else {
		int appIdx = triggerMgrFindAppIndex(titleId);
		if (appIdx < 0) {
			g_triggerMgrTableLock.WriterUnlock();
			return TRIGGER_MGR_ERROR_APP_NOT_FOUND;
		}
		TriggerAppRecord *appRecord = triggerMgrGetAppRecord(appIdx);
		if (enabled)
			appRecord->flags |= TRIGGER_APP_FLAG_ENABLED;
		else
			appRecord->flags &= ~TRIGGER_APP_FLAG_ENABLED;

		int err = triggerPeriodicTimerSetWakeUpForApp(appIdx, enabled);
		if (err != 0)
			triggerMgrAssertFail(NULL, err);
		err = triggerTimeOfDayTimerSetWakeUpForApp(appIdx, enabled);
		if (err != 0)
			triggerMgrAssertFail(NULL, err);
		g_triggerMgrTableLock.WriterUnlock();
	}

	void *mem = triggerMgrHeapAlloc(sizeof(SaveDatabaseJob));
	SaveDatabaseJob *saveJob = NULL;
	if (mem != NULL)
		saveJob = new (mem) SaveDatabaseJob();
	common::SharedPtr<job::JobItem> jobRef(saveJob);
	if (saveJob != NULL && jobRef.get() != NULL)
		g_triggerMgrJobQueue->Enqueue(jobRef);

	return 0;
}

/* Lists registered application title IDs sorted by registration order (0x812A5100) */
static int getAppTitleIdList(char *outTitleIdList, SceUInt32 *outCount)
{
	g_triggerMgrTableLock.ReaderLock();
	sce_paf_memset(outTitleIdList, 0, 4);

	TriggerAppRecord *sortedRecords[TRIGGER_MGR_MAX_APPS];
	SceUInt32 count = 0;
	for (int appIdx = 0; appIdx < TRIGGER_MGR_MAX_APPS; ++appIdx) {
		if (g_triggerDb.apps[appIdx].titleId[0] != '\0')
			sortedRecords[count++] = &g_triggerDb.apps[appIdx];
	}

	if (count > 1) {
		for (SceUInt32 outer = 0; outer < count - 1; ++outer) {
			for (SceUInt32 inner = 0; inner < count - 1 - outer; ++inner) {
				if (sortedRecords[inner]->registrationOrder >
				    sortedRecords[inner + 1]->registrationOrder) {
					TriggerAppRecord *swapRec = sortedRecords[inner];
					sortedRecords[inner] = sortedRecords[inner + 1];
					sortedRecords[inner + 1] = swapRec;
				}
			}
		}
	}

	char *cursor = outTitleIdList;
	for (SceUInt32 idx = 0; idx < count; ++idx) {
		sce_paf_strncpy(cursor, sortedRecords[idx]->titleId, TRIGGER_MGR_TITLE_ID_LEN);
		cursor += TRIGGER_MGR_TITLE_ID_LEN;
	}

	*outCount = count;
	g_triggerMgrTableLock.ReaderUnlock();
	return 0;
}

/* Lists active built-in system service IDs in display order (0x812A558C) */
static int getSystemTitleIdList(char *outServiceIdList, SceUInt32 *outCount)
{
	g_triggerMgrTableLock.ReaderLock();
	sce_paf_memset(outServiceIdList, 0, 4);

	SceUInt32 count = 0;
	char *cursor = outServiceIdList;
	for (int orderIdx = 0; s_serviceListOrder[orderIdx] != NULL; ++orderIdx) {
		TriggerServiceRecord *record = NULL;
		BuiltinServiceRuntime *runtime = NULL;
		if (findBuiltinService(s_serviceListOrder[orderIdx], &record, &runtime) == 0 &&
		    record->serviceId[0] != '\0' && runtime->registered) {
			sce_paf_strncpy(cursor, record->serviceId, TRIGGER_MGR_TITLE_ID_LEN);
			cursor += TRIGGER_MGR_TITLE_ID_LEN;
			++count;
		}
	}

	*outCount = count;
	g_triggerMgrTableLock.ReaderUnlock();
	return 0;
}

static int enqueueCoreIpcJob(IPMI::Session *session, SceUInt32 requestId, job::JobItem *rawJob)
{
	common::SharedPtr<job::JobItem> jobRef(rawJob);
	if (rawJob == NULL || jobRef.get() == NULL)
		return TRIGGER_MGR_ERROR_NO_MEMORY;

	g_triggerMgrIpcLock.Lock();
	TriggerIpcJobSlot *slot = triggerMgrAllocIpcSlot(session, requestId, jobRef);
	g_triggerMgrIpcLock.Unlock();

	if (slot == NULL)
		return TRIGGER_MGR_ERROR_BUSY;

	g_triggerMgrJobQueue->Enqueue(jobRef);
	return 0;
}

/* Async job enqueue helpers for GetPolicy, GetAppInfo, GetServiceInfo (0x812A51F4-0x812A5636) */
static int enqueueGetPolicyJob(IPMI::Session *session, SceUInt32 requestId, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(GetPolicyJob));
	GetPolicyJob *job = NULL;
	if (mem != NULL)
		job = new (mem) GetPolicyJob(session, requestId, useCallback);
	return enqueueCoreIpcJob(session, requestId, job);
}

static int enqueueGetAppInfoJob(IPMI::Session *session, SceUInt32 requestId,
				const char *titleId, SceUInt32 language, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(GetAppInfoJob));
	GetAppInfoJob *job = NULL;
	if (mem != NULL)
		job = new (mem) GetAppInfoJob(session, requestId, titleId, language, useCallback);
	return enqueueCoreIpcJob(session, requestId, job);
}

static int enqueueGetServiceInfoJob(IPMI::Session *session, SceUInt32 requestId,
				    const char *serviceId, SceUInt32 language, bool useCallback)
{
	void *mem = triggerMgrHeapAlloc(sizeof(GetServiceInfoJob));
	GetServiceInfoJob *job = NULL;
	if (mem != NULL)
		job = new (mem) GetServiceInfoJob(
			session, requestId, serviceId, language, useCallback);
	return enqueueCoreIpcJob(session, requestId, job);
}

static int enqueueSvcCtrlJob(int command, SvcCtrlJob::SvcCtrlResultCallback callback,
			     void *userData)
{
	void *mem = triggerMgrHeapAlloc(sizeof(SvcCtrlJob));
	SvcCtrlJob *job = NULL;
	if (mem != NULL)
		job = new (mem) SvcCtrlJob(command, callback, userData);
	common::SharedPtr<job::JobItem> jobRef(job);
	if (job == NULL || jobRef.get() == NULL)
		return TRIGGER_MGR_ERROR_NO_MEMORY;
	g_triggerMgrJobQueue->Enqueue(jobRef);
	return 0;
}

} /* anonymous namespace */

/* Launches or resumes an application for a fired trigger (0x812A336E) */
int triggerMgrLaunchOrResumeApp(TriggerFireJobBase *fireJob, const char *titleId,
				const char *launchArg, bool waitLaunchResult, bool resumeOnly)
{
	s_stateLock.Lock();
	if (s_serviceState != TRIGGER_SVC_STATE_RUNNING) {
		s_stateLock.Unlock();
		return TRIGGER_MGR_ERROR_MISSED;
	}

	bool bgCapable = isBackgroundCapableApp(titleId);
	SceAppMgrAppStatus appStatus;
	bool isRunning = (sceAppMgrGetStatusByName(titleId, &appStatus) == 0);
	bool isActivated = isRunning && (appStatus.activated != 0);
	bool isRunRequested = isRunning && (appStatus.processRunRequested != 0);

	if (resumeOnly) {
		if (!bgCapable && !isActivated) {
			s_stateLock.Unlock();
			return 0;
		}
		if (bgCapable && !isRunning) {
			s_stateLock.Unlock();
			return 0;
		}
	}

	int result = TRIGGER_MGR_ERROR_INTERNAL;
	if (bgCapable) {
		result = launchBackgroundApp(fireJob, titleId, launchArg, waitLaunchResult);
	} else if (resumeOnly || (isActivated && isRunRequested)) {
		Plugin *powerPlugin = Plugin::Find("power_manage_plugin");
		if (powerPlugin != NULL) {
			power_manage::PowerManageInterface *powerIface =
				(power_manage::PowerManageInterface *)powerPlugin->GetInterface(1);
			if (powerIface != NULL) {
				sceClibPrintf("[SceTriggerMgr] Resume %s\n", titleId);
				powerIface->wakeAndLaunchForegroundApp(titleId, launchArg);
				result = 0;
			}
		}
	} else {
		result = showAppIncomingDialog(titleId);
	}

	s_stateLock.Unlock();
	return result;
}

/* Computes checksum and persists ur0:shell/trigger_table.bin (0x812A34A4) */
int triggerMgrSaveDatabase(void)
{
	++g_triggerDb.sequenceCounter;
	const SceUInt32 *words = (const SceUInt32 *)&g_triggerDb.version;
	SceUInt32 wordCount = (sizeof(g_triggerDb) / sizeof(SceUInt32)) - 1u;
	SceUInt32 checksum = 0;
	for (SceUInt32 idx = 0; idx < wordCount; ++idx)
		checksum += words[idx];
	g_triggerDb.checksum = checksum;

	sceIoMkdir(s_dbDirPath, 6);
	for (;;) {
		bool retryWrite = false;
		SceUID fd = sceIoOpen(s_dbFilePath,
				      SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC | 0x00010000, 6);
		if (fd < 0) {
			sceClibPrintf("[SceTriggerMgr] Saving DB failed. 0x%x\n", fd);
			return fd;
		}

		SceSSize written = sceIoWrite(fd, &g_triggerDb, sizeof(g_triggerDb));
		if (written >= 0) {
			if ((SceSize)written != sizeof(g_triggerDb))
				triggerMgrAssertFailNoCode();
		} else if (written == -2147418093) {
			retryWrite = true;
		} else {
			sceClibPrintf("[SceTriggerMgr] Saving DB failed. 0x%x\n", written);
		}

		int closeErr = sceIoClose(fd);
		if (!retryWrite)
			return closeErr;
	}
}

/* Database lookup helpers (0x812A365C, 0x812A3672, 0x812A45B8, 0x812A43F2) */
TriggerAppRecord *triggerMgrGetAppRecord(int appIndex)
{
	return &g_triggerDb.apps[appIndex];
}

int triggerMgrFindAppIndex(const char *titleId)
{
	for (int appIdx = 0; appIdx < TRIGGER_MGR_MAX_APPS; ++appIdx) {
		if (g_triggerDb.apps[appIdx].titleId[0] != '\0' &&
		    sce_paf_strncmp(titleId, g_triggerDb.apps[appIdx].titleId,
				    TRIGGER_MGR_TITLE_ID_LEN) == 0)
			return appIdx;
	}
	return -1;
}

int triggerMgrFindFreeAppIndex(void)
{
	for (int appIdx = 0; appIdx < TRIGGER_MGR_MAX_APPS; ++appIdx) {
		if (g_triggerDb.apps[appIdx].titleId[0] == '\0')
			return appIdx;
	}
	return -1;
}

SceUInt32 triggerMgrGetSequenceCounter(void)
{
	return g_triggerDb.sequenceCounter;
}

/* Resolves an IPC client's PID to its canonical parent/title ID (0x812A36AA) */
int triggerMgrResolveProcessTitleId(SceUID processId, char *outTitleId)
{
	SceAppMgrAppStatus appStatus;
	int statusErr = sceAppMgrGetStatusById(processId, &appStatus);
	if (statusErr != 0) {
		sceClibPrintf("[SceTriggerMgr] sceAppMgrGetStatusById(pid: 0x%x) returned 0x%x\n",
			      processId, statusErr);
		return statusErr;
	}

	if (sce_paf_strncmp(appStatus.appName, "NPXS10998", TRIGGER_MGR_TITLE_ID_LEN) == 0 ||
	    sce_paf_strncmp(appStatus.appName, "NPXS10999", TRIGGER_MGR_TITLE_ID_LEN) == 0) {
		sce_paf_strncpy(outTitleId, appStatus.appName, TRIGGER_MGR_TITLE_ID_LEN);
		outTitleId[TRIGGER_MGR_TITLE_ID_LEN] = '\0';
		return 0;
	}

	AppInfoQClient lsdbClient;
	SceLsdb_A6D8772C(&lsdbClient);
	LsdbAppInfoPtr appInfo(new LsdbAppInfo());
	SceLsdb_E90B81CD(&lsdbClient);

	string appNameKey(appStatus.appName);
	int queryErr = SceLsdb_D239031A(&lsdbClient, &appNameKey, &appInfo);
	if (queryErr == 0) {
		LsdbValue &parentVal = (*appInfo)[kLsdbKeyParentTitleId];
		const char *parentTitle = "";
		if (parentVal.type == LSDB_VALUE_TEXT && parentVal.text != NULL)
			parentTitle = parentVal.text->c_str();
		sce_paf_strncpy(outTitleId, parentTitle, TRIGGER_MGR_TITLE_ID_LEN);
		if (outTitleId[0] == '\0')
			sce_paf_strncpy(outTitleId, appStatus.appName, TRIGGER_MGR_TITLE_ID_LEN);
		outTitleId[TRIGGER_MGR_TITLE_ID_LEN] = '\0';
	}

	SceLsdb_E2101B03(&lsdbClient);
	return queryErr;
}

/* Checks whether a target title ID exists in LSDB (0x812A469A) */
bool triggerMgrIsTitleInstalled(const char *titleId)
{
	AppInfoQClient lsdbClient;
	SceLsdb_A6D8772C(&lsdbClient);
	SceLsdb_E90B81CD(&lsdbClient);
	string titleKey(titleId != NULL ? titleId : "");
	int exists = SceLsdb_9B5ABE3E(&lsdbClient, &titleKey);
	SceLsdb_E2101B03(&lsdbClient);
	return exists != 0;
}

/* Initializes SceTriggerMgr heap, job queue, LSDB listener, and DB (0x812A4876) */
int shellTriggerMgrInit(void)
{
	sce_paf_memset(s_ipcJobSlots, 0, sizeof(s_ipcJobSlots));
	sce_paf_memset(s_builtinRuntime, 0, sizeof(s_builtinRuntime));

	g_triggerMgrHeap = new memory::HeapAllocator(NULL, 0x8000u, "SceTriggerMgrHeap", NULL);
	if (g_triggerMgrHeap == NULL)
		triggerMgrAssertFailNoCode();

	job::JobQueue::Option queueOpt;
	queueOpt.stackSize = 0x2000;
	void *queueMem = triggerMgrHeapAlloc(sizeof(job::JobQueue));
	if (queueMem != NULL)
		g_triggerMgrJobQueue = new (queueMem) job::JobQueue("SceTriggerMgrJob", &queueOpt);
	if (g_triggerMgrJobQueue == NULL)
		triggerMgrAssertFailNoCode();

	void *listenerMem = triggerMgrHeapAlloc(sizeof(LsdbEventListener));
	if (listenerMem != NULL)
		s_lsdbListener = new (listenerMem) LsdbEventListener();
	SceLsdb_BF481B29(s_lsdbListener, onLsdbAppEvent, NULL);

	g_triggerMgrTableLock.ReaderLock();
	int loadErr = loadDatabase();
	if (loadErr != 0) {
		sceClibPrintf(
			"[SceTriggerMgr] Loading DB failed (0x%x). Initializing new DB.\n",
			loadErr);
		sce_paf_memset(&g_triggerDb, 0, sizeof(g_triggerDb));
		g_triggerDb.version = 1;
		for (int idx = 0; s_builtinServices[idx].serviceId != NULL; ++idx) {
			TriggerServiceRecord *record = &g_triggerDb.services[idx];
			sce_paf_memset(record, 0, sizeof(*record));
			sce_paf_strncpy(record->serviceId, s_builtinServices[idx].serviceId,
					TRIGGER_MGR_TITLE_ID_LEN);
			if (sce_paf_strncmp(s_builtinServices[idx].serviceId, "SHELL0006",
					    TRIGGER_MGR_TITLE_ID_LEN) != 0)
				record->flags |= TRIGGER_APP_FLAG_ENABLED;
		}
	}
	g_triggerMgrTableLock.ReaderUnlock();
	return 0;
}

/* Resumes (starts) trigger execution (0x812A4A3A) */
int shellTriggerMgrResume(ShellTriggerCtrlCallback callback, void *userData)
{
	return enqueueSvcCtrlJob(0, (SvcCtrlJob::SvcCtrlResultCallback)callback, userData);
}

/* Suspends trigger execution (0x812A4B48) */
int shellTriggerMgrSuspend(ShellTriggerCtrlCallback callback, void *userData)
{
	return enqueueSvcCtrlJob(1, (SvcCtrlJob::SvcCtrlResultCallback)callback, userData);
}

/* Compatibility entry used by boot.cpp (0x812A4A3A) */
int shellTriggerMgrSvcCtrl(int command, int arg)
{
	return enqueueSvcCtrlJob(command, NULL, (void *)(SceIntPtr)arg);
}

/* Removes ur0:shell/trigger_table.bin (0x812A4C58) */
void shellTriggerTableReset(void)
{
	sceIoRemove(s_dbFilePath);
}

/* Built-in service registration and state queries (0x812A4C6C-0x812A509E) */
int shellTriggerMgrRegisterService(const char *serviceId)
{
	g_triggerMgrTableLock.WriterLock();
	int metaIdx = -1;
	for (int idx = 0; s_builtinServices[idx].serviceId != NULL; ++idx) {
		if (sce_paf_strncmp(s_builtinServices[idx].serviceId, serviceId,
				    TRIGGER_MGR_TITLE_ID_LEN) == 0) {
			metaIdx = idx;
			break;
		}
	}
	if (metaIdx < 0) {
		g_triggerMgrTableLock.WriterUnlock();
		return TRIGGER_MGR_ERROR_SERVICE_NOT_FOUND;
	}
	s_builtinRuntime[metaIdx].registered = true;
	g_triggerMgrTableLock.WriterUnlock();
	return 0;
}

int shellTriggerMgrUnregisterService(const char *serviceId)
{
	int metaIdx = -1;
	for (int idx = 0; s_builtinServices[idx].serviceId != NULL; ++idx) {
		if (sce_paf_strncmp(s_builtinServices[idx].serviceId, serviceId,
				    TRIGGER_MGR_TITLE_ID_LEN) == 0) {
			metaIdx = idx;
			break;
		}
	}
	if (metaIdx < 0)
		return TRIGGER_MGR_ERROR_SERVICE_NOT_FOUND;
	s_builtinRuntime[metaIdx].registered = false;
	return 0;
}

int shellTriggerMgrSetServiceEnabled(const char *serviceId, bool enabled)
{
	return setTitleTriggerEnabled(serviceId, enabled);
}

int shellTriggerMgrIsServiceEnabled(const char *serviceId, bool *outEnabled)
{
	g_triggerMgrTableLock.ReaderLock();
	TriggerServiceRecord *record = NULL;
	BuiltinServiceRuntime *runtime = NULL;
	int err = findBuiltinService(serviceId, &record, &runtime);
	if (err == 0)
		*outEnabled = (record->flags & TRIGGER_APP_FLAG_ENABLED) != 0;
	g_triggerMgrTableLock.ReaderUnlock();
	return err;
}

int shellTriggerMgrSetServiceCallback(const char *serviceId,
				      ShellTriggerServiceCallback callback, void *userData)
{
	g_triggerMgrTableLock.WriterLock();
	TriggerServiceRecord *record = NULL;
	BuiltinServiceRuntime *runtime = NULL;
	int err = findBuiltinService(serviceId, &record, &runtime);
	if (err == 0) {
		runtime->callback = callback;
		runtime->userData = userData;
	}
	g_triggerMgrTableLock.WriterUnlock();
	return err;
}

int shellTriggerMgrClearServiceCallback(const char *serviceId)
{
	g_triggerMgrTableLock.WriterLock();
	TriggerServiceRecord *record = NULL;
	BuiltinServiceRuntime *runtime = NULL;
	int err = findBuiltinService(serviceId, &record, &runtime);
	if (err == 0) {
		runtime->registered = false;
		runtime->callback = NULL;
		runtime->userData = NULL;
	}
	g_triggerMgrTableLock.WriterUnlock();
	return err;
}

/* IPMI async response helpers (0x812A5D62-0x812A5EE4) */
int triggerMgrRespondRemainingTime(IPMI::Session *session, SceUInt32 requestId,
				   int result, const SceUInt32 *remainingMinutes)
{
	IPMI::DataInfo out;
	out.pData = remainingMinutes;
	out.dataSize = sizeof(*remainingMinutes);
	return session->respondToAsyncMethodRequest(
		TRIGGER_METHOD_GET_PERIODIC_TIMER_REMAINING_TIME_RESULT,
		requestId, result, &out, 1);
}

int triggerMgrRespondTimeOfDayTime(IPMI::Session *session, SceUInt32 requestId,
				   int result, const SceRtcTick *targetTick, const bool *expired)
{
	IPMI::DataInfo out[2];
	out[0].pData = targetTick;
	out[0].dataSize = sizeof(*targetTick);
	out[1].pData = expired;
	out[1].dataSize = sizeof(*expired);
	return session->respondToAsyncMethodRequest(
		TRIGGER_METHOD_GET_TIME_OF_DAY_TIMER_TIME_RESULT,
		requestId, result, out, 2);
}

int triggerMgrRespondDailyTimerInfo(IPMI::Session *session, SceUInt32 requestId,
				    int result, const char *targetTitleId,
				    const TriggerDailyTimerParam *timerParam,
				    const SceInt32 *remainingSeconds)
{
	IPMI::DataInfo out[3];
	out[0].pData = targetTitleId;
	out[0].dataSize = 16;
	out[1].pData = timerParam;
	out[1].dataSize = sizeof(*timerParam);
	out[2].pData = remainingSeconds;
	out[2].dataSize = sizeof(*remainingSeconds);
	return session->respondToAsyncMethodRequest(
		TRIGGER_METHOD_GET_DAILY_TIMER_INFO_RESULT,
		requestId, result, out, 3);
}

int triggerMgrRespondPolicy(IPMI::Session *session, SceUInt32 requestId,
			    int result, const SceUInt8 *policy)
{
	IPMI::DataInfo out;
	out.pData = policy;
	out.dataSize = sizeof(*policy);
	return session->respondToAsyncMethodRequest(
		TRIGGER_METHOD_GET_POLICY_RESULT, requestId, result, &out, 1);
}

int triggerMgrRespondResultOnly(IPMI::Session *session, SceUInt32 requestId, int result)
{
	return session->respondToAsyncMethodRequest(
		TRIGGER_METHOD_GET_RESULT, requestId, result, NULL, 0);
}

int triggerMgrRespondAppInfo(IPMI::Session *session, SceUInt32 requestId,
			     int result, const TriggerAppInfoResponse *appInfo)
{
	IPMI::DataInfo out;
	out.pData = appInfo;
	out.dataSize = sizeof(*appInfo);
	return session->respondToAsyncMethodRequest(
		TRIGGER_METHOD_GET_APP_INFO_RESULT, requestId, result, &out, 1);
}

int triggerMgrRespondServiceInfo(IPMI::Session *session, SceUInt32 requestId,
				 int result, const TriggerServiceInfoResponse *serviceInfo)
{
	IPMI::DataInfo out;
	out.pData = serviceInfo;
	out.dataSize = sizeof(*serviceInfo);
	return session->respondToAsyncMethodRequest(
		TRIGGER_METHOD_GET_SYSTEM_SERVICE_INFO_RESULT, requestId, result, &out, 1);
}

/* SceShellSvc IPMI channel 0x00100000 dispatchers (0x812A581C-0x812A5840, 0x812A5F28, 0x812A5FD0) */
int shellSvcDisp_10_Sync(IPMI::Session *session, SceUInt32 methodId,
			 const IPMI::DataInfo *inList, SceUInt32 inCount,
			 IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	if (methodId == TRIGGER_METHOD_SET_TITLE_FLAG) {
		bool enabled = (*(const SceUInt8 *)inList[0].pData) != 0;
		const char *titleId = (const char *)inList[1].pData;
		int result = setTitleTriggerEnabled(titleId, enabled);
		return session->respondToSyncMethodRequest(result, outList, outCount);
	}
	if (methodId == TRIGGER_METHOD_GET_APP_TITLE_ID_LIST) {
		int result = getAppTitleIdList((char *)outList[0].pBuffer,
					       (SceUInt32 *)outList[1].pBuffer);
		outList[0].outSize = 1000;
		outList[1].outSize = 4;
		return session->respondToSyncMethodRequest(result, outList, outCount);
	}
	if (methodId == TRIGGER_METHOD_GET_SYSTEM_TITLE_ID_LIST) {
		int result = getSystemTitleIdList((char *)outList[0].pBuffer,
						  (SceUInt32 *)outList[1].pBuffer);
		outList[0].outSize = 320;
		outList[1].outSize = 4;
		return session->respondToSyncMethodRequest(result, outList, outCount);
	}
	return TRIGGER_MGR_ERROR_UNSUPPORTED_METHOD;
}

int shellSvcDisp_10_Async(IPMI::Session *session, SceUInt32 requestId,
			  SceUInt32 methodId, const IPMI::DataInfo *inList,
			  SceUInt32 inCount)
{
	(void)inCount;
	switch (methodId) {
	case TRIGGER_METHOD_REGISTER_PERIODIC_TIMER: {
		const TriggerTimerRequestHeader *hdr =
			(const TriggerTimerRequestHeader *)inList[0].pData;
		return triggerPeriodicTimerEnqueueRegister(
			session, requestId, (const char *)inList[1].pData,
			(const TriggerPeriodicTimerParam *)inList[2].pData,
			hdr->timerSlot, hdr->useCallback);
	}
	case TRIGGER_METHOD_UNREGISTER_PERIODIC_TIMER: {
		const TriggerTimerRequestHeader *hdr =
			(const TriggerTimerRequestHeader *)inList[0].pData;
		return triggerPeriodicTimerEnqueueUnregister(
			session, requestId, hdr->timerSlot, hdr->useCallback);
	}
	case TRIGGER_METHOD_GET_PERIODIC_TIMER_REMAINING_TIME: {
		const TriggerTimerRequestHeader *hdr =
			(const TriggerTimerRequestHeader *)inList[0].pData;
		return triggerPeriodicTimerEnqueueGetRemainingTime(
			session, requestId, hdr->timerSlot, hdr->useCallback);
	}
	case TRIGGER_METHOD_GET_PERIODIC_TIMER_LAST_ERROR: {
		const TriggerTimerRequestHeader *hdr =
			(const TriggerTimerRequestHeader *)inList[0].pData;
		return triggerPeriodicTimerEnqueueGetLastError(
			session, requestId, hdr->timerSlot, hdr->useCallback);
	}
	case TRIGGER_METHOD_REGISTER_TIME_OF_DAY_TIMER: {
		const TriggerTimerRequestHeader *hdr =
			(const TriggerTimerRequestHeader *)inList[0].pData;
		return triggerTimeOfDayTimerEnqueueRegister(
			session, requestId, (const char *)inList[1].pData,
			(const TriggerTimeOfDayTimerParam *)inList[2].pData,
			hdr->timerSlot, hdr->useCallback);
	}
	case TRIGGER_METHOD_UNREGISTER_TIME_OF_DAY_TIMER: {
		const TriggerTimerRequestHeader *hdr =
			(const TriggerTimerRequestHeader *)inList[0].pData;
		return triggerTimeOfDayTimerEnqueueUnregister(
			session, requestId, hdr->timerSlot, hdr->useCallback);
	}
	case TRIGGER_METHOD_GET_TIME_OF_DAY_TIMER_TIME: {
		const TriggerTimerRequestHeader *hdr =
			(const TriggerTimerRequestHeader *)inList[0].pData;
		return triggerTimeOfDayTimerEnqueueGetTime(
			session, requestId, hdr->timerSlot, hdr->useCallback);
	}
	case TRIGGER_METHOD_GET_TIME_OF_DAY_TIMER_LAST_ERROR: {
		const TriggerTimerRequestHeader *hdr =
			(const TriggerTimerRequestHeader *)inList[0].pData;
		return triggerTimeOfDayTimerEnqueueGetLastError(
			session, requestId, hdr->timerSlot, hdr->useCallback);
	}
	case TRIGGER_METHOD_REGISTER_DAILY_TIMER: {
		const TriggerTimerRequestHeader *hdr =
			(const TriggerTimerRequestHeader *)inList[0].pData;
		return triggerDailyTimerEnqueueRegister(
			session, requestId, (const char *)inList[1].pData,
			(const TriggerDailyTimerParam *)inList[2].pData,
			hdr->timerSlot, hdr->useCallback);
	}
	case TRIGGER_METHOD_UNREGISTER_DAILY_TIMER: {
		const TriggerTimerRequestHeader *hdr =
			(const TriggerTimerRequestHeader *)inList[0].pData;
		return triggerDailyTimerEnqueueUnregister(
			session, requestId, hdr->timerSlot, hdr->useCallback);
	}
	case TRIGGER_METHOD_GET_DAILY_TIMER_INFO: {
		const TriggerTimerRequestHeader *hdr =
			(const TriggerTimerRequestHeader *)inList[0].pData;
		return triggerDailyTimerEnqueueGetInfo(
			session, requestId, hdr->timerSlot, hdr->useCallback);
	}
	case TRIGGER_METHOD_GET_DAILY_TIMER_LAST_ERROR: {
		const TriggerTimerRequestHeader *hdr =
			(const TriggerTimerRequestHeader *)inList[0].pData;
		return triggerDailyTimerEnqueueGetLastError(
			session, requestId, hdr->timerSlot, hdr->useCallback);
	}
	case TRIGGER_METHOD_GET_POLICY: {
		bool useCallback = (*(const SceUInt8 *)inList[0].pData) != 0;
		return enqueueGetPolicyJob(session, requestId, useCallback);
	}
	case TRIGGER_METHOD_GET_APP_INFO: {
		const TriggerInfoRequestHeader *hdr =
			(const TriggerInfoRequestHeader *)inList[0].pData;
		return enqueueGetAppInfoJob(
			session, requestId, (const char *)inList[1].pData,
			hdr->language, hdr->useCallback);
	}
	case TRIGGER_METHOD_GET_SYSTEM_SERVICE_INFO: {
		const TriggerInfoRequestHeader *hdr =
			(const TriggerInfoRequestHeader *)inList[0].pData;
		return enqueueGetServiceInfoJob(
			session, requestId, (const char *)inList[1].pData,
			hdr->language, hdr->useCallback);
	}
	default:
		return TRIGGER_MGR_ERROR_UNSUPPORTED_METHOD;
	}
}

void shellSvcDisp_10_Disconn(IPMI::Session *session)
{
	g_triggerMgrIpcLock.Lock();
	for (unsigned int slotIdx = 0; slotIdx < TRIGGER_MGR_MAX_IPC_JOBS; ++slotIdx) {
		if (s_ipcJobSlots[slotIdx].session == session) {
			triggerMgrClearIpcSlot(&s_ipcJobSlots[slotIdx]);
			break;
		}
	}
	g_triggerMgrIpcLock.Unlock();
}
