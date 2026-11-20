/*
 * The module's start and stop, and the video player's dialog: two images with their titles, a
 * check box and its buttons. No binary names this file, its objects or the dialog; the names are
 * ours.
 */
#include <stdint.h>
#include <std/new>
#include <graphics/drawobj/draw_obj.h>
#include "dialog.h"

/* the C library's atexit functions (paf's declarations, with C linkage) */
extern "C" {
#include <src/std/stdc++/atexit.h>
}

using namespace paf;
using namespace paf::ui;

/* the video dialog's parts */
#define VIDEO_DIALOG_PART_HEADING	6
#define VIDEO_DIALOG_PART_CHECK_BOX	15
#define VIDEO_DIALOG_PART_FIRST_TITLE	24
#define VIDEO_DIALOG_PART_FIRST_IMAGE	25
#define VIDEO_DIALOG_PART_FIRST_ICON	26
#define VIDEO_DIALOG_PART_SECOND_TITLE	27
#define VIDEO_DIALOG_PART_SECOND_DETAIL	28
#define VIDEO_DIALOG_PART_SECOND_IMAGE	29
#define VIDEO_DIALOG_PART_SECOND_ICON	30

/* the check box's label, a child of the check box's parent */
#define VIDEO_DIALOG_CHECK_BOX_LABEL	0x4DE01ED9

/* the gap between an image and the icon placed after it */
#define VIDEO_DIALOG_ICON_GAP		6.0f

extern "C" void *__dso_handle;
void *__dso_handle;

/* SNC's linker-provided constructor bounds */
extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

static __attribute__((noinline)) void commonGuiDialogInit(void)
{
	void (**init)(void);

	__dso_handle = &__dso_handle;
	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
}

static __attribute__((noinline)) void commonGuiDialogTerm(void)
{
	__cxa_finalize(&__dso_handle);
}

extern "C" int module_start(SceSize argSize, const void *argp)
{
	commonGuiDialogInit();
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	commonGuiDialogTerm();
	return 0;
}

int SceCommonGuiDialog_ED620EF1(int dialogId, bool *checked)
{
	return GetPartChecked(dialogId, VIDEO_DIALOG_PART_CHECK_BOX, checked);
}

/* the image laid out: the icon after it placed beside its right edge */
class VideoDialogIconListener : public EventListener {
public:
	VideoDialogIconListener(int32_t id, int32_t index) : m_id(id), m_index(index)
	{
	}

	virtual int32_t Do(int32_t type, Handler *self, Event *e)
	{
		Widget *icon = SceCommonGuiDialog_8947FC23(m_id, m_index);

		if (icon != NULL) {
			graph::DrawObjBounds bounds = ((Widget *)self)->GetDrawObj(0)->GetBounds();
			math::v4 pos = icon->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

			pos.set_x((bounds.m_max - bounds.m_min).extract_x() + VIDEO_DIALOG_ICON_GAP);
			icon->SetPos(pos);
		}
		return 0;
	}

	int32_t m_id;
	int32_t m_index;
};

/* the dialog's parts and buttons, by its type */
static Dialog::PartsList s_videoParts[] = {
	{ 1, IDParam((uint32_t)0xD0791765) },
	{ VIDEO_DIALOG_PART_FIRST_TITLE, IDParam((uint32_t)0x51DF3C38) },
	{ VIDEO_DIALOG_PART_FIRST_IMAGE, IDParam((uint32_t)0x2A35777A) },
	{ VIDEO_DIALOG_PART_FIRST_ICON, IDParam((uint32_t)0xB5200162) },
	{ VIDEO_DIALOG_PART_SECOND_TITLE, IDParam((uint32_t)0x5A364BDC) },
	{ VIDEO_DIALOG_PART_SECOND_DETAIL, IDParam((uint32_t)0xB3521FB7) },
	{ VIDEO_DIALOG_PART_SECOND_IMAGE, IDParam((uint32_t)0x77B94BFE) },
	{ VIDEO_DIALOG_PART_SECOND_ICON, IDParam((uint32_t)0xB4C3CCBE) },
	{ VIDEO_DIALOG_PART_CHECK_BOX, IDParam((uint32_t)0x8F6754C3) },
	{ 12, IDParam((uint32_t)0x5049F5E4) },
	{ 13, IDParam((uint32_t)0xBD559A97) },
	{ 14, IDParam((uint32_t)0xBCFBF95C) },
	{ 0, IDParam() },
};

static const CommonGuiDialogPage s_videoPage = {
	{ 0x3A918C2C, 0x438C7285, 0x90DA7BD2 }
};

static const CommonGuiDialogPage s_videoPageThree = {
	{ 0x3A918C2C, 0x438C7285, 0xD0A279E9 }
};

static const CommonGuiDialogButton s_videoButtons[] = {
	{ 12, 0x9B260EEF, 12, 0xC17D7980 },
	{ 13, 0x35C6DBDC, 11, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static const CommonGuiDialogButton s_videoButtonsSecond[] = {
	{ 12, 0x9B260EEF, 12, 0xC17D7980 },
	{ 13, 0x813136D5, 10, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static const CommonGuiDialogButton s_videoButtonsThree[] = {
	{ 12, 0x9B260EEF, 12, 0xC17D7980 },
	{ 13, 0x35C6DBDC, 11, 0x20413274 },
	{ 14, 0x813136D5, 10, 0x20413274 },
	{ 0, 0, 0, 0 },
};

static CommonGuiDialogLayout s_videoLayout = {
	&s_videoPage, s_videoParts, s_videoButtons
};

static CommonGuiDialogLayout s_videoLayoutSecond = {
	&s_videoPage, s_videoParts, s_videoButtonsSecond
};

static CommonGuiDialogLayout s_videoLayoutThree = {
	&s_videoPageThree, s_videoParts, s_videoButtonsThree
};

/* the part shown or hidden at once */
static inline void ShowPartNow(Widget *part, bool show)
{
	if (show)
		ShowWidget(part, NULL);
	else
		HideWidget(part, NULL);
}

/* the image's pictures: the cached picture, the picture and the icon */
static inline void SetImage(int id, int imageIndex, int iconIndex,
			    const CommonGuiVideoDialogImage &image)
{
	Widget *part = SceCommonGuiDialog_8947FC23(id, imageIndex);

	if (part != NULL)
		part->SetDrawObjMaskBit(&image.cachedImage, 0, 0);
	part = SceCommonGuiDialog_8947FC23(id, imageIndex);
	if (part != NULL)
		part->SetTexture(image.image);
	part = SceCommonGuiDialog_8947FC23(id, iconIndex);
	if (part != NULL)
		part->SetTexture(image.icon);
}

static inline void SetString(int id, int index, const wstring &text)
{
	Widget *part = SceCommonGuiDialog_8947FC23(id, index);

	if (part != NULL)
		part->SetString(text);
}

int SceCommonGuiDialog_5B9511E9(const CommonGuiVideoDialogParam *param)
{
	CommonGuiLayoutDialogParam layoutParam;
	int id;

	CopyDialogParam(&layoutParam, param);
	layoutParam.text = param->text;
	switch (param->type) {
	case 0:
		layoutParam.layout = s_videoLayout;
		break;
	case 1:
		layoutParam.layout = s_videoLayoutSecond;
		break;
	case 2:
		layoutParam.layout = s_videoLayoutThree;
		break;
	}
	layoutParam.handler = param->handler;
	id = SceCommonGuiDialog_CCFCD9A6(&layoutParam);
	if (id < 0)
		return id;

	SetString(id, VIDEO_DIALOG_PART_HEADING, param->heading);
	SetString(id, VIDEO_DIALOG_PART_FIRST_TITLE, param->firstTitle);
	SetImage(id, VIDEO_DIALOG_PART_FIRST_IMAGE, VIDEO_DIALOG_PART_FIRST_ICON, param->firstImage);
	SetString(id, VIDEO_DIALOG_PART_SECOND_TITLE, param->secondTitle);
	SetString(id, VIDEO_DIALOG_PART_SECOND_DETAIL, param->secondDetail);
	SetImage(id, VIDEO_DIALOG_PART_SECOND_IMAGE, VIDEO_DIALOG_PART_SECOND_ICON,
		 param->secondImage);

	Widget *checkBox = SceCommonGuiDialog_8947FC23(id, VIDEO_DIALOG_PART_CHECK_BOX);

	if (checkBox != NULL) {
		ShowPartNow(checkBox, param->checkBox);

		Widget *parent = checkBox->m_parent;

		if (parent != NULL) {
			Widget *label = parent->FindChild(
				IDParam(s_empty_name, VIDEO_DIALOG_CHECK_BOX_LABEL), 0);

			if (label != NULL)
				ShowPartNow(label, param->checkBox);
		}
	}

	Widget *image = SceCommonGuiDialog_8947FC23(id, VIDEO_DIALOG_PART_FIRST_IMAGE);

	if (image != NULL)
		image->AddEventListener(Handler::CB_STATE_READY,
					new VideoDialogIconListener(id, VIDEO_DIALOG_PART_FIRST_ICON));
	image = SceCommonGuiDialog_8947FC23(id, VIDEO_DIALOG_PART_SECOND_IMAGE);
	if (image != NULL)
		image->AddEventListener(Handler::CB_STATE_READY,
					new VideoDialogIconListener(id, VIDEO_DIALOG_PART_SECOND_ICON));
	return id;
}

void SceCommonGuiDialog_C4601FBE(int dialogId)
{
	SceCommonGuiDialog_BC311DC0(dialogId);
}
