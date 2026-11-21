/*
 * impose_psmenu_bgm.cpp: the PS menu's music player controls (0x811558C0-0x81158652): the
 * playing track's title, album and artist, the play, previous and next buttons (held, they
 * seek) and the volume, all through the shell's music player; while an application owns the
 * BGM port, its name and icon instead. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <math/math.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <common/char_encoding.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_button_base.h>
#include <widget/w_plane.h>
#include <widget/w_plane_button.h>
#include <widget/w_slidebar.h>
#include <widget/w_text.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include "../../livespace_db/livespace_db_internal.h"
#include "../../livespace_db/lsdb_keys.h"
#include "../../applauncher_plugin/src/applauncher.h"
#include "../../../plugins.h"
#include "impose.h"

using namespace paf;

/* the template of the controls and its children (PLACEHOLDER names) */
#define BGM_TEMPLATE		0x8B23313D
#define BGM_CONTROLS		0xE219E0DE	/* the player's controls */
#define BGM_APP_ICON		0xC6B67573	/* the icon of the application playing */
#define BGM_APP_NAME		0x90E6251E
#define BGM_VOLUME		0xDC2C2D38
#define BGM_TITLE		0x66FDAFE3
#define BGM_ALBUM		0xF099B450
#define BGM_PAUSE_ICON		0xE057A7D0
#define BGM_PLAY_ICON		0xB26E4562
#define BGM_PREV_ICON		0xD639AB27
#define BGM_NEXT_ICON		0x6FA104CC
#define BGM_PLAY		0x56734CD1
#define BGM_PREV		0x9F6A44DA
#define BGM_NEXT		0x6CA674D6
#define BGM_ERROR_TEXT		0x5608C548	/* a track the player cannot play */
#define BGM_STR_UNKNOWN		0xB8A55B5E	/* an album or artist the track does not name */

/* the BGM port's owner's priorities: up to the system's, then up to an application's
   (PLACEHOLDER names) */
#define BGM_PRIORITY_SYSTEM	0x80
#define BGM_PRIORITY_APP	0x82

/* the player's states and the state shown before one is known or with no track (PLACEHOLDER
   names) */
#define BGM_STATE_PAUSED	0
#define BGM_STATE_PLAYING	1
#define BGM_STATE_SEEKING	2
#define BGM_STATE_STOPPED	3
#define BGM_STATE_NO_TRACK	8
#define BGM_STATE_UNKNOWN	15

/* the player's commands (Shell_811CBDE6; PLACEHOLDER names) */
#define BGM_COMMAND_PAUSE	0
#define BGM_COMMAND_PLAY	1
#define BGM_COMMAND_NEXT	3
#define BGM_COMMAND_PREV	4
#define BGM_COMMAND_FORWARD	11
#define BGM_COMMAND_REWIND	14
#define BGM_COMMAND_SEEK_END	17

/* the errors of a track the player cannot play (PLACEHOLDER names) */
#define BGM_ERROR_UNSUPPORTED_1	((int)0x80101308)
#define BGM_ERROR_UNSUPPORTED_2	((int)0x80101309)

/* the volume's steps: 1 to 16, the bar's values one less */
#define BGM_VOLUME_MAX		15

/* a button held: repeats begin after 300 ms, every 5 ms */
#define BGM_REPEAT_BEGIN	300
#define BGM_REPEAT_TIME		5
/* the previous button within the first second of a track goes to the previous track */
#define BGM_RESTART_TIME	1000

/* the touch on a button let go (PLACEHOLDER name) */
#define BGM_EVENT_TOUCH_UP	0x40002

/* the controls' animations, in milliseconds */
#define BGM_ANIM_TIME		200.0f
#define BGM_ANIM_TIME_SHORT	160.0f
#define BGM_ICON_SIZE		64.0f

/*
 * PLACEHOLDER names: the shell's music player's state (Shell_811CBF10 fills it; 0x1820 bytes,
 * in the shell's own code): its volume, its state, the track's title, artist and album, an
 * error to read and the time played in milliseconds
 */
struct ShellMusicInfo {
	int volume;
	SceUInt8 state;			/* BGM_STATE_* */
	SceUInt8 reserved5;		/* not read by the menu */
	int reserved8;			/* not read by the menu */
	char title[0x800];
	char artist[0x800];
	char album[0x800];
	SceUInt8 hasError;		/* 1: Shell_811CC17C has an error */
	int reserved1[2];		/* not read by the menu */
	SceUInt32 position;
	int volumeStep;			/* 1 to 16 */
};

/* PLACEHOLDER: the shell's music player (0x811CBCD6-0x811CCD36, in the shell's own code): its
   volume set, a command and a seek given, its state read, its error read, and its updates
   started and stopped */
int Shell_811CBCD6(int volumeStep);
int Shell_811CBD5E(int arg, int start, int end);
int Shell_811CBDE6(int type, int arg);
int Shell_811CBF10(ShellMusicInfo *info);
int Shell_811CC17C(int *error);
int Shell_811CCC40(int arg);
int Shell_811CCD36(int arg);

/* (0x81564FC8) */
static ShellMusicInfo s_musicInfo;

/* (0x811558C0) */
ImposePsMenuBgm::ImposePsMenuBgm(Plugin *plugin)
	: m_plugin(plugin), m_box(NULL), m_title(NULL), m_album(NULL), m_play(NULL), m_prev(NULL),
	  m_next(NULL), m_pauseIcon(NULL), m_playIcon(NULL), m_prevIcon(NULL), m_nextIcon(NULL),
	  m_active(false), m_state(BGM_STATE_UNKNOWN), m_seekCount(0)
{
}

/* (0x811558F4) */
ImposePsMenuBgm::~ImposePsMenuBgm()
{
}

/* the buttons and the volume grayed out, while the menu disables them or nothing plays
   (0x81155918) */
void ImposePsMenuBgm::Disable(bool disable)
{
	AppmgrCurrentBgmState state;
	int result = sceAppMgrGetCurrentBgmState2(&state);

	if (disable || result < 0 || (SceUInt32)state.priority > BGM_PRIORITY_APP) {
		if (m_volume->m_enabled)
			m_volume->Disable();
	} else if (!m_volume->m_enabled) {
		m_volume->Enable();
	}
	if (disable || m_play->IsButtonEnabled()) {
		m_play->Disable(false);
		m_prev->Disable(false);
		m_next->Disable(false);
		m_pauseIcon->SetSize(math::v4(BGM_ICON_SIZE, BGM_ICON_SIZE, 0.0f, 0.0f),
				     new Timer(BGM_ANIM_TIME_SHORT, Timer::FUNC_QUAD_OUT),
				     ui::Widget::ANIM_COLOR, 1);
		m_playIcon->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
				    new Timer(BGM_ANIM_TIME, Timer::FUNC_BACK_OUT),
				    ui::Widget::ANIM_COLOR, 1);
		m_pauseIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.3f),
				      new Timer(BGM_ANIM_TIME_SHORT, Timer::FUNC_QUAD_OUT),
				      ui::Widget::ANIM_COLOR, 1);
		m_playIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.3f),
				     new Timer(BGM_ANIM_TIME_SHORT, Timer::FUNC_QUAD_OUT),
				     ui::Widget::ANIM_COLOR, 1);
		m_prevIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.3f),
				     new Timer(BGM_ANIM_TIME_SHORT, Timer::FUNC_QUAD_OUT),
				     ui::Widget::ANIM_COLOR, 1);
		m_nextIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.3f),
				     new Timer(BGM_ANIM_TIME_SHORT, Timer::FUNC_QUAD_OUT),
				     ui::Widget::ANIM_COLOR, 1);
		m_title->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
				  new Timer(BGM_ANIM_TIME_SHORT, Timer::FUNC_QUAD_OUT),
				  ui::Widget::ANIM_COLOR, 1);
		m_album->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
				  new Timer(BGM_ANIM_TIME_SHORT, Timer::FUNC_QUAD_OUT),
				  ui::Widget::ANIM_COLOR, 1);
	}
}

/* while an application owns the BGM port: its name and its icon in place of the controls
   (0x81155DCA) */
void ImposePsMenuBgm::UpdateApp()
{
	AppmgrCurrentBgmState state;
	SceAppMgrAppStatus status;

	sceAppMgrGetCurrentBgmState2(&state);
	if ((SceUInt32)state.priority <= BGM_PRIORITY_SYSTEM ||
	    (SceUInt32)state.priority > BGM_PRIORITY_APP ||
	    sceAppMgrGetStatusByAppId(state.appId, &status) != 0)
		return;

	LsdbAppInfoPtr app;
	ShellPluginInterface *topmenu = (ShellPluginInterface *)
		applauncherPluginGetInterface("topmenu_plugin", 1);

	if (topmenu != NULL)
		((void (*)(LsdbAppInfoPtr *, const char *))
		 topmenu->func[TOPMENU_PLUGIN_FUNC_GET_APP_INFO])(&app, status.appName);
	if (app.get() == NULL)
		return;

	ui::Widget *name = m_box->FindChild(IDParam(BGM_APP_NAME), 0);

	if (name != NULL) {
		wstring str;

		common::Utf8ToUtf16(string(sceLsdbGetTitle(&app)), &str, 0);
		name->SetString(str);
	}

	ShellPluginInterface *applauncher = (ShellPluginInterface *)
		applauncherPluginGetInterface("applauncher_plugin", 1);

	if (applauncher != NULL) {
		intrusive_ptr<cache::CacheImageInterface> icon;
		ui::Widget *iconWidget = m_box->FindChild(IDParam(BGM_APP_ICON), 0);

		if (iconWidget != NULL) {
			bool found = ((bool (*)(intrusive_ptr<cache::CacheImageInterface> *,
						const string &, void *, void *))applauncher->func[3])(
					&icon, string(sceLsdbGetName(&app)),
					(void *)imposePsMenuOnIcon, iconWidget) &&
				icon.get() != NULL;

			if (found) {
				iconWidget->SetDrawObjMaskBit(&icon, 0);
				iconWidget->Show(0xff, NULL, NULL, 0.0f);
			} else {
				iconWidget->Hide(0xff, NULL, NULL, 0.0f);
			}
		}
		m_box->FindChild(IDParam(BGM_CONTROLS), 0)->Hide(0xff, NULL, NULL, 0.0f);
	}
}

/* the controls opened in the menu's box, the volume from the player (0x811561AA) */
void ImposePsMenuBgm::Open(ui::Widget *box)
{
	m_box = box;
	m_state = BGM_STATE_UNKNOWN;
	m_seekCount = 0;
	m_trackTitle.clear();
	Shell_811CCC40(1);
	m_plugin->TemplateOpen(m_box, IDParam(BGM_TEMPLATE), Plugin::TemplateOpenParam());
	m_box->FindChild(IDParam(BGM_CONTROLS), 0)->Show(0xff, NULL, NULL, 0.0f);
	m_box->FindChild(IDParam(BGM_APP_ICON), 0)->Hide(0xff, NULL, NULL, 0.0f);
	UpdateApp();

	m_volume = PSMENU_FIND(m_box, ui::SlideBar, BGM_VOLUME);
	if (m_volume != NULL) {
		m_volume->AddEventCallback(ui::SlideBar::CB_SLIDEBAR_CHANGED, OnVolume, this);
		m_volume->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		m_volume->SetMaxValue(BGM_VOLUME_MAX);
		Shell_811CBF10(&s_musicInfo);
		m_volume->SetValue(s_musicInfo.volumeStep > 0 ? s_musicInfo.volumeStep - 1 : 0,
				   false);
	}
	m_title = PSMENU_FIND(m_box, ui::Text, BGM_TITLE);
	m_album = PSMENU_FIND(m_box, ui::Text, BGM_ALBUM);
	m_pauseIcon = PSMENU_FIND(m_box, ui::Plane, BGM_PAUSE_ICON);
	m_playIcon = PSMENU_FIND(m_box, ui::Plane, BGM_PLAY_ICON);
	m_prevIcon = PSMENU_FIND(m_box, ui::Plane, BGM_PREV_ICON);
	m_nextIcon = PSMENU_FIND(m_box, ui::Plane, BGM_NEXT_ICON);

	m_play = PSMENU_FIND(m_box, ui::PlaneButton, BGM_PLAY);
	if (m_play != NULL)
		m_play->AddEventCallback(ui::ButtonBase::CB_BTN_DECIDE, OnPlayDecide, this);

	m_prev = PSMENU_FIND(m_box, ui::PlaneButton, BGM_PREV);
	if (m_prev != NULL) {
		m_prev->EnableRepeat(BGM_REPEAT_BEGIN, BGM_REPEAT_TIME);
		m_prev->AddEventCallback(ui::ButtonBase::CB_BTN_DECIDE, OnPrevDecide, this);
		m_prev->AddEventCallback(ui::ButtonBase::CB_BTN_PRESS_REPEAT, OnPrevRepeat, this);
		m_prev->AddEventCallback(ui::ButtonBase::CB_BTN_PRESS_REPEAT_END, OnSeekEnd, this);
		m_prev->AddEventCallback(BGM_EVENT_TOUCH_UP, OnSeekEnd, this);
		m_prev->m_pad_tap_on_press = false;
	}

	m_next = PSMENU_FIND(m_box, ui::PlaneButton, BGM_NEXT);
	if (m_next != NULL) {
		m_next->EnableRepeat(BGM_REPEAT_BEGIN, BGM_REPEAT_TIME);
		m_next->AddEventCallback(ui::ButtonBase::CB_BTN_DECIDE, OnNextDecide, this);
		m_next->AddEventCallback(ui::ButtonBase::CB_BTN_PRESS_REPEAT, OnNextRepeat, this);
		m_next->AddEventCallback(ui::ButtonBase::CB_BTN_PRESS_REPEAT_END, OnSeekEnd, this);
		m_next->AddEventCallback(BGM_EVENT_TOUCH_UP, OnSeekEnd, this);
		m_next->m_pad_tap_on_press = false;
	}
	Disable(true);
}

/* (0x8115756C) */
void ImposePsMenuBgm::Close()
{
	Shell_811CCD36(1);
	m_trackTitle.clear();
	m_seekCount = 0;
	m_box = NULL;
	m_state = BGM_STATE_UNKNOWN;
	m_title = NULL;
	m_album = NULL;
	m_play = NULL;
	m_prev = NULL;
	m_next = NULL;
	m_pauseIcon = NULL;
	m_playIcon = NULL;
	m_prevIcon = NULL;
	m_nextIcon = NULL;
}

/* the buttons enabled, the pause or the play icon shown (0x811575A2) */
void ImposePsMenuBgm::SetPlaying(bool playing)
{
	m_volume->Enable();
	m_play->Enable(false);
	m_prev->Enable(false);
	m_next->Enable(false);
	if (playing) {
		m_pauseIcon->SetSize(math::v4(BGM_ICON_SIZE, BGM_ICON_SIZE, 0.0f, 0.0f),
				     new Timer(BGM_ANIM_TIME, Timer::FUNC_BACK_OUT),
				     ui::Widget::ANIM_SIZE, 1);
		m_playIcon->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
				    new Timer(BGM_ANIM_TIME_SHORT, Timer::FUNC_QUAD_OUT),
				    ui::Widget::ANIM_SIZE, 1);
	} else {
		m_pauseIcon->SetSize(math::v4(0.0f, 0.0f, 0.0f, 0.0f),
				     new Timer(BGM_ANIM_TIME_SHORT, Timer::FUNC_QUAD_OUT),
				     ui::Widget::ANIM_SIZE, 1);
		m_playIcon->SetSize(math::v4(BGM_ICON_SIZE, BGM_ICON_SIZE, 0.0f, 0.0f),
				    new Timer(BGM_ANIM_TIME, Timer::FUNC_BACK_OUT),
				    ui::Widget::ANIM_SIZE, 1);
	}
	m_pauseIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
			      new Timer(BGM_ANIM_TIME, Timer::FUNC_QUAD_OUT),
			      ui::Widget::ANIM_COLOR, 1);
	m_playIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
			     new Timer(BGM_ANIM_TIME, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_COLOR,
			     1);
	m_prevIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
			     new Timer(BGM_ANIM_TIME, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_COLOR,
			     1);
	m_nextIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
			     new Timer(BGM_ANIM_TIME, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_COLOR,
			     1);
	m_title->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
			  new Timer(BGM_ANIM_TIME, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_COLOR, 1);
	m_album->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
			  new Timer(BGM_ANIM_TIME, Timer::FUNC_QUAD_OUT), ui::Widget::ANIM_COLOR, 1);
}

/* every 60 ms while the menu is open: the track and the player's state shown (0x81157ACE) */
void ImposePsMenuBgm::Update()
{
	bool hidden = m_box->FindChild(IDParam(BGM_CONTROLS), 0)->m_show_a_sum < 0.02f;

	if (hidden) {
		Disable(false);
		return;
	}
	if (Shell_811CBF10(&s_musicInfo) < 0) {
		Disable(false);
		return;
	}
	if (s_musicInfo.hasError == 1) {
		int error;

		Shell_811CC17C(&error);
		if (error == BGM_ERROR_UNSUPPORTED_1 || error == BGM_ERROR_UNSUPPORTED_2) {
			ui::Text *text = PSMENU_FIND(m_box, ui::Text, BGM_ERROR_TEXT);

			if (text != NULL) {
				text->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
					       new Timer(400.0f, Timer::FUNC_QUAD_OUT),
					       ui::Widget::ANIM_COLOR, 2);
				text->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
					       new Timer(1600.0f, Timer::FUNC_QUAD_IN),
					       ui::Widget::ANIM_COLOR, 0, 400.0f);
			}
			if (m_title != NULL) {
				m_title->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
						  new Timer(BGM_ANIM_TIME, Timer::FUNC_QUAD_OUT),
						  ui::Widget::ANIM_COLOR, 2);
				m_title->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
						  new Timer(1600.0f, Timer::FUNC_QUAD_OUT),
						  ui::Widget::ANIM_COLOR, 0, 600.0f);
			}
		}
	}
	if (s_musicInfo.title[0] == '\0') {
		if (m_state != BGM_STATE_NO_TRACK) {
			m_state = BGM_STATE_NO_TRACK;
			Disable(false);
		}
		return;
	}
	if (!(s_musicInfo.title == m_trackTitle)) {
		ui::Text *text = PSMENU_FIND(m_box, ui::Text, BGM_ERROR_TEXT);

		if (text != NULL)
			text->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
				       new Timer(100.0f, Timer::FUNC_QUAD_OUT),
				       ui::Widget::ANIM_COLOR, 2);
		m_trackTitle = s_musicInfo.title;

		wstring title;

		common::Utf8ToUtf16(string(s_musicInfo.title), &title, 0);
		if (m_title != NULL) {
			m_title->SetString(title);
			m_title->StopAutoScroll();
			m_title->StartAutoScroll(ui::Text::SCROLL_TYPE_HORIZONTAL);
		}
		if (m_album != NULL) {
			wstring artist;
			wstring album;

			common::Utf8ToUtf16(string(s_musicInfo.artist), &artist, 0);
			common::Utf8ToUtf16(string(s_musicInfo.album), &album, 0);
			if (artist.empty())
				artist = m_plugin->GetString(IDParam(BGM_STR_UNKNOWN));
			if (album.empty())
				album = m_plugin->GetString(IDParam(BGM_STR_UNKNOWN));
			m_album->SetString(album + L" / " + artist);
			m_album->StopAutoScroll();
			m_album->StartAutoScroll(ui::Text::SCROLL_TYPE_HORIZONTAL);
		}
	}
	if (m_active) {
		if (m_state != s_musicInfo.state) {
			m_state = s_musicInfo.state;
			switch (m_state) {
			case BGM_STATE_PAUSED:
				SetPlaying(false);
				break;
			case BGM_STATE_PLAYING:
			case BGM_STATE_SEEKING:
				SetPlaying(true);
				break;
			case BGM_STATE_STOPPED:
				Disable(false);
				break;
			}
		}
	} else {
		Disable(false);
	}
}

/* (0x81158590) */
void ImposePsMenuBgm::SetActive(bool active)
{
	m_active = active;
}

/* the volume bar moved: the player's volume set (0x81158596) */
void ImposePsMenuBgm::OnVolume(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	Shell_811CBCD6(((ImposePsMenuBgm *)data)->m_volume->m_value + 1);
}

/* the play button: played, or paused while it plays (0x811585A6) */
__attribute__((noinline)) void ImposePsMenuBgm::OnPlay(int32_t type, ui::Handler *self,
						       ui::Event *e, void *data)
{
	switch (m_state) {
	case BGM_STATE_PAUSED:
		Shell_811CBDE6(BGM_COMMAND_PLAY, 0);
		break;
	case BGM_STATE_PLAYING:
	case BGM_STATE_SEEKING:
		Shell_811CBDE6(BGM_COMMAND_PAUSE, 0);
		break;
	case BGM_STATE_STOPPED:
		Shell_811CBDE6(BGM_COMMAND_PAUSE, 0);
		break;
	}
}

/* the previous button: the previous track within its first second, else the track's start
   (0x8141B5D8) */
inline void ImposePsMenuBgm::OnPrevDecide(int32_t type, ui::Handler *self, ui::Event *e,
					  void *data)
{
	Shell_811CBF10(&s_musicInfo);
	if (s_musicInfo.position < BGM_RESTART_TIME)
		Shell_811CBDE6(BGM_COMMAND_PREV, 0);
	else
		Shell_811CBD5E(0, -1, -1);
}

/* (0x8141B610) */
inline void ImposePsMenuBgm::OnNextDecide(int32_t type, ui::Handler *self, ui::Event *e,
					  void *data)
{
	Shell_811CBDE6(BGM_COMMAND_NEXT, 0);
}

/* the previous button held: rewound (0x8141B61C) */
inline void ImposePsMenuBgm::OnPrevRepeat(int32_t type, ui::Handler *self, ui::Event *e,
					  void *data)
{
	ImposePsMenuBgm *bgm = (ImposePsMenuBgm *)data;

	if (bgm->m_seekCount == 0)
		Shell_811CBDE6(BGM_COMMAND_REWIND, 0);
	bgm->m_seekCount++;
}

/* the next button held: forwarded (0x8141B634) */
inline void ImposePsMenuBgm::OnNextRepeat(int32_t type, ui::Handler *self, ui::Event *e,
					  void *data)
{
	ImposePsMenuBgm *bgm = (ImposePsMenuBgm *)data;

	if (bgm->m_seekCount == 0)
		Shell_811CBDE6(BGM_COMMAND_FORWARD, 0);
	bgm->m_seekCount++;
}

/* a held button let go: the seek ended, the button's glow off (0x8141B64C) */
inline void ImposePsMenuBgm::OnSeekEnd(int32_t type, ui::Handler *self, ui::Event *e,
				       void *data)
{
	ImposePsMenuBgm *bgm = (ImposePsMenuBgm *)data;

	if (bgm->m_seekCount != 0) {
		bgm->m_seekCount = 0;
		Shell_811CBDE6(BGM_COMMAND_SEEK_END, 0);
		if (self != NULL)
			((ui::ButtonBase *)self)->SetGlowAnim(ui::ButtonBase::GLOW_OFF, true);
	}
}
