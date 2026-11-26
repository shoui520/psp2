/*
 * Updater GUI pages and wizard controller
 * (sys/internal/usermode/src/psp2swu/updater_page.cpp).
 */
#include <stddef.h>
#include <std/new>
#include <std/string>
#include <widget/w_scene.h>
#include <widget/w_text.h>
#include "update_gui_view.h"
#include "updater_page.h"

#define PAGE_ID_PROGRESS			0x443F6BEEU
#define PAGE_ID_UPDATE				0x2F3A480DU
#define PAGE_ID_BACKGROUND			0x9573096EU

#define WIDGET_ID_PROGRESS_TITLE		0x35E86C67U
#define WIDGET_ID_UPDATE_TITLE			0xE25D8102U
#define WIDGET_ID_UPDATE_MESSAGE		0x236E9CEBU
#define WIDGET_ID_REMAIN_TIME_TEXT		0x6A203912U

#define STRING_ID_FACTORY_TITLE			0xD5AD2BBAU
#define STRING_ID_FACTORY_MESSAGE		0x6DACBDA5U

ProgressPage::ProgressPage(paf::Plugin *plugin, const paf::IDParam &pageId,
			   const paf::Plugin::PageOpenParam &openParam)
	: WizardPage(plugin, pageId, openParam)
{
}

ProgressPage::~ProgressPage()
{
}

void ProgressPage::OnInit(int32_t eventCode, paf::ui::Handler *handler, paf::ui::Event *event)
{
	if (!UpdateGuiView::GetInstance()->IsFactoryMode())
		return;

	paf::wstring title(m_plugin->GetString(paf::IDParam("", STRING_ID_FACTORY_TITLE)));
	paf::ui::Widget *widget = FindChild(paf::IDParam("", WIDGET_ID_PROGRESS_TITLE), 0);

	if (widget != NULL) {
		paf::ui::Text *textWidget = widget->IsInherit(paf::ui::Text::TypeName()) ?
			static_cast<paf::ui::Text *>(widget) : NULL;
		textWidget->SetString(title);
	}
}

UpdaterMessagePage::UpdaterMessagePage(paf::Plugin *plugin, const paf::IDParam &pageId,
				       const paf::Plugin::PageOpenParam &openParam)
	: WizardPage(plugin, pageId, openParam)
{
}

UpdaterMessagePage::~UpdaterMessagePage()
{
}

void UpdaterMessagePage::OnInit(int32_t eventCode, paf::ui::Handler *handler,
				paf::ui::Event *event)
{
	if (!UpdateGuiView::GetInstance()->IsFactoryMode())
		return;

	paf::wstring text(m_plugin->GetString(paf::IDParam("", STRING_ID_FACTORY_TITLE)));
	paf::ui::Widget *titleWidget = FindChild(paf::IDParam("", WIDGET_ID_UPDATE_TITLE), 0);

	if (titleWidget != NULL) {
		paf::ui::Text *titleText = titleWidget->IsInherit(paf::ui::Text::TypeName()) ?
			static_cast<paf::ui::Text *>(titleWidget) : NULL;
		titleText->SetString(text);
	}

	text = m_plugin->GetString(paf::IDParam("", STRING_ID_FACTORY_MESSAGE));
	paf::ui::Widget *msgWidget = FindChild(paf::IDParam("", WIDGET_ID_UPDATE_MESSAGE), 0);

	if (msgWidget != NULL) {
		paf::ui::Text *msgText = msgWidget->IsInherit(paf::ui::Text::TypeName()) ?
			static_cast<paf::ui::Text *>(msgWidget) : NULL;
		msgText->SetString(text);
	}
}

UpdaterGuiController::UpdaterGuiController()
	: m_plugin(NULL), m_wizard(NULL), m_initialized(false),
	  m_progressPage(NULL), m_updatePage(NULL)
{
}

UpdaterGuiController::~UpdaterGuiController()
{
}

void UpdaterGuiController::OnPageNotify(PageObservable *sender, int32_t eventCode,
					const paf::IDParam &pageId)
{
}

SceInt32 UpdaterGuiController::Initialize(paf::Plugin *plugin)
{
	if (m_initialized)
		return 0;

	m_plugin = plugin;
	m_wizard = PageWizard::GetInstance();

	paf::Plugin::PageOpenParam openParam;

	openParam.overwrite_draw_priority = m_plugin->m_draw_priority;
	m_progressPage = new ProgressPage(m_plugin, paf::IDParam("", PAGE_ID_PROGRESS), openParam);
	m_updatePage = new UpdaterMessagePage(m_plugin, paf::IDParam("", PAGE_ID_UPDATE),
					      openParam);
	m_wizard->RegisterPage(m_progressPage);
	m_wizard->RegisterPage(m_updatePage);
	m_initialized = true;
	return 0;
}

SceInt32 UpdaterGuiController::Finalize()
{
	if (!m_initialized)
		return 0;

	m_wizard->ClearPages();
	if (m_progressPage != NULL)
		delete m_progressPage;
	if (m_updatePage != NULL)
		delete m_updatePage;
	PageWizard::DestroyInstance();
	m_wizard = NULL;
	m_plugin = NULL;
	m_initialized = false;
	return 0;
}

SceInt32 UpdaterGuiController::Start()
{
	if (!m_initialized)
		return -1;

	PageWizard::InitParam initParam;

	initParam.backgroundPageId.m_idhash = PAGE_ID_BACKGROUND;
	m_wizard->Initialize(m_plugin, initParam);
	m_wizard->PushPage(paf::IDParam("", PAGE_ID_PROGRESS), PageWizard::TRANSITION_TURN_FORWARD);
	return 0;
}

SceInt32 UpdaterGuiController::Stop()
{
	if (!m_initialized)
		return -1;

	m_wizard->Finalize();
	return 0;
}

SceInt32 UpdaterGuiController::ShowUpdatePage()
{
	m_wizard->PushPage(paf::IDParam("", PAGE_ID_UPDATE), PageWizard::TRANSITION_TURN_FORWARD);
	return 0;
}

paf::ui::Text *UpdaterGuiController::GetRemainTimeTextWidget()
{
	paf::ui::Scene *root = m_updatePage->GetRoot();

	if (root == NULL)
		return NULL;

	paf::ui::Widget *found = root->FindChild(paf::IDParam("", WIDGET_ID_REMAIN_TIME_TEXT), 0);

	if (found != NULL && found->IsInherit(paf::ui::Text::TypeName()))
		return static_cast<paf::ui::Text *>(
			root->FindChild(paf::IDParam("", WIDGET_ID_REMAIN_TIME_TEXT), 0));
	return NULL;
}
