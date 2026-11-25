/*
 * SceShell livearea_plugin: Music LiveArea banner
 * 3.65-CEX: 0x811AFC90 - 0x811B204C (sti 0x811B1F96)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <common/char_encoding.h>
#include <common/string_util.h>
#include <math/math.h>
#include <plugin.h>
#include <timer/timer.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_button_base.h>
#include <widget/w_text.h>

#include "../../livespace_db/lsdb_keys.h"

extern void *g_liveareaMgr;					/* 0x815670A8 */

struct MusicPlaybackInfo {
	SceUInt8 pad0[4];
	SceUInt8 status;					/* +4 (0x815674B4) */
	SceUInt8 substatus;					/* +5 (0x815674B5) */
	SceUInt8 pad1[2];
	int errCode;						/* +8 (0x815674B8) */
	char title[2048];					/* +12 (0x815674BC) */
	char album[2048];					/* +2060 (0x81567CBC) */
	char artist[2048];					/* +4108 (0x815684BC) */
	SceUInt8 active;					/* +6156 (0x81568CBC) */
	SceUInt8 pad2[11];
	SceUInt32 positionMs;					/* +6168 (0x81568CC8) */
};

extern int musicPlayerSeek(int mode, int offset, int flags);		/* 0x811CBD5E */
extern int musicPlayerCmd(int cmd, int arg);			/* 0x811CBDE6 */
extern int musicPlayerGetInfo(MusicPlaybackInfo *info);		/* 0x811CBF10 */
extern int musicPlayerClearFlag(void *out);			/* 0x811CC17C */
extern void musicPlayerInit(int arg);				/* 0x811CCC40 */
extern void musicPlayerTerm(int arg);				/* 0x811CCD36 */

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
static MusicPlaybackInfo s_musicInfo;				/* 0x815674B0 */

struct MusicCbEntry {
	void (*fn)(void);
	void *arg;
};

struct MusicCbList {
	void *alloc;
	MusicCbEntry *data;
	int count;
	int capacity;
};

static void liveareaMusicOnPrev(SceInt32 ev, paf::ui::Handler *h, paf::ui::Event *e, void *userdata);
static void liveareaMusicOnPrevRepeat(SceInt32 ev, paf::ui::Handler *h, paf::ui::Event *e, void *userdata);
static void liveareaMusicOnPlayPause(SceInt32 ev, paf::ui::Handler *h, paf::ui::Event *e, void *userdata);
static void liveareaMusicOnNext(SceInt32 ev, paf::ui::Handler *h, paf::ui::Event *e, void *userdata);
static void liveareaMusicOnNextRepeat(SceInt32 ev, paf::ui::Handler *h, paf::ui::Event *e, void *userdata);
static void liveareaMusicOnSeekEnd(SceInt32 ev, paf::ui::Handler *h, paf::ui::Event *e, void *userdata);
static void liveareaMusicOnDestroy(void);
static void liveareaMusicOnTimer(SceInt32 ev, paf::ui::Handler *h, paf::ui::Event *e, void *userdata);

static paf::IDParam makeMusicId(SceUInt32 hash)
{
	return paf::IDParam(hash);
}

static paf::Timer *makeMusicTimer(float dur, int func)
{
	paf::Timer *t = new paf::Timer();
	if (t)
		t->InitTimer(dur, func, 0, -1, NULL);
	return t;
}

static void addMusicListener(paf::ui::Handler *h, SceInt32 ev, paf::ui::HandlerCB fn, void *ud)
{
	paf::ui::EventCBListener *cb = new paf::ui::EventCBListener(fn, ud);
	h->AddEventListener(ev, cb, 0);
}

/* 0x811AFC90 */
void liveareaMusicSetup(paf::ui::Widget *root, void *userdata)
{
	if (!root)
		return;

	MusicCbList *list = (MusicCbList *)((char *)root + 604);
	if (list->data && list->count < list->capacity) {
		list->data[list->count].fn = liveareaMusicOnDestroy;
		list->data[list->count].arg = NULL;
		list->count++;
	}

	paf::ui::Widget *p1 = root->FindChild(makeMusicId((SceUInt32)-2030468014), 0);
	s_musicTitleText = (paf::ui::Text *)p1->FindChild(makeMusicId(1727901667u), 0);
	s_musicStatusText = root->FindChild(makeMusicId(1443415368u), 0);

	paf::Plugin *impose = paf::Plugin::Find("impose_plugin");
	if (impose) {
		void *iface = impose->GetInterface(1);
		if (iface) {
			typedef void (*SetActiveFn)(int);
			(*(SetActiveFn *)((char *)iface + 40))(1);
		}
	}

	paf::ui::Widget *p2 = root->FindChild(makeMusicId(1211511918u), 0);
	s_musicSubText = (paf::ui::Text *)p2->FindChild(makeMusicId((SceUInt32)-258362288), 0);

	paf::ui::Widget *pPrev = root->FindChild(makeMusicId(1252053333u), 0);
	paf::ui::ButtonBase *prevBtn = (paf::ui::ButtonBase *)pPrev->FindChild(makeMusicId((SceUInt32)-1620425510), 0);

	paf::ui::Widget *pPlay = root->FindChild(makeMusicId(1467296697u), 0);
	paf::ui::ButtonBase *playBtn = (paf::ui::ButtonBase *)pPlay->FindChild(makeMusicId((SceUInt32)-305312720), 0);

	paf::ui::Widget *pPause = root->FindChild(makeMusicId(114483710u), 0);
	paf::ui::ButtonBase *pauseBtn = (paf::ui::ButtonBase *)pPause->FindChild(makeMusicId((SceUInt32)-442531690), 0);

	paf::ui::Widget *pNext = root->FindChild(makeMusicId(703947029u), 0);
	paf::ui::ButtonBase *nextBtn = (paf::ui::ButtonBase *)pNext->FindChild(makeMusicId(1822848214u), 0);

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

	paf::math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
	s_musicPlayBtn->SetSize(zero, NULL, 65540, 0, 0.0f, NULL, NULL);
	s_musicPauseBtn->SetSize(zero, NULL, 65540, 0, 0.0f, NULL, NULL);
	s_musicPrevBtn->SetSize(zero, NULL, 65540, 0, 0.0f, NULL, NULL);
	s_musicNextBtn->SetSize(zero, NULL, 65540, 0, 0.0f, NULL, NULL);

	paf::ui::EventCBListener *tcb = new paf::ui::EventCBListener(liveareaMusicOnTimer, userdata);
	root->SetTimerListener(33554452, 500.0f, tcb);

	*((SceUInt8 *)s_musicPrevBtn + 407) &= ~2u;
	*((SceUInt8 *)s_musicNextBtn + 407) &= ~2u;
	s_musicCurTitle.clear();
	s_musicSeekCount = 0;
	if (!s_musicInited)
		musicPlayerInit(0);
	s_musicInited = 1;
}

/* 0x811B0688 */
void liveareaMusicShowPauseState(void *userdata)
{
	s_musicPlayBtn->Enable(false);
	s_musicPauseBtn->Enable(false);
	s_musicPrevBtn->Enable(false);
	s_musicNextBtn->Enable(false);

	paf::math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
	paf::math::v4 btnSz(106.0f, 64.0f, 0.0f, 1.0f);

	s_musicPlayBtn->SetSize(zero, makeMusicTimer(200.0f, 21), 65540, 0, 0.0f, NULL, NULL);
	s_musicPauseBtn->SetSize(btnSz, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);
	s_musicPrevBtn->SetSize(btnSz, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);
	s_musicNextBtn->SetSize(btnSz, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);

	typedef void (*SetFocusFn)(paf::ui::Widget *, int);
	typedef void (*SetSelFn)(paf::ui::Widget *, int);
	(*(SetFocusFn *)(*(char **)s_musicPlayBtn + 76))(s_musicPlayBtn, 0);
	(*(SetFocusFn *)(*(char **)s_musicPauseBtn + 76))(s_musicPauseBtn, 1);
	(*(SetSelFn *)(*(char **)s_musicPauseBtn + 308))(s_musicPauseBtn, 0);

	void *mgr = *(void **)((char *)g_liveareaMgr + 4);
	typedef void (*NotifyFn)(void *, int);
	(*(NotifyFn *)((char *)mgr + 188))(userdata, 1);
}

/* 0x811B0972 */
void liveareaMusicShowPlayState(void *userdata)
{
	s_musicPlayBtn->Enable(false);
	s_musicPauseBtn->Enable(false);
	s_musicPrevBtn->Enable(false);
	s_musicNextBtn->Enable(false);

	paf::math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
	paf::math::v4 btnSz(106.0f, 64.0f, 0.0f, 1.0f);

	s_musicPlayBtn->SetSize(btnSz, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);
	s_musicPauseBtn->SetSize(zero, makeMusicTimer(200.0f, 21), 65540, 0, 0.0f, NULL, NULL);
	s_musicPrevBtn->SetSize(btnSz, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);
	s_musicNextBtn->SetSize(btnSz, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);

	typedef void (*SetFocusFn)(paf::ui::Widget *, int);
	typedef void (*SetSelFn)(paf::ui::Widget *, int);
	(*(SetFocusFn *)(*(char **)s_musicPlayBtn + 76))(s_musicPlayBtn, 1);
	(*(SetFocusFn *)(*(char **)s_musicPauseBtn + 76))(s_musicPauseBtn, 0);
	(*(SetSelFn *)(*(char **)s_musicPlayBtn + 308))(s_musicPlayBtn, 0);

	void *mgr = *(void **)((char *)g_liveareaMgr + 4);
	typedef void (*NotifyFn)(void *, int);
	(*(NotifyFn *)((char *)mgr + 188))(userdata, 0);
}

/* 0x811B0C5E */
void liveareaMusicShowDisabledState(void *userdata)
{
	s_musicPlayBtn->Disable(false);
	s_musicPauseBtn->Disable(false);
	s_musicPrevBtn->Disable(false);
	s_musicNextBtn->Disable(false);

	paf::math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
	paf::math::v4 btnSz(106.0f, 64.0f, 0.0f, 1.0f);

	s_musicPlayBtn->SetSize(btnSz, makeMusicTimer(200.0f, 21), 65540, 0, 0.0f, NULL, NULL);
	s_musicPauseBtn->SetSize(zero, makeMusicTimer(200.0f, 32), 65540, 0, 0.0f, NULL, NULL);
	s_musicPrevBtn->SetSize(btnSz, makeMusicTimer(200.0f, 21), 65540, 0, 0.0f, NULL, NULL);
	s_musicNextBtn->SetSize(btnSz, makeMusicTimer(200.0f, 21), 65540, 0, 0.0f, NULL, NULL);

	void *mgr = *(void **)((char *)g_liveareaMgr + 4);
	typedef void (*NotifyFn)(void *, int);
	(*(NotifyFn *)((char *)mgr + 188))(userdata, 0);
}

/* 0x811B0F0E */
void liveareaMusicCleanup(paf::ui::Widget *root)
{
	if (!root)
		return;

	root->DeleteEventCallback(33554452, NULL, NULL);

	MusicCbList *list = (MusicCbList *)((char *)root + 604);
	if (list->data) {
		for (int i = 0; i < list->count; ++i) {
			if (list->data[i].fn == liveareaMusicOnDestroy && list->data[i].arg == NULL) {
				for (int j = i + 1; j < list->count; ++j)
					list->data[j - 1] = list->data[j];
				list->count--;
				break;
			}
		}
	}

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

	paf::IDParam ids[7] = {
		makeMusicId(1443415368u),
		makeMusicId((SceUInt32)-2030468014),
		makeMusicId(1211511918u),
		makeMusicId(1252053333u),
		makeMusicId(1467296697u),
		makeMusicId(114483710u),
		makeMusicId(703947029u)
	};
	for (SceUInt32 i = 0; i < 7; ++i) {
		paf::ui::Widget *c = root->FindChild(ids[i], 0);
		if (c) {
			*(paf::IDParam *)((char *)c + 320) = paf::IDParam("");
			typedef void (*DestroyFn)(paf::ui::Widget *);
			(*(DestroyFn *)(*(char **)c + 108))(c);
		}
	}
}

/* 0x811B1202 */
void liveareaMusicShowStatusMsg(void)
{
	if (!s_musicStatusText)
		return;

	paf::Plugin *impose = paf::Plugin::Find("impose_plugin");
	if (impose) {
		SceUInt32 sid = (s_musicInfo.substatus == 2 && s_musicInfo.errCode == 192)
			? (SceUInt32)-1890905394 : (SceUInt32)-1463023626;
		const wchar_t *ws = impose->GetString(makeMusicId(sid));
		paf::wstring wt(ws ? ws : L"");
		typedef void (*SetTextFn)(paf::ui::Widget *, const paf::wstring &);
		(*(SetTextFn *)(*(char **)s_musicStatusText + 284))(s_musicStatusText, wt);
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
	musicPlayerGetInfo(&s_musicInfo);
	if (s_musicInfo.positionMs >= 1000)
		musicPlayerSeek(0, -1, -1);
	else
		musicPlayerCmd(4, 0);
}

/* 0x811B1532 */
static void liveareaMusicOnPrevRepeat(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *)
{
	if (s_musicState == 3) {
		liveareaMusicShowStatusMsg();
	} else if (s_musicSeekCount == 0) {
		musicPlayerCmd(14, 0);
	}
	s_musicSeekCount++;
}

/* 0x811B157E */
static void liveareaMusicOnPlayPause(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *)
{
	if (s_musicState == 0)
		musicPlayerCmd(1, 0);
	else if (s_musicState <= 2)
		musicPlayerCmd(0, 0);
	else if (s_musicState <= 3)
		liveareaMusicShowStatusMsg();
}

/* 0x811B15B8 */
static void liveareaMusicOnNext(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *)
{
	if (s_musicState == 3)
		liveareaMusicShowStatusMsg();
	else
		musicPlayerCmd(3, 0);
}

/* 0x811B15D8 */
static void liveareaMusicOnNextRepeat(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *)
{
	if (s_musicState == 3) {
		liveareaMusicShowStatusMsg();
	} else if (s_musicSeekCount == 0) {
		musicPlayerCmd(11, 0);
	}
	s_musicSeekCount++;
}

/* 0x811B1624 */
static void liveareaMusicOnSeekEnd(SceInt32, paf::ui::Handler *h, paf::ui::Event *, void *)
{
	if (s_musicSeekCount != 0) {
		s_musicSeekCount = 0;
		if (s_musicState != 3)
			musicPlayerCmd(17, 0);
		if (h)
			((paf::ui::ButtonBase *)h)->SetGlowAnim(0, true);
	}
}

/* 0x811B165C */
static void liveareaMusicOnDestroy(void)
{
	paf::Plugin *impose = paf::Plugin::Find("impose_plugin");
	if (impose) {
		void *iface = impose->GetInterface(1);
		if (iface) {
			typedef void (*SetActiveFn)(int);
			(*(SetActiveFn *)((char *)iface + 40))(0);
		}
	}
	musicPlayerCmd(1, 0);
	musicPlayerTerm(0);
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
	int r = musicPlayerGetInfo(&s_musicInfo);
	paf::Plugin *impose = paf::Plugin::Find("impose_plugin");
	void *iface = impose ? impose->GetInterface(1) : NULL;

	if (r < 0) {
		liveareaMusicShowDisabledState(userdata);
		return;
	}

	typedef int (*CheckSuspendedFn)(void);
	if (s_musicInfo.active == 1 && iface && (*(CheckSuspendedFn *)((char *)iface + 36))() == 0) {
		char tmp[8];
		musicPlayerClearFlag(tmp);
		if (s_musicStatusText) {
			const wchar_t *ws = impose->GetString(makeMusicId(712943388u));
			paf::wstring wt(ws ? ws : L"");
			typedef void (*SetTextFn)(paf::ui::Widget *, const paf::wstring &);
			(*(SetTextFn *)(*(char **)s_musicStatusText + 284))(s_musicStatusText, wt);

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
			paf::string u8Title(s_musicInfo.title);
			paf::wstring wTitle;
			paf::common::Utf8ToUtf16(u8Title, &wTitle);

			if (s_musicStatusText) {
				paf::math::v4 transparent(1.0f, 1.0f, 1.0f, 0.0f);
				s_musicStatusText->SetColor(transparent, makeMusicTimer(200.0f, 21), 65537, 0, 0.0f, NULL, NULL);
			}
			if (s_musicTitleText) {
				typedef void (*SetTextFn)(paf::ui::Text *, const paf::wstring &);
				(*(SetTextFn *)(*(char **)s_musicTitleText + 284))(s_musicTitleText, wTitle);
				s_musicTitleText->StopAutoScroll();
				s_musicTitleText->StartAutoScroll((paf::ui::Text::ScrollType)1);
			}

			paf::string u8Album(s_musicInfo.album);
			paf::wstring wAlbum;
			paf::common::Utf8ToUtf16(u8Album, &wAlbum);

			paf::string u8Artist(s_musicInfo.artist);
			paf::wstring wArtist;
			paf::common::Utf8ToUtf16(u8Artist, &wArtist);

			if (impose) {
				if (wArtist.size() == 0) {
					const wchar_t *ws = impose->GetString(makeMusicId((SceUInt32)-1197122722));
					wArtist = ws ? ws : L"";
				}
				if (wAlbum.size() == 0) {
					const wchar_t *ws = impose->GetString(makeMusicId((SceUInt32)-1197122722));
					wAlbum = ws ? ws : L"";
				}
			} else {
				if (wArtist.size() == 0)
					wArtist = L"-";
				if (wAlbum.size() == 0)
					wAlbum = L"-";
			}

			if (s_musicSubText) {
				s_musicSubText->StopAutoScroll();
				paf::wstring combined = wArtist + L" / " + wAlbum;
				typedef void (*SetTextFn)(paf::ui::Text *, const paf::wstring &);
				(*(SetTextFn *)(*(char **)s_musicSubText + 284))(s_musicSubText, combined);
				s_musicSubText->StartAutoScroll((paf::ui::Text::ScrollType)1);
			}
		}

		if (s_musicState != s_musicInfo.status) {
			s_musicState = s_musicInfo.status;
			if (s_musicInfo.status == 0)
				liveareaMusicShowPauseState(userdata);
			else if (s_musicInfo.status < 4)
				liveareaMusicShowPlayState(userdata);
		}
	} else if (s_musicState != 8) {
		liveareaMusicShowDisabledState(userdata);
		paf::wstring empty(L"");
		typedef void (*SetTextFn)(paf::ui::Text *, const paf::wstring &);
		(*(SetTextFn *)(*(char **)s_musicTitleText + 284))(s_musicTitleText, empty);
		s_musicSubText->StopAutoScroll();
		(*(SetTextFn *)(*(char **)s_musicSubText + 284))(s_musicSubText, empty);
		s_musicCurTitle.clear();
		s_musicState = 8;
	}
}
