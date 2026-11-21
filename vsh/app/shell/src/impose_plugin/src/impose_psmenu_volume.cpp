/*
 * impose_psmenu_volume.cpp: the PS menu's volume slide bar (0x81152FB8-0x81153A18): the master
 * volume set while the slider moves, written to the registry by a job once it is let go, and
 * the bar's up and down keys scrolling the menu. Its text is laid out to 300 pixels in
 * Finnish. The file's and the names are ours
 * (PLACEHOLDER), but for the job's, Sony's.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <plugin.h>
#include <std/string>
#include <math/math.h>
#include <versatileparam/idparam.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/event.h>
#include <widget/w_slidebar.h>
#include <widget/w_check_box.h>
#include <widget/w_scroll_view_base.h>
#include <regmgr/regmgr.h>
#include <av_config/av_config.h>
#include <system_param.h>
#include "../../../shell.h"
#include "impose.h"

using namespace paf;

/* the template of the bar and its children, the menu's scroll view and the touch emulation's
   check box (PLACEHOLDER names) */
#define VOLUME_TEMPLATE			0x84E0F33A
#define VOLUME_BAR			0x2225DDFC
#define VOLUME_ROW			0x6732A935
#define VOLUME_TEXT			0xADBEB9FF
#define VOLUME_SCROLL			0xC5634630
#define VOLUME_TOUCH_CHECK		0xC718FB9D

/* the bar let go (value only: no name known) */
#define VOLUME_EVENT_RELEASE		0x40004

/* the master volume's steps, and the volume when none can be read (value only) */
#define VOLUME_STEPS			30
#define VOLUME_DEFAULT			20

/* the message the error dialog is given (empty; 0x81564F20) */
static wstring s_errorMessage;

/* the master volume written to the registry, its failure told in the error dialog (Sony's
   name) */
class ImposePsMenuMVolRegWrite : public job::JobItem {
public:
	ImposePsMenuMVolRegWrite() : job::JobItem("ImposePsMenuMVolRegWrite", NULL), m_error(0)
	{
	}

	virtual ~ImposePsMenuMVolRegWrite()
	{
	}

	virtual int Run()
	{
		m_error = sceAVConfigWriteMasterVol();
		return 0;
	}

	virtual void Finish(int result)
	{
		if (SCE_ERROR_ISFAILED(m_error) &&
		    SCE_ERROR_FACILITY(m_error) == SHELL_ERROR_FACILITY_REGMGR) {
			int type = SHELL_ERROR_DIALOG_START;

			if (sceRegMgrIsBlueScreen(m_error) != 0)
				type = SHELL_ERROR_DIALOG_BLUE_SCREEN;
			shellErrorDialogShow(type, m_error, &s_errorMessage, 0);
		}
	}

	int m_error;
};

/* (0x81152FB8) */
ImposePsMenuVolume::ImposePsMenuVolume(Plugin *plugin)
	: m_plugin(plugin), m_box(NULL), m_volume(0)
{
}

static void imposePsMenuVolumeOnKey(int32_t type, ui::Handler *self, ui::Event *e, void *data);

/* the bar opened in the menu's box, at the master volume (0x81152FC4) */
void ImposePsMenuVolume::Open(ui::Widget *box)
{
	m_box = box;
	m_plugin->TemplateOpen(box, IDParam(VOLUME_TEMPLATE), Plugin::TemplateOpenParam());
	m_slider = PSMENU_FIND(m_box, ui::SlideBar, VOLUME_BAR);
	if (m_slider != NULL) {
		int volume;

		m_slider->AddEventListener(ui::SlideBar::CB_SLIDEBAR_CHANGED,
			new ImposePsMenuListener<ImposePsMenuVolume>(this,
								     &ImposePsMenuVolume::OnChange),
			false);
		m_slider->AddEventListener(VOLUME_EVENT_RELEASE,
			new ImposePsMenuListener<ImposePsMenuVolume>(this,
								     &ImposePsMenuVolume::OnRelease),
			false);
		m_slider->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		m_slider->SetMaxValue(VOLUME_STEPS);
		m_slider->AddEventListener(PSMENU_EVENT_KEY_UP,
			new ui::EventCBListener(imposePsMenuVolumeOnKey, this), false);
		m_slider->AddEventListener(PSMENU_EVENT_KEY_DOWN,
			new ui::EventCBListener(imposePsMenuVolumeOnKey, this), false);
		volume = VOLUME_DEFAULT;
		sceAVConfigGetMasterVol(&volume);
		m_slider->SetValue(volume, false);
		m_volume = volume;
		if (g_imposePlugin->m_locale == SCE_SYSTEM_PARAM_LANG_FINNISH)
			imposeLayoutText(m_box->FindChild(IDParam(VOLUME_ROW), 0),
					 IDParam(VOLUME_TEXT), 300.0f);
		else
			imposeLayoutText(m_box->FindChild(IDParam(VOLUME_ROW), 0),
					 IDParam(VOLUME_TEXT), 0.0f);
	}
}

/* the volume written to the registry by a job, when it changed (0x811534D4) */
void ImposePsMenuVolume::Save()
{
	int volume = m_slider->m_value;

	if (volume != m_volume) {
		common::SharedPtr<job::JobItem> job(new ImposePsMenuMVolRegWrite);

		job::JobQueue::default_queue->Enqueue(job);
		m_volume = volume;
	}
}

/* (0x811535D2) */
void ImposePsMenuVolume::Close()
{
	if (m_box != NULL) {
		Save();
		m_box = NULL;
	}
}

/* (0x811535E6) */
void ImposePsMenuVolume::OnChange()
{
	sceAVConfigSetMasterVol(m_slider->m_value);
}

/* (0x811535F4) */
void ImposePsMenuVolume::OnRelease()
{
	Save();
}

/* up and down on the bar scroll the menu, down to the touch emulation's check box
   (0x811535FC) */
static void imposePsMenuVolumeOnKey(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	ImposePsMenuVolume *volume = (ImposePsMenuVolume *)data;
	ui::ScrollView *scroll = PSMENU_FIND(volume->m_box->m_scene, ui::ScrollView,
					     VOLUME_SCROLL);

	if (scroll == NULL || e == NULL)
		return;
	if ((uint8_t)e->m_detail == 1) {
		scroll->ScePafWidget_AD706F2A(math::v2(0.0f, 1.0f), NULL);

		ui::CheckBox *check = PSMENU_FIND(volume->m_box, ui::CheckBox, VOLUME_TOUCH_CHECK);

		if (check != NULL && ScePafWidget_DB0FFD33(check))
			check->SetFocus(NULL);
	} else if ((uint8_t)e->m_detail == 2) {
		scroll->ScePafWidget_AD706F2A(math::v2(0.0f, -1.0f), NULL);
	}
}
