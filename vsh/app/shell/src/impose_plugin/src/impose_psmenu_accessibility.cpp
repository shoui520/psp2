/*
 * impose_psmenu_accessibility.cpp: the PS menu's accessibility settings (0x811527E0-
 * 0x81152FB8): the check boxes for inverted colours and for the remapped buttons, each kept in
 * the registry; the remap is offered only while the console's own pad is the current port. The
 * file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <plugin.h>
#include <std/string>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_button_base.h>
#include <widget/w_check_box.h>
#include <regmgr/regmgr.h>
#include <inputdevice/pad/pad.h>
#include "../../../shell.h"
#include "impose.h"

using namespace paf;

/* the template of the settings and its children (PLACEHOLDER names) */
#define ACCESSIBILITY_TEMPLATE		0x8A49DCB4
#define ACCESSIBILITY_INVERT_CHECK	0xE4C8738C
#define ACCESSIBILITY_REMAP_CHECK	0x6F79308A
#define ACCESSIBILITY_REMAP_TEXT	0x64911D52
#define ACCESSIBILITY_ROW		0x8E275F73

/* the remap's text, shown and dimmed with the remap taken away (value only) */
#define ACCESSIBILITY_ALPHA_ON		1.0f
#define ACCESSIBILITY_ALPHA_OFF		0.3f

/* the message the error dialog is given (empty; 0x81564EF8) */
static wstring s_errorMessage;

/* (0x811527E0) */
ImposePsMenuAccessibility::ImposePsMenuAccessibility(Plugin *plugin)
	: m_plugin(plugin), m_box(NULL)
{
}

/* the settings opened in the menu's box, from the registry (0x811527E8) */
void ImposePsMenuAccessibility::Open(ui::Widget *box)
{
	m_box = box;
	m_plugin->TemplateOpen(box, IDParam(ACCESSIBILITY_TEMPLATE), Plugin::TemplateOpenParam());

	ui::CheckBox *invert = PSMENU_FIND(box, ui::CheckBox, ACCESSIBILITY_INVERT_CHECK);

	if (invert != NULL) {
		int value;

		invert->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
			new ImposePsMenuArgListener<ImposePsMenuAccessibility, ui::CheckBox *>(
				this, &ImposePsMenuAccessibility::OnInvertColor, invert),
			true);
		value = 0;
		if (sceRegMgrGetKeyInt("/CONFIG/ACCESSIBILITY", "invert_color", &value) == 0)
			invert->SetCheck(value != 0, -0.1f);
	}

	ui::CheckBox *remap = PSMENU_FIND(box, ui::CheckBox, ACCESSIBILITY_REMAP_CHECK);

	if (remap != NULL) {
		int port = inputdevice::pad::GetCurrPort();

		if (port == 0 || port == 1) {
			int value;

			remap->AddEventListener(ui::ButtonBase::CB_BTN_DECIDE,
				new ImposePsMenuArgListener<ImposePsMenuAccessibility, ui::CheckBox *>(
					this, &ImposePsMenuAccessibility::OnKeyRemap, remap),
				true);
			value = 0;
			if (sceRegMgrGetKeyInt("/CONFIG/ACCESSIBILITY", "keyremap_enable",
					       &value) == 0)
				remap->SetCheck(value != 0, -0.1f);
			box->FindChild(IDParam(ACCESSIBILITY_REMAP_TEXT), 0)
				->SetMetaAlpha(ACCESSIBILITY_ALPHA_ON);
		} else {
			remap->Disable(false);
			remap->SetCheck(false, -0.1f);
			box->FindChild(IDParam(ACCESSIBILITY_REMAP_TEXT), 0)
				->SetMetaAlpha(ACCESSIBILITY_ALPHA_OFF);
		}
	}
	imposeLayoutText(m_box->FindChild(IDParam(ACCESSIBILITY_ROW), 0),
			 IDParam(ACCESSIBILITY_REMAP_TEXT), 534.0f);
}

/* (0x81152E5C) */
void ImposePsMenuAccessibility::Close()
{
	if (m_box != NULL)
		m_box = NULL;
}

/* a setting written; a registry failure told in the error dialog */
static inline void imposePsMenuAccessibilitySet(const char *name, int value)
{
	int ret = sceRegMgrSetKeyInt("/CONFIG/ACCESSIBILITY", name, value);

	if (SCE_ERROR_ISFAILED(ret) && SCE_ERROR_FACILITY(ret) == SHELL_ERROR_FACILITY_REGMGR) {
		int type = SHELL_ERROR_DIALOG_START;

		if (sceRegMgrIsBlueScreen(ret) != 0)
			type = SHELL_ERROR_DIALOG_BLUE_SCREEN;
		shellErrorDialogShow(type, ret, &s_errorMessage, 0);
	}
}

/* (0x81152E66) */
void ImposePsMenuAccessibility::OnInvertColor(ui::CheckBox *check)
{
	imposePsMenuAccessibilitySet("invert_color", check->IsChecked());
}

/* (0x81152EB4) */
void ImposePsMenuAccessibility::OnKeyRemap(ui::CheckBox *check)
{
	imposePsMenuAccessibilitySet("keyremap_enable", check->IsChecked());
}
