/*
 * SceFriendSelect (vs0:sys/external/friend_select.suprx):
 * FriendSelectItemFactory, single/multi list item builders, profile-to-widget
 * binding/unbinding, localized message formatter, and locale separator table.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/list>
#include <std/string.h>
#include <std/string>
#include <common/string_util.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/surface/surface.h>
#include <graphics/text/text_layout.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <widget/core/event.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_button_base.h>
#include <widget/w_check_box.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <widget/w_text.h>
#include "friend_select_internal.h"

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::math;
using namespace paf::ui;

#define FRIEND_EVENT_LAYOUT_DONE	0x00100004

typedef const wchar_t *(*MessageTokenResolver)(wchar_t token, void *ctx);

static MessageTokenResolver s_messageTokenResolver = NULL;

/* ========================================================================= */
/* 79-entry locale decimal / grouping separator table (static init & dtor)   */
/* ========================================================================= */

struct LocaleSeparatorEntry {
	string country;
	int32_t languageId;
	string decimalSep;
	string groupingSep;
};

static const LocaleSeparatorEntry s_localeSeparators[79] = {
	{ "JP",  0, ".", "," },
	{ "US",  1, ".", "," },
	{ "CA",  1, ".", "," },
	{ "CA",  2, ",", " " },
	{ "MX",  3, ".", "," },
	{ "MX",  1, ".", "," },
	{ "BR", 17, ",", "." },
	{ "BR",  1, ".", "," },
	{ "CL",  3, ",", " " },
	{ "CL",  1, ".", "," },
	{ "PE",  3, ",", " " },
	{ "PE",  1, ".", "," },
	{ "AR",  3, ",", " " },
	{ "AR",  1, ".", "," },
	{ "CO",  3, ",", " " },
	{ "CO",  1, ".", "," },
	{ "GB", 18, ".", "," },
	{ "IE", 18, ".", "," },
	{ "BE",  6, ",", " " },
	{ "BE",  2, ",", " " },
	{ "LU",  4, ",", "." },
	{ "LU",  2, ",", " " },
	{ "NL",  6, ",", "." },
	{ "FR",  2, ",", " " },
	{ "DE",  4, ",", "." },
	{ "AT",  4, ",", "." },
	{ "CH",  4, ",", " " },
	{ "CH",  2, ",", " " },
	{ "CH",  5, ",", " " },
	{ "IT",  5, ",", "." },
	{ "PT",  7, ",", "." },
	{ "DK", 14, ",", " " },
	{ "DK", 18, ".", "," },
	{ "FI", 12, ",", " " },
	{ "FI", 18, ".", "," },
	{ "NO", 15, ",", " " },
	{ "NO", 18, ".", "," },
	{ "SE", 13, ",", " " },
	{ "SE", 18, ".", "," },
	{ "AU", 18, ".", "," },
	{ "NZ", 18, ".", "," },
	{ "ES",  3, ",", "." },
	{ "RU",  8, ",", " " },
	{ "AE", 18, ".", "," },
	{ "ZA", 18, ".", "," },
	{ "PL", 16, ",", " " },
	{ "PL", 18, ".", "," },
	{ "GR", 18, ".", "," },
	{ "SA", 18, ".", "," },
	{ "CZ", 18, ".", "," },
	{ "TR", 18, ".", "," },
	{ "TR", 19, ",", "." },
	{ "IN", 18, ".", "," },
	{ "HR", 18, ".", "," },
	{ "SI", 18, ".", "," },
	{ "KW", 18, ".", "," },
	{ "IL", 18, ".", "," },
	{ "UA",  8, ",", " " },
	{ "BG", 18, ".", "," },
	{ "RO", 18, ".", "," },
	{ "HU", 18, ".", "," },
	{ "QA", 18, ".", "," },
	{ "SK", 18, ".", "," },
	{ "BH", 18, ".", "," },
	{ "LB", 18, ".", "," },
	{ "OM", 18, ".", "," },
	{ "MT", 18, ".", "," },
	{ "IS", 18, ".", "," },
	{ "CY", 18, ".", "," },
	{ "HK", 10, ".", "," },
	{ "HK", 18, ".", "," },
	{ "HK", 11, ".", "," },
	{ "TW", 10, ".", "," },
	{ "TW", 18, ".", "," },
	{ "SG", 18, ".", "," },
	{ "MY", 18, ".", "," },
	{ "ID", 18, ".", "," },
	{ "TH", 18, ".", "," },
	{ "KR",  9, ".", "," }
};

/* ========================================================================= */
/* FriendSelectItemFactory                                                   */
/* ========================================================================= */

FriendSelectItemFactory::FriendSelectItemFactory(
	FuncCallback1<ListItem *, Widget *> *createCb, Callback2<Widget *, int32_t> *bindCb,
	Callback2<Widget *, int32_t> *unbindCb, Callback1<int32_t> *selectCb)
	: m_createCb(createCb), m_bindCb(bindCb), m_unbindCb(unbindCb), m_selectCb(selectCb),
	  m_recycled(), m_detached(false)
{
}

FriendSelectItemFactory::~FriendSelectItemFactory()
{
	if (m_createCb != NULL)
		delete m_createCb;
	if (m_bindCb != NULL)
		delete m_bindCb;
	if (m_unbindCb != NULL)
		delete m_unbindCb;
	if (m_selectCb != NULL)
		delete m_selectCb;
}

void FriendSelectItemFactory::DetachCallbacks(void)
{
	if (m_createCb != NULL) {
		delete m_createCb;
		m_createCb = NULL;
	}
	if (m_bindCb != NULL) {
		delete m_bindCb;
		m_bindCb = NULL;
	}
	if (m_unbindCb != NULL) {
		delete m_unbindCb;
		m_unbindCb = NULL;
	}
	if (m_selectCb != NULL) {
		delete m_selectCb;
		m_selectCb = NULL;
	}
	m_detached = true;
}

namespace paf {
template <>
inline __attribute__((always_inline)) void
list<ui::ListItem *, allocator<ui::ListItem *> >::pop_front()
{
	if (head != NULL && head->next != head) {
		_Node *node = head->next;
		node->next->prev = node->prev;
		node->prev->next = node->next;
		destroy(&node->value);
		operator delete(node);
		--count;
	}
}
}

ListItem *FriendSelectItemFactory::Create(listview::ItemFactory::CreateParam &param)
{
	if (m_detached)
		return NULL;

	ListItem *item = NULL;

	if (m_recycled.size() != 0) {
		item = m_recycled.front();
		m_recycled.pop_front();
		item->SetParent(param.m_parent, NULL, 0);
		item->Show(0xFF, NULL, NULL, 0.0f);
	} else if (m_createCb != NULL) {
		item = m_createCb->Invoke(param.m_parent);
	}
	if (item == NULL)
		return NULL;

	Widget *rowBtn = item->GetChild(0);
	Widget *checkBox = item->FindChild(IDParam(RCO_ITEM_CHECK_BOX), 0);
	int32_t cellIdx = param.m_cell;

	if (checkBox != NULL) {
		rowBtn->AddEventListener(
			ButtonBase::CB_BTN_DECIDE,
			new MemberEventListener<FriendSelectItemFactory>(
				this, &FriendSelectItemFactory::OnRowButtonDecide,
				(void *)(intptr_t)cellIdx),
			false);
		checkBox->AddEventListener(
			ButtonBase::CB_BTN_DECIDE,
			new MemberEventListener<FriendSelectItemFactory>(
				this, &FriendSelectItemFactory::OnItemDecide,
				(void *)(intptr_t)cellIdx),
			false);
	} else {
		rowBtn->AddEventListener(
			ButtonBase::CB_BTN_DECIDE,
			new MemberEventListener<FriendSelectItemFactory>(
				this, &FriendSelectItemFactory::OnItemDecide,
				(void *)(intptr_t)cellIdx),
			false);
	}

	if (m_bindCb != NULL)
		m_bindCb->Invoke(rowBtn, cellIdx);

	item->SetMetaAlpha(0.0f, NULL, 65539, 0, 0.0f, NULL, NULL);
	Timer *fadeTimer = new Timer(200.0f, 0, 0, -1, NULL);

	item->SetMetaAlpha(1.0f, fadeTimer, 65539, 0, 0.0f, NULL, NULL);
	return item;
}

void FriendSelectItemFactory::Stop(listview::ItemFactory::StopParam &param)
{
	if (m_detached)
		return;
	int32_t cellIdx = param.m_cell;
	ListItem *item = param.m_item;
	Widget *rowBtn = item->GetChild(0);
	Widget *checkBox = item->FindChild(IDParam(RCO_ITEM_CHECK_BOX), 0);

	if (m_unbindCb != NULL)
		m_unbindCb->Invoke(rowBtn, cellIdx);

	rowBtn->DeleteEventListener(ButtonBase::CB_BTN_DECIDE, NULL);
	if (checkBox != NULL)
		checkBox->DeleteEventListener(ButtonBase::CB_BTN_DECIDE, NULL);

	item->SetPos(v4(0.0f, 0.0f, 0.0f, 0.0f), NULL, 0x10000, 0, 0.0f, NULL, NULL);
	item->Hide(0xFF, NULL, NULL, 0.0f);
	item->SetParent(item->m_parent, NULL, 0);
	m_recycled.push_back(item);
}

void FriendSelectItemFactory::OnItemDecide(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	if (m_selectCb != NULL)
		m_selectCb->Invoke((int32_t)(intptr_t)userData);
}

void FriendSelectItemFactory::OnRowButtonDecide(int32_t type, Handler *self, Event *e,
						void *userData)
{
	(void)type;
	(void)e;
	CheckBox *checkBox = (CheckBox *)((Widget *)self)->FindChild(IDParam(RCO_ITEM_CHECK_BOX), 0);

	if (checkBox->m_meta_alpha > 0.0f) {
		bool wasChecked = checkBox->IsChecked();

		checkBox->SetCheck(!wasChecked, 0.0f, false);
		FriendLibPlayCommonSE(wasChecked ? 1U : 0U);
		if (m_selectCb != NULL)
			m_selectCb->Invoke((int32_t)(intptr_t)userData);
	}
}

/* ========================================================================= */
/* Item template builders & presence platform icon layout callback           */
/* ========================================================================= */

static void OnPresenceTextLayout(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)e;
	(void)userData;
	Text *textWidget = (Text *)self;
	TextLayoutLineInfo lineInfo;

	if (textWidget->GetLineInfo(0, lineInfo) >= 0) {
		Widget *platIcon = textWidget->FindChild(IDParam(RCO_ITEM_PLATFORM_ICON), 0);

		if (platIcon != NULL) {
			platIcon->SetPos(v4(lineInfo.frame.extract_z() + 8.0f, 0.0f, 0.0f, 0.0f), NULL,
					 0x10000, 0, 0.0f, NULL, NULL);
			platIcon->Show(0xFF, NULL, NULL, 0.0f);
			textWidget->DeleteEventCallback(FRIEND_EVENT_LAYOUT_DONE,
							OnPresenceTextLayout, NULL);
		}
	}
}

ListItem *FriendSelectCreateSingleItem(Widget *parent)
{
	Plugin::TemplateOpenParam tmplParam;

	FriendSelectTemplateOpen(parent, IDParam(RCO_TMPL_SINGLE_ITEM), tmplParam);
	ListItem *item = (ListItem *)parent->FindChild(IDParam(RCO_ITEM_ROOT), 0);

	if (item != NULL) {
		item->m_name.m_idhash = 0;
		Widget *rowBtn = item->GetChild(0);

		if (rowBtn != NULL)
			rowBtn->SetHitBox(2, v4(900.0f, 80.0f, 0.0f, 0.0f),
					  v4(0.0f, -30.0f, 0.0f, 0.0f));
	}
	return item;
}

ListItem *FriendSelectCreateMultiItem(Widget *parent)
{
	Plugin::TemplateOpenParam tmplParam;

	FriendSelectTemplateOpen(parent, IDParam(RCO_TMPL_MULTI_ITEM), tmplParam);
	ListItem *item = (ListItem *)parent->FindChild(IDParam(RCO_ITEM_ROOT), 0);

	if (item != NULL) {
		item->m_name.m_idhash = 0;
		Widget *rowBtn = item->GetChild(0);

		if (rowBtn != NULL)
			rowBtn->SetHitBox(2, v4(900.0f, 80.0f, 0.0f, 0.0f),
					  v4(0.0f, -30.0f, 0.0f, 0.0f));
	}
	return item;
}

/* ========================================================================= */
/* Profile-to-widget binding & unbinding                                     */
/* ========================================================================= */

void FriendSelectBindItemWidget(Widget *widget, const FriendUserProfile &profile)
{
	DrawObj *bgObj = widget->GetDrawObj(0);

	if (bgObj != NULL) {
		uint32_t bgColor = profile.m_panelBgColor;

		if (bgObj->m_color != bgColor && bgColor != FRIEND_SELECT_DEFAULT_PANEL_COLOR) {
			widget->SetMetaAlpha(0.0f, NULL, 65539, 0, 0.0f, NULL, NULL);
			Timer *t = new Timer(100.0f, 0, 0, -1, NULL);

			widget->SetMetaAlpha(1.0f, t, 65539, 0, 0.0f, NULL, NULL);
		}
		bgObj->m_color = bgColor;
	}

	Widget *panelImg = widget->FindChild(IDParam(RCO_ITEM_PANEL_IMAGE), 0);

	if (panelImg != NULL) {
		intrusive_ptr<Surface> curTex;

		panelImg->GetTexture(curTex);
		if (curTex.get() == NULL)
			panelImg->Hide(0xFF, NULL, NULL, 0.0f);
		if (!profile.m_panelUrl.empty())
			FriendLibRequestWidgetImage(profile.m_panelUrl.c_str(), panelImg);
	}

	CheckBox *checkBox = (CheckBox *)widget->FindChild(IDParam(RCO_ITEM_CHECK_BOX), 0);

	if (checkBox != NULL) {
		if (FriendSelectGetMultiData() != NULL && profile.m_relation != FRIEND_RELATION_NONE &&
		    !FriendSelectGetMultiData()->IsExisting(profile)) {
			checkBox->Show(0xFF, NULL, NULL, 0.0f);
			checkBox->SetCheck(FriendSelectMultiIsSelected(profile), 0.0f, false);
		} else {
			checkBox->Hide(0xFF, NULL, NULL, 0.0f);
		}
	}

	Widget *plusIcon = widget->FindChild(IDParam(RCO_ITEM_PLUS_ICON), 0);

	if (plusIcon != NULL) {
		if (profile.m_isPlus)
			plusIcon->Show(0xFF, NULL, NULL, 0.0f);
		else
			plusIcon->Hide(0xFF, NULL, NULL, 0.0f);
	}

	Widget *avatar = widget->FindChild(IDParam(RCO_ITEM_AVATAR), 0);

	if (avatar != NULL) {
		intrusive_ptr<Surface> curTex;

		avatar->GetTexture(curTex);
		if (curTex.get() == NULL) {
			intrusive_ptr<Surface> defTex =
				FriendSelectGetTexture(IDParam(RCO_ITEM_DEFAULT_AVATAR));

			avatar->SetTexture(defTex);
		}
		if (!profile.m_avatarUrl.empty())
			FriendLibRequestWidgetImage(profile.m_avatarUrl.c_str(), avatar);
	}

	Widget *onlineIdText = widget->FindChild(IDParam(RCO_ITEM_ONLINE_ID), 0);

	if (onlineIdText != NULL) {
		wstring idStr;

		Utf8ToUtf16(profile.m_npId.handle.data, &idStr);
		onlineIdText->SetString(wstring(idStr.c_str()));
	}

	Widget *statusIcon = widget->FindChild(IDParam(RCO_ITEM_STATUS_ICON), 0);

	if (statusIcon != NULL) {
		uint32_t statusTexHash = 0;
		bool showStatus = false;

		if (profile.m_relation >= FRIEND_RELATION_SELF &&
		    profile.m_relation <= FRIEND_RELATION_FRIEND) {
			if (profile.m_onlineStatus == FRIEND_STATUS_UNKNOWN) {
				statusTexHash = RCO_TEX_STATUS_UNKNOWN;
				showStatus = true;
			} else if (profile.m_onlineStatus == FRIEND_STATUS_OFFLINE) {
				statusTexHash = RCO_TEX_STATUS_OFFLINE;
				showStatus = true;
			} else if (profile.m_onlineStatus == FRIEND_STATUS_STANDBY) {
				statusTexHash = RCO_TEX_STATUS_STANDBY;
				showStatus = true;
			} else if (profile.m_onlineStatus == FRIEND_STATUS_ONLINE) {
				statusTexHash = RCO_TEX_STATUS_ONLINE;
				showStatus = true;
			}
		}
		if (showStatus) {
			intrusive_ptr<Surface> tex =
				FriendSelectGetTexture(IDParam(statusTexHash));

			statusIcon->SetTexture(tex);
			statusIcon->Show(0xFF, NULL, NULL, 0.0f);
		} else {
			statusIcon->Hide(0xFF, NULL, NULL, 0.0f);
		}
	}

	Widget *presGroup = widget->FindChild(IDParam(RCO_ITEM_PRESENCE_GROUP), 0);
	Widget *presText = widget->FindChild(IDParam(RCO_ITEM_PRESENCE_TEXT), 0);
	Widget *platIcon = widget->FindChild(IDParam(RCO_ITEM_PLATFORM_ICON), 0);

	if (presGroup != NULL && presText != NULL && platIcon != NULL) {
		if (!profile.m_gameTitleName.empty()) {
			presGroup->Show(0xFF, NULL, NULL, 0.0f);

			uint32_t platTexHash = 0;

			if (profile.m_platform == FRIEND_PLATFORM_PSVITA)
				platTexHash = RCO_TEX_PLAT_VITA;
			else if (profile.m_platform == FRIEND_PLATFORM_PS4)
				platTexHash = RCO_TEX_PLAT_PS4;
			else if (profile.m_platform == FRIEND_PLATFORM_PS3)
				platTexHash = RCO_TEX_PLAT_PS3;

			if (platTexHash != 0) {
				intrusive_ptr<Surface> tex =
					FriendSelectGetTexture(IDParam(platTexHash));

				platIcon->SetTexture(tex);
			} else {
				platIcon->SetTexture(intrusive_ptr<Surface>());
			}
			platIcon->SetMetaAlpha(1.0f, NULL, 65539, 0, 0.0f, NULL, NULL);
			platIcon->Show(0xFF, NULL, NULL, 0.0f);

			wstring titleW;

			Utf8ToUtf16(profile.m_gameTitleName, &titleW);
			presText->SetString(wstring(titleW.c_str()));
			presText->Show(0xFF, NULL, NULL, 0.0f);
			if (presText->m_parent != NULL) {
				platIcon->Hide(0xFF, NULL, NULL, 0.0f);
				presText->AddEventListener(
					FRIEND_EVENT_LAYOUT_DONE,
					new EventCBListener(OnPresenceTextLayout, NULL), false);
			}
		} else {
			presGroup->Hide(0xFF, NULL, NULL, 0.0f);
			platIcon->Hide(0xFF, NULL, NULL, 0.0f);
			presText->SetString(wstring());
			presText->Hide(0xFF, NULL, NULL, 0.0f);
		}
	}

	int32_t trophyLevel = profile.m_trophyLevel;
	Widget *trophyIcon = widget->FindChild(IDParam(RCO_ITEM_TROPHY_ICON), 0);

	if (trophyIcon != NULL) {
		if (trophyLevel >= 0)
			trophyIcon->Show(0xFF, NULL, NULL, 0.0f);
		else
			trophyIcon->Hide(0xFF, NULL, NULL, 0.0f);
	}

	Widget *trophyText = widget->FindChild(IDParam(RCO_ITEM_TROPHY_TEXT), 0);

	if (trophyText != NULL) {
		if (trophyLevel >= 0) {
			if (trophyLevel != 0) {
				FriendLibSetNumberText(trophyText, trophyLevel, 3);
			} else {
				const wchar_t *dash =
					FriendSelectGetString(IDParam(RCO_STR_TROPHY_DASH));

				trophyText->SetString(wstring(dash));
			}
			trophyText->Show(0xFF, NULL, NULL, 0.0f);
		} else {
			trophyText->SetString(wstring());
			trophyText->Hide(0xFF, NULL, NULL, 0.0f);
		}
	}
}

void FriendSelectUnbindItemWidget(Widget *widget, const FriendUserProfile &profile)
{
	if (!profile.m_panelUrl.empty()) {
		Widget *panelImg = widget->FindChild(IDParam(RCO_ITEM_PANEL_IMAGE), 0);

		if (panelImg != NULL) {
			FriendLibCancelWidgetImage(profile.m_panelUrl.c_str(), panelImg);
			panelImg->SetTexture(intrusive_ptr<Surface>());
		}
	}
	if (!profile.m_avatarUrl.empty()) {
		Widget *avatar = widget->FindChild(IDParam(RCO_ITEM_AVATAR), 0);

		if (avatar != NULL) {
			FriendLibCancelWidgetImage(profile.m_avatarUrl.c_str(), avatar);
			avatar->SetTexture(intrusive_ptr<Surface>());
		}
	}

	Widget *onlineIdText = widget->FindChild(IDParam(RCO_ITEM_ONLINE_ID), 0);

	if (onlineIdText != NULL)
		onlineIdText->SetString(wstring());

	Widget *presText = widget->FindChild(IDParam(RCO_ITEM_PRESENCE_TEXT), 0);

	if (presText != NULL) {
		presText->DeleteEventCallback(FRIEND_EVENT_LAYOUT_DONE, OnPresenceTextLayout, NULL);
		presText->SetString(wstring());
	}

	Widget *trophyText = widget->FindChild(IDParam(RCO_ITEM_TROPHY_TEXT), 0);

	if (trophyText != NULL)
		trophyText->SetString(wstring());
}

/* ========================================================================= */
/* Localized message formatter (%1..%9, %(10), %(11), %%, %A..%Z/%a..%z)     */
/* ========================================================================= */

wstring FriendSelectFormatMessage(const wchar_t *fmt, const wchar_t *a1, const wchar_t *a2,
				  const wchar_t *a3, const wchar_t *a4, const wchar_t *a5,
				  const wchar_t *a6, const wchar_t *a7, const wchar_t *a8,
				  const wchar_t *a9, const wchar_t *a10, const wchar_t *a11)
{
	const wchar_t *args[11] = { a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11 };
	wstring out;
	const wchar_t *cur = fmt;
	int32_t substitutions = 0;

	for (;;) {
		const wchar_t *pct = sce_paf_wcschr(cur, L'%');

		if (pct == NULL)
			break;
		out.append(cur, (size_t)(pct - cur));
		wchar_t ch = pct[1];

		if (ch == L'%') {
			out.append(1, L'%');
			cur = pct + 2;
		} else if (ch >= L'1' && ch <= L'9') {
			const wchar_t *arg = args[ch - L'1'];

			out.append(arg, sce_paf_wcslen(arg));
			cur = pct + 2;
		} else if ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z')) {
			if (s_messageTokenResolver != NULL) {
				const wchar_t *resolved =
					s_messageTokenResolver(ch, &s_messageTokenResolver);

				out.append(resolved, sce_paf_wcslen(resolved));
			}
			cur = pct + 2;
		} else if (ch == L'(' && pct[2] == L'1' &&
			   (pct[3] == L'0' || pct[3] == L'1') && pct[4] == L')') {
			const wchar_t *arg = args[9 + (pct[3] - L'0')];

			out.append(arg, sce_paf_wcslen(arg));
			cur = pct + 5;
		} else if (ch == L'\0') {
			return out;
		} else {
			cur = pct + 2;
		}
		++substitutions;
	}

	if (substitutions != 0)
		out.append(cur, sce_paf_wcslen(cur));
	else
		out.assign(fmt, (fmt != NULL) ? sce_paf_wcslen(fmt) : 0);
	return out;
}
