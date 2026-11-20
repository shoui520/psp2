/*
 * The library's API over the dialog object: the parameters' defaults, opening a dialog with its
 * heading and text, finding it by its ID to set its parts, colours and images, holding and
 * letting it go, and the handlers it calls. No binary names this file, its classes or their
 * members; the names are ours.
 */
#include <stdint.h>
#include <std/new>
#include <widget/w_busyindicator.h>
#include <widget/w_scroll_view_base.h>
#include <widget/w_state_button_base.h>
#include <widget/w_text.h>
#include "dialog.h"

using namespace paf;
using namespace paf::ui;

/* the dialog's parts this file uses */
#define DIALOG_PART_DIMMER		3
#define DIALOG_PART_BOX			4
#define DIALOG_PART_IMAGE		5
#define DIALOG_PART_HEADING		6
#define DIALOG_PART_HEADING_LINE	7
#define DIALOG_PART_CANCEL		8
#define DIALOG_PART_VIEW		9
#define DIALOG_PART_CONTENTS		10
#define DIALOG_PART_LIST_VIEW		11
#define DIALOG_PART_BUTTONS		12
#define DIALOG_PART_CHECK_BOX		15

/* the box's children: the space above its contents, its background and dividers, and its
   waiting animation */
#define DIALOG_HEADING_SPACE		0xC49DC41B
#define DIALOG_BACKGROUND		0x2C5B8B18
#define DIALOG_DIVIDER_TOP		0x1BCD9CA1
#define DIALOG_DIVIDER_BOTTOM		0x85A75B2E
#define DIALOG_BUSY			0x1332D6FC

/* the width from which the dialog has the wide layout */
#define DIALOG_WIDE_WIDTH		960.0f

/* the page's defaults */
#define DIALOG_PAGE_OPTION		128

/* the timer that sets the scroll view's clipping once the dialog has opened */
#define DIALOG_OPEN_TIMER		Handler::CB_TIMEOUT

/* the scroll view of a dialog: its scroll bar placed by the dialog's layout */
class DialogScrollBarController : public ScrollViewScrollBarController {
public:
	DialogScrollBarController(CommonGuiDialog *dialog, ScrollViewBase *view)
		: m_dialog(dialog), m_view(view), m_height(0.0f)
	{
	}

	virtual void UpdateRange(const ScrollViewBarRange &range);

	CommonGuiDialog *m_dialog;
	ScrollViewBase *m_view;
	/* the view's height the view box was last fitted to */
	float m_height;
};

/* the dialog has opened: the scroll view clips its contents unless they end on whole lines */
class DialogOpenListener : public EventListener {
public:
	DialogOpenListener(ScrollViewBase *view) : m_view(view)
	{
	}

	virtual int32_t Do(int32_t type, Handler *self, Event *e);

	ScrollViewBase *m_view;
};

/* the function form of a handler */
class DialogFunctionHandler : public CommonGuiDialogHandler {
public:
	DialogFunctionHandler(CommonGuiDialogFunction func, void *arg) : m_func(func), m_arg(arg)
	{
	}

	virtual ~DialogFunctionHandler();
	virtual void operator()(SceInt32 value, SceInt32 button);

	CommonGuiDialogFunction m_func;
	void *m_arg;
};

/* the function form of an open handler, for an interval */
class DialogIntervalFunction : public CommonGuiLayoutDialogOpenHandler {
public:
	DialogIntervalFunction(CommonGuiDialogOpenFunction func, void *arg) : m_func(func), m_arg(arg)
	{
	}

	virtual ~DialogIntervalFunction();
	virtual void operator()(SceInt32 id);

	CommonGuiDialogOpenFunction m_func;
	void *m_arg;
};

/* the function form of an open handler */
class DialogOpenFunction : public CommonGuiLayoutDialogOpenHandler {
public:
	DialogOpenFunction(CommonGuiDialogOpenFunction func, void *arg) : m_func(func), m_arg(arg)
	{
	}

	virtual void operator()(SceInt32 id)
	{
		if (m_func != NULL)
			m_func(id, m_arg);
	}

	CommonGuiDialogOpenFunction m_func;
	void *m_arg;
};

/* the handler of a dialog a call waits for: the button is its result */
class DialogWaitHandler : public CommonGuiDialogHandler {
public:
	virtual void operator()(SceInt32 value, SceInt32 button)
	{
		SceCommonGuiDialog_2CA16BBF(value, button);
		SceCommonGuiDialog_BC311DC0(value);
	}
};

CommonGuiDialogParam *SceCommonGuiDialog_3EA370C9(CommonGuiDialogParam *param)
{
	ColorV4(param->bgColor) = SceCommonGuiDialog_EF2C5382;
	ColorV4(param->dimmerColor) = SceCommonGuiDialog_EF2C5382;
	param->soundId = 0;
	param->dimmer = 0;
	param->priority = -1;
	param->waitTransition = false;
	param->notifyClose = false;
	param->dialogId = -1;
	param->outId = NULL;
	param->pageOption = DIALOG_PAGE_OPTION;
	param->resourcePlugin = Framework::Instance()->GetCommonResourcePlugin();
	return param;
}

CommonGuiDialogParam *SceCommonGuiDialog_696DB9C9(CommonGuiDialogParam *param)
{
	ColorV4(param->bgColor) = SceCommonGuiDialog_EF2C5382;
	ColorV4(param->dimmerColor) = SceCommonGuiDialog_EF2C5382;
	param->soundId = 0;
	param->dimmer = 0;
	param->priority = -1;
	param->waitTransition = false;
	param->notifyClose = false;
	param->dialogId = -1;
	param->outId = NULL;
	param->pageOption = DIALOG_PAGE_OPTION;
	param->resourcePlugin = Framework::Instance()->GetCommonResourcePlugin();
	return param;
}

CommonGuiDialogParam *CopyDialogParam(CommonGuiDialogParam *dst, const CommonGuiDialogParam *src)
{
	dst->resourcePlugin = src->resourcePlugin;
	ColorV4(dst->bgColor) = ColorV4(src->bgColor);
	ColorV4(dst->dimmerColor) = ColorV4(src->dimmerColor);
	dst->soundId = src->soundId;
	dst->dimmer = src->dimmer;
	dst->priority = src->priority;
	dst->waitTransition = src->waitTransition;
	dst->notifyClose = src->notifyClose;
	dst->dialogId = src->dialogId;
	dst->outId = src->outId;
	dst->pageOption = src->pageOption;
	return dst;
}

Widget *CommonGuiDialogTemplateFactory::Create(Widget *parent)
{
	int32_t index = parent->m_children.size();

	if (m_plugin->TemplateOpen(parent, m_template, Plugin::TemplateOpenParam()) == 0)
		return parent->GetChild(index);
	return NULL;
}

/* the dialog's scroll view, NULL without one */
static ScrollViewBase *GetScrollView(CommonGuiDialog *dialog)
{
	Widget *view;

	view = dialog->GetPart(DIALOG_PART_LIST_VIEW);
	if (view == NULL) {
		view = dialog->GetPart(DIALOG_PART_VIEW);
		if (view == NULL) {
			view = dialog->GetPart(DIALOG_PART_BOX);
			if (view != NULL)
				view = view->FindChild(IDParam((uint32_t)DIALOG_VIEW), 0);
		}
	}
	if (view != NULL && view->IsInherit(ScrollViewBase::TypeName()))
		return (ScrollViewBase *)view;
	return NULL;
}

void SceCommonGuiDialog_BC311DC0(int dialogId)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog == NULL)
		return;
	dialog->DeleteReadyListeners();
	if (dialog->m_refCount > 0)
		dialog->m_refCount--;
	if (dialog->m_refCount == 0) {
		ScrollViewBase *view = GetScrollView(dialog);

		if (view != NULL)
			view->m_clip_mode = 1;
		dialog->Close();
	}
}

int SceCommonGuiDialog_2CA16BBF(int dialogId, int result)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog == NULL)
		return -1;
	dialog->SetResult(result);
	return 0;
}

/* the dialog's heading, and the space above its contents fitted to it: 0, or an error */
static int32_t SetHeading(CommonGuiDialog *dialog, const wstring &heading)
{
	Widget *box = dialog->GetPart(DIALOG_PART_BOX);
	Widget *space;
	float height;
	int32_t ret;
	bool wide;

	if (box == NULL)
		return COMMON_GUI_DIALOG_ERROR_NO_DIALOG;
	wide = box->GetSize(0)->extract_x() >= DIALOG_WIDE_WIDTH;
	ret = dialog->m_dialog->SetPartsString(DIALOG_PART_HEADING, heading);
	if (ret != 0)
		return ret;
	if (heading.length() != 0) {
		dialog->ShowPart(DIALOG_PART_HEADING_LINE);
		height = 76.0f;
		if (wide)
			height = 64.0f;
	} else {
		dialog->HidePart(DIALOG_PART_HEADING_LINE);
		if (!wide) {
			height = 12.0f;
			if (dialog->GetPart(DIALOG_PART_CANCEL) != NULL)
				height = 76.0f;
		} else {
			height = 0.0f;
		}
	}
	space = box->FindChild(IDParam((uint32_t)DIALOG_HEADING_SPACE), 0);
	if (space != NULL)
		space->SetSize(math::v4(0.0f, height, 0.0f, 0.0f));
	return ret;
}

/*
 * Opens the dialog with the ID again, or a new one, with the layout, heading and text, which calls
 * the handler (taken over): its ID, or a negative error.
 */
static int32_t OpenDialog(Plugin *plugin, int32_t dialogId, int32_t priority, bool waitTransition,
			  bool notifyClose, int32_t dimmer, int32_t pageOption, const wstring &heading,
			  const wstring &text, const CommonGuiDialogLayout *layout,
			  CommonGuiDialogHandler *handler)
{
	CommonGuiDialog *dialog;
	ScrollViewBase *view;
	Widget *busy;
	bool reopened = false;
	float time;
	int32_t ret;

	dialog = CommonGuiDialog::Find(dialogId);
	if (dialog != NULL) {
		dialog->Hide();
		reopened = true;
	} else {
		dialog = new CommonGuiDialog();
	}
	if (dialog == NULL) {
		if (handler != NULL)
			delete handler;
		return COMMON_GUI_DIALOG_ERROR_NO_MEMORY;
	}
	dialog->SetHandler(handler);
	if (dialog->m_id < 0) {
		delete dialog;
		return -1;
	}
	ret = dialog->Open(plugin, priority, waitTransition, notifyClose, dimmer, pageOption,
			   layout->page, layout->parts, &time);
	if (ret != 0) {
		delete dialog;
		return ret;
	}
	SetHeading(dialog, heading);
	dialog->m_dialog->SetPartsString(1, text);
	view = GetScrollView(dialog);
	if (view != NULL) {
		view->SetScrollBarController(new DialogScrollBarController(dialog, view));
		view->m_clip_mode = 1;
		view->SetTimerListener(DIALOG_OPEN_TIMER, time, new DialogOpenListener(view));
	}
	busy = dialog->GetPart(DIALOG_PART_BOX)->FindChild(IDParam((uint32_t)DIALOG_BUSY), 0);
	if (busy != NULL && busy->IsInherit(BusyIndicator::TypeName()))
		((BusyIndicator *)busy)->Start();
	dialog->SetButtons(layout->buttons);
	ret = dialog->m_id;
	if (!reopened)
		dialog->m_refCount++;
	return ret;
}

int SceCommonGuiDialog_694696F3(void)
{
	return CommonGuiDialog::GetDefaultPriority();
}

void SceCommonGuiDialog_5057376C(int priority)
{
	CommonGuiDialog::SetDefaultPriority(priority);
}

int SceCommonGuiDialog_D2381B88(int dialogId)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog == NULL)
		return -1;
	return dialog->Wait();
}

void SceCommonGuiDialog_17091699(int dialogId, CommonGuiLayoutDialogOpenHandler *handler,
				 float interval)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog != NULL)
		dialog->SetInterval(handler, interval);
	else if (handler != NULL)
		delete handler;
}

int SceCommonGuiDialog_0BA67FE0(int dialogId, CommonGuiLayoutDialogOpenHandler *handler)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog == NULL) {
		if (handler != NULL)
			delete handler;
		return -1;
	}
	dialog->AddReadyListener(handler);
	return 0;
}

int SceCommonGuiDialog_D3F78BA5(int dialogId, SceUInt32 id)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog == NULL)
		return -1;
	dialog->m_soundId = id;
	return 0;
}

Widget *SceCommonGuiDialog_8947FC23(int dialogId, int index)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog == NULL)
		return NULL;
	return dialog->GetPart(index);
}

void SceCommonGuiDialog_206623FF(int dialogId, const math::v4 &color, const math::v4 &unused)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);
	Widget *part;

	if (dialog == NULL)
		return;
	part = dialog->GetPart(DIALOG_PART_DIMMER);
	if (part != NULL)
		part->SetColor(color);
}

void SceCommonGuiDialog_2BBC4734(int dialogId, const math::v4 &color)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);
	Widget *part;

	if (dialog == NULL)
		return;
	part = dialog->GetPart(DIALOG_PART_HEADING);
	if (part != NULL)
		part->SetColor(color);
}

/* the colour of the box's child with the ID */
static inline void SetChildColor(Widget *box, uint32_t id, const math::v4 &color)
{
	Widget *child = box->FindChild(IDParam(id), 0);

	if (child != NULL)
		child->SetColor(color);
}

void SceCommonGuiDialog_0C2B57DC(int dialogId, const math::v4 &color)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);
	Widget *box;

	if (dialog == NULL)
		return;
	box = dialog->GetPart(DIALOG_PART_BOX);
	if (box == NULL)
		return;
	SetChildColor(box, DIALOG_BACKGROUND, color);
	SetChildColor(box, DIALOG_DIVIDER_TOP, color);
	SetChildColor(box, DIALOG_DIVIDER_BOTTOM, color);
}

/* the image of the part's draw object */
static inline void SetPartTexture(int dialogId, int index, intrusive_ptr<graph::Surface> image,
				  uint32_t texture)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);
	Widget *part;

	if (dialog == NULL)
		return;
	part = dialog->GetPart(index);
	if (part != NULL)
		part->SetTexture(image, 0, texture);
}

/* the cached image of the part's draw object */
static inline void SetPartCacheImage(int dialogId, int index,
				     intrusive_ptr<cache::CacheImageInterface> image)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);
	Widget *part;

	if (dialog == NULL)
		return;
	part = dialog->GetPart(index);
	if (part != NULL)
		part->SetDrawObjMaskBit(&image, 0, 0);
}

void SceCommonGuiDialog_52AB228A(int dialogId, int index, intrusive_ptr<graph::Surface> image)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);
	Widget *part;

	if (dialog == NULL)
		return;
	part = dialog->GetPart(index);
	if (part != NULL)
		part->SetTexture(image, 0, 0);
}

int SceCommonGuiDialog_CCFCD9A6(CommonGuiLayoutDialogParam *param)
{
	CommonGuiDialogHandler *handler = param->handler;
	CommonGuiLayoutDialogOpenHandler *openHandler = param->openHandler;
	CommonGuiLayoutDialogOpenHandler *updateHandler = param->updateHandler;
	int id;

	if (param->wait) {
		if (handler != NULL)
			delete handler;
		handler = new DialogWaitHandler();
	}
	id = OpenDialog(param->resourcePlugin, param->dialogId, param->priority,
			param->waitTransition, param->notifyClose, param->dimmer, param->pageOption,
			param->heading, param->text, &param->layout, handler);
	if (id < 0) {
		SceCommonGuiDialog_BC311DC0(-1);
		if (openHandler != NULL)
			delete openHandler;
		if (updateHandler != NULL)
			delete updateHandler;
		return id;
	}
	if (param->outId != NULL)
		*param->outId = id;
	if (param->image.get() != NULL)
		SetPartTexture(id, DIALOG_PART_IMAGE, param->image, 0);
	if (param->cachedImage.get() != NULL)
		SetPartCacheImage(id, DIALOG_PART_IMAGE, param->cachedImage);
	if (param->secondImage.get() != NULL)
		SetPartTexture(id, DIALOG_PART_IMAGE, param->secondImage, 1);
	if (!(SceCommonGuiDialog_EF2C5382 == ColorV4(param->bgColor)))
		SceCommonGuiDialog_0C2B57DC(id, ColorV4(param->bgColor));
	if (!(SceCommonGuiDialog_EF2C5382 == param->textColor))
		SceCommonGuiDialog_2BBC4734(id, param->textColor);
	if (!(SceCommonGuiDialog_EF2C5382 == ColorV4(param->dimmerColor)))
		SceCommonGuiDialog_206623FF(id, ColorV4(param->dimmerColor),
					    ColorV4(param->dimmerColor));
	SceCommonGuiDialog_D3F78BA5(id, param->soundId);
	if (openHandler != NULL)
		SceCommonGuiDialog_0BA67FE0(id, openHandler);
	if (updateHandler != NULL)
		SceCommonGuiDialog_17091699(id, updateHandler, param->interval);
	if (param->wait)
		id = SceCommonGuiDialog_D2381B88(id);
	return id;
}

int SceCommonGuiDialog_FA31832E(Plugin *plugin, const wstring &heading, const wstring &text,
				const CommonGuiDialogLayout *layout, CommonGuiDialogHandler *handler)
{
	return OpenDialog(plugin, -1, -1, false, false, 0, DIALOG_PAGE_OPTION, heading, text,
			  layout, handler);
}

int SceCommonGuiDialog_6CE14F66(Plugin *plugin, const wstring &heading, const wstring &text,
				const CommonGuiDialogLayout *layout, CommonGuiDialogFunction func,
				void *arg)
{
	return OpenDialog(plugin, -1, -1, false, false, 0, DIALOG_PAGE_OPTION, heading, text,
			  layout, new DialogFunctionHandler(func, arg));
}

void SceCommonGuiDialog_D0B729B2(int dialogId)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog != NULL)
		dialog->m_refCount++;
}

void SceCommonGuiDialog_D222DCE5(int dialogId)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog == NULL)
		return;
	dialog->DeleteReadyListeners();
	if (dialog->m_refCount > 0)
		dialog->m_refCount--;
	if (dialog->m_refCount == 0) {
		ScrollViewBase *view = GetScrollView(dialog);

		if (view != NULL)
			view->m_clip_mode = 1;
		dialog->Close();
	}
}

int SceCommonGuiDialog_B1BA8D9E(int dialogId, CommonGuiDialogOpenFunction func, void *arg)
{
	return SceCommonGuiDialog_0BA67FE0(dialogId, new DialogOpenFunction(func, arg));
}

void SceCommonGuiDialog_F1A40838(int dialogId, CommonGuiDialogOpenFunction func, void *arg,
				 float interval)
{
	SceCommonGuiDialog_17091699(dialogId, new DialogIntervalFunction(func, arg), interval);
}

void SceCommonGuiDialog_47BEE7F8(int dialogId)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog != NULL)
		dialog->KillInterval();
}

void SceCommonGuiDialog_F9903161(int dialogId, bool on)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog != NULL)
		dialog->SetDebugMode(on);
}

int SceCommonGuiDialog_7775C6B6(int dialogId, const wstring &heading)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog == NULL)
		return COMMON_GUI_DIALOG_ERROR_NO_DIALOG;
	return SetHeading(dialog, heading);
}

int SceCommonGuiDialog_273A476A(int dialogId, const wstring &text)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog == NULL)
		return COMMON_GUI_DIALOG_ERROR_NO_DIALOG;
	return dialog->m_dialog->SetPartsString(1, text);
}

int SceCommonGuiDialog_22475C30(int dialogId, int index, const wstring &text)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog == NULL)
		return COMMON_GUI_DIALOG_ERROR_NO_DIALOG;
	return dialog->m_dialog->SetPartsString(index, text);
}

int SceCommonGuiDialog_6A6F5DEE(int dialogId, math::v4 *color)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);
	Widget *box;
	Widget *background;

	if (dialog == NULL)
		return COMMON_GUI_DIALOG_ERROR_NO_DIALOG;
	box = dialog->GetPart(DIALOG_PART_BOX);
	if (box == NULL)
		return COMMON_GUI_DIALOG_ERROR_NO_DIALOG;
	background = box->FindChild(IDParam((uint32_t)DIALOG_BACKGROUND), 0);
	if (background == NULL)
		return COMMON_GUI_DIALOG_ERROR_NO_DIALOG;
	*color = background->GetColor();
	return 0;
}

/* shows or hides the box's child with the ID */
static inline void ShowChild(Widget *box, uint32_t id, bool show)
{
	Widget *child = box->FindChild(IDParam(id), 0);

	if (child == NULL)
		return;
	if (show)
		ShowWidget(child, NULL);
	else
		HideWidget(child, NULL);
}

void SceCommonGuiDialog_052D0230(int dialogId, bool show)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);
	Widget *box;

	if (dialog == NULL)
		return;
	box = dialog->GetPart(DIALOG_PART_BOX);
	if (box == NULL)
		return;
	ShowChild(box, DIALOG_DIVIDER_TOP, show);
	ShowChild(box, DIALOG_DIVIDER_BOTTOM, show);
}

int32_t GetPartChecked(int32_t id, int32_t index, bool *checked)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(id);
	Widget *part;

	if (dialog == NULL)
		return COMMON_GUI_DIALOG_ERROR_NO_DIALOG;
	/* the check box's part, whatever the index (its only caller passes it) */
	part = dialog->GetPart(DIALOG_PART_CHECK_BOX);
	if (part == NULL || !part->IsInherit(StateButtonBase::TypeName()))
		return COMMON_GUI_DIALOG_ERROR_NO_DIALOG;
	if (checked != NULL)
		*checked = ((StateButtonBase *)part)->IsChecked();
	return 0;
}

int SceCommonGuiDialog_01702008(int dialogId, int result)
{
	CommonGuiDialog *dialog = CommonGuiDialog::Find(dialogId);

	if (dialog == NULL)
		return -1;
	dialog->Select(result);
	return 0;
}

int SceCommonGuiDialog_B59AE4AD(Plugin *plugin, const wstring &heading, const wstring &text,
				const CommonGuiDialogLayout *layout,
				CommonGuiLayoutDialogOpenHandler *handler)
{
	int id;

	id = OpenDialog(plugin, -1, -1, false, false, 0, DIALOG_PAGE_OPTION, heading, text, layout,
			new DialogWaitHandler());
	if (id < 0) {
		if (handler != NULL)
			delete handler;
		return id;
	}
	if (SceCommonGuiDialog_0BA67FE0(id, handler) != 0)
		return -1;
	return SceCommonGuiDialog_D2381B88(id);
}

int SceCommonGuiDialog_EB559AFE(Plugin *plugin, const wstring &heading, const wstring &text,
				const CommonGuiDialogLayout *layout, CommonGuiDialogOpenFunction func,
				void *arg)
{
	return SceCommonGuiDialog_B59AE4AD(plugin, heading, text, layout,
					   new DialogOpenFunction(func, arg));
}

int SceCommonGuiDialog_061DD12B(int dialogId, int index, CommonGuiDialogWidgetFactory *factory)
{
	CommonGuiDialog *dialog;
	Widget *part;
	Widget *parent;
	Widget *widget;

	if (factory == NULL)
		return -1;
	dialog = CommonGuiDialog::Find(dialogId);
	if (dialog != NULL && (part = dialog->GetPart(index)) != NULL &&
	    (parent = part->m_parent) != NULL && (widget = factory->Create(parent)) != NULL) {
		widget->SetParent(parent, part, 0);
		delete factory;
		return 0;
	}
	delete factory;
	return -1;
}

void DialogScrollBarController::UpdateRange(const ScrollViewBarRange &range)
{
	Widget *box = m_dialog->GetPart(DIALOG_PART_BOX);
	Widget *space;
	Widget *contents;
	ScrollView *view;
	Text *text;
	math::v4 size;
	float spaceHeight;
	float height;
	float contentsHeight;
	bool wide;

	if (box == NULL)
		return;
	wide = box->GetSize(0)->extract_x() >= DIALOG_WIDE_WIDTH;
	spaceHeight = 0.0f;
	space = box->FindChild(IDParam((uint32_t)DIALOG_HEADING_SPACE), 0);
	if (space != NULL)
		spaceHeight = space->GetSize(0)->extract_y();
	ScrollViewScrollBarController::UpdateRange(range);
	view = NULL;
	if (m_view != NULL && m_view->IsInherit(ScrollView::TypeName()))
		view = (ScrollView *)m_view;
	if (view != NULL) {
		Widget *first = view->GetInternalWidget()->GetChild(0);

		if (first == NULL || first->m_state != 0)
			return;
	}
	size = *m_view->GetSize(0);
	height = size.extract_y();
	if (view != NULL && m_height != height) {
		text = NULL;
		if (m_dialog->GetPart(1) != NULL && m_dialog->GetPart(1)->IsInherit(Text::TypeName()))
			text = (Text *)m_dialog->GetPart(1);
		contentsHeight = height;
		contents = m_dialog->GetPart(DIALOG_PART_CONTENTS);
		if (contents != NULL)
			contentsHeight = contents->GetSize(0)->extract_y();
		if (wide && text != NULL) {
			bool buttons;
			int32_t textTop;
			int32_t minTop;
			uint32_t lines;
			float top;
			float bottom;

			view->SetAutoBound(false);
			buttons = m_dialog->GetPart(DIALOG_PART_BUTTONS) != NULL;
			textTop = buttons ? 172 : 204;
			text->GetLineCount(lines);
			bottom = (float)textTop - (float)(lines - 1) * 20.0f;
			minTop = spaceHeight > 0.0f ? 124 : 51;
			top = (float)minTop;
			if (top < bottom)
				top = bottom;
			if (contentsHeight < height)
				contentsHeight = height;
			top -= spaceHeight + 32.0f;
			if (m_view->GetMovableDirection(false) != 0)
				contentsHeight += (float)(buttons ? 34 : 28);
			view->SetManualViewBox(math::v4(size.extract_x() * -0.5f, top,
							size.extract_x() * 0.5f, top - contentsHeight));
		} else {
			view->SetAutoBound(true);
			m_view->SetViewFocus(math::v2(0.0f, height * -0.5f), NULL, true, true);
		}
		m_height = height;
	}
	if (m_view->GetMovableDirection(false) != 0) {
		m_view->m_clip_children = true;
		m_view->SetSnapTime(-1.0f);
	} else {
		m_view->m_clip_children = false;
		m_view->SetSnapTime(0.0f);
	}
}

int32_t DialogOpenListener::Do(int32_t type, Handler *self, Event *e)
{
	if (m_view->GetMovableDirection(false) != 0) {
		math::rect clip = m_view->CalcClipRect();
		float top = clip.top();
		float bottom = clip.bottom();
		uint32_t mode = 1;

		top += 16.0f;
		if ((float)((int32_t)top / 32 * 32) == top) {
			bottom += 16.0f;
			if ((float)((int32_t)bottom / 32 * 32) == bottom)
				mode = 0;
		}
		m_view->m_clip_mode = mode;
	}
	return 0;
}

DialogFunctionHandler::~DialogFunctionHandler()
{
}

void DialogFunctionHandler::operator()(SceInt32 value, SceInt32 button)
{
	if (m_func != NULL)
		m_func(value, button, m_arg);
}

DialogIntervalFunction::~DialogIntervalFunction()
{
}

void DialogIntervalFunction::operator()(SceInt32 id)
{
	if (m_func != NULL)
		m_func(id, m_arg);
}

const math::v4 SceCommonGuiDialog_EF2C5382 = -math::v4(1.0f, 1.0f, 1.0f, 1.0f);
