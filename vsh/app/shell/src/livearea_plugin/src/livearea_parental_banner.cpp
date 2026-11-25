/*
 * SceShell livearea_plugin: Parental Controls LiveArea banner
 * 3.65-CEX: 0x811AE858 - 0x811AFC90 (sti 0x811AFC20)
 */
#include <std/stdio.h>
#include <std/string.h>
#include <std/math.h>
#include <common/char_encoding.h>
#include <common/string_util.h>
#include <datetime/datetime.h>
#include <graphics/drawobj/draw_obj.h>
#include <math/math.h>
#include <timer/timer.h>
#include <widget/core/handler.h>
#include <widget/w_button.h>
#include "livearea_internal.h"

extern "C" {
extern int sceRegMgrGetKeyIntForUser(const char *category, const char *name, int *buf);
extern int sceRtcGetDayOfWeek(int year, int month, int day);
extern float sce_paf_ceilf(float x);
}

extern int parentalGetRemainingSec(void);			/* 0x812327F8 */

static paf::ui::Widget *s_parentalPieHalfLeft = NULL;		/* 0x81544DA0 */
static paf::ui::Widget *s_parentalPieHalfRight = NULL;		/* 0x81544DA4 */
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

static void liveareaParentalOnTimer(SceInt32 ev, paf::ui::Handler *handler,
				    paf::ui::Event *event, void *userdata);
static void liveareaParentalResetGlobals(paf::ui::Widget *widget, void *userdata);

static paf::IDParam makeParentalId(SceUInt32 hash)
{
	return paf::IDParam(hash);
}

/* 0x811AE858 */
void liveareaParentalAnimateMode(void)
{
	paf::ui::Widget *showBox;
	paf::ui::Widget *hideBox;
	if (parentalGetRemainingSec() == -1) {
		showBox = s_parentalUnlimitedBox;
		hideBox = s_parentalLimitedBox;
	} else {
		showBox = s_parentalLimitedBox;
		hideBox = s_parentalUnlimitedBox;
	}

	showBox->Hide(0xff, NULL, NULL, 0.0f);
	paf::Timer *fadeTimer = new paf::Timer();
	if (fadeTimer != NULL)
		fadeTimer->InitTimer(200.0f, 21, 0, -1, NULL);
	showBox->Show(0xff, fadeTimer, NULL, 0.0f);

	paf::math::v4 zeroScale(0.0f, 0.0f, 0.0f, 0.0f);
	showBox->SetScale(zeroScale, NULL, 65541, 0, 0.0f, NULL, NULL);

	paf::math::v4 fullScale(1.0f, 1.0f, 0.0f, 0.0f);
	paf::Timer *scaleTimer = new paf::Timer();
	if (scaleTimer != NULL)
		scaleTimer->InitTimer(200.0f, 32, 0, -1, NULL);
	showBox->SetScale(fullScale, scaleTimer, 65541, 2, 0.0f, NULL, NULL);

	hideBox->Hide(0xff, NULL, NULL, 0.0f);
}

/* 0x811AEAA2 */
void liveareaParentalPulseButton(paf::ui::Button *btn)
{
	int glowObjId = btn->m_glow_obj;
	paf::graph::DrawObj *glowObj = btn->GetDrawObj(glowObjId);
	if (glowObj == NULL)
		return;

	SceUInt32 rgba = glowObj->m_color;
	float red = (float)(rgba & 0xFF) / 255.0f;
	float green = (float)((rgba >> 8) & 0xFF) / 255.0f;
	float blue = (float)((rgba >> 16) & 0xFF) / 255.0f;

	paf::math::v4 dimColor(red, green, blue, 0.1f);
	paf::math::v4 brightColor(red, green, blue, 0.38f);

	paf::Timer *dimTimer = new paf::Timer();
	if (dimTimer != NULL)
		dimTimer->InitTimer(1000.0f, 39, 0, -1, NULL);
	btn->SetObjAnim(glowObjId, 268567296, dimColor, dimTimer, 0);

	paf::Timer *brightTimer = new paf::Timer();
	if (brightTimer != NULL)
		brightTimer->InitTimer(1000.0f, 39, 0, -1, NULL);
	btn->SetObjAnim(glowObjId, 268567296, brightColor, brightTimer, 0);
}

/* 0x811AEC32 */
void liveareaParentalSetup(paf::ui::Widget *root, void *userdata)
{
	if (root == NULL)
		return;

	root->AddDeleteCB(liveareaParentalResetGlobals, NULL);

	s_parentalImposePlugin = paf::Plugin::Find("impose_plugin");
	if (s_parentalImposePlugin == NULL)
		return;

	s_parentalLimitedBox = root->FindChild(makeParentalId(1040921716U), 0);
	if (s_parentalLimitedBox != NULL) {
		s_parentalPieHalfLeft = s_parentalLimitedBox->FindChild(makeParentalId((SceUInt32)-389872614), 0);
		s_parentalPieHalfRight = s_parentalLimitedBox->FindChild(makeParentalId((SceUInt32)-701381721), 0);
		s_parentalPieHand = s_parentalLimitedBox->FindChild(makeParentalId(656888574U), 0);
		s_parentalTimeText = s_parentalLimitedBox->FindChild(makeParentalId((SceUInt32)-185658303), 0);

		paf::ui::Widget *limitLabel =
			s_parentalLimitedBox->FindChild(makeParentalId((SceUInt32)-1655017831), 0);
		const wchar_t *limitStr = s_parentalImposePlugin->GetString(makeParentalId(338610026U));
		paf::wstring limitText(limitStr ? limitStr : L"");
		limitLabel->SetString(limitText);
	}

	s_parentalUnlimitedBox = root->FindChild(makeParentalId((SceUInt32)-398007981), 0);
	if (s_parentalUnlimitedBox != NULL) {
		paf::ui::Widget *headingLabel =
			s_parentalUnlimitedBox->FindChild(makeParentalId(1624740770U), 0);
		const wchar_t *headingStr =
			s_parentalImposePlugin->GetString(makeParentalId((SceUInt32)-35738359));
		paf::wstring headingText(headingStr ? headingStr : L"");
		headingLabel->SetString(headingText);

		paf::ui::Widget *bodyLabel =
			s_parentalUnlimitedBox->FindChild(makeParentalId(1786128809U), 0);
		const wchar_t *bodyStr =
			s_parentalImposePlugin->GetString(makeParentalId((SceUInt32)-422527122));
		paf::wstring bodyText(bodyStr ? bodyStr : L"");
		bodyLabel->SetString(bodyText);
	}

	paf::ui::Button *btn = NULL;
	paf::ui::Widget *candidate = root->FindChild(makeParentalId((SceUInt32)-688361520), 0);
	if (candidate != NULL && candidate->IsInherit(paf::ui::Button::TypeName()))
		btn = (paf::ui::Button *)candidate;
	if (btn != NULL) {
		btn->DisableEvent(0x40);
		liveareaParentalPulseButton(btn);
	}

	paf::ui::EventCBListener *timerListener =
		new paf::ui::EventCBListener(liveareaParentalOnTimer, userdata);
	root->SetTimerListener(33554453, 500.0f, timerListener);
	liveareaParentalAnimateMode();
}

/* 0x811AF4CC */
void liveareaParentalCleanup(paf::ui::Widget *root)
{
	if (root == NULL)
		return;

	root->DeleteEventCallback(33554453, NULL, NULL);
	root->RemoveDeleteCB(liveareaParentalResetGlobals, NULL);

	if (s_parentalLimitedBox != NULL) {
		s_parentalLimitedBox->m_name = paf::IDParam("");
		s_parentalLimitedBox->DestroyWidget();
		s_parentalLimitedBox = NULL;
		s_parentalPieHalfLeft = NULL;
		s_parentalPieHalfRight = NULL;
		s_parentalPieHand = NULL;
		s_parentalTimeText = NULL;
	}
	if (s_parentalUnlimitedBox != NULL) {
		s_parentalUnlimitedBox->m_name = paf::IDParam("");
		s_parentalUnlimitedBox->DestroyWidget();
		s_parentalUnlimitedBox = NULL;
	}
}

/* 0x811AF668 */
int liveareaParentalGetDailyLimitSec(void)
{
	int minutes = 0;
	int regValue = 0;
	paf::datetime::DateTime now = paf::datetime::DateTime::Now();
	if (sceRegMgrGetKeyIntForUser("/CONFIG/SECURITY/PARENTAL", "restrict_mode", &regValue) == 0 &&
	    (regValue == 1 || regValue == 2)) {
		int dayIndex = 0;
		if (regValue == 2)
			dayIndex = sceRtcGetDayOfWeek(now.m_year, now.m_month, now.m_day) + 1;
		if (sceRegMgrGetKeyIntForUser("/CONFIG/SECURITY/PARENTAL",
					      s_restrictDurationKeys[dayIndex], &regValue) == 0 &&
		    regValue >= 0) {
			minutes = regValue;
			int today = 10000 * now.m_year + 100 * now.m_month + now.m_day;
			if (sceRegMgrGetKeyIntForUser("/CONFIG/SECURITY/PARENTAL", "change_date", &regValue) == 0 &&
			    regValue == today &&
			    sceRegMgrGetKeyIntForUser("/CONFIG/SECURITY/PARENTAL", "change_duration", &regValue) == 0 &&
			    regValue >= -minutes) {
				minutes += regValue;
			}
		}
	}
	return 60 * minutes;
}

/* 0x811AF766 */
static void liveareaParentalOnTimer(SceInt32, paf::ui::Handler *, paf::ui::Event *, void *)
{
	if (parentalGetRemainingSec() == -1) {
		s_parentalUnlimitedBox->Show(0xff, NULL, NULL, 0.0f);
		s_parentalUnlimitedBox->SetMetaAlpha(1.0f, NULL, 65539, 0, 0.0f, NULL, NULL);
		s_parentalLimitedBox->Hide(0xff, NULL, NULL, 0.0f);
		return;
	}

	s_parentalUnlimitedBox->Hide(0xff, NULL, NULL, 0.0f);
	s_parentalLimitedBox->Show(0xff, NULL, NULL, 0.0f);
	s_parentalLimitedBox->SetMetaAlpha(1.0f, NULL, 65539, 0, 0.0f, NULL, NULL);

	int totalSec = liveareaParentalGetDailyLimitSec();
	int remSec = parentalGetRemainingSec();
	float ratio = (float)remSec / (float)totalSec;

	paf::math::v4 rot = s_parentalPieHand->GetRot(0, NULL, paf::math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0);
	rot.set_z(3.1415927f - ratio * 6.2831855f);
	s_parentalPieHand->SetRot(rot, 0, NULL, 65542, 0, 0.0f, NULL, NULL);

	if (ratio <= 0.5f) {
		s_parentalPieHalfLeft->Show(0xff, NULL, NULL, 0.0f);
		s_parentalPieHalfRight->Hide(0xff, NULL, NULL, 0.0f);
	} else {
		s_parentalPieHalfLeft->Hide(0xff, NULL, NULL, 0.0f);
		s_parentalPieHalfRight->Show(0xff, NULL, NULL, 0.0f);
	}

	char timeBuf[32];
	sce_paf_memset(timeBuf, 0, sizeof(timeBuf));
	paf::datetime::DateTime dt(0, 0, 0, 0, 0, 0, 0);
	int remNow = parentalGetRemainingSec();
	int totalMin = (int)sce_paf_ceilf((float)remNow / 60.0f);
	int hours = totalMin / 60;
	int mins = totalMin % 60;
	sce_paf_snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", hours, mins);

	paf::string utf8Time(timeBuf);
	paf::wstring utf16Time;
	paf::common::Utf8ToUtf16(utf8Time, &utf16Time);
	s_parentalTimeText->SetString(utf16Time);
}

/* 0x811AFBD6 */
static void liveareaParentalResetGlobals(paf::ui::Widget *widget, void *userdata)
{
	(void)widget;
	(void)userdata;
	s_parentalPieHalfLeft = NULL;
	s_parentalPieHalfRight = NULL;
	s_parentalPieHand = NULL;
	s_parentalTimeText = NULL;
	s_parentalLimitedBox = NULL;
	s_parentalUnlimitedBox = NULL;
	s_parentalImposePlugin = NULL;
}
