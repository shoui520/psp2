/*
 * ScePocsDialogPlugin: PocketStation menu UI controller, jobs, and page management.
 */
#include <string.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/string.h>
#include <framework.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <thread/thread.h>
#include <widget/w_scene.h>
#include <widget/w_plane.h>
#include <widget/w_busyindicator.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "pocketstation_menu.h"

using namespace paf;

static PocketStationMenu *s_menuInstance = NULL;
static SceUInt32 s_resultStorage[2] = {0, 0};

class PocketStationGuiDialogHandler : public CommonGuiDialogHandler {
public:
	explicit PocketStationGuiDialogHandler(PocketStationMenu *owner)
		: m_owner(owner)
	{
	}

	virtual ~PocketStationGuiDialogHandler()
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (m_owner != NULL)
			m_owner->HandleGuiDialogButton(value, button);
	}

private:
	PocketStationMenu *m_owner;
};

WaitJob::WaitJob(SceUInt32 waitMs, PocketStationWaitCallback callback, void *userData)
	: job::JobItem("WaitJob", NULL)
	, m_waitMs(waitMs)
	, m_callback(callback)
	, m_userData(userData)
{
}

WaitJob::~WaitJob()
{
}

SceInt32 WaitJob::Run(void)
{
	if (!IsCanceled() && m_waitMs > 0)
		thread::Sleep(m_waitMs);
	return SCE_OK;
}

void WaitJob::Finish(SceInt32 result)
{
	(void)result;
	if (!IsCanceled() && m_callback != NULL)
		m_callback(m_userData);
}

MargeFontAndBgJob::MargeFontAndBgJob(ui::Widget *targetWidget,
	const intrusive_ptr<graph::Surface> &bgSurface,
	const intrusive_ptr<graph::Surface> &fontSurface)
	: job::JobItem("MargeFontAndBgJob", NULL)
	, m_targetWidget(targetWidget)
	, m_bgSurface(bgSurface)
	, m_fontSurface(fontSurface)
{
}

MargeFontAndBgJob::~MargeFontAndBgJob()
{
}

SceInt32 MargeFontAndBgJob::Run(void)
{
	if (IsCanceled())
		return -1;
	return SCE_OK;
}

void MargeFontAndBgJob::Finish(SceInt32 result)
{
	if (!IsCanceled() && result == SCE_OK && m_targetWidget != NULL &&
	    m_bgSurface.get() != NULL)
		m_targetWidget->SetTexture(m_bgSurface);
}

PocketStationMenu::PocketStationMenu()
	: m_closeCb(NULL)
	, m_furlStack(true)
	, m_guiDialogId(-1)
	, m_selectedAction(0)
	, m_selectedIndex(0)
	, m_isOpen(false)
	, m_screenshotSavedNotice(false)
	, m_disconnectedNotice(false)
{
	sce_paf_memset(&m_param, 0, sizeof(m_param));
}

PocketStationMenu::~PocketStationMenu()
{
	CloseGuiDialog();
	m_furlStack.Clear();
	m_pageStack.clear();
}

void PocketStationMenu::CloseGuiDialog(void)
{
	if (m_guiDialogId != -1) {
		SceCommonGuiDialog_BC311DC0(m_guiDialogId);
		m_guiDialogId = -1;
	}
}

ui::Widget *PocketStationMenu::OpenPageTemplate(const char *pageId, const char *templateId)
{
	if (g_pocketStationPlugin == NULL || pageId == NULL)
		return NULL;
	Plugin::PageOpenParam openParam;
	openParam.option = Plugin::PageOption_Create;
	ui::Widget *page = g_pocketStationPlugin->PageOpen(IDParam(pageId), openParam);
	if (page != NULL && templateId != NULL) {
		Plugin::TemplateOpenParam tplParam;
		g_pocketStationPlugin->TemplateOpen(page, IDParam(templateId), tplParam);
	}
	return page;
}

void PocketStationMenu::PopulateTopMenuItems(ui::Widget *listWidget)
{
	if (g_pocketStationPlugin == NULL || listWidget == NULL)
		return;

	static const char *const kItemTemplates[] = {
		"list_item_01_template",
		"list_item_03_template",
		"list_item_03i_template",
		"list_item_04_template",
		"list_item_label_template"
	};

	Plugin::TemplateOpenParam tplParam;
	for (SceSize idx = 0; idx < 5; ++idx)
		g_pocketStationPlugin->TemplateOpen(listWidget, IDParam(kItemTemplates[idx]), tplParam);

	ui::Widget *itemLabel = listWidget->FindChild(IDParam("list_item_label"), 0);
	if (itemLabel != NULL)
		(void)itemLabel->FindChild(IDParam("list_item_label_plane"), 0);

	ui::Widget *itemButton = listWidget->FindChild(IDParam("list_item_button"), 0);
	if (itemButton != NULL) {
		(void)itemButton->FindChild(IDParam("list_item_1st_text"), 0);
		(void)itemButton->FindChild(IDParam("list_item_2nd_text"), 0);
		(void)itemButton->FindChild(IDParam("list_item_3rd_text"), 0);
		(void)itemButton->FindChild(IDParam("list_item_4th_text"), 0);
		(void)itemButton->FindChild(IDParam("list_indicator_on"), 0);
	}
}

void PocketStationMenu::PopulateCorrespondentItems(ui::Widget *listWidget)
{
	if (g_pocketStationPlugin == NULL || listWidget == NULL)
		return;
	Plugin::TemplateOpenParam tplParam;
	g_pocketStationPlugin->TemplateOpen(listWidget, IDParam("list_item_01_template"), tplParam);
	ui::Widget *item = listWidget->FindChild(IDParam("list_item01"), 0);
	if (item != NULL)
		(void)item->FindChild(IDParam("list_item_button"), 0);
}

SceInt32 PocketStationMenu::OpenTopMenu(const PocketStationDialogParam *param,
	PocketStationCloseCallback closeCb)
{
	if (param != NULL)
		m_param = *param;
	m_closeCb = closeCb;
	m_isOpen = true;

	ui::Widget *bgPage = OpenPageTemplate("back_ground_page_id", "list_view_top_menu_template");
	if (bgPage == NULL)
		return -1;

	Plugin::TemplateOpenParam tplParam;
	g_pocketStationPlugin->TemplateOpen(bgPage, IDParam("plane_page_footer_template"), tplParam);

	ui::Widget *header = bgPage->FindChild(IDParam("plane_top_menu_header"), 0);
	ui::Widget *title = bgPage->FindChild(IDParam("text_top_menu_title"), 0);
	ui::Widget *closeBtn = bgPage->FindChild(IDParam("button_close_menu"), 0);
	ui::Widget *backBtn = bgPage->FindChild(IDParam("corner_button_back"), 0);
	ui::Widget *listView = bgPage->FindChild(IDParam("list_view_top_menu"), 0);
	(void)title;
	(void)closeBtn;
	(void)backBtn;

	PopulateTopMenuItems(listView);

	PocketStationMenuPageEntry entry;
	entry.rootWidget = bgPage;
	entry.pageName = "back_ground_page_id";
	entry.headerWidget = header;
	entry.footerWidget = NULL;
	m_pageStack.push_back(entry);
	m_furlStack.Push(bgPage);

	if (m_param.openMode == 1)
		OpenCorrespondentList(true, false);
	else if (m_param.openMode == 2)
		OpenCorrespondentList(true, true);
	else if (m_param.openMode == 3)
		OpenReadManual();

	CheckSystemNotifications();
	return SCE_OK;
}

SceInt32 PocketStationMenu::OpenCorrespondentList(bool directOpen, bool withShortcut)
{
	const char *pageId = "psi_correspondent_list";
	if (directOpen)
		pageId = withShortcut ? "psi_correspondent_list_direct_open_with_short_cut"
				      : "psi_correspondent_list_direct_open";

	ui::Widget *page = OpenPageTemplate(pageId, "list_view_correspondent_list_template");
	if (page == NULL)
		return -1;

	Plugin::TemplateOpenParam tplParam;
	g_pocketStationPlugin->TemplateOpen(page,
		IDParam("correspondent_list_page_header_template"), tplParam);
	PopulateCorrespondentItems(page);

	PocketStationMenuPageEntry entry;
	entry.rootWidget = page;
	entry.pageName = pageId;
	entry.headerWidget = NULL;
	entry.footerWidget = NULL;
	m_pageStack.push_back(entry);
	m_furlStack.Push(page);
	return SCE_OK;
}

SceInt32 PocketStationMenu::OpenReadManual(void)
{
	ui::Widget *page = OpenPageTemplate("psi_read_manual", "read_manual_page_template");
	if (page == NULL)
		return -1;

	(void)page->FindChild(IDParam("manual_area"), 0);
	(void)page->FindChild(IDParam("manual_scrollview"), 0);

	PocketStationMenuPageEntry entry;
	entry.rootWidget = page;
	entry.pageName = "psi_read_manual";
	entry.headerWidget = NULL;
	entry.footerWidget = NULL;
	m_pageStack.push_back(entry);
	m_furlStack.Push(page);
	return SCE_OK;
}

SceInt32 PocketStationMenu::OpenBackgroundSelector(void)
{
	if (g_pocketStationPlugin == NULL || m_pageStack.size() == 0)
		return -1;

	ui::Widget *root = m_pageStack[m_pageStack.size() - 1].rootWidget;
	if (root == NULL)
		return -1;

	ui::Widget *bgBtn = root->FindChild(IDParam("bg_change_btn"), 0);
	ui::Widget *bgText = root->FindChild(IDParam("bg_change_text"), 0);
	(void)bgText;

	intrusive_ptr<graph::Surface> offscreen = g_pocketStationPlugin->GetTexture(
		IDParam("sharedfb_a_OffScreen"));
	intrusive_ptr<graph::Surface> fontTex = g_pocketStationPlugin->GetTexture(
		IDParam("tx_offscreen_text"));

	if (bgBtn != NULL && job::JobQueue::default_queue != NULL) {
		common::SharedPtr<job::JobItem> mergeJob(
			new MargeFontAndBgJob(bgBtn, offscreen, fontTex));
		job::JobQueue::default_queue->Enqueue(mergeJob);
	}
	return SCE_OK;
}

void PocketStationMenu::CloseTopPage(void)
{
	if (m_pageStack.size() == 0)
		return;
	PocketStationMenuPageEntry &top = m_pageStack[m_pageStack.size() - 1];
	m_furlStack.Pop();
	if (g_pocketStationPlugin != NULL && !top.pageName.empty()) {
		Plugin::PageCloseParam closeParam;
		g_pocketStationPlugin->PageClose(IDParam(top.pageName.c_str()), closeParam);
	}
	m_pageStack.pop_back();
}

void PocketStationMenu::CloseAllPages(SceInt32 result)
{
	CloseGuiDialog();
	while (m_pageStack.size() > 0)
		CloseTopPage();
	m_isOpen = false;
	s_resultStorage[0] = m_selectedAction;
	s_resultStorage[1] = m_selectedIndex;
	if (m_closeCb != NULL) {
		PocketStationCloseCallback cb = m_closeCb;
		m_closeCb = NULL;
		cb(result);
	}
}

void PocketStationMenu::ShowScreenshotBusy(bool visible)
{
	if (m_pageStack.size() == 0)
		return;
	ui::Widget *root = m_pageStack[0].rootWidget;
	if (root == NULL)
		return;
	ui::Widget *busy = root->FindChild(IDParam("screenshot_busy_indicator"), 0);
	if (busy != NULL) {
		if (visible)
			busy->Show(0, NULL, NULL, NULL);
		else
			busy->Hide(0, NULL, NULL, NULL);
	}
}

void PocketStationMenu::ShowDisconnectBusy(bool visible)
{
	if (m_pageStack.size() == 0)
		return;
	ui::Widget *root = m_pageStack[0].rootWidget;
	if (root == NULL)
		return;
	ui::Widget *busy = root->FindChild(IDParam("disconnect_busy_indicator"), 0);
	if (busy != NULL) {
		if (visible)
			busy->Show(0, NULL, NULL, NULL);
		else
			busy->Hide(0, NULL, NULL, NULL);
	}
}

void PocketStationMenu::ShowErrorDialog(SceInt32 errorCode)
{
	CloseGuiDialog();
	CommonGuiMessageDialogParam dlgParam;
	dlgParam.type = errorCode;
	dlgParam.handler = new PocketStationGuiDialogHandler(this);
	m_guiDialogId = SceCommonGuiDialog_20B141F1(&dlgParam);
}

void PocketStationMenu::ShowConfirmDialog(const wstring &message, bool yesNo)
{
	CloseGuiDialog();
	CommonGuiLayoutDialogParam dlgParam;
	dlgParam.text = message;
	dlgParam.layout = yesNo ? SceCommonGuiDialog_6CD802FC : SceCommonGuiDialog_AF75CA6D;
	dlgParam.handler = new PocketStationGuiDialogHandler(this);
	m_guiDialogId = SceCommonGuiDialog_CCFCD9A6(&dlgParam);
}

static void OnScreenshotSavedNotice(void *userData)
{
	(void)userData;
}

static void OnDisconnectedNotice(void *userData)
{
	(void)userData;
}

void PocketStationMenu::CheckSystemNotifications(void)
{
	if (m_screenshotSavedNotice) {
		thread::CallQueue::main_thread_callqueue.Enqueue(
			"ScePocsHaveJustSavedScreenshotFromCompatSelf",
			OnScreenshotSavedNotice, this);
		m_screenshotSavedNotice = false;
	}
	if (m_disconnectedNotice) {
		thread::CallQueue::main_thread_callqueue.Enqueue(
			"ScePocsForNoticeDisconnected",
			OnDisconnectedNotice, this);
		m_disconnectedNotice = false;
	}
}

void PocketStationMenu::HandleGuiDialogButton(SceInt32 dialogValue, SceInt32 button)
{
	(void)dialogValue;
	(void)button;
	m_guiDialogId = -1;
}

void PocketStationMenu::ScheduleWait(SceUInt32 waitMs, PocketStationWaitCallback cb,
	void *userData)
{
	if (job::JobQueue::default_queue == NULL)
		return;
	common::SharedPtr<job::JobItem> job(new WaitJob(waitMs, cb, userData));
	job::JobQueue::default_queue->Enqueue(job);
}

SceInt32 PocketStationMenu::Initialize(void)
{
	if (s_menuInstance == NULL)
		s_menuInstance = new PocketStationMenu();
	return SCE_OK;
}

SceInt32 PocketStationMenu::Stop(void)
{
	return SCE_OK;
}

SceInt32 PocketStationMenu::Finalize(void)
{
	if (s_menuInstance != NULL) {
		delete s_menuInstance;
		s_menuInstance = NULL;
	}
	return SCE_OK;
}

SceInt32 PocketStationMenu::Open(const PocketStationDialogParam *param,
	PocketStationCloseCallback closeCb)
{
	if (s_menuInstance == NULL)
		Initialize();
	if (s_menuInstance == NULL)
		return -1;
	return s_menuInstance->OpenTopMenu(param, closeCb);
}

SceInt32 PocketStationMenu::Close(bool isAborted)
{
	if (s_menuInstance != NULL)
		s_menuInstance->CloseAllPages(isAborted ? SCE_COMMON_DIALOG_RESULT_ABORTED
							: SCE_COMMON_DIALOG_RESULT_OK);
	return SCE_OK;
}

SceInt32 PocketStationMenu::Resume(void)
{
	if (s_menuInstance != NULL)
		s_menuInstance->CheckSystemNotifications();
	return SCE_OK;
}

SceInt32 PocketStationMenu::CopyResultData(void *outBuffer)
{
	if (outBuffer != NULL)
		sceClibMemcpy_safe(outBuffer, s_resultStorage, sizeof(s_resultStorage));
	return SCE_OK;
}
