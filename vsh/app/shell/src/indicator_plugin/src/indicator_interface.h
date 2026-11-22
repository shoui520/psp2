/*
 * indicator_interface.h: the table indicator_plugin gives as its GetInterface(1), through which
 * the shell and its other plugins drive the information bar. The names are ours (PLACEHOLDER).
 */
#ifndef INDICATOR_INTERFACE_H
#define INDICATOR_INTERFACE_H

#include <scetypes.h>
#include <std/memory>

namespace paf {
namespace graph {
class Surface;
}
namespace ui {
class Widget;
class TextureSource;
}
}

/* how the bar is drawn: given by value (12 bytes; PLACEHOLDER names) */
struct IndicatorMode {
	int type;		/* 1: the bar's notices hidden */
	int style;		/* 1: dark, 2: the theme's colors */
	int dimmed;		/* with type 1: the bar drawn at 0.7 alpha */
};

/* what indicator_plugin is told of a download task */
struct IndicatorTaskEvent {
	IndicatorTaskEvent() : taskId(0)
	{
		reserved[0] = 0;
		reserved[1] = 0;
	}

	SceUInt32 taskId;
	int reserved[2];
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
	void *itemFunctions[5];			/* PLACEHOLDER: filled by the bar's items */
	/* the page dots': a page's dot added (its widget) and removed by the page's key, the dots
	   refreshed, and a dot's image or texture set (indicator_interface.cpp) */
	paf::ui::Widget *(*AddPageDot)(int key);
	void (*RemovePageDot)(int key);
	void (*RefreshPageDots)(void);
	void (*SetPageDotImage)(int key, const paf::intrusive_ptr<paf::ui::TextureSource> *image);
	void (*SetPageDotTexture)(int key, const paf::intrusive_ptr<paf::graph::Surface> &texture);
	void *moreItemFunctions[8];		/* PLACEHOLDER: filled by the bar's items */
	void (*UpdateTheme)(void);		/* the theme's look taken again, on the main thread */
};

#endif
