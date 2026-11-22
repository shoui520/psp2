/*
 * indicator_interface.h: the table indicator_plugin gives as its GetInterface(1), through which
 * the shell and its other plugins drive the information bar. The names are ours (PLACEHOLDER).
 */
#ifndef INDICATOR_INTERFACE_H
#define INDICATOR_INTERFACE_H

#include <scetypes.h>
#include <std/memory>
#include <widget/core/widget.h>

namespace paf {
namespace graph {
class Surface;
}
namespace ui {
class Widget;
class TextureSource;
}
}

struct IndicatorInhaleAnim;
struct IndicatorNoticeRequest;
struct IndicatorPushNewEventParam;
class IndicatorEventItem;

/* how the bar is drawn: given by value (12 bytes; PLACEHOLDER names) */
struct IndicatorMode {
	int type;		/* 1: the bar's notices hidden */
	int style;		/* 1: dark, 2: the theme's colors */
	int dimmed;		/* with type 1: the bar drawn at 0.7 alpha */
};

/* what indicator_plugin is told of a download task: the new events' button popped, the
   callback told when it is done, after the delay (milliseconds) */
struct IndicatorTaskEvent {
	IndicatorTaskEvent() : taskId(0), callback(NULL), delay(0)
	{
	}

	SceUInt32 taskId;
	paf::ui::AnimCB callback;
	float delay;
};

/*
 * The table (128 bytes): the bar's own functions, filled when the bar is made, then those its
 * items fill. The values a setter is given are the status's of the same name (indicator.h).
 */
struct IndicatorPluginInterface {
	/* the bar shown in a mode, slid in or at once; -1 without the bar */
	int (*Show)(IndicatorMode mode, bool animate);
	int (*Hide)(bool animate);
	int (*GetModeType)(int *type);
	bool (*IsVisible)(void);
	int (*SetValue4)(int value);
	int (*SetFlightMode)(int on);
	int (*SetValue6)(int value);
	int (*SetBluetooth)(int on);
	int (*SetValue8)(int value);
	int (*Unsupported9)(void);		/* -1 */
	int (*Unsupported10)(void);		/* -1 */
	int (*SetNotification)(int on);
	void (*TaskEvent)(const IndicatorTaskEvent *event);
	/* the new events' item's (indicator_new_event.cpp): an icon's inhale animation started
	   ("StartInhaleAnim"), the shell's notice pushed and cancelled
	   ("primaryNotificationPush", "primaryNotificationCancel"), a new event pushed, and an
	   object of the caller's deleted */
	int (*StartInhaleAnim)(const IndicatorInhaleAnim *anim);
	int (*PrimaryNotificationPush)(const IndicatorNoticeRequest *request);
	int (*PrimaryNotificationCancel)(const char *titleId);
	int (*PushNewEvent)(IndicatorPushNewEventParam *param);
	int (*DeleteObject)(IndicatorEventItem **item);
	/* the page dots': a page's dot added (its widget) and removed by the page's key, the dots
	   refreshed, and a dot's image or texture set (indicator_interface.cpp) */
	paf::ui::Widget *(*AddPageDot)(int key);
	void (*RemovePageDot)(int key);
	void (*RefreshPageDots)(void);
	void (*SetPageDotImage)(int key, const paf::intrusive_ptr<paf::ui::TextureSource> *image);
	void (*SetPageDotTexture)(int key, const paf::intrusive_ptr<paf::graph::Surface> &texture);
	void *moreItemFunctions[4];		/* PLACEHOLDER: filled by the bar's items */
	/* the new events' item's: its button disabled while one or more hold it, enabled again,
	   the notices' state updated, and the button */
	void (*LockButton)(void);
	void (*UnlockButton)(void);
	bool (*UpdateNotices)(void);
	paf::ui::Widget *(*GetButton)(void);
	void (*UpdateTheme)(void);		/* the theme's look taken again, on the main thread */
};

#endif
