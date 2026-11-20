/*
 * The list dialog's parameters: their defaults and the lines a caller sets, each with its text
 * and, if it has one, its button. No binary names this file or its variables; the names are
 * ours.
 */
#include <stdint.h>
#include <std/new>
#include "dialog.h"

using namespace paf;
using namespace paf::ui;

/* the list dialog's part whose button takes the focus within its group */
#define LIST_DIALOG_PART_CANCEL		8

/* the list dialog's background, and the colour of its heading and lines */
static const math::v4 s_listBgColor(205.0f / 255.0f, 204.0f / 255.0f, 210.0f / 255.0f, 1.0f);
static const math::v4 s_listTextColor(109.0f / 255.0f, 107.0f / 255.0f, 117.0f / 255.0f, 1.0f);

void SceCommonGuiDialog_96B9C9F6(CommonGuiListDialogItem *item, SceInt32 plain,
				 const wstring &text)
{
	item->mark = plain == 0;
	item->shadow = true;
	item->markColor = s_listTextColor;
	item->text = text;
	item->textColor = s_listTextColor;
	item->buttonColor = s_listTextColor;
}

void SceCommonGuiDialog_2FA91F45(CommonGuiListDialogItem *item, SceInt32 plain,
				 const wstring &text, const wstring &buttonLabel,
				 SceInt32 buttonValue)
{
	SceCommonGuiDialog_96B9C9F6(item, plain, text);
	item->buttonLabel = buttonLabel;
	item->buttonValue = buttonValue;
}

void SceCommonGuiDialog_D44D1EFB(CommonGuiListDialogParam *param, SceUInt32 index,
				 SceInt32 plain, const wstring &text)
{
	if (param->items.size() <= index)
		param->items.resize(index + 1, CommonGuiListDialogItem());
	SceCommonGuiDialog_96B9C9F6(&param->items[index], plain, text);
}

void SceCommonGuiDialog_6A0E7F14(CommonGuiListDialogParam *param, SceUInt32 index,
				 SceInt32 plain, const wstring &text, const wstring &buttonLabel,
				 SceInt32 buttonValue)
{
	if (param->items.size() <= index)
		param->items.resize(index + 1, CommonGuiListDialogItem());
	SceCommonGuiDialog_2FA91F45(&param->items[index], plain, text, buttonLabel,
				    buttonValue);
}

CommonGuiListDialogParam::CommonGuiListDialogParam()
	: type(0), fit(false), headingColor(SceCommonGuiDialog_EF2C5382), wait(false),
	  outId(NULL), handler(NULL)
{
	ColorV4(bgColor) = s_listBgColor;
	headingColor = s_listTextColor;
	fit = true;
}

int SceCommonGuiDialog_BACBA7F2(const CommonGuiListDialogParam *param)
{
	int id = SceCommonGuiDialog_F3D37CAE(param);

	if (id >= 0) {
		Widget *cancel = SceCommonGuiDialog_8947FC23(id, LIST_DIALOG_PART_CANCEL);

		if (cancel != NULL)
			cancel->m_focus_in_group = true;
	}
	return id;
}
