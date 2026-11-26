/*
 * Updater GUI pages and wizard controller
 * (sys/internal/usermode/src/psp2swu/updater_page.h):
 * ProgressPage, UpdaterMessagePage, and UpdaterGuiController managing the
 * PageWizard stack for updater_plugin.
 */
#ifndef PSP2SWU_UPDATER_PAGE_H
#define PSP2SWU_UPDATER_PAGE_H

#include <scetypes.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <widget/w_text.h>
#include "../../../vsh/app/common/src/page_wizard/page_wizard.h"

class ProgressPage : public WizardPage {
public:
	ProgressPage(paf::Plugin *plugin, const paf::IDParam &pageId,
		     const paf::Plugin::PageOpenParam &openParam);
	virtual ~ProgressPage();

	virtual void OnInit(int32_t eventCode, paf::ui::Handler *handler, paf::ui::Event *event);
};

class UpdaterMessagePage : public WizardPage {
public:
	UpdaterMessagePage(paf::Plugin *plugin, const paf::IDParam &pageId,
			   const paf::Plugin::PageOpenParam &openParam);
	virtual ~UpdaterMessagePage();

	virtual void OnInit(int32_t eventCode, paf::ui::Handler *handler, paf::ui::Event *event);
};

class UpdaterGuiController : public PageObserver {
public:
	UpdaterGuiController();
	virtual ~UpdaterGuiController();

	virtual void OnPageNotify(PageObservable *sender, int32_t eventCode,
				  const paf::IDParam &pageId);

	SceInt32 Initialize(paf::Plugin *plugin);
	SceInt32 Finalize();
	SceInt32 Start();
	SceInt32 Stop();
	SceInt32 ShowUpdatePage();
	paf::ui::Text *GetRemainTimeTextWidget();
	PageWizard *GetWizard() const { return m_wizard; }

private:
	paf::Plugin *m_plugin;
	PageWizard *m_wizard;
	bool m_initialized;
	ProgressPage *m_progressPage;
	UpdaterMessagePage *m_updatePage;
};

#endif
