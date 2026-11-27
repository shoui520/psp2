/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * Flow scenes (ResetPasswordDone, Eula, CompleteNotice, InitSession,
 * SessionExpired, TermSession, ChangePassword, VerifyDob), PageBase with
 * PageTurn animations, PopupDialog/ErrorDialog managers, form rows
 * (TextFieldRow, DobFieldRow, SelectListFieldRow, PasswordInputGroup),
 * RichTextController, IME/TextBox helpers, HeaderBar/FooterBar,
 * environment helpers, and CountryTable/CountryModel.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/math.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <framework.h>
#include <graphics/drawobj/draw_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/surface/surface.h>
#include <inputdevice/inputdevice.h>
#include <math/ngp/quaternion.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <widget/core/animlistener.h>
#include <widget/core/event.h>
#include <widget/core/handler.h>
#include <widget/core/widget.h>
#include <widget/w_box.h>
#include <widget/w_busyindicator.h>
#include <widget/w_button_base.h>
#include <widget/w_pageturn.h>
#include <widget/w_rich_text.h>
#include <widget/w_scene.h>
#include <widget/w_scroll_view_base.h>
#include <widget/w_scroll_bar.h>
#include <widget/w_text.h>
#include <widget/w_text_box.h>
#include <widget/wutil.h>
#include "signin_ext_internal.h"

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::ui;

/* ---- Static state ---- */

static vector<CountryEntry> s_countryList;
static int32_t s_savedSystemLocale = 0;
int32_t g_pageTransitionMode = 0;
static TextBox *s_suspendedEditTextBox = NULL;

void SigninExtSetPageTransitionMode(int32_t mode)
{
	g_pageTransitionMode = mode;
}

/* ---- PageRotateAnimListener & SceneBase / PageBase ---- */

static void ApplyPivotRotation(Widget *widget, const math::v4 &origPos,
			       const math::quaternion &origQuat,
			       const math::v4 &pivot, const math::v4 &axis,
			       float angle)
{
	if (widget == NULL)
		return;
	float half = angle * 0.5f;
	float s = sce_paf_sinf(half);
	float c = sce_paf_cosf(half);
	math::quaternion q(axis.extract_x() * s, axis.extract_y() * s,
			   axis.extract_z() * s, c);
	math::v4 offset = origPos - pivot;
	math::quaternion p(offset.extract_x(), offset.extract_y(), offset.extract_z(), 0.0f);
	math::quaternion qConj(sce::Vectormath::Simd::Aos::conj(q.getImpl()));
	math::quaternion rotated = (q * p) * qConj;
	math::v4 newPos = math::v4(sce::Vectormath::Simd::Aos::Vector4(rotated.getImpl())) + pivot;
	widget->SetPos_core(newPos, 0xFF01, false);
	math::quaternion newQuat = origQuat * q;
	widget->SetQuat_core(newQuat, 0xFF08);
}

class PageRotateAnimListener : public AnimListener {
public:
	PageRotateAnimListener(Timer *timer, Widget *target,
			       const math::v4 &pivot, const math::v4 &axis, float angle);
	virtual ~PageRotateAnimListener() {}

	virtual void OnUpdate(void)
	{
		if (m_widget != NULL) {
			float angle = m_angle * m_timer->m_mapped_x;
			ApplyPivotRotation(m_widget, m_origPos, m_origQuat,
					   m_pivot, m_axis, angle);
		}
	}

	virtual void OnFinish(void *data1, void *data2)
	{
		(void)data1;
		(void)data2;
		if (m_widget != NULL) {
			ApplyPivotRotation(m_widget, m_origPos, m_origQuat,
					   m_pivot, m_axis, m_angle);
		}
	}

	math::v4 m_axis;
	float m_angle;
	math::v4 m_pivot;
	math::v4 m_origPos;
	math::quaternion m_origQuat;
};

PageRotateAnimListener::PageRotateAnimListener(Timer *timer, Widget *target,
					       const math::v4 &pivot,
					       const math::v4 &axis, float angle)
	: AnimListener(timer, target, NULL, NULL, NULL),
	  m_axis(axis), m_angle(angle), m_pivot(pivot),
	  m_origPos(0.0f, 0.0f, 0.0f, 0.0f),
	  m_origQuat(0.0f, 0.0f, 0.0f, 1.0f)
{
	if (m_widget != NULL) {
		m_origPos = m_widget->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		m_origQuat = m_widget->GetQuat(0, NULL, math::quaternion(0.0f, 0.0f, 0.0f, 1.0f));
	}
}

SceneBase::SceneBase() : m_finishCallback(NULL), m_result(0x7FFFFFFF)
{
}

SceneBase::~SceneBase()
{
	if (m_finishCallback != NULL) {
		delete m_finishCallback;
		m_finishCallback = NULL;
	}
}

void SceneBase::Start(Callback1<int32_t> *callback)
{
	m_finishCallback = callback;
}

void SceneBase::Abort(void)
{
	m_result = -2;
	Finish();
}

void SceneBase::Finish(void)
{
	if (m_finishCallback != NULL) {
		Callback1<int32_t> *cb = m_finishCallback;
		m_finishCallback = NULL;
		cb->Invoke(m_result);
		delete cb;
	}
}

void SceneBase::OnSessionExpired(void)
{
	m_result = -3;
	Finish();
}

PageBase::PageBase(const IDParam &pageId)
	: SceneBase(), m_pageId(pageId), m_scene(NULL)
{
}

PageBase::~PageBase()
{
	if (m_scene != NULL) {
		SigninExtPageClose(m_pageId);
		m_scene = NULL;
	}
}

void PageBase::Start(Callback1<int32_t> *callback)
{
	SceneBase::Start(callback);
	m_scene = SigninExtPageOpen(m_pageId);
	if (m_scene == NULL) {
		m_result = -3;
		Finish();
		return;
	}

	OnCreate();
	PlayOpenAnim();
	OnOpenAnimDone();
}

void PageBase::PlayOpenAnim(void)
{
	if (g_pageTransitionMode == 0 || m_scene == NULL)
		return;

	switch (g_pageTransitionMode) {
	case 1: {
		m_scene->SetMetaAlpha(0.0f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
		Timer *t = new Timer(300.0f, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
		m_scene->SetMetaAlpha(1.0f, t, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
		break;
	}
	case 2: {
		math::v4 origPos = m_scene->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		math::quaternion origQuat = m_scene->GetQuat(0, NULL, math::quaternion(0.0f, 0.0f, 0.0f, 1.0f));
		math::v4 pivot(0.0f, 0.0f, 500.0f, 0.0f);
		math::v4 axis(0.0f, 1.0f, 0.0f, 0.0f);
		ApplyPivotRotation(m_scene, origPos, origQuat, pivot, axis, 1.5707963f);
		Timer *t = new Timer(300.0f, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
		PageRotateAnimListener *anim = new PageRotateAnimListener(
			t, m_scene, pivot, axis, -1.5707963f);
		m_scene->SetAnimListener(0xFF0000, anim, 2);
		break;
	}
	case 3: {
		math::v4 origPos = m_scene->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
		math::quaternion origQuat = m_scene->GetQuat(0, NULL, math::quaternion(0.0f, 0.0f, 0.0f, 1.0f));
		math::v4 pivot(-480.0f, 0.0f, 0.0f, 0.0f);
		math::v4 axis(0.0f, 1.0f, 0.0f, 0.0f);
		ApplyPivotRotation(m_scene, origPos, origQuat, pivot, axis, -1.5707963f);
		Timer *t = new Timer(300.0f, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
		PageRotateAnimListener *anim = new PageRotateAnimListener(
			t, m_scene, pivot, axis, 1.5707963f);
		m_scene->SetAnimListener(0xFF0000, anim, 2);
		break;
	}
	default:
		break;
	}
}

void PageBase::PlayCloseAnim(void)
{
	if (g_pageTransitionMode == 0) {
		SigninExtPageClose(m_pageId);
		m_scene = NULL;
		inputdevice::Unlock(0x7FU);
		Finish();
		return;
	}

	if (g_pageTransitionMode == 1) {
		Timer *t = new Timer(300.0f, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
		m_scene->SetMetaAlpha(0.0f, t, Widget::ANIM_META_ALPHA, 2, 0.0f, NULL, NULL);
	} else {
		Timer *tf = new Timer(300.0f, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
		m_scene->SetMetaAlpha(0.0f, tf, Widget::ANIM_META_ALPHA, 2, 0.0f, NULL, NULL);
		if (g_pageTransitionMode == 2) {
			Timer *t = new Timer(300.0f, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
			math::v4 pivot(-480.0f, 0.0f, 0.0f, 0.0f);
			math::v4 axis(0.0f, 1.0f, 0.0f, 0.0f);
			PageRotateAnimListener *anim = new PageRotateAnimListener(
				t, m_scene, pivot, axis, -1.5707963f);
			m_scene->SetAnimListener(0xFF0000, anim, 2);
			SigninExtPlayCommonSE(0);
		} else if (g_pageTransitionMode == 3) {
			Timer *t = new Timer(300.0f, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
			math::v4 pivot(0.0f, 0.0f, 500.0f, 0.0f);
			math::v4 axis(0.0f, 1.0f, 0.0f, 0.0f);
			PageRotateAnimListener *anim = new PageRotateAnimListener(
				t, m_scene, pivot, axis, 1.5707963f);
			m_scene->SetAnimListener(0xFF0000, anim, 2);
			SigninExtPlayCommonSE(2);
		}
	}

	m_scene->SetTimerListener(
		Handler::CB_TIMEOUT, 300.0f,
		new MemberEventListener<PageBase>(this, &PageBase::OnCloseTimer, NULL));
}

void PageBase::OnCloseTimer(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	SigninExtPageClose(m_pageId);
	m_scene = NULL;
	inputdevice::Unlock(0x7FU);
	OnCloseAnimDone();
}

void PageBase::Abort(void)
{
	SigninExtCloseAllDialogs();
	m_result = -2;
	Finish();
}

void PageBase::Finish(void)
{
	SigninExtCloseAllDialogs();
	SceneBase::Finish();
}

void PageBase::OnOpenAnimDone(void)
{
}

void PageBase::OnCloseAnimStart(void)
{
}

void PageBase::OnCloseAnimDone(void)
{
	Finish();
}

void PageBase::OnButtonPressed(int32_t buttonId)
{
	if (buttonId == 0) {
		SigninExtPlayCommonSE(1);
		CloseWithResult(-1);
	} else if (buttonId == 1) {
		SigninExtPlayCommonSE(0);
		CloseWithResult(0);
	}
}

void PageBase::CloseWithResult(int32_t result)
{
	m_result = result;
	OnCloseAnimStart();
	if (m_scene != NULL) {
		inputdevice::Lock(0x7FU);
		PlayCloseAnim();
	} else {
		OnCloseAnimDone();
	}
}

/* ---- ResetPasswordDoneScene ---- */

ResetPasswordDoneScene::ResetPasswordDoneScene()
	: SceneBase(), m_email(NULL)
{
}

ResetPasswordDoneScene::~ResetPasswordDoneScene()
{
}

void ResetPasswordDoneScene::ShowConfirmPopup(void)
{
	wstring email(m_email != NULL ? m_email : L"");
	SigninExtPopupShow(4, &email, NULL,
			   new MemberCallback1<ResetPasswordDoneScene, int32_t>(
				   this, &ResetPasswordDoneScene::OnConfirmResult));
}

void ResetPasswordDoneScene::Start(Callback1<int32_t> *callback)
{
	SceneBase::Start(callback);
	m_email = g_credentialsModel->m_id.m_buf;
	ShowConfirmPopup();
}

void ResetPasswordDoneScene::OnConfirmResult(int32_t result, void *userData)
{
	(void)userData;
	if (result > -2) {
		if (result < 0) {
			m_result = 0;
			Finish();
		} else if (result == 0) {
			wstring empty;
			SigninExtPopupShow(2, &empty, NULL,
					   new MemberCallback1<ResetPasswordDoneScene, int32_t>(
						   this, &ResetPasswordDoneScene::OnBusyCancel));
			SigninExtTaskStart(new Commerce2ResetPasswordTask(
				new SceneTaskCallback<ResetPasswordDoneScene>(
					this, &ResetPasswordDoneScene::OnResetMailDone)));
		}
	} else if (result == -3) {
		wstring email(m_email != NULL ? m_email : L"");
		SigninExtPopupShow(5, &email, NULL,
				   new MemberCallback1<ResetPasswordDoneScene, int32_t>(
					   this, &ResetPasswordDoneScene::OnHelpClosed));
	}
}

void ResetPasswordDoneScene::OnHelpClosed(int32_t result, void *userData)
{
	(void)result;
	(void)userData;
	ShowConfirmPopup();
}

void ResetPasswordDoneScene::OnSentOk(int32_t result, void *userData)
{
	(void)result;
	(void)userData;
	m_result = 0;
	Finish();
}

void ResetPasswordDoneScene::OnBusyCancel(int32_t result, void *userData)
{
	(void)userData;
	if (result == -1)
		SigninExtTaskCancel();
}

void ResetPasswordDoneScene::OnResetMailDone(int32_t result, void *userData)
{
	(void)userData;
	SigninExtPopupClose(-2);
	if (result >= 0) {
		const wchar_t *fmt = SigninExtGetString(IDParam(0x353BE71DU));
		wstring msg = SigninExtFormatMessage(fmt, m_email);
		SigninExtPopupShow(6, &msg, NULL,
				   new MemberCallback1<ResetPasswordDoneScene, int32_t>(
					   this, &ResetPasswordDoneScene::OnSentOk));
	} else if (result == -1) {
		ShowConfirmPopup();
	} else {
		ErrorDialogParam err(IDParam(0U), result);
		SigninExtErrorShow(&err, NULL,
				   new MemberCallback0<ResetPasswordDoneScene, int32_t>(
					   this, &ResetPasswordDoneScene::OnErrorDismissed, result));
	}
}

void ResetPasswordDoneScene::OnErrorDismissed(int32_t errCode)
{
	if (errCode != -2141908877 && errCode != -2130706424) {
		ShowConfirmPopup();
		return;
	}
	m_result = -3;
	Finish();
}

/* ---- EulaScene ---- */

EulaScene::EulaScene()
	: PageBase(IDParam(RCO_PAGE_EULA)),
	  m_langRow(),
	  m_richTextCtrl(),
	  m_edgeBinder(),
	  m_header(),
	  m_footer(),
	  m_busyIndicator(NULL),
	  m_urlBuilder(),
	  m_eulaAccepted(false),
	  m_loading(false)
{
}

EulaScene::~EulaScene()
{
	m_richTextCtrl.Clear();
}

void EulaScene::OnCreate(void)
{
	m_header.Init(m_scene, IDParam(0x521B117AU));
	m_footer.Init(m_scene, new VirtualMemberCallback1<EulaScene, int32_t>(
			      this, &EulaScene::OnButtonPressed));
	m_footer.SetButtonVisible(0, true);
	m_footer.SetButtonVisible(1, true);
	m_footer.SetNextLabel(IDParam(0xA444324CU));
	m_footer.SetNextEnabled(false, false);

	Widget *langLabel = m_scene->FindChild(IDParam(0x4560E44FU), 0);
	Widget *langBtn = m_scene->FindChild(IDParam(0xDC085239U), 0);
	m_langRow.Bind(langLabel, langBtn, IDParam(0xC23D543FU), IDParam(0xC23D543FU),
		       &g_accountInfoModel->m_language,
		       new MemberCallback1<EulaScene, int32_t>(
			       this, &EulaScene::OnLanguageChanged));
	m_langRow.SyncFromModel(0);

	RichText *richText = static_cast<RichText *>(m_scene->FindChild(IDParam(0xC8E58A1FU), 0));
	Widget *panel = m_scene->FindChild(IDParam(RCO_DIALOG_PANEL), 0);
	Widget *scrollBar = m_scene->FindChild(IDParam(RCO_DIALOG_SCROLLBAR), 0);
	if (scrollBar != NULL && panel != NULL) {
		scrollBar->SetParent(panel, NULL, 0);
		scrollBar->SetAnchor(2, scrollBar->m_anchor_y, scrollBar->m_anchor_z, NULL);
		scrollBar->SetAlign(2, scrollBar->m_align_y, scrollBar->m_align_z, NULL);
		scrollBar->SetPos(math::v4(-12.0f, -12.0f, 0.0f, 0.0f), NULL,
				  Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
	}
	if (richText != NULL) {
		richText->SetScrollBarController(new SigninExtScrollBarController());
		m_edgeBinder.Attach(richText, panel, IDParam(RCO_STYLE_LIST_EDGE_STD));
		m_richTextCtrl.Bind(richText, new MemberCallback1<EulaScene, int32_t>(
						      this, &EulaScene::OnRichTextStatus));
	}

	m_busyIndicator = m_scene->FindChild(IDParam(RCO_DIALOG_BUSY_INDICATOR), 0);
}

void EulaScene::OnOpenAnimDone(void)
{
	StartLoadDocument();
}

void EulaScene::OnCloseAnimStart(void)
{
	m_richTextCtrl.Clear();
}

void EulaScene::StartLoadDocument(void)
{
	m_loading = true;
	m_footer.SetNextEnabled(false, true);
	if (m_busyIndicator != NULL)
		static_cast<BusyIndicator *>(m_busyIndicator)->Start();

	const char *country = g_accountInfoModel->m_country.GetCountryCode(
		g_accountInfoModel->m_country.m_selectedIndex);
	const char *lang = g_accountInfoModel->m_language.GetLanguageCode(
		g_accountInfoModel->m_language.m_selectedIndex);

	m_urlBuilder.SetVersioned(g_accountInfoModel->m_eulaDocCount > 0 ? 1 : 0);
	m_urlBuilder.SetDocType(0);
	m_urlBuilder.SetCountry(country);
	m_urlBuilder.SetLanguage(lang);
	m_urlBuilder.SetPageIndex(g_accountInfoModel->m_eulaDocIndex);
	m_richTextCtrl.LoadUrl(m_urlBuilder.GetUrl());
}

void EulaScene::OnLanguageChanged(int32_t index, void *userData)
{
	(void)userData;
	g_accountInfoModel->m_language.SetSelectedIndex(index);
	int32_t langId = g_accountInfoModel->m_language.GetLanguageId(index);
	SigninExtSetLanguageId(langId);
	CloseWithResult(4);
}

void EulaScene::OnRichTextStatus(int32_t status, void *userData)
{
	(void)userData;
	m_loading = false;
	if (m_busyIndicator != NULL)
		static_cast<BusyIndicator *>(m_busyIndicator)->Stop();

	if (status == 0) {
		m_footer.SetNextEnabled(true, true);
	} else if (status == 1) {
		CloseWithResult(-1);
	} else {
		ErrorDialogParam err(IDParam(0U), status);
		SigninExtErrorShow(&err, m_scene,
				   new MemberCallback0<EulaScene>(
					   this, &EulaScene::OnLoadErrorDismissed));
	}
}

void EulaScene::OnButtonPressed(int32_t buttonId)
{
	if (buttonId == 0) {
		SigninExtPlayCommonSE(1);
		wstring empty;
		SigninExtPopupShow(3, &empty, m_scene,
				   new MemberCallback1<EulaScene, int32_t>(
					   this, &EulaScene::OnCancelConfirm));
	} else if (buttonId == 1) {
		SigninExtPlayCommonSE(0);
		if (g_accountInfoModel->m_eulaDocIndex < g_accountInfoModel->m_eulaDocCount) {
			g_accountInfoModel->AdvanceEulaDoc();
			StartLoadDocument();
		} else {
			wstring empty;
			SigninExtPopupShow(2, &empty, m_scene,
					   new MemberCallback1<EulaScene, int32_t>(
						   this, &EulaScene::OnAcceptPopupResult));
			SigninExtTaskStart(new Commerce2AcceptEulaTask(
				new SceneTaskCallback<EulaScene>(
					this, &EulaScene::OnAcceptDone)));
		}
	}
}

void EulaScene::OnCancelConfirm(int32_t result, void *userData)
{
	(void)userData;
	if (result == 0)
		m_richTextCtrl.CancelAndNotify();
}

void EulaScene::OnAcceptDone(int32_t result, void *userData)
{
	(void)userData;
	if (result >= 0) {
		m_eulaAccepted = true;
		SigninExtPopupClose(-2);
		CloseWithResult(0);
	} else {
		ErrorDialogParam err(IDParam(0U), result);
		Callback0 *cb = NULL;
		if (result == SIGNIN_EXT_ERROR_COMMERCE_SESSION_EXPIRED)
			cb = new MemberCallback0<EulaScene>(this, &EulaScene::OnErrorDismissed);
		SigninExtErrorShow(&err, m_scene, cb);
	}
}

void EulaScene::OnAcceptPopupResult(int32_t result, void *userData)
{
	(void)userData;
	if (result == -1)
		SigninExtTaskCancel();
}

void EulaScene::OnErrorDismissed(void *userData)
{
	(void)userData;
	CloseWithResult(-3);
}

void EulaScene::OnLoadErrorDismissed(void *userData)
{
	(void)userData;
	CloseWithResult(-3);
}

/* ---- CompleteNoticeScene ---- */

CompleteNoticeScene::CompleteNoticeScene(int32_t noticeType)
	: PageBase(IDParam(RCO_PAGE_COMPLETE_NOTICE)),
	  m_noticeType(noticeType),
	  m_header(),
	  m_footer()
{
}

CompleteNoticeScene::~CompleteNoticeScene()
{
}

void CompleteNoticeScene::OnCreate(void)
{
	uint32_t titleHash;
	uint32_t msgHash;
	if (m_noticeType == 0) {
		titleHash = 0xB994933CU;
		msgHash = 0x16B43106U;
	} else if (m_noticeType == 1) {
		titleHash = 0x46F7D8ABU;
		msgHash = 0x047ACDF7U;
	} else {
		titleHash = 0x46F7D8ABU;
		msgHash = 0x16B43106U;
	}

	m_header.Init(m_scene, IDParam(titleHash));
	m_footer.Init(m_scene, new VirtualMemberCallback1<CompleteNoticeScene, int32_t>(
			      this, &CompleteNoticeScene::OnButtonPressed));
	m_footer.SetButtonVisible(0, false);
	m_footer.SetButtonVisible(1, true);
	m_footer.SetNextLabel(IDParam(0x14E3A258U));

	Widget *msgText = m_scene->FindChild(IDParam(0x4EE68905U), 0);
	if (msgText != NULL) {
		const wchar_t *fmt = SigninExtGetString(IDParam(msgHash));
		wstring msg;
		if (m_noticeType == 1)
			msg = wstring(fmt);
		else
			msg = SigninExtFormatMessage(fmt, g_credentialsModel->m_id.m_buf);
		msgText->SetString(msg);
	}
}

void CompleteNoticeScene::OnButtonPressed(int32_t buttonId)
{
	if (buttonId == 1) {
		SigninExtPlayCommonSE(0);
		CloseWithResult(0);
	} else {
		PageBase::OnButtonPressed(buttonId);
	}
}

/* ---- InitSessionScene ---- */

InitSessionScene::InitSessionScene(int32_t mode)
	: SceneBase(), m_mode(mode), m_httpClient(SigninExtGetJobQueue()), m_aborted(false)
{
}

InitSessionScene::~InitSessionScene()
{
	m_httpClient.CancelAll();
}

void InitSessionScene::Start(Callback1<int32_t> *callback)
{
	SceneBase::Start(callback);
	wstring empty;
	SigninExtPopupShow(2, &empty, NULL,
			   new MemberCallback1<InitSessionScene, int32_t>(
				   this, &InitSessionScene::OnPopupResult));
	SigninExtTaskStart(new Commerce2InitTask(
		new SceneTaskCallback<InitSessionScene>(
			this, &InitSessionScene::OnCommerceInitDone)));
}

void InitSessionScene::Abort(void)
{
	m_aborted = true;
	SigninExtTaskCancel();
	m_httpClient.CancelAll();
	SigninExtCloseAllDialogs();
	m_result = -2;
	Finish();
}

void InitSessionScene::OnPopupResult(int32_t result, void *userData)
{
	(void)userData;
	if (result == -1) {
		SigninExtTaskCancel();
		m_httpClient.CancelAll();
	}
}

void InitSessionScene::OnCommerceInitDone(int32_t result, void *userData)
{
	(void)userData;
	if (result < 0) {
		HandleError(result);
		return;
	}

	switch (m_mode) {
	case 0:
		SigninExtTaskStart(new Commerce2GetCountryListTask(
			new SceneTaskCallback<InitSessionScene>(
				this, &InitSessionScene::OnCountryListDone)));
		break;
	case 1:
		SigninExtTaskStart(new RegDateFormatTask(
			new SceneTaskCallback<InitSessionScene>(
				this, &InitSessionScene::OnRegDateFormatDone)));
		break;
	case 2:
		SigninExtTaskStart(new Commerce2PasswordLoginTask(
			new SceneTaskCallback<InitSessionScene>(
				this, &InitSessionScene::OnPasswordLoginDone)));
		break;
	case 3:
		SigninExtTaskStart(new Commerce2ResetPasswordTask(
			new SceneTaskCallback<InitSessionScene>(
				this, &InitSessionScene::OnResetPasswordDone)));
		break;
	default:
		HandleError(-3);
		break;
	}
}

void InitSessionScene::OnCountryListDone(int32_t result, void *userData)
{
	(void)userData;
	if (result < 0) {
		HandleError(result);
		return;
	}
	SigninExtTaskStart(new Commerce2EulaLoginTask(
		new SceneTaskCallback<InitSessionScene>(
			this, &InitSessionScene::OnEulaLoginDone)));
}

void InitSessionScene::OnEulaLoginDone(int32_t result, void *userData)
{
	(void)userData;
	if (result < 0) {
		HandleError(result);
		return;
	}

	EulaUrlBuilder builder;
	const char *country = g_accountInfoModel->m_country.GetCountryCode(
		g_accountInfoModel->m_country.m_selectedIndex);
	const char *lang = g_accountInfoModel->m_language.GetLanguageCode(
		g_accountInfoModel->m_language.m_selectedIndex);
	builder.SetVersioned(1);
	builder.SetDocType(0);
	builder.SetCountry(country);
	builder.SetLanguage(lang);
	builder.SetPageIndex(0);

	m_httpClient.Enqueue(
		builder.GetUrl(), 0,
		new SceneHttpCallback<InitSessionScene>(
			this, &InitSessionScene::OnEulaDatFetched));
}

void InitSessionScene::OnEulaDatFetched(HttpRequestJob *job, int32_t result, void *userData)
{
	(void)userData;
	if (result == -1) {
		m_result = -1;
		Finish();
		return;
	}

	int32_t docCount = 0;
	if (result >= 0 && job->m_buffer.get() != NULL) {
		const char *text = static_cast<const char *>(job->m_buffer->GetAddr());
		if (text != NULL)
			docCount = (int32_t)sce_paf_strtol(text, NULL, 0);
	}
	g_accountInfoModel->SetEulaDocCount(docCount);
	SigninExtPopupClose(-2);
	m_result = 0;
	Finish();
}

void InitSessionScene::OnRegDateFormatDone(int32_t result, void *userData)
{
	(void)userData;
	if (result < 0) {
		HandleError(result);
		return;
	}
	SigninExtPopupClose(-2);
	m_result = 0;
	Finish();
}

void InitSessionScene::OnPasswordLoginDone(int32_t result, void *userData)
{
	(void)userData;
	if (result == SIGNIN_EXT_ERROR_COMMERCE_PW_EXPIRED) {
		SigninExtPopupClose(-2);
		m_result = 4;
		Finish();
		return;
	}
	if (result < 0) {
		HandleError(result);
		return;
	}
	SigninExtPopupClose(-2);
	m_result = 0;
	Finish();
}

void InitSessionScene::OnResetPasswordDone(int32_t result, void *userData)
{
	(void)userData;
	if (result == SIGNIN_EXT_ERROR_COMMERCE_RESET_SENT || result >= 0) {
		SigninExtPopupClose(-2);
		m_result = 0;
		Finish();
		return;
	}
	HandleError(result);
}

void InitSessionScene::HandleError(int32_t errCode)
{
	if (errCode == -1) {
		m_result = -1;
		Finish();
		return;
	}
	if (errCode == SIGNIN_EXT_ERROR_NP_ACCOUNT_NOT_MATCH) {
		m_result = 4;
		Finish();
		return;
	}

	ErrorDialogParam err(IDParam(0U), errCode);
	SigninExtErrorShow(&err, NULL,
			   new MemberCallback0<InitSessionScene>(
				   this, &InitSessionScene::OnErrorDismissed));
}

void InitSessionScene::OnErrorDismissed(void *userData)
{
	(void)userData;
	m_result = -3;
	Finish();
}

/* ---- SessionExpiredScene ---- */

SessionExpiredScene::SessionExpiredScene(int32_t reason)
	: PageBase(IDParam(RCO_PAGE_SESSION_EXPIRED)),
	  m_reason(reason),
	  m_header(),
	  m_footer()
{
}

SessionExpiredScene::~SessionExpiredScene()
{
}

void SessionExpiredScene::OnCreate(void)
{
	IDParam titleId(0U);
	const wchar_t *signInId;
	if (m_reason != 0) {
		titleId = IDParam(0x46F7D8ABU);
		signInId = g_passwordChangeModel->m_signInId.m_buf;
	} else {
		titleId = IDParam(0xB994933CU);
		signInId = g_dobVerifyModel->m_signInId.m_buf;
	}

	m_header.Init(m_scene, titleId);
	m_footer.Init(m_scene, new VirtualMemberCallback1<SessionExpiredScene, int32_t>(
			      this, &SessionExpiredScene::OnButtonPressed));
	m_footer.SetButtonVisible(0, true);
	m_footer.SetButtonVisible(1, true);
	m_footer.SetNextLabel(IDParam(0x805BECA6U));

	Widget *scrollView = m_scene->FindChild(IDParam(RCO_DIALOG_SCROLL), 0);
	Widget *inner = scrollView->GetInternalWidget();

	if (m_reason != 0) {
		Plugin::TemplateOpenParam tmplParam;
		SigninExtTemplateOpen(inner, IDParam(0xCFB12704U), tmplParam);

		const wchar_t *fmt = SigninExtGetString(IDParam(0x8F345388U));
		wstring msg = SigninExtFormatMessage(fmt, signInId);
		Widget *msgWidget = m_scene->FindChild(IDParam(0xDEB7FBF1U), 0);
		msgWidget->SetString(msg);

		if (g_passwordChangeModel->m_countryCode == COUNTRY_CODE('j', 'p')) {
			Widget *jpNotice = m_scene->FindChild(IDParam(0x0085FEE8U), 0);
			const wchar_t *jpStr = SigninExtGetString(IDParam(0x128E4203U));
			jpNotice->SetString(wstring(jpStr));
		}
	} else {
		Plugin::TemplateOpenParam tmplParam;
		SigninExtTemplateOpen(inner, IDParam(0x4B582599U), tmplParam);

		Widget *idWidget = m_scene->FindChild(IDParam(0xA88AD8CDU), 0);
		idWidget->SetString(wstring(signInId));
	}
}

void SessionExpiredScene::OnButtonPressed(int32_t buttonId)
{
	if (buttonId == 1) {
		wstring empty;
		SigninExtPopupShow(2, &empty, m_scene,
				   new MemberCallback1<SessionExpiredScene, int32_t>(
					   this, &SessionExpiredScene::OnSendMailPopupResult));
		SigninExtTaskStart(new Commerce2SendResetMailTask(
			new SceneTaskCallback<SessionExpiredScene>(
				this, &SessionExpiredScene::OnSendMailDone)));
	} else {
		PageBase::OnButtonPressed(buttonId);
	}
}

void SessionExpiredScene::OnSendMailDone(int32_t result, void *userData)
{
	(void)userData;
	if (result == SIGNIN_EXT_ERROR_COMMERCE_RESET_SENT || result >= 0) {
		SigninExtPopupClose(-2);
		CloseWithResult(0);
	} else {
		ErrorDialogParam err(IDParam(0U), result);
		SigninExtErrorShow(&err, m_scene, NULL);
	}
}

void SessionExpiredScene::OnSendMailPopupResult(int32_t result, void *userData)
{
	(void)userData;
	if (result == -1)
		SigninExtTaskCancel();
}

/* ---- TermSessionScene ---- */

TermSessionScene::TermSessionScene() : SceneBase()
{
}

TermSessionScene::~TermSessionScene()
{
}

void TermSessionScene::Start(Callback1<int32_t> *callback)
{
	SceneBase::Start(callback);
	SigninExtImageCancelAll(NULL);
	SigninExtTaskStart(new Commerce2LogoutTask(
		new SceneTaskCallback<TermSessionScene>(
			this, &TermSessionScene::OnLogoutDone)));
}

void TermSessionScene::Abort(void)
{
}

void TermSessionScene::OnLogoutDone(int32_t result, void *userData)
{
	(void)result;
	(void)userData;
	SigninExtImageCancelAll(new MemberCallback0<TermSessionScene>(
		this, &TermSessionScene::OnImageCanceled));
}

void TermSessionScene::OnImageCanceled(void *userData)
{
	(void)userData;
	SigninExtTaskStart(new Commerce2TermTask(
		new SceneTaskCallback<TermSessionScene>(
			this, &TermSessionScene::OnCommerceTermDone)));
}

void TermSessionScene::OnCommerceTermDone(int32_t result, void *userData)
{
	(void)result;
	(void)userData;
	m_result = 1;
	Finish();
}

/* ---- ChangePasswordScene ---- */

ChangePasswordScene::ChangePasswordScene()
	: PageBase(IDParam(RCO_PAGE_CHANGE_PW)),
	  m_passwordGroup(),
	  m_header(),
	  m_footer(),
	  m_model(NULL)
{
}

ChangePasswordScene::~ChangePasswordScene()
{
}

void ChangePasswordScene::OnCreate(void)
{
	m_header.Init(m_scene, IDParam(0x46F7D8ABU));
	m_footer.Init(m_scene, new VirtualMemberCallback1<ChangePasswordScene, int32_t>(
			      this, &ChangePasswordScene::OnButtonPressed));
	m_footer.SetButtonVisible(0, true);
	m_footer.SetButtonVisible(1, true);
	m_footer.SetNextLabel(IDParam(0x805BECA6U));

	m_model = g_passwordChangeModel;
	Widget *pwContainer = m_scene->FindChild(IDParam(0xF9717B64U), 0);
	m_passwordGroup.Bind(
		pwContainer, &m_model->m_newPassword, &m_model->m_confirmPassword,
		new MemberCallback2<ChangePasswordScene, int32_t, const wchar_t *>(
			this, &ChangePasswordScene::OnPasswordFieldChanged),
		false);

	bool ready = (m_model->m_newPassword.m_status != 2) &&
		     (m_model->m_confirmPassword.m_status != 1);
	m_footer.SetNextEnabled(ready, false);
}

void ChangePasswordScene::OnButtonPressed(int32_t buttonId)
{
	if (buttonId == 1) {
		wstring empty;
		SigninExtPopupShow(2, &empty, m_scene,
				   new MemberCallback1<ChangePasswordScene, int32_t>(
					   this, &ChangePasswordScene::OnChangePopupResult));
		SigninExtTaskStart(new Commerce2ChangePasswordTask(
			new SceneTaskCallback<ChangePasswordScene>(
				this, &ChangePasswordScene::OnChangePasswordDone)));
	} else {
		PageBase::OnButtonPressed(buttonId);
	}
}

void ChangePasswordScene::OnPasswordFieldChanged(int32_t fieldIndex, const wchar_t *text,
						 void *userData)
{
	(void)userData;
	IDParam errTitle(0U);
	int32_t errCode = 0;
	uint32_t ruleFlags = 0;

	if (fieldIndex == 0) {
		m_model->SetNewPassword(text);
		errTitle = IDParam(0x46F7D8ABU);
		errCode = m_model->m_newPassword.m_errorCode;
		ruleFlags = m_model->m_newPassword.m_ruleFlags;
	} else if (fieldIndex == 1) {
		m_model->SetConfirmPassword(text);
		errTitle = IDParam(0xBA3619A2U);
		errCode = m_model->m_confirmPassword.m_errorCode;
		ruleFlags = 0;
	}

	m_passwordGroup.SyncFromModels();
	bool ready = (m_model->m_newPassword.m_status != 2) &&
		     (m_model->m_confirmPassword.m_status != 1);
	m_footer.SetNextEnabled(ready, true);

	if (errCode >= 0) {
		if (fieldIndex == 0 && m_passwordGroup.GetFieldEditState(0) == 0)
			m_passwordGroup.BeginEditField(1);
	} else {
		ErrorDialogParam err(errTitle, errCode, ruleFlags);
		SigninExtErrorShow(&err, m_scene, NULL);
	}
}

void ChangePasswordScene::OnChangePasswordDone(int32_t result, void *userData)
{
	(void)userData;
	if (result >= 0) {
		SigninExtPopupClose(-2);
		wstring empty;
		SigninExtPopupShow(1, &empty, m_scene, NULL);
		SigninExtTaskStart(new Commerce2LogoutTask(
			new SceneTaskCallback<ChangePasswordScene>(
				this, &ChangePasswordScene::OnLogoutAfterChangeDone)));
	} else {
		ErrorDialogParam err(IDParam(0U), result);
		SigninExtErrorShow(&err, m_scene, NULL);
	}
}

void ChangePasswordScene::OnLogoutAfterChangeDone(int32_t result, void *userData)
{
	(void)result;
	(void)userData;
	SigninExtPopupClose(-2);
	CloseWithResult(0);
}

void ChangePasswordScene::OnChangePopupResult(int32_t result, void *userData)
{
	(void)userData;
	if (result == -1)
		SigninExtTaskCancel();
}

/* ---- VerifyDobScene ---- */

VerifyDobScene::VerifyDobScene()
	: PageBase(IDParam(RCO_PAGE_VERIFY_DOB)),
	  m_signInIdRow(),
	  m_dobRow(),
	  m_header(),
	  m_footer(),
	  m_model(NULL)
{
}

VerifyDobScene::~VerifyDobScene()
{
}

void VerifyDobScene::OnCreate(void)
{
	m_header.Init(m_scene, IDParam(0xB994933CU));
	m_footer.Init(m_scene, new VirtualMemberCallback1<VerifyDobScene, int32_t>(
			      this, &VerifyDobScene::OnButtonPressed));
	m_footer.SetButtonVisible(0, true);
	m_footer.SetButtonVisible(1, true);

	m_model = g_dobVerifyModel;

	Widget *idLabel = m_scene->FindChild(IDParam(0x2B167349U), 0);
	Widget *idBox = m_scene->FindChild(IDParam(0xF70B77F3U), 0);
	m_signInIdRow.BindDirect(
		idLabel, idBox, IDParam(0xC1C05E8BU), &m_model->m_signInId,
		new MemberCallback1<VerifyDobScene, const wchar_t *>(
			this, &VerifyDobScene::OnSignInIdCommit));
	m_signInIdRow.m_textBox.SetPlaceholder(IDParam(0xB459DE1FU));
	m_signInIdRow.SyncFromModel(0);

	Widget *dobContainer = m_scene->FindChild(IDParam(0x801E7639U), 0);
	m_dobRow.Bind(dobContainer, IDParam(0x02987C29U), IDParam(0x02987C29U),
		      &m_model->m_birthDate,
		      new MemberCallback1<VerifyDobScene, BirthDateModel *>(
			      this, &VerifyDobScene::OnDobSelected),
		      false);
	m_dobRow.SyncFromModel();

	bool ready = (m_model->m_signInId.m_length != 0) &&
		     (m_model->m_birthDate.m_status != 2);
	m_footer.SetNextEnabled(ready, false);
}

void VerifyDobScene::OnButtonPressed(int32_t buttonId)
{
	if (buttonId == 0) {
		SigninExtPlayCommonSE(1);
		CloseWithResult(-1);
	} else if (buttonId == 1) {
		wstring empty;
		SigninExtPopupShow(2, &empty, m_scene,
				   new MemberCallback1<VerifyDobScene, int32_t>(
					   this, &VerifyDobScene::OnVerifyPopupResult));
		SigninExtTaskStart(new Commerce2VerifyDobTask(
			new SceneTaskCallback<VerifyDobScene>(
				this, &VerifyDobScene::OnVerifyDone)));
	}
}

void VerifyDobScene::OnSignInIdCommit(const wchar_t *text, void *userData)
{
	(void)userData;
	m_model->m_signInId.SetWide(text);
	m_signInIdRow.SyncFromModel(0);
	bool ready = (m_model->m_signInId.m_length != 0) &&
		     (m_model->m_birthDate.m_status != 2);
	m_footer.SetNextEnabled(ready, true);
}

void VerifyDobScene::OnDobSelected(BirthDateModel *date, void *userData)
{
	(void)userData;
	m_model->m_birthDate.CopyFrom(*date);
	m_dobRow.SyncFromModel();
	bool ready = (m_model->m_signInId.m_length != 0) &&
		     (m_model->m_birthDate.m_status != 2);
	m_footer.SetNextEnabled(ready, true);
}

void VerifyDobScene::OnVerifyDone(int32_t result, void *userData)
{
	(void)userData;
	if (result >= 0) {
		SigninExtPopupClose(-2);
		CloseWithResult(0);
	} else {
		Callback0 *cb = NULL;
		if (result == SIGNIN_EXT_ERROR_COMMERCE_SESSION_EXPIRED) {
			cb = new MemberCallback0<VerifyDobScene>(
				this, &VerifyDobScene::OnSessionExpiredDismissed);
		}
		ErrorDialogParam err(IDParam(0U), result);
		SigninExtErrorShow(&err, m_scene, cb);
	}
}

void VerifyDobScene::OnSessionExpiredDismissed(void *userData)
{
	(void)userData;
	CloseWithResult(-3);
}

void VerifyDobScene::OnVerifyPopupResult(int32_t result, void *userData)
{
	(void)userData;
	if (result == -1)
		SigninExtTaskCancel();
}

/* ---- DobFieldRow ---- */

DobFieldRow::DobFieldRow()
	: FormRowBase(), m_label(), m_dobButton(), m_model(NULL)
{
}

DobFieldRow::~DobFieldRow()
{
}

void DobFieldRow::Bind(Widget *parent, const IDParam &labelId, const IDParam &dialogTitleId,
		       BirthDateModel *model, Callback1<BirthDateModel *> *onSelected,
		       bool openWrapper)
{
	SetupContainer(parent, false, openWrapper);
	Plugin::TemplateOpenParam tmplParam;
	if (SigninExtTemplateOpen(m_container, IDParam(0xA23E30C9U), tmplParam) < 0) {
		if (onSelected != NULL)
			delete onSelected;
		return;
	}

	Widget *labelWidget = m_container->FindChild(IDParam(0x1F7159E8U), 0);
	Widget *btnWidget = m_container->FindChild(IDParam(0x77CC8ED6U), 0);
	labelWidget->m_name.m_id = 0;
	btnWidget->m_name.m_id = 0;

	m_model = model;
	m_label.Bind(labelWidget, labelId);
	m_dobButton.Bind(btnWidget, dialogTitleId, model, onSelected);
}

void DobFieldRow::SyncFromModel(void)
{
	if (m_model != NULL) {
		m_label.SetErrorState(m_model->m_errorCode);
		m_dobButton.RefreshText();
	}
}

/* ---- PopupDialog & PopupDialogManager ---- */

static const CommonGuiDialogButton s_resetMailConfirmButtons[4] = {
	{ 12, 0x3887A5A3U, 10, 0x20413274U },
	{ 13, 0xE10ACC33U, 2,  0x20413274U },
	{ 14, 0x805BECA6U, 3,  0x20413274U },
	{ 0,  0,           0,  0 }
};

class PopupDialog;
static PopupDialog *s_popupDialog = NULL;
static Callback1<int32_t> *s_popupCallback = NULL;

template <typename Obj>
class MemberCommonGuiHandler : public CommonGuiDialogHandler {
public:
	typedef void (Obj::*Method)(SceInt32, SceInt32, void *);

	MemberCommonGuiHandler(Obj *obj, Method method, void *userData = NULL)
		: m_obj(obj), m_method(method), m_userData(userData)
	{
	}

	virtual ~MemberCommonGuiHandler() {}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (m_obj != NULL && m_method != NULL)
			(m_obj->*m_method)(value, button, m_userData);
	}

	Obj *m_obj;
	Method m_method;
	void *m_userData;
};

class PopupDialog : public DialogBase {
public:
	PopupDialog() : DialogBase(), m_busyIndicator(NULL), m_powerTick() {}
	virtual ~PopupDialog() {}

	void ShowCommonGui(const wstring &text, const CommonGuiDialogLayout &layout);
	void ShowResetMailHelp(const wstring &email);
	void ShowResetMailConfirm(const wstring &email);
	void ShowCancelConfirm(void);
	void SetupPanelReadyShow(void);
	void ShowCancelableBusy(void);
	void ShowBusy(void);
	void ShowCustomPopup(const IDParam &tmplId, const IDParam &textWidgetId,
			     const wstring &text);
	void Open(int32_t type, const wstring *text, Widget *parentScene);

	virtual void PlayOpenAnim(void);
	virtual void PlayCloseAnim(void);
	virtual void Close(bool animate);

	void OnPanelReady(int32_t type, Handler *self, Event *e, void *userData);
	void OnInfoOkClicked(int32_t type, Handler *self, Event *e, void *userData);
	void OnBusyCancelClicked(int32_t type, Handler *self, Event *e, void *userData);
	void OnCommonGuiButton(SceInt32 value, SceInt32 button, void *userData);

	Widget *m_busyIndicator;
	PowerTickGuard m_powerTick;
};

void PopupDialog::ShowCommonGui(const wstring &text, const CommonGuiDialogLayout &layout)
{
	CommonGuiLayoutDialogParam param;
	param.resourcePlugin = SigninExtGetPlugin();
	param.handler = new MemberCommonGuiHandler<PopupDialog>(
		this, &PopupDialog::OnCommonGuiButton);
	param.text = text;
	param.layout = layout;
	param.bgColor[0] = 0.0f;
	param.bgColor[1] = 0.0f;
	param.bgColor[2] = 0.0f;
	param.bgColor[3] = 0.0f;
	param.soundId = 0xE0DA0908U;

	int32_t id = SceCommonGuiDialog_CCFCD9A6(&param);
	if (id >= 0) {
		m_commonDialogId = id;
		PrepareAndShow(true);
	}
}

void PopupDialog::ShowResetMailHelp(const wstring &email)
{
	wstring empty;
	ShowCommonGui(empty, SceCommonGuiDialog_AF75CA6D);
	if (m_commonDialogId < 0)
		return;

	Box *contentBox = static_cast<Box *>(
		SceCommonGuiDialog_8947FC23(m_commonDialogId, 10));
	if (contentBox == NULL)
		return;

	Box::LayoutVars vars = contentBox->GetLayoutVars();
	vars.space = 24.0f;
	contentBox->SetLayoutVars(vars);
	contentBox->SetChildrenAdjustX(Box::CHILDREN_ADJUST_MAX, 0.0f);

	Widget *textPart = SceCommonGuiDialog_8947FC23(m_commonDialogId, 1);
	if (textPart != NULL)
		textPart->DestroyWidget();

	Plugin::TemplateOpenParam tmplParam;
	if (SigninExtTemplateOpen(contentBox, IDParam(0x3CDA4A14U), tmplParam) < 0)
		return;

	wstring desc(SigninExtGetString(IDParam(0xC0B23583U)));
	desc.append(L"\n");
	desc.append(SigninExtGetString(IDParam(0xF3A61E0DU)));
	Widget *descW = contentBox->FindChild(IDParam(0xDD948AE0U), 0);
	descW->SetString(desc);

	const wchar_t *fmt = SigninExtGetString(IDParam(0xB95890DFU));
	wstring mailMsg = SigninExtFormatMessage(fmt, email.c_str());
	Widget *mailW = contentBox->FindChild(IDParam(0x1854F982U), 0);
	mailW->SetString(mailMsg);

	Widget *num1 = contentBox->FindChild(IDParam(0x4AC0CF00U), 0);
	num1->SetString(wstring(L"1. "));
	Widget *num2 = contentBox->FindChild(IDParam(0x741524C9U), 0);
	num2->SetString(wstring(L"2. "));
}

void PopupDialog::ShowResetMailConfirm(const wstring &email)
{
	CommonGuiDialogButton buttons[4];
	sce_paf_memcpy(buttons, s_resetMailConfirmButtons, sizeof(buttons));

	CommonGuiDialogLayout layout;
	layout.page = SceCommonGuiDialog_78399261.page;
	layout.parts = SceCommonGuiDialog_AF75CA6D.parts;
	layout.buttons = buttons;

	const wchar_t *fmt = SigninExtGetString(IDParam(0x3ACC4DB1U));
	wstring text = SigninExtFormatMessage(fmt, email.c_str());
	ShowCommonGui(text, layout);
}

void PopupDialog::ShowCancelConfirm(void)
{
	wstring text(SigninExtGetString(IDParam(0x44B9CCEFU)));
	ShowCommonGui(text, SceCommonGuiDialog_6CD802FC);
}

void PopupDialog::SetupPanelReadyShow(void)
{
	m_panel->Show(0xff, NULL, NULL, 0.0f);
	m_panel->SetMetaAlpha(0.0f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
	m_panel->AddEventListener(
		0x20100004,
		new MemberEventListener<PopupDialog>(
			this, &PopupDialog::OnPanelReady, NULL),
		false);
	PrepareAndShow(false);
}

void PopupDialog::ShowCancelableBusy(void)
{
	m_pageId = IDParam(RCO_PAGE_BUSY_CANCELABLE);
	Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = 2;
	m_scene = SigninExtPageOpenWithParam(m_pageId, openParam);
	m_panel = m_scene->FindChild(IDParam(RCO_DIALOG_PANEL), 0);
	m_busyIndicator = m_scene->FindChild(IDParam(RCO_DIALOG_BUSY_INDICATOR), 0);

	Widget *cancelBtn = m_panel->FindChild(IDParam(RCO_BTN_POPUP_ACTION), 0);
	cancelBtn->AddEventListener(
		ButtonBase::CB_BTN_DECIDE,
		new MemberEventListener<PopupDialog>(
			this, &PopupDialog::OnBusyCancelClicked, NULL),
		false);

	SetupPanelReadyShow();
	m_powerTick.Enable();
}

void PopupDialog::ShowBusy(void)
{
	m_pageId = IDParam(RCO_PAGE_BUSY);
	Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = 2;
	m_scene = SigninExtPageOpenWithParam(m_pageId, openParam);
	m_panel = m_scene->FindChild(IDParam(RCO_DIALOG_PANEL), 0);
	m_busyIndicator = m_scene->FindChild(IDParam(RCO_DIALOG_BUSY_INDICATOR), 0);

	SetupPanelReadyShow();
	m_powerTick.Enable();
}

void PopupDialog::ShowCustomPopup(const IDParam &tmplId, const IDParam &textWidgetId,
				  const wstring &text)
{
	m_pageId = IDParam(RCO_PAGE_POPUP_INFO);
	Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = 2;
	m_scene = SigninExtPageOpenWithParam(m_pageId, openParam);
	m_panel = m_scene->FindChild(IDParam(RCO_DIALOG_PANEL), 0);

	Widget *okBtn = m_panel->FindChild(IDParam(RCO_BTN_POPUP_ACTION), 0);
	okBtn->AddEventListener(
		ButtonBase::CB_BTN_DECIDE,
		new MemberEventListener<PopupDialog>(
			this, &PopupDialog::OnInfoOkClicked, NULL),
		false);

	Widget *container = m_panel->FindChild(IDParam(RCO_DIALOG_POPUP_CONTAINER), 0);
	Plugin::TemplateOpenParam tmplParam;
	if (SigninExtTemplateOpen(container, tmplId, tmplParam) >= 0) {
		Widget *textWidget = m_panel->FindChild(textWidgetId, 0);
		textWidget->SetString(text);
		SetupPanelReadyShow();
	}
}

void PopupDialog::Open(int32_t type, const wstring *text, Widget *parentScene)
{
	m_parentScene = parentScene;
	switch (type) {
	case 0:
		ShowCustomPopup(IDParam(0x588E67D9U), IDParam(RCO_DIALOG_POPUP_TEXT), *text);
		break;
	case 1:
		ShowBusy();
		break;
	case 2:
		ShowCancelableBusy();
		break;
	case 3:
		ShowCancelConfirm();
		break;
	case 4:
		ShowResetMailConfirm(*text);
		break;
	case 5:
		ShowResetMailHelp(*text);
		break;
	case 6:
		ShowCommonGui(*text, SceCommonGuiDialog_AF75CA6D);
		break;
	default:
		break;
	}
}

void PopupDialog::PlayOpenAnim(void)
{
	DialogBase::PlayOpenAnim();
	if (m_busyIndicator != NULL)
		static_cast<BusyIndicator *>(m_busyIndicator)->Start();
}

void PopupDialog::PlayCloseAnim(void)
{
	if (m_busyIndicator != NULL)
		static_cast<BusyIndicator *>(m_busyIndicator)->Stop();
	DialogBase::PlayCloseAnim();
}

void PopupDialog::Close(bool animate)
{
	if (animate && m_panel != NULL &&
	    m_panel->m_map_e_listener.find(0x20100004) != m_panel->m_map_e_listener.end()) {
		animate = false;
	}
	DialogBase::Close(animate);
}

void PopupDialog::OnPanelReady(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	m_panel->DeleteEventListener(0x20100004, NULL);
	SigninExtPlayCommonSE(4);
	PlayOpenAnim();
}

void PopupDialog::OnInfoOkClicked(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	if (s_popupCallback == NULL)
		SigninExtPlayCommonSE(0);
	SigninExtPopupClose((int32_t)(intptr_t)userData);
}

void PopupDialog::OnBusyCancelClicked(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)e;
	(void)userData;
	static_cast<ButtonBase *>(self)->Disable(false);
	SigninExtPlayCommonSE(1);
	if (s_popupCallback != NULL)
		s_popupCallback->Invoke(-1);
}

void PopupDialog::OnCommonGuiButton(SceInt32 value, SceInt32 button, void *userData)
{
	(void)value;
	(void)userData;
	int32_t mapped = -3;
	if (button == 2 || button == 4)
		mapped = 0;
	else if (button == 3 || button == 5)
		mapped = -1;

	SceCommonGuiDialog_BC311DC0(m_commonDialogId);
	m_commonDialogId = -1;
	SigninExtPopupClose(mapped);
}

void SigninExtPopupInit(void)
{
	s_popupDialog = NULL;
	s_popupCallback = NULL;
}

void SigninExtPopupCloseImmediate(void)
{
	if (s_popupDialog != NULL) {
		s_popupDialog->Close(false);
		delete s_popupDialog;
		s_popupDialog = NULL;
		if (s_popupCallback != NULL) {
			delete s_popupCallback;
			s_popupCallback = NULL;
		}
	}
}

void SigninExtPopupTerm(void)
{
	if (s_popupDialog != NULL) {
		s_popupDialog->Close(false);
		delete s_popupDialog;
		s_popupDialog = NULL;
		if (s_popupCallback != NULL) {
			delete s_popupCallback;
			s_popupCallback = NULL;
		}
	}
}

void SigninExtPopupClose(int32_t result)
{
	if (s_popupDialog != NULL) {
		s_popupDialog->Close(true);
		delete s_popupDialog;
		s_popupDialog = NULL;
		if (s_popupCallback != NULL) {
			Callback1<int32_t> *cb = s_popupCallback;
			s_popupCallback = NULL;
			if (result != -2)
				cb->Invoke(result);
			delete cb;
		}
	}
}

int32_t SigninExtPopupShow(int32_t type, const wstring *text,
			   Widget *parentScene, Callback1<int32_t> *callback)
{
	if (s_popupDialog != NULL) {
		s_popupDialog->Close(true);
		delete s_popupDialog;
		s_popupDialog = NULL;
		if (s_popupCallback != NULL) {
			Callback1<int32_t> *oldCb = s_popupCallback;
			s_popupCallback = NULL;
			delete oldCb;
		}
	}

	SigninExtImeEndEdit();
	s_popupDialog = new PopupDialog();
	if (s_popupDialog == NULL) {
		if (callback != NULL)
			delete callback;
		return SIGNIN_EXT_ERROR_NOMEM;
	}

	s_popupDialog->Open(type, text, parentScene);
	s_popupCallback = callback;
	return 0;
}

/* ---- ErrorDialog & ErrorDialogManager ---- */

class ErrorDialog;
static ErrorDialog *s_errorDialog = NULL;
static Callback0 *s_errorCallback = NULL;

static void LookupErrorMessage(int32_t errorCode, wstring &outMsg)
{
	uint32_t resId = 0;
	switch (errorCode) {
	case SIGNIN_EXT_ERROR_COMMERCE_BAD_REQUEST:
		resId = 0xD67905F5U;
		break;
	case SIGNIN_EXT_ERROR_COMMERCE_NOT_FOUND:
		resId = 0xD2551728U;
		break;
	case SIGNIN_EXT_ERROR_COMMERCE_PW_WEAK:
		resId = 0xADB0003AU;
		break;
	case -2141908972:
		resId = 0xD7D25EEBU;
		break;
	case -2141908965:
	case -2141908964:
		resId = 0x6FF43E24U;
		break;
	case -2141908963:
		resId = 0x391BA7BAU;
		break;
	case -2141908926:
	case SIGNIN_EXT_ERROR_COMMERCE_PW_EXPIRED:
	case -2141908912:
		resId = 0x305E669FU;
		break;
	case SIGNIN_EXT_ERROR_COMMERCE_SESSION_EXPIRED:
		resId = 0x1AB21033U;
		break;
	case SIGNIN_EXT_ERROR_PW_INVALID_CHAR:
		resId = 0x305E669FU;
		break;
	case SIGNIN_EXT_ERROR_PW_POLICY:
		resId = 0x970B858BU;
		break;
	case SIGNIN_EXT_ERROR_PW_MISMATCH:
		resId = 0x337500E1U;
		break;
	case SIGNIN_EXT_ERROR_PW_SAME_AS_OLD:
		resId = 0xA7D9B4ADU;
		break;
	case -2130706427:
		resId = 0x9BF68905U;
		break;
	case SIGNIN_EXT_ERROR_ACCOUNT_NOT_FOUND:
		resId = 0xD2551728U;
		break;
	case -2130706425:
		resId = 0x5EB9A66FU;
		break;
	case -2130706424:
		resId = 0x46301CE3U;
		break;
	default:
		break;
	}

	const wchar_t *str = SigninExtGetString(IDParam(resId));
	outMsg = (str != NULL) ? wstring(str) : wstring();
}

static void AppendPasswordRuleLine(Widget *parent, const IDParam &strId,
				   bool addSpacing, bool highlightError)
{
	Plugin::TemplateOpenParam tmplParam;
	if (SigninExtTemplateOpen(parent, IDParam(0xB02BC8F2U), tmplParam) < 0)
		return;

	Widget *textW = parent->FindChild(IDParam(0xB4EF8F28U), 0);
	Widget *bulletW = parent->FindChild(IDParam(0x1AA7D754U), 0);
	textW->m_name.m_id = 0;
	bulletW->m_name.m_id = 0;

	const wchar_t *s = SigninExtGetString(strId);
	textW->SetString((s != NULL) ? wstring(s) : wstring());

	if (addSpacing) {
		AdjustParam *adj = new AdjustParam();
		if (textW->m_adjust_param != NULL)
			*adj = *textW->m_adjust_param;
		adj->m_bottom_margin = 10.0f;
		ScePafWidget_55D83675(textW, adj);
	}

	if (highlightError) {
		math::v4 errColor(229.0f / 255.0f, 0.0f, 79.0f / 255.0f, 1.0f);
		textW->SetColor(errColor, NULL, Widget::ANIM_COLOR, 0, 0.0f, NULL, NULL);
		bulletW->SetColor(errColor, NULL, Widget::ANIM_COLOR, 0, 0.0f, NULL, NULL);
	}
}

static void AppendPasswordRuleList(Widget *parent, uint32_t ruleFlags)
{
	AppendPasswordRuleLine(parent, IDParam(0xFE9BC8ABU), true, (ruleFlags & 0x01U) != 0);
	AppendPasswordRuleLine(parent, IDParam(0xBDCA2FFDU), true, (ruleFlags & 0x02U) != 0);
	AppendPasswordRuleLine(parent, IDParam(0x60692E33U), true, (ruleFlags & 0x04U) != 0);
	AppendPasswordRuleLine(parent, IDParam(0x6C6D242DU), true, false);
	AppendPasswordRuleLine(parent, IDParam(0xA7D9B4ADU), false, (ruleFlags & 0x60U) != 0);
}

static void BuildErrorContent(Widget *parent, ErrorDialogParam *param)
{
	Plugin::TemplateOpenParam tmplParam;
	if (SigninExtTemplateOpen(parent, IDParam(0x06A72A0EU), tmplParam) < 0)
		return;

	if (param->message.empty())
		LookupErrorMessage(param->errorCode, param->message);

	Widget *msgW = parent->FindChild(IDParam(0x8A59BBCDU), 0);
	msgW->m_name.m_id = 0;
	msgW->SetString(param->message);

	if (param->errorCode == SIGNIN_EXT_ERROR_PW_POLICY) {
		AdjustParam *adj = new AdjustParam();
		if (msgW->m_adjust_param != NULL)
			*adj = *msgW->m_adjust_param;
		adj->m_bottom_margin = 13.0f;
		ScePafWidget_55D83675(msgW, adj);
		AppendPasswordRuleList(parent, param->pwRuleFlags);
	}
}

class ErrorDialog : public DialogBase {
public:
	ErrorDialog() : DialogBase(), m_edgeBinder() {}
	virtual ~ErrorDialog() {}

	void OpenCustom(ErrorDialogParam *param);
	void OpenSystemError(ErrorDialogParam *param);
	void OnCustomOkClicked(int32_t type, Handler *self, Event *e, void *userData);
	void OnSystemErrorClosed(SceInt32 value, SceInt32 button, void *userData);
	static void CloseAndNotify(void);

	ScrollListEdgeHelper m_edgeBinder;
};

void ErrorDialog::OpenCustom(ErrorDialogParam *param)
{
	IDParam edgeStyle(0U);
	math::v4 scrollBarPos(0.0f, 0.0f, 0.0f, 0.0f);

	if (param->titleId.GetIDHash() != 0) {
		m_pageId = IDParam(RCO_PAGE_ERROR_WITH_TITLE);
		edgeStyle = IDParam(RCO_STYLE_LIST_EDGE_STD);
		scrollBarPos = math::v4(-12.0f, -12.0f, 0.0f, 0.0f);
	} else {
		m_pageId = IDParam(RCO_PAGE_ERROR_NO_TITLE);
		edgeStyle = IDParam(RCO_STYLE_LIST_EDGE_NO_TITLE);
		scrollBarPos = math::v4(-12.0f, 32.0f, 0.0f, 0.0f);
	}

	Plugin::PageOpenParam openParam;
	openParam.overwrite_draw_priority = 3;
	m_scene = SigninExtPageOpenWithParam(m_pageId, openParam);
	if (m_scene == NULL)
		return;

	m_panel = m_scene->FindChild(IDParam(RCO_DIALOG_PANEL), 0);
	ScrollViewBase *scrollView = static_cast<ScrollViewBase *>(
		m_scene->FindChild(IDParam(RCO_DIALOG_SCROLL), 0));
	Widget *scrollBar = m_scene->FindChild(IDParam(RCO_DIALOG_SCROLLBAR), 0);

	scrollBar->SetParent(m_panel, NULL, 0);
	scrollBar->SetAnchor(2, scrollBar->m_anchor_y, scrollBar->m_anchor_z, NULL);
	scrollBar->SetAlign(2, scrollBar->m_align_y, scrollBar->m_align_z, NULL);
	scrollBar->SetPos(scrollBarPos, NULL, Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);

	scrollView->SetScrollBarController(new SigninExtScrollBarController());
	m_edgeBinder.Attach(scrollView, m_panel, edgeStyle);

	if (param->titleId.GetIDHash() != 0) {
		Widget *titleW = m_scene->FindChild(IDParam(RCO_DIALOG_TITLE), 0);
		SigninExtSetAutoShrinkText(titleW, SigninExtGetString(param->titleId));
	}

	Widget *content = m_scene->FindChild(IDParam(RCO_DIALOG_CONTENT), 0);
	BuildErrorContent(content, param);

	Widget *okBtn = m_scene->FindChild(IDParam(RCO_BTN_ERROR_OK), 0);
	okBtn->AddEventListener(
		ButtonBase::CB_BTN_DECIDE,
		new MemberEventListener<ErrorDialog>(
			this, &ErrorDialog::OnCustomOkClicked, NULL),
		false);

	SigninExtPlayCommonSE(5);
}

void ErrorDialog::OpenSystemError(ErrorDialogParam *param)
{
	CommonGuiMessageDialogParam dlgParam;
	dlgParam.type = param->errorCode;
	dlgParam.resourcePlugin = SigninExtGetPlugin();
	dlgParam.option.flags = 4;
	dlgParam.handler = new MemberCommonGuiHandler<ErrorDialog>(
		this, &ErrorDialog::OnSystemErrorClosed);
	dlgParam.bgColor[0] = 0.0f;
	dlgParam.bgColor[1] = 0.0f;
	dlgParam.bgColor[2] = 0.0f;
	dlgParam.bgColor[3] = 0.0f;

	int32_t id = SceCommonGuiDialog_20B141F1(&dlgParam);
	if (id >= 0)
		m_commonDialogId = id;
}

void ErrorDialog::OnCustomOkClicked(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	SigninExtPlayCommonSE(0);
	CloseAndNotify();
}

void ErrorDialog::OnSystemErrorClosed(SceInt32 value, SceInt32 button, void *userData)
{
	(void)value;
	(void)button;
	(void)userData;
	m_commonDialogId = -1;
	CloseAndNotify();
}

void ErrorDialog::CloseAndNotify(void)
{
	if (s_errorDialog != NULL) {
		s_errorDialog->Close(true);
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

void SigninExtErrorInit(void)
{
	s_errorDialog = NULL;
	s_errorCallback = NULL;
}

void SigninExtErrorCloseImmediate(void)
{
	if (s_errorDialog != NULL) {
		s_errorDialog->Close(false);
		delete s_errorDialog;
		s_errorDialog = NULL;
		if (s_errorCallback != NULL) {
			delete s_errorCallback;
			s_errorCallback = NULL;
		}
	}
}

void SigninExtErrorTerm(void)
{
	if (s_errorDialog != NULL) {
		s_errorDialog->Close(false);
		delete s_errorDialog;
		s_errorDialog = NULL;
		if (s_errorCallback != NULL) {
			delete s_errorCallback;
			s_errorCallback = NULL;
		}
	}
}

int32_t SigninExtErrorShow(ErrorDialogParam *param, Widget *parentScene, Callback0 *callback)
{
	if (s_errorDialog != NULL) {
		s_errorDialog->Close(true);
		delete s_errorDialog;
		s_errorDialog = NULL;
		if (s_errorCallback != NULL) {
			Callback0 *oldCb = s_errorCallback;
			s_errorCallback = NULL;
			delete oldCb;
		}
	}

	SigninExtImeEndEdit();
	SigninExtPopupClose(-2);

	s_errorDialog = new ErrorDialog();
	if (s_errorDialog == NULL) {
		if (callback != NULL)
			delete callback;
		return SIGNIN_EXT_ERROR_NOMEM;
	}

	s_errorDialog->m_parentScene = parentScene;
	if (param->message.empty())
		LookupErrorMessage(param->errorCode, param->message);

	if (!param->message.empty())
		s_errorDialog->OpenCustom(param);
	else
		s_errorDialog->OpenSystemError(param);

	s_errorDialog->PrepareAndShow(true);
	s_errorCallback = callback;
	return 0;
}

/* ---- FieldLabel & FormRowBase ---- */

FieldLabel::FieldLabel() : m_widget(NULL), m_textId(0U)
{
}

FieldLabel::~FieldLabel()
{
}

void FieldLabel::Bind(Widget *widget, const IDParam &textId)
{
	m_widget = NULL;
	if (widget != NULL && widget->IsInherit(Text::TypeName()))
		m_widget = widget;

	if (m_widget != NULL) {
		m_textId = textId;
		const wchar_t *str = SigninExtGetString(m_textId);
		m_widget->SetString((str != NULL) ? wstring(str) : wstring());
	}
}

void FieldLabel::SetErrorState(int32_t errCode)
{
	if (m_widget == NULL)
		return;

	uint32_t packed = (errCode < 0) ? 0xFF4F00E5U : 0xFFFFFFFFU;
	math::v4 col(
		(float)(packed & 0xFFU) / 255.0f,
		(float)((packed >> 8) & 0xFFU) / 255.0f,
		(float)((packed >> 16) & 0xFFU) / 255.0f,
		(float)((packed >> 24) & 0xFFU) / 255.0f);
	m_widget->SetColor(col, NULL, Widget::ANIM_COLOR, 0, 0.0f, NULL, NULL);
}

void FormRowBase::SetupContainer(Widget *parent, bool verticalBox, bool openWrapperTemplate)
{
	if (openWrapperTemplate) {
		Plugin::TemplateOpenParam tmplParam;
		if (SigninExtTemplateOpen(parent, IDParam(0x60047ADCU), tmplParam) < 0)
			return;
		m_container = parent->FindChild(IDParam(0xD555F437U), 0);
		m_container->m_name.m_id = 0;
	} else {
		m_container = parent;
	}

	if (verticalBox && m_container != NULL && m_container->IsInherit(Box::TypeName())) {
		Box *box = static_cast<Box *>(m_container);
		box->SetLayoutType(Box::LAYOUT_VERTICAL);
		Box::LayoutVars vars = box->GetLayoutVars();
		vars.space = GetBoxSpacing();
		box->SetLayoutVars(vars);
	}
}

/* ---- ScrollListEdgeHelper & SigninExtScrollBarController ---- */

void SigninExtScrollBarController::UpdateLayout(const ScrollViewBarLayout &layout)
{
	if (layout.m_vertical_bar != NULL) {
		math::v4 sz = *layout.m_vertical_bar->GetSize(0);
		sz.set_y(layout.m_view_size.extract_y() - 36.0f);
		layout.m_vertical_bar->SetSize(sz, NULL, Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
	}
}

ScrollListEdgeHelper::ScrollListEdgeHelper() : m_listEdge(NULL)
{
}

ScrollListEdgeHelper::~ScrollListEdgeHelper()
{
}

void ScrollListEdgeHelper::Attach(Widget *parent, Widget *target, const IDParam &styleId)
{
	m_listEdge = static_cast<ListEdge *>(
		SigninExtCreateWidget(parent, ListEdge::TypeName(), IDParam(0U), styleId));
	m_listEdge->SetAdjust(1, 1, 1);
	m_listEdge->AttachTarget(target);
}

/* ---- TextFieldRow & SelectListFieldRow ---- */

TextFieldRow::TextFieldRow() : FormRowBase(), m_label(), m_textBox()
{
}

TextFieldRow::~TextFieldRow()
{
}

void TextFieldRow::BindDirect(Widget *labelWidget, Widget *textBoxWidget,
			      const IDParam &labelId, TextInputField *field,
			      Callback1<const wchar_t *> *onCommit)
{
	m_label.Bind(labelWidget, labelId);
	m_textBox.Bind(static_cast<TextBox *>(textBoxWidget), field, onCommit);
}

void TextFieldRow::BindTemplate(Widget *parent, const IDParam &labelId,
				TextInputField *field, Callback1<const wchar_t *> *onCommit,
				bool openWrapper)
{
	SetupContainer(parent, false, openWrapper);
	Plugin::TemplateOpenParam tmplParam;
	if (SigninExtTemplateOpen(m_container, IDParam(0xDE957AC1U), tmplParam) < 0) {
		if (onCommit != NULL)
			delete onCommit;
		return;
	}

	Widget *labelW = m_container->FindChild(IDParam(0x0F99FB88U), 0);
	Widget *boxW = m_container->FindChild(IDParam(0x65DA9F51U), 0);
	labelW->m_name.m_id = 0;
	boxW->m_name.m_id = 0;

	m_label.Bind(labelW, labelId);
	m_textBox.Bind(static_cast<TextBox *>(boxW), field, onCommit);
}

void TextFieldRow::SyncFromModel(int32_t errCode)
{
	m_label.SetErrorState(errCode);
	m_textBox.SyncFromModel();
}

SelectListFieldRow::SelectListFieldRow() : FormRowBase(), m_label(), m_selectButton()
{
}

SelectListFieldRow::~SelectListFieldRow()
{
}

void SelectListFieldRow::Bind(Widget *labelWidget, Widget *btnWidget,
			      const IDParam &labelId, const IDParam &dialogTitleId,
			      SelectListModel *model, Callback1<int32_t> *onSelected)
{
	m_label.Bind(labelWidget, labelId);
	m_selectButton.Bind(btnWidget, dialogTitleId, model, onSelected);
}

void SelectListFieldRow::SyncFromModel(int32_t errCode)
{
	m_label.SetErrorState(errCode);
	m_selectButton.RefreshText();
}

/* ---- FooterBar ---- */

FooterBar::FooterBar()
	: m_onButton(NULL), m_root(NULL), m_nextLabelId(0U)
{
	m_buttons[0] = NULL;
	m_buttons[1] = NULL;
}

FooterBar::~FooterBar()
{
	if (m_onButton != NULL) {
		delete m_onButton;
		m_onButton = NULL;
	}
}

void FooterBar::Init(Widget *parent, Callback1<int32_t> *onButton)
{
	m_onButton = onButton;
	Plugin::TemplateOpenParam tmplParam;
	if (SigninExtTemplateOpen(parent, IDParam(0xEB68029FU), tmplParam) < 0)
		return;

	m_root = parent->FindChild(IDParam(0xE8121D6BU), 0);
	m_buttons[0] = parent->FindChild(IDParam(0xC2D5367FU), 0);
	m_buttons[1] = parent->FindChild(IDParam(0xB908AF82U), 0);

	m_root->SetMetaAlpha(0.0f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);

	for (int32_t i = 0; i < 2; ++i) {
		m_buttons[i]->AddEventListener(
			ButtonBase::CB_BTN_DECIDE,
			new MemberEventListener<FooterBar>(
				this, &FooterBar::OnButtonEvent, (void *)(intptr_t)i),
			false);
		m_buttons[i]->Hide(0xff, NULL, NULL, 0.0f);
	}
	m_buttons[0]->m_input_target = false;
}

void FooterBar::SetButtonVisible(int32_t index, bool visible)
{
	if (visible) {
		m_root->SetMetaAlpha(1.0f, NULL, Widget::ANIM_META_ALPHA, 0, 0.0f, NULL, NULL);
		m_buttons[index]->Show(0xff, NULL, NULL, 0.0f);
	} else {
		m_buttons[index]->Hide(0xff, NULL, NULL, 0.0f);
	}
}

void FooterBar::SetNextLabel(const IDParam &labelId)
{
	m_nextLabelId = labelId;
	const wchar_t *str = SigninExtGetString(m_nextLabelId);
	m_buttons[1]->SetString((str != NULL) ? wstring(str) : wstring());
}

void FooterBar::SetNextEnabled(bool enabled, bool animated)
{
	ButtonBase *btn = static_cast<ButtonBase *>(m_buttons[1]);
	if (enabled)
		btn->Enable(!animated);
	else
		btn->Disable(!animated);
}

void FooterBar::OnButtonEvent(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	if (m_onButton != NULL)
		m_onButton->Invoke((int32_t)(intptr_t)userData);
}

/* ---- PasswordInputGroup ---- */

PasswordInputGroup::PasswordInputGroup()
	: FormRowBase(),
	  m_pwRow(),
	  m_confirmRow(),
	  m_pwField(NULL),
	  m_confirmField(NULL),
	  m_onFieldCommit(NULL)
{
}

PasswordInputGroup::~PasswordInputGroup()
{
	if (m_onFieldCommit != NULL)
		delete m_onFieldCommit;
}

void PasswordInputGroup::Bind(Widget *parent, PasswordField *pwField,
			      ConfirmPasswordField *confirmField,
			      Callback2<int32_t, const wchar_t *> *onFieldCommit,
			      bool openWrapper)
{
	m_pwField = pwField;
	m_confirmField = confirmField;
	m_onFieldCommit = onFieldCommit;
	SetupContainer(parent, true, openWrapper);

	Plugin::TemplateOpenParam tmplParam;
	if (SigninExtTemplateOpen(m_container, IDParam(0x408448F6U), tmplParam) < 0)
		return;

	Widget *pwLabel = m_container->FindChild(IDParam(0x772B5968U), 0);
	Widget *pwBox = m_container->FindChild(IDParam(0x1504063FU), 0);
	m_pwRow.BindDirect(
		pwLabel, pwBox, IDParam(0x46F7D8ABU), m_pwField,
		new MemberCallback1<PasswordInputGroup, const wchar_t *, int32_t>(
			this, &PasswordInputGroup::OnFieldCommit, 0));

	Widget *confirmBox = m_container->FindChild(IDParam(0xBC8F46B0U), 0);
	m_confirmRow.BindTemplate(
		confirmBox, IDParam(0xBA3619A2U), m_confirmField,
		new MemberCallback1<PasswordInputGroup, const wchar_t *, int32_t>(
			this, &PasswordInputGroup::OnFieldCommit, 1),
		false);

	Widget *infoBtn = m_container->FindChild(IDParam(0xC4F67FD8U), 0);
	infoBtn->AddEventListener(
		ButtonBase::CB_BTN_DECIDE,
		new MemberEventListener<PasswordInputGroup>(
			this, &PasswordInputGroup::OnPolicyInfoClicked, NULL),
		false);

	Widget *policyText = m_container->FindChild(IDParam(0x3C41C924U), 0);
	policyText->AddEventListener(
		0x100004,
		new MemberEventListener<PasswordInputGroup>(
			this, &PasswordInputGroup::OnPolicyLabelLayout, NULL),
		false);

	SyncFromModels();
}

void PasswordInputGroup::SyncFromModels(void)
{
	if (m_pwField != NULL && m_confirmField != NULL) {
		m_pwRow.SyncFromModel(m_pwField->m_errorCode);
		m_confirmRow.SyncFromModel(m_confirmField->m_errorCode);
	}
}

void PasswordInputGroup::BeginEditField(int32_t fieldIndex)
{
	if (fieldIndex == 0)
		m_pwRow.m_textBox.BeginEdit();
	else if (fieldIndex == 1)
		m_confirmRow.m_textBox.BeginEdit();
}

int32_t PasswordInputGroup::GetFieldEditState(int32_t fieldIndex) const
{
	if (fieldIndex != 0)
		return m_confirmRow.m_textBox.m_editState;
	return m_pwRow.m_textBox.m_editState;
}

void PasswordInputGroup::OnFieldCommit(const wchar_t *text, int32_t fieldIndex)
{
	if (m_onFieldCommit != NULL)
		m_onFieldCommit->Invoke(fieldIndex, text);
}

void PasswordInputGroup::OnPolicyInfoClicked(int32_t type, Handler *self,
					     Event *e, void *userData)
{
	(void)type;
	(void)e;
	(void)userData;
	Widget *btn = static_cast<Widget *>(self);
	SigninExtShowInfoScrollDialog(IDParam(0x46F7D8ABU), IDParam(0xA1D8DF84U),
				      btn->m_scene, NULL);
}

void PasswordInputGroup::OnPolicyLabelLayout(int32_t type, Handler *self,
					     Event *e, void *userData)
{
	(void)type;
	(void)e;
	(void)userData;
	Text *textW = static_cast<Text *>(self);
	TextObj *textObj = static_cast<TextObj *>(textW->GetDrawObj(0));
	textW->DeleteEventListener(0x100004, NULL);

	uint32_t lineCount = 0;
	textW->GetLineCount(lineCount);
	if (lineCount <= 1) {
		math::v2 bounds;
		textW->GetBounds(bounds);
		if (bounds.extract_x() > 446.0f) {
			math::v4 size = *textW->GetSize(0);
			math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
			math::v4 pos = textW->GetPos(0, NULL, zero);
			pos.set_x(size.extract_x() * 0.5f);
			textW->SetPos(pos, NULL, Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
			textW->SetAlign(2, textW->m_align_y, textW->m_align_z, NULL);
			textObj->SetLayoutAttribute(TextLayoutAttribute_AlignX, 2);
		}
	} else {
		math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
		math::v4 pos = textW->GetPos(0, NULL, zero);
		pos.set_x(0.0f);
		textW->SetPos(pos, NULL, Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		textW->SetAlign(0, textW->m_align_y, textW->m_align_z, NULL);
		textObj->SetLayoutAttribute(TextLayoutAttribute_AlignX, 1);
	}
}

/* ---- RichTextController ---- */

RichTextController::RichTextController()
	: m_httpClient(SigninExtGetJobQueue()),
	  m_richText(NULL),
	  m_onStatus(NULL),
	  m_url(),
	  m_lastHttpStatus(0),
	  m_active(true)
{
	m_httpClient.m_maxHttpConns = 1;
}

RichTextController::~RichTextController()
{
	if (m_onStatus != NULL)
		delete m_onStatus;
}

void RichTextController::Bind(RichText *richText, Callback1<int32_t> *onStatus)
{
	m_onStatus = onStatus;
	m_richText = richText;
	richText->m_no_image_size = true;

	richText->AddEventListener(
		RichText::CB_RICH_TEXT_REQ_IMG_SIZE,
		new MemberEventListener<RichTextController>(
			this, &RichTextController::OnReqImageSize, NULL),
		false);
	richText->AddEventListener(
		RichText::CB_RICH_TEXT_SCROLL_IN,
		new MemberEventListener<RichTextController>(
			this, &RichTextController::OnScrollInItem, NULL),
		false);
	richText->AddEventListener(
		RichText::CB_RICH_TEXT_FINISH_LAYOUT,
		new MemberEventListener<RichTextController>(
			this, &RichTextController::OnLayoutFinished, NULL),
		false);
}

void RichTextController::LoadUrl(const char *url)
{
	if (m_richText == NULL)
		return;

	if (m_httpClient.HasActiveJobs())
		m_httpClient.CancelAll();
	SigninExtImageClearAll();
	m_richText->Clear();

	if (url != NULL)
		m_url = url;
	m_lastHttpStatus = 0;

	m_httpClient.Enqueue(
		m_url.c_str(), 0,
		new MemberCallback2<RichTextController, HttpRequestJob *, int32_t>(
			this, &RichTextController::OnHttpDone));
}

void RichTextController::Clear(void)
{
	if (m_richText == NULL)
		return;

	if (m_httpClient.HasActiveJobs())
		m_httpClient.CancelAll();
	SigninExtImageClearAll();
	m_richText->Clear();
	m_url.clear();
}

void RichTextController::CancelAndNotify(void)
{
	m_active = false;
	if (m_httpClient.HasActiveJobs()) {
		m_httpClient.CancelAll();
		return;
	}
	SigninExtImageClearAll();
	if (m_onStatus != NULL)
		m_onStatus->Invoke(1);
}

void RichTextController::OnHttpDone(HttpRequestJob *job, int32_t result, void *userData)
{
	(void)userData;
	if (!m_active) {
		if (!m_httpClient.HasActiveJobs() && m_onStatus != NULL)
			m_onStatus->Invoke(1);
		return;
	}
	if (result == -1)
		return;

	if (result < 0) {
		m_lastHttpStatus = job->m_statusCode;
		if (m_onStatus != NULL)
			m_onStatus->Invoke(result);
		return;
	}

	SharedPtr<Buffer> buf = job->m_buffer;
	const char *html = static_cast<const char *>(buf->GetAddr());
	int32_t setRet = m_richText->SetCText(html, sce_paf_strlen(html));
	if (setRet < 0 && m_onStatus != NULL)
		m_onStatus->Invoke(SIGNIN_EXT_ERROR_NOMEM);
}

void RichTextController::OnReqImageSize(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)userData;
	RichText *rt = static_cast<RichText *>(self);
	int32_t imgIdx = e->m_data[0];

	string imgUrl;
	rt->GetImageUrl(imgIdx, imgUrl);

	math::v4 htmlSize;
	rt->GetHtmlImageSize(imgIdx, htmlSize);

	ImageExtentRequest extent = { 0, 0 };
	ImageLoadCallback *cb = NULL;
	int32_t w = (int32_t)htmlSize.extract_x();
	int32_t h = (int32_t)htmlSize.extract_y();
	if (w > 0 && h > 0) {
		rt->SetImageSize(imgIdx, htmlSize);
		extent.width = w;
		extent.height = h;
	} else {
		cb = new MemberCallback3<RichTextController, const char *, int32_t,
					 intrusive_ptr<Surface>, int32_t>(
			this, &RichTextController::OnImageSizeLoaded, imgIdx);
	}
	SigninExtImageRequest(imgUrl.c_str(), cb, &extent);
}

void RichTextController::OnScrollInItem(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)userData;
	RichText *rt = static_cast<RichText *>(self);
	int32_t itemIdx = e->m_data[0];

	if (rt->GetItemType(itemIdx) == RichText::ITEM_TYPE_IMAGE) {
		string itemUrl;
		rt->GetItemUrl(itemIdx, itemUrl);
		ImageLoadCallback *cb = new MemberCallback3<
			RichTextController, const char *, int32_t,
			intrusive_ptr<Surface>, int32_t>(
			this, &RichTextController::OnImageTexLoaded, itemIdx);
		ImageExtentRequest extent = { 0, 0 };
		SigninExtImageRequest(itemUrl.c_str(), cb, &extent);
	}
}

void RichTextController::OnImageSizeLoaded(const char *url, int32_t result,
					   intrusive_ptr<Surface> surf, int32_t imgIndex)
{
	(void)url;
	if (result >= 0 && surf.get() != NULL) {
		math::v4 sz((float)surf->GetWidth(), (float)surf->GetHeight(), 0.0f, 0.0f);
		m_richText->SetImageSize(imgIndex, sz);
	} else {
		math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
		m_richText->SetImageSize(imgIndex, zero);
	}
}

void RichTextController::OnImageTexLoaded(const char *url, int32_t result,
					  intrusive_ptr<Surface> surf, int32_t itemIndex)
{
	(void)url;
	if (result >= 0) {
		m_richText->SetTexture(surf, itemIndex, 0);
	} else {
		intrusive_ptr<Surface> fallback = SigninExtGetTexture(IDParam(0x5F98D96FU));
		m_richText->SetTexture(fallback, itemIndex, 0);
	}
}

void RichTextController::OnLayoutFinished(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	if (m_onStatus != NULL)
		m_onStatus->Invoke(0);
}

/* ---- IME edit helpers ---- */

void SigninExtImeInit(void)
{
	s_suspendedEditTextBox = NULL;
}

void SigninExtImeEndEdit(void)
{
	s_suspendedEditTextBox = NULL;
	TextBoxEditListener *listener = wutil::GetEditListener();
	if (listener != NULL) {
		TextBox *tb = listener->GetTextBox();
		if (tb != NULL)
			tb->EndEdit();
	}
}

void SigninExtImeSuspendEdit(void)
{
	TextBoxEditListener *listener = wutil::GetEditListener();
	if (listener != NULL) {
		s_suspendedEditTextBox = listener->GetTextBox();
		if (s_suspendedEditTextBox != NULL)
			s_suspendedEditTextBox->EndEdit();
	}
}

void SigninExtImeResumeEdit(void)
{
	if (s_suspendedEditTextBox != NULL) {
		s_suspendedEditTextBox->BeginEdit();
		s_suspendedEditTextBox = NULL;
	}
}

/* ---- TextBoxController ---- */

TextBoxController::TextBoxController()
	: m_textBox(NULL), m_field(NULL), m_onCommit(NULL), m_editState(1), m_suspended(false)
{
}

TextBoxController::~TextBoxController()
{
	if (m_onCommit != NULL)
		delete m_onCommit;
}

void TextBoxController::ConfigureIme(const TextInputField *field)
{
	TextBox::ImeParam ime;
	switch (field->m_fieldType) {
	case 1:
		ime.type = 2;
		break;
	case 2:
		ime.type = 2;
		ime.filter = reinterpret_cast<int32_t (*)(wchar_t *, uint32_t *, const wchar_t *, uint32_t)>(
			ScePafWidget_48D5B0D2);
		break;
	case 3:
	case 5:
	case 7:
		ime.type = 1;
		break;
	case 4:
		ime.type = 1;
		ime.filter = reinterpret_cast<int32_t (*)(wchar_t *, uint32_t *, const wchar_t *, uint32_t)>(
			ScePafWidget_48D5B0D2);
		break;
	case 6:
		ime.type = 4;
		ime.filter = reinterpret_cast<int32_t (*)(wchar_t *, uint32_t *, const wchar_t *, uint32_t)>(
			ScePafWidget_48D5B0D2);
		break;
	default:
		break;
	}
	m_textBox->SetImeParam(ime);
}

void TextBoxController::Bind(TextBox *textBox, TextInputField *field,
			     Callback1<const wchar_t *> *onCommit)
{
	m_textBox = textBox;
	m_field = field;
	m_onCommit = onCommit;

	textBox->SetMaxLength(field->m_maxLen);
	if (field->m_fieldType == 7)
		textBox->SetPasswordMode(true);

	wstring initial((field->m_buf != NULL) ? field->m_buf : L"");
	textBox->SetString(initial);
	ConfigureIme(field);

	textBox->AddEventListener(
		TextBox::CB_TEXT_BOX_EDIT_BEGIN,
		new MemberEventListener<TextBoxController>(
			this, &TextBoxController::OnEditBegin, NULL),
		false);
	textBox->AddEventListener(
		TextBox::CB_TEXT_BOX_ENTER_PRESSED,
		new MemberEventListener<TextBoxController>(
			this, &TextBoxController::OnEnterPressed, NULL),
		false);
	textBox->AddEventListener(
		TextBox::CB_TEXT_BOX_EDIT_END,
		new MemberEventListener<TextBoxController>(
			this, &TextBoxController::OnEditEnd, NULL),
		false);
}

void TextBoxController::SetPlaceholder(const IDParam &textId)
{
	if (m_textBox == NULL)
		return;
	const wchar_t *s = SigninExtGetString(textId);
	wstring ws((s != NULL) ? s : L"");
	ScePafWidget_3CA096AE(m_textBox, ws);
}

void TextBoxController::BeginEdit(void)
{
	if (m_textBox != NULL && m_field != NULL)
		m_textBox->BeginEdit();
}

void TextBoxController::SyncFromModel(void)
{
	if (m_textBox != NULL && m_field != NULL) {
		wstring ws((m_field->m_buf != NULL) ? m_field->m_buf : L"");
		m_textBox->SetString(ws);
	}
}

void TextBoxController::OnEditBegin(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)e;
	(void)userData;
	if (s_popupDialog != NULL || s_errorDialog != NULL) {
		m_editState = 2;
		m_textBox->EndEdit();
		return;
	}

	m_editState = 1;
	if (!m_suspended)
		SigninExtPlayCommonSE(0);
	m_suspended = false;

	Widget *tb = static_cast<Widget *>(self);
	Widget *footerRoot = tb->m_scene->FindChild(IDParam(0xE8121D6BU), 0);
	if (footerRoot != NULL) {
		Timer *t = new Timer(0.0f, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
		footerRoot->Hide(0xff, t, NULL, 0.0f);
	}
}

void TextBoxController::OnEnterPressed(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)self;
	(void)e;
	(void)userData;
	m_editState = 0;
	wstring str;
	m_textBox->GetString(str);
	if (m_onCommit != NULL)
		m_onCommit->Invoke(str.c_str());
	m_textBox->EndEdit();
}

void TextBoxController::OnEditEnd(int32_t type, Handler *self, Event *e, void *userData)
{
	(void)type;
	(void)e;
	(void)userData;
	Widget *tb = static_cast<Widget *>(self);
	Widget *footerRoot = tb->m_scene->FindChild(IDParam(0xE8121D6BU), 0);
	if (footerRoot != NULL) {
		Timer *t = new Timer(0.0f, Timer::FUNC_LINEAR, Timer::REPEAT_NONE, -1, NULL);
		footerRoot->Show(0xff, t, NULL, 0.0f);
	}

	if (s_suspendedEditTextBox != NULL) {
		m_suspended = true;
	} else if (m_editState == 1) {
		wstring str;
		m_textBox->GetString(str);
		if (m_onCommit != NULL)
			m_onCommit->Invoke(str.c_str());
	}
}

/* ---- AutoShrinkTextListener & HeaderBar ---- */

class AutoShrinkTextListener : public EventListener {
public:
	AutoShrinkTextListener(float targetWidth, float posDeltaX)
		: m_targetWidth(targetWidth), m_posDeltaX(posDeltaX)
	{
	}

	virtual ~AutoShrinkTextListener() {}

	virtual int32_t Do(int32_t type, Handler *self, Event *e)
	{
		(void)type;
		(void)e;
		Text *textW = static_cast<Text *>(self);
		float fontSize = 0.0f;
		textW->GetStyleAttribute(TextStyleAttribute_Point, 0, fontSize);
		uint32_t lineCount = 0;
		if (fontSize == 24.0f || (textW->GetLineCount(lineCount), lineCount <= 1)) {
			textW->DeleteEventListener(0x100004, this);
			textW->Show(0xff, NULL, NULL, 0.0f);
			return 0;
		}

		scePafWidgetSetFontSize(textW, 24.0f, TextStyleAttribute_Point, 0, 0);
		if (m_targetWidth > 0.0f) {
			math::v4 size = *textW->GetSize(0);
			size.set_x(m_targetWidth);
			textW->SetSize(size, NULL, Widget::ANIM_SIZE, 0, 0.0f, NULL, NULL);
		}
		if (m_posDeltaX > 0.0f) {
			math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
			math::v4 pos = textW->GetPos(0, NULL, zero);
			pos.set_x(pos.extract_x() + m_posDeltaX);
			textW->SetPos(pos, NULL, Widget::ANIM_MOVE, 0, 0.0f, NULL, NULL);
		}
		return 0;
	}

	float m_targetWidth;
	float m_posDeltaX;
};

void SigninExtSetAutoShrinkText(Widget *widget, const wchar_t *text, float extraWidth)
{
	widget->Hide(0xff, NULL, NULL, 0.0f);
	float targetWidth = widget->GetSize(0)->extract_x();
	float deltaX = 0.0f;
	if (extraWidth > 0.0f) {
		targetWidth += extraWidth;
		deltaX = extraWidth * 0.5f;
	}

	widget->AddEventListener(0x100004, new AutoShrinkTextListener(targetWidth, deltaX), false);
	widget->SetString((text != NULL) ? wstring(text) : wstring());
}

HeaderBar::HeaderBar() : m_titleWidget(NULL), m_titleId(0U)
{
}

HeaderBar::~HeaderBar()
{
}

void HeaderBar::Init(Widget *parent, const IDParam &titleId)
{
	m_titleId = titleId;
	Plugin::TemplateOpenParam tmplParam;
	if (SigninExtTemplateOpen(parent, IDParam(0x987AD459U), tmplParam) < 0)
		return;

	m_titleWidget = parent->FindChild(IDParam(0x59907AA0U), 0);
	if (m_titleWidget != NULL) {
		scePafWidgetSetFontSize(m_titleWidget, 30.0f, TextStyleAttribute_Point, 0, 0);
		SigninExtSetAutoShrinkText(m_titleWidget, SigninExtGetString(m_titleId));
	}
}

/* ---- Environment & Plugin helpers ---- */

void SigninExtSetLanguageId(int32_t locale)
{
	Plugin *plugin = SigninExtGetPlugin();
	if (plugin->m_locale != locale)
		plugin->SetLocale((Locale)locale);

	Framework *fw = Framework::Instance();
	if ((int32_t)fw->GetLocale() != locale) {
		fw->SetLocale((Locale)locale);
		fw->SetCommonResourceLocale((Locale)locale);
	}
}

void SigninExtEnvInit(void)
{
	s_savedSystemLocale = (int32_t)Framework::Instance()->GetLocale();
	SigninExtImeInit();
	SigninExtDobDialogInit();
	SigninExtListDialogInit();
	SigninExtPopupInit();
	SigninExtErrorInit();

	int32_t locale = (int32_t)Framework::Instance()->GetLocale();
	if (locale >= 20)
		locale = 1;
	SigninExtSetLanguageId(locale);
}

void SigninExtEnvTerm(void)
{
	SigninExtErrorTerm();
	SigninExtPopupTerm();
	SigninExtDobDialogTerm();
	SigninExtListDialogTerm();
	SigninExtImeEndEdit();

	Framework *fw = Framework::Instance();
	fw->SetLocale((Locale)s_savedSystemLocale);
	fw->SetCommonResourceLocale((Locale)s_savedSystemLocale);
}

const wchar_t *SigninExtGetString(const IDParam &id)
{
	Plugin *plugin = SigninExtGetPlugin();
	return plugin->GetString(id);
}

intrusive_ptr<Surface> SigninExtGetTexture(const IDParam &id)
{
	Plugin *plugin = SigninExtGetPlugin();
	return plugin->GetTexture(id);
}

Scene *SigninExtPageOpenWithParam(const IDParam &id, Plugin::PageOpenParam &param)
{
	Plugin *plugin = SigninExtGetPlugin();
	Plugin::PageCloseParam closeParam;
	plugin->PageClose(id, closeParam);

	if (param.overwrite_draw_priority >= 0)
		param.overwrite_draw_priority = plugin->m_draw_priority - param.overwrite_draw_priority;
	return plugin->PageOpen(id, param);
}

Scene *SigninExtPageOpen(const IDParam &id)
{
	Plugin::PageOpenParam openParam;
	return SigninExtPageOpenWithParam(id, openParam);
}

void SigninExtPageClose(const IDParam &id)
{
	Plugin *plugin = SigninExtGetPlugin();
	Plugin::PageCloseParam closeParam;
	plugin->PageClose(id, closeParam);
}

void SigninExtPageCloseAll(void)
{
	Plugin *plugin = SigninExtGetPlugin();
	plugin->PageCloseAll();
}

int32_t SigninExtTemplateOpen(Widget *parent, const IDParam &id,
			      const Plugin::TemplateOpenParam &param)
{
	Plugin *plugin = SigninExtGetPlugin();
	return plugin->TemplateOpen(parent, id, param);
}

Widget *SigninExtCreateWidget(Widget *parent, const char *type,
			      const IDParam &id, const IDParam &style)
{
	Plugin *plugin = SigninExtGetPlugin();
	return plugin->CreateWidget(parent, type, id, style);
}

void SigninExtCloseAllDialogs(void)
{
	SigninExtDobDialogCloseImmediate();
	SigninExtListDialogCloseImmediate();
	SigninExtImeEndEdit();
	SigninExtPopupCloseImmediate();
	SigninExtErrorCloseImmediate();
}

/* ---- CountryTable & CountryModel ---- */

void SigninExtCountryTableClear(void)
{
	s_countryList.clear();
}

int32_t SigninExtCountryTableParse(const void *buf, SceSize size)
{
	s_countryList.clear();

	SceNpCommerce2CountryList list;
	int32_t ret = SceNpCommerce2_86405E79(&list, buf, size);
	if (ret < 0)
		return ret;

	if (s_countryList.capacity() < list.count)
		s_countryList.reserve(list.count);

	for (uint32_t i = 0; i < list.count; ++i) {
		SceNpCommerce2CountryInfo info;
		ret = SceNpCommerce2_4BE534B6(&list, i, &info);
		if (ret < 0)
			return ret;

		CountryEntry entry;
		entry.code = 0;
		if (info.countryCode != NULL) {
			entry.code = ((uint32_t)(uint8_t)info.countryCode[0] | 0x20U) |
				     (((uint32_t)(uint8_t)info.countryCode[1] | 0x20U) << 8);
		}

		if (info.dateFormat == 3)
			entry.dateFormat = 0;
		else if (info.dateFormat == 2)
			entry.dateFormat = 2;
		else
			entry.dateFormat = 1;

		entry.flag = (info.flag != 0);

		for (uint32_t j = 0; j < info.langCount; ++j) {
			const char *langStr = NULL;
			int32_t lret = SceNpCommerce2_BDECB406(&info, j, &langStr);
			if (lret < 0)
				return lret;

			int32_t langId = SigninExtFindLanguageIndex(langStr);
			langId = SigninExtAdjustLanguageForCountry(langId, entry.code);
			entry.languages.push_back(langId);
		}

		if (!entry.languages.empty())
			s_countryList.push_back(entry);
	}
	return 0;
}

const CountryEntry *SigninExtCountryTableGet(int32_t index)
{
	if (index >= 0 && index < (int32_t)s_countryList.size())
		return &s_countryList[index];
	return NULL;
}

void SigninExtCountryTableAddFallback(const char *countryCode, const char *langCode)
{
	CountryEntry entry;
	entry.code = 0;
	if (countryCode != NULL) {
		entry.code = ((uint32_t)(uint8_t)countryCode[0] | 0x20U) |
			     (((uint32_t)(uint8_t)countryCode[1] | 0x20U) << 8);
	}

	int32_t langId = SigninExtFindLanguageIndex(langCode);
	langId = SigninExtAdjustLanguageForCountry(langId, entry.code);
	entry.languages.push_back(langId);
	entry.dateFormat = 1;
	entry.flag = false;

	s_countryList.push_back(entry);
}

int32_t SigninExtCountryTableCount(void)
{
	return (int32_t)s_countryList.size();
}

int32_t CountryModel::GetCount(void)
{
	return (int32_t)s_countryList.size();
}

const wchar_t *CountryModel::GetLabel(int32_t index)
{
	(void)index;
	return NULL;
}

const char *CountryModel::GetCountryCode(int32_t index)
{
	if (index < 0 || index >= (int32_t)s_countryList.size())
		return NULL;
	return reinterpret_cast<const char *>(&s_countryList[index].code);
}

int32_t CountryModel::FindByCode(const char *code)
{
	if (code == NULL || sce_paf_strlen(code) < 2)
		return -1;

	uint32_t target = ((uint32_t)(uint8_t)code[0] | 0x20U) |
			  (((uint32_t)(uint8_t)code[1] | 0x20U) << 8);
	int32_t count = GetCount();
	for (int32_t i = 0; i < count; ++i) {
		if (s_countryList[i].code == target)
			return i;
	}
	return -1;
}
