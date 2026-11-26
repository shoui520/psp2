/*
 * UpdateCuiView (sys/internal/usermode/src/psp2swu/update_cui_view.cpp):
 * double-buffered CDRAM framebuffer and libdbgfont console output for CUI-mode
 * system software updates.
 */
#include <stddef.h>
#include <scetypes.h>
#include <display/display.h>
#include <kernel/sysmem.h>
#include <kernel/threadmgr.h>
#include <libdbgfont.h>
#include <libkernel/libkernel.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <updater_core/updater_core.h>
#include "../cui_setupper/cui_view.h"
#include "update_cui_view.h"

#define CUI_FB_COUNT			2
#define CUI_FB_WIDTH			480
#define CUI_FB_HEIGHT			272
#define CUI_FB_PITCH			512
#define CUI_FB_ALLOC_SIZE		0x100000U
#define CUI_FB_CLEAR_BYTES		(CUI_FB_PITCH * CUI_FB_HEIGHT * 4)
#define CUI_FB_MEMBLOCK_TYPE		0x09408060U
#define CUI_PROGRESS_BAR_MAX_DOTS	38
#define CUI_PROGRESS_INTERVAL_MS	1000
#define CUI_PHASE_WRITING_FIRMWARE	6
#define CUI_ERROR_NOT_INITIALIZED	((SceInt32)0x80182800)
#define CUI_ERROR_RELEASE_CHECK_MODE	((SceInt32)0x80182802)
#define CUI_COLOR_WHITE			0xFFFFFFFFU
#define CUI_COLOR_NORMAL		0x80C000C0U
#define CUI_COLOR_ERROR			0x8000C0C0U
#define CUI_COLOR_BLACK			0x00000000U

static UpdateCuiView *s_cuiViewInstance;
static SceUID s_frameBufUid[CUI_FB_COUNT];
static void *s_frameBufBase[CUI_FB_COUNT];
static void *s_currentFrameBuf;
static SceUInt32 s_frameBufToggle;
static SceUInt64 s_currentPhase;
static const char *s_statusMessage;
static const char *s_errorMessage;
static SceInt32 s_errorCode;
static SceInt32 s_progressPercent;
static bool s_blinkOff;
static bool s_displayReady;
static char s_progressBar[CUI_PROGRESS_BAR_MAX_DOTS + 2];

static inline void ClearFrameBuffer(void *frameBuf)
{
	SceUInt32 *row = static_cast<SceUInt32 *>(frameBuf);

	for (SceUInt32 rowIdx = 0; rowIdx < CUI_FB_HEIGHT; rowIdx++) {
		SceUInt32 *pixel = row;

		for (SceInt32 group = CUI_FB_WIDTH / 8; group > 0; group--) {
			pixel[0] = 0;
			pixel[1] = 0;
			pixel[2] = 0;
			pixel[3] = 0;
			pixel[4] = 0;
			pixel[5] = 0;
			pixel[6] = 0;
			pixel[7] = 0;
			pixel += 8;
		}
		row += CUI_FB_PITCH;
	}
}

static inline void PresentCurrentFrameBuffer()
{
	SceDisplayFrameBuf frameBuf;

	sce_paf_memset(&frameBuf, 0, sizeof(frameBuf));
	frameBuf.size = sizeof(frameBuf);
	frameBuf.base = s_currentFrameBuf;
	frameBuf.pitch = CUI_FB_PITCH;
	frameBuf.pixelformat = 0;
	frameBuf.width = CUI_FB_WIDTH;
	frameBuf.height = CUI_FB_HEIGHT;
	sceDisplaySetFrameBufInternal(0, 0, &frameBuf, 1);
	sceDisplayWaitVblankStart();
}

UpdateCuiView::UpdateCuiView() : UpdateViewBase()
{
	s_cuiViewInstance = this;
}

UpdateCuiView::~UpdateCuiView()
{
}

SceInt32 UpdateCuiView::Initialize()
{
	sceUpdaterSetProgressCallback(ProgressTimerCallback, CUI_PROGRESS_INTERVAL_MS);

	SceInt32 ret = InitDisplay();

	if (ret < 0) {
		CuiInformUpdateFinished(ret, "init dbgfont failed.", 20);
		return ret;
	}

	ret = OnPhaseStarted(3, 0, "Initializing.");
	if (ret < 0) {
		CuiInformUpdateFinished(ret, "show message failed.", 20);
		ShutdownDisplay();
		return ret;
	}

	CuiSetCallbacks(OnPhaseStarted, OnPhaseProgress, OnPhaseError);
	return ret;
}

SceInt32 UpdateCuiView::Operate()
{
	return ExecuteUpdate();
}

SceInt32 UpdateCuiView::Finalize()
{
	SceInt32 ret = ShutdownDisplay();

	if (ret < 0)
		sceClibPrintf("[WARNING]%s: %x\n", "shutdown dbgfont failed.", ret);
	return ret;
}

SceInt32 UpdateCuiView::OnVerifyFinished()
{
	return 0;
}

SceInt32 UpdateCuiView::OnUpdateFinished()
{
	return 0;
}

void UpdateCuiView::OnReleaseCheckError(SceInt32 errorCode)
{
}

void UpdateCuiView::ProgressTimerCallback(int phase, int step, SceUpdaterProgress progress)
{
	CuiInformUpdateOngoing(static_cast<SceUInt32>(phase), step, progress.rate, NULL, -1);
}

SceInt32 UpdateCuiView::DrawProgressScreen()
{
	void *drawBuf = s_frameBufBase[s_frameBufToggle++ & 1];

	ClearFrameBuffer(drawBuf);

	s_progressBar[0] = '\0';
	if (s_progressPercent > 0) {
		SceInt32 dots = (CUI_PROGRESS_BAR_MAX_DOTS * s_progressPercent) / 100;

		if (dots > CUI_PROGRESS_BAR_MAX_DOTS)
			dots = CUI_PROGRESS_BAR_MAX_DOTS;
		sce_paf_memset(s_progressBar, '.', dots);
		s_progressBar[dots] = '\0';
	}

	char headerText[16] = "System Update\n";
	char confText[20] = "SCE CONFIDENTIAL\n";
	char copyText[52] = "Copyright(C) 2013 Sony Computer Entertainment Inc.\n";

	sceDbgFontPrint(30, 30, CUI_COLOR_WHITE, reinterpret_cast<const SceChar8 *>(headerText));
	sceDbgFontPrint(30, 235, CUI_COLOR_WHITE, reinterpret_cast<const SceChar8 *>(confText));
	sceDbgFontPrint(30, 250, CUI_COLOR_WHITE, reinterpret_cast<const SceChar8 *>(copyText));
	sceDbgFontPrint(90, 85, CUI_COLOR_NORMAL,
			reinterpret_cast<const SceChar8 *>(s_statusMessage));
	sceDbgFontPrint(90, 105, CUI_COLOR_NORMAL,
			reinterpret_cast<const SceChar8 *>(s_progressBar));
	if (s_currentPhase == CUI_PHASE_WRITING_FIRMWARE)
		sceDbgFontPrint(120, 145, CUI_COLOR_NORMAL,
				reinterpret_cast<const SceChar8 *>(
					"Do not turn off the system.\n"));
	sceDbgFontPrint(90, 165, CUI_COLOR_NORMAL,
			reinterpret_cast<const SceChar8 *>(
				"After the installation has completed,\n"));
	sceDbgFontPrint(90, 185, CUI_COLOR_NORMAL,
			reinterpret_cast<const SceChar8 *>(
				"the system will automatically restart.\n"));

	SceDbgFontFrameBufInfo fbInfo;
	SceUInt32 flushIdx = s_frameBufToggle++ & 1;

	sce_paf_memset(&fbInfo, 0, sizeof(fbInfo));
	fbInfo.frameBufAddr = static_cast<SceUChar8 *>(s_frameBufBase[flushIdx]);
	s_currentFrameBuf = fbInfo.frameBufAddr;
	fbInfo.frameBufPitch = CUI_FB_PITCH;
	fbInfo.frameBufPixelformat = 0;
	fbInfo.frameBufWidth = CUI_FB_WIDTH;
	fbInfo.frameBufHeight = CUI_FB_HEIGHT;
	return sceDbgFontFlush(&fbInfo);
}

SceInt32 UpdateCuiView::OnPhaseProgress(SceUInt32 phase, SceInt32 step, SceInt32 progress)
{
	if (!s_displayReady)
		return CUI_ERROR_NOT_INITIALIZED;

	s_progressPercent = progress;

	SceInt32 ret = DrawProgressScreen();

	PresentCurrentFrameBuffer();
	return ret;
}

SceInt32 UpdateCuiView::OnPhaseStarted(SceUInt32 phase, SceInt32 step, const char *message)
{
	if (!s_displayReady)
		return CUI_ERROR_NOT_INITIALIZED;

	ClearFrameBuffer(s_frameBufBase[s_frameBufToggle++ & 1]);
	s_currentPhase = static_cast<SceUInt64>(phase & 0x7FFFFFFFU);
	s_statusMessage = message;
	s_progressPercent = 0;

	SceInt32 ret = DrawProgressScreen();

	PresentCurrentFrameBuffer();
	return ret;
}

SceInt32 UpdateCuiView::ShutdownDisplay()
{
	if (!s_displayReady)
		return 0;

	SceInt32 ret = sceDbgFontExit();

	if (ret >= 0) {
		for (SceInt32 idx = 0; idx < CUI_FB_COUNT; idx++) {
			sce_paf_memset(s_frameBufBase[idx], 0, CUI_FB_CLEAR_BYTES);
			sceKernelFreeMemBlock(s_frameBufUid[idx]);
		}
	}
	s_displayReady = false;
	return ret;
}

SceInt32 UpdateCuiView::DrawErrorScreen()
{
	ClearFrameBuffer(s_frameBufBase[s_frameBufToggle++ & 1]);

	char headerText[16] = "System Update\n";
	char confText[20] = "SCE CONFIDENTIAL\n";
	char copyText[52] = "Copyright(C) 2012 Sony Computer Entertainment Inc.\n";

	sceDbgFontPrint(30, 30, CUI_COLOR_WHITE, reinterpret_cast<const SceChar8 *>(headerText));
	sceDbgFontPrint(30, 235, CUI_COLOR_WHITE, reinterpret_cast<const SceChar8 *>(confText));
	sceDbgFontPrint(30, 250, CUI_COLOR_WHITE, reinterpret_cast<const SceChar8 *>(copyText));

	if (s_blinkOff) {
		sceDbgFontPrint(50, 95, CUI_COLOR_BLACK,
				reinterpret_cast<const SceChar8 *>(">>> ERROR <<<"));
		sceDbgFontPrint(50, 155, CUI_COLOR_BLACK,
				reinterpret_cast<const SceChar8 *>(
					"System will automatically restart in 30 sec.\n"));
	} else {
		sceDbgFontPrint(50, 95, CUI_COLOR_ERROR,
				reinterpret_cast<const SceChar8 *>(">>> ERROR <<<"));
		sceDbgFontPrint(50, 155, CUI_COLOR_ERROR,
				reinterpret_cast<const SceChar8 *>(
					"System will automatically restart in 30 sec.\n"));
	}

	if (s_errorCode == CUI_ERROR_RELEASE_CHECK_MODE) {
		sceDbgFontPrint(50, 115, CUI_COLOR_ERROR,
				reinterpret_cast<const SceChar8 *>(
					"Release check mode is enabled.\n"));
		sceDbgFontPrint(50, 135, CUI_COLOR_ERROR,
				reinterpret_cast<const SceChar8 *>(
					"Please set release check mode to development mode.\n"));
	} else {
		char codeText[16];

		sce_paf_snprintf(codeText, sizeof(codeText), "%08x", s_errorCode);
		sceDbgFontPrint(50, 115, CUI_COLOR_ERROR,
				reinterpret_cast<const SceChar8 *>(s_errorMessage));
		sceDbgFontPrint(50, 135, CUI_COLOR_ERROR,
				reinterpret_cast<const SceChar8 *>(codeText));
	}

	SceDbgFontFrameBufInfo fbInfo;
	SceUInt32 flushIdx = s_frameBufToggle++ & 1;

	sce_paf_memset(&fbInfo, 0, sizeof(fbInfo));
	fbInfo.frameBufAddr = static_cast<SceUChar8 *>(s_frameBufBase[flushIdx]);
	s_currentFrameBuf = fbInfo.frameBufAddr;
	fbInfo.frameBufPitch = CUI_FB_PITCH;
	fbInfo.frameBufPixelformat = 0;
	fbInfo.frameBufWidth = CUI_FB_WIDTH;
	fbInfo.frameBufHeight = CUI_FB_HEIGHT;
	return sceDbgFontFlush(&fbInfo);
}

SceInt32 UpdateCuiView::OnPhaseError(SceInt32 errorCode, const char *message)
{
	if (!s_displayReady)
		return CUI_ERROR_NOT_INITIALIZED;

	ClearFrameBuffer(s_frameBufBase[s_frameBufToggle++ & 1]);
	s_errorMessage = message;
	s_errorCode = errorCode;

	SceInt32 ret = 0;

	for (SceInt32 cycle = 15; cycle > 0; cycle--) {
		s_blinkOff = true;
		DrawErrorScreen();
		s_blinkOff = false;
		PresentCurrentFrameBuffer();
		sceKernelDelayThread(1000000);

		ret = DrawErrorScreen();
		PresentCurrentFrameBuffer();
		sceKernelDelayThread(1000000);
	}

	ShutdownDisplay();
	return ret;
}

SceInt32 UpdateCuiView::InitDisplay()
{
	for (SceUInt32 idx = 0; idx < CUI_FB_COUNT; idx++) {
		void *base = NULL;
		SceUID uid = sceKernelAllocMemBlock("FrameBuffer", CUI_FB_MEMBLOCK_TYPE,
						    CUI_FB_ALLOC_SIZE, NULL);

		s_frameBufUid[idx] = uid;
		if (uid >= 0) {
			if (sceKernelGetMemBlockBase(uid, &base) < 0) {
				sce_paf_printf_dummy();
				base = NULL;
			}
		} else {
			sce_paf_printf_dummy();
			base = NULL;
		}
		s_frameBufBase[idx] = base;
		ClearFrameBuffer(base);
	}

	SceDbgFontConfig config;

	sce_paf_memset(&config, 0, sizeof(config));
	config.fontSize = SCE_DBGFONT_FONTSIZE_DEFAULT;

	SceInt32 ret = sceDbgFontInit(&config);

	s_displayReady = true;
	return ret;
}
