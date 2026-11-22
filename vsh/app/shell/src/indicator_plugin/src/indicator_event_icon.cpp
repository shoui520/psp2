/*
 * indicator_event_icon.cpp: the item of the notices' list with an icon: its drawer made on the
 * list's widget, its icon loaded through the cache from a file, the event's own icon data or
 * the icon of the title the event runs, its title, its time and its frame, and its title
 * scrolled while it is focused (0x8117B780-0x8117E0BC). The file's and all names here are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <common/shared_ptr.h>
#include <common/string_util.h>
#include <job/job.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/environment.h>
#include <widget/w_composite_drawer.h>
#include <widget/w_appicon_simple.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include "indicator.h"

using namespace paf;

/* the drawer's events the item is laid out again on (PLACEHOLDER names) */
#define INDICATOR_ICON_ON_LAYOUT	0x100004
#define INDICATOR_ICON_ON_RESIZE	0x100008

/* the icon's texture made and not made */
#define INDICATOR_ICON_ON_READY		0x100005
#define INDICATOR_ICON_ON_ERROR		0x100006

/* the drawer's style, the title's string and the icon of the item without one (their IDs'
   hashes) */
#define INDICATOR_ICON_STYLE		0xC3ABA10C
#define INDICATOR_ICON_TITLE		0xEB3CAA2D
#define INDICATOR_ICON_NONE		0xFF2C073F

/* the frame's texture, in the list's mode 2 and in the others */
#define INDICATOR_ICON_FRAME_SMALL	0xFACFB344
#define INDICATOR_ICON_FRAME		0x94A74EDF

/* the icon an event of act type 11 (a web page's) shows */
#define INDICATOR_ICON_BROWSER		"vs0:app/NPXS10002/sce_sys/icon0.png"

/* the side of the square the icon's image is cached at */
#define INDICATOR_ICON_IMAGE_SIZE	64

/* the scene's state of a drawer closing, when it is not laid out again */
#define INDICATOR_ICON_SCENE_CLOSING	3


/* the item's drawer on its widget */
static inline ui::Widget *indicatorIconFindDrawer(ui::Widget *widget)
{
	return widget->FindChild(IDParam("composite_item"), 0);
}

static void indicatorIconOnLayout(int32_t type, ui::Handler *self, ui::Event *e, void *data);
static void indicatorIconOnIconInfo(IndicatorIconInfoMap *icons, int result, void *arg);
static common::SharedPtr<File> indicatorIconOpenData(const char *path, void *userdata,
						     int32_t *error);

/* the cache's option of the item's icon, its image scaled to the size given (0: as it is) */
static inline void indicatorIconOption(cache::CacheImage::Option *option, int32_t size)
{
	option->auto_delete = true;
	option->retry = true;
	option->retry_count = -1;
	option->reserved = true;
	option->read_size = 0;
	option->read_offset = 0;
	option->max_size_kb = 0;
	option->open_func = NULL;
	option->open_userdata = NULL;
	memset(&option->sub_rect, 0, sizeof(option->sub_rect));
	option->scale = image::ImageExtent(size, size);
	option->resize_type = image::ImageResizeType_NearestNeighbor;
	option->direct_load = false;
	option->image_order = ImageOrder_NONE;
	option->image_order_param = 0;
	option->check_image = NULL;
	option->check_image_userdata = NULL;
	option->surface_flag = false;
}

/* (0x8117B780) */
int IndicatorEventIconItem::Release()
{
	if (m_icon != NULL) {
		m_icon->DeleteEventCallback(INDICATOR_ICON_ON_READY, OnIconReady, this);
		m_icon->DeleteEventCallback(INDICATOR_ICON_ON_ERROR, OnIconError, this);
		m_icon = NULL;
	}

	ui::Widget *drawer = INDICATOR_ICON_DRAWER(m_widget);

	if (drawer != NULL) {
		drawer->DeleteEventCallback(INDICATOR_ICON_ON_LAYOUT, indicatorIconOnLayout, this);
		drawer->DeleteEventCallback(INDICATOR_ICON_ON_RESIZE, indicatorIconOnLayout, this);
	}
	if (m_scroll != NULL) {
		delete m_scroll;
		m_scroll = NULL;
	}
	if (m_iconJob.get() != NULL) {
		m_iconJob->Cancel();
		((JobGetIconInfo *)m_iconJob.get())->m_callback = NULL;
		m_iconJob.reset();
	}
	return 0;
}

/* (0x8117BA42) */
IndicatorEventIconItem::~IndicatorEventIconItem()
{
	Release();
}

/* the item's drawer made on its widget and laid out again on its events, the widget's look,
   the title, the icon, the time and the frame set, and the title's scroll made (0x8117BAF8) */
int IndicatorEventIconItem::Attach(ui::Widget *widget, IndicatorEventItemLook look)
{
	int ret = IndicatorEventItem::Attach(widget, look);

	if (ret != 0)
		return ret;

	ui::Widget *drawer = g_indicatorPlugin->CreateWidget(
		widget, "composite_drawer", IDParam("composite_item"),
		IDParam(s_empty_name, INDICATOR_ICON_STYLE));

	if (drawer == NULL)
		return ret;

	drawer->SetOrigin(drawer->m_origin_x, 2, drawer->m_origin_z, NULL);
	drawer->SetAlign(drawer->m_align_x, 2, drawer->m_align_z, NULL);
	drawer->m_draw_before_children = false;
	drawer->AddEventListener(INDICATOR_ICON_ON_LAYOUT,
				 new ui::EventCBListener(indicatorIconOnLayout, this), false);
	ret = drawer->AddEventListener(INDICATOR_ICON_ON_RESIZE,
				       new ui::EventCBListener(indicatorIconOnLayout, this), false);
	SetStyle(widget);
	SetText(drawer, IDParam(s_empty_name, INDICATOR_ICON_TITLE));
	SetIcon(drawer, IDParam("listbase_list_item_icon"), m_event.iconPath.c_str());
	SetTime(drawer);
	SetIconFrame(drawer);
	m_scroll = new IndicatorTextScroll(drawer->m_scene);
	return ret;
}

/* (0x8117BE28) */
int IndicatorEventIconItem::Detach()
{
	IndicatorEventItem::Detach();
	return Release();
}

/* the icon's texture made: the file's option let go and the icon shown (0x8117BE38) */
void IndicatorEventIconItem::OnIconReady(int32_t type, ui::Handler *self, ui::Event *e,
					 void *data)
{
	IndicatorEventIconItem *item = (IndicatorEventIconItem *)data;

	if (item == NULL)
		return;
	if (item->m_iconPath.length() != 0) {
		cache::CacheImage::Option option;

		item->m_iconPath.clear();
		indicatorIconOption(&option, 0);
		item->m_iconOption = option;
	}

	ui::Widget *icon = (ui::Widget *)self;

	if (icon != NULL && icon->IsInherit(ui::Widget::TypeName())) {
		item->ShowIcon(icon);
		icon->Show(0xff, new Timer(20.0f), NULL, 0.0f);
	}
}

/* the icon's texture not made: the file's image cached once more, else the icon's texture
   let go and the item's own listened to (0x8117BF7A) */
void IndicatorEventIconItem::OnIconError(int32_t type, ui::Handler *self, ui::Event *e,
					 void *data)
{
	IndicatorEventIconItem *item = (IndicatorEventIconItem *)data;
	ui::Widget *icon = (ui::Widget *)self;

	if (item == NULL || icon == NULL || !icon->IsInherit(ui::Widget::TypeName()))
		return;
	if (item->m_iconPath.length() != 0) {
		indicatorCacheMakeSpace();

		intrusive_ptr<ui::TextureSource> mask = icon->GetDrawObjMaskBit(0);

		/* the cache's image is a texture source of the widget: one class under two names */
		indicatorCacheCreateFile(item->m_iconPath,
					 *(intrusive_ptr<cache::CacheImageInterface> *)&mask,
					 &item->m_iconOption, NULL);
		icon->SetDrawObjMaskBit(&mask, 0, 0);
		item->m_iconPath.clear();

		cache::CacheImage::Option option;

		indicatorIconOption(&option, 0);
		item->m_iconOption = option;
	} else {
		intrusive_ptr<ui::TextureSource> mask = icon->GetDrawObjMaskBit(0);

		mask.clear();
		icon->SetDrawObjMaskBit(&mask, 0, 0);
		item->ListenIcon(icon);
	}
}

/* the icon shown, dimmed in the list's mode 0 and lit for the event's own icon data
   (0x8117C0A4) */
void IndicatorEventIconItem::ShowIcon(ui::Widget *icon)
{
	if (icon == NULL || !icon->IsInherit(ui::AppIconSimple::TypeName()))
		return;
	if (m_event.iconDataSize != 0)
		((ui::AppIconSimple *)icon)->SetAppearance(2);
	else if (GetMode() == 0)
		((ui::AppIconSimple *)icon)->SetAppearance(3);
	else
		((ui::AppIconSimple *)icon)->SetAppearance(0);
	icon->Show(0xff, new Timer(200.0f), NULL, 0.0f);
}

/* the item's icon of none set from the plugin's texture, the icon listened to (0x8117C188) */
void IndicatorEventIconItem::ListenIcon(ui::Widget *icon)
{
	if (icon == NULL || !icon->IsInherit(ui::AppIconSimple::TypeName()))
		return;

	intrusive_ptr<cache::CacheImageInterface> image;
	IDParam texture(s_empty_name, INDICATOR_ICON_NONE);
	int ret;

	{
		IDParam id = texture;

		ret = indicatorCacheCreateTexture(g_indicatorPlugin, id, image, NULL, NULL);
	}
	if (ret != 0)
		return;
	icon->AddEventListener(INDICATOR_ICON_ON_READY,
			       new ui::EventCBListener(OnIconReady, this), true);
	icon->AddEventListener(INDICATOR_ICON_ON_ERROR,
			       new ui::EventCBListener(OnIconError, this), true);
	icon->SetDrawObjMaskBit(&image, 0, 0);
}

/* (0x8117C34A) */
int IndicatorEventIconItem::LoadIconInfo()
{
	vector<string> titleIds;

	titleIds.push_back(string(m_event.execTitleId.c_str()));
	m_iconJob = indicatorIconInfoJobStart(&titleIds, indicatorIconOnIconInfo, this);
	if (m_event.actType == 11)
		m_event.iconPath = INDICATOR_ICON_BROWSER;
	return 0;
}

/* (0x8117C542) */
int IndicatorEventIconItem::LoadIconFile(ui::Widget *icon, const string &path)
{
	if (icon == NULL || !icon->IsInherit(ui::AppIconSimple::TypeName()))
		return -1;

	intrusive_ptr<cache::CacheImageInterface> image;
	cache::CacheImage::Option option;

	indicatorIconOption(&option, INDICATOR_ICON_IMAGE_SIZE);

	int ret = indicatorCacheCreateFile(path, image, &option, NULL);

	if (ret != 0)
		return ret;
	m_iconPath = path;
	m_iconOption = option;
	icon->AddEventListener(INDICATOR_ICON_ON_READY,
			       new ui::EventCBListener(OnIconReady, this), true);
	icon->AddEventListener(INDICATOR_ICON_ON_ERROR,
			       new ui::EventCBListener(OnIconError, this), true);
	icon->SetDrawObjMaskBit(&image, 0, 0);
	return ret;
}

/* (0x8117C6B4) */
string IndicatorEventIconItem::GetIconDataPath(const LsdbNewEvent *event)
{
	string prefix("newevtdb:");
	char rowid[64];

	sce_paf_snprintf(rowid, sizeof(rowid), "%lld", event->rowid);
	return prefix + string(rowid);
}

/* (0x8117C7EE) */
int IndicatorEventIconItem::LoadIconData(ui::Widget *icon, const LsdbNewEvent *event)
{
	if (icon == NULL || !icon->IsInherit(ui::AppIconSimple::TypeName()))
		return -1;

	cache::CacheImage::Option option;

	indicatorIconOption(&option, INDICATOR_ICON_IMAGE_SIZE);
	option.open_func = indicatorIconOpenData;

	intrusive_ptr<cache::CacheImageInterface> image;
	string path = GetIconDataPath(event);
	int ret = indicatorCacheCreateFile(path, image, &option, NULL);

	if (ret != 0)
		return ret;
	m_iconPath = path;
	m_iconOption = option;
	icon->AddEventListener(INDICATOR_ICON_ON_READY,
			       new ui::EventCBListener(OnIconReady, this), true);
	icon->AddEventListener(INDICATOR_ICON_ON_ERROR,
			       new ui::EventCBListener(OnIconError, this), true);
	icon->SetDrawObjMaskBit(&image, 0, 0);
	return ret;
}

/* the item's icon made on the drawer, placed by the list's mode and hidden, and its image
   loaded: the event's own icon data, its file, or the icon of the title it runs, else the
   icon of none (0x8117C9AA) */
int IndicatorEventIconItem::SetIcon(ui::Widget *widget, IDParam id, const char *path)
{
	int ret = -1;
	string iconPath(path);

	if (m_icon == NULL) {
		m_icon = new ui::AppIconSimple(widget, NULL);
		if (m_icon == NULL)
			return ret;
		m_icon->m_name = IDParam("listbase_list_item_icon");
	}

	int mode = GetMode();

	if (mode == 2) {
		m_icon->SetPos(math::v4(-175.0f, -26.0f, 0.0f, 0.0f));
		m_icon->SetSize(math::v4(40.0f, 40.0f, 0.0f, 0.0f));
	} else if (mode == 1) {
		m_icon->SetPos(math::v4(-339.0f, -46.0f, 0.0f, 0.0f));
		m_icon->SetSize(math::v4(64.0f, 64.0f, 0.0f, 0.0f));
	} else {
		m_icon->SetPos(math::v4(-297.0f, -37.0f, 0.0f, 0.0f));
		m_icon->SetSize(math::v4(56.0f, 56.0f, 0.0f, 0.0f));
	}
	m_icon->Hide(0xff, NULL, NULL, 0.0f);
	m_iconPath.clear();
	if (m_event.iconDataSize != 0)
		ret = LoadIconData(m_icon, &m_event);
	else if (iconPath.length() != 0)
		ret = LoadIconFile(m_icon, iconPath);
	else if (indicatorNoticeIsShown(&m_event) && m_event.execTitleId.length() != 0 &&
		 iconPath.length() == 0)
		ret = LoadIconInfo();
	else
		ListenIcon(m_icon);
	return ret;
}

/* the frame put on the icon of a notified event, placed by the list's mode (0x8117CCFA) */
int IndicatorEventIconItem::SetIconFrame(ui::Widget *widget)
{
	if (!IsNotified() || widget == NULL ||
	    !widget->IsInherit(ui::CompositeDrawer::TypeName()))
		return 0;

	graph::PlaneObj *frame = INDICATOR_ICON_PLANE(widget, INDICATOR_ICON_OBJ_FRAME);

	if (frame == NULL) {
		common::SharedPtr<graph::DrawObj> obj(new graph::PlaneObj(graph::PlaneObjStyle()));

		((ui::CompositeDrawer *)widget)->InsertDrawObj(obj, INDICATOR_ICON_OBJ_FRAME, false);
		frame = obj.get() != NULL && obj->IsInherit(graph::PlaneObj::DrawObjType()) ?
				(graph::PlaneObj *)obj.get() : NULL;
	}
	if (frame == NULL)
		return 0;

	int mode = GetMode();
	IDParam texture(s_empty_name,
			mode == 2 ? INDICATOR_ICON_FRAME_SMALL : INDICATOR_ICON_FRAME);

	frame->SetVisible(false);
	frame->SetTexture(g_indicatorPlugin->GetTexture(texture), 0);

	math::v3 pos;

	if (mode == 2)
		pos = math::v3(-134.0f, -19.0f, 0.0f);
	else if (mode == 1)
		pos = math::v3(-284.0f, -45.0f, 0.0f);
	else
		pos = math::v3(-250.0f, -45.0f, 0.0f);
	frame->SetScaleMode(graph::PlaneObj::SCALE_SURFACE, graph::PlaneObj::SCALE_SURFACE);
	frame->SetPos(pos);
	return 0;
}

/* the item's title: a description's or title's text for msgType 256 and 258, else the event's
   message, placed by the list's mode and whether the event is notified (0x8117CF64) */
int IndicatorEventIconItem::SetText(ui::Widget *widget, IDParam id)
{
	int ret = -1;

	if (widget != NULL) {
		common::String str(L"");

		if (widget->IsInherit(ui::CompositeDrawer::TypeName())) {
			graph::TextObj *text =
				INDICATOR_ICON_TEXT(widget, INDICATOR_ICON_OBJ_TITLE);

			if (text != NULL) {
				text->SetVisible(false);
				text->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, false);

				unsigned int notified = IsNotified();
				int mode = GetMode();

				if (mode == 2) {
					float width = 337.0f;

					if (notified)
						width -= 30.0f;

					float x = width * 0.5f - 148.0f;

					if (notified)
						x += 30.0f;
					text->SetSize(math::v3(width, 0.0f, 0.0f));
					text->SetPos(math::v3(x, -26.0f, 0.0f));
					text->SetLayoutAttribute(graph::TextLayoutAttribute_LinePitch, 20.0f);
					text->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0, 18.0f);
					text->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0, false);
					text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
								math::v4(0.43137255f, 0.43137255f, 0.43137255f, 1.0f));
					text->SetStyleAttribute(graph::TextStyleAttribute_EnlargeFontSize, 0, 0, false);
				} else {
					float width = 590.0f;

					if (notified)
						width -= 30.0f;

					float left = mode == 0 ? 261.0f : 298.0f;
					float x = width * 0.5f - left;

					if (notified)
						x += 30.0f;

					float height = mode != 0 ? 90.0f : 76.0f;

					text->SetSize(math::v3(width, 0.0f, 0.0f));
					text->SetPos(math::v3(x, height * 0.5f, 0.0f));
					text->SetLayoutAttribute(graph::TextLayoutAttribute_LinePitch, 26.0f);
					text->SetLayoutAttribute(graph::TextLayoutAttribute_EnlargeLinePitch, true);
					text->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0, true);
					if (mode == 0) {
						text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0, m_color);
						text->SetStyleAttribute(graph::TextStyleAttribute_Shadow, 0, 0, true);
					} else if (m_event.newFlag) {
						text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
									math::v4(0.98039216f, 0.98039216f, 0.98039216f,
										 1.0f));
					} else {
						text->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
									math::v4(0.98039216f, 0.98039216f, 0.98039216f,
										 1.0f));
					}
				}

				SceInt64 type = m_event.msgType;

				if (type == 256 || type == 258) {
					if (m_event.desc.length() != 0) {
						string desc(m_event.desc.c_str());
						wstring wdesc;

						common::Utf8ToUtf16(desc, &wdesc, 0);
						str = common::String(wdesc);
					} else if (m_event.title.length() != 0) {
						string title(m_event.title.c_str());
						wstring wtitle;

						common::Utf8ToUtf16(title, &wtitle, 0);
						str = common::String(wtitle);
					}
				} else {
					str = common::String(indicatorEventText(&m_event));
				}
				ret = text->SetString(str.GetWString());
			}
		}
	}
	return ret;
}

/* the event's time put under the item's icon in the list's mode 1 (0x8117D448) */
int IndicatorEventIconItem::SetTime(ui::Widget *widget)
{
	if (GetMode() != 1 || widget == NULL ||
	    !widget->IsInherit(ui::CompositeDrawer::TypeName()))
		return 0;

	graph::TextObj *time = INDICATOR_ICON_TEXT(widget, INDICATOR_ICON_OBJ_TIME);

	if (time == NULL) {
		common::SharedPtr<graph::DrawObj> obj(new graph::TextObj(graph::TextObjStyle()));

		((ui::CompositeDrawer *)widget)->InsertDrawObj(obj, INDICATOR_ICON_OBJ_TIME, false);
		time = obj.get() != NULL && obj->IsInherit(graph::TextObj::DrawObjType()) ?
			       (graph::TextObj *)obj.get() : NULL;
	}
	if (time == NULL)
		return 0;

	time->SetVisible(false);

	wstring text = indicatorTimeText(m_event.timeUpdated.c_str());

	if (text.length() != 0)
		time->SetString(text);
	time->SetSize(math::v3(210.0f, 0.0f, 0.0f));
	time->SetPos(math::v3(254.0f, -80.0f, 0.0f));
	time->SetLayoutAttribute(graph::TextLayoutAttribute_AlignX, 2);
	time->SetLayoutAttribute(graph::TextLayoutAttribute_TextAlign, 2);
	time->SetLayoutAttribute(graph::TextLayoutAttribute_AdjustY, true);
	time->SetLayoutAttribute(graph::TextLayoutAttribute_WordWrap, false);
	time->SetStyleAttribute(graph::TextStyleAttribute_Point, 0, 0, 16.0f);
	time->SetStyleAttribute(graph::TextStyleAttribute_EnlargeFontSize, 0, 0, false);
	time->SetStyleAttribute(graph::TextStyleAttribute_Bold, 0, 0, true);
	if (m_event.newFlag)
		time->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
					math::v4(0.98039216f, 0.98039216f, 0.98039216f, 1.0f));
	else
		time->SetStyleAttribute(graph::TextStyleAttribute_Color, 0, 0,
					math::v4(0.98039216f, 0.98039216f, 0.98039216f, 1.0f));
	return 0;
}

/* the title scrolled while the item is focused (0x8117D6E4) */
void IndicatorEventIconItem::OnFocus()
{
	ui::Widget *drawer = indicatorIconFindDrawer(m_widget) != NULL &&
				     indicatorIconFindDrawer(m_widget)
					     ->IsInherit(ui::CompositeDrawer::TypeName()) ?
			     indicatorIconFindDrawer(m_widget) : NULL;

	if (drawer == NULL)
		return;

	graph::TextObj *text = INDICATOR_ICON_TEXT(drawer, INDICATOR_ICON_OBJ_TITLE);

	if (text != NULL && m_scroll != NULL)
		m_scroll->Start(1, text);
}

/* the title put back in place (0x8117D902) */
void IndicatorEventIconItem::OnUnfocus()
{
	ui::Widget *drawer = indicatorIconFindDrawer(m_widget) != NULL &&
				     indicatorIconFindDrawer(m_widget)
					     ->IsInherit(ui::CompositeDrawer::TypeName()) ?
			     indicatorIconFindDrawer(m_widget) : NULL;

	if (drawer == NULL)
		return;

	graph::TextObj *text = INDICATOR_ICON_TEXT(drawer, INDICATOR_ICON_OBJ_TITLE);

	if (text != NULL && m_scroll != NULL)
		m_scroll->Stop(text);
}

/* the cache's file of the event's icon data, the row read from "newevtdb:<rowid>"
   (0x8117DB1E) */
static common::SharedPtr<File> indicatorIconOpenData(const char *path, void *userdata,
						     int32_t *error)
{
	IndicatorNewEventDb *db = indicatorNewEventDb();

	if (db == NULL)
		return common::SharedPtr<File>();

	const char *rowid = sce_paf_strchr(path, ':');

	if (rowid == NULL)
		return common::SharedPtr<File>();

	SceInt64 row = sce_paf_atoll(rowid + 1);
	int result = 0;
	common::SharedPtr<File> file = db->GetIcon(&row, &result);

	if (result < 0) {
		if (error != NULL)
			*error = result;
		return common::SharedPtr<File>();
	}
	return file;
}

/* the drawer laid out once it is drawn and its scene not closing (0x8117DCDC) */
static void indicatorIconOnLayout(int32_t type, ui::Handler *self, ui::Event *e, void *data)
{
	ui::Widget *drawer = (ui::Widget *)self;
	IndicatorEventIconItem *item = (IndicatorEventIconItem *)data;

	if (drawer->m_state == INDICATOR_ICON_SCENE_CLOSING || drawer->m_prepare_count != 0 ||
	    drawer->IsPrepareObjs())
		return;
	if (item != NULL)
		item->Layout(drawer, true);
}

/* the title placed by its lines, the frame beside it and the time below the icon
   (0x8117DD0C) */
int IndicatorEventIconItem::Layout(ui::Widget *widget, bool animate)
{
	if (widget == NULL || !widget->IsInherit(ui::CompositeDrawer::TypeName()))
		return 1;

	int mode = GetMode();
	graph::TextObj *text = INDICATOR_ICON_TEXT(widget, INDICATOR_ICON_OBJ_TITLE);

	if (text != NULL) {
		float y = mode == 2 ? -26.0f : -(mode == 0 ? 76.0f : 90.0f) * 0.5f;
		math::v4 size(text->m_size, 0.0f);
		math::v4 offset;

		AlignText(text, 4, size, &offset);

		math::v3 pos = text->m_pos;

		pos.set_y(y + offset.extract_y());
		text->SetPos(pos);
		text->SetVisible(true);

		graph::PlaneObj *frame = INDICATOR_ICON_PLANE(widget, INDICATOR_ICON_OBJ_FRAME);

		if (frame != NULL) {
			AlignText(text, 9, size, &offset);

			math::v3 framePos = frame->m_pos;

			framePos.set_y(pos.extract_y() - offset.extract_y());
			frame->SetPos(framePos);
			frame->SetVisible(true);
		}
	}

	graph::TextObj *time = INDICATOR_ICON_TEXT(widget, INDICATOR_ICON_OBJ_TIME);

	if (time != NULL) {
		math::v4 size(time->m_size, 0.0f);
		math::v4 offset;

		AlignText(time, 5, size, &offset);

		math::v3 pos = time->m_pos;

		pos.set_y(offset.extract_y() + -80.0f);
		time->SetPos(pos);
		time->SetVisible(true);
	}
	return 1;
}

/* (0x8117DFE2) */
void IndicatorEventIconItem::OnIconInfo(IndicatorIconInfoMap *icons, int result)
{
	IndicatorIconInfoMap::iterator it = icons->find(m_event.execTitleId);

	if (it != icons->end() && it->second->iconPath.length() != 0)
		m_event.iconPath = it->second->iconPath;
	if (m_event.iconPath.length() != 0)
		LoadIconFile(m_icon, m_event.iconPath);
	else
		ListenIcon(m_icon);
	m_iconJob.reset();
}

/* (0x8117E0AC) */
static void indicatorIconOnIconInfo(IndicatorIconInfoMap *icons, int result, void *arg)
{
	((IndicatorEventIconItem *)arg)->OnIconInfo(icons, result);
}
