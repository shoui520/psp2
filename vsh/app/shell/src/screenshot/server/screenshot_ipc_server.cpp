/*
 * screenshot_ipc_server.cpp: SceShell SceScreenShot IPMI server ("SceScreenShot",
 * methods 0x34560000 - 0x3456000B) and ScreenShotManager lifecycle/capture control.
 * 3.65-CEX: 0x81244AF4 - 0x81246A77, 0x81248436 - 0x81248A0F
 */
#include "screenshot_server.h"

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/stdio.h>
#include <screenshot.h>
#include <appmgr/appmgr.h>
#include <dir/dir.h>
#include <display/display.h>
#include <file/localfile.h>
#include <graphics/graphmem/graphmem.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_display.h>
#include <image/image.h>
#include <kernel/iofilemgr.h>
#include <kernel/sysmem/memblock.h>
#include <kernel/threadmgr.h>
#include <memory/heap_allocator.h>
#include <regmgr/regmgr.h>
#include <ss_mgr/ss_mgr.h>
#include <sysmodule/sysmodule.h>
#include <widget/core/environment.h>

#include "../client/screenshot_internal.h"
#include "shell.h"

extern "C" {
extern int sceAppMgrGetStatusByName(const char *appName, SceAppMgrAppStatus *status);
extern int sceAppMgrCaptureFrameBufDMACByAppId(SceInt32 appId, SceDisplayFrameBuf *frameBuf);
extern int sceAppMgrCaptureFrameBufIFTUByAppId(SceInt32 appId, SceDisplayFrameBuf *frameBuf);
extern int sceAppMgrIsCameraActive(SceUID pid, int *active);
extern int sceAppMgrSendParam(const char *appName, const char *param, SceUInt32 flags);
}

namespace screenshot {

namespace {

static paf::job::JobQueue *s_jobQueue = NULL;
static ScreenShotManager *s_manager = NULL;
static ScreenShotIpcServer *s_ipcServer = NULL;

static const SceInt32 kAppIdSelf = -2;
static const SceInt32 kAppIdForeground = -4;
static const SceInt32 kAppIdCurrentDisplay = -5;

static const SceUInt32 kScreenShotServerPriority = 0x100000F7;
static const SceSize kScreenShotServerStackSize = 0x4000;
static const SceSize kIpmiMaxRequestSize = 3840;
static const SceSize kIpmiMaxMessageSize = 17408;
static const SceSize kIpmiServerObjectSize = 0x100;
static const SceUInt32 kIpmiWorkBufferAlign = 0x10;
static const SceUInt32 kOverlayOpenFlags = SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC | 0x10000;

static const SceUInt32 kMaxPhotoTitleChars = 64;
static const SceUInt32 kMaxGameTitleChars = 64;
static const SceUInt32 kMaxGameCommentChars = 128;
static const SceInt32 kLegacyTitleMaxBytes = 128;

struct AppEventPayload {
	SceUID processId;
	SceInt32 appId;
};

struct OverlayChunkHeader {
	SceSize size;
	SceInt32 index;
};

struct OverlayOffsetParam {
	SceInt32 offsetX;
	SceInt32 offsetY;
};

/* 0x81244CF8 */
static int prepareTempDirectories(void)
{
	const char *tempRoot = "ur0:temp";
	const char *ssDir = "ur0:temp/screenshot";
	const char *overlayDir = "ur0:temp/screenshot/overlay";

	int ret = paf::Dir::RemoveRecursive(ssDir);
	if (!paf::Dir::IsExist(tempRoot)) {
		ret = paf::Dir::Create(tempRoot, 6U);
		if (ret != 0)
			return ret;
	}
	if (!paf::Dir::IsExist(ssDir)) {
		ret = paf::Dir::Create(ssDir, 6U);
		if (ret != 0)
			return ret;
	}
	if (!paf::Dir::IsExist(overlayDir))
		ret = paf::Dir::Create(overlayDir, 6U);
	return ret;
}

/* 0x812450EA */
static int queryAppStatus(SceInt32 appId, SceAppMgrAppStatus *outStatus)
{
	int ret = sceAppMgrGetStatusByAppId(appId, outStatus);
	if (ret != 0)
		outStatus->appId = -1;
	return ret;
}

static void queryForegroundStatus(SceAppMgrAppStatus *outStatus)
{
	sce_paf_memset(outStatus, 0, sizeof(*outStatus));
	int ret = sceAppMgrGetStatusByAppId(kAppIdForeground, outStatus);
	if (ret != 0)
		outStatus->appId = -1;
	if (ret == kAppMgrErrNoForeground) {
		if (sceAppMgrGetStatusByAppId(kAppIdSelf, outStatus) != 0)
			outStatus->appId = -1;
	}
}

static bool validateUtf8MaxChars(const char *text, SceUInt32 maxChars)
{
	const char *str = (text != NULL) ? text : "";
	paf::string copy(str, sce_paf_strlen(str));
	SceUInt32 totalBytes = copy.size();
	if (totalBytes == 0)
		return true;

	SceUInt32 offset = 0;
	SceInt32 remaining = static_cast<SceInt32>(totalBytes);
	SceUInt32 charCount = 0;
	while (offset < totalBytes) {
		int charBytes = 0;
		if (shellUtf8DecodeChar(copy.c_str() + offset, remaining, &charBytes) != 0)
			return false;
		++charCount;
		offset += static_cast<SceUInt32>(charBytes);
		remaining -= charBytes;
	}
	return charCount < maxChars;
}

/* 0x812467B6 */
static int screenShotServerThreadEntry(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	if (s_ipcServer != NULL)
		s_ipcServer->RunDispatcher();
	return 0;
}

/* 0x81246A4C */
static void onCaptureJobComplete(int result, SceInt32 appId, const char *exportedPath,
				 void *userData)
{
	ScreenShotManager *manager = static_cast<ScreenShotManager *>(userData);
	if (manager != NULL)
		manager->OnCaptureJobFinished(result, appId, exportedPath);
}

/* 0x81246A60 */
static void onShellAppEvent(int eventCode, const void *data, SceSize size, void *userData)
{
	(void)size;
	ScreenShotManager *manager = static_cast<ScreenShotManager *>(userData);
	const AppEventPayload *payload = static_cast<const AppEventPayload *>(data);
	if (manager != NULL && payload != NULL)
		manager->OnAppEvent(payload->processId, payload->appId, eventCode);
}

/* 0x81245092 */
static int createManager(const InitParam *param)
{
	if (s_manager != NULL)
		return kScreenShotErrAlreadyInit;

	setAllocator(param->allocator);
	s_manager = new ScreenShotManager(*param);
	if (s_manager == NULL)
		return kScreenShotErrNoMemory;

	s_manager->Start();
	return 0;
}

/* IPC method handlers (0x8124868C - 0x8124884E) */

static int ipcSetParam(IPMI::Session *session, const char *photoTitle,
		       const char *gameTitle, const char *gameComment,
		       SceUInt8 legacyLimitMode)
{
	if (s_manager == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	return s_manager->SetParam(session->getAppId(), photoTitle, gameTitle,
				   gameComment, legacyLimitMode);
}

static int ipcSendOverlayData(IPMI::Session *session, const void *chunkData,
			      SceSize chunkSize, SceInt32 chunkIndex)
{
	if (s_manager == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	return s_manager->ReceiveOverlayData(session->getAppId(), chunkData,
					     chunkSize, chunkIndex);
}

static int ipcSetOverlayOffset(IPMI::Session *session, SceInt32 offsetX, SceInt32 offsetY)
{
	if (s_manager == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	return s_manager->SetOverlayOffset(session->getAppId(), offsetX, offsetY);
}

static int ipcEnable(IPMI::Session *session)
{
	if (s_manager == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	return s_manager->EnableCapture(session->getAppId());
}

static int ipcDisable(IPMI::Session *session)
{
	if (s_manager == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	return s_manager->DisableCapture(session->getAppId());
}

static int ipcEnableOverlay(IPMI::Session *session)
{
	if (s_manager == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	return s_manager->EnableOverlay(session->getAppId());
}

static int ipcDisableOverlay(IPMI::Session *session)
{
	if (s_manager == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	return s_manager->DisableOverlay(session->getAppId());
}

static int ipcEnableNotification(IPMI::Session *session)
{
	if (s_manager == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	return s_manager->EnableNotification(session->getAppId());
}

static int ipcDisableNotification(IPMI::Session *session)
{
	if (s_manager == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	return s_manager->DisableNotification(session->getAppId());
}

static int ipcStartCapture(IPMI::Session *session, SceUInt32 requestId, SceUInt32 mode)
{
	if (s_manager == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	return s_manager->RequestIpcCapture(session, requestId, mode);
}

static int ipcCancelCapture(IPMI::Session *session)
{
	if (s_manager == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	return s_manager->CancelIpcCapture(session->getAppId());
}

} /* anonymous namespace */

ScreenShotManager *getManager(void)
{
	return s_manager;
}

ScreenShotIpcServer *getIpcServer(void)
{
	return s_ipcServer;
}

/* ScreenShotManager */

void *ScreenShotManager::operator new(size_t size)
{
	return allocMemory(size);
}

void ScreenShotManager::operator delete(void *ptr)
{
	freeMemory(ptr);
}

/* 0x81244F94 */
ScreenShotManager::ScreenShotManager(const InitParam &param)
	: m_serverThreadId(0),
	  m_allocator(param.allocator),
	  m_threadPriority(param.threadPriority),
	  m_threadStackSize(param.threadStackSize),
	  m_cpuAffinityMask(param.cpuAffinityMask),
	  m_jobQueue(param.jobQueue),
	  m_frameBuffer(NULL),
	  m_frameBufferSize(0),
	  m_captureFlags(0),
	  m_completeCallback(NULL),
	  m_completeUserData(NULL),
	  m_allowAlways(false),
	  m_mediaMounted(false),
	  m_requestCallback(NULL),
	  m_requestUserData(NULL)
{
	m_allowAlways = (sceSblQafMgrIsAllowScreenShotAlways() == 1);
	sce_paf_memset(m_exportedPath, 0, sizeof(m_exportedPath));
}

/* 0x81244C70 */
int ScreenShotManager::AllocateSurface(SceUInt32 width, SceUInt32 height)
{
	if (m_frameBuffer == NULL)
		return -1;

	paf::intrusive_ptr<paf::graph::Surface> newSurface(
		new paf::graph::Surface(static_cast<int32_t>(width),
					static_cast<int32_t>(height),
					ImageMode_RGBA8888,
					ImageOrder_Linear,
					1,
					m_frameBuffer,
					0,
					1,
					false));
	m_captureSurface = newSurface;

	if (m_captureSurface.get() != NULL && m_captureSurface->GetMemory() != NULL)
		return 0;
	return -1;
}

/* 0x81244D98 */
void ScreenShotManager::Start()
{
	sceSysmoduleLoadModule(SCE_SYSMODULE_PHOTO_EXPORT);
	sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_JPEG_ENC_ARM);
	pictureFileCreatorInit();
	prepareTempDirectories();

	m_mediaMounted = paf::LocalFile::Exists("ux0:");
	if (m_jobQueue == NULL)
		m_jobQueue = paf::job::JobQueue::default_queue;

	SceKernelAllocMemBlockOpt memOpt;
	sce_paf_memset(&memOpt, 0, sizeof(memOpt));
	memOpt.size = sizeof(memOpt);
	memOpt.attr = 0x200000;

	paf::GraphMem::Option graphOpt;
	graphOpt.type = paf::GraphMem::Option::MemoryType_Default;
	graphOpt.memblock_option = &memOpt;
	graphOpt.gpu_map_type = paf::GraphMem::Option::GpuMapType_Default;
	graphOpt.gpu_map_attrib = 3U;
	graphOpt.gpu_usse_offset = 0;
	graphOpt.report_error = false;

	m_frameBuffer = paf::GraphMem::AllocOSMemory(
		paf::GraphMem::DeviceType_SystemMemory,
		kFrameBufferSize, "SceScreenShotBuffer", &graphOpt);
	if (m_frameBuffer != NULL)
		m_frameBufferSize = kFrameBufferSize;

	AllocateSurface(kScreenWidth, kScreenHeight);

	SceAppMgrAppStatus selfStatus;
	sce_paf_memset(&selfStatus, 0, sizeof(selfStatus));
	int statusRet = sceAppMgrGetStatusByName(NULL, &selfStatus);
	m_shellEntry.appId = selfStatus.appId;
	m_shellEntry.processId = (statusRet == 0) ? selfStatus.processId : kAppIdSelf;
	m_shellEntry.captureEnabled = false;
	m_shellEntry.reservedState = false;
	m_shellEntry.overlayEnabled = false;
	m_shellEntry.notificationEnabled = false;

	for (SceUInt32 idx = 0; idx < kMaxAppEntries; ++idx) {
		m_appEntries[idx].appId = -1;
		m_appEntries[idx].processId = -1;
		m_appEntries[idx].overlayFileFd = -1;
		m_appEntries[idx].captureEnabled = true;
		m_appEntries[idx].reservedState = false;
		m_appEntries[idx].overlayEnabled = false;
		m_appEntries[idx].notificationEnabled = false;
		m_appEntries[idx].photoTitle.assign("", 0);
		m_appEntries[idx].gameTitle.assign("", 0);
		m_appEntries[idx].gameComment.assign("", 0);
	}

	s_ipcServer = new ScreenShotIpcServer();
	if (s_ipcServer != NULL)
		statusRet = s_ipcServer->Create();

	if (statusRet == 0) {
		m_serverThreadId = sceKernelCreateThread(
			"SceScreenShotServer",
			screenShotServerThreadEntry,
			m_threadPriority,
			m_threadStackSize,
			0,
			m_cpuAffinityMask,
			NULL);
		if (m_serverThreadId > 0)
			sceKernelStartThread(m_serverThreadId, 0, NULL);
	}

	SceShell_83D16CA8(SHELL_APP_EVENT_STARTED, onShellAppEvent, this);
	SceShell_83D16CA8(SHELL_APP_EVENT_ENDED, onShellAppEvent, this);
}

AppEntry *ScreenShotManager::FindEntry(SceInt32 appId)
{
	if (appId == kAppIdSelf || appId == m_shellEntry.appId)
		return &m_shellEntry;
	for (SceUInt32 idx = 0; idx < kMaxAppEntries; ++idx) {
		if (m_appEntries[idx].appId == appId)
			return &m_appEntries[idx];
	}
	return NULL;
}

void ScreenShotManager::ResetPendingIpcCapture(int errorResult)
{
	sce_paf_memset(m_exportedPath, 0, sizeof(m_exportedPath));
	if (m_pendingIpc.appId != -1 &&
	    !m_pendingIpc.appExited &&
	    s_ipcServer != NULL &&
	    !m_pendingIpc.canceled) {
		s_ipcServer->SendCaptureResponse(m_pendingIpc.session,
						 m_pendingIpc.requestId,
						 errorResult, m_exportedPath);
	}
	m_pendingIpc.Clear();
}

/* 0x812450FE */
int ScreenShotManager::CaptureOverlayAndComposite(SceInt32 appId,
						  SceUInt32 *inOutWidth,
						  SceUInt32 *inOutHeight)
{
	SceDisplayFrameBuf frameBuf;
	frameBuf.size = sizeof(frameBuf);
	frameBuf.base = m_frameBuffer;
	frameBuf.pitch = *inOutWidth;
	frameBuf.pixelformat = 0;
	frameBuf.width = *inOutWidth;
	frameBuf.height = *inOutHeight;

	sce_paf_memset(m_frameBuffer, 0, m_frameBufferSize);
	int ret = sceAppMgrCaptureFrameBufDMACByAppId(appId, &frameBuf);
	if (ret < 0)
		sce_paf_memset(m_frameBuffer, 0, m_frameBufferSize);
	if (ret != 0)
		return ret;

	paf::intrusive_ptr<paf::graph::DisplaySurface> rootSurface(paf::ui::Screen::s_root_surface);
	SceInt32 page = ScePafGraphics_B000FA14(rootSurface.get());
	void *displayPixels = rootSurface->Lock(page);
	rootSurface->Unlock();

	SurfaceDesc displayDesc;
	displayDesc.width = static_cast<SceUInt32>(rootSurface->GetWidth());
	displayDesc.height = static_cast<SceUInt32>(rootSurface->GetHeight());
	displayDesc.pixels = displayPixels;
	displayDesc.pitchBytes = static_cast<SceUInt32>(rootSurface->GetStride(0));
	displayDesc.pixelPitch = static_cast<SceUInt32>(rootSurface->GetWidth());

	SurfaceDesc captureDesc;
	captureDesc.width = frameBuf.width;
	captureDesc.height = frameBuf.height;
	captureDesc.pixels = m_frameBuffer;
	captureDesc.pitchBytes = frameBuf.width * 4U;
	captureDesc.pixelPitch = frameBuf.pitch;

	bool lockSwapOnBlend = true;
	if (captureDesc.width != displayDesc.width || captureDesc.height != displayDesc.height) {
		paf::common::SharedPtr<paf::job::JobItem> lockJob(new LockSwapJob());
		if (lockJob.get() != NULL)
			ret = m_jobQueue->Enqueue(lockJob);

		paf::common::SharedPtr<paf::job::JobItem> resizeJob(
			new ResizeImageJob(captureDesc, displayDesc));
		if (resizeJob.get() != NULL)
			ret = m_jobQueue->Enqueue(resizeJob);

		captureDesc.width = displayDesc.width;
		captureDesc.height = displayDesc.height;
		captureDesc.pitchBytes = displayDesc.pitchBytes;
		lockSwapOnBlend = false;
	}

	paf::common::SharedPtr<paf::job::JobItem> blendJob(
		new BlendImageJob(captureDesc, displayDesc, lockSwapOnBlend));
	if (blendJob.get() != NULL)
		ret = m_jobQueue->Enqueue(blendJob);

	*inOutWidth = captureDesc.width;
	*inOutHeight = captureDesc.height;
	return ret;
}

/* 0x81245522 */
int ScreenShotManager::StartCapture(CaptureCompleteFn callback, void *userData)
{
	if (!m_mediaMounted)
		return SCE_SCREENSHOT_ERROR_MEDIA_NOT_EXIST;

	SceAppMgrAppStatus fgStatus;
	queryForegroundStatus(&fgStatus);

	SceUInt32 ipcMode = 0;
	if (m_pendingIpc.appId != -1) {
		if (m_pendingIpc.appId != fgStatus.appId) {
			ResetPendingIpcCapture(SCE_SCREENSHOT_ERROR_CAPTURE_FAILED);
			return SCE_SCREENSHOT_ERROR_CAPTURE_FAILED;
		}
		ipcMode = m_pendingIpc.captureMode;
	}

	AppEntry *entry = FindEntry(fgStatus.appId);
	if (entry == NULL) {
		ResetPendingIpcCapture(SCE_SCREENSHOT_ERROR_INTERNAL);
		return kScreenShotErrAlreadyInit;
	}

	bool bypassDisable = false;
	if (m_allowAlways) {
		int debugScreenshot = 0;
		if (sceRegMgrGetKeyInt("/CONFIG/PHOTO", "debug_screenshot", &debugScreenshot) == 0 &&
		    debugScreenshot == 1) {
			bypassDisable = true;
		}
	}

	if (!bypassDisable &&
	    ((ipcMode == 0 && !entry->captureEnabled) || !m_shellEntry.captureEnabled)) {
		ResetPendingIpcCapture(SCE_SCREENSHOT_ERROR_SCREENSHOT_DISABLED);
		return kScreenShotErrDisabled;
	}

	if (m_activeJob.get() != NULL)
		return kScreenShotErrAlreadyInit;

	FrameBufferInfo fbInfo;
	fbInfo.pixels = m_frameBuffer;
	fbInfo.width = kScreenWidth;
	fbInfo.height = kScreenHeight;
	fbInfo.pitchBytes = kScreenStrideBytes;

	if (CaptureOverlayAndComposite(kAppIdCurrentDisplay, &fbInfo.width, &fbInfo.height) ==
	    kAppMgrErrCaptureBlocked) {
		return kScreenShotErrDisabled;
	}

	int cameraActive = 0;
	int useCameraVol = 1;
	if (sceAppMgrIsCameraActive(fgStatus.processId, &cameraActive) == 0)
		useCameraVol = (cameraActive != 0) ? 1 : 0;
	shellShutterSoundPlay(0, useCameraVol);

	paf::common::SharedPtr<paf::job::JobItem> captureJob(
		new CaptureJob(*entry, fgStatus.processId, fbInfo, m_allowAlways,
			       m_exportedPath, onCaptureJobComplete, this));
	if (captureJob.get() == NULL) {
		ResetPendingIpcCapture(SCE_SCREENSHOT_ERROR_NO_MEMORY);
		return kScreenShotErrNoMemory;
	}

	m_completeCallback = callback;
	m_completeUserData = userData;
	m_activeJob = captureJob;
	return m_jobQueue->Enqueue(captureJob);
}

/* 0x81245A28 */
int ScreenShotManager::CaptureToSurface(paf::intrusive_ptr<paf::graph::Surface> *outSurface,
					SceInt32 appId)
{
	SceDisplayFrameBuf frameBuf;
	frameBuf.size = sizeof(frameBuf);
	frameBuf.base = m_frameBuffer;
	frameBuf.pitch = kScreenWidth;
	frameBuf.pixelformat = 0;
	frameBuf.width = kScreenWidth;
	frameBuf.height = kScreenHeight;

	sce_paf_memset(m_frameBuffer, 0, m_frameBufferSize);
	if (sceAppMgrCaptureFrameBufIFTUByAppId(appId, &frameBuf) < 0)
		sce_paf_memset(m_frameBuffer, 0, m_frameBufferSize);

	*outSurface = m_captureSurface;
	return 0;
}

/* 0x81245AAE */
int ScreenShotManager::OnAppEvent(SceUID processId, SceInt32 appId, int eventType)
{
	SceAppMgrAppStatus status;
	sce_paf_memset(&status, 0, sizeof(status));
	int statusRet = sceAppMgrGetStatusByAppId(appId, &status);
	SceInt32 resolvedAppId = (statusRet == 0) ? status.appId : -1;

	AppEntry *found = NULL;
	AppEntry *freeSlot = NULL;

	if (resolvedAppId == -1) {
		for (SceUInt32 idx = 0; idx < kMaxAppEntries; ++idx) {
			if (m_appEntries[idx].processId == processId) {
				found = &m_appEntries[idx];
				break;
			}
			if (freeSlot == NULL && m_appEntries[idx].processId == -1)
				freeSlot = &m_appEntries[idx];
		}
	} else {
		for (SceUInt32 idx = 0; idx < kMaxAppEntries; ++idx) {
			if (m_appEntries[idx].appId == resolvedAppId) {
				found = &m_appEntries[idx];
				break;
			}
			if (freeSlot == NULL && m_appEntries[idx].appId == -1)
				freeSlot = &m_appEntries[idx];
		}
	}

	if (eventType == SHELL_APP_EVENT_STARTED) {
		if (found == NULL && freeSlot != NULL)
			freeSlot->Reset(resolvedAppId, processId);
	} else if (eventType == SHELL_APP_EVENT_ENDED && found != NULL) {
		if (found->overlayPath.size() != 0 &&
		    paf::LocalFile::IsFile(found->overlayPath.c_str())) {
			paf::LocalFile::RemoveFile(found->overlayPath.c_str());
		}
		found->Reset(-1, -1);
		if (m_pendingIpc.appId != -1 && m_pendingIpc.appId == resolvedAppId)
			m_pendingIpc.appExited = true;
	}
	return 0;
}

/* 0x81245CF2 */
int ScreenShotManager::EnableCapture(SceInt32 appId)
{
	AppEntry *entry = FindEntry(appId);
	if (entry == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	entry->captureEnabled = true;
	return 0;
}

/* 0x81245D34 */
int ScreenShotManager::DisableCapture(SceInt32 appId)
{
	AppEntry *entry = FindEntry(appId);
	if (entry == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	entry->captureEnabled = false;
	return 0;
}

/* 0x81245D74 */
int ScreenShotManager::SetOverlayOffset(SceInt32 appId, SceInt32 offsetX, SceInt32 offsetY)
{
	AppEntry *entry = FindEntry(appId);
	if (entry == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;

	if (entry->overlayFileFd != -1) {
		sceIoClose(entry->overlayFileFd);
		entry->overlayFileFd = -1;
	}

	char overlayPath[SCE_SCREENSHOT_MAX_FS_PATH];
	sce_paf_memset(overlayPath, 0, sizeof(overlayPath));
	sce_paf_snprintf(overlayPath, sizeof(overlayPath),
			 "ur0:temp/screenshot/overlay/%08x.jpg", appId);
	entry->overlayPath.assign(overlayPath, sce_paf_strlen(overlayPath));
	entry->overlayOffsetX = offsetX;
	entry->overlayOffsetY = offsetY;
	return 0;
}

/* 0x81245E42 */
int ScreenShotManager::SetParam(SceInt32 appId, const char *photoTitle,
				const char *gameTitle, const char *gameComment,
				SceUInt8 legacyLimitMode)
{
	if (legacyLimitMode != 0) {
		if (photoTitle != NULL &&
		    static_cast<SceInt32>(sce_paf_strlen(photoTitle)) >= kLegacyTitleMaxBytes) {
			return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
		}
		if (gameTitle != NULL &&
		    static_cast<SceInt32>(sce_paf_strlen(gameTitle)) >= kLegacyTitleMaxBytes) {
			return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
		}
		if (!validateUtf8MaxChars(gameComment, kMaxGameCommentChars))
			return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	} else {
		if (!validateUtf8MaxChars(photoTitle, kMaxPhotoTitleChars))
			return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
		if (!validateUtf8MaxChars(gameTitle, kMaxGameTitleChars))
			return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
		if (!validateUtf8MaxChars(gameComment, kMaxGameCommentChars))
			return SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	}

	AppEntry *entry = FindEntry(appId);
	if (entry == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;

	const char *safePhoto = (photoTitle != NULL) ? photoTitle : "";
	const char *safeGame = (gameTitle != NULL) ? gameTitle : "";
	const char *safeComment = (gameComment != NULL) ? gameComment : "";

	entry->photoTitle.assign(safePhoto, sce_paf_strlen(safePhoto));
	entry->gameTitle.assign(safeGame, sce_paf_strlen(safeGame));
	entry->gameComment.assign(safeComment, sce_paf_strlen(safeComment));
	return 0;
}

/* 0x812461BA */
int ScreenShotManager::ReceiveOverlayData(SceInt32 appId, const void *chunkData,
					  SceSize chunkSize, SceInt32 chunkIndex)
{
	AppEntry *entry = FindEntry(appId);
	if (entry == NULL || chunkData == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	if (chunkSize == 0)
		return 0;

	if (chunkIndex == 0) {
		paf::common::SharedPtr<paf::image::Image> image = paf::image::Image::Open(
			const_cast<void *>(chunkData), chunkSize, NULL,
			paf::image::ImageFormat_None);
		if (image.get() == NULL)
			return SCE_SCREENSHOT_ERROR_NOT_SUPPORTED_FORMAT;
		if (image->GetFormat() != paf::image::ImageFormat_Png)
			return SCE_SCREENSHOT_ERROR_NOT_SUPPORTED_FORMAT;

		paf::image::ImageExtent extent = image->GetExtent();
		if (extent.w > static_cast<int32_t>(kMaxOverlayWidth) ||
		    extent.h > static_cast<int32_t>(kMaxOverlayHeight))
			return SCE_SCREENSHOT_ERROR_NOT_SUPPORTED_FORMAT;

		if (!paf::LocalFile::Exists("ur0:temp/screenshot"))
			sceIoMkdir("ur0:temp/screenshot", 6);
		if (!paf::LocalFile::Exists("ur0:temp/screenshot/overlay"))
			sceIoMkdir("ur0:temp/screenshot/overlay", 6);

		char overlayPath[SCE_SCREENSHOT_MAX_FS_PATH];
		sce_paf_snprintf(overlayPath, sizeof(overlayPath),
				 "ur0:temp/screenshot/overlay/%08x.jpg", entry->appId);
		if (entry->overlayFileFd != -1) {
			sceIoClose(entry->overlayFileFd);
			entry->overlayFileFd = -1;
		}

		SceUID fd = sceIoOpen(overlayPath, kOverlayOpenFlags, 6);
		entry->overlayFileFd = fd;
		if (fd < 0) {
			entry->overlayFileFd = -1;
			return SCE_SCREENSHOT_ERROR_INTERNAL;
		}
	}

	if (entry->overlayFileFd != -1) {
		if (sceIoLseek32(entry->overlayFileFd, chunkIndex << 14, SCE_SEEK_SET) < 0 ||
		    sceIoWrite(entry->overlayFileFd, chunkData, chunkSize) < 0) {
			sceIoClose(entry->overlayFileFd);
			entry->overlayFileFd = -1;
			return SCE_SCREENSHOT_ERROR_INTERNAL;
		}
	}
	return 0;
}

/* 0x812464AA */
int ScreenShotManager::EnableOverlay(SceInt32 appId)
{
	AppEntry *entry = FindEntry(appId);
	if (entry == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	entry->overlayEnabled = true;
	return 0;
}

/* 0x812464EC */
int ScreenShotManager::DisableOverlay(SceInt32 appId)
{
	AppEntry *entry = FindEntry(appId);
	if (entry == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	entry->overlayEnabled = false;
	return 0;
}

/* 0x8124652C */
int ScreenShotManager::EnableNotification(SceInt32 appId)
{
	AppEntry *entry = FindEntry(appId);
	if (entry == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	entry->notificationEnabled = true;
	return 0;
}

/* 0x8124656E */
int ScreenShotManager::DisableNotification(SceInt32 appId)
{
	AppEntry *entry = FindEntry(appId);
	if (entry == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	entry->notificationEnabled = false;
	return 0;
}

/* 0x812465AE */
int ScreenShotManager::RequestIpcCapture(IPMI::Session *session, SceUInt32 requestId,
					 SceUInt32 captureMode)
{
	int err = 0;
	if (captureMode >= 2U) {
		err = SCE_SCREENSHOT_ERROR_INVALID_ARGUMENT;
	} else if (!m_mediaMounted) {
		err = SCE_SCREENSHOT_ERROR_MEDIA_NOT_EXIST;
	} else if (m_activeJob.get() != NULL || m_pendingIpc.appId != -1) {
		err = SCE_SCREENSHOT_ERROR_CAPTURE_BUSY;
	} else {
		SceInt32 clientAppId = session->getAppId();
		AppEntry *entry = FindEntry(clientAppId);
		SceAppMgrAppStatus fgStatus;
		queryForegroundStatus(&fgStatus);

		if (entry == NULL)
			err = SCE_SCREENSHOT_ERROR_INTERNAL;
		else if (fgStatus.appId != clientAppId)
			err = SCE_SCREENSHOT_ERROR_CAPTURE_FAILED;
		else if (m_requestCallback == NULL)
			err = SCE_SCREENSHOT_ERROR_INTERNAL;
	}

	if (err != 0) {
		char emptyPath[SCE_SCREENSHOT_MAX_FS_PATH];
		sce_paf_memset(emptyPath, 0, sizeof(emptyPath));
		if (s_ipcServer != NULL)
			s_ipcServer->SendCaptureResponse(session, requestId, err, emptyPath);
		return err;
	}

	m_pendingIpc.Clear();
	m_pendingIpc.appId = session->getAppId();
	m_pendingIpc.captureMode = captureMode;
	m_pendingIpc.session = session;
	m_pendingIpc.requestId = requestId;
	m_requestCallback(m_requestUserData);
	return 0;
}

/* 0x81246740 */
int ScreenShotManager::SetRequestCallback(CaptureRequestFn callback, void *userData)
{
	m_requestUserData = userData;
	m_requestCallback = callback;
	return SCE_SCREENSHOT_ERROR_INTERNAL;
}

/* 0x8124675A */
int ScreenShotManager::CancelIpcCapture(SceInt32 appId)
{
	AppEntry *entry = FindEntry(appId);
	if (entry == NULL)
		return SCE_SCREENSHOT_ERROR_INTERNAL;
	if (m_pendingIpc.appId != -1 && m_pendingIpc.appId == appId)
		m_pendingIpc.canceled = true;
	return 0;
}

/* 0x812467CC */
void ScreenShotManager::SendNotification(SceInt32 appId, const char *exportedPath)
{
	AppEntry *entry = FindEntry(appId);
	if (entry == NULL || !entry->notificationEnabled)
		return;

	int triggerType = (m_pendingIpc.appId != -1) ? 1 : 0;
	AppTitleInfo appInfo;
	if (queryAppTitleInfo(appId, &appInfo) == 0) {
		char *notifyParam = static_cast<char *>(allocMemory(SCE_SCREENSHOT_MAX_FS_PATH));
		if (notifyParam != NULL) {
			sce_paf_snprintf(
				notifyParam, SCE_SCREENSHOT_MAX_FS_PATH,
				"screenshot:event_type=SCREENSHOT_NOTIFICATION&trigger_type=%d&path=%s",
				triggerType, exportedPath);
			notifyParam[SCE_SCREENSHOT_MAX_FS_PATH - 1] = '\0';
			sceAppMgrSendParam(appInfo.titleId.c_str(), notifyParam, 0);
			freeMemory(notifyParam);
		}
	}
}

/* 0x812468E8 */
void ScreenShotManager::OnCaptureJobFinished(int result, SceInt32 appId, const char *exportedPath)
{
	int mappedResult = result;
	int ipcResult = result;

	switch (result) {
	case kPhotoExportErrMediaFull:
		mappedResult = SCE_SCREENSHOT_ERROR_MEDIA_FULL;
		ipcResult = SCE_SCREENSHOT_ERROR_MEDIA_FULL;
		break;
	case kPhotoExportErrMediaNotExist:
		mappedResult = SCE_SCREENSHOT_ERROR_MEDIA_NOT_EXIST;
		ipcResult = SCE_SCREENSHOT_ERROR_MEDIA_NOT_EXIST;
		break;
	case kPhotoExportErrContentFull:
		mappedResult = SCE_SCREENSHOT_ERROR_CONTENT_FULL;
		ipcResult = SCE_SCREENSHOT_ERROR_CONTENT_FULL;
		break;
	default:
		if (result != 0)
			ipcResult = SCE_SCREENSHOT_ERROR_INTERNAL;
		break;
	}

	if (m_completeCallback != NULL) {
		int callbackResult = mappedResult;
		m_completeCallback(&callbackResult, m_completeUserData);
	}

	if (m_pendingIpc.appId != -1 &&
	    !m_pendingIpc.appExited &&
	    s_ipcServer != NULL &&
	    !m_pendingIpc.canceled) {
		s_ipcServer->SendCaptureResponse(m_pendingIpc.session,
						 m_pendingIpc.requestId,
						 ipcResult, exportedPath);
	}

	SendNotification(appId, exportedPath);

	m_completeCallback = NULL;
	m_completeUserData = NULL;
	m_activeJob = paf::common::SharedPtr<paf::job::JobItem>();
	m_pendingIpc.Clear();
}

/* ScreenShotIpcServer (0x81248436 - 0x81248A0F) */

void *ScreenShotIpcServer::operator new(size_t size)
{
	return allocMemory(size);
}

void ScreenShotIpcServer::operator delete(void *ptr)
{
	freeMemory(ptr);
}

ScreenShotIpcServer::ScreenShotIpcServer()
	: m_server(NULL),
	  m_sessionCount(0),
	  m_workBuffer(NULL),
	  m_workBufferSize(0)
{
	for (SceUInt32 idx = 0; idx < kMaxIpmiSessions; ++idx)
		m_sessions[idx] = NULL;
}

/* 0x81248436 */
ScreenShotIpcServer::~ScreenShotIpcServer()
{
	if (m_server != NULL && m_sessionCount == 0 && m_server->destroy() == 0) {
		freeMemory(m_server);
		m_server = NULL;
		freeMemory(m_workBuffer);
		m_workBuffer = NULL;
	}
}

/* 0x81248484 */
int ScreenShotIpcServer::Create()
{
	if (m_server != NULL)
		return kScreenShotErrAlreadyInit;

	IPMI::Server::Config config;
	config.maxRequestSize = kIpmiMaxRequestSize;
	config.maxMessageSize = kIpmiMaxMessageSize;
	config.pEventHandler = this;
	sce_paf_strncpy(config.serviceName, "SceScreenShot", sizeof(config.serviceName));
	config.budget = IPMI::USER_BUDGET;
	config.openLimit = -1;

	m_workBufferSize = IPMI::Server::getWorkBufferSize(&config);
	void *serverMem = allocMemory(kIpmiServerObjectSize);
	m_workBuffer = allocAligned(kIpmiWorkBufferAlign, m_workBufferSize);
	if (serverMem == NULL || m_workBuffer == NULL) {
		freeMemory(serverMem);
		freeMemory(m_workBuffer);
		return kScreenShotErrNoMemory;
	}

	int ret = IPMI::Server::create(&m_server, &config, this, serverMem);
	if (ret != 0) {
		freeMemory(serverMem);
		freeMemory(m_workBuffer);
	}
	return ret;
}

/* 0x81248542 */
int ScreenShotIpcServer::RunDispatcher()
{
	if (m_server == NULL)
		return kScreenShotErrNotInit;
	return m_server->runDispatcher(m_workBuffer, m_workBufferSize);
}

/* 0x81248562 -> 0x81248874 */
int ScreenShotIpcServer::SendCaptureResponse(IPMI::Session *session, SceUInt32 requestId,
					     int result, const char *exportedPath)
{
	IPMI::DataInfo outInfo;
	outInfo.pData = exportedPath;
	outInfo.dataSize = SCE_SCREENSHOT_MAX_FS_PATH;
	return session->respondToAsyncMethodRequest(
		SCREENSHOT_METHOD_GET_CAPTURE_RESULT, requestId, result, &outInfo, 1);
}

/* 0x81248574 -> 0x812488B8 */
void ScreenShotIpcServer::onSyncMethodDispatch(IPMI::Session *session, SceUInt32 methodId,
					       const IPMI::DataInfo *inList, SceUInt32 inCount,
					       IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	(void)inCount;
	int result = kIpmiErrUnsupported;

	switch (methodId) {
	case SCREENSHOT_METHOD_SET_PARAM:
		result = ipcSetParam(
			session,
			static_cast<const char *>(inList[1].pData),
			static_cast<const char *>(inList[2].pData),
			static_cast<const char *>(inList[3].pData),
			*static_cast<const SceUInt8 *>(inList[0].pData));
		session->respondToSyncMethodRequest(result, outList, outCount);
		break;

	case SCREENSHOT_METHOD_SEND_OVERLAY_DATA: {
		const OverlayChunkHeader *hdr =
			static_cast<const OverlayChunkHeader *>(inList[0].pData);
		result = ipcSendOverlayData(session, inList[1].pData, hdr->size, hdr->index);
		session->respondToSyncMethodRequest(result, outList, outCount);
		break;
	}

	case SCREENSHOT_METHOD_SET_OVERLAY_OFFSET: {
		const OverlayOffsetParam *offset =
			static_cast<const OverlayOffsetParam *>(inList[0].pData);
		result = ipcSetOverlayOffset(session, offset->offsetX, offset->offsetY);
		session->respondToSyncMethodRequest(result, outList, outCount);
		break;
	}

	case SCREENSHOT_METHOD_ENABLE:
		result = ipcEnable(session);
		session->respondToSyncMethodRequest(result, outList, outCount);
		break;

	case SCREENSHOT_METHOD_DISABLE:
		result = ipcDisable(session);
		session->respondToSyncMethodRequest(result, outList, outCount);
		break;

	case 0x34560005U:
		result = ipcEnableOverlay(session);
		session->respondToSyncMethodRequest(result, outList, outCount);
		break;

	case 0x34560006U:
		result = ipcDisableOverlay(session);
		session->respondToSyncMethodRequest(result, outList, outCount);
		break;

	case SCREENSHOT_METHOD_ENABLE_NOTIFICATION:
		result = ipcEnableNotification(session);
		session->respondToSyncMethodRequest(result, outList, outCount);
		break;

	case SCREENSHOT_METHOD_DISABLE_NOTIFICATION:
		result = ipcDisableNotification(session);
		session->respondToSyncMethodRequest(result, outList, outCount);
		break;

	case SCREENSHOT_METHOD_CANCEL_CAPTURE:
		result = ipcCancelCapture(session);
		session->respondToSyncMethodRequest(result, outList, outCount);
		break;

	default:
		break;
	}
}

/* 0x81248592 -> 0x812489F0 */
void ScreenShotIpcServer::onAsyncMethodDispatch(IPMI::Session *session, SceUInt32 requestId,
						SceUInt32 methodId, const IPMI::DataInfo *inList,
						SceUInt32 inCount)
{
	(void)inCount;
	if (methodId == SCREENSHOT_METHOD_START_CAPTURE) {
		SceUInt32 mode = *static_cast<const SceUInt32 *>(inList[0].pData);
		ipcStartCapture(session, requestId, mode);
	}
}

/* 0x812485AC */
int ScreenShotIpcServer::onNewClient(IPMI::Server *server,
				     const IPMI::Session::Config *config,
				     const void *param, SceSize paramSize)
{
	(void)param;
	(void)paramSize;
	if (server != m_server || m_sessionCount == kMaxIpmiSessions)
		return SCE_SCREENSHOT_ERROR_INTERNAL;

	++m_sessionCount;
	IPMI::Session **slot = NULL;
	for (SceUInt32 idx = 0; idx < kMaxIpmiSessions; ++idx) {
		if (m_sessions[idx] == NULL) {
			slot = &m_sessions[idx];
			break;
		}
	}

	void *sessionMem = allocMemory(config->bufferSize);
	if (sessionMem == NULL) {
		--m_sessionCount;
		return kScreenShotErrNoMemory;
	}

	if (server->createSession(slot, this, sessionMem) == kIpmiErrSessionFull) {
		freeMemory(sessionMem);
		return kScreenShotErrIpmiConnect;
	}
	return 0;
}

/* 0x81248648 */
void ScreenShotIpcServer::onSessionClosedByClient(IPMI::Session *session)
{
	IPMI::Session **slot = NULL;
	for (SceUInt32 idx = 0; idx < kMaxIpmiSessions; ++idx) {
		if (m_sessions[idx] == session) {
			slot = &m_sessions[idx];
			break;
		}
	}

	session->destroy();
	freeMemory(session);
	if (slot != NULL)
		*slot = NULL;
	--m_sessionCount;
}

void ScreenShotIpcServer::onSessionKilled(IPMI::Session *session)
{
	onSessionClosedByClient(session);
}

} /* namespace screenshot */

/* Top-level shell screenshot APIs (0x81244AF4 - 0x81244C63) */

/* 0x81244AF4 */
int shellScreenshotStart(void)
{
	paf::job::JobQueue::Option queueOption;
	screenshot::s_jobQueue = new paf::job::JobQueue("SceScreenShotJobQueue", &queueOption);

	screenshot::InitParam param;
	param.allocator = GetGlobalHeapAllocator();
	param.threadPriority = screenshot::kScreenShotServerPriority;
	param.threadStackSize = screenshot::kScreenShotServerStackSize;
	param.cpuAffinityMask = 0;
	param.jobQueue = screenshot::s_jobQueue;
	return screenshot::createManager(&param);
}

/* 0x81244B62 */
int shellAppStateMarkForeground(void)
{
	screenshot::ScreenShotManager *manager = screenshot::getManager();
	if (manager == NULL)
		return 0;

	SceAppMgrAppStatus status;
	sce_paf_memset(&status, 0, sizeof(status));
	screenshot::queryAppStatus(screenshot::kAppIdSelf, &status);
	return manager->EnableCapture(status.appId);
}

/* 0x81244BB0 */
int Shell_81244BB0(void)
{
	screenshot::ScreenShotManager *manager = screenshot::getManager();
	if (manager == NULL)
		return 0;

	SceAppMgrAppStatus status;
	sce_paf_memset(&status, 0, sizeof(status));
	screenshot::queryAppStatus(screenshot::kAppIdSelf, &status);
	return manager->DisableCapture(status.appId);
}

/* 0x81244BFE */
int shellScreenshotCapture(void (*callback)(int *result, void *data), void *data)
{
	screenshot::ScreenShotManager *manager = screenshot::getManager();
	if (manager == NULL)
		return screenshot::kScreenShotErrNotInit;
	return manager->StartCapture(callback, data);
}

/* 0x81244C20 */
int shellScreenshotCaptureSurface(paf::intrusive_ptr<paf::graph::Surface> *surface, int flags)
{
	screenshot::ScreenShotManager *manager = screenshot::getManager();
	if (manager == NULL)
		return screenshot::kScreenShotErrNotInit;
	return manager->CaptureToSurface(surface, flags);
}

/* 0x81244C42 */
int shellScreenshotSetRequestCallback(void (*callback)(void *data), void *data)
{
	screenshot::ScreenShotManager *manager = screenshot::getManager();
	if (manager == NULL)
		return screenshot::kScreenShotErrNotInit;
	return manager->SetRequestCallback(callback, data);
}
