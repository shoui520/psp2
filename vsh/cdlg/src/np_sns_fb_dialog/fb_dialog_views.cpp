/*
 * SceNpSnsFacebookDialogPlugin: all 13 FbViewBase subclasses and their UI/CommonGuiDialog handlers.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/new>
#include <std/string>
#include <std/string.h>
#include <std/vector>
#include <math/ngp/v4.h>
#include <common/encoded_string.h>
#include <common/string_util.h>
#include <graphics/drawobj/plane_obj.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include <widget/w_button_base.h>
#include <widget/w_text.h>
#include <widget/w_text_box.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../msg_dialog/msg_dialog_util.h"
#include "fb_dialog_plugin.h"
#include "fb_dialog_job.h"
#include "fb_dialog_view_mgr.h"
#include "fb_dialog_controller.h"

using namespace paf;
using namespace paf::ui;

static const char s_empty[] = "";

/* Helper to read UTF-16 resource string by hash ID */
static const wchar_t *GetPluginText(Plugin *plugin, uint32_t hash)
{
	IDParam id(s_empty, hash);

	return plugin != NULL ? plugin->GetString(id) : NULL;
}

/* Helper to set a Text widget's string by hash ID */
static void SetTextById(Widget *root, uint32_t hash, const wstring &text)
{
	IDParam id(s_empty, hash);
	Text *w = (Text *)root->FindChild(id, 0);

	if (w != NULL)
		w->SetString(text);
}

/* =========================================================================
 * 1. FbBgView (0x9572CA6E)
 * ========================================================================= */
class FbBgView : public FbViewBase {
public:
	FbBgView(FbEventBus *bus, Plugin *plugin)
		: FbViewBase(bus, plugin), m_hasColor(false), m_color(0.0f, 0.0f, 0.0f, 0.0f)
	{
		m_id = IDParam(s_empty, FB_VIEW_BG_ID);
	}

	virtual ~FbBgView() {}

	virtual int Open()
	{
		if (m_open != 0)
			return 0;
		Plugin::PageOpenParam opt;
		Widget *page = m_plugin->PageOpen(m_id, opt);

		if (page == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		m_open = 1;
		if (m_hasColor) {
			IDParam bgId(s_empty, 0x9FA340BD);
			Widget *bg = page->FindChild(bgId, 0);

			if (bg != NULL) {
				graph::DrawObj *obj = bg->GetDrawObj(0);

				if (obj != NULL && obj->IsInherit(graph::PlaneObj::DrawObjType())) {
					((graph::PlaneObj *)obj)->SetColor(m_color);
					if (m_color.extract_w() > 0.95f)
						ScePafWidget_0253DC9C(bg, true);
				}
			}
		}
		return 0;
	}

	virtual int Resume()
	{
		return 0;
	}

	virtual int Close()
	{
		if (m_open != 0) {
			Plugin::PageCloseParam opt;

			m_plugin->PageClose(m_id, opt);
		}
		m_open = 0;
		return 0;
	}

	virtual int SetBgColor(const math::v4 &color)
	{
		m_hasColor = true;
		m_color = color;
		return 0;
	}

	virtual int OnBgAction()
	{
		return (m_hasColor && m_color.extract_w() < 0.01f) ? 1 : 0;
	}

	bool m_hasColor;
	math::v4 m_color;
};

/* =========================================================================
 * 2. FbInputBaseView + FbLoginView + FbInputShortView + FbInputLongView
 * ========================================================================= */
class FbInputBaseView : public FbViewBase {
public:
	__attribute__((noinline)) FbInputBaseView(FbEventBus *bus, Plugin *plugin)
		: FbViewBase(bus, plugin), m_altPageId(s_empty, 0x7BC96F04), m_useAltPage(0),
		  m_emailBoxId(s_empty, 0x687895D7), m_passBoxId(s_empty, 0x9559D299),
		  m_okBtnId(s_empty, 0xBA8C98A3), m_cancelBtnId(s_empty, 0x4D84F6D1),
		  m_accountParam(NULL), m_passwordParam(NULL)
	{
		sce_paf_memset(&m_accountBuf, 0, sizeof(m_accountBuf));
		sce_paf_memset(m_passwordBuf, 0, sizeof(m_passwordBuf));
	}

	virtual __attribute__((noinline)) ~FbInputBaseView() {}

	const IDParam &ActivePageId() const
	{
		return m_useAltPage != 0 ? m_altPageId : m_id;
	}

	virtual int Open()
	{
		if (m_open != 0)
			return 0;
		m_useAltPage = (m_accountParam != NULL && m_accountParam->reserved != 0) ? 1 : 0;
		Plugin::PageOpenParam opt;
		IDParam pageId = ActivePageId();
		Widget *page = m_plugin->PageOpen(pageId, opt);

		if (page == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		m_plugin->PageActivate(pageId);
		int ret = SetupButtons();

		if (ret < 0)
			return ret;
		ret = SetupTextBoxes();
		if (ret < 0)
			return ret;
		m_open = 1;
		return Resume();
	}

	virtual int Resume()
	{
		if (m_open == 0)
			return FB_DIALOG_ERROR_INVALID_STATE;
		m_plugin->PageActivate(m_id);
		if (m_accountParam != NULL) {
			int ret = PopulateEmail();

			if (ret < 0)
				return ret;
		}
		int ret = ClearPassword();

		UpdateOkButton();
		return ret;
	}

	virtual int Close()
	{
		if (m_open != 0) {
			IDParam pageId = ActivePageId();
			Plugin::PageCloseParam opt;

			m_plugin->PageClose(pageId, opt);
		}
		m_open = 0;
		m_useAltPage = 0;
		return 0;
	}

	virtual void SetParamPtr3(const void *ptr)
	{
		m_accountParam = (const FbAccountId *)ptr;
	}

	virtual void SetParamPtr4(const void *ptr)
	{
		m_passwordParam = (const char *)ptr;
	}

	virtual void OnCancelButton() {}
	virtual void OnOkButton() = 0;

	virtual void OnEmailEditEnd()
	{
		IDParam pageId = ActivePageId();
		Widget *page = m_plugin->PageRoot(pageId);

		if (page != NULL && page->FindChild(m_emailBoxId, 0) != NULL)
			UpdateOkButton();
	}

	virtual void OnEmailEnter()
	{
		IDParam pageId = ActivePageId();
		Widget *page = m_plugin->PageRoot(pageId);

		if (page == NULL)
			return;
		TextBox *emailBox = (TextBox *)page->FindChild(m_emailBoxId, 0);

		if (emailBox == NULL)
			return;
		TextBox *passBox = (TextBox *)page->FindChild(m_passBoxId, 0);

		if (passBox != NULL) {
			passBox->SetCaretIndex(0);
			passBox->BeginEdit();
			UpdateOkButton();
		} else {
			emailBox->EndEdit();
		}
	}

	virtual void OnPassEditEnd()
	{
		UpdateOkButton();
	}

	virtual void OnPassEnter()
	{
		IDParam pageId = ActivePageId();
		Widget *page = m_plugin->PageRoot(pageId);

		if (page == NULL)
			return;
		TextBox *passBox = (TextBox *)page->FindChild(m_passBoxId, 0);

		if (passBox != NULL)
			passBox->EndEdit();
	}

	virtual int SetupButtons()
	{
		IDParam pageId = ActivePageId();
		Widget *page = m_plugin->PageRoot(pageId);

		if (page == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		Widget *cancelBtn = page->FindChild(m_cancelBtnId, 0);

		if (cancelBtn == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		cancelBtn->AddEventListener(0x10000008,
					    new EventCBListener(CancelBtnCb, this), false);
		cancelBtn->m_event_enable |= 0x10;
		ScePafWidget_21125870(cancelBtn, 32);

		Widget *okBtn = page->FindChild(m_okBtnId, 0);

		if (okBtn == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		okBtn->AddEventListener(0x10000008, new EventCBListener(OkBtnCb, this), false);
		return 0;
	}

	virtual int SetupTextBoxes()
	{
		IDParam pageId = ActivePageId();
		Widget *page = m_plugin->PageRoot(pageId);

		if (page == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		TextBox *emailBox = (TextBox *)page->FindChild(m_emailBoxId, 0);

		if (emailBox == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;

		TextBox::ImeParam emailIme;

		emailIme.type = 4;
		emailIme.filter = (int32_t (*)(wchar_t *, uint32_t *, const wchar_t *,
					       uint32_t))ScePafWidget_48D5B0D2;
		emailBox->SetImeParam(emailIme);
		emailBox->AddEventListener(TextBox::CB_TEXT_BOX_EDIT_END,
					   new EventCBListener(EmailEditEndCb, this), false);
		emailBox->AddEventListener(TextBox::CB_TEXT_BOX_ENTER_PRESSED,
					   new EventCBListener(EmailEnterCb, this), false);

		TextBox *passBox = (TextBox *)page->FindChild(m_passBoxId, 0);

		if (passBox == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;

		TextBox::ImeParam passIme;

		passIme.type = 1;
		passBox->SetImeParam(passIme);
		passBox->AddEventListener(TextBox::CB_TEXT_BOX_EDIT_END,
					  new EventCBListener(PassEditEndCb, this), false);
		passBox->AddEventListener(TextBox::CB_TEXT_BOX_ENTER_PRESSED,
					  new EventCBListener(PassEnterCb, this), false);
		return 0;
	}

	virtual void UpdateOkButton()
	{
		wstring emailStr;
		wstring passStr;
		IDParam pageId = ActivePageId();
		Widget *page = m_plugin->PageRoot(pageId);

		if (page == NULL)
			return;
		TextBox *emailBox = (TextBox *)page->FindChild(m_emailBoxId, 0);

		if (emailBox == NULL)
			return;
		emailBox->GetString(emailStr);
		TextBox *passBox = (TextBox *)page->FindChild(m_passBoxId, 0);

		if (passBox == NULL)
			return;
		passBox->GetString(passStr);

		bool enabled = (emailStr.size() != 0 && passStr.size() != 0);
		ButtonBase *okBtn = (ButtonBase *)page->FindChild(m_okBtnId, 0);

		if (okBtn == NULL)
			return;
		okBtn->Show(0xff, NULL, NULL, 0.0f);
		if (enabled)
			okBtn->Enable(false);
		else
			okBtn->Disable(false);
	}

	__attribute__((noinline)) int ClearPassword()
	{
		wstring empty;
		IDParam pageId = ActivePageId();
		Widget *page = m_plugin->PageRoot(pageId);

		if (page == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		TextBox *passBox = (TextBox *)page->FindChild(m_passBoxId, 0);

		if (passBox == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		passBox->SetString(empty);
		return 0;
	}

	__attribute__((noinline)) int PopulateEmail()
	{
		if (m_accountParam == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		IDParam pageId = ActivePageId();
		Widget *page = m_plugin->PageRoot(pageId);

		if (page == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		string utf8(m_accountParam->email);
		wstring utf16;

		common::Utf8ToUtf16(utf8, &utf16, 0);
		TextBox *emailBox = (TextBox *)page->FindChild(m_emailBoxId, 0);

		if (emailBox == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		emailBox->SetString(utf16);
		return 0;
	}

	void ReadInputAndSubmit(bool isLogin)
	{
		wstring emailStr;
		wstring passStr;
		Widget *page = isLogin ? m_plugin->PageRoot(m_id)
				       : m_plugin->PageRoot(ActivePageId());

		if (page != NULL) {
			TextBox *emailBox = (TextBox *)page->FindChild(m_emailBoxId, 0);

			if (emailBox != NULL) {
				emailBox->GetString(emailStr);
				TextBox *passBox = (TextBox *)page->FindChild(m_passBoxId, 0);

				if (passBox != NULL) {
					passBox->GetString(passStr);
					common::EncodedString enc;

					ScePafMisc_DCBA2BD4(&enc, emailStr.c_str(),
							    common::EncodedString::Encoding_Utf16);
					sce_paf_strncpy(m_accountBuf.email,
							ScePafMisc_A7C6AF5C(&enc), 0x13F);
					ScePafMisc_B45A7521(&enc);

					ScePafMisc_DCBA2BD4(&enc, passStr.c_str(),
							    common::EncodedString::Encoding_Utf16);
					sce_paf_strncpy(m_passwordBuf, ScePafMisc_A7C6AF5C(&enc),
							0xFF);
					ScePafMisc_B45A7521(&enc);

					if (isLogin)
						m_owner->OnLoginSubmit(&m_accountBuf,
								       m_passwordBuf);
					else
						m_owner->OnInputSubmit(&m_accountBuf,
								       m_passwordBuf);
					return;
				}
			}
		}
		if (isLogin)
			m_owner->OnLoginSubmit(NULL, NULL);
		else
			m_owner->OnInputSubmit(NULL, NULL);
	}

	static void EmailEditEndCb(int32_t ev, Handler *self, Event *e, void *ud)
	{
		(void)ev;
		(void)self;
		(void)e;
		((FbInputBaseView *)ud)->OnEmailEditEnd();
	}

	static void EmailEnterCb(int32_t ev, Handler *self, Event *e, void *ud)
	{
		(void)ev;
		(void)self;
		(void)e;
		((FbInputBaseView *)ud)->OnEmailEnter();
	}

	static void PassEditEndCb(int32_t ev, Handler *self, Event *e, void *ud)
	{
		(void)ev;
		(void)self;
		(void)e;
		((FbInputBaseView *)ud)->OnPassEditEnd();
	}

	static void PassEnterCb(int32_t ev, Handler *self, Event *e, void *ud)
	{
		(void)ev;
		(void)self;
		(void)e;
		((FbInputBaseView *)ud)->OnPassEnter();
	}

	static void CancelBtnCb(int32_t ev, Handler *self, Event *e, void *ud)
	{
		(void)ev;
		(void)self;
		(void)e;
		((FbInputBaseView *)ud)->OnCancelButton();
	}

	static void OkBtnCb(int32_t ev, Handler *self, Event *e, void *ud)
	{
		(void)ev;
		(void)self;
		(void)e;
		((FbInputBaseView *)ud)->OnOkButton();
	}

	IDParam m_altPageId;
	int m_useAltPage;
	IDParam m_emailBoxId;
	IDParam m_passBoxId;
	IDParam m_okBtnId;
	IDParam m_cancelBtnId;
	FbAccountId m_accountBuf;
	char m_passwordBuf[256];
	const FbAccountId *m_accountParam;
	const char *m_passwordParam;
};

class FbLoginView : public FbInputBaseView {
public:
	FbLoginView(FbEventBus *bus, Plugin *plugin) : FbInputBaseView(bus, plugin)
	{
		m_id = IDParam(s_empty, FB_VIEW_LOGIN_ID);
		m_emailBoxId = IDParam(s_empty, 0x687895D7);
		m_passBoxId = IDParam(s_empty, 0x9559D299);
		m_okBtnId = IDParam(s_empty, 0x76ABE561);
		m_cancelBtnId = IDParam(s_empty, 0x51FA74AC);
		sce_paf_memset(m_titleName, 0, sizeof(m_titleName));
	}

	virtual ~FbLoginView() {}

	virtual int Open()
	{
		if (m_open != 0)
			return 0;
		m_useAltPage = 0;
		Plugin::PageOpenParam opt;
		Widget *page = m_plugin->PageOpen(m_id, opt);

		if (page == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		m_plugin->PageActivate(m_id);

		string titleUtf8(m_titleName);

		FbFormatSingleLine(titleUtf8, true);
		wstring titleUtf16;

		common::Utf8ToUtf16(titleUtf8, &titleUtf16, 0);
		wstring msg = MsgDialogFormat(GetPluginText(m_plugin, 0xBC9926BE),
					      titleUtf16.c_str());

		SetTextById(page, 0xCEBB36B6, msg);

		int ret = SetupButtons();

		if (ret < 0)
			return ret;
		ret = SetupTextBoxes();
		if (ret < 0)
			return ret;
		m_open = 1;
		return Resume();
	}

	virtual int Resume()
	{
		if (m_open == 0)
			return FB_DIALOG_ERROR_INVALID_STATE;
		m_plugin->PageActivate(m_id);
		if (m_accountParam != NULL) {
			int ret = PopulateEmail();

			if (ret < 0)
				return ret;
		}
		int ret = ClearPassword();

		UpdateOkButton();
		return ret;
	}

	virtual void SetParamPtr2(const void *ptr)
	{
		if (ptr != NULL)
			sce_paf_strncpy(m_titleName, (const char *)ptr, sizeof(m_titleName) - 1);
	}

	virtual void OnCancelButton()
	{
		m_owner->OnLoginCancel();
	}

	virtual void OnOkButton()
	{
		ReadInputAndSubmit(true);
	}

	char m_titleName[128];
};

class FbInputShortView : public FbInputBaseView {
public:
	FbInputShortView(FbEventBus *bus, Plugin *plugin) : FbInputBaseView(bus, plugin)
	{
		m_id = IDParam(s_empty, FB_VIEW_INPUT_SHORT_ID);
		m_emailBoxId = IDParam(s_empty, 0x687895D7);
		m_passBoxId = IDParam(s_empty, 0x9559D299);
		m_okBtnId = IDParam(s_empty, 0xBA8C98A3);
		m_cancelBtnId = IDParam(s_empty, 0x4D84F6D1);
	}

	virtual ~FbInputShortView() {}

	virtual void OnCancelButton()
	{
		m_owner->OnInputCancel();
	}

	virtual void OnOkButton()
	{
		ReadInputAndSubmit(false);
	}
};

class FbInputLongView : public FbInputBaseView {
public:
	FbInputLongView(FbEventBus *bus, Plugin *plugin) : FbInputBaseView(bus, plugin)
	{
		m_id = IDParam(s_empty, FB_VIEW_INPUT_LONG_ID);
		m_emailBoxId = IDParam(s_empty, 0x687895D7);
		m_passBoxId = IDParam(s_empty, 0x9559D299);
		m_okBtnId = IDParam(s_empty, 0xBA8C98A3);
		m_cancelBtnId = IDParam(s_empty, 0x4D84F6D1);
	}

	virtual ~FbInputLongView() {}

	virtual void OnCancelButton()
	{
		m_owner->OnInputCancel();
	}

	virtual void OnOkButton()
	{
		ReadInputAndSubmit(false);
	}
};

/* =========================================================================
 * 3. FbPermissionView (0xA07067CB)
 * ========================================================================= */
class FbPermissionView : public FbViewBase {
public:
	FbPermissionView(FbEventBus *bus, Plugin *plugin)
		: FbViewBase(bus, plugin), m_permList(NULL)
	{
		m_id = IDParam(s_empty, FB_VIEW_PERMISSION_ID);
		sce_paf_memset(m_titleName, 0, sizeof(m_titleName));
	}

	virtual ~FbPermissionView() {}

	virtual int Open()
	{
		if (m_open != 0)
			return 0;
		if (m_permList == NULL || m_permList->size() == 0)
			return FB_DIALOG_ERROR_INVALID_STATE;

		Plugin::PageOpenParam opt;
		Widget *page = m_plugin->PageOpen(m_id, opt);

		if (page == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;

		string titleUtf8(m_titleName);

		FbFormatSingleLine(titleUtf8, true);
		wstring titleUtf16;

		common::Utf8ToUtf16(titleUtf8, &titleUtf16, 0);
		wstring header = MsgDialogFormat(GetPluginText(m_plugin, 0x1979D4E8),
						 titleUtf16.c_str());

		SetTextById(page, 0x8B480F68, header);

		IDParam listId(s_empty, 0x45A7CCCE);
		Widget *list = page->FindChild(listId, 0);

		if (list == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;

		for (size_t i = 0; i < m_permList->size(); ++i) {
			IDParam tmplId(s_empty, 0x784E9B09);
			Plugin::TemplateOpenParam tmplOpt;

			m_plugin->TemplateOpen(list, tmplId, tmplOpt);
		}

		for (size_t i = 0; i < m_permList->size(); ++i) {
			Widget *item = list->GetChild(i);

			if (item == NULL)
				return FB_DIALOG_ERROR_INVALID_STATE;

			string nameUtf8((*m_permList)[i].name);
			wstring nameUtf16;

			common::Utf8ToUtf16(nameUtf8, &nameUtf16, 0);
			SetTextById(item, 0x8A89C4B0, nameUtf16);

			if ((*m_permList)[i].desc[0] != '\0') {
				IDParam boxId(s_empty, 0x241D63BA);
				Widget *box = item->FindChild(boxId, 0);
				IDParam descTmplId(s_empty, 0x69BA076A);
				Plugin::TemplateOpenParam tmplOpt;

				m_plugin->TemplateOpen(box, descTmplId, tmplOpt);

				string dashUtf8("  - ");
				wstring dashUtf16;

				common::Utf8ToUtf16(dashUtf8, &dashUtf16, 0);
				SetTextById(item, 0x9857217F, dashUtf16);

				string descUtf8((*m_permList)[i].desc);
				wstring descUtf16;

				common::Utf8ToUtf16(descUtf8, &descUtf16, 0);
				SetTextById(item, 0x3AF0940F, descUtf16);
			}
		}

		IDParam cancelId(s_empty, 0xB70744D0);
		Widget *cancelBtn = page->FindChild(cancelId, 0);

		if (cancelBtn == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		cancelBtn->AddEventListener(0x10000008, new EventCBListener(CancelCb, this),
					    false);
		cancelBtn->m_event_enable |= 0x10;
		ScePafWidget_21125870(cancelBtn, 32);

		IDParam acceptId(s_empty, 0xEFADEEBE);
		Widget *acceptBtn = page->FindChild(acceptId, 0);

		if (acceptBtn == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		acceptBtn->AddEventListener(0x10000008, new EventCBListener(AcceptCb, this),
					    false);

		m_plugin->PageActivate(m_id);
		m_open = 1;
		return 0;
	}

	virtual int Resume()
	{
		if (m_open == 0)
			return FB_DIALOG_ERROR_INVALID_STATE;
		m_plugin->PageActivate(m_id);
		return 0;
	}

	virtual int Close()
	{
		if (m_open != 0) {
			Plugin::PageCloseParam opt;

			m_plugin->PageClose(m_id, opt);
		}
		m_open = 0;
		return 0;
	}

	virtual void SetParamPtr1(const void *ptr)
	{
		m_permList = (const vector<FbPermissionDesc> *)ptr;
	}

	virtual void SetParamPtr2(const void *ptr)
	{
		if (ptr != NULL)
			sce_paf_strncpy(m_titleName, (const char *)ptr, sizeof(m_titleName) - 1);
	}

	static void CancelCb(int32_t ev, Handler *self, Event *e, void *ud)
	{
		(void)ev;
		(void)self;
		(void)e;
		((FbPermissionView *)ud)->m_owner->OnPermissionCancel();
	}

	static void AcceptCb(int32_t ev, Handler *self, Event *e, void *ud)
	{
		(void)ev;
		(void)self;
		(void)e;
		((FbPermissionView *)ud)->m_owner->OnPermissionAccept();
	}

	const vector<FbPermissionDesc> *m_permList;
	char m_titleName[128];
};

/* =========================================================================
 * 4. FbConfirmBindView (0x0B876C53)
 * ========================================================================= */
class FbConfirmBindView : public FbViewBase {
public:
	FbConfirmBindView(FbEventBus *bus, Plugin *plugin) : FbViewBase(bus, plugin)
	{
		m_id = IDParam(s_empty, FB_VIEW_CONFIRM_BIND_ID);
		sce_paf_memset(m_titleName, 0, sizeof(m_titleName));
	}

	virtual ~FbConfirmBindView() {}

	virtual int Open()
	{
		if (m_open != 0)
			return 0;
		Plugin::PageOpenParam opt;
		Widget *page = m_plugin->PageOpen(m_id, opt);

		if (page == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		m_plugin->PageActivate(m_id);

		string titleUtf8(m_titleName);

		FbFormatSingleLine(titleUtf8, true);
		wstring titleUtf16;

		common::Utf8ToUtf16(titleUtf8, &titleUtf16, 0);
		wstring msg = MsgDialogFormat(GetPluginText(m_plugin, 0x925672D4),
					      titleUtf16.c_str());

		SetTextById(page, 0x33B7FB69, msg);

		IDParam cancelId(s_empty, 0xBD9FE2EE);
		Widget *cancelBtn = page->FindChild(cancelId, 0);

		if (cancelBtn == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		cancelBtn->AddEventListener(0x10000008, new EventCBListener(CancelCb, this),
					    false);
		cancelBtn->m_event_enable |= 0x10;
		ScePafWidget_21125870(cancelBtn, 32);

		IDParam acceptId(s_empty, 0x874FBCE5);
		Widget *acceptBtn = page->FindChild(acceptId, 0);

		if (acceptBtn == NULL)
			return FB_DIALOG_ERROR_INVALID_STATE;
		acceptBtn->AddEventListener(0x10000008, new EventCBListener(AcceptCb, this),
					    false);

		m_open = 1;
		return 0;
	}

	virtual int Resume()
	{
		if (m_open == 0)
			return FB_DIALOG_ERROR_INVALID_STATE;
		m_plugin->PageActivate(m_id);
		return 0;
	}

	virtual int Close()
	{
		if (m_open != 0) {
			Plugin::PageCloseParam opt;

			m_plugin->PageClose(m_id, opt);
		}
		m_open = 0;
		return 0;
	}

	virtual void SetParamPtr2(const void *ptr)
	{
		if (ptr != NULL)
			sce_paf_strncpy(m_titleName, (const char *)ptr, sizeof(m_titleName) - 1);
	}

	static void CancelCb(int32_t ev, Handler *self, Event *e, void *ud)
	{
		(void)ev;
		(void)self;
		(void)e;
		((FbConfirmBindView *)ud)->m_owner->OnConfirmBindCancel();
	}

	static void AcceptCb(int32_t ev, Handler *self, Event *e, void *ud)
	{
		(void)ev;
		(void)self;
		(void)e;
		((FbConfirmBindView *)ud)->m_owner->OnConfirmBindAccept();
	}

	char m_titleName[128];
};

/* =========================================================================
 * 5. FbWaitView (0x337FEFEB)
 * ========================================================================= */
class FbWaitView : public FbViewBase {
public:
	class Handler : public CommonGuiDialogHandler {
	public:
		Handler(FbWaitView *view) : m_view(view) {}
		virtual ~Handler() {}
		virtual void operator()(SceInt32 value, SceInt32 button)
		{
			(void)value;
			if (button != 0) {
				if (m_view != NULL) {
					m_view->m_owner->OnWaitCancel();
					m_view->m_canceled = 1;
					m_view->Close();
				}
			} else if (m_view != NULL && m_view->m_open != 0) {
				m_view->m_open = 0;
				m_view->m_closing = 0;
				m_view->m_owner->OnWaitClosed();
			}
		}

		FbWaitView *m_view;
	};

	FbWaitView(FbEventBus *bus, Plugin *plugin)
		: FbViewBase(bus, plugin), m_dialogId(0), m_canceled(0), m_closing(0)
	{
		m_id = IDParam(s_empty, FB_VIEW_WAIT_ID);
	}

	virtual ~FbWaitView() {}

	virtual int Open()
	{
		m_canceled = 0;
		m_closing = 0;
		CommonGuiLayoutDialogParam param;

		param.text = GetPluginText(m_plugin, 0x632BD493);
		param.handler = new Handler(this);
		param.notifyClose = true;
		param.layout = SceCommonGuiDialog_F8B2D7E1;

		int ret = SceCommonGuiDialog_CCFCD9A6(&param);

		if (ret >= 0) {
			m_dialogId = ret;
			m_open = 1;
		}
		return ret;
	}

	virtual int Resume()
	{
		return 0;
	}

	virtual int Close()
	{
		if (m_open != 0 && m_closing == 0) {
			SceCommonGuiDialog_BC311DC0(m_dialogId);
			m_dialogId = 0;
			m_closing = 1;
		}
		return 0;
	}

	virtual int IsOverlay() const
	{
		return 1;
	}

	int m_dialogId;
	int m_canceled;
	int m_closing;
};

/* =========================================================================
 * 6..11. CommonGuiMessageDialogParam-based overlay views
 * ========================================================================= */
class FbErrorView : public FbViewBase {
public:
	class Handler : public CommonGuiDialogHandler {
	public:
		Handler(FbErrorView *view) : m_view(view) {}
		virtual ~Handler() {}
		virtual void operator()(SceInt32 value, SceInt32 button)
		{
			(void)value;
			if (button == 0 && m_view != NULL && m_view->m_open != 0) {
				m_view->m_open = 0;
				m_view->m_owner->OnErrorClosed(m_view->m_error);
			}
		}

		FbErrorView *m_view;
	};

	FbErrorView(FbEventBus *bus, Plugin *plugin)
		: FbViewBase(bus, plugin), m_dialogId(0), m_error(0)
	{
		m_id = IDParam(s_empty, FB_VIEW_ERROR_ID);
	}

	virtual ~FbErrorView() {}

	virtual int Open()
	{
		if (m_open != 0)
			return 0;
		int retryable = FbIsRetryableError(m_error);
		int converted = FbConvertError(m_error);
		CommonGuiMessageDialogParam param;

		param.type = converted;
		param.handler = new Handler(this);
		param.defaultButton = 2;
		param.cancelButton = 2;
		param.notifyClose = true;
		param.option.flags = 14;
		if (retryable != 0)
			param.title.format = GetPluginText(m_plugin, 0xCB6191A6);

		int ret = SceCommonGuiDialog_20B141F1(&param);

		if (ret >= 0) {
			m_dialogId = ret;
			m_open = 1;
			return 0;
		}
		return ret;
	}

	virtual int Resume()
	{
		return 0;
	}

	virtual int Close()
	{
		if (m_open != 0) {
			SceCommonGuiDialog_BC311DC0(m_dialogId);
			m_dialogId = 0;
		}
		return 0;
	}

	virtual int IsDismissed() const
	{
		return 1;
	}

	virtual int IsOverlay() const
	{
		return 1;
	}

	virtual void SetParamInt1(int value)
	{
		m_error = value;
	}

	int m_dialogId;
	int m_error;
};

class FbRetryServerView : public FbViewBase {
public:
	class Handler : public CommonGuiDialogHandler {
	public:
		Handler(FbRetryServerView *view) : m_view(view) {}
		virtual ~Handler() {}
		virtual void operator()(SceInt32 value, SceInt32 button)
		{
			(void)value;
			if (button == 0 && m_view != NULL && m_view->m_open != 0) {
				m_view->m_open = 0;
				m_view->m_owner->OnErrorClosed(m_view->m_error);
			}
		}

		FbRetryServerView *m_view;
	};

	FbRetryServerView(FbEventBus *bus, Plugin *plugin)
		: FbViewBase(bus, plugin), m_dialogId(0), m_error(0), m_param2(0)
	{
		m_id = IDParam(s_empty, FB_VIEW_RETRY_SERVER_ID);
	}

	virtual ~FbRetryServerView() {}

	virtual int Open()
	{
		if (m_open != 0)
			return 0;
		CommonGuiMessageDialogParam param;

		param.type = m_error;
		param.messageId = 0x048E5C10;
		param.handler = new Handler(this);
		param.defaultButton = 1;
		param.cancelButton = 0;
		param.notifyClose = true;
		param.option.flags = 14;

		int ret = SceCommonGuiDialog_20B141F1(&param);

		if (ret >= 0) {
			m_dialogId = ret;
			m_open = 1;
			return 0;
		}
		return ret;
	}

	virtual int Resume()
	{
		return 0;
	}

	virtual int Close()
	{
		if (m_open != 0) {
			SceCommonGuiDialog_BC311DC0(m_dialogId);
			m_dialogId = 0;
		}
		return 0;
	}

	virtual int IsDismissed() const
	{
		return 1;
	}

	virtual int IsOverlay() const
	{
		return 1;
	}

	virtual void SetParamInt1(int value)
	{
		m_error = value;
	}

	virtual void SetParamInt2(int value)
	{
		m_param2 = value;
	}

	int m_dialogId;
	int m_error;
	int m_param2;
};

class FbFeedConfirmView : public FbViewBase {
public:
	class Handler : public CommonGuiDialogHandler {
	public:
		Handler(FbFeedConfirmView *view) : m_view(view) {}
		virtual ~Handler() {}
		virtual void operator()(SceInt32 value, SceInt32 button)
		{
			(void)value;
			if (button == 0 && m_view != NULL && m_view->m_open != 0) {
				m_view->m_open = 0;
				m_view->m_owner->OnRetryAuthClosed(m_view->m_step);
			}
		}

		FbFeedConfirmView *m_view;
	};

	FbFeedConfirmView(FbEventBus *bus, Plugin *plugin)
		: FbViewBase(bus, plugin), m_dialogId(0), m_error(0), m_step(0)
	{
		m_id = IDParam(s_empty, FB_VIEW_FEED_CONFIRM_ID);
	}

	virtual ~FbFeedConfirmView() {}

	virtual int Open()
	{
		if (m_open != 0)
			return 0;
		CommonGuiMessageDialogParam param;

		param.type = m_error;
		param.title.format = GetPluginText(m_plugin, 0xFAC0A35B);
		param.handler = new Handler(this);
		param.defaultButton = 0;
		param.cancelButton = 0;
		param.notifyClose = true;
		param.option.flags = 14;

		int ret = SceCommonGuiDialog_20B141F1(&param);

		if (ret >= 0) {
			m_dialogId = ret;
			m_open = 1;
			return 0;
		}
		return ret;
	}

	virtual int Resume()
	{
		return 0;
	}

	virtual int Close()
	{
		if (m_open != 0) {
			SceCommonGuiDialog_BC311DC0(m_dialogId);
			m_dialogId = 0;
		}
		return 0;
	}

	virtual int IsDismissed() const
	{
		return 1;
	}

	virtual int IsOverlay() const
	{
		return 1;
	}

	virtual void SetParamInt1(int value)
	{
		m_error = value;
	}

	virtual void SetParamInt2(int value)
	{
		m_step = value;
	}

	int m_dialogId;
	int m_error;
	int m_step;
};

class FbRetryAuthView : public FbViewBase {
public:
	class Handler : public CommonGuiDialogHandler {
	public:
		Handler(FbRetryAuthView *view) : m_view(view) {}
		virtual ~Handler() {}
		virtual void operator()(SceInt32 value, SceInt32 button)
		{
			(void)value;
			if (button == 0 && m_view != NULL && m_view->m_open != 0) {
				m_view->m_open = 0;
				m_view->m_owner->OnErrorClosed(m_view->m_error);
			}
		}

		FbRetryAuthView *m_view;
	};

	FbRetryAuthView(FbEventBus *bus, Plugin *plugin)
		: FbViewBase(bus, plugin), m_dialogId(0), m_error(0), m_param2(0)
	{
		m_id = IDParam(s_empty, FB_VIEW_RETRY_AUTH_ID);
	}

	virtual ~FbRetryAuthView() {}

	virtual int Open()
	{
		if (m_open != 0)
			return 0;
		CommonGuiMessageDialogParam param;

		param.type = SCE_NP_SNS_FB_ERROR_SERVER;
		param.messageId = 0xC51B20E4;
		param.handler = new Handler(this);
		param.defaultButton = 1;
		param.cancelButton = 0;
		param.notifyClose = true;
		param.option.flags = 14;

		int ret = SceCommonGuiDialog_20B141F1(&param);

		if (ret >= 0) {
			m_dialogId = ret;
			m_open = 1;
			return 0;
		}
		return ret;
	}

	virtual int Resume()
	{
		return 0;
	}

	virtual int Close()
	{
		if (m_open != 0) {
			SceCommonGuiDialog_BC311DC0(m_dialogId);
			m_dialogId = 0;
		}
		return 0;
	}

	virtual int IsDismissed() const
	{
		return 1;
	}

	virtual int IsOverlay() const
	{
		return 1;
	}

	virtual void SetParamInt1(int value)
	{
		m_error = value;
	}

	virtual void SetParamInt2(int value)
	{
		m_param2 = value;
	}

	int m_dialogId;
	int m_error;
	int m_param2;
};

class FbChatRestrictView : public FbViewBase {
public:
	class Handler : public CommonGuiDialogHandler {
	public:
		Handler(FbChatRestrictView *view) : m_view(view) {}
		virtual ~Handler() {}
		virtual void operator()(SceInt32 value, SceInt32 button)
		{
			(void)value;
			if (button == 0 && m_view != NULL && m_view->m_open != 0) {
				m_view->m_open = 0;
				m_view->m_owner->OnChatRestrictClosed();
			}
		}

		FbChatRestrictView *m_view;
	};

	FbChatRestrictView(FbEventBus *bus, Plugin *plugin)
		: FbViewBase(bus, plugin), m_dialogId(0), m_error(0)
	{
		m_id = IDParam(s_empty, FB_VIEW_CHAT_RESTRICT_ID);
	}

	virtual ~FbChatRestrictView() {}

	virtual int Open()
	{
		if (m_open != 0)
			return 0;
		CommonGuiMessageDialogParam param;

		param.type = m_error;
		param.title.format = GetPluginText(m_plugin, 0x5A21EEAA);
		param.handler = new Handler(this);
		param.defaultButton = 0;
		param.cancelButton = 0;
		param.notifyClose = true;
		param.option.flags = 14;

		int ret = SceCommonGuiDialog_20B141F1(&param);

		if (ret >= 0) {
			m_dialogId = ret;
			m_open = 1;
			return 0;
		}
		return ret;
	}

	virtual int Resume()
	{
		return 0;
	}

	virtual int Close()
	{
		if (m_open != 0) {
			SceCommonGuiDialog_BC311DC0(m_dialogId);
			m_dialogId = 0;
		}
		return 0;
	}

	virtual int IsDismissed() const
	{
		return 1;
	}

	virtual int IsOverlay() const
	{
		return 1;
	}

	virtual void SetParamInt1(int value)
	{
		m_error = value;
	}

	int m_dialogId;
	int m_error;
};

class FbNoticeView : public FbViewBase {
public:
	class Handler : public CommonGuiDialogHandler {
	public:
		Handler(FbNoticeView *view) : m_view(view) {}
		virtual ~Handler() {}
		virtual void operator()(SceInt32 value, SceInt32 button)
		{
			(void)value;
			if (button != 0) {
				if (m_view != NULL)
					m_view->m_owner->OnNoticeClosed();
			} else if (m_view != NULL && m_view->m_open != 0) {
				m_view->m_open = 0;
				m_view->m_owner->OnErrorClosed(m_view->m_error);
			}
		}

		FbNoticeView *m_view;
	};

	FbNoticeView(FbEventBus *bus, Plugin *plugin)
		: FbViewBase(bus, plugin), m_dialogId(0), m_error((SceInt32)0x80106101), m_param2(0)
	{
		m_id = IDParam(s_empty, FB_VIEW_NOTICE_ID);
	}

	virtual ~FbNoticeView() {}

	virtual int Open()
	{
		if (m_open != 0)
			return 0;
		CommonGuiMessageDialogParam param;

		param.type = m_error;
		param.title.format = GetPluginText(m_plugin, 0xB6EC9F9B);
		param.handler = new Handler(this);
		param.defaultButton = 0;
		param.cancelButton = 0;
		param.notifyClose = true;
		param.option.flags = 14;

		int ret = SceCommonGuiDialog_20B141F1(&param);

		if (ret >= 0) {
			m_dialogId = ret;
			m_open = 1;
		}
		return ret;
	}

	virtual int Resume()
	{
		return 0;
	}

	virtual int Close()
	{
		if (m_open != 0) {
			SceCommonGuiDialog_BC311DC0(m_dialogId);
			m_dialogId = 0;
		}
		return 0;
	}

	virtual int IsDismissed() const
	{
		return 1;
	}

	virtual int IsOverlay() const
	{
		return 1;
	}

	virtual void SetParamInt1(int value)
	{
		m_error = value;
	}

	virtual void SetParamInt2(int value)
	{
		m_param2 = value;
	}

	int m_dialogId;
	int m_error;
	int m_param2;
};

/* View factories */
FbViewBase *CreateFbBgView(FbEventBus *bus, Plugin *plugin)
{
	return new FbBgView(bus, plugin);
}

FbViewBase *CreateFbLoginView(FbEventBus *bus, Plugin *plugin)
{
	return new FbLoginView(bus, plugin);
}

FbViewBase *CreateFbConfirmBindView(FbEventBus *bus, Plugin *plugin)
{
	return new FbConfirmBindView(bus, plugin);
}

FbViewBase *CreateFbInputShortView(FbEventBus *bus, Plugin *plugin)
{
	return new FbInputShortView(bus, plugin);
}

FbViewBase *CreateFbInputLongView(FbEventBus *bus, Plugin *plugin)
{
	return new FbInputLongView(bus, plugin);
}

FbViewBase *CreateFbPermissionView(FbEventBus *bus, Plugin *plugin)
{
	return new FbPermissionView(bus, plugin);
}

FbViewBase *CreateFbWaitView(FbEventBus *bus, Plugin *plugin)
{
	return new FbWaitView(bus, plugin);
}

FbViewBase *CreateFbErrorView(FbEventBus *bus, Plugin *plugin)
{
	return new FbErrorView(bus, plugin);
}

FbViewBase *CreateFbChatRestrictView(FbEventBus *bus, Plugin *plugin)
{
	return new FbChatRestrictView(bus, plugin);
}

FbViewBase *CreateFbFeedConfirmView(FbEventBus *bus, Plugin *plugin)
{
	return new FbFeedConfirmView(bus, plugin);
}

FbViewBase *CreateFbRetryAuthView(FbEventBus *bus, Plugin *plugin)
{
	return new FbRetryAuthView(bus, plugin);
}

FbViewBase *CreateFbRetryServerView(FbEventBus *bus, Plugin *plugin)
{
	return new FbRetryServerView(bus, plugin);
}

FbViewBase *CreateFbNoticeView(FbEventBus *bus, Plugin *plugin)
{
	return new FbNoticeView(bus, plugin);
}
