/*
 * indicator_event_item.cpp: an item of the notices' list, the base of every kind: the new event
 * copied with its callback and serial number, the widget it is given told of its presses and
 * focus, what the event runs, the widget's look and the place of its text
 * (0x81174D48-0x8117587C). The file's and all names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <framework.h>
#include <std/string>
#include <math/ngp/v4.h>
#include <versatileparam/idparam.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/core/environment.h>
#include <widget/w_button.h>
#include <widget/w_scene.h>
#include <graphics/drawobj/text_obj.h>
#include <graphics/text/text_layout.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include "../../applauncher_plugin/src/applauncher.h"
#include "../../../plugins.h"
#include "../../livespace_db/lsdb_keys.h"
#include "indicator.h"

using namespace paf;

/* the widget's events the item listens to */
#define INDICATOR_EVENT_ITEM_ON_PRESSED		0x10000008
#define INDICATOR_EVENT_ITEM_ON_FOCUS_IN	0x40001
#define INDICATOR_EVENT_ITEM_ON_FOCUS_OUT	0x40002
#define INDICATOR_EVENT_ITEM_ON_LONG_PRESS	0x10000007
/* its scene's event the item listens to (as w_text's links do) */
#define INDICATOR_EVENT_ITEM_ON_SCENE		0x80000A

/* how long a press is for a long press, in milliseconds */
#define INDICATOR_EVENT_ITEM_LONG_PRESS		500

/* the widget's textures (their IDs' hashes) */
#define INDICATOR_EVENT_ITEM_BACK_NEW		0xB0B3F4AD
#define INDICATOR_EVENT_ITEM_BACK		0x0D60DC18
#define INDICATOR_EVENT_ITEM_FRAME		0xA753C6D6

/* the widget's draw object of the frame */
#define INDICATOR_EVENT_ITEM_FRAME_OBJ		68

/* what the event runs: its act type */
#define INDICATOR_EVENT_ACT_APPLAUNCHER		1	/* the title focused on the home screen */
#define INDICATOR_EVENT_ACT_LAUNCH		2	/* the title launched with its argument */
#define INDICATOR_EVENT_ACT_LIVEAREA		8	/* the title's LiveArea opened */
#define INDICATOR_EVENT_ACT_URI			11	/* the argument launched as a URI */

/* applauncher's and livearea's functions the item calls (PLACEHOLDER names) */
#define INDICATOR_APPLAUNCHER_FUNC_FOCUS	4
#define INDICATOR_LIVEAREA_FUNC_OPEN		11

/* the items made so far (0x815669E0) */
static SceInt64 s_eventItemSerial;

static void indicatorEventItemOnPressed(int32_t type, ui::Handler *self, ui::Event *e,
					void *data);
static void indicatorEventItemOnFocusIn(int32_t type, ui::Handler *self, ui::Event *e,
					void *data);
static void indicatorEventItemOnFocusOut(int32_t type, ui::Handler *self, ui::Event *e,
					 void *data);
static void indicatorEventItemOnLongPress(int32_t type, ui::Handler *self, ui::Event *e,
					  void *data);
static void indicatorEventItemOnScene(int32_t type, ui::Handler *self, ui::Event *e,
				      void *data);

/* (0x81174D48) */
IndicatorEventItem::IndicatorEventItem(const LsdbNewEvent &event, IndicatorEventItemCB callback)
	: m_event(event), m_callback(callback), m_mode(0),
	  m_color(0.0f, 0.0f, 0.0f, 0.0f), m_widget(NULL)
{
	m_serial = ++s_eventItemSerial;
}

/* (0x81174ECC) */
int IndicatorEventItem::Detach()
{
	if (m_widget != NULL)
		m_widget->m_scene->DeleteEventCallback(INDICATOR_EVENT_ITEM_ON_SCENE, NULL, NULL);
	return 0;
}

/* (0x81174EEA) */
IndicatorEventItem::~IndicatorEventItem()
{
	if (m_widget != NULL)
		m_widget->m_scene->DeleteEventCallback(INDICATOR_EVENT_ITEM_ON_SCENE, NULL, NULL);
}

/* (0x81174FFC) */
int IndicatorEventItem::Attach(ui::Widget *widget, int mode, SceUInt32 color)
{
	if (g_indicatorPlugin == NULL || widget == NULL)
		return -1;

	m_widget = widget;
	m_mode = mode;

	uint16x8_t rgba = vmovl_u8(vreinterpret_u8_u32(vdup_n_u32(color)));

	m_color = math::v4(sce::Vectormath::Simd::Aos::Vector4(
		vmulq_f32(vcvtq_f32_u32(vmovl_u16(vget_low_u16(rgba))),
			  vdupq_n_f32(1.0f / 255.0f))));
	m_color.set_w(1.0f);

	widget->SetOrigin(widget->m_origin_x, 2, widget->m_origin_z, NULL);

	ui::Button *button = NULL;

	if (widget->IsInherit(ui::Button::TypeName()))
		button = (ui::Button *)widget;

	int result = 0;

	if (button != NULL) {
		result = button->AddEventListener(
			INDICATOR_EVENT_ITEM_ON_PRESSED,
			new ui::EventCBListener(indicatorEventItemOnPressed, this));
		button->AddEventListener(INDICATOR_EVENT_ITEM_ON_FOCUS_IN,
					 new ui::EventCBListener(indicatorEventItemOnFocusIn, this));
		button->AddEventListener(INDICATOR_EVENT_ITEM_ON_FOCUS_OUT,
					 new ui::EventCBListener(indicatorEventItemOnFocusOut, this));
		button->AddEventListener(INDICATOR_EVENT_ITEM_ON_LONG_PRESS,
					 new ui::EventCBListener(indicatorEventItemOnLongPress,
								 this));
		button->m_enable_long_press = true;
		button->m_long_press_flag = true;
		button->m_long_press_msec = INDICATOR_EVENT_ITEM_LONG_PRESS;
		button->SetGestureMask(button->m_touch_gesture_mask | 4, false);
		button->m_scene->AddEventListener(INDICATOR_EVENT_ITEM_ON_SCENE,
						  new ui::EventCBListener(indicatorEventItemOnScene,
									  this));
	}
	return result;
}

/* (0x81175238) */
int IndicatorEventItem::Execute()
{
	const LsdbNewEvent *event = &m_event;
	int actType = m_event.actType;

	if (actType < 1 || actType > INDICATOR_EVENT_ACT_URI)
		return 0;

	switch (actType) {
	case INDICATOR_EVENT_ACT_LAUNCH:
		if (event != NULL && event->execTitleId.length() != 0)
			sceAppMgrLaunchAppByName(event->execMode, event->execTitleId.c_str(),
						 event->execArg.c_str());
		break;
	case INDICATOR_EVENT_ACT_APPLAUNCHER:
		if (event != NULL && event->execTitleId.length() != 0) {
			ApplauncherInterface *applauncher = (ApplauncherInterface *)
				applauncherPluginGetInterface("applauncher_plugin", 1);

			if (applauncher != NULL)
				((void (*)(const string *, int))
				 applauncher->func[INDICATOR_APPLAUNCHER_FUNC_FOCUS])(
					&event->execTitleId, 1);
		}
		break;
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
	case 9:
	case 10:
		return 0;
	case INDICATOR_EVENT_ACT_LIVEAREA:
		if (event != NULL && event->execTitleId.length() != 0) {
			ShellPluginInterface *livearea = (ShellPluginInterface *)
				applauncherPluginGetInterface("livearea_plugin", 1);

			if (livearea != NULL) {
				void (*open)(const char *, const char *) =
					(void (*)(const char *, const char *))
					livearea->func[INDICATOR_LIVEAREA_FUNC_OPEN];

				if (event->execArg.length() != 0)
					open(event->execTitleId.c_str(), event->execArg.c_str());
				else
					open(event->execTitleId.c_str(), NULL);
			}
		}
		break;
	case INDICATOR_EVENT_ACT_URI:
		sceAppMgrLaunchAppByUri(event->execMode | 0x100000, event->execArg.c_str());
		break;
	}
	return 0;
}

/* (0x81175302) */
int IndicatorEventItem::SetStyle(ui::Widget *widget)
{
	if (widget == NULL || !widget->IsInherit(ui::Button::TypeName()))
		return -1;

	ui::Button *button = (ui::Button *)widget;
	math::v4 white(1.0f, 1.0f, 1.0f, 1.0f);

	if (button->m_disable_color != NULL)
		*button->m_disable_color = white;
	else
		button->m_disable_color = new math::v4(white);

	IDParam back(s_empty_name, m_event.newFlag ? INDICATOR_EVENT_ITEM_BACK_NEW :
						     INDICATOR_EVENT_ITEM_BACK);

	widget->SetTexture(g_indicatorPlugin->GetTexture(back), 0, 0);

	IDParam frame(s_empty_name, INDICATOR_EVENT_ITEM_FRAME);

	widget->SetTexture(g_indicatorPlugin->GetTexture(frame), INDICATOR_EVENT_ITEM_FRAME_OBJ, 0);
	return -1;
}

/* (0x81175490) */
const LsdbNewEvent *IndicatorEventItem::GetEvent()
{
	return &m_event;
}

/* the widget pressed: the event run and the callback told (0x81175494) */
static void indicatorEventItemOnPressed(int32_t type, ui::Handler *self, ui::Event *e,
					void *data)
{
	IndicatorEventItem *item = (IndicatorEventItem *)data;

	if (item != NULL) {
		int result = item->Execute();

		if (item->m_callback != NULL)
			item->m_callback(result, (int)&item->m_event);
	}
}

/* the widget focused while the focus is shown (0x811754B2) */
static void indicatorEventItemOnFocusIn(int32_t type, ui::Handler *self, ui::Event *e,
					void *data)
{
	IndicatorEventItem *item = (IndicatorEventItem *)data;

	if (ScePafWidget_529FB877(Framework::Instance()->GetEnvironmentInstance(), -1))
		item->OnFocus();
}

/* (0x811754D4) */
static void indicatorEventItemOnFocusOut(int32_t type, ui::Handler *self, ui::Event *e,
					 void *data)
{
	((IndicatorEventItem *)data)->OnUnfocus();
}

/* (0x811754E0) */
static void indicatorEventItemOnLongPress(int32_t type, ui::Handler *self, ui::Event *e,
					  void *data)
{
	((IndicatorEventItem *)data)->OnFocus();
}

/* the scene's event: the item told again while the cursor is on its widget and shown
   (0x811754EC) */
static void indicatorEventItemOnScene(int32_t type, ui::Handler *self, ui::Event *e,
				      void *data)
{
	IndicatorEventItem *item = (IndicatorEventItem *)data;
	ui::Widget *widget = item->m_widget;

	if (widget != NULL && widget->m_cursor_on &&
	    ScePafWidget_529FB877(Framework::Instance()->GetEnvironmentInstance(), -1))
		item->OnFocus();
}

/* (0x81175520) */
int IndicatorEventItem::GetMode()
{
	return m_mode;
}

/* (0x81175526) */
int IndicatorEventItem::AlignText(graph::TextObj *text, int align, const math::v4 &size,
				  math::v4 *pos)
{
	int result = 0x80AF0022;
	uint32_t lines;

	if (text == NULL)
		return result;

	result = text->GetLineCount(lines);
	if (result != 0 || lines == 0)
		return result;

	switch (align) {
	case 4: {
		graph::TextLayoutLineInfo first;

		text->GetLineInfo(0, first);

		graph::TextLayoutLineInfo last;

		text->GetLineInfo(lines - 1, last);
		pos->set_y((float)(int)((first.frame.extract_y() + first.descender +
					 last.frame.extract_y() + last.descender -
					 size.extract_y()) * 0.5f));
		break;
	}
	case 5: {
		graph::TextLayoutLineInfo last;

		text->GetLineInfo(lines - 1, last);
		pos->set_y((float)(int)(last.frame.extract_y() + last.descender -
					size.extract_y() * 0.5f));
		break;
	}
	case 6: {
		graph::TextLayoutLineInfo first;

		text->GetLineInfo(0, first);
		pos->set_y((float)(int)(first.frame.extract_y() + first.descender -
					size.extract_y() * 0.5f));
		break;
	}
	case 7: {
		graph::TextLayoutLineInfo first;

		text->GetLineInfo(0, first);
		pos->set_y((float)(int)(first.frame.extract_y() + first.descender -
					size.extract_y() * 0.5f));
		break;
	}
	case 8: {
		graph::TextLayoutLineInfo last;

		text->GetLineInfo(lines - 1, last);
		pos->set_y((float)(int)(last.frame.extract_y() +
					(last.descender + last.ascender - size.extract_y()) *
						0.5f));
		break;
	}
	case 9: {
		graph::TextLayoutLineInfo first;

		text->GetLineInfo(0, first);
		pos->set_y((float)(int)(first.frame.extract_y() +
					(first.descender + first.ascender - size.extract_y()) *
						0.5f));
		break;
	}
	default:
		break;
	}
	return result;
}

/* (0x81175802) */
unsigned int IndicatorEventItem::IsNotified()
{
	return indicatorEventIsNotified(&m_event);
}
