/*
 * updater_plugin callbacks, error dialog presentation, progress bar updates,
 * and remaining-time display (sys/internal/usermode/src/psp2swu/updater_plugin.cpp).
 */
#include <stddef.h>
#include <scetypes.h>
#include <error/error.h>
#include <kernel/processmgr.h>
#include <common/char_encoding.h>
#include <common/encoded_string.h>
#include <common/main_thread_call_list.h>
#include <common/shared_ptr.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <graphics/drawobj/draw_obj.h>
#include <job/job.h>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <std/stdio.h>
#include <std/string>
#include <std/string.h>
#include <versatileparam/idparam.h>
#include <widget/w_busyindicator.h>
#include <widget/w_button.h>
#include <widget/w_button_base.h>
#include <widget/w_progress_bar.h>
#include <widget/w_scene.h>
#include <widget/w_text.h>
#include "../common_gui_dialog/layouts.h"
#include "../../../vsh/cdlg/src/msg_dialog/msg_dialog_util.h"
#include "update_gui_view.h"
#include "updater_page.h"
#include "updater_plugin.h"

#define WIDGET_ID_BUSY_INDICATOR		0xC2D50EE4U
#define WIDGET_ID_PROGRESS_BAR			0x026B1BB7U

#define STRING_ID_ERR_GENERIC			0xC51AA4A4U
#define STRING_ID_ERR_CORRUPT_DATA		0xC8FAD0B6U
#define STRING_ID_ERR_RELEASE_CHECK_MODE	0x8C0A9557U
#define STRING_ID_ERR_INCOMPATIBLE_SYSTEM	0x5E4B250AU
#define STRING_ID_ERR_DIFF_UPDATE		0xF91B07B7U
#define STRING_ID_ERR_RETRY_RESTART		0xE5058F4EU
#define STRING_ID_ERR_FATAL_HEADER		0xCC058FD1U
#define STRING_ID_ERR_FATAL_CONTACT		0x2F0C3670U
#define STRING_ID_ERR_FATAL_POWER_OFF		0xDE678A06U
#define STRING_ID_REMAIN_SECONDS		0x95954687U
#define STRING_ID_REMAIN_MINUTES		0x1F2827C8U
#define STRING_ID_RESTART_BUTTON		0x805BECA6U

#define DIALOG_PART_RESTART_BUTTON		11
#define DIALOG_BUTTON_RESTART			7
#define AUTO_RESTART_TICKS			600
#define REMAIN_SECONDS_THRESHOLD_MS		120000ULL
#define PROGRESS_BAR_MAX_VALUE			1000.0f

#define UPDATER_PHASE_VERIFY			1
#define UPDATER_PHASE_UPDATE			2

SceInt32 g_updaterComplete;

static paf::Plugin *s_plugin;
static paf::ui::ProgressBar *s_progressBar;
static SceInt32 s_restartCountdown;
static paf::ui::BusyIndicator *s_busyIndicator;
static SceInt32 s_updatePhase;
static SceUInt64 s_estimatedUpdateMs;
static SceUInt64 s_remainingMs;
static UpdaterGuiController s_guiController;

class ErrorDialogHandler : public CommonGuiDialogHandler {
public:
	virtual ~ErrorDialogHandler()
	{
	}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		UpdateGuiView::GetInstance()->OnUpdateFinished();
	}
};

class AutoRestartUpdateHandler : public CommonGuiLayoutDialogOpenHandler {
public:
	virtual ~AutoRestartUpdateHandler()
	{
	}

	virtual void operator()(SceInt32 dialogId)
	{
		if (--s_restartCountdown < 0)
			UpdateGuiView::GetInstance()->OnUpdateFinished();
	}
};

static void ShowVerifyErrorDialog(SceInt32 errorCode);
static void ShowDevModeErrorDialog(SceInt32 errorCode);
static void ShowFactoryErrorDialog(SceInt32 errorCode);
static void ShowUpdateErrorDialog(SceInt32 errorCode);

class CallUpdaterMainJob : public paf::job::JobItem {
public:
	CallUpdaterMainJob(const char *name, const Option *option)
		: paf::job::JobItem(name, option)
	{
	}

	virtual ~CallUpdaterMainJob()
	{
	}

	virtual int Run()
	{
		return UpdateGuiView::GetInstance()->RunUpdate();
	}

	virtual void Finish(int result)
	{
		if (s_busyIndicator != NULL) {
			s_busyIndicator->Stop();
			s_busyIndicator = NULL;
		}
		if (result == 0)
			return;
		if (s_updatePhase == UPDATER_PHASE_VERIFY) {
			ShowVerifyErrorDialog(result);
		} else if (UpdateGuiView::GetInstance()->IsDiffUpdate() != 0) {
			ShowDevModeErrorDialog(result);
		} else if (UpdateGuiView::GetInstance()->IsFactoryMode()) {
			ShowFactoryErrorDialog(result);
		} else {
			ShowUpdateErrorDialog(result);
		}
	}
};

static SceInt32 AppendErrorCode(SceInt32 errorCode, paf::wstring *outText)
{
	paf::wstring codeText;
	char externalCode[16];
	char formatted[64];
	SceInt32 ret = sceErrorGetExternalString(externalCode, static_cast<SceUInt32>(errorCode));

	sce_paf_snprintf(formatted, sizeof(formatted), "\n(%s)", externalCode);
	paf::common::Utf8ToUtf16(formatted, &codeText);
	outText->append(codeText);
	return ret;
}

static bool IsCorruptDataError(SceUInt32 code)
{
	if (code == 0x8025BFA6U || code == 0x80180105U || code == 0x80180106U ||
	    code == 0x80180617U)
		return true;
	if (code >= 0x80180210U && code <= 0x801802E7U && (code & 0xFU) == 0x7U)
		return true;
	if (code >= 0x801803F0U && code <= 0x801804CDU) {
		SceUInt32 low = code & 0xFU;

		if (low == 0xDU)
			return true;
		if (low == 0x6U && code != 0x80180406U)
			return true;
	}
	return false;
}

static bool IsIncompatibleSystemError(SceUInt32 code)
{
	if (code == 0x800F0231U || code == 0x800F0237U ||
	    code == 0x80182804U || code == 0x8018280AU)
		return true;
	if (code >= 0x801803F0U && code <= 0x801804CCU) {
		SceUInt32 low = code & 0xFU;

		if (low == 0x7U || low == 0xCU)
			return true;
	}
	return false;
}

static SceInt32 GetErrorMessage(paf::wstring *outText, SceInt32 errorCode)
{
	SceUInt32 code = static_cast<SceUInt32>(errorCode);
	SceUInt32 stringId;

	if (code == 0x80182802U)
		stringId = STRING_ID_ERR_RELEASE_CHECK_MODE;
	else if (IsIncompatibleSystemError(code))
		stringId = STRING_ID_ERR_INCOMPATIBLE_SYSTEM;
	else if (IsCorruptDataError(code))
		stringId = STRING_ID_ERR_CORRUPT_DATA;
	else
		stringId = STRING_ID_ERR_GENERIC;

	*outText = s_plugin->GetString(paf::IDParam("", stringId));
	return AppendErrorCode(errorCode, outText);
}

static inline void FocusRestartButton(SceInt32 dialogId)
{
	if (dialogId < 0)
		return;

	paf::ui::Widget *button = SceCommonGuiDialog_8947FC23(dialogId, DIALOG_PART_RESTART_BUTTON);

	if (button == NULL)
		return;

	button->SetFocus(NULL);

	paf::graph::DrawObj *glow = button->GetDrawObj(paf::ui::Button::OBJ_GLOW);

	if (glow != NULL)
		glow->SetColor(paf::math::v4(0.0f, 108.0f / 255.0f, 1.0f, 1.0f));
	if (button->IsInherit(paf::ui::Button::TypeName()))
		static_cast<paf::ui::Button *>(button)->SetGlowAnim(
			paf::ui::ButtonBase::GLOW_PULSE, false);
}

static void ShowDevModeErrorDialog(SceInt32 errorCode)
{
	if (s_progressBar != NULL)
		s_progressBar->DeleteEventListener(paf::ui::Handler::CB_INTERVAL, NULL);
	s_progressBar = NULL;

	CommonGuiDialogLayout layout = SceCommonGuiDialog_7AEA192C;
	paf::wstring message(s_plugin->GetString(paf::IDParam("", STRING_ID_ERR_DIFF_UPDATE)));

	AppendErrorCode(errorCode, &message);

	paf::wstring emptyHeading;
	SceInt32 dialogId = SceCommonGuiDialog_FA31832E(
		s_plugin, emptyHeading, message, &layout, new ErrorDialogHandler());

	s_restartCountdown = AUTO_RESTART_TICKS;
	SceCommonGuiDialog_17091699(dialogId, new AutoRestartUpdateHandler(), 100.0f);
}

static void ShowUpdateErrorDialog(SceInt32 errorCode)
{
	if (s_progressBar != NULL)
		s_progressBar->DeleteEventListener(paf::ui::Handler::CB_INTERVAL, NULL);
	s_progressBar = NULL;

	CommonGuiDialogLayout layout = SceCommonGuiDialog_7AEA192C;
	SceInt32 retryCount = 0;

	if (UpdateGuiView::GetInstance()->HasUpdateFlagError() != 0) {
		retryCount = 2;
	} else if (UpdateGuiView::GetInstance()->GetRetryCount(&retryCount) != 0) {
		retryCount = 2;
	}

	paf::wstring message;

	if (retryCount < 2) {
		message = s_plugin->GetString(paf::IDParam("", STRING_ID_ERR_RETRY_RESTART));
		AppendErrorCode(errorCode, &message);
	} else {
		wchar_t lineBreak[64];

		message = s_plugin->GetString(paf::IDParam("", STRING_ID_ERR_FATAL_HEADER));
		AppendErrorCode(errorCode, &message);
		sce_paf_swprintf(lineBreak, 64, L"\n\n");
		message.append(lineBreak);
		message.append(s_plugin->GetString(paf::IDParam("", STRING_ID_ERR_FATAL_CONTACT)));
		message.append(lineBreak);
		message.append(s_plugin->GetString(paf::IDParam("", STRING_ID_ERR_FATAL_POWER_OFF)));
	}

	paf::wstring emptyHeading;
	SceInt32 dialogId = SceCommonGuiDialog_FA31832E(
		s_plugin, emptyHeading, message, &layout, new ErrorDialogHandler());

	if (retryCount < 2) {
		s_restartCountdown = AUTO_RESTART_TICKS;
		SceCommonGuiDialog_17091699(dialogId, new AutoRestartUpdateHandler(), 100.0f);
	}
}

static void ShowFactoryErrorDialog(SceInt32 errorCode)
{
	CommonGuiDialogButton buttons[2] = {
		{ DIALOG_PART_RESTART_BUTTON, STRING_ID_RESTART_BUTTON, DIALOG_BUTTON_RESTART, 0 },
		{ 0, 0, 0, 0 }
	};

	if (s_progressBar != NULL)
		s_progressBar->DeleteEventListener(paf::ui::Handler::CB_INTERVAL, NULL);
	s_progressBar = NULL;

	CommonGuiDialogLayout layout = { &g_pageAF75CA6D, g_partsAF75CA6D, buttons };
	paf::wstring message(s_plugin->GetString(paf::IDParam("", STRING_ID_ERR_GENERIC)));

	AppendErrorCode(errorCode, &message);

	paf::wstring emptyHeading;
	SceInt32 dialogId = SceCommonGuiDialog_FA31832E(
		s_plugin, emptyHeading, message, &layout, new ErrorDialogHandler());

	FocusRestartButton(dialogId);
}

static void ShowVerifyErrorDialog(SceInt32 errorCode)
{
	CommonGuiDialogButton buttons[2] = {
		{ DIALOG_PART_RESTART_BUTTON, STRING_ID_RESTART_BUTTON, DIALOG_BUTTON_RESTART, 0 },
		{ 0, 0, 0, 0 }
	};

	if (s_progressBar != NULL)
		s_progressBar->DeleteEventListener(paf::ui::Handler::CB_INTERVAL, NULL);
	s_progressBar = NULL;

	SceInt32 retryCount = 0;

	if (UpdateGuiView::GetInstance()->GetRetryCount(&retryCount) != 0 || retryCount > 0) {
		ShowUpdateErrorDialog(errorCode);
		return;
	}

	CommonGuiDialogLayout layout = { &g_pageAF75CA6D, g_partsAF75CA6D, buttons };
	paf::wstring message;

	GetErrorMessage(&message, errorCode);

	paf::wstring emptyHeading;
	SceInt32 dialogId = SceCommonGuiDialog_FA31832E(
		s_plugin, emptyHeading, message, &layout, new ErrorDialogHandler());

	FocusRestartButton(dialogId);
}

static void UpdateRemainingTimeText()
{
	paf::wstring formattedText;
	paf::ui::Text *remainText = s_guiController.GetRemainTimeTextWidget();
	char numberBuf[256];
	paf::common::EncodedString encoded;

	if (s_remainingMs <= REMAIN_SECONDS_THRESHOLD_MS) {
		SceInt32 seconds = static_cast<SceInt32>(s_remainingMs / 1000ULL);

		sce_paf_snprintf(numberBuf, sizeof(numberBuf), "%d", seconds);
		encoded.Construct(numberBuf, paf::common::EncodedString::Encoding_Utf8);
		formattedText = MsgDialogFormat(
			s_plugin->GetString(paf::IDParam("", STRING_ID_REMAIN_SECONDS)),
			encoded.GetWideString(),
			NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		encoded.Destroy();
		remainText->SetString(formattedText);
	} else {
		SceInt32 minutes = static_cast<SceInt32>(s_remainingMs / 60000ULL);

		sce_paf_snprintf(numberBuf, sizeof(numberBuf), "%d", minutes);
		encoded.Construct(numberBuf, paf::common::EncodedString::Encoding_Utf8);
		formattedText = MsgDialogFormat(
			s_plugin->GetString(paf::IDParam("", STRING_ID_REMAIN_MINUTES)),
			encoded.GetWideString(),
			NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL);
		encoded.Destroy();
		remainText->SetString(formattedText);
	}
}

static void OnProgressUpdate(int32_t eventCode, paf::ui::Handler *handler,
			     paf::ui::Event *event, void *userContext)
{
	paf::ui::ProgressBar *bar = static_cast<paf::ui::ProgressBar *>(handler);

	if (g_updaterComplete != 0) {
		if (bar->GetValue(paf::ui::ProgressBar::BAR_FORE) < PROGRESS_BAR_MAX_VALUE) {
			bar->SetValue(1000, true, paf::ui::ProgressBar::BAR_FORE);
			s_remainingMs = 0;
		}
	} else {
		SceInt32 rate = 0;
		SceUInt64 remainingMs = 0;

		if (UpdateGuiView::GetInstance()->GetProgress(&rate, &remainingMs) != 0)
			return;
		if (bar->GetValue(paf::ui::ProgressBar::BAR_FORE) < PROGRESS_BAR_MAX_VALUE) {
			bar->SetValue(rate * 10, true, paf::ui::ProgressBar::BAR_FORE);
			s_remainingMs = remainingMs;
		}
	}
	UpdateRemainingTimeText();
}

void OnVerifyFinishedMainThread(void *userContext)
{
	if (s_busyIndicator != NULL) {
		s_busyIndicator->Stop();
		s_busyIndicator = NULL;
	}
	s_guiController.ShowUpdatePage();

	paf::ui::Scene *root = s_guiController.GetWizard()->GetCurrentPage()->GetRoot();
	paf::ui::Widget *widget = root->FindChild(paf::IDParam("", WIDGET_ID_PROGRESS_BAR), 0);

	if (widget != NULL && widget->IsInherit(paf::ui::ProgressBar::TypeName())) {
		s_progressBar = static_cast<paf::ui::ProgressBar *>(widget);
		g_updaterComplete = 0;
		UpdateGuiView::GetInstance()->GetEstimateUpdateTime(&s_estimatedUpdateMs);
		s_remainingMs = s_estimatedUpdateMs;
		UpdateRemainingTimeText();
		s_progressBar->SetMaxValue(PROGRESS_BAR_MAX_VALUE);
		s_progressBar->AddEventCallback(paf::ui::Handler::CB_INTERVAL, OnProgressUpdate,
						NULL);
	}
	s_updatePhase = UPDATER_PHASE_UPDATE;
}

static void PowerTickCallback(void *userContext)
{
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
}

void OnUpdaterPluginSetParam(paf::Plugin::InitParam2 *param)
{
}

void OnUpdaterPluginInit(paf::Plugin *plugin)
{
	s_plugin = plugin;
	s_guiController.Initialize(plugin);
	paf::common::MainThreadCallList::Register(PowerTickCallback, NULL);
}

void OnUpdaterPluginStart(paf::Plugin *plugin)
{
	s_guiController.Start();

	paf::ui::Scene *root = s_guiController.GetWizard()->GetCurrentPage()->GetRoot();
	paf::ui::Widget *found = root->FindChild(paf::IDParam("", WIDGET_ID_BUSY_INDICATOR), 0);

	if (found != NULL && found->IsInherit(paf::ui::BusyIndicator::TypeName()))
		s_busyIndicator = static_cast<paf::ui::BusyIndicator *>(
			root->FindChild(paf::IDParam("", WIDGET_ID_BUSY_INDICATOR), 0));
	else
		s_busyIndicator = NULL;

	s_busyIndicator->Start();
	s_updatePhase = UPDATER_PHASE_VERIFY;

	paf::common::SharedPtr<paf::job::JobItem> job(
		new CallUpdaterMainJob("CallUpdaterMain", NULL));

	paf::job::JobQueue::default_queue->Enqueue(job);
}

void OnUpdaterPluginStop(paf::Plugin *plugin)
{
	if (s_progressBar != NULL)
		s_progressBar->DeleteEventListener(paf::ui::Handler::CB_INTERVAL, NULL);
	s_progressBar = NULL;
	s_guiController.Stop();
}

void OnUpdaterPluginExit(paf::Plugin *plugin)
{
	s_guiController.Finalize();
}
