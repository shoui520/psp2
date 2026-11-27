/*
 * SceShell livearea_plugin: Music LiveArea banner
 * 3.65-CEX: 0x811AFC90 - 0x811B204C (sti 0x811B1F96)
 */
#include <std/stdio.h>
#include <std/string.h>
#include <common/char_encoding.h>
#include <common/string_util.h>
#include <math/math.h>
#include <timer/timer.h>
#include <widget/core/handler.h>
#include <widget/w_button_base.h>
#include <widget/w_text.h>
#include "livearea_internal.h"

static paf::ui::Text *s_musicTitleText = NULL;			/* 0x81544DBC */
static paf::ui::Text *s_musicSubText = NULL;			/* 0x81544DC0 */
static paf::ui::Widget *s_musicStatusText = NULL;		/* 0x81544DC4 */
static paf::ui::ButtonBase *s_musicPlayBtn = NULL;		/* 0x81544DC8 */
static paf::ui::ButtonBase *s_musicPauseBtn = NULL;		/* 0x81544DCC */
static paf::ui::ButtonBase *s_musicPrevBtn = NULL;		/* 0x81544DD0 */
static paf::ui::ButtonBase *s_musicNextBtn = NULL;		/* 0x81544DD4 */
static SceUInt8 s_musicState = 15;				/* 0x81544DD8 */
static int s_musicSeekCount = 0;				/* 0x81544DDC */
static SceUInt8 s_musicInited = 0;				/* 0x81544DE0 */

static paf::string s_musicCurTitle;				/* 0x815674A0 */
static MusicCoreImposeInfo s_musicInfo;				/* 0x815674B0 */

static void liveareaMusicOnPrev(SceInt32 ev, paf::ui::Handler *handler,
				paf::ui::Event *event, void *userdata);
static void liveareaMusicOnPrevRepeat(SceInt32 ev, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userdata);
static void liveareaMusicOnPlayPause(SceInt32 ev, paf::ui::Handler *handler,
				     paf::ui::Event *event, void *userdata);
static void liveareaMusicOnNext(SceInt32 ev, paf::ui::Handler *handler,
				paf::ui::Event *event, void *userdata);
static void liveareaMusicOnNextRepeat(SceInt32 ev, paf::ui::Handler *handler,
				      paf::ui::Event *event, void *userdata);
static void liveareaMusicOnSeekEnd(SceInt32 ev, paf::ui::Handler *handler,
				   paf::ui::Event *event, void *userdata);
static void liveareaMusicOnDestroy(paf::ui::Widget *widget, void *userdata);
static void liveareaMusicOnTimer(SceInt32 ev, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userdata);

static paf::IDParam makeMusicId(SceUInt32 hash)
{
	return paf::IDParam(hash);
}

static paf::Timer *makeMusicTimer(float duration, int easeFunc)
{
	paf::Timer *timer = new paf::Timer();
	if (timer != NULL)
		timer->InitTimer(duration, easeFunc, 0, -1, NULL);
	return timer;
}

static void addMusicListener(paf::ui::Handler *handler, SceInt32 ev,
			     paf::ui::HandlerCB callback, void *userdata)
{
	paf::ui::EventCBListener *listener = new paf::ui::EventCBListener(callback, userdata);
	handler->AddEventListener(ev, listener, 0);
}

/* 0x811AFC90 */
void liveareaMusicSetup(paf::ui::Widget *root, void *userdata)
{
	if (root == NULL)
		return;

	root->AddDeleteCB(liveareaMusicOnDestroy, NULL);

	paf::ui::Widget *titlePanel = root->FindChild(makeMusicId((SceUInt32)-2030468014), 0);
	s_musicTitleText = (paf::ui::Text *)titlePanel->FindChild(makeMusicId(1727901667U), 0);
	s_musicStatusText = root->FindChild(makeMusicId(1443415368U), 0);

	paf::Plugin *impose = paf::Plugin::Find("impose_plugin");
	if (impose != NULL) {
		LiveAreaImposeInterface *imposeIf =
			(LiveAreaImposeInterface *)impose->GetInterface(1);
		if (imposeIf != NULL)
			imposeIf->setMusicBannerActive(1);
	}

	paf::ui::Widget *subPanel = root->FindChild(makeMusicId(1211511918U), 0);
	s_musicSubText = (paf::ui::Text *)subPanel->FindChild(makeMusicId((SceUInt32)-258362288), 0);

	paf::ui::Widget *prevPanel = root->FindChild(makeMusicId(1252053333U), 0);
	paf::ui::ButtonBase *prevBtn =
		(paf::ui::ButtonBase *)prevPanel->FindChild(makeMusicId((SceUInt32)-1620425510), 0);

	paf::ui::Widget *playPanel = root->FindChild(makeMusicId(1467296697U), 0);
	paf::ui::ButtonBase *playBtn =
		(paf::ui::ButtonBase *)playPanel->FindChild(makeMusicId((SceUInt32)-305312720), 0);

	paf::ui::Widget *pausePanel = root->FindChild(makeMusicId(114483710U), 0);
	paf::ui::ButtonBase *pauseBtn =
		(paf::ui::ButtonBase *)pausePanel->FindChild(makeMusicId((SceUInt32)-442531690), 0);

	paf::ui::Widget *nextPanel = root->FindChild(makeMusicId(703947029U), 0);
	paf::ui::ButtonBase *nextBtn =
		(paf::ui::ButtonBase *)nextPanel->FindChild(makeMusicId(1822848214U), 0);

	prevBtn->EnableRepeat(300, 5);
	addMusicListener(prevBtn, 268435464, liveareaMusicOnPrev, NULL);
	addMusicListener(prevBtn, 268435461, liveareaMusicOnPrevRepeat, NULL);
	addMusicListener(prevBtn, 268435462, liveareaMusicOnSeekEnd, NULL);
	addMusicListener(prevBtn, 262146, liveareaMusicOnSeekEnd, NULL);

	addMusicListener(playBtn, 268435464, liveareaMusicOnPlayPause, NULL);
	addMusicListener(pauseBtn, 268435464, liveareaMusicOnPlayPause, NULL);

	nextBtn->EnableRepeat(300, 5);
	addMusicListener(nextBtn, 268435464, liveareaMusicOnNext, NULL);
	addMusicListener(nextBtn, 268435461, liveareaMusicOnNextRepeat, NULL);
	addMusicListener(nextBtn, 268435462, liveareaMusicOnSeekEnd, NULL);
	addMusicListener(nextBtn, 262146, liveareaMusicOnSeekEnd, NULL);

	s_musicPrevBtn = prevBtn;
	s_musicPlayBtn = playBtn;
	s_musicPauseBtn = pauseBtn;
	s_musicNextBtn = nextBtn;

	paf::math::v4 zeroSize(0.0f, 0.0f, 0.0f, 0.0f);
	s_musicPlayBtn->SetSize(zeroSize, NULL, 65540, 0, 0.0f, NULL, NULL);
	s_musicPauseBtn->SetSize(zeroSize, NULL, 65540, 0, 0.0f, NULL, NULL);
	s_musicPrevBtn->SetSize(zeroSize, NULL, 65540, 0, 0.0f, NULL, NULL);
	s_musicNextBtn->SetSize(zeroSize, NULL, 65540, 0, 0.0f, NULL, NULL);

	paf::ui::EventCBListener *timerListener =
		new paf::ui::EventCBListener(liveareaMusicOnTimer, userdata);
	root->SetTimerListener(33554452, 500.0f, timerListener);

	s_musicPrevBtn->m_pad_tap_on_press = false;
	s_musicNextBtn->m_pad_tap_on_press = false;
	s_musicCurTitle.clear();
	s_musicSeekCount = 0;
	if (!s_musicInited)
		musicCoreSetMute1(0);
	s_musicInited = 1;
}

/* 0x811B0688 */
void liveareaMusicShowPauseState(int pageKey)
{
	s_musicPlayBtn->Enable(false);
	s_musicPauseBtn->Enable(false);
	s_musicPrevBtn->Enable(false);
	s_musicNextBtn->Enable(false);

	paf::math::v4 zeroSize(0.0f, 0.0f, 0.0f, 0.0f);
	paf::math::v4 btnSize(106.0f, 64.0f, 0.0f, 1.0f);

	s_musicPlayBtn->SetSize(zeroSize, makeMusicTimer(200.0f, 21), 65540, 0, 0.0f, NULL, NULL);
	s_musicPauseBtn->SetSize(btnSize, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);
	s_musicPrevBtn->SetSize(btnSize, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);
	s_musicNextBtn->SetSize(btnSize, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);

	s_musicPlayBtn->EnableFocusEvent(false);
	s_musicPauseBtn->EnableFocusEvent(true);
	s_musicPauseBtn->SetFocus(NULL);

	g_liveareaMgr->m_topmenuIf->setPageMusicPlaying(pageKey, 1);
}

/* 0x811B0972 */
void liveareaMusicShowPlayState(int pageKey)
{
	s_musicPlayBtn->Enable(false);
	s_musicPauseBtn->Enable(false);
	s_musicPrevBtn->Enable(false);
	s_musicNextBtn->Enable(false);

	paf::math::v4 zeroSize(0.0f, 0.0f, 0.0f, 0.0f);
	paf::math::v4 btnSize(106.0f, 64.0f, 0.0f, 1.0f);

	s_musicPlayBtn->SetSize(btnSize, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);
	s_musicPauseBtn->SetSize(zeroSize, makeMusicTimer(200.0f, 21), 65540, 0, 0.0f, NULL, NULL);
	s_musicPrevBtn->SetSize(btnSize, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);
	s_musicNextBtn->SetSize(btnSize, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);

	s_musicPlayBtn->EnableFocusEvent(true);
	s_musicPauseBtn->EnableFocusEvent(false);
	s_musicPlayBtn->SetFocus(NULL);

	g_liveareaMgr->m_topmenuIf->setPageMusicPlaying(pageKey, 0);
}

/* 0x811B0C5E */
void liveareaMusicShowDisabledState(int pageKey)
{
	s_musicPlayBtn->Disable(false);
	s_musicPauseBtn->Disable(false);
	s_musicPrevBtn->Disable(false);
	s_musicNextBtn->Disable(false);

	paf::math::v4 zeroSize(0.0f, 0.0f, 0.0f, 0.0f);
	paf::math::v4 btnSize(106.0f, 64.0f, 0.0f, 1.0f);

	s_musicPlayBtn->SetSize(btnSize, makeMusicTimer(200.0f, 21), 65540, 0, 0.0f, NULL, NULL);
	s_musicPauseBtn->SetSize(zeroSize, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);
	s_musicPrevBtn->SetSize(btnSize, makeMusicTimer(200.0f, 21), 65540, 0, 0.0f, NULL, NULL);
	s_musicNextBtn->SetSize(btnSize, makeMusicTimer(200.0f, 21), 65540, 0, 0.0f, NULL, NULL);

	g_liveareaMgr->m_topmenuIf->setPageMusicPlaying(pageKey, 0);
}

/* 0x811B0F0E */
void liveareaMusicCleanup(paf::ui::Widget *root)
{
	if (root == NULL)
		return;

	root->DeleteEventCallback(33554452, NULL, NULL);
	root->RemoveDeleteCB(liveareaMusicOnDestroy, NULL);

	s_musicTitleText = NULL;
	s_musicSubText = NULL;
	s_musicStatusText = NULL;
	s_musicPlayBtn = NULL;
	s_musicPauseBtn = NULL;
	s_musicPrevBtn = NULL;
	s_musicNextBtn = NULL;
	s_musicState = 15;
	s_musicCurTitle.clear();
	s_musicSeekCount = 0;

	paf::IDParam widgetIds[7] = {
		makeMusicId(1443415368U),
		makeMusicId((SceUInt32)-2030468014),
		makeMusicId(1211511918U),
		makeMusicId(1252053333U),
		makeMusicId(1467296697U),
		makeMusicId(114483710U),
		makeMusicId(703947029U)
	};
	for (SceUInt32 i = 0; i < 7; ++i) {
		paf::ui::Widget *child = root->FindChild(widgetIds[i], 0);
		if (child != NULL) {
			child->m_name = paf::IDParam("");
			child->DestroyWidget();
		}
	}
}

/* 0x811B1202 */
void liveareaMusicShowStatusMsg(void)
{
	if (s_musicStatusText == NULL)
		return;

	paf::Plugin *impose = paf::Plugin::Find("impose_plugin");
	if (impose != NULL) {
		SceUInt32 msgId = (s_musicInfo.bgmReason == 2 && s_musicInfo.bgmPriority == 192)
			? (SceUInt32)-1890905394 : (SceUInt32)-1463023626;
		const wchar_t *msgStr = impose->GetString(makeMusicId(msgId));
		paf::wstring msgText(msgStr ? msgStr : L"");
		s_musicStatusText->SetString(msgText);
	}

	paf::math::v4 opaque(1.0f, 1.0f, 1.0f, 1.0f);
	s_musicStatusText->SetColor(opaque, makeMusicTimer(200.0f, 21), 65537, 2, 0.0f, NULL, NULL);

	paf::math::v4 transparent(1.0f, 1.0f, 1.0f, 0.0f);
	s_musicStatusText->SetColor(transparent, makeMusicTimer(2000.0f, 7), 65537, 0, 0.0f, NULL, NULL);
}

/* 0x811B14E8 */
static void liveareaMusicOnPrev(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *)
{
	if (s_musicState == 3) {
		liveareaMusicShowStatusMsg();
		return;
	}
	musicCoreGetImposeInfo(&s_musicInfo);
	if ((SceUInt32)s_musicInfo.posLo >= 1000)
		musicPlayerSeek(0, -1, -1);
	else
		musicCorePlayerControl(4, 0);
}

/* 0x811B1532 */
static void liveareaMusicOnPrevRepeat(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *)
{
	if (s_musicState == 3) {
		liveareaMusicShowStatusMsg();
	} else if (s_musicSeekCount == 0) {
		musicCorePlayerControl(14, 0);
	}
	s_musicSeekCount++;
}

/* 0x811B157E */
static void liveareaMusicOnPlayPause(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *)
{
	if (s_musicState == 0)
		musicCorePlayerControl(1, 0);
	else if (s_musicState <= 2)
		musicCorePlayerControl(0, 0);
	else if (s_musicState <= 3)
		liveareaMusicShowStatusMsg();
}

/* 0x811B15B8 */
static void liveareaMusicOnNext(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *)
{
	if (s_musicState == 3)
		liveareaMusicShowStatusMsg();
	else
		musicCorePlayerControl(3, 0);
}

/* 0x811B15D8 */
static void liveareaMusicOnNextRepeat(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *)
{
	if (s_musicState == 3) {
		liveareaMusicShowStatusMsg();
	} else if (s_musicSeekCount == 0) {
		musicCorePlayerControl(11, 0);
	}
	s_musicSeekCount++;
}

/* 0x811B1624 */
static void liveareaMusicOnSeekEnd(SceInt32, paf::ui::Handler *handler, paf::ui::Event *, void *)
{
	if (s_musicSeekCount != 0) {
		s_musicSeekCount = 0;
		if (s_musicState != 3)
			musicCorePlayerControl(17, 0);
		if (handler != NULL)
			((paf::ui::ButtonBase *)handler)->SetGlowAnim(0, true);
	}
}

/* 0x811B165C */
static void liveareaMusicOnDestroy(paf::ui::Widget *widget, void *userdata)
{
	(void)widget;
	(void)userdata;
	paf::Plugin *impose = paf::Plugin::Find("impose_plugin");
	if (impose != NULL) {
		LiveAreaImposeInterface *imposeIf =
			(LiveAreaImposeInterface *)impose->GetInterface(1);
		if (imposeIf != NULL)
			imposeIf->setMusicBannerActive(0);
	}
	musicCorePlayerControl(1, 0);
	musicCoreSetMute2(0);
	s_musicTitleText = NULL;
	s_musicSubText = NULL;
	s_musicStatusText = NULL;
	s_musicPlayBtn = NULL;
	s_musicPauseBtn = NULL;
	s_musicPrevBtn = NULL;
	s_musicNextBtn = NULL;
	s_musicState = 15;
	s_musicCurTitle.clear();
	s_musicSeekCount = 0;
	s_musicInited = 0;
}

/* 0x811B1706 */
static void liveareaMusicOnTimer(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *userdata)
{
	int pageKey = (int)userdata;
	int infoRes = musicCoreGetImposeInfo(&s_musicInfo);
	paf::Plugin *impose = paf::Plugin::Find("impose_plugin");
	LiveAreaImposeInterface *imposeIf =
		impose ? (LiveAreaImposeInterface *)impose->GetInterface(1) : NULL;

	if (infoRes < 0) {
		liveareaMusicShowDisabledState(pageKey);
		return;
	}

	if (s_musicInfo.canPlay == 1 && imposeIf != NULL && imposeIf->isPsMenuOpen() == 0) {
		int lastErr = 0;
		musicCoreGetLastError(&lastErr);
		if (s_musicStatusText != NULL) {
			const wchar_t *errStr = impose->GetString(makeMusicId(712943388U));
			paf::wstring errText(errStr ? errStr : L"");
			s_musicStatusText->SetString(errText);

			paf::math::v4 opaque(1.0f, 1.0f, 1.0f, 1.0f);
			s_musicStatusText->SetColor(opaque, makeMusicTimer(200.0f, 21), 65537, 2, 0.0f, NULL, NULL);

			paf::math::v4 transparent(1.0f, 1.0f, 1.0f, 0.0f);
			s_musicStatusText->SetColor(transparent, makeMusicTimer(2000.0f, 7), 65537, 0, 0.0f, NULL, NULL);
		}
	}

	if (s_musicInfo.title[0] != '\0') {
		if (s_musicInfo.title[0] != s_musicCurTitle[0] ||
		    sce_paf_strcmp(s_musicInfo.title, s_musicCurTitle.c_str()) != 0) {
			s_musicCurTitle = s_musicInfo.title;
			paf::string utf8Title(s_musicInfo.title);
			paf::wstring wTitle;
			paf::common::Utf8ToUtf16(utf8Title.c_str(), &wTitle);

			if (s_musicStatusText != NULL) {
				paf::math::v4 transparent(1.0f, 1.0f, 1.0f, 0.0f);
				s_musicStatusText->SetColor(transparent, makeMusicTimer(200.0f, 21),
							    65537, 0, 0.0f, NULL, NULL);
			}
			if (s_musicTitleText != NULL) {
				s_musicTitleText->SetString(wTitle);
				s_musicTitleText->StopAutoScroll();
				s_musicTitleText->StartAutoScroll((paf::ui::Text::ScrollType)1);
			}

			paf::string utf8Artist(s_musicInfo.artist);
			paf::wstring wArtist;
			paf::common::Utf8ToUtf16(utf8Artist.c_str(), &wArtist);

			paf::string utf8Album(s_musicInfo.album);
			paf::wstring wAlbum;
			paf::common::Utf8ToUtf16(utf8Album.c_str(), &wAlbum);

			if (impose != NULL) {
				if (wArtist.size() == 0) {
					const wchar_t *fallback =
						impose->GetString(makeMusicId((SceUInt32)-1197122722));
					wArtist = fallback ? fallback : L"";
				}
				if (wAlbum.size() == 0) {
					const wchar_t *fallback =
						impose->GetString(makeMusicId((SceUInt32)-1197122722));
					wAlbum = fallback ? fallback : L"";
				}
			} else {
				if (wArtist.size() == 0)
					wArtist = L"-";
				if (wAlbum.size() == 0)
					wAlbum = L"-";
			}

			if (s_musicSubText != NULL) {
				s_musicSubText->StopAutoScroll();
				paf::wstring combined = wAlbum + L" / " + wArtist;
				s_musicSubText->SetString(combined);
				s_musicSubText->StartAutoScroll((paf::ui::Text::ScrollType)1);
			}
		}

		if (s_musicState != s_musicInfo.playMode) {
			s_musicState = s_musicInfo.playMode;
			if (s_musicInfo.playMode == 0)
				liveareaMusicShowPauseState(pageKey);
			else if (s_musicInfo.playMode < 4)
				liveareaMusicShowPlayState(pageKey);
		}
	} else if (s_musicState != 8) {
		liveareaMusicShowDisabledState(pageKey);
		paf::wstring empty(L"");
		s_musicTitleText->SetString(empty);
		s_musicSubText->StopAutoScroll();
		s_musicSubText->SetString(empty);
		s_musicCurTitle.clear();
		s_musicState = 8;
	}
}
