/*
 * The dialog's page in trophysetup_dialog_plugin's resource. The file's name is ours: no Sony
 * path names it.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <timer/timer.h>
#include <widget/w_scene.h>
#include "np_trophy_setup_dialog_plugin.h"

using namespace paf;

static TrophySetupPage s_page;

TrophySetupPage *TrophySetupPage::GetInstance()
{
	return &s_page;
}

TrophySetupPage::TrophySetupPage() : m_plugin(NULL), m_scene(NULL), m_reserved(-1)
{
}

TrophySetupPage::~TrophySetupPage()
{
}

int TrophySetupPage::Open()
{
	Plugin *plugin = Plugin::Find(TROPHY_SETUP_PLUGIN_NAME);

	if (plugin == NULL)
		return -1;
	m_plugin = plugin;
	if (m_scene == NULL) {
		Plugin::PageOpenParam param;

		m_scene = m_plugin->PageOpen(IDParam(s_empty_name, TROPHY_SETUP_PAGE_ID), param);
		/* the parameters' timer goes with them (Sony's code deletes it on both paths) */
		if (m_scene == NULL) {
			if (param.timer != NULL)
				delete param.timer;
			return -1;
		}
		if (param.timer != NULL)
			delete param.timer;
	}
	return 0;
}

void TrophySetupPage::Close()
{
	if (m_scene != NULL)
		m_plugin->PageClose(IDParam(s_empty_name, TROPHY_SETUP_PAGE_ID), Plugin::PageCloseParam());
}
