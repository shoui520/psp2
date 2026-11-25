/*
 * SceShell livearea_plugin: Parental Controls LiveArea banner
 * 3.65-CEX: 0x811AE858 - 0x811AFC90 (sti 0x811AFC20)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/math.h>
#include <common/char_encoding.h>
#include <common/string_util.h>
#include <datetime/datetime.h>
#include <math/math.h>
#include <plugin.h>
#include <timer/timer.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_button.h>

#include "../../livespace_db/lsdb_keys.h"

extern "C" {
extern int sceRegMgrGetKeyIntForUser(const char *category, const char *name, int *buf);
extern int sceRtcGetDayOfWeek(int year, int month, int day);
extern float sce_paf_ceilf(float x);
}

extern int parentalGetRemainingSec(void);			/* 0x812327F8 */

static paf::ui::Widget *s_parentalPieHalf1 = NULL;		/* 0x81544DA0 */
static paf::ui::Widget *s_parentalPieHalf2 = NULL;		/* 0x81544DA4 */
static paf::ui::Widget *s_parentalPieHand = NULL;		/* 0x81544DA8 */
static paf::ui::Widget *s_parentalTimeText = NULL;		/* 0x81544DAC */
static paf::ui::Widget *s_parentalLimitedBox = NULL;		/* 0x81544DB0 */
static paf::ui::Widget *s_parentalUnlimitedBox = NULL;		/* 0x81544DB4 */
static paf::Plugin *s_parentalImposePlugin = NULL;		/* 0x81544DB8 */

static const char *const s_restrictDurationKeys[8] = {		/* 0x81511854 */
	"restrict_duration_one",
	"restrict_duration_sun",
	"restrict_duration_mon",
	"restrict_duration_tue",
	"restrict_duration_wed",
	"restrict_duration_thu",
	"restrict_duration_fri",
	"restrict_duration_sat"
};

struct ParentalCbEntry {
	void (*fn)(void);
	void *arg;
};

struct ParentalCbList {
	void *alloc;
	ParentalCbEntry *data;
	int count;
	int capacity;
};

static void liveareaParentalOnTimer(SceInt32 ev, paf::ui::Handler *h, paf::ui::Event *e, void *userdata);
static void liveareaParentalResetGlobals(void);

static paf::IDParam makeParentalId(SceUInt32 hash)
{
	return paf::IDParam(hash);
}

static void parentalWidgetShow(paf::ui::Widget *w, paf::Timer *t)
{
	SceUInt8 flags = *((SceUInt8 *)w + 406);
	if ((flags & 0x80) == 0) {
		*((SceUInt8 *)w + 388) = 0xFF;
		*((SceUInt8 *)w + 406) = flags | 0x80;
		typedef void (*Fn)(paf::ui::Widget *, int);
		(*(Fn *)(*(char **)w + 144))(w, 1);
	}
	ScePafWidget_FF5BC957(w, t, 2, 0.0f, NULL);
}

static void parentalWidgetHide(paf::ui::Widget *w)
{
	*((SceUInt8 *)w + 406) &= ~0x80u;
	*((SceUInt8 *)w + 388) = 0xFF;
	typedef void (*Fn)(paf::ui::Widget *, int);
	(*(Fn *)(*(char **)w + 148))(w, 1);
	ScePafWidget_4588699E(w, NULL, 2, 0.0f, NULL);
}

/* 0x811AE858 */
void liveareaParentalAnimateMode(void)
{
	paf::ui::Widget *showW;
	paf::ui::Widget *hideW;
	if (parentalGetRemainingSec() == -1) {
		showW = s_parentalUnlimitedBox;
		hideW = s_parentalLimitedBox;
	} else {
		showW = s_parentalLimitedBox;
		hideW = s_parentalUnlimitedBox;
	}

	parentalWidgetHide(showW);
	paf::Timer *t1 = new paf::Timer();
	if (t1)
		t1->InitTimer(200.0f, 21, 0, -1, NULL);
	parentalWidgetShow(showW, t1);

	paf::math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
	showW->SetScale(zero, NULL, 65541, 0, 0.0f, NULL, NULL);

	paf::math::v4 one(1.0f, 1.0f, 0.0f, 0.0f);
	paf::Timer *t2 = new paf::Timer();
	if (t2)
		t2->InitTimer(200.0f, 32, 0, -1, NULL);
	showW->SetScale(one, t2, 65541, 2, 0.0f, NULL, NULL);

	parentalWidgetHide(hideW);
}

/* 0x811AEAA2 */
void liveareaParentalPulseButton(paf::ui::Widget *btn)
{
	int objId = *(int *)((char *)btn + 636);
	typedef void *(*GetObjFn)(paf::ui::Widget *, int);
	void *obj = (*(GetObjFn *)(*(char **)btn + 248))(btn, objId);
	if (!obj)
		return;

	SceUInt32 rgba = *(SceUInt32 *)((char *)obj + 44);
	float r = (float)(rgba & 0xFF) / 255.0f;
	float g = (float)((rgba >> 8) & 0xFF) / 255.0f;
	float b = (float)((rgba >> 16) & 0xFF) / 255.0f;

	paf::math::v4 colLo(r, g, b, 0.1f);
	paf::math::v4 colHi(r, g, b, 0.38f);

	paf::Timer *t1 = new paf::Timer();
	if (t1)
		t1->InitTimer(1000.0f, 39, 0, -1, NULL);
	btn->SetObjAnim(objId, 268567296, colLo, t1, 0);

	paf::Timer *t2 = new paf::Timer();
	if (t2)
		t2->InitTimer(1000.0f, 39, 0, -1, NULL);
	btn->SetObjAnim(objId, 268567296, colHi, t2, 0);
}

/* 0x811AEC32 */
void liveareaParentalSetup(paf::ui::Widget *root, void *userdata)
{
	if (!root)
		return;

	ParentalCbList *list = (ParentalCbList *)((char *)root + 604);
	if (list->data && list->count < list->capacity) {
		list->data[list->count].fn = liveareaParentalResetGlobals;
		list->data[list->count].arg = NULL;
		list->count++;
	}

	s_parentalImposePlugin = paf::Plugin::Find("impose_plugin");
	if (!s_parentalImposePlugin)
		return;

	s_parentalLimitedBox = root->FindChild(makeParentalId(1040921716u), 0);
	if (s_parentalLimitedBox) {
		s_parentalPieHalf1 = s_parentalLimitedBox->FindChild(makeParentalId((SceUInt32)-389872614), 0);
		s_parentalPieHalf2 = s_parentalLimitedBox->FindChild(makeParentalId((SceUInt32)-701381721), 0);
		s_parentalPieHand = s_parentalLimitedBox->FindChild(makeParentalId(656888574u), 0);
		s_parentalTimeText = s_parentalLimitedBox->FindChild(makeParentalId((SceUInt32)-185658303), 0);

		paf::ui::Widget *lbl = s_parentalLimitedBox->FindChild(makeParentalId((SceUInt32)-1655017831), 0);
		const wchar_t *ws = s_parentalImposePlugin->GetString(makeParentalId(338610026u));
		paf::wstring wtext(ws ? ws : L"");
		typedef void (*SetTextFn)(paf::ui::Widget *, const paf::wstring &);
		(*(SetTextFn *)(*(char **)lbl + 284))(lbl, wtext);
	}

	s_parentalUnlimitedBox = root->FindChild(makeParentalId((SceUInt32)-398007981), 0);
	if (s_parentalUnlimitedBox) {
		paf::ui::Widget *lbl1 = s_parentalUnlimitedBox->FindChild(makeParentalId(1624740770u), 0);
		const wchar_t *ws1 = s_parentalImposePlugin->GetString(makeParentalId((SceUInt32)-35738359));
		paf::wstring wt1(ws1 ? ws1 : L"");
		typedef void (*SetTextFn)(paf::ui::Widget *, const paf::wstring &);
		(*(SetTextFn *)(*(char **)lbl1 + 284))(lbl1, wt1);

		paf::ui::Widget *lbl2 = s_parentalUnlimitedBox->FindChild(makeParentalId(1786128809u), 0);
		const wchar_t *ws2 = s_parentalImposePlugin->GetString(makeParentalId((SceUInt32)-422527122));
		paf::wstring wt2(ws2 ? ws2 : L"");
		(*(SetTextFn *)(*(char **)lbl2 + 284))(lbl2, wt2);
	}

	paf::ui::Widget *btn = NULL;
	if (root->FindChild(makeParentalId((SceUInt32)-688361520), 0)) {
		paf::ui::Widget *cand = root->FindChild(makeParentalId((SceUInt32)-688361520), 0);
		typedef int (*IsTypeFn)(paf::ui::Widget *, const char *);
		if ((*(IsTypeFn *)(*(char **)cand + 100))(cand, paf::ui::Button::TypeName()))
			btn = root->FindChild(makeParentalId((SceUInt32)-688361520), 0);
	}
	if (btn) {
		*(SceUInt32 *)((char *)btn + 8) &= ~0x40u;
		liveareaParentalPulseButton(btn);
	}

	paf::ui::EventCBListener *tcb = new paf::ui::EventCBListener(liveareaParentalOnTimer, userdata);
	root->SetTimerListener(33554453, 500.0f, tcb);
	liveareaParentalAnimateMode();
}

/* 0x811AF4CC */
void liveareaParentalCleanup(paf::ui::Widget *root)
{
	if (!root)
		return;

	root->DeleteEventCallback(33554453, NULL, NULL);

	ParentalCbList *list = (ParentalCbList *)((char *)root + 604);
	if (list->data) {
		for (int i = 0; i < list->count; ++i) {
			if (list->data[i].fn == liveareaParentalResetGlobals && list->data[i].arg == NULL) {
				for (int j = i + 1; j < list->count; ++j)
					list->data[j - 1] = list->data[j];
				list->count--;
				break;
			}
		}
	}

	if (s_parentalLimitedBox) {
		*(paf::IDParam *)((char *)s_parentalLimitedBox + 320) = paf::IDParam("");
		typedef void (*DestroyFn)(paf::ui::Widget *);
		(*(DestroyFn *)(*(char **)s_parentalLimitedBox + 108))(s_parentalLimitedBox);
		s_parentalLimitedBox = NULL;
		s_parentalPieHalf1 = NULL;
		s_parentalPieHalf2 = NULL;
		s_parentalPieHand = NULL;
		s_parentalTimeText = NULL;
	}
	if (s_parentalUnlimitedBox) {
		*(paf::IDParam *)((char *)s_parentalUnlimitedBox + 320) = paf::IDParam("");
		typedef void (*DestroyFn)(paf::ui::Widget *);
		(*(DestroyFn *)(*(char **)s_parentalUnlimitedBox + 108))(s_parentalUnlimitedBox);
		s_parentalUnlimitedBox = NULL;
	}
}

/* 0x811AF668 */
int liveareaParentalGetDailyLimitSec(void)
{
	int minutes = 0;
	int val = 0;
	paf::datetime::DateTime now = paf::datetime::DateTime::Now();
	if (sceRegMgrGetKeyIntForUser("/CONFIG/SECURITY/PARENTAL", "restrict_mode", &val) == 0 &&
	    (val == 1 || val == 2)) {
		int idx = 0;
		if (val == 2)
			idx = sceRtcGetDayOfWeek(now.m_year, now.m_month, now.m_day) + 1;
		if (sceRegMgrGetKeyIntForUser("/CONFIG/SECURITY/PARENTAL", s_restrictDurationKeys[idx], &val) == 0 &&
		    val >= 0) {
			minutes = val;
			int today = 10000 * now.m_year + 100 * now.m_month + now.m_day;
			if (sceRegMgrGetKeyIntForUser("/CONFIG/SECURITY/PARENTAL", "change_date", &val) == 0 &&
			    val == today &&
			    sceRegMgrGetKeyIntForUser("/CONFIG/SECURITY/PARENTAL", "change_duration", &val) == 0 &&
			    val >= -minutes) {
				minutes += val;
			}
		}
	}
	return 60 * minutes;
}

/* 0x811AF766 */
static void liveareaParentalOnTimer(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *)
{
	if (parentalGetRemainingSec() == -1) {
		parentalWidgetShow(s_parentalUnlimitedBox, NULL);
		s_parentalUnlimitedBox->SetMetaAlpha(1.0f, NULL, 65539, 0, 0.0f, NULL, NULL);
		parentalWidgetHide(s_parentalLimitedBox);
		return;
	}

	parentalWidgetHide(s_parentalUnlimitedBox);
	parentalWidgetShow(s_parentalLimitedBox, NULL);
	s_parentalLimitedBox->SetMetaAlpha(1.0f, NULL, 65539, 0, 0.0f, NULL, NULL);

	int totalSec = liveareaParentalGetDailyLimitSec();
	int remSec = parentalGetRemainingSec();
	float ratio = (float)remSec / (float)totalSec;

	paf::math::v4 rot = s_parentalPieHand->GetRot(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0);
	rot.set_z(3.1415927f - ratio * 6.2831855f);
	s_parentalPieHand->SetRot(rot, 0, NULL, 65542, 0, 0.0f, NULL, NULL);

	if (ratio <= 0.5f) {
		parentalWidgetShow(s_parentalPieHalf1, NULL);
		parentalWidgetHide(s_parentalPieHalf2);
	} else {
		parentalWidgetHide(s_parentalPieHalf1);
		parentalWidgetShow(s_parentalPieHalf2, NULL);
	}

	char buf[32];
	sce_paf_memset(buf, 0, sizeof(buf));
	paf::datetime::DateTime dt(0, 0, 0, 0, 0, 0, 0);
	int rem = parentalGetRemainingSec();
	int totalMin = (int)sce_paf_ceilf((float)rem / 60.0f);
	int h = totalMin / 60;
	int m = totalMin % 60;
	sce_paf_snprintf(buf, sizeof(buf), "%02d:%02d", h, m);

	paf::string u8(buf);
	paf::wstring u16;
	paf::common::Utf8ToUtf16(u8, &u16);
	typedef void (*SetTextFn)(paf::ui::Widget *, const paf::wstring &);
	(*(SetTextFn *)(*(char **)s_parentalTimeText + 284))(s_parentalTimeText, u16);
}

/* 0x811AFBD6 */
static void liveareaParentalResetGlobals(void)
{
	s_parentalPieHalf1 = NULL;
	s_parentalPieHalf2 = NULL;
	s_parentalPieHand = NULL;
	s_parentalTimeText = NULL;
	s_parentalLimitedBox = NULL;
	s_parentalUnlimitedBox = NULL;
	s_parentalImposePlugin = NULL;
}
