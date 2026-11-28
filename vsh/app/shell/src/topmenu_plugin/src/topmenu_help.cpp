/*
 * topmenu_help.cpp: first-run tutorial/help dialogs for LiveArea, Home screen,
 * and Edit mode, backed by /CONFIG/SHELL registry keys (0x81294B24-0x81296253).
 */
#include "topmenu_internal.h"
#include <inputdevice/pad/pad.h>

namespace {

enum TopmenuHelpResourceId {
	TOPMENU_HELP_TITLE_ID               = 0x667DC4B6u,
	TOPMENU_HELP_LIVEAREA_TV_LINE_A     = 0x358881F0u,
	TOPMENU_HELP_LIVEAREA_TV_LINE_B     = 0x1B9B3F1Fu,
	TOPMENU_HELP_LIVEAREA_TV_LINE_C     = 0xE4348B47u,
	TOPMENU_HELP_LIVEAREA_VITA_LINE     = 0x9554CE90u,
	TOPMENU_HELP_LIVEAREA_TV_TEMPLATE   = 0x2E7F4EECu,
	TOPMENU_HELP_PEEL_TV_LINE           = 0x91F6296Fu,
	TOPMENU_HELP_PEEL_VITA_LINE         = 0x5A14B68Fu,
	TOPMENU_HELP_PEEL_TEMPLATE          = 0xE5BD4B99u,
	TOPMENU_HELP_HOME_TV_LINE           = 0x9F792A36u,
	TOPMENU_HELP_HOME_VITA_FIRST_LINE   = 0xCEEC6933u,
	TOPMENU_HELP_HOME_VITA_SECOND_LINE  = 0xFAA6232Cu,
	TOPMENU_HELP_HOME_VITA_FIRST_TMPL   = 0xA1822973u,
	TOPMENU_HELP_HOME_COMMON_TMPL       = 0xA29F6398u,
	TOPMENU_HELP_EDIT_LINE_A            = 0x4FA9D3F5u,
	TOPMENU_HELP_EDIT_LINE_B            = 0xE9849052u,
	TOPMENU_HELP_EDIT_LINE_C            = 0x85A17FA2u,
	TOPMENU_HELP_EDIT_LINE_D            = 0xC0BB2CC2u,
	TOPMENU_HELP_EDIT_LINE_E            = 0x4CFAA54Eu,
	TOPMENU_HELP_EDIT_LINE_F            = 0x8A8363EAu
};

/* 0x81294B24..0x81294C02 */
class SetRegKeyJob : public paf::job::JobItem {
public:
	SetRegKeyJob(const char *category, const char *keyName, int value);

	virtual ~SetRegKeyJob() {}

	virtual int32_t Run()
	{
		sceRegMgrSetKeyInt(m_category, m_keyName, m_value);
		return 0;
	}

	virtual void Finish(int32_t result)
	{
		(void)result;
	}

private:
	const char *m_category;
	const char *m_keyName;
	int m_value;
};

/* 0x81294B24 */
SetRegKeyJob::SetRegKeyJob(const char *category, const char *keyName, int value)
	: paf::job::JobItem("SetRegKeyJob", NULL),
	  m_category(category),
	  m_keyName(keyName),
	  m_value(value)
{
}

static int enqueueSetRegKeyJob(paf::common::SharedPtr<paf::job::JobItem> *outJob,
			       const char *category, const char *keyName, int value)
{
	paf::common::SharedPtr<paf::job::JobItem> job(new SetRegKeyJob(category, keyName, value));
	*outJob = job;
	return paf::job::JobQueue::default_queue->Enqueue(*outJob);
}

/* 0x81294C04 */
class TopmenuHelpDialogHandler : public CommonGuiDialogHandler {
public:
	TopmenuHelpDialogHandler(const char *category, const char *keyName, int value)
		: m_category(category), m_keyName(keyName), m_value(value)
	{
	}

	virtual ~TopmenuHelpDialogHandler() {}

	virtual void operator()(SceInt32 dialogId, SceInt32 button)
	{
		(void)button;
		topmenuEndGameHold();
		SceCommonGuiDialog_BC311DC0(dialogId);

		paf::common::SharedPtr<paf::job::JobItem> job;
		enqueueSetRegKeyJob(&job, m_category, m_keyName, m_value);

		if (sce_paf_strncmp(m_keyName, "edit_help", 4) == 0) {
			paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
			if (gamePlugin != NULL) {
				GamePluginInterface *gameIf =
					(GamePluginInterface *)gamePlugin->GetInterface(1);
				if (gameIf != NULL) {
					typedef void (*SetDialogHoldFn)(int hold);
					((SetDialogHoldFn)gameIf->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD])(0);
				}
			}
		}
	}

private:
	const char *m_category;
	const char *m_keyName;
	int m_value;
};

static void focusHelpDialogScrollPart(int dialogId)
{
	paf::ui::Widget *scrollPart = SceCommonGuiDialog_8947FC23(dialogId, 8);
	if (scrollPart != NULL)
		scrollPart->SetFocus(NULL);
}

} /* namespace */

class TopmenuHelpMgr {
public:
	TopmenuHelpMgr();

	void ShowLiveAreaIntroHelp();
	void ShowLiveAreaPeelHelp();
	void ShowHomeHelp();
	void ShowEditHelp();

	int m_liveAreaHelpLevel;
	int m_homeHelpLevel;
	int m_editHelpLevel;
	bool m_liveAreaIntroPending;
	bool m_liveAreaPeelPending;
};

TopmenuHelpMgr *g_topmenuHelpMgr;

/* 0x81294CC4 */
TopmenuHelpMgr::TopmenuHelpMgr()
	: m_liveAreaHelpLevel(0),
	  m_homeHelpLevel(0),
	  m_editHelpLevel(0),
	  m_liveAreaIntroPending(false),
	  m_liveAreaPeelPending(false)
{
	int value = 0;
	if (sceRegMgrGetKeyInt("/CONFIG/SHELL", "livearea_help", &value) == 0)
		m_liveAreaHelpLevel = value;
	if (sceRegMgrGetKeyInt("/CONFIG/SHELL", "home_help", &value) == 0)
		m_homeHelpLevel = value;
	if (sceRegMgrGetKeyInt("/CONFIG/SHELL", "edit_help", &value) == 0)
		m_editHelpLevel = value;
}

/* 0x81294D50 */
void TopmenuHelpMgr::ShowLiveAreaIntroHelp()
{
	if (!m_liveAreaIntroPending || m_liveAreaHelpLevel != 0)
		return;

	m_liveAreaIntroPending = false;
	m_liveAreaHelpLevel = 1;

	CommonGuiListDialogParam param(true);
	param.handler = new TopmenuHelpDialogHandler("/CONFIG/SHELL", "livearea_help",
						     m_liveAreaHelpLevel);
	param.priority = 33;
	param.resourcePlugin = g_topmenuPlugin;
	param.heading = paf::wstring(g_topmenuPlugin->GetString(paf::IDParam(TOPMENU_HELP_TITLE_ID)));

	if (paf::system::Model::GetType() == paf::system::Model::Type_Dolce) {
		SceCommonGuiDialog_D44D1EFB(
			&param, 0, 1,
			paf::wstring(g_topmenuPlugin->GetString(
				paf::IDParam(TOPMENU_HELP_LIVEAREA_TV_LINE_A))));
		SceCommonGuiDialog_D44D1EFB(
			&param, 1, 1,
			paf::wstring(g_topmenuPlugin->GetString(
				paf::IDParam(TOPMENU_HELP_LIVEAREA_TV_LINE_B))));
		SceCommonGuiDialog_D44D1EFB(
			&param, 2, 1,
			paf::wstring(g_topmenuPlugin->GetString(
				paf::IDParam(TOPMENU_HELP_LIVEAREA_TV_LINE_C))));
		param.items[1].mark = true;
		param.items[2].mark = true;
	} else {
		SceCommonGuiDialog_D44D1EFB(
			&param, 0, 1,
			paf::wstring(g_topmenuPlugin->GetString(
				paf::IDParam(TOPMENU_HELP_LIVEAREA_VITA_LINE))));
	}

	int dialogId = SceCommonGuiDialog_BACBA7F2(&param);
	if (dialogId < 0)
		topmenuEndGameHold();

	if (paf::system::Model::GetType() == paf::system::Model::Type_Dolce) {
		paf::ui::Widget *contents = SceCommonGuiDialog_8947FC23(dialogId, 10);
		if (contents != NULL) {
			g_topmenuPlugin->TemplateOpen(
				contents, paf::IDParam(TOPMENU_HELP_LIVEAREA_TV_TEMPLATE),
				paf::Plugin::TemplateOpenParam());
		}
	}

	if (dialogId >= 0)
		focusHelpDialogScrollPart(dialogId);
}

/* 0x81295242 */
void TopmenuHelpMgr::ShowLiveAreaPeelHelp()
{
	if (!m_liveAreaPeelPending || m_liveAreaHelpLevel != 1)
		return;

	m_liveAreaPeelPending = false;
	m_liveAreaHelpLevel = 2;

	CommonGuiListDialogParam param(true);
	param.handler = new TopmenuHelpDialogHandler("/CONFIG/SHELL", "livearea_help",
						     m_liveAreaHelpLevel);
	param.priority = 33;
	param.resourcePlugin = g_topmenuPlugin;
	param.heading = paf::wstring(g_topmenuPlugin->GetString(paf::IDParam(TOPMENU_HELP_TITLE_ID)));

	if (paf::system::Model::GetType() == paf::system::Model::Type_Dolce) {
		const wchar_t circleGlyph[2] = { (wchar_t)0xF880u, 0 };
		const wchar_t crossGlyph[2] = { (wchar_t)0xF881u, 0 };
		const wchar_t *enterGlyph = (ScePafMisc_9CB5D72E() != 0) ? circleGlyph : crossGlyph;
		paf::wstring formatted = shellFormatText(
			g_topmenuPlugin->GetString(paf::IDParam(TOPMENU_HELP_PEEL_TV_LINE)),
			enterGlyph);
		SceCommonGuiDialog_D44D1EFB(&param, 0, 1, formatted);
	} else {
		SceCommonGuiDialog_D44D1EFB(
			&param, 0, 1,
			paf::wstring(g_topmenuPlugin->GetString(
				paf::IDParam(TOPMENU_HELP_PEEL_VITA_LINE))));
	}

	int dialogId = SceCommonGuiDialog_BACBA7F2(&param);
	paf::ui::Widget *contents = SceCommonGuiDialog_8947FC23(dialogId, 10);
	if (contents != NULL) {
		g_topmenuPlugin->TemplateOpen(contents, paf::IDParam(TOPMENU_HELP_PEEL_TEMPLATE),
					      paf::Plugin::TemplateOpenParam());
	}

	if (dialogId >= 0)
		focusHelpDialogScrollPart(dialogId);
	else
		topmenuEndGameHold();
}

/* 0x812955DA */
void TopmenuHelpMgr::ShowHomeHelp()
{
	if (m_homeHelpLevel >= 2)
		return;

	++m_homeHelpLevel;
	if (paf::system::Model::GetType() == paf::system::Model::Type_Dolce)
		m_homeHelpLevel = 2;

	CommonGuiListDialogParam param(true);
	param.priority = 33;
	param.resourcePlugin = g_topmenuPlugin;
	param.handler = new TopmenuHelpDialogHandler("/CONFIG/SHELL", "home_help",
						     m_homeHelpLevel);
	param.heading = paf::wstring(g_topmenuPlugin->GetString(paf::IDParam(TOPMENU_HELP_TITLE_ID)));

	int dialogId = -1;
	if (paf::system::Model::GetType() == paf::system::Model::Type_Dolce) {
		SceCommonGuiDialog_D44D1EFB(
			&param, 0, 1,
			paf::wstring(g_topmenuPlugin->GetString(
				paf::IDParam(TOPMENU_HELP_HOME_TV_LINE))));
		dialogId = SceCommonGuiDialog_BACBA7F2(&param);
		paf::ui::Widget *contents = SceCommonGuiDialog_8947FC23(dialogId, 10);
		if (contents != NULL) {
			g_topmenuPlugin->TemplateOpen(
				contents, paf::IDParam(TOPMENU_HELP_HOME_COMMON_TMPL),
				paf::Plugin::TemplateOpenParam());
		}
	} else if (m_homeHelpLevel == 1) {
		SceCommonGuiDialog_D44D1EFB(
			&param, 0, 1,
			paf::wstring(g_topmenuPlugin->GetString(
				paf::IDParam(TOPMENU_HELP_HOME_VITA_FIRST_LINE))));
		dialogId = SceCommonGuiDialog_BACBA7F2(&param);
		paf::ui::Widget *contents = SceCommonGuiDialog_8947FC23(dialogId, 10);
		if (contents != NULL) {
			g_topmenuPlugin->TemplateOpen(
				contents, paf::IDParam(TOPMENU_HELP_HOME_VITA_FIRST_TMPL),
				paf::Plugin::TemplateOpenParam());
		}
	} else {
		SceCommonGuiDialog_D44D1EFB(
			&param, 0, 1,
			paf::wstring(g_topmenuPlugin->GetString(
				paf::IDParam(TOPMENU_HELP_HOME_VITA_SECOND_LINE))));
		dialogId = SceCommonGuiDialog_BACBA7F2(&param);
		paf::ui::Widget *contents = SceCommonGuiDialog_8947FC23(dialogId, 10);
		if (contents != NULL) {
			g_topmenuPlugin->TemplateOpen(
				contents, paf::IDParam(TOPMENU_HELP_HOME_COMMON_TMPL),
				paf::Plugin::TemplateOpenParam());
		}
	}

	if (dialogId >= 0) {
		topmenuBeginGameHold();
		focusHelpDialogScrollPart(dialogId);
	}
}

/* 0x81295B1A */
void TopmenuHelpMgr::ShowEditHelp()
{
	if (m_editHelpLevel > 0)
		return;

	++m_editHelpLevel;

	paf::Plugin *gamePlugin = paf::Plugin::Find("game_plugin");
	if (gamePlugin != NULL) {
		GamePluginInterface *gameIf =
			(GamePluginInterface *)gamePlugin->GetInterface(1);
		if (gameIf != NULL) {
			typedef void (*SetDialogHoldFn)(int hold);
			((SetDialogHoldFn)gameIf->func[GAME_PLUGIN_FUNC_SET_DIALOG_HOLD])(1);
		}
	}

	CommonGuiListDialogParam param(true);
	param.priority = 33;
	param.resourcePlugin = g_topmenuPlugin;
	param.handler = new TopmenuHelpDialogHandler("/CONFIG/SHELL", "edit_help",
						     m_editHelpLevel);
	param.heading = paf::wstring(g_topmenuPlugin->GetString(paf::IDParam(TOPMENU_HELP_TITLE_ID)));

	SceCommonGuiDialog_D44D1EFB(
		&param, 0, 1,
		paf::wstring(g_topmenuPlugin->GetString(paf::IDParam(TOPMENU_HELP_EDIT_LINE_A))));
	SceCommonGuiDialog_D44D1EFB(
		&param, 1, 1,
		paf::wstring(g_topmenuPlugin->GetString(paf::IDParam(TOPMENU_HELP_EDIT_LINE_B))));
	SceCommonGuiDialog_D44D1EFB(
		&param, 2, 1,
		paf::wstring(g_topmenuPlugin->GetString(paf::IDParam(TOPMENU_HELP_EDIT_LINE_C))));
	SceCommonGuiDialog_D44D1EFB(
		&param, 3, 1,
		paf::wstring(g_topmenuPlugin->GetString(paf::IDParam(TOPMENU_HELP_EDIT_LINE_D))));
	SceCommonGuiDialog_D44D1EFB(
		&param, 4, 1,
		paf::wstring(g_topmenuPlugin->GetString(paf::IDParam(TOPMENU_HELP_EDIT_LINE_E))));
	SceCommonGuiDialog_D44D1EFB(
		&param, 5, 1,
		paf::wstring(g_topmenuPlugin->GetString(paf::IDParam(TOPMENU_HELP_EDIT_LINE_F))));

	param.items[1].mark = true;
	param.items[2].mark = true;
	param.items[3].mark = true;
	param.items[4].mark = true;
	param.items[5].mark = true;

	int dialogId = SceCommonGuiDialog_BACBA7F2(&param);
	if (dialogId >= 0) {
		topmenuBeginGameHold();
		focusHelpDialogScrollPart(dialogId);
	}
}

/* 0x81296108 */
void topmenuHelpInit(void)
{
	g_topmenuHelpMgr = new TopmenuHelpMgr();
}

/* 0x81296128 */
void topmenuHelpTerm(void)
{
	delete g_topmenuHelpMgr;
	g_topmenuHelpMgr = NULL;
}

/* 0x81296148 */
void topmenuHelpPrepareLiveAreaIntro(void)
{
	if (shellStartupIsPmMode() || shellStartupGetIduMode() != 0)
		return;
	if (g_topmenuHelpMgr->m_liveAreaHelpLevel == 0) {
		g_topmenuHelpMgr->m_liveAreaIntroPending = true;
		topmenuBeginGameHold();
	}
}

/* 0x81296172 */
void topmenuHelpShowLiveAreaIntro(void)
{
	if (shellStartupIsPmMode() || shellStartupGetIduMode() != 0)
		return;
	g_topmenuHelpMgr->ShowLiveAreaIntroHelp();
}

/* 0x81296194 */
void topmenuHelpCancelLiveAreaIntro(void)
{
	if (shellStartupIsPmMode() || shellStartupGetIduMode() != 0)
		return;
	if (g_topmenuHelpMgr->m_liveAreaIntroPending &&
	    g_topmenuHelpMgr->m_liveAreaHelpLevel == 0) {
		g_topmenuHelpMgr->m_liveAreaIntroPending = false;
		topmenuEndGameHold();
	}
}

/* 0x812961C2 */
void topmenuHelpPrepareLiveAreaPeel(void)
{
	if (shellStartupIsPmMode() || shellStartupGetIduMode() != 0)
		return;
	if (g_topmenuHelpMgr->m_liveAreaHelpLevel == 1) {
		g_topmenuHelpMgr->m_liveAreaPeelPending = true;
		topmenuBeginGameHold();
	}
}

/* 0x812961EE */
void topmenuHelpShowLiveAreaPeel(void)
{
	if (shellStartupIsPmMode() || shellStartupGetIduMode() != 0)
		return;
	g_topmenuHelpMgr->ShowLiveAreaPeelHelp();
}

/* 0x81296210 */
void topmenuHelpShowHomeHelp(void)
{
	if (shellStartupIsPmMode() || shellStartupGetIduMode() != 0)
		return;
	g_topmenuHelpMgr->ShowHomeHelp();
}

/* 0x81296232 */
void topmenuHelpShowEditHelp(void)
{
	if (shellStartupIsPmMode() || shellStartupGetIduMode() != 0)
		return;
	g_topmenuHelpMgr->ShowEditHelp();
}
