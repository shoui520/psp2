/*
 * The text input dialog: a text, a second text and a text box, with cancel and OK (and a third
 * button in its second layout). The text typed is read back as UTF-8. No binary names this file,
 * its classes or their members; the names are ours.
 */
#include <stdint.h>
#include <std/new>
#include <common/encoded_string.h>
#include <widget/w_text.h>
#include <widget/w_text_box.h>
#include "dialog.h"

using namespace paf;
using namespace paf::ui;

/* the dialog's parts */
#define INPUT_DIALOG_PART_TEXT		1
#define INPUT_DIALOG_PART_SUB_TEXT	2
#define INPUT_DIALOG_PART_CANCEL	12
#define INPUT_DIALOG_PART_OK		13
#define INPUT_DIALOG_PART_THIRD		14
#define INPUT_DIALOG_PART_TEXT_BOX	22

/* the button OK closes the dialog with */
#define INPUT_DIALOG_BUTTON_OK		2

/* the focus directions the third button is linked by: below the text box, above the others */
#define INPUT_DIALOG_FOCUS_UP		0
#define INPUT_DIALOG_FOCUS_DOWN		1

/* the IME's types for imeType 1 to 3: SCE_IME_TYPE_NUMBER, SCE_IME_TYPE_EXTENDED_NUMBER (libime's
   values) and a type the SDK does not name */
#define INPUT_DIALOG_IME_TYPE_1		2
#define INPUT_DIALOG_IME_TYPE_2		3
#define INPUT_DIALOG_IME_TYPE_3		9

/* the bottom margin of the text's hit region when it is one line */
#define INPUT_DIALOG_TEXT_MARGIN	20.0f

/* the layouts: the text, the second text, the text box, cancel and OK, and the third button */
static Dialog::PartsList s_inputParts[] = {
	{ INPUT_DIALOG_PART_TEXT, IDParam((uint32_t)0xD0791765) },
	{ INPUT_DIALOG_PART_SUB_TEXT, IDParam((uint32_t)0x66EA7A2F) },
	{ INPUT_DIALOG_PART_TEXT_BOX, IDParam((uint32_t)0x770B9F3A) },
	{ INPUT_DIALOG_PART_CANCEL, IDParam((uint32_t)0x5049F5E4) },
	{ INPUT_DIALOG_PART_OK, IDParam((uint32_t)0xBD559A97) },
	{ 0, IDParam() },
};

static const CommonGuiDialogPage s_inputPage = {
	{ 0xA0B259A1, 0x438C7285, 0x428A76A2 }
};

/* the buttons of the text dialog with cancel and OK */
static CommonGuiDialogLayout s_inputLayout = {
	&s_inputPage, s_inputParts, SceCommonGuiDialog_92752DE2.buttons
};

static Dialog::PartsList s_inputPartsThird[] = {
	{ INPUT_DIALOG_PART_TEXT, IDParam((uint32_t)0xD0791765) },
	{ INPUT_DIALOG_PART_SUB_TEXT, IDParam((uint32_t)0x66EA7A2F) },
	{ INPUT_DIALOG_PART_TEXT_BOX, IDParam((uint32_t)0x770B9F3A) },
	{ INPUT_DIALOG_PART_CANCEL, IDParam((uint32_t)0x5049F5E4) },
	{ INPUT_DIALOG_PART_OK, IDParam((uint32_t)0xBD559A97) },
	{ INPUT_DIALOG_PART_THIRD, IDParam((uint32_t)0xBCFBF95C) },
	{ 0, IDParam() },
};

static const CommonGuiDialogPage s_inputPageThird = {
	{ 0x6833951C, 0x438C7285, 0x1D312764 }
};

static const CommonGuiDialogButton s_inputButtonsThird[] = {
	{ INPUT_DIALOG_PART_CANCEL, 0xADCD9ADF, 3, 0xC17D7980 },
	{ INPUT_DIALOG_PART_OK, 0x805BECA6, INPUT_DIALOG_BUTTON_OK, 0x20413274 },
	{ INPUT_DIALOG_PART_THIRD, 0xB992EEBC, 9, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static CommonGuiDialogLayout s_inputLayoutThird = {
	&s_inputPageThird, s_inputPartsThird, s_inputButtonsThird
};

/* the dialog's handler: the text typed written when OK closes it, then the caller's handler
   called, or the result set for the call that waits */
class InputDialogHandler : public CommonGuiDialogHandler {
public:
	InputDialogHandler(bool wait, string *outText, CommonGuiDialogHandler *handler)
		: m_wait(wait), m_outText(outText), m_handler(handler)
	{
	}

	virtual ~InputDialogHandler()
	{
		if (m_handler != NULL)
			delete m_handler;
	}

	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		if (button == INPUT_DIALOG_BUTTON_OK) {
			string *outText = m_outText;

			if (outText != NULL)
				*outText = SceCommonGuiDialog_1D4E3C83(value);
		}
		if (m_wait)
			SceCommonGuiDialog_2CA16BBF(value, button);
		else if (m_handler != NULL)
			(*m_handler)(value, button);
		SceCommonGuiDialog_BC311DC0(value);
	}

	bool m_wait;
	string *m_outText;
	CommonGuiDialogHandler *m_handler;
};

/* the text box's enter key: the dialog closed with OK */
class InputEnterListener : public EventListener {
public:
	InputEnterListener(int32_t id) : m_id(id)
	{
	}

	virtual int32_t Do(int32_t type, Handler *self, Event *e)
	{
		SceCommonGuiDialog_01702008(m_id, INPUT_DIALOG_BUTTON_OK);
		return 0;
	}

	int32_t m_id;
};

/* the dialog has opened: the text box edited at once */
class InputOpenHandler : public CommonGuiLayoutDialogOpenHandler {
public:
	virtual void operator()(SceInt32 id)
	{
		Widget *box = SceCommonGuiDialog_8947FC23(id, INPUT_DIALOG_PART_TEXT_BOX);

		if (box == NULL || !box->IsInherit(TextBox::TypeName()))
			return;
		box->AddEventListener(TextBox::CB_TEXT_BOX_ENTER_PRESSED, new InputEnterListener(id));
		((TextBox *)box)->BeginEdit();
	}
};

/* the text is ready: one line of it takes a taller hit region */
class InputTextListener : public EventListener {
public:
	virtual int32_t Do(int32_t type, Handler *self, Event *e)
	{
		Text *text = (Text *)self;
		uint32_t lines;

		if (text != NULL && text->IsInherit(Text::TypeName()) &&
		    text->GetLineCount(lines) == 0 && lines == 1) {
			const AdjustParam *adjust = &ScePafWidget_BE1CEA2E;

			if (text->m_adjust_param != NULL)
				adjust = text->m_adjust_param;

			AdjustParam param(*adjust);

			param.m_bottom_margin = INPUT_DIALOG_TEXT_MARGIN;
			ScePafWidget_622193BD(text, false);
			text->SetHitRegion(param);
			text->ChangeState(3, -1);
		}
		return 0;
	}
};

/* a wide string as UTF-8, for as long as it lives */
class Utf8String {
public:
	Utf8String(const wstring &text)
	{
		ScePafMisc_7709D1B2(&m_string, text, common::EncodedString::Encoding_Utf16);
	}

	~Utf8String()
	{
		ScePafMisc_B45A7521(&m_string);
	}

	const char *c_str()
	{
		return ScePafMisc_A7C6AF5C(&m_string);
	}

	common::EncodedString m_string;
};

string SceCommonGuiDialog_1D4E3C83(int dialogId)
{
	Widget *box = SceCommonGuiDialog_8947FC23(dialogId, INPUT_DIALOG_PART_TEXT_BOX);

	if (box != NULL) {
		wstring text;

		box->GetString(text);
		return string(Utf8String(text).c_str());
	}
	return string((const char *)NULL);
}

int SceCommonGuiDialog_7C019CF0(const CommonGuiInputDialogParam *param)
{
	CommonGuiLayoutDialogParam layoutParam;
	int id;

	CopyDialogParam(&layoutParam, param);
	layoutParam.text = param->text;
	layoutParam.layout = param->thirdButton ? s_inputLayoutThird : s_inputLayout;
	layoutParam.openHandler = new InputOpenHandler();
	layoutParam.handler = new InputDialogHandler(param->wait, param->outText, param->handler);
	id = SceCommonGuiDialog_CCFCD9A6(&layoutParam);
	if (id < 0)
		return id;

	Widget *box = SceCommonGuiDialog_8947FC23(id, INPUT_DIALOG_PART_TEXT_BOX);

	if (box != NULL && box->IsInherit(TextBox::TypeName())) {
		((TextBox *)box)->SetMaxLength(param->maxLength);
		if (param->imeType == 1) {
			TextBox::ImeParam ime;

			((TextBox *)box)->GetImeParam(ime);
			ime.type = INPUT_DIALOG_IME_TYPE_1;
			((TextBox *)box)->SetImeParam(ime);
		} else if (param->imeType == 2) {
			TextBox::ImeParam ime;

			((TextBox *)box)->GetImeParam(ime);
			ime.type = INPUT_DIALOG_IME_TYPE_2;
			((TextBox *)box)->SetImeParam(ime);
		} else if (param->imeType == 3) {
			TextBox::ImeParam ime;

			((TextBox *)box)->GetImeParam(ime);
			ime.type = INPUT_DIALOG_IME_TYPE_3;
			((TextBox *)box)->SetImeParam(ime);
		}
	}
	if (param->thirdButton) {
		Widget *third = SceCommonGuiDialog_8947FC23(id, INPUT_DIALOG_PART_THIRD);

		if (third != NULL) {
			Widget *part = SceCommonGuiDialog_8947FC23(id, INPUT_DIALOG_PART_TEXT_BOX);

			if (part != NULL)
				part->SetFocusDirectionWidget(INPUT_DIALOG_FOCUS_DOWN, third);
			part = SceCommonGuiDialog_8947FC23(id, INPUT_DIALOG_PART_CANCEL);
			if (part != NULL)
				part->SetFocusDirectionWidget(INPUT_DIALOG_FOCUS_UP, third);
			part = SceCommonGuiDialog_8947FC23(id, INPUT_DIALOG_PART_OK);
			if (part != NULL)
				part->SetFocusDirectionWidget(INPUT_DIALOG_FOCUS_UP, third);
		}
	}

	Widget *text = SceCommonGuiDialog_8947FC23(id, INPUT_DIALOG_PART_TEXT);

	if (text != NULL)
		text->AddEventListener(Handler::CB_STATE_READY, new InputTextListener());

	Widget *subText = SceCommonGuiDialog_8947FC23(id, INPUT_DIALOG_PART_SUB_TEXT);

	if (subText != NULL)
		subText->SetString(param->subText);
	if (param->wait)
		id = SceCommonGuiDialog_D2381B88(id);
	return id;
}

void SceCommonGuiDialog_11D35CB5(int dialogId)
{
	SceCommonGuiDialog_BC311DC0(dialogId);
}
