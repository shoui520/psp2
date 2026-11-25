/*
 * power_manage_menu.cpp: power-off / standby confirmation menu in power_manage_plugin.
 * 3.65-CEX: 0x81236C74 - 0x812380F8
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <common/transition.h>
#include <graphics/drawobj/text_obj.h>
#include <inputdevice/inputdevice.h>
#include <math/math.h>
#include <system/system.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/w_button.h>
#include <widget/w_image_button.h>
#include <widget/w_plane_button.h>
#include <widget/w_text.h>

#include "shell.h"
#include "plugins.h"
#include "src/livespace_db/lsdb_keys.h"
#include "power_manage_internal.h"

namespace power_manage {

PowerMenu *g_powerMenu = NULL;

static const SceUInt32 kPowerMenuTemplateHash = 0x26ED32B8U;	/* 653076344 */
static const SceUInt32 kPowerMenuRootHash = 0x05C1A0C5U;	/* 96577541 */
static const SceUInt32 kPowerMenuDolceTemplateHash = 0x0EFF9AF1U;	/* 251623281 */
static const SceUInt32 kPowerMenuVitaTemplateHash = 0xA508B7D8U;	/* -1526131992 */
static const SceUInt32 kPowerMenuPanelHash = 0x22BCD1E7U;	/* 582810791 */
static const SceUInt32 kPowerOffButtonHash = 0xDC1CD5C2U;	/* -602082046 */
static const SceUInt32 kCancelButtonHash = 0xDAA39E80U;		/* -626804928 */
static const SceUInt32 kStandbyButtonHash = 0x8BBE3A78U;	/* -1950484776 */
static const SceUInt32 kBackdropButtonHash = 0xD53C9FB3U;	/* -717451629 */
static const SceUInt32 kHintTextHash = 0x1B2D39F7U;		/* 455964535 */
static const SceUInt32 kStandbyHintMessageHash = 0xB18944C5U;	/* -1316404587 */
static const SceUInt32 kPowerOffHintMessageHash = 0x544CE1EAU;	/* 1414338986 */

static const int kImposeSlotClosePsMenu = 4;			/* +0x10 (16) */
static const int kTopmenuSlotOverlayBegin = 52;			/* +0xD0 (208) */
static const int kTopmenuSlotOverlayEnd = 53;			/* +0xD4 (212) */

static const SceUInt32 kInputOwnershipMask = 0x6FU;

class PowerMenuFocusListener : public paf::ui::EventListener {
public:
	PowerMenuFocusListener(PowerMenu *menu, SceUInt32 messageHash)
		: m_menu(menu), m_messageHash(messageHash)
	{
	}

	virtual int32_t Do(int32_t eventType, paf::ui::Handler *handler, paf::ui::Event *event)
	{
		(void)eventType;
		(void)handler;
		(void)event;
		if (m_killed)
			return (int32_t)0x80AF4101U;
		if (m_menu != NULL)
			m_menu->UpdateHintText(m_messageHash);
		return 0;
	}

	PowerMenu *m_menu;
	SceUInt32 m_messageHash;
};

template <typename WidgetType>
static WidgetType *findTypedChild(paf::ui::Widget *parent, SceUInt32 hash)
{
	paf::ui::Widget *child = parent->FindChild(paf::IDParam(hash), 0);
	if (child != NULL && child->IsInherit(WidgetType::TypeName()))
		return (WidgetType *)child;
	return NULL;
}

PowerMenu::PowerMenu()
	: m_root(NULL),
	  m_powerOffButton(NULL),
	  m_cancelButton(NULL),
	  m_standbyButton(NULL),
	  m_backdropButton(NULL),
	  m_hintText(NULL)
{
	g_powerMenu = this;
}

PowerMenu::~PowerMenu()
{
	g_powerMenu = NULL;
}

void PowerMenu::Open()
{
	if (m_root != NULL)
		return;

	paf::Plugin *imposePlugin = paf::Plugin::Find("impose_plugin");
	paf::Plugin *topmenuPlugin = paf::Plugin::Find("topmenu_plugin");
	if (imposePlugin == NULL || topmenuPlugin == NULL)
		return;

	ShellPluginInterface *imposeIface =
		(ShellPluginInterface *)imposePlugin->GetInterface(1);
	ShellPluginInterface *topmenuIface =
		(ShellPluginInterface *)topmenuPlugin->GetInterface(1);
	if (imposeIface == NULL || topmenuIface == NULL)
		return;

	imposeIface->func[kImposeSlotClosePsMenu]();

	void (*overlayBegin)(void *, int, int, int, int) =
		(void (*)(void *, int, int, int, int))topmenuIface->func[kTopmenuSlotOverlayBegin];
	overlayBegin(this, g_powerManagePlugin->m_draw_priority, 1, 0, 0);

	paf::Plugin::TemplateOpenParam tplParam;
	g_powerManagePlugin->TemplateOpen(g_powerManage->m_page,
					  paf::IDParam(kPowerMenuTemplateHash), tplParam);
	m_root = g_powerManage->m_page->FindChild(paf::IDParam(kPowerMenuRootHash), 0);
	ScePafWidget_0253DC9C(m_root, true);

	if (paf::system::Model::GetType() == paf::system::Model::Type_Dolce) {
		paf::Plugin::TemplateOpenParam subParam;
		g_powerManagePlugin->TemplateOpen(m_root,
						  paf::IDParam(kPowerMenuDolceTemplateHash),
						  subParam);
	} else {
		paf::Plugin::TemplateOpenParam subParam;
		g_powerManagePlugin->TemplateOpen(m_root,
						  paf::IDParam(kPowerMenuVitaTemplateHash),
						  subParam);
	}

	paf::ui::Widget *panel = m_root->FindChild(paf::IDParam(kPowerMenuPanelHash), 0);
	ScePafWidget_A1193AAB(m_root, true, false);
	ScePafWidget_42C5A4D4(m_root, 0, 0);

	m_powerOffButton = findTypedChild<paf::ui::ImageButton>(panel, kPowerOffButtonHash);
	if (m_powerOffButton != NULL) {
		paf::math::v3 texSize(30.0f, 38.0f, 0.0f);
		m_powerOffButton->SetTextureSize(texSize);
		m_powerOffButton->AddEventListener(
			0x100004, new paf::ui::EventCBListener(OnPowerOffLayout, this), false);
		m_powerOffButton->AddEventListener(
			0x10000008, new paf::ui::EventCBListener(OnPowerOffPressed, this), false);
		m_powerOffButton->Hide(0xFF, NULL, NULL, 0.0f);
	}

	m_cancelButton = findTypedChild<paf::ui::Button>(panel, kCancelButtonHash);
	if (m_cancelButton != NULL) {
		m_cancelButton->AddEventListener(
			0x10000008, new paf::ui::EventCBListener(OnCancelPressed, this), false);
		m_cancelButton->Hide(0xFF, NULL, NULL, 0.0f);
	}

	m_standbyButton = findTypedChild<paf::ui::Button>(panel, kStandbyButtonHash);
	if (m_standbyButton != NULL) {
		m_standbyButton->AddEventListener(
			0x10000008, new paf::ui::EventCBListener(OnStandbyPressed, this), false);
		m_standbyButton->Hide(0xFF, NULL, NULL, 0.0f);
	}

	m_backdropButton = findTypedChild<paf::ui::PlaneButton>(m_root, kBackdropButtonHash);
	if (m_backdropButton != NULL) {
		m_backdropButton->AddEventListener(
			0x10000008, new paf::ui::EventCBListener(OnCancelPressed, this), false);
		m_backdropButton->EnableTouchFrontEvent(true);
	}

	if (m_standbyButton != NULL && m_powerOffButton != NULL) {
		m_hintText = findTypedChild<paf::ui::Text>(m_root, kHintTextHash);
		m_standbyButton->AddEventListener(
			0x40001, new PowerMenuFocusListener(this, kStandbyHintMessageHash), false);
		m_powerOffButton->AddEventListener(
			0x40001, new PowerMenuFocusListener(this, kPowerOffHintMessageHash), false);
	}

	powerManagePushKeyHandler(OnKeyCancel, NULL);
	ScePafCommon_6CF3A3C8(0.0f, m_root, paf::common::transition::Type_SlideFromBottom4,
			      true, false);
	paf::inputdevice::GetOwnership(kInputOwnershipMask);
}

void PowerMenu::Close()
{
	if (m_root == NULL)
		return;

	m_powerOffButton = NULL;
	m_cancelButton = NULL;
	m_standbyButton = NULL;
	m_backdropButton = NULL;
	m_hintText = NULL;

	powerManagePopKeyHandler(OnKeyCancel, NULL);
	paf::inputdevice::ReleaseOwnership(kInputOwnershipMask);
	ScePafCommon_B48C9BDC(0.0f, m_root, paf::common::transition::Type_SlideFromBottom4,
			      true, false);
	m_root->m_name.SetID(paf::string(""));

	paf::Plugin *topmenuPlugin = paf::Plugin::Find("topmenu_plugin");
	if (topmenuPlugin != NULL) {
		ShellPluginInterface *topmenuIface =
			(ShellPluginInterface *)topmenuPlugin->GetInterface(1);
		if (topmenuIface != NULL) {
			void (*overlayEnd)(void *, int) =
				(void (*)(void *, int))topmenuIface->func[kTopmenuSlotOverlayEnd];
			overlayEnd(this, 230);
		}
	}
	m_root = NULL;
}

void PowerMenu::UpdateLayout()
{
	paf::math::v3 texSize(30.0f, 38.0f, 0.0f);
	m_powerOffButton->SetTextureSize(texSize);

	paf::graph::DrawObj *drawObj = m_powerOffButton->GetDrawObj(1);
	if (drawObj != NULL && drawObj->IsInherit(paf::graph::TextObj::DrawObjType())) {
		paf::graph::TextObj *labelObj = (paf::graph::TextObj *)drawObj;
		paf::ui::AdjustParam adjust = (m_powerOffButton->m_adjust_param != NULL)
			? *m_powerOffButton->m_adjust_param
			: ScePafWidget_BE1CEA2E;
		paf::math::v2 textBounds(0.0f, 0.0f);
		labelObj->GetBounds(textBounds);
		adjust.m_left_margin = 170.0f - (textBounds.extract_x() + 46.0f) * 0.5f;
		ScePafWidget_B1C23134(m_powerOffButton, adjust);
	}

	m_powerOffButton->Show(0xFF, new paf::Timer(100.0f), NULL, 0.0f);
	if (m_cancelButton != NULL)
		m_cancelButton->Show(0xFF, new paf::Timer(100.0f), NULL, 0.0f);
	if (m_standbyButton != NULL)
		m_standbyButton->Show(0xFF, new paf::Timer(100.0f), NULL, 0.0f);
}

void PowerMenu::UpdateHintText(SceUInt32 messageHash)
{
	if (m_hintText == NULL)
		return;

	paf::wstring currentText;
	m_hintText->GetString(currentText);
	const wchar_t *nextText = g_powerManagePlugin->GetString(paf::IDParam(messageHash));
	if (currentText != nextText) {
		paf::wstring newLabel(nextText);
		m_hintText->SetString(newLabel);
		m_hintText->Hide(0xFF, NULL, NULL, 0.0f);
		m_hintText->Show(0xFF, new paf::Timer(100.0f), NULL, 0.0f);
	}
}

void PowerMenu::OnPowerOffPressed(int32_t eventType, paf::ui::Handler *handler,
				  paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)handler;
	(void)event;
	(void)userData;
	powerManagePopKeyHandler(OnKeyCancel, NULL);
	shellShutdownRequest(0, 0);
}

void PowerMenu::OnCancelPressed(int32_t eventType, paf::ui::Handler *handler,
				paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)handler;
	(void)event;
	(void)userData;
	if (g_powerMenu != NULL)
		g_powerMenu->Close();
}

void PowerMenu::OnStandbyPressed(int32_t eventType, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)handler;
	(void)event;
	(void)userData;
	powerManageRequestDisplayOff();
}

void PowerMenu::OnPowerOffLayout(int32_t eventType, paf::ui::Handler *handler,
				 paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)handler;
	(void)event;
	(void)userData;
	if (g_powerMenu != NULL)
		g_powerMenu->UpdateLayout();
}

void PowerMenu::OnKeyCancel(void *userData)
{
	(void)userData;
	if (g_powerMenu != NULL)
		g_powerMenu->Close();
}

} /* namespace power_manage */
