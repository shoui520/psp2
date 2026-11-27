/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * signup_library shared components — EulaUrlBuilder, TextInputField,
 * DobSelectButton/DobSelectDialog, DialogBase open/close PlaneShadow
 * animations, ListSelectButton/ListSelectDialog, NumSpinController,
 * HttpImageClient, FormatMessage, and the 79-entry locale number format table.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/math.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <buffer/buffer.h>
#include <common/char_encoding.h>
#include <common/shared_ptr.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <image/image.h>
#include <job/job.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <widget/core/animlistener.h>
#include <widget/core/event.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_button_base.h>
#include <widget/w_list_item.h>
#include <widget/w_list_view.h>
#include <widget/w_num_spin.h>
#include <widget/w_plane.h>
#include <widget/w_scroll_view_base.h>
#include <widget/w_spin_base.h>
#include "signin_ext_internal.h"

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::image;
using namespace paf::job;
using namespace paf::ui;

/* ---- EULA URL builder ---- */

static const char *const s_eulaDocSuffixes[8] = {
	"_tosua_",
	"_privacy_",
	"_about_",
	"_health_",
	"_rating_",
	"_copyright_",
	"_softwareusage_",
	"_tosua_privacy_"
};

EulaUrlBuilder::EulaUrlBuilder()
	: m_versioned(0),
	  m_docType(1),
	  m_country(NULL),
	  m_language(NULL),
	  m_pageIndex(0),
	  m_url(),
	  m_dirty(true)
{
}

EulaUrlBuilder::~EulaUrlBuilder()
{
}

void EulaUrlBuilder::SetVersioned(int32_t versioned)
{
	if (m_versioned != versioned) {
		m_versioned = versioned;
		m_dirty = true;
	}
}

void EulaUrlBuilder::SetDocType(int32_t docType)
{
	if (m_docType != docType) {
		m_docType = docType;
		m_dirty = true;
	}
}

void EulaUrlBuilder::SetCountry(const char *country)
{
	if (m_country != country) {
		m_country = country;
		m_dirty = true;
	}
}

void EulaUrlBuilder::SetLanguage(const char *language)
{
	if (m_language != language) {
		m_language = language;
		m_dirty = true;
	}
}

void EulaUrlBuilder::SetPageIndex(int32_t pageIndex)
{
	if (m_pageIndex != pageIndex) {
		m_pageIndex = pageIndex;
		m_dirty = true;
	}
}

const char *EulaUrlBuilder::GetUrl(void)
{
	if (m_dirty) {
		char buf[128];
		const char *suffix = s_eulaDocSuffixes[m_docType];
		if (m_versioned != 0) {
			if (m_pageIndex <= 0) {
				sce_paf_snprintf(buf, sizeof(buf), "%s%s%s%s%s",
						 "http://legaldoc.dl.playstation.net/ps3-eula/psn/",
						 m_country, suffix, m_language, ".dat");
			} else {
				sce_paf_snprintf(buf, sizeof(buf), "%s%s%s%s%02d%s",
						 "http://legaldoc.dl.playstation.net/ps3-eula/psn/",
						 m_country, suffix, m_language, m_pageIndex, ".html");
			}
		} else {
			sce_paf_snprintf(buf, sizeof(buf), "%s%s%s%s%s",
					 "http://legaldoc.dl.playstation.net/ps3-eula/psn/",
					 m_country, suffix, m_language, ".html");
		}
		m_url = buf;
		m_dirty = false;
	}
	return m_url.c_str();
}

/* ---- TextInputField ---- */

TextInputField::TextInputField(int32_t maxLen, int32_t fieldType)
	: m_buf(NULL), m_maxLen(maxLen), m_length(0), m_fieldType(fieldType)
{
	SceSize bytes = (SceSize)(2 * (maxLen + 1));
	m_buf = new wchar_t[maxLen + 1];
	sce_paf_memset(m_buf, 0, bytes);
}

TextInputField::~TextInputField()
{
	if (m_buf != NULL) {
		if (m_fieldType == 7)
			sce_paf_memset(m_buf, 0, (SceSize)(2 * m_maxLen + 2));
		delete[] m_buf;
	}
}

void TextInputField::ToUtf8(string *out) const
{
	Utf16ToUtf8(m_buf, out);
}

void TextInputField::SetWide(const wchar_t *src)
{
	if (src != NULL) {
		sce_paf_wcsncpy(m_buf, src, m_maxLen);
		m_length = (int32_t)sce_paf_wcslen(m_buf);
	} else {
		m_buf[0] = L'\0';
		m_length = 0;
	}
}

void TextInputField::SetUtf8(const char *src)
{
	wstring wide;
	Utf8ToUtf16(src, &wide);
	SetWide(wide.c_str());
}

void TextInputField::Clear(void)
{
	SetWide(NULL);
}

bool TextInputField::Equals(const TextInputField &other) const
{
	return m_length != 0 && other.m_length != 0 &&
	       sce_paf_wcsncmp(m_buf, other.m_buf, m_maxLen) == 0;
}

/* ---- DialogBase + PlaneShadow Open/Close Animations ---- */

class DialogOpenAnimListener : public AnimListener {
public:
	DialogOpenAnimListener(Timer *timer, Widget *target);
	virtual ~DialogOpenAnimListener()
	{
		if (m_shadow != NULL)
			m_shadow->DestroyWidget();
	}

	virtual void OnUpdate(void)
	{
		if (m_shadow == NULL)
			return;
		float t = m_timer->m_mapped_x;
		if (t < 0.8f) {
			float s = (t / 0.8f) * 0.3f + 0.8f;
			m_shadow->SetScale_core(math::v4(s, s, s, s), 0xFF04);
			m_shadow->SetMetaAlpha_core(t / 0.8f);
		} else {
			float s = 1.1f - ((t - 0.8f) / 0.2f) * 0.1f;
			m_shadow->SetScale_core(math::v4(s, s, s, s), 0xFF04);
		}
	}

	virtual void OnFinish(void)
	{
		if (m_shadow != NULL) {
			m_shadow->DestroyWidget();
			m_shadow = NULL;
		}
		if (m_widget != NULL)
			m_widget->SetMetaAlpha_core(1.0f);
	}

	Widget *m_shadow;
};

DialogOpenAnimListener::DialogOpenAnimListener(Timer *timer, Widget *target)
	: AnimListener(timer, target, NULL, NULL, NULL), m_shadow(NULL)
{
	if (m_widget != NULL) {
		m_shadow = SigninExtCreateWidget(
			m_widget->m_parent, PlaneShadow::TypeName(),
			IDParam(0U), IDParam(RCO_STYLE_DIALOG_SHADOW));
		if (m_widget->m_update_flag != 0)
			m_widget->UpdateMatrix();
		m_shadow->SetPos(m_widget->m_pos, NULL, Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		m_shadow->SetSize(*m_widget->GetSize(0), NULL, Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		m_shadow->SetMetaAlpha(0.0f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
		m_widget->SetMetaAlpha(0.0f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
	}
}

class DialogCloseAnimListener : public AnimListener {
public:
	typedef void (*DoneFunc)(uint32_t pageId);

	DialogCloseAnimListener(Timer *timer, Widget *target, DoneFunc doneFunc, uint32_t pageId);
	virtual ~DialogCloseAnimListener()
	{
		if (m_shadow != NULL)
			m_shadow->DestroyWidget();
	}

	virtual void OnUpdate(void)
	{
		if (m_shadow == NULL)
			return;
		float t = m_timer->m_mapped_x;
		float s = 1.0f - t * 0.2f;
		m_shadow->SetScale_core(math::v4(s, s, s, s), 0xFF04);
		m_shadow->SetMetaAlpha_core(1.0f - t);
	}

	virtual void OnFinish(void)
	{
		if (m_shadow != NULL) {
			m_shadow->DestroyWidget();
			m_shadow = NULL;
		}
		if (m_doneFunc != NULL)
			m_doneFunc(m_pageId);
	}

	Widget *m_shadow;
	DoneFunc m_doneFunc;
	uint32_t m_pageId;
};

DialogCloseAnimListener::DialogCloseAnimListener(Timer *timer, Widget *target, DoneFunc doneFunc,
						 uint32_t pageId)
	: AnimListener(timer, target, NULL, NULL, NULL),
	  m_shadow(NULL), m_doneFunc(doneFunc), m_pageId(pageId)
{
	if (m_widget != NULL) {
		m_shadow = SigninExtCreateWidget(
			m_widget->m_parent, PlaneShadow::TypeName(),
			IDParam(0U), IDParam(RCO_STYLE_DIALOG_SHADOW));
		if (m_widget->m_update_flag != 0)
			m_widget->UpdateMatrix();
		m_shadow->SetPos(m_widget->m_pos, NULL, Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		m_shadow->SetSize(*m_widget->GetSize(0), NULL, Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		m_widget->SetMetaAlpha(0.0f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
	}
}

static void CloseDialogPageById(uint32_t pageId)
{
	SigninExtPageClose(IDParam(pageId));
}

DialogBase::DialogBase()
	: m_pageId(0U),
	  m_commonDialogId(-1),
	  m_parentScene(NULL),
	  m_scene(NULL),
	  m_panel(NULL)
{
}

DialogBase::~DialogBase()
{
}

void DialogBase::PrepareAndShow(bool playOpenAnim)
{
	if (m_parentScene != NULL) {
		Timer *t = new Timer(250.0f, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
		m_parentScene->SetMetaAlpha(0.5f, t, Widget::ANIM_META_ALPHA, 2, 0.0f, NULL, NULL);
	}

	if (m_pageId.GetIDHash() != 0) {
		if (m_panel != NULL) {
			ScePafWidget_42C5A4D4(m_panel, 0, 0);
			Plane *blocker = new Plane(m_panel->m_parent, NULL);
			blocker->SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f), NULL,
					  Widget::ANIM_COLOR, 0, 0.0f, NULL, NULL);
			blocker->SetAdjust(1, 1, 1);
			blocker->SetParent(blocker->m_scene, NULL, 5);
		}
		if (playOpenAnim)
			PlayOpenAnim();
	}
}

void DialogBase::PlayOpenAnim(void)
{
	if (m_panel == NULL)
		return;
	Timer *t = new Timer(200.0f, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
	DialogOpenAnimListener *anim = new DialogOpenAnimListener(t, m_panel);
	m_panel->SetAnimListener(0xFF0000, anim, 2);
}

void DialogBase::PlayCloseAnim(void)
{
	if (m_parentScene != NULL) {
		Timer *t = new Timer(250.0f, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
		m_parentScene->SetMetaAlpha(1.0f, t, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
	}
	if (m_panel != NULL) {
		Timer *t = new Timer(200.0f, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
		DialogCloseAnimListener *anim = new DialogCloseAnimListener(
			t, m_panel, CloseDialogPageById, m_pageId.GetIDHash());
		m_panel->SetAnimListener(0xFF0000, anim, 2);
	}
}

void DialogBase::Close(bool animate)
{
	if (animate) {
		PlayCloseAnim();
		return;
	}

	if (m_parentScene != NULL)
		m_parentScene->SetMetaAlpha(1.0f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);

	if (m_pageId.GetIDHash() != 0)
		SigninExtPageClose(m_pageId);
	else if (m_commonDialogId >= 0)
		SceCommonGuiDialog_BC311DC0(m_commonDialogId);
}

/* ---- NumSpinController ---- */

NumSpinController::NumSpinController()
	: m_spin(NULL), m_model(NULL), m_onChanged(NULL)
{
}

NumSpinController::~NumSpinController()
{
	if (m_onChanged != NULL)
		delete m_onChanged;
	if (m_spin != NULL)
		m_spin->DeleteEventListener(SpinBase::CB_VALUE_CHANGED, NULL);
}

void NumSpinController::Bind(Widget *widget, SpinFieldModel *model, Callback1<int32_t> *onChanged)
{
	NumSpin *spin = NULL;
	if (widget != NULL && widget->IsInherit(NumSpin::TypeName()))
		spin = static_cast<NumSpin *>(widget);

	m_spin = spin;
	if (spin == NULL) {
		if (onChanged != NULL)
			delete onChanged;
		return;
	}

	m_model = model;
	m_onChanged = onChanged;
	spin->SetRange(model->GetMin(), model->GetMax());
	spin->SetValue(model->GetValue());
	ScePafWidget_176F3B21(spin, 0);

	for (int32_t i = SpinBase::OBJ_DRUM; i <= SpinBase::OBJ_DRUM_END; ++i) {
		DrawObj *obj = spin->GetDrawObj(i);
		if (obj != NULL && obj->IsInherit(TextObj::DrawObjType())) {
			TextObj *textObj = static_cast<TextObj *>(obj);
			textObj->SetStyleAttribute(TextStyleAttribute_Shadow, 0, 0, true);
			textObj->SetStyleAttribute(TextStyleAttribute_Bold, 0, 0, true);
		}
	}

	EventListener *listener = new MemberEventListener<NumSpinController>(
		this, &NumSpinController::OnValueChanged, NULL);
	spin->AddEventListener(SpinBase::CB_VALUE_CHANGED, listener, false);
}

void NumSpinController::SyncFromModel(void)
{
	if (m_spin == NULL || m_model == NULL)
		return;

	int32_t curMin = m_spin->m_min;
	int32_t curMax = m_spin->m_max;
	if (curMin != m_model->GetMin() || curMax != m_model->GetMax())
		m_spin->SetRange(m_model->GetMin(), m_model->GetMax());

	if (m_spin->m_value != m_model->GetValue())
		m_spin->SetValue(m_model->GetValue());
}

void NumSpinController::OnValueChanged(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)userData;
	if (m_onChanged != NULL)
		m_onChanged->Invoke(e->m_data[0]);
}

/* ---- DobSelectDialog & DobSelectButton ---- */

struct DobSelectDialogParam {
	Widget *anchorWidget;
	const wchar_t *title;
	BirthDateModel *model;
	Callback1<BirthDateModel *> *callback;
};

class DobSelectDialog : public DialogBase {
public:
	DobSelectDialog();
	virtual ~DobSelectDialog();

	void Open(const DobSelectDialogParam *param);
	void OnFieldChanged(int32_t value, int32_t fieldIndex);
	void OnOkPressed(int32_t type, Handler *self, Event *e, void *userData);
	void OnCancelPressed(int32_t type, Handler *self, Event *e, void *userData);

	NumSpinController m_spinYear;
	NumSpinController m_spinMonth;
	NumSpinController m_spinDay;
	ScrollListEdgeHelper m_edgeBinder;
	BirthDateModel m_model;
	ButtonBase *m_okButton;
};

DobSelectDialog::DobSelectDialog()
	: m_spinYear(), m_spinMonth(), m_spinDay(),
	  m_edgeBinder(), m_model(), m_okButton(NULL)
{
}

DobSelectDialog::~DobSelectDialog()
{
}

void DobSelectDialog::Open(const DobSelectDialogParam *param)
{
	m_parentScene = param->anchorWidget->m_scene;
	m_pageId = IDParam(RCO_PAGE_DOB_DIALOG);
	m_model.CopyFrom(*param->model);

	if (m_model.m_year.m_value < 0)
		m_model.SetYear(m_model.m_year.GetMin());
	if (m_model.m_month.m_value < 0)
		m_model.SetMonth(m_model.m_month.GetMin());
	if (m_model.m_day.m_value < 0)
		m_model.SetDay(m_model.m_day.GetMin());

	Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = 1;
	m_scene = SigninExtPageOpenWithParam(m_pageId, openParam);
	m_panel = m_scene->FindChild(IDParam(RCO_DIALOG_PANEL), 0);
	m_okButton = static_cast<ButtonBase *>(m_scene->FindChild(IDParam(RCO_BTN_OK), 0));
	Widget *cancelBtn = m_scene->FindChild(IDParam(RCO_BTN_CANCEL), 0);
	Widget *spinBox = m_scene->FindChild(IDParam(RCO_DOB_SPIN_BOX), 0);

	Widget *spin1 = spinBox->FindChild(IDParam(RCO_DOB_SPIN_1), 0);
	Widget *slash1 = spinBox->FindChild(IDParam(RCO_DOB_SLASH_1), 0);
	Widget *spin2 = spinBox->FindChild(IDParam(RCO_DOB_SPIN_2), 0);
	Widget *slash2 = spinBox->FindChild(IDParam(RCO_DOB_SLASH_2), 0);
	Widget *spin3 = spinBox->FindChild(IDParam(RCO_DOB_SPIN_3), 0);
	Widget *titleText = m_scene->FindChild(IDParam(RCO_DIALOG_TITLE), 0);

	(void)slash1;
	(void)slash2;
	SigninExtSetAutoShrinkText(titleText, param->title);

	Widget *yearW;
	Widget *monthW;
	Widget *dayW;
	if (m_model.m_dateFormat == 0) {
		yearW = spin1;
		monthW = spin2;
		dayW = spin3;
	} else if (m_model.m_dateFormat == 1) {
		monthW = spin1;
		dayW = spin2;
		yearW = spin3;
	} else {
		dayW = spin1;
		monthW = spin2;
		yearW = spin3;
	}

	m_spinYear.Bind(yearW, &m_model.m_year,
			new MemberCallback1<DobSelectDialog, int32_t, int32_t>(
				this, &DobSelectDialog::OnFieldChanged, 0));
	m_spinYear.SyncFromModel();

	m_spinMonth.Bind(monthW, &m_model.m_month,
			 new MemberCallback1<DobSelectDialog, int32_t, int32_t>(
				 this, &DobSelectDialog::OnFieldChanged, 1));
	m_spinMonth.SyncFromModel();

	m_spinDay.Bind(dayW, &m_model.m_day,
		       new MemberCallback1<DobSelectDialog, int32_t, int32_t>(
			       this, &DobSelectDialog::OnFieldChanged, 2));
	m_spinDay.SyncFromModel();

	m_okButton->AddEventListener(
		ButtonBase::CB_BTN_DECIDE,
		new MemberEventListener<DobSelectDialog>(
			this, &DobSelectDialog::OnOkPressed, NULL),
		false);
	cancelBtn->AddEventListener(
		ButtonBase::CB_BTN_DECIDE,
		new MemberEventListener<DobSelectDialog>(
			this, &DobSelectDialog::OnCancelPressed, NULL),
		false);

	spin1->SetFocus(NULL);

	if (m_model.m_status == 2)
		m_okButton->Disable(true);
	else
		m_okButton->Enable(true);

	SigninExtPlayCommonSE(4);
	PrepareAndShow(true);
}

void DobSelectDialog::OnFieldChanged(int32_t value, int32_t fieldIndex)
{
	m_model.SetField(fieldIndex, value);
	m_spinYear.SyncFromModel();
	m_spinMonth.SyncFromModel();
	m_spinDay.SyncFromModel();
	if (m_model.m_status == 2)
		m_okButton->Disable(false);
	else
		m_okButton->Enable(false);
}

static DobSelectDialog *s_dobDialog = NULL;
static Callback1<BirthDateModel *> *s_dobDialogCallback = NULL;

void SigninExtDobDialogInit(void)
{
}

void SigninExtDobDialogCloseImmediate(void)
{
	if (s_dobDialog != NULL) {
		s_dobDialog->Close(false);
		delete s_dobDialog;
		s_dobDialog = NULL;
		if (s_dobDialogCallback != NULL) {
			delete s_dobDialogCallback;
			s_dobDialogCallback = NULL;
		}
	}
}

void SigninExtDobDialogTerm(void)
{
	if (s_dobDialog != NULL) {
		s_dobDialog->Close(false);
		delete s_dobDialog;
		s_dobDialog = NULL;
		if (s_dobDialogCallback != NULL) {
			delete s_dobDialogCallback;
			s_dobDialogCallback = NULL;
		}
	}
}

static void CompleteDobDialog(BirthDateModel *selected)
{
	if (s_dobDialog == NULL)
		return;

	SigninExtPlayCommonSE(selected != NULL ? 0 : 1);
	s_dobDialog->Close(true);
	delete s_dobDialog;
	s_dobDialog = NULL;

	if (s_dobDialogCallback != NULL) {
		Callback1<BirthDateModel *> *cb = s_dobDialogCallback;
		s_dobDialogCallback = NULL;
		cb->Invoke(selected);
		delete cb;
	}
}

void DobSelectDialog::OnOkPressed(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	CompleteDobDialog(&m_model);
}

void DobSelectDialog::OnCancelPressed(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	CompleteDobDialog(NULL);
}

static int32_t ShowDobDialog(const DobSelectDialogParam *param)
{
	if (s_dobDialog != NULL) {
		if (param->callback != NULL)
			delete param->callback;
		return SIGNIN_EXT_ERROR_BUSY;
	}

	SigninExtImeEndEdit();
	s_dobDialogCallback = param->callback;
	s_dobDialog = new DobSelectDialog();
	s_dobDialog->Open(param);
	return 0;
}

DobSelectButton::DobSelectButton()
	: m_button(NULL), m_titleId(0U), m_model(NULL), m_onSelected(NULL)
{
}

DobSelectButton::~DobSelectButton()
{
	if (m_onSelected != NULL)
		delete m_onSelected;
}

void DobSelectButton::Bind(Widget *button, const IDParam &titleId, BirthDateModel *model,
			   Callback1<BirthDateModel *> *onSelected)
{
	m_button = button;
	m_titleId = titleId;
	m_model = model;
	m_onSelected = onSelected;
	m_button->AddEventListener(
		ButtonBase::CB_BTN_DECIDE,
		new MemberEventListener<DobSelectButton>(
			this, &DobSelectButton::OnButtonPressed, NULL),
		false);
}

void DobSelectButton::OpenDialog(void)
{
	if (m_button == NULL || m_model == NULL)
		return;

	DobSelectDialogParam param;
	param.anchorWidget = m_button;
	param.title = SigninExtGetString(m_titleId);
	param.model = m_model;
	param.callback = new MemberCallback1<DobSelectButton, BirthDateModel *>(
		this, &DobSelectButton::OnDialogResult);
	ShowDobDialog(&param);
}

void DobSelectButton::RefreshText(void)
{
	if (m_button != NULL && m_model != NULL) {
		wstring text = m_model->FormatDisplayString();
		m_button->SetString(text);
	}
}

void DobSelectButton::OnButtonPressed(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	OpenDialog();
}

void DobSelectButton::OnDialogResult(BirthDateModel *result, void *userData)
{
	(void)userData;
	if (result != NULL && m_onSelected != NULL)
		m_onSelected->Invoke(result);
}

/* ---- InfoScrollDialog ---- */

class InfoScrollDialog : public DialogBase {
public:
	InfoScrollDialog() : m_edgeBinder(), m_contentBox(NULL) {}
	virtual ~InfoScrollDialog() {}

	void Open(const IDParam &titleId, const IDParam &templateId, Widget *parentScene);
	virtual void Close(bool animate);
	void OnContentReady(int32_t type, Handler *self, Event *e, void *userData);
	void OnClosePressed(int32_t type, Handler *self, Event *e, void *userData);

	ScrollListEdgeHelper m_edgeBinder;
	Widget *m_contentBox;
};

void InfoScrollDialog::Open(const IDParam &titleId, const IDParam &templateId, Widget *parentScene)
{
	m_parentScene = parentScene;
	m_pageId = IDParam(RCO_PAGE_INFO_DIALOG);

	Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = 1;
	m_scene = SigninExtPageOpenWithParam(m_pageId, openParam);
	if (m_scene == NULL)
		return;

	m_panel = m_scene->FindChild(IDParam(RCO_DIALOG_PANEL), 0);
	m_contentBox = m_scene->FindChild(IDParam(RCO_DIALOG_CONTENT), 0);
	ScrollViewBase *scrollView = static_cast<ScrollViewBase *>(
		m_scene->FindChild(IDParam(RCO_DIALOG_SCROLL), 0));
	Widget *scrollBar = m_scene->FindChild(IDParam(RCO_DIALOG_SCROLLBAR), 0);

	scrollBar->SetParent(m_panel, NULL, 0);
	scrollBar->SetAnchor(2, scrollBar->m_anchor_y, scrollBar->m_anchor_z, NULL);
	scrollBar->SetAlign(2, scrollBar->m_align_y, scrollBar->m_align_z, NULL);
	scrollBar->SetPos(math::v4(-12.0f, -252.0f, 0.0f, 0.0f), NULL,
			  Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);

	scrollView->SetScrollBarController(new SigninExtScrollBarController());
	m_edgeBinder.Attach(scrollView, m_panel, IDParam(RCO_STYLE_LIST_EDGE_STD));

	Widget *titleWidget = m_scene->FindChild(IDParam(RCO_DIALOG_TITLE), 0);
	SigninExtSetAutoShrinkText(titleWidget, SigninExtGetString(titleId));

	Widget *closeBtn = m_scene->FindChild(IDParam(RCO_BTN_CLOSE), 0);
	closeBtn->AddEventListener(
		ButtonBase::CB_BTN_DECIDE,
		new MemberEventListener<InfoScrollDialog>(
			this, &InfoScrollDialog::OnClosePressed, NULL),
		false);

	Plugin::TemplateOpenParam tmplParam;
	if (SigninExtTemplateOpen(m_contentBox, templateId, tmplParam) < 0)
		return;

	m_panel->SetMetaAlpha(0.0f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
	m_contentBox->AddEventListener(
		0x20100004,
		new MemberEventListener<InfoScrollDialog>(
			this, &InfoScrollDialog::OnContentReady, NULL),
		false);
	PrepareAndShow(false);
}

void InfoScrollDialog::Close(bool animate)
{
	if (animate && m_contentBox != NULL &&
	    m_contentBox->m_map_e_listener.find(0x20100004) !=
		    m_contentBox->m_map_e_listener.end()) {
		animate = false;
	}
	DialogBase::Close(animate);
}

void InfoScrollDialog::OnContentReady(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	m_contentBox->DeleteEventListener(0x20100004, NULL);

	ScrollViewBase *scrollView = static_cast<ScrollViewBase *>(
		m_scene->FindChild(IDParam(RCO_DIALOG_SCROLL), 0));
	float contentH = sce_paf_ceilf(m_contentBox->GetSize(0)->extract_y());
	math::v4 scrollSize = *scrollView->GetSize(0);
	float diff = scrollSize.extract_y() - contentH;

	if (diff > 0.0f) {
		scrollSize.set_y(contentH);
		scrollView->SetSize(scrollSize, NULL, Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		math::v4 panelSize = *m_panel->GetSize(0);
		panelSize.set_y(panelSize.extract_y() - diff);
		m_panel->SetSize(panelSize, NULL, Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
	}

	math::v2 focusPos(0.0f, scrollSize.extract_y() * -0.5f);
	scrollView->SetViewFocus(focusPos, NULL, true, true);
	SigninExtPlayCommonSE(4);
	PlayOpenAnim();
}

static InfoScrollDialog *s_infoScrollDialog = NULL;
static Callback0 *s_infoScrollCallback = NULL;

void InfoScrollDialog::OnClosePressed(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	SigninExtPlayCommonSE(1);
	if (s_infoScrollDialog != NULL) {
		s_infoScrollDialog->Close(true);
		delete s_infoScrollDialog;
		s_infoScrollDialog = NULL;
		SigninExtImeResumeEdit();
		if (s_infoScrollCallback != NULL) {
			Callback0 *cb = s_infoScrollCallback;
			s_infoScrollCallback = NULL;
			cb->Invoke();
			delete cb;
		}
	}
}

int32_t SigninExtShowInfoScrollDialog(const IDParam &titleId, const IDParam &templateId,
				      Widget *parentScene, Callback0 *onClosed)
{
	if (s_infoScrollDialog != NULL) {
		if (onClosed != NULL)
			delete onClosed;
		return SIGNIN_EXT_ERROR_ALREADY_CREATED;
	}

	SigninExtImeSuspendEdit();
	s_infoScrollDialog = new InfoScrollDialog();
	if (s_infoScrollDialog == NULL) {
		if (onClosed != NULL)
			delete onClosed;
		return SIGNIN_EXT_ERROR_NOMEM;
	}

	s_infoScrollDialog->Open(titleId, templateId, parentScene);
	s_infoScrollCallback = onClosed;
	return 0;
}

/* ---- ListSelectDialog & ListSelectButton ---- */

struct ListSelectDialogParam {
	Widget *anchorWidget;
	const wchar_t *title;
	int32_t itemCount;
	int32_t selectedIndex;
	Callback2<Widget *, int32_t> *itemBinder;
	Callback1<int32_t> *onSelected;
};

class ListSelectItemFactory : public listview::ItemFactory {
public:
	ListSelectItemFactory(ListView *listView, Callback2<Widget *, int32_t> *binder,
			      int32_t count, int32_t selectedIndex)
		: m_listView(listView), m_binder(binder), m_count(count),
		  m_selectedIndex((selectedIndex >= 0 || selectedIndex < count) ? selectedIndex : -1)
	{
	}

	virtual ~ListSelectItemFactory()
	{
		if (m_binder != NULL)
			delete m_binder;
	}

	virtual ListItem *Create(CreateParam &param)
	{
		Plugin::TemplateOpenParam tmplParam;
		if (SigninExtTemplateOpen(param.m_parent, IDParam(RCO_TMPL_LIST_ITEM), tmplParam) < 0)
			return NULL;

		ListItem *item = static_cast<ListItem *>(
			param.m_parent->FindChild(IDParam(RCO_LIST_ITEM_ROOT), 0));
		Widget *btn = param.m_parent->FindChild(IDParam(RCO_LIST_ITEM_BTN), 0);
		item->m_name.m_id = 0;
		btn->m_name.m_id = 0;

		int32_t idx = param.m_cell;
		if (idx == 0) {
			item->SetOrigin(item->m_origin_x, 1, item->m_origin_z, NULL);
			btn->SetAlign(btn->m_align_x, 1, btn->m_align_z, NULL);
		} else if (idx == m_count - 1) {
			item->SetOrigin(item->m_origin_x, 2, item->m_origin_z, NULL);
			btn->SetAlign(btn->m_align_x, 2, btn->m_align_z, NULL);
		}

		math::v4 cellSize = param.m_list_view->GetCellSize(0, idx);
		item->SetSize(cellSize, NULL, Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);

		DrawObj *textObj = btn->GetDrawObj(1);
		uint32_t packed = textObj->m_color;
		math::v4 col((float)(packed & 0xFFU) / 255.0f,
			     (float)((packed >> 8) & 0xFFU) / 255.0f,
			     (float)((packed >> 16) & 0xFFU) / 255.0f, 0.0f);
		textObj->SetColor(col);

		btn->AddEventListener(
			0x100004,
			new MemberEventListener<ListSelectItemFactory>(
				this, &ListSelectItemFactory::OnTextPrepared, (void *)(intptr_t)idx),
			false);
		btn->AddEventListener(
			ButtonBase::CB_BTN_DECIDE,
			new MemberEventListener<ListSelectItemFactory>(
				this, &ListSelectItemFactory::OnItemDecided, (void *)(intptr_t)idx),
			false);

		if (m_binder != NULL)
			m_binder->Invoke(btn, idx);

		return item;
	}

	void OnTextPrepared(int32_t type, Handler *self, Event *e, void *userData)
	{
		(void)type;
		(void)e;
		int32_t idx = (int32_t)(intptr_t)userData;
		Widget *btn = static_cast<Widget *>(self);
		TextObj *textObj = static_cast<TextObj *>(btn->GetDrawObj(1));

		float fontSize = 0.0f;
		textObj->GetStyleAttribute(TextStyleAttribute_Point, 0, fontSize);
		uint32_t lineCount = 0;
		if (fontSize != 24.0f) {
			textObj->GetLineCount(lineCount);
			if (lineCount > 1) {
				textObj->SetStyleAttribute(TextStyleAttribute_Point, 0, 0, 24.0f);
				return;
			}
		}

		btn->DeleteEventListener(0x100004, NULL);
		uint32_t packed = textObj->m_color;
		math::v4 col((float)(packed & 0xFFU) / 255.0f,
			     (float)((packed >> 8) & 0xFFU) / 255.0f,
			     (float)((packed >> 16) & 0xFFU) / 255.0f, 1.0f);
		textObj->SetColor(col);

		if (idx == m_selectedIndex) {
			Plugin::TemplateOpenParam tmplParam;
			SigninExtTemplateOpen(btn, IDParam(RCO_TMPL_CHECKMARK), tmplParam);
		}
	}

	void OnItemDecided(int32_t type, Handler *self, Event *e, void *userData);

	ListView *m_listView;
	Callback2<Widget *, int32_t> *m_binder;
	int32_t m_count;
	int32_t m_selectedIndex;
};

class ListSelectDialog : public DialogBase {
public:
	ListSelectDialog() : m_initialIndex(-1), m_edgeBinder() {}
	virtual ~ListSelectDialog() {}

	void Open(const ListSelectDialogParam *param);
	void OnClosePressed(int32_t type, Handler *self, Event *e, void *userData);

	int32_t m_initialIndex;
	ScrollListEdgeHelper m_edgeBinder;
};

void ListSelectDialog::Open(const ListSelectDialogParam *param)
{
	m_parentScene = param->anchorWidget->m_scene;
	m_pageId = IDParam(RCO_PAGE_LIST_DIALOG);
	m_initialIndex = param->selectedIndex;

	Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = 1;
	m_scene = SigninExtPageOpenWithParam(m_pageId, openParam);
	m_panel = m_scene->FindChild(IDParam(RCO_DIALOG_PANEL), 0);

	Widget *closeBtn = m_scene->FindChild(IDParam(RCO_BTN_CLOSE), 0);
	ListView *listView = static_cast<ListView *>(
		m_scene->FindChild(IDParam(RCO_LIST_VIEW), 0));
	Widget *scrollBar = m_scene->FindChild(IDParam(RCO_DIALOG_SCROLLBAR), 0);
	Widget *titleText = m_scene->FindChild(IDParam(RCO_DIALOG_TITLE), 0);
	SigninExtSetAutoShrinkText(titleText, param->title);

	closeBtn->AddEventListener(
		ButtonBase::CB_BTN_DECIDE,
		new MemberEventListener<ListSelectDialog>(
			this, &ListSelectDialog::OnClosePressed, NULL),
		false);

	math::v4 listSize = *listView->GetSize(0);
	scrollBar->SetParent(m_panel, NULL, 0);
	scrollBar->SetAnchor(2, scrollBar->m_anchor_y, scrollBar->m_anchor_z, NULL);
	scrollBar->SetAlign(2, scrollBar->m_align_y, scrollBar->m_align_z, NULL);
	scrollBar->SetPos(math::v4(-12.0f, -268.0f, 0.0f, 0.0f), NULL,
			  Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);

	listView->SetScrollBarController(new SigninExtScrollBarController());

	float topMargin = 0.0f;
	float bottomMargin = 0.0f;
	float totalH = (float)param->itemCount * 70.0f;
	if (totalH >= listSize.extract_y()) {
		m_edgeBinder.Attach(listView, m_panel, IDParam(RCO_STYLE_LIST_EDGE_STD));
		topMargin = m_edgeBinder.m_listEdge->m_topMargin;
		bottomMargin = m_edgeBinder.m_listEdge->m_bottomMargin;
	} else {
		math::v4 panelSize = *m_panel->GetSize(0);
		panelSize.set_y(totalH + (panelSize.extract_y() - listSize.extract_y()));
		listSize.set_y(totalH);
		m_panel->SetSize(panelSize, NULL, Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		listView->SetSize(listSize, NULL, Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
	}

	listView->SetItemFactory(new ListSelectItemFactory(
		listView, param->itemBinder, param->itemCount, param->selectedIndex));

	math::v4 cellSize(listSize.extract_x(), 70.0f, 0.0f, 0.0f);
	listView->BlockSync();
	listView->InsertSegment(0, 1);
	listView->SetSegmentLayoutType(0, ListView::LAYOUT_TYPE_LIST);
	listView->SetCellSizeDefault(0, cellSize);
	listView->InsertCell(0, 0, (uint32_t)param->itemCount);

	if (topMargin > 0.0f) {
		math::v4 firstSize(cellSize.extract_x(), 80.0f, 0.0f, 0.0f);
		listView->SetCellSize(0, 0, firstSize);
	}
	if (bottomMargin > 0.0f) {
		math::v4 lastSize(cellSize.extract_x(), 80.0f, 0.0f, 0.0f);
		listView->SetCellSize(0, param->itemCount - 1, lastSize);
	}
	listView->UnblockSync();

	int32_t focusIdx = m_initialIndex;
	if (focusIdx < 0)
		focusIdx = 0;
	if (focusIdx > param->itemCount - 1)
		focusIdx = param->itemCount - 1;
	listView->SetFocus(0, focusIdx, (ListView::FocusAlignType)1, NULL);

	SigninExtPlayCommonSE(4);
	PrepareAndShow(true);
}

static ListSelectDialog *s_listDialog = NULL;
static Callback1<int32_t> *s_listDialogCallback = NULL;

void SigninExtListDialogInit(void)
{
}

void SigninExtListDialogCloseImmediate(void)
{
	if (s_listDialog != NULL) {
		s_listDialog->Close(false);
		delete s_listDialog;
		s_listDialog = NULL;
		if (s_listDialogCallback != NULL) {
			delete s_listDialogCallback;
			s_listDialogCallback = NULL;
		}
	}
}

void SigninExtListDialogTerm(void)
{
	if (s_listDialog != NULL) {
		s_listDialog->Close(false);
		delete s_listDialog;
		s_listDialog = NULL;
		if (s_listDialogCallback != NULL) {
			delete s_listDialogCallback;
			s_listDialogCallback = NULL;
		}
	}
}

static void CompleteListDialog(int32_t index)
{
	if (s_listDialog == NULL)
		return;

	if (index >= 0) {
		SigninExtPlayCommonSE(0);
		if (s_listDialog->m_initialIndex == index)
			index = -1;
	} else {
		SigninExtPlayCommonSE(1);
	}

	s_listDialog->Close(true);
	delete s_listDialog;
	s_listDialog = NULL;

	if (s_listDialogCallback != NULL) {
		Callback1<int32_t> *cb = s_listDialogCallback;
		s_listDialogCallback = NULL;
		cb->Invoke(index);
		delete cb;
	}
}

void ListSelectDialog::OnClosePressed(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	CompleteListDialog(-1);
}

void ListSelectItemFactory::OnItemDecided(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	CompleteListDialog((int32_t)(intptr_t)userData);
}

static int32_t ShowListDialog(const ListSelectDialogParam *param)
{
	if (s_listDialog != NULL) {
		if (param->itemBinder != NULL)
			delete param->itemBinder;
		if (param->onSelected != NULL)
			delete param->onSelected;
		return SIGNIN_EXT_ERROR_BUSY;
	}

	SigninExtImeEndEdit();
	s_listDialogCallback = param->onSelected;
	s_listDialog = new ListSelectDialog();
	s_listDialog->Open(param);
	return 0;
}

ListSelectButton::ListSelectButton()
	: m_button(NULL), m_titleId(0U), m_model(NULL), m_onSelected(NULL)
{
}

ListSelectButton::~ListSelectButton()
{
	if (m_onSelected != NULL)
		delete m_onSelected;
}

void ListSelectButton::Bind(Widget *button, const IDParam &titleId, SelectListModel *model,
			    Callback1<int32_t> *onSelected)
{
	m_button = button;
	m_titleId = titleId;
	m_model = model;
	m_onSelected = onSelected;
	m_button->AddEventListener(
		ButtonBase::CB_BTN_DECIDE,
		new MemberEventListener<ListSelectButton>(
			this, &ListSelectButton::OnButtonPressed, NULL),
		false);
}

void ListSelectButton::OpenDialog(void)
{
	if (m_button == NULL || m_model == NULL || m_model->GetCount() <= 1)
		return;

	ListSelectDialogParam param;
	param.anchorWidget = m_button;
	param.title = SigninExtGetString(m_titleId);
	param.itemCount = m_model->GetCount();
	param.selectedIndex = m_model->m_selectedIndex;
	param.itemBinder = new MemberCallback2<ListSelectButton, Widget *, int32_t>(
		this, &ListSelectButton::OnBindItem);
	param.onSelected = new MemberCallback1<ListSelectButton, int32_t>(
		this, &ListSelectButton::OnDialogResult);
	ShowListDialog(&param);
}

void ListSelectButton::RefreshText(void)
{
	if (m_button == NULL || m_model == NULL)
		return;

	DrawObj *bgObj = m_button->GetDrawObj(0);
	DrawObj *textObj = m_button->GetDrawObj(1);
	if (bgObj != NULL && textObj != NULL) {
		if (m_model->GetCount() > 1) {
			bgObj->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
			m_button->EnableEvent(0xFFF0);
			static_cast<TextObj *>(textObj)->SetStyleAttribute(
				TextStyleAttribute_Shadow, 0, 0, true);
		} else {
			bgObj->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
			m_button->DisableEvent(0xAFF0);
			static_cast<TextObj *>(textObj)->SetStyleAttribute(
				TextStyleAttribute_Shadow, 0, 0, false);
		}
	}

	const wchar_t *label = m_model->GetLabel(m_model->m_selectedIndex);
	m_button->SetString(wstring(label));
}

void ListSelectButton::OnButtonPressed(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	OpenDialog();
}

void ListSelectButton::OnBindItem(Widget *itemWidget, int32_t index, void *userData)
{
	(void)userData;
	const wchar_t *label = m_model->GetLabel(index);
	itemWidget->SetString(wstring(label));
}

void ListSelectButton::OnDialogResult(int32_t index, void *userData)
{
	(void)userData;
	if (index >= 0 && m_onSelected != NULL)
		m_onSelected->Invoke(index);
}

/* ---- HttpImageFetchJob & HttpImageClient ---- */

static bool ImageJobIsCanceled(void *userData)
{
	return static_cast<HttpImageFetchJob *>(userData)->IsCanceled();
}

static intrusive_ptr<Surface> ImageJobCreateSurface(
	SurfacePool *pool, const SharedPtr<Image> &image,
	int32_t numPage, int32_t numLevel, void *userData)
{
	(void)numPage;
	HttpImageFetchJob *job = static_cast<HttpImageFetchJob *>(userData);
	if (job->m_surfaceCreator != NULL)
		return job->m_surfaceCreator->Invoke(pool, image, numLevel);
	return intrusive_ptr<Surface>();
}

HttpImageFetchJob::HttpImageFetchJob(HttpClient *client, const char *url,
				     Callback2<HttpImageFetchJob *, int32_t> *callback,
				     const HttpImageRequestOption *opt)
	: HttpRequestJob(client, url, 1, NULL),
	  m_imageCallback(callback),
	  m_width(opt->width),
	  m_height(opt->height),
	  m_surfacePool(opt->surfacePool),
	  m_surfaceCreator(opt->surfaceCreator),
	  m_surface()
{
	if (opt->preloadedBuffer.get() != NULL)
		m_buffer = opt->preloadedBuffer;
	if (m_surfacePool == NULL)
		m_surfacePool = SigninExtGetSurfacePool();
}

HttpImageFetchJob::~HttpImageFetchJob()
{
	if (m_imageCallback != NULL) {
		delete m_imageCallback;
		m_imageCallback = NULL;
	}
	if (m_surfaceCreator != NULL) {
		delete m_surfaceCreator;
		m_surfaceCreator = NULL;
	}
}

int32_t HttpImageFetchJob::DecodeImage(void)
{
	Surface::LoadOption opt;
	opt.scale = image::ImageExtent(m_width, m_height);
	opt.cancel = ImageJobIsCanceled;
	opt.cancel_userdata = this;
	if (m_surfaceCreator != NULL) {
		opt.alloc_surface = ImageJobCreateSurface;
		opt.alloc_surface_userdata = this;
	}

	m_surface = Surface::Load(m_surfacePool, m_buffer->GetAddr(),
				  (int64_t)m_buffer->GetSize(), &opt);
	if (m_surface.get() != NULL)
		return 0;
	return opt.error;
}

int32_t HttpImageFetchJob::Run(void)
{
	if (IsCanceled())
		return -1;
	if (m_buffer.get() != NULL)
		return DecodeImage();

	int32_t ret = ExecuteHttp();
	if (ret >= 0)
		return DecodeImage();
	return ret;
}

void HttpImageFetchJob::OnFinished(int32_t result)
{
	if (m_imageCallback != NULL) {
		Callback2<HttpImageFetchJob *, int32_t> *cb = m_imageCallback;
		m_imageCallback = NULL;
		cb->Invoke(this, result);
		delete cb;
	}
}

HttpImageClient::HttpImageClient(JobQueue *queue)
	: HttpClient(queue)
{
}

HttpImageClient::~HttpImageClient()
{
}

void HttpImageClient::EnqueueImage(const char *url,
				   Callback2<HttpImageFetchJob *, int32_t> *callback,
				   const HttpImageRequestOption *opt)
{
	HttpImageRequestOption defaultOpt;
	if (opt == NULL)
		opt = &defaultOpt;

	SharedPtr<JobItem> item(new HttpImageFetchJob(this, url, callback, opt));
	m_activeJobs.push_back(item);
	m_jobQueue->Enqueue(item);
}

/* ---- Message formatting & 79-entry locale number format table ---- */

static const wchar_t *(*s_customFormatHandler)(wchar_t ch) = NULL;

wstring SigninExtFormatMessage(const wchar_t *fmt,
			       const wchar_t *a1, const wchar_t *a2, const wchar_t *a3,
			       const wchar_t *a4, const wchar_t *a5, const wchar_t *a6,
			       const wchar_t *a7, const wchar_t *a8, const wchar_t *a9,
			       const wchar_t *a10, const wchar_t *a11)
{
	const wchar_t *args[11] = { a1, a2, a3, a4, a5, a6, a7, a8, a9, a10, a11 };
	wstring out;
	const wchar_t *p = fmt;
	int32_t replaced = 0;

	for (;;) {
		const wchar_t *pct = sce_paf_wcschr(p, L'%');
		if (pct == NULL)
			break;

		out.append(p, (size_t)(pct - p));
		wchar_t ch = pct[1];
		if (ch == L'%') {
			out.append(1, L'%');
			p = pct + 2;
		} else if (ch >= L'1' && ch <= L'9') {
			const wchar_t *val = args[ch - L'1'];
			out.append(val, sce_paf_wcslen(val));
			p = pct + 2;
		} else if ((ch >= L'A' && ch <= L'Z') || (ch >= L'a' && ch <= L'z')) {
			if (s_customFormatHandler != NULL) {
				const wchar_t *val = s_customFormatHandler(ch);
				out.append(val, sce_paf_wcslen(val));
			}
			p = pct + 2;
		} else if (ch == L'(' && pct[2] == L'1' &&
			   (pct[3] == L'0' || pct[3] == L'1') && pct[4] == L')') {
			const wchar_t *val = args[9 + (pct[3] - L'0')];
			out.append(val, sce_paf_wcslen(val));
			p = pct + 5;
		} else if (ch == L'\0') {
			return out;
		} else {
			p = pct + 2;
		}
		++replaced;
	}

	if (replaced != 0)
		out.append(p, sce_paf_wcslen(p));
	else if (fmt != NULL)
		out.assign(fmt, sce_paf_wcslen(fmt));

	return out;
}

struct LocaleNumberFormat {
	string country;
	int32_t languageId;
	string decimalSep;
	string thousandSep;
};

static const LocaleNumberFormat s_localeNumberFormats[79] = {
	{ "JP",  0, ".", "," }, { "US",  1, ".", "," }, { "CA",  1, ".", "," },
	{ "CA",  2, ",", " " }, { "MX",  3, ".", "," }, { "MX",  1, ".", "," },
	{ "BR", 17, ",", "." }, { "BR",  1, ".", "," }, { "CL",  3, ",", " " },
	{ "CL",  1, ".", "," }, { "PE",  3, ",", " " }, { "PE",  1, ".", "," },
	{ "AR",  3, ",", " " }, { "AR",  1, ".", "," }, { "CO",  3, ",", " " },
	{ "CO",  1, ".", "," }, { "GB", 18, ".", "," }, { "IE", 18, ".", "," },
	{ "BE",  6, ",", " " }, { "BE",  2, ",", " " }, { "LU",  4, ",", "." },
	{ "LU",  2, ",", " " }, { "NL",  6, ",", "." }, { "FR",  2, ",", " " },
	{ "DE",  4, ",", "." }, { "AT",  4, ",", "." }, { "CH",  4, ",", " " },
	{ "CH",  2, ",", " " }, { "CH",  5, ",", " " }, { "IT",  5, ",", "." },
	{ "PT",  7, ",", "." }, { "DK", 14, ",", " " }, { "DK", 18, ".", "," },
	{ "FI", 12, ",", " " }, { "FI", 18, ".", "," }, { "NO", 15, ",", " " },
	{ "NO", 18, ".", "," }, { "SE", 13, ",", " " }, { "SE", 18, ".", "," },
	{ "AU", 18, ".", "," }, { "NZ", 18, ".", "," }, { "ES",  3, ",", "." },
	{ "RU",  8, ",", " " }, { "AE", 18, ".", "," }, { "ZA", 18, ".", "," },
	{ "PL", 16, ",", " " }, { "PL", 18, ".", "," }, { "GR", 18, ".", "," },
	{ "SA", 18, ".", "," }, { "CZ", 18, ".", "," }, { "TR", 18, ".", "," },
	{ "TR", 19, ",", "." }, { "IN", 18, ".", "," }, { "HR", 18, ".", "," },
	{ "SI", 18, ".", "," }, { "KW", 18, ".", "," }, { "IL", 18, ".", "," },
	{ "UA",  8, ",", " " }, { "BG", 18, ".", "," }, { "RO", 18, ".", "," },
	{ "HU", 18, ".", "," }, { "QA", 18, ".", "," }, { "SK", 18, ".", "," },
	{ "BH", 18, ".", "," }, { "LB", 18, ".", "," }, { "OM", 18, ".", "," },
	{ "MT", 18, ".", "," }, { "IS", 18, ".", "," }, { "CY", 18, ".", "," },
	{ "HK", 10, ".", "," }, { "HK", 18, ".", "," }, { "HK", 11, ".", "," },
	{ "TW", 10, ".", "," }, { "TW", 18, ".", "," }, { "SG", 18, ".", "," },
	{ "MY", 18, ".", "," }, { "ID", 18, ".", "," }, { "TH", 18, ".", "," },
	{ "KR",  9, ".", "," }
};
