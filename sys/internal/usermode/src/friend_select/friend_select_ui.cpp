/*
 * SceFriendSelect: UI components (FriendSelectBackground,
 * FriendSelectErrorDialog, FriendSelectHeaderBar, FriendSelectHeader).
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/transition.h>
#include <framework.h>
#include <graphics/drawobj/text_obj.h>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <system/system.h>
#include <versatileparam/idparam.h>
#include <widget/core/event.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_button_base.h>
#include <widget/w_scene.h>
#include <widget/w_text.h>
#include "friend_select_internal.h"

extern "C" {
int sceRegMgrIsBlueScreen(void);
int SceShellUtil_BB54D049(int mode, uint32_t errorCode, int opt);
int SceCommonGuiDialog_696DB9C9(void *param);
int SceCommonGuiDialog_20B141F1(void *param);
int SceCommonGuiDialog_BC311DC0(int dialogId);
}

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::ui;

/* Background dimmer */
class FriendSelectBackground {
public:
	FriendSelectBackground();
	virtual ~FriendSelectBackground()
	{
		FriendSelectPageClose(IDParam(RCO_PAGE_BG));
	}
	void SetVisible(int32_t mode);

	int32_t m_reserved;
	Scene *m_scene;
};

FriendSelectBackground::FriendSelectBackground() : m_scene(NULL)
{
	m_scene = FriendSelectPageOpen(IDParam(RCO_PAGE_BG), 1);
	uint32_t bgColor = FriendSelectGetBgColor();
	Widget *plane = m_scene->FindChild(IDParam(RCO_PLANE_BG), 0);
	const float inv255 = 1.0f / 255.0f;
	math::v4 color((float)(bgColor & 0xFFU) * inv255,
		       (float)((bgColor >> 8) & 0xFFU) * inv255,
		       (float)((bgColor >> 16) & 0xFFU) * inv255,
		       (float)((bgColor >> 24) & 0xFFU) * inv255);
	plane->SetColor(color, NULL, Widget::ANIM_COLOR, 0, 0.0f, NULL, NULL);
	ScePafWidget_42C5A4D4(m_scene, 0, 0);
	m_scene->Hide(0xFF, NULL, NULL, 0.0f);
}

void FriendSelectBackground::SetVisible(int32_t mode)
{
	if (mode == 0) {
		m_scene->Show(0xFF, NULL, NULL, 0.0f);
		ScePafCommon_6CF3A3C8(0.0f, m_scene, (transition::Type)3, false, false);
	} else if (mode == 1) {
		ScePafCommon_B48C9BDC(0.0f, m_scene, (transition::Type)3, false, false);
		m_scene->Hide(0xFF, NULL, NULL, 0.0f);
	}
}

static FriendSelectBackground *s_background = NULL;

void FriendSelectBgCreate(void)
{
	s_background = new FriendSelectBackground();
}

void FriendSelectBgDestroy(void)
{
	if (s_background != NULL) {
		delete s_background;
		s_background = NULL;
	}
}

void FriendSelectBgShow(void)
{
	if (s_background != NULL)
		s_background->SetVisible(0);
}

void FriendSelectBgHide(void)
{
	if (s_background != NULL)
		s_background->SetVisible(1);
}

/* CommonGuiDialog parameter block matching SceCommonGuiDialog ABI */
struct CommonGuiDialogItem {
	wstring title;
	vector<wstring> subItems;
	wstring body;
	uint32_t reserved;
};

class CommonGuiDialogCallback {
public:
	virtual ~CommonGuiDialogCallback() {}
	virtual void Invoke(int32_t result) = 0;
};

template <typename Obj>
class CommonGuiDialogMemberCallback : public CommonGuiDialogCallback {
public:
	typedef void (Obj::*Method)(int32_t, void *);

	CommonGuiDialogMemberCallback(Obj *obj, Method method, void *arg = NULL)
		: m_obj(obj), m_method(method), m_arg(arg)
	{
	}

	virtual ~CommonGuiDialogMemberCallback() {}

	virtual void Invoke(int32_t result)
	{
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)(result, m_arg);
	}

	Obj *m_obj;
	Method m_method;
	void *m_arg;
};

struct CommonGuiDialogParam {
	Plugin *plugin;
	uint32_t headerFields[11];
	int32_t drawPriority;
	uint32_t extraFields[4];
	uint32_t errorCode;
	uint32_t dialogType;
	int32_t buttonType;
	int32_t iconType;
	int32_t dimMode;
	int32_t textAlign;
	int32_t flagReserved;
	wstring message;
	vector<wstring> buttonLabels;
	wstring titleText;
	vector<uint32_t> buttonIds;
	vector<CommonGuiDialogItem> items;
	bool flagA;
	bool flagB;
	bool flagC;
	uint8_t pad[13];
	CommonGuiDialogCallback *callback;
	int32_t reserved;
};

class FriendSelectErrorDialog {
public:
	FriendSelectErrorDialog() : m_dialogId(-1) {}
	virtual ~FriendSelectErrorDialog() {}

	void Open(int32_t errorCode);
	void OnClosed(int32_t result, void *arg);

	int32_t m_dialogId;
};

static FriendSelectErrorDialog *s_errorDialog = NULL;
static Callback0 *s_errorCallback = NULL;

void FriendSelectErrorInit(void)
{
	s_errorDialog = NULL;
	s_errorCallback = NULL;
}

void FriendSelectErrorClose(void)
{
	if (s_errorDialog != NULL) {
		if (s_errorDialog->m_dialogId >= 0)
			SceCommonGuiDialog_BC311DC0(s_errorDialog->m_dialogId);
		delete s_errorDialog;
		s_errorDialog = NULL;
		if (s_errorCallback != NULL) {
			delete s_errorCallback;
			s_errorCallback = NULL;
		}
	}
}

void FriendSelectErrorTerm(void)
{
	if (s_errorDialog != NULL) {
		if (s_errorDialog->m_dialogId >= 0)
			SceCommonGuiDialog_BC311DC0(s_errorDialog->m_dialogId);
		delete s_errorDialog;
		s_errorDialog = NULL;
		if (s_errorCallback != NULL) {
			delete s_errorCallback;
			s_errorCallback = NULL;
		}
	}
}

static void ResolveErrorMessage(int32_t *errorCode, wstring &outMsg, int32_t *outDimMode,
				int32_t *outIconType)
{
	*outDimMode = 0;
	*outIconType = 0;
	IDParam strId;

	if (*errorCode == FRIEND_SELECT_ERROR_MAX_SELECTED) {
		wchar_t numBuf[10];
		sce_paf_swprintf(numBuf, 10, L"%d", FriendSelectGetMultiMaxTotal());
		const wchar_t *fmt = FriendSelectGetString(IDParam(RCO_STR_MAX_SELECT_ERR));
		outMsg = FriendSelectFormatMessage(fmt, numBuf);
		return;
	}
	if (*errorCode == FRIEND_SELECT_ERROR_HTTP_STATUS) {
		strId.m_idhash = RCO_STR_HTTP_ERR;
	} else if (*errorCode == FRIEND_SELECT_ERROR_NP_OFFLINE) {
		strId.m_idhash = RCO_STR_NP_OFFLINE_ERR;
	} else {
		*outDimMode = 2;
		*outIconType = 1;
		return;
	}
	const wchar_t *msg = FriendSelectGetString(strId);
	if (msg != NULL)
		outMsg = msg;
}

void FriendSelectErrorDialog::Open(int32_t errorCode)
{
	CommonGuiDialogParam param;
	SceCommonGuiDialog_696DB9C9(&param);

	param.errorCode = 0;
	param.dialogType = 0;
	param.buttonType = -2;
	param.iconType = 2;
	param.dimMode = 2;
	param.textAlign = 2;
	param.flagReserved = 0;
	param.flagA = false;
	param.flagB = true;
	param.flagC = false;
	param.callback = NULL;
	param.reserved = 0;

	Plugin *plugin = FriendSelectGetPlugin();
	param.plugin = plugin;
	param.drawPriority = plugin->m_draw_priority - 6;
	param.dialogType = (param.dialogType & ~0xFFU) | 5U;
	param.callback = new CommonGuiDialogMemberCallback<FriendSelectErrorDialog>(
		this, &FriendSelectErrorDialog::OnClosed);
	param.errorCode = (uint32_t)errorCode;

	int32_t errVal = errorCode;
	ResolveErrorMessage(&errVal, param.message, &param.dimMode, &param.iconType);
	param.errorCode = (uint32_t)errVal;

	if (param.message.empty() && ((param.errorCode >> 16) & 0xFFFU) == 0xDU) {
		int32_t mode = (sceRegMgrIsBlueScreen() == 1) ? 4 : 5;
		SceShellUtil_BB54D049(mode, param.errorCode, 0);
	} else {
		int32_t dlgId = SceCommonGuiDialog_20B141F1(&param);
		if (dlgId >= 0)
			m_dialogId = dlgId;
	}
}

void FriendSelectErrorDialog::OnClosed(int32_t result, void *arg)
{
	(void)result;
	(void)arg;
	m_dialogId = -1;
	if (s_errorDialog != NULL) {
		if (s_errorDialog->m_dialogId >= 0)
			SceCommonGuiDialog_BC311DC0(s_errorDialog->m_dialogId);
		delete s_errorDialog;
		s_errorDialog = NULL;
		if (s_errorCallback != NULL) {
			Callback0 *cb = s_errorCallback;
			s_errorCallback = NULL;
			cb->Invoke();
			delete cb;
		}
	}
}

int32_t FriendSelectErrorShow(int32_t errorCode, Callback0 *onDismiss)
{
	if (s_errorDialog != NULL) {
		if (s_errorDialog->m_dialogId >= 0)
			SceCommonGuiDialog_BC311DC0(s_errorDialog->m_dialogId);
		delete s_errorDialog;
		s_errorDialog = NULL;
		if (s_errorCallback != NULL) {
			Callback0 *oldCb = s_errorCallback;
			s_errorCallback = NULL;
			delete oldCb;
		}
	}
	FriendLibPopupClose();
	s_errorDialog = new FriendSelectErrorDialog();
	if (s_errorDialog != NULL) {
		s_errorDialog->Open(errorCode);
		s_errorCallback = onDismiss;
		return 0;
	}
	if (onDismiss != NULL)
		delete onDismiss;
	return FRIEND_SELECT_ERROR_NOMEM;
}

/* Header bar controller */
static FriendSelectHeader *s_activeHeader = NULL;
static Scene *s_headerScene = NULL;
static Widget *s_headerCancelBtn = NULL;
static Widget *s_headerSortBtn = NULL;
static UpdateButtonWidget *s_headerUpdateBtn = NULL;
static Widget *s_headerMultiGroup = NULL;
static ButtonBase *s_headerClearBtn = NULL;
static Widget *s_headerCountGroup = NULL;
static Widget *s_headerCurrentText = NULL;
static Widget *s_headerSlashText = NULL;
static Text *s_headerMaxText = NULL;
static ButtonBase *s_headerOkBtn = NULL;
static bool s_headerMultiVisible = false;

static void OnHeaderButtonDecide(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	if (s_activeHeader != NULL && s_activeHeader->m_callback != NULL)
		s_activeHeader->m_callback->Invoke((int32_t)(intptr_t)userData);
}

void FriendSelectHeaderBarInit(void)
{
	s_headerScene = FriendSelectPageOpen(IDParam(RCO_PAGE_HEADER), 3);

	s_headerCancelBtn = s_headerScene->FindChild(IDParam(RCO_BTN_CANCEL), 0);
	s_headerCancelBtn->AddEventListener(
		ButtonBase::CB_BTN_DECIDE, new EventCBListener(OnHeaderButtonDecide, (void *)0),
		false);

	s_headerSortBtn = s_headerScene->FindChild(IDParam(RCO_BTN_SORT), 0);
	s_headerSortBtn->AddEventListener(
		ButtonBase::CB_BTN_DECIDE, new EventCBListener(OnHeaderButtonDecide, (void *)1),
		false);

	s_headerUpdateBtn = (UpdateButtonWidget *)s_headerScene->FindChild(IDParam(RCO_BTN_UPDATE),
									   0);
	s_headerUpdateBtn->AddEventListener(
		ButtonBase::CB_BTN_DECIDE, new EventCBListener(OnHeaderButtonDecide, (void *)2),
		false);

	s_headerMultiGroup = s_headerScene->FindChild(IDParam(RCO_MULTI_GROUP), 0);
	s_headerClearBtn = (ButtonBase *)s_headerMultiGroup->FindChild(IDParam(RCO_BTN_CLEAR), 0);
	s_headerClearBtn->AddEventListener(
		ButtonBase::CB_BTN_DECIDE, new EventCBListener(OnHeaderButtonDecide, (void *)3),
		false);

	s_headerCountGroup = s_headerMultiGroup->FindChild(IDParam(RCO_COUNT_GROUP), 0);
	s_headerSlashText = s_headerMultiGroup->FindChild(IDParam(RCO_COUNT_SLASH), 0);
	s_headerSlashText->SetString(wstring(L"/"));

	s_headerCurrentText = s_headerMultiGroup->FindChild(IDParam(RCO_COUNT_CURRENT), 0);
	s_headerMaxText = (Text *)s_headerMultiGroup->FindChild(IDParam(RCO_COUNT_MAX), 0);

	s_headerOkBtn = (ButtonBase *)s_headerMultiGroup->FindChild(IDParam(RCO_BTN_OK), 0);
	s_headerOkBtn->AddEventListener(
		ButtonBase::CB_BTN_DECIDE, new EventCBListener(OnHeaderButtonDecide, (void *)4),
		false);

	int32_t lang = Framework::Instance()->GetLocale();
	if (lang == 8 || lang == 16) {
		Text *titleText = (Text *)s_headerScene->FindChild(IDParam(RCO_HEADER_TITLE), 0);
		titleText->SetStyleAttribute((TextStyleAttribute)14, 0, 0, false);
		TextObj *sortTextObj = (TextObj *)s_headerSortBtn->GetDrawObj(1);
		sortTextObj->SetStyleAttribute((TextStyleAttribute)14, 0, 0, false);
	}
	if (lang == 16 && (ScePafMisc_C51EF81D() & 1) != 0) {
		TextObj *clearTextObj = (TextObj *)s_headerClearBtn->GetDrawObj(1);
		clearTextObj->SetStyleAttribute((TextStyleAttribute)1, 0, 0, 20.0f);
	}

	s_headerMultiGroup->Hide(0xFF, NULL, NULL, 0.0f);
	s_headerMultiVisible = false;
	s_headerScene->Hide(0xFF, NULL, NULL, 0.0f);
}

void FriendSelectHeaderBarTerm(void)
{
	FriendSelectPageClose(IDParam(RCO_PAGE_HEADER));
	s_activeHeader = NULL;
	s_headerScene = NULL;
	s_headerCancelBtn = NULL;
	s_headerSortBtn = NULL;
	s_headerUpdateBtn = NULL;
	s_headerMultiGroup = NULL;
	s_headerClearBtn = NULL;
	s_headerCountGroup = NULL;
	s_headerCurrentText = NULL;
	s_headerSlashText = NULL;
	s_headerMaxText = NULL;
	s_headerOkBtn = NULL;
}

void FriendSelectHeaderBarShow(int32_t transitionMode)
{
	if (s_headerScene != NULL && transitionMode == 1) {
		s_headerScene->Show(0xFF, NULL, NULL, 0.0f);
		ScePafCommon_6CF3A3C8(0.0f, s_headerScene, (transition::Type)3, false, false);
	}
}

void FriendSelectHeaderBarHide(int32_t transitionMode)
{
	if (s_headerScene != NULL && transitionMode == 2) {
		ScePafCommon_B48C9BDC(0.0f, s_headerScene, (transition::Type)3, false, false);
		s_headerScene->Hide(0xFF, NULL, NULL, 0.0f);
	}
}

Widget *FriendSelectHeaderGetButton(int32_t buttonId)
{
	switch (buttonId) {
	case 0:
		return s_headerCancelBtn;
	case 1:
		return s_headerSortBtn;
	case 2:
		return s_headerUpdateBtn;
	case 3:
		return s_headerClearBtn;
	case 4:
		return s_headerOkBtn;
	default:
		return NULL;
	}
}

/* FriendSelectHeader */
FriendSelectHeader::FriendSelectHeader() : m_callback(NULL)
{
	s_activeHeader = this;
}

FriendSelectHeader::~FriendSelectHeader()
{
	if (m_callback != NULL) {
		delete m_callback;
		m_callback = NULL;
	}
	if (s_activeHeader == this)
		s_activeHeader = NULL;
}

void FriendSelectHeader::Configure(const FriendSelectHeaderParam &param)
{
	if (m_callback != NULL)
		delete m_callback;
	m_callback = param.actionCallback;
	if (s_activeHeader == this) {
		if (param.showMultiControls) {
			if (!s_headerMultiVisible) {
				s_headerMultiVisible = true;
				s_headerMultiGroup->Show(0xFF, NULL, NULL, 0.0f);
				ScePafCommon_6CF3A3C8(0.0f, s_headerMultiGroup,
						      (transition::Type)8, false, false);
			}
		} else if (s_headerMultiVisible) {
			s_headerMultiVisible = false;
			ScePafCommon_B48C9BDC(0.0f, s_headerMultiGroup, (transition::Type)8, false,
					      false);
			s_headerMultiGroup->Hide(0xFF, NULL, NULL, 0.0f);
		}
	}
}

void FriendSelectHeader::SetSortMode(int32_t sortMode)
{
	if (s_activeHeader == this && s_headerSortBtn != NULL) {
		IDParam strId;
		strId.m_idhash = (sortMode != 0) ? RCO_STR_SORT_BY_STATUS : RCO_STR_SORT_BY_ID;
		const wchar_t *label = FriendSelectGetString(strId);
		s_headerSortBtn->SetString(wstring(label != NULL ? label : L""));
	}
}

void FriendSelectHeader::SetUpdating(bool updating)
{
	if (s_activeHeader == this && s_headerUpdateBtn != NULL) {
		if (updating)
			s_headerUpdateBtn->StartSpin();
		else
			s_headerUpdateBtn->StopSpin();
	}
}

bool FriendSelectHeader::IsUpdating(void)
{
	if (s_activeHeader == this && s_headerUpdateBtn != NULL)
		return s_headerUpdateBtn->m_spinning;
	return false;
}

void FriendSelectHeader::SetClearEnabled(bool enabled, bool animate)
{
	if (s_activeHeader == this && s_headerClearBtn != NULL) {
		if (enabled)
			s_headerClearBtn->Enable(animate);
		else
			s_headerClearBtn->Disable(animate);
	}
}

void FriendSelectHeader::SetSelectedCount(int32_t count)
{
	if (s_activeHeader == this && s_headerCurrentText != NULL)
		FriendLibSetNumberText(s_headerCurrentText, count, 9);
}

void FriendSelectHeader::SetMaxCount(int32_t maxCount)
{
	if (s_activeHeader == this && s_headerMaxText != NULL) {
		FriendLibSetNumberText(s_headerMaxText, maxCount, 9);
		if (s_headerCountGroup != NULL) {
			int32_t digits = 0;
			int32_t n = maxCount;
			while (n != 0) {
				n /= 10;
				++digits;
			}
			float fontSize = 0.0f;
			s_headerMaxText->GetStyleAttribute((TextStyleAttribute)16, 0, fontSize);
			float x = -34.0f + ((float)digits * -16.0f) * (fontSize / 24.0f);
			math::v4 pos(x, 0.0f, 0.0f, 0.0f);
			s_headerCountGroup->SetPos(pos, NULL, Widget::ANIM_MOVE, 0, 0.0f, NULL,
						   NULL);
		}
	}
}

void FriendSelectHeader::SetOkEnabled(bool enabled, bool animate)
{
	if (s_activeHeader == this && s_headerOkBtn != NULL) {
		if (enabled)
			s_headerOkBtn->Enable(animate);
		else
			s_headerOkBtn->Disable(animate);
	}
}
