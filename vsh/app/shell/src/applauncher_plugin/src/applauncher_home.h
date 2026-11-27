/*
 * applauncher_home.h: the home screen's object (the pages of icons the user scrolls through, made
 * at the plugin's start), the snap list its pages are kept in, and the parts of the shell it
 * reaches. Names ours (PLACEHOLDER): the 3.6x shell names none of them; DeletePageJob is the job's
 * own name.
 */
#ifndef _APPLAUNCHER_HOME_H
#define _APPLAUNCHER_HOME_H

#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/vector>
#include <std/list>
#include <std/map>
#include <std/memory>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <math/ngp/quaternion.h>
#include <math/ngp/matrix.h>
#include <versatileparam/idparam.h>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
#include <graphics/drawobj/draw_obj.h>
#include <widget/core/widget.h>
#include <widget/w_appicon_base.h>
#include <widget/w_scroll_view_base.h>
#include <inputdevice/inputlistener.h>
#include <job/job.h>
#include <cache/cache_image.h>
#include <cache/cache_manager.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include "../../../plugins.h"
#include "../../livespace_db/livespace_db_internal.h"
#include "applauncher_mode.h"
#include "applauncher_folderframe.h"

namespace paf { namespace ui { class CompositeDrawer; class ListView; class TextBox; } }

class ApplauncherPage;
struct ApplauncherIconView;

/* an icon's data is livespace_db's LsdbIcon (its folder's LsdbFolder), as the data cache gives
   it */

/* PLACEHOLDER: an icon of a page (72 bytes, vtable 0x8152F520; applauncher_icon.cpp) */
class ApplauncherIcon {
public:
	/* the icon of the data, its label from the data's title (the power icon's from the
	   plugin's string), its image loaded; the plugin is not read (0x810CFB92) */
	ApplauncherIcon(paf::Plugin *plugin, const LsdbIconPtr &data);
	/* its view given back, its title's topmenu entry removed, taken off its page
	   (0x810D1E0A) */
	virtual ~ApplauncherIcon();

	/* PLACEHOLDER name: the icon's own pointer kept, given by applauncherIconCreate
	   (0x810CF75E) */
	void SetSelf(const paf::common::SharedPtr<ApplauncherIcon> &self);
	/* PLACEHOLDER name: the drag ended, the icon put back and the drag let go of
	   (0x810D0900) */
	void Drop();
	/* PLACEHOLDER names: the child of the title taken out of the folder (0x810D192E), and
	   the folder's look updated (0x810CF7DE) */
	void RemoveChild(const char *titleId);
	/* PLACEHOLDER name: the child of the title put in the folder (0x810D17C0) */
	bool AddChild(const char *titleId);
	void Update(bool animate);
	/* PLACEHOLDER names: the icon given its application's new data (0x810D1502), and a
	   folder's children's data taken in (0x810D1C90) */
	void SetData(const LsdbIconPtr &data);
	/* PLACEHOLDER names: the icon given other data of its page, the power icon's title its
	   label (0x810D0352), and its image loaded again (0x810D055C) */
	void Reset(const LsdbIconPtr &data);
	void Reload();
	/* PLACEHOLDER name: the icon's application started (a web link's in the browser), the
	   launch effect played; 0 when nothing started (0x810D0566) */
	int Launch();
	void UpdateFolder();
	/* PLACEHOLDER names: the icon marked selected (0x810D0326), and its widget given the
	   focus, 1 without a widget (0x810D0332) */
	void Select();
	int Focus();
	/* PLACEHOLDER name: the selected mark cleared (0x810D032C) */
	void Unselect();
	/* PLACEHOLDER names: the icon marked for an update (0x810D229A), and its application's
	   data asked for again (0x810D22A2) */
	void MarkUpdate();
	void RequestData();
	/* PLACEHOLDER name: the dragged icon moved over the slot under it (0x810D1138) */
	void MoveDrag(int option);
	/* the icon's widget, NULL without one (0x810D0006) */
	paf::ui::Widget *GetWidget();
	/* PLACEHOLDER name: the icon's widget faded in (0x810D1D38) */
	void Show();
	/* PLACEHOLDER name: the icon's widgets given back to the icon pool (0x810CFE08) */
	void ReleaseView(bool keep);
	/* PLACEHOLDER name: the icon's widgets taken from the icon pool (0x810D14D6) */
	void LoadView();
	/* PLACEHOLDER name: where the icon's widget stands in its slot (0x810D0012) */
	paf::math::v4 GetSlotPos();
	/* PLACEHOLDER name: a folder, its data's type 5 (0x810CF74C) */
	bool IsFolder();
	/* PLACEHOLDER name: the icon widget's effect resumed (0x810D19F6) */
	void Resume();
	/* PLACEHOLDER names: the icon put in the edit (0x810CFFD2), and out (0x810CFFEC) */
	void BeginEdit(bool animate);
	void EndEdit();
	/* PLACEHOLDER names: whether the folder takes the title (a folder with room, the title
	   not in it yet; 0x810D1738), and the icon put in the folder and deleted from its page,
	   deleted alone without a folder (0x810D1A06) */
	bool CanAddChild(const char *titleId);
	/* PLACEHOLDER name: the title is in the folder (0x810D1C24) */
	bool HasChild(const paf::string &titleId);
	void PutInFolder(const paf::common::SharedPtr<ApplauncherIcon> &folder);
	/* PLACEHOLDER names: the icon's widget pressed (0x810D076C), and the drag events
	   0x2000D-0x2000F of its widget (0x810D1110) */
	void OnPressed();
	void OnDragEvent(paf::ui::Event *e, paf::ui::AppIconBase *widget);
	/* PLACEHOLDER name: a step of the icon's drag over the pages (0 begun, 1 moved, 2 ended)
	   with its widget, the page turned at an edge (0x810D0AB0) */
	void UpdateDrag(int phase, paf::ui::AppIconBase *widget, int edge);

	paf::wstring m_label;			/* PLACEHOLDER: the label its widget shows */
	ApplauncherIconView *m_icon;		/* PLACEHOLDER: its view from the icon pool */
	ApplauncherPage *m_page;		/* PLACEHOLDER: the page it is on */
	SceUInt32 m_launchLock;		/* PLACEHOLDER: not launched while it is set */
	bool m_selected;			/* PLACEHOLDER: marked selected (Select, Unselect) */
	bool m_hidden;				/* PLACEHOLDER: not shown (left out of the snaps) */
	bool m_focusPending;			/* PLACEHOLDER: Focus asked before its widget came */
	bool m_added;				/* PLACEHOLDER: just put on the home by entry #0 */
	int m_slot;				/* PLACEHOLDER: its slot on the page */
	ApplauncherPage *m_movePage;		/* PLACEHOLDER: the page its move began on */
	int m_moveSlot;				/* PLACEHOLDER: the slot its move began in */
	int m_homeSlot;				/* PLACEHOLDER: the slot it goes back to when moved aside */
	paf::intrusive_ptr<paf::cache::CacheImageInterface> m_image;	/* PLACEHOLDER: its image */
	LsdbIconPtr m_data;			/* PLACEHOLDER: its data */
	bool m_unavailable;			/* PLACEHOLDER: a press shows an error, starts nothing */
	bool m_removeEntry;			/* PLACEHOLDER: its title's topmenu entry removed with it */
	/* PLACEHOLDER: the icon itself, kept from its creation to its delete (its view's effect
	   plays while it is set) */
	paf::common::SharedPtr<ApplauncherIcon> m_self;
};

/*
 * PLACEHOLDER: the home's wobble of an icon's view: its widget tipped about a normal that
 * swings back to upright, pushed by the page's moves and its own spin, and bobbed while the
 * icon is selected (96 bytes, applauncher_iconeffect.cpp)
 */
class ApplauncherIconWobble {
public:
	ApplauncherIconWobble()
	{
		Reset();
	}

	/* PLACEHOLDER names: still and upright (0x810CC8D0), and moved on a frame (0x810CCDFE) */
	void Reset();
	void Update(ApplauncherIconView *view);

	float m_angle;				/* the turn its swing is pushed with */
	float m_limit;				/* the most it tips, in degrees over ten */
	float m_phase;				/* seconds into the selected bob (half a second) */
	SceUInt32 reserved;
	paf::math::v4 m_normal;			/* the way it faces */
	paf::math::v4 m_spin;			/* the normal's speed */
	paf::math::v4 m_move;			/* the page's move on the last frame */
	paf::math::v4 m_bob;			/* its sway off its place */
	paf::math::v4 m_fling;			/* a fast move of the page, dying away */
};

/*
 * PLACEHOLDER: the edit's tilt of an icon's view: its widget falls back to the page's plane
 * and bounces, its tilt turned back with each bounce (56 bytes)
 */
class ApplauncherIconTilt {
public:
	/* PLACEHOLDER names: moved on a frame (0x810CC946), the widget's pose taken as the
	   start (0x810CDA8C), and at rest on the plane (0x810CDCBC) */
	void Update(ApplauncherIconView *view);
	void Capture(ApplauncherIconView *view);
	void Begin(ApplauncherIconView *view);

	paf::math::v4 m_pos;
	paf::math::quaternion m_rot;
	paf::math::quaternion m_target;		/* the tilt each bounce turns it to */
	float m_z;
	float m_speed;				/* falling toward the plane */
};

/* PLACEHOLDER: an icon's view dropped in from above its slot (4 bytes) */
class ApplauncherIconDrop {
public:
	ApplauncherIconDrop() : m_time(-1.0f)
	{
	}

	/* (0x810CDB32) */
	void Begin(ApplauncherIconView *view);

	float m_time;
};

/* PLACEHOLDER: the effect an icon's view plays (160 bytes, its functions 0x810CC8D0 on) */
class ApplauncherIconEffect {
public:
	/* PLACEHOLDER names: made (0x810CC914), cleared (0x810CDD04), played
	   each frame (0x810CDA62), and begun for the view (0x810CDCFA) */
	ApplauncherIconEffect();
	void Reset();
	void Update(ApplauncherIconView *view);
	void Begin(ApplauncherIconView *view);
	/* PLACEHOLDER names: the view's widget's pose taken as the edit's start (0x810CDB28), and
	   the widget dropped in from above its place (0x810CDCB2) */
	void Capture(ApplauncherIconView *view);
	void DropIn(ApplauncherIconView *view);

	ApplauncherIconWobble m_wobble;
	ApplauncherIconTilt m_tilt;
	ApplauncherIconDrop m_drop;
};

/*
 * PLACEHOLDER: an icon's widgets, taken from the icon pool while the icon is shown (176
 * bytes, 42 made by applauncherIconPoolCreate)
 */
struct ApplauncherIconView {
	/* (0x810BB748, 0x810BBC6E) */
	ApplauncherIconView();
	~ApplauncherIconView();

	paf::ui::AppIconBase *m_widget;
	paf::common::SharedPtr<ApplauncherIcon> m_owner;	/* the icon it shows */
	SceUInt32 reserved;
	ApplauncherIconEffect m_effect;
};


/* PLACEHOLDER: an icon taken off a page, as the page's saved order is told of it */
struct ApplauncherRemovedIcon {
	int m_id;
	int m_pageIndex;
	int m_slot;
};
PAF_VECTOR_INLINE_DTOR(ApplauncherRemovedIcon)

/* PLACEHOLDER: the image of a folder's child, and its surface, by its position in the folder */
struct ApplauncherFolderImage {
	int m_pos;
	paf::intrusive_ptr<paf::cache::CacheImageInterface> m_image;
};
struct ApplauncherFolderSurface {
	int m_pos;
	paf::intrusive_ptr<paf::graph::Surface> m_surface;
};

/* PLACEHOLDER: a folder's icon drawn from its children's surfaces by the icon capture
   (SceShellIconCaptureManager), as a PNG the caller frees, its size given back (0x813C3A72) */
void *applauncherIconCaptureFolder(const paf::vector<ApplauncherFolderSurface> *surfaces,
				   int *size);

/*
 * PLACEHOLDER: an item of a ShellSnapList, its widget snapped to by the list's scroll view
 * (the class is the shell's code at 0x8128xxxx, not written yet; fields read here only).
 */
class ShellSnapList;

class ShellSnapListItem {
public:
	/* the item made on the list: the template opened on its view, the widget found in it and
	   named after the item, the list's next key taken (0x8128471E) */
	ShellSnapListItem(paf::Plugin *plugin, ShellSnapList *list, const paf::IDParam &templateId,
			  const paf::IDParam &widgetId);
	/* its listeners told and freed, its widget destroyed (0x81284A4C) */
	virtual ~ShellSnapListItem();
	/* PLACEHOLDER names: the item snapped to by the list, and left (both empty here: the
	   shell's empty function 0x814098DC) */
	virtual void OnSnapped(void)
	{
	}
	virtual void OnUnsnapped(void)
	{
	}

	/* PLACEHOLDER name: a box of the size about its widget (or the other widget) seen on the
	   screen, by the list's camera (0x8128472E) */
	bool IsInView(const paf::math::v4 &size, paf::ui::Widget *other);

	SceUInt32 headPad[3];
	paf::ui::Widget *m_widget;		/* what the view snaps to */
	SceUInt32 midPad[7];
	int m_index;				/* its place in the list */
	SceUInt32 indexPad;
	paf::math::v2 m_spacing;		/* PLACEHOLDER: 20,20 on an application page, 0
						   from the list (not read yet) */
	int m_key;				/* the key of its listeners */
};

/* PLACEHOLDER: what a ShellSnapList calls on an event of its items: the event's two arguments
   (not read yet), the item's key and the listener's data */
typedef void (*ShellSnapListCB)(int event, void *arg, int key, void *data);

/* PLACEHOLDER: a ShellSnapList's listener: its function called with its data (vtable 0x8152F36C;
   the list's code at 0x8128xxxx, not written yet) */
class ShellSnapListListener {
public:
	ShellSnapListListener(ShellSnapListCB func, void *data)
		: m_func(func), m_data(data), m_killed(false)
	{
	}

	virtual ~ShellSnapListListener()
	{
	}

	virtual int Do(int event, void *arg, int key)
	{
		m_func(event, arg, key, m_data);
		return 0;
	}

	ShellSnapListCB m_func;
	void *m_data;
	bool m_killed;				/* taken out while its list runs */
};

/* PLACEHOLDER: the listeners of one event of a ShellSnapList (vtable 0x8152F35C) */
class ShellSnapListListenerList {
public:
	ShellSnapListListenerList() : m_onExec(false)
	{
	}

	virtual ~ShellSnapListListenerList()
	{
		while (m_list.size() != 0) {
			paf::list<ShellSnapListListener *>::iterator it = m_list.begin();

			delete *it;
			m_list.erase(it);
		}
	}

	/* the listener when it is in the list, else NULL */
	ShellSnapListListener *Find(ShellSnapListListener *listener)
	{
		for (paf::list<ShellSnapListListener *>::iterator it = m_list.begin();
		     it != m_list.end(); ++it) {
			if (*it == listener)
				return *it;
		}
		return NULL;
	}

	/* the listener of the function and data, else NULL */
	ShellSnapListListener *Find(ShellSnapListCB func, void *data)
	{
		for (paf::list<ShellSnapListListener *>::iterator it = m_list.begin();
		     it != m_list.end(); ++it) {
			if ((*it)->m_func == func && (*it)->m_data == data)
				return *it;
		}
		return NULL;
	}

	/* put at the end unless it is in the list */
	void Add(ShellSnapListListener *listener)
	{
		if (Find(listener) == NULL)
			m_list.push_back(listener);
	}

	/* killed while the list runs, else taken out and deleted */
	void Del(ShellSnapListListener *listener)
	{
		for (paf::list<ShellSnapListListener *>::iterator it = m_list.begin();
		     it != m_list.end(); ++it) {
			if (*it == listener) {
				if (m_onExec) {
					listener->m_killed = true;
				} else {
					m_list.erase(it);
					delete listener;
				}
				return;
			}
		}
	}

	paf::list<ShellSnapListListener *> m_list;
	bool m_onExec;				/* while its listeners are called */
};

/* Sony has the lookups in a snap list's listener map inline, like paf's handlers */
namespace paf {
template<>
inline __attribute__((always_inline)) _Tree<_Tmap_traits<int, ShellSnapListListenerList *, less<int> > >::iterator
_Tree<_Tmap_traits<int, ShellSnapListListenerList *, less<int> > >::find(const int &key)
{
	return iterator(_Find(key));
}
}

/*
 * PLACEHOLDER: a list of items laid out in a scroll view, snapping to each (the shell's code at
 * 0x81284EB4-0x812861xx, not written yet): its listeners by key, the view, the items and the
 * space between them.
 */
class ShellSnapList {
public:
	typedef paf::vector<ShellSnapListItem *>::iterator iterator;

	ShellSnapList(paf::Plugin *plugin, int axis, int capacity, int spacing);
	virtual ~ShellSnapList();
	/* the items put in their places; the base leaves them */
	virtual void Layout(int mode);
	/* the view moved to the item over time, or at once */
	virtual void MoveTo(ShellSnapListItem *item, float time);
	virtual void JumpTo(ShellSnapListItem *item);

	/* the view given, its snaps followed */
	void SetScrollView(paf::ui::ScrollView *view);
	/* the item taken out, its listeners told; the item after it */
	iterator Remove(ShellSnapListItem *item);
	/* the item put in after the other (at the end without one) */
	void Insert(ShellSnapListItem *item, int notify, ShellSnapListItem *after);
	/* the item nearest the view's position, and the one nearest its focus */
	ShellSnapListItem *GetNearest();
	ShellSnapListItem *GetFocused();
	/* the item after it and before it; the item itself at either end */
	ShellSnapListItem *GetNext(ShellSnapListItem *item);
	ShellSnapListItem *GetPrevious(ShellSnapListItem *item);
	/* PLACEHOLDER name: the item whose widget has the view's snap ID, NULL if none
	   (0x81285D04) */
	ShellSnapListItem *GetItemBySnapID(int id);
	/* PLACEHOLDER name: the view's snap ID of the item's widget (0x81285DB6) */
	int GetSnapID(ShellSnapListItem *item);
	/* PLACEHOLDER name: the view's snap point set at the item and the view scrolled to it
	   (0x81285EC4) */
	void ScrollTo(ShellSnapListItem *item);

	/* the listener called on the event (inline wherever Sony adds one) */
	void AddListener(int event, ShellSnapListCB func, void *data)
	{
		ShellSnapListListener *listener = new ShellSnapListListener(func, data);
		paf::map<int, ShellSnapListListenerList *>::iterator it = m_listeners.find(event);

		if (it == m_listeners.end()) {
			ShellSnapListListenerList *list = new ShellSnapListListenerList();

			list->Add(listener);
			m_listeners.insert(paf::pair<int, ShellSnapListListenerList *>(event, list));
		} else if (it->second != NULL) {
			it->second->Add(listener);
		}
	}

	/* the listener of the function and data taken off the event (inline the same way) */
	void DelListener(int event, ShellSnapListCB func, void *data)
	{
		paf::map<int, ShellSnapListListenerList *>::iterator it = m_listeners.find(event);

		if (it != m_listeners.end()) {
			ShellSnapListListenerList *list = it->second;

			if (list != NULL)
				list->Del(list->Find(func, data));
		}
	}

	/* PLACEHOLDER name: the item of the key, NULL for none or key -1 (0x81285CCE) */
	ShellSnapListItem *GetItemByKey(int key);

	paf::map<int, ShellSnapListListenerList *> m_listeners;	/* by event */
	paf::Plugin *m_plugin;
	paf::ui::ScrollView *m_scrollView;
	int m_axis;
	paf::vector<ShellSnapListItem *> m_items;
	int m_reserved;
	int m_capacity;
	int m_spacing;
	bool m_notify;
};

/* the home's pages in its scroll view, each put one spacing below the one before */
class ApplauncherPageScroller : public ShellSnapList {
public:
	ApplauncherPageScroller(paf::Plugin *plugin) : ShellSnapList(plugin, 2, 10, 512)
	{
	}

	virtual ~ApplauncherPageScroller();
	virtual void Layout(int mode);
};


/*
 * PLACEHOLDER: a page's widgets, from the pool of three for the home's pages and one for a
 * folder's (applauncher_pageview.cpp, 0x810C3B3C-0x810C61DC): the page's widget opened from its
 * template, the frames of its ten slots, the widget its icons go on, the edit's buttons and a
 * folder's name.
 */
class ApplauncherPageView {
public:
	ApplauncherPageView()
		: m_widget(NULL), m_root(NULL), m_deleteButton(NULL), m_themeButton(NULL),
		  m_addButton(NULL), m_name(NULL), m_nameHeld(false), m_isFolder(false),
		  m_page(NULL)
	{
		for (int i = 0; i < 10; i++)
			m_slotShown[i] = true;
	}
	~ApplauncherPageView()
	{
		if (m_widget != NULL) {
			m_widget->DestroyWidget();
			m_widget = NULL;
		}
	}

	/* PLACEHOLDER name: the widgets opened from the template, a folder's or a page's
	   (0x810C3B3C) */
	void Init(bool folder);
	/* PLACEHOLDER names: the view's delete button shown or hidden (0x810C5156), and its add
	   button with the home's page count (0x810C5A0E) */
	void SetEmpty(bool empty);
	void Refresh();
	/* PLACEHOLDER names: the edit begun on the view, empty or not (0x810C5688), and ended
	   (0x810C51E2); the slot's frame faded in (0x810C5C08) and out (0x810C5D0C) */
	void BeginEdit(bool empty);
	void EndEdit();
	void ShowSlotFrame(int slot, bool animate);
	void HideSlotFrame(int slot, bool animate);

	paf::ui::Widget *m_widget;		/* PLACEHOLDER: the page's widget, named after the view */
	paf::ui::CompositeDrawer *m_slotFrames;	/* PLACEHOLDER: a plane for each slot's frame */
	paf::ui::Widget *m_root;		/* PLACEHOLDER: the widget its icons go on, a button */
	paf::ui::Widget *m_deleteButton;	/* PLACEHOLDER: the edit's buttons: the empty page */
	paf::ui::Widget *m_themeButton;		/* deleted, the theme chosen and a page added */
	paf::ui::Widget *m_addButton;
	paf::ui::TextBox *m_name;		/* PLACEHOLDER: a folder's name, edited in the edit */
	bool m_nameHeld;			/* PLACEHOLDER: the name's show hold taken */
	bool m_isFolder;			/* PLACEHOLDER: a folder's view */
	bool m_slotShown[10];			/* PLACEHOLDER: each slot's frame shown */
	ApplauncherPage *m_page;		/* PLACEHOLDER: the page it is taken for */
};
/* PLACEHOLDER: a view taken from the pool for the page (the folder's for a folder's page), and
   given back to it (0x810C4A3A, 0x810C4E74) */
ApplauncherPageView *applauncherPageViewGet(ApplauncherPage *page, bool fast);
void applauncherPageViewRelease(ApplauncherPageView *view);

/*
 * PLACEHOLDER: the theme chooser AppTabSetThemeMode shows (0x810C6498, 44 bytes, one at a
 * time): a list of chips, the photo to pick, the page's image, the user's own wave and the 31
 * waves, with a cursor on the page's. The chips are made by the list's item factory.
 */
class ApplauncherThemeSelector {
public:
	ApplauncherThemeSelector();
	~ApplauncherThemeSelector();

	paf::ui::Widget *m_root;		/* PLACEHOLDER: the plane topmenu made for it */
	paf::ui::Widget *m_frame;		/* PLACEHOLDER: the template's widgets: its frame, */
	paf::ui::Widget *m_box;			/* the box, the cursor on the chosen chip */
	paf::ui::Widget *m_cursor;
	paf::ui::ListView *m_list;		/* and the list of chips */
	int m_waitDialog;			/* PLACEHOLDER: the dialog while a photo converts */
	int m_wallpaperIndex;			/* PLACEHOLDER: the wallpaper a pick started from */
	bool reserved;				/* 0 at construction; not read */
	bool m_photoPicking;			/* PLACEHOLDER: a photo pick open, */
	bool m_wallpaperPicking;		/* and a wallpaper pick */
	paf::common::SharedPtr<paf::job::JobItem> m_loadJob;	/* the page's image loaded */
	paf::intrusive_ptr<paf::graph::Surface> m_image;	/* and the image */
};
/* PLACEHOLDER: the chooser made, taken down and the page it chose, once (0x810C6CD0,
   0x810C6D84, 0x810C6DAC) */
ApplauncherThemeSelector *applauncherThemeSelectorCreate(void);
void applauncherThemeSelectorDestroy(void);
/* PLACEHOLDER: the page the chooser chose, as it keeps it (its fields LsdbPage's, without
   its constructor) */
struct ApplauncherThemePage {
	paf::string themeFile;
	int bgColor;
	int texWidth;
	int texHeight;
	int imageWidth;
	int imageHeight;
	int reserved01;
};

bool applauncherThemeSelectorGetPage(ApplauncherThemePage *page);
/* PLACEHOLDER names: a pick of the chooser under way, the theme's mode kept on a power event
   while one is (0x810C96A0, 0x810C96B0; the theme mode's file) */
void applauncherBusyBegin(void);
void applauncherBusyEnd(void);

/*
 * PLACEHOLDER: a page of the home (its code in the file after this one, 0x810B297C on): its
 * icons' slots, its state and its place in the scroller. The virtual functions the home calls
 * are named; the others are inferred from usage.
 */
/* the slots of a page */
#define APPLAUNCHER_PAGE_SLOTS		10
/* PLACEHOLDER: the home's pages at most */
#define APPLAUNCHER_PAGES_MAX		10

class ApplauncherPageBase {
public:
	/* the page of the type, its slots empty (0x810B8516) */
	ApplauncherPageBase(int type);
	virtual ~ApplauncherPageBase();
	/* PLACEHOLDER name: the page's view shown empty, or not (0x810B7DD4) */
	virtual void SetEmpty(bool empty);
	virtual void Refresh(void);		/* PLACEHOLDER name */
	/* PLACEHOLDER names: the edit's page buttons: the page deleted, its theme set, and a page
	   added after it */
	virtual void Delete(void) = 0;
	virtual void SetTheme(void) = 0;
	virtual void AddPage(void) = 0;
	virtual int GetIndex(void) = 0;		/* PLACEHOLDER name */
	virtual paf::ui::Widget *GetWidget(void) = 0;
	virtual bool IsOpened(void);		/* PLACEHOLDER name: the application page's opening done */
	/* PLACEHOLDER name: where the slot's icon stands on the page */
	virtual paf::math::v2 GetSlotPos(int slot) = 0;
	/* PLACEHOLDER name: the wave's fade applied each frame (nothing in the base: the shell's
	   empty function 0x814098DC) */
	virtual void UpdateFade(void)
	{
	}

	int m_type;				/* 1: the last, empty page */
	paf::common::SharedPtr<ApplauncherIcon> m_slots[APPLAUNCHER_PAGE_SLOTS];
	int m_state;				/* 1: shown in place */
	int m_moveSlot;				/* PLACEHOLDER: the slot an icon's move began in */
	ApplauncherPageView *m_view;		/* PLACEHOLDER: its widgets, from the page pool */
	bool m_changed;			/* PLACEHOLDER: an icon taken off it */
	bool m_iconsLoaded;			/* its icons' widgets taken */
	SceUInt8 tailPad[2];

	/* its icons' widgets and its own given back to the pool (0x810B3BBA) */
	void Unload();
	/* PLACEHOLDER name: the widget its icons go on, NULL without its view (0x810B46F6) */
	paf::ui::Widget *GetRoot();
};

/*
 * PLACEHOLDER: the sway a page gives its icons while the wave fades in: a basis whose normal
 * circles upright with the eased fade (68 bytes)
 */
class ApplauncherPageSway {
public:
	/* PLACEHOLDER names: upright (0x810CF2DC), and moved on a frame (0x810CF332) */
	ApplauncherPageSway();
	void Update(ApplauncherPage *page);

	paf::math::matrix m_matrix;
	float m_phase;				/* the normal's angle about upright */
};

class ApplauncherPage : public ApplauncherPageBase, public ShellSnapListItem {
public:
	/* the page made on the plugin, from the parameters (0x810D3BB8) */
	ApplauncherPage(paf::Plugin *plugin, const LsdbPage *param, int type);
	/* its widget hidden and destroyed after 200 ms, the wallpapers no page uses deleted
	   (0x810D2578) */
	virtual ~ApplauncherPage();
	/* PLACEHOLDER names: its icons kicked when the list snaps to it (0x810D2B30), and their
	   selected marks cleared when it leaves (0x810D2B38) */
	virtual void OnSnapped(void);
	virtual void OnUnsnapped(void);
	/* its index in the list (0x810D3B5E), its widget (0x810D3B64), and where the slot's
	   icon stands on it, at the home's scale (0x810D3B68) */
	virtual int GetIndex(void);
	virtual paf::ui::Widget *GetWidget(void);
	virtual paf::math::v2 GetSlotPos(int slot);
	/* the edit's page buttons: the page deleted (0x810D4040), the theme mode entered on it
	   from the edit (0x810D406A), and a page added after it while fewer than ten
	   (0x810D408C) */
	virtual void Delete(void);
	virtual void SetTheme(void);
	virtual void AddPage(void);
	/* PLACEHOLDER name: the sway moved on, its place kept, the page shown while it is in
	   view and hidden out of it, the slots' frames set in the edit (0x810D3EBE) */
	virtual void UpdateFade(void);
	/* PLACEHOLDER name: each icon's view pushed from below by a random sideways kick, but on a
	   page shown in place (0x810D29D6) */
	void KickIcons();
	/* PLACEHOLDER name: a folder made in the icon's slot, the icon put in it (the context
	   menu's "make a folder"; 0x810D2C58) */
	int MakeFolder(paf::common::SharedPtr<ApplauncherIcon> &icon);
	/* PLACEHOLDER name: the edit begun on the page: its view refreshed below ten pages, its
	   icons and view put in the edit, its slots' frames set (0x810B86B4) */
	void BeginEdit();
	/* PLACEHOLDER name: the slots' frames shown where no folder stands in place, hidden
	   under the others (0x810B85C6) */
	void UpdateSlotFrames(bool animate);

	LsdbPage m_param;		/* what it was made from */
	paf::ui::Widget *m_noticeText;		/* the notice page's text */
	SceUInt32 noticePad;
	ApplauncherPageSway m_sway;		/* PLACEHOLDER: its icons' sway */
	paf::math::v4 m_prevPos;		/* PLACEHOLDER: its place on the last frame */
	paf::math::v4 m_pos;			/* PLACEHOLDER: its place on this frame */

	/* an icon in every slot (0x810B4B0C) */
	bool IsFull();
	/* PLACEHOLDER name: the page out of the edit, its state and its slots' marks cleared
	   (0x810B871E) */
	void EndEdit();
	/* no icon in any slot (0x810B43F0) */
	bool IsEmpty();
	/* its icons loaded (0x810B8752) */
	void Load(bool fast);
	/* PLACEHOLDER name: the icons' widgets taken from the pool once (again when forced), those
	   just added hidden to be shown later (0x810B7E5A) */
	void LoadIcons(bool force);
	/* PLACEHOLDER names: the page's parameters copied out (0x810D2716), and set
	   (0x810D274C) */
	void GetParam(LsdbPage *param);
	void SetParam(const LsdbPage *param, bool save);
	/* PLACEHOLDER name: the icons put back in their slots, the focused one from its drag
	   (0x810D364E) */
	void RestoreIcons(const paf::common::SharedPtr<ApplauncherIcon> &focus);
	/* PLACEHOLDER name: the icons in its slots (0x810B5506) */
	int GetIconCount();
	/* PLACEHOLDER name: the icons given their new data (0x810D2B40) */
	void UpdateIcons(LsdbIconList *list);
	/* PLACEHOLDER names: the icon of the title in a slot (0x810B7DEE), or in a slot or a
	   folder of one, the folder given back (0x810D3A64) */
	paf::common::SharedPtr<ApplauncherIcon> FindIcon(const paf::string &titleId);
	paf::common::SharedPtr<ApplauncherIcon> FindIcon(const paf::string &titleId,
		paf::common::SharedPtr<ApplauncherIcon> *folder);
	/* PLACEHOLDER names: the icon of the application in a slot (0x810D3996), and the icons
	   moved aside from the one opened (0x810D319A) */
	paf::common::SharedPtr<ApplauncherIcon> FindIcon(SceUInt64 appId);
	/* PLACEHOLDER name: the icon whose data's state has the id (0x810D39EE) */
	paf::common::SharedPtr<ApplauncherIcon> FindIconById(int id);
	void MoveIconsAside(const paf::common::SharedPtr<ApplauncherIcon> &icon);
	/* PLACEHOLDER name: the selected marks of its icons cleared (0x810B7EF2) */
	void UnselectAll();
	/* PLACEHOLDER names: the slot whose icon has the cursor, -1 without one or unloaded
	   (0x810B7F08), and the slot's icon given the focus (0x810B7F48) */
	int GetCursorSlot();
	void FocusSlot(int slot);
	/* PLACEHOLDER names: each icon's effect resumed (0x810B7F7A), and each icon's image
	   reloaded (0x810D2C3E) */
	void ResumeIcons();
	void ReloadIcons();
	/* PLACEHOLDER names: whether an icon fits after the last one (not the last, empty page,
	   and its last slot free; 0x810B7F94), and the icon in the last slot used (0x810B7FA8) */
	bool CanAddIcon();
	paf::common::SharedPtr<ApplauncherIcon> GetLastIcon();
	/* PLACEHOLDER name: the slot after the last one used, -1 when the last is (0x810B6ACC) */
	int GetFreeSlot();
	/* PLACEHOLDER names: an icon made for the data and put in its data's slot, none when the
	   page is full (0x810B4B24), the icons' focus moves to each other set (0x810B3BF0), and what the page
	   does once an icon is added: nothing (an empty function, 0x814098DC) */
	paf::common::SharedPtr<ApplauncherIcon> AddIcon(const LsdbIconPtr &data);
	void UpdateFocusLinks();
	void OnIconAdded();
	/* PLACEHOLDER names: the first free slot, -1 when none is (0x810B4702), and the icon taken off
	   its page and put in the slot (the first free one for -1), its widget moved onto the page
	   when attach is set, into the folder open on the page when toFolder is (0x810B471A) */
	int GetEmptySlot();
	/* the icon's move begun on the page (Sony's name, from its message; 0x810B4CBE) */
	void iconMoveBegin(const paf::common::SharedPtr<ApplauncherIcon> &icon);
	/* PLACEHOLDER name (after iconMoveBegin): the dragged icon moved over the page's slots, the
	   page it leaves for or opens given back, NULL when it stays (0x810B551E) */
	ApplauncherPage *iconMove(const paf::common::SharedPtr<ApplauncherIcon> &icon, bool held,
				 int phase);
	bool PutIcon(const paf::common::SharedPtr<ApplauncherIcon> &icon, int slot, bool attach,
		     bool toFolder);
	/* PLACEHOLDER name (after iconMove): the dragged icon let go of on the page (0x810B6D62) */
	void iconMoveEnd(const paf::common::SharedPtr<ApplauncherIcon> &icon);
};

/* the home's page indicator: a dot for each page, the page in view's dot the theme's current
   page image (0x810C24D8-0x810C3B3C, applauncher_indicator.cpp; names ours) */
class ApplauncherPageIndicator {
public:
	ApplauncherPageIndicator(paf::Plugin *plugin, paf::ui::Widget *widget);
	~ApplauncherPageIndicator();
	/* the theme's dot images loaded again, the dots refreshed (0x810C36B8) */
	void Update();
	/* PLACEHOLDER name: the dots shown over the time (0x810C2C2C) */
	void Show(float time);
	/* PLACEHOLDER name: the dots hidden over the time (0x810C2CD6) */
	void Hide(float time);
	/* PLACEHOLDER name: the dots put in the pages' order, the page in view's dot given the
	   current page image and the others the base one (0x810C2D96) */
	void Refresh();
	/* PLACEHOLDER name: a dot opened for the page of the key, grown in (0x810C3160) */
	void AddDot(int key);
	/* PLACEHOLDER name: the dot of the key faded out and shrunk, then destroyed (0x810C34C0) */
	void RemoveDot(int key);

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_box;			/* the dots' box */
	bool m_hasBaseImage;			/* the theme gives the dots' images */
	bool m_hasCurImage;
	paf::intrusive_ptr<paf::cache::CacheImageInterface> m_baseImage;
	paf::intrusive_ptr<paf::cache::CacheImageInterface> m_curImage;
};

/*
 * PLACEHOLDER: the page a folder's icon opens over the home, its icons read from the folder's
 * page in the database (160 bytes, vtable 0x8152F2D4). The home keeps it as its ApplauncherPage
 * (m_appPage), its icon read through applauncherPageAsFolder: in Sony's build both are made on
 * ApplauncherPageBase, and the page methods it calls (AddIcon, UpdateFocusLinks, Load,
 * BeginEdit) read only that base.
 */
class ApplauncherFolderPage : public ApplauncherPageBase {
public:
	/* the page opened from the icon, the icon of the title given the focus (0x810BA824) */
	ApplauncherFolderPage(const paf::common::SharedPtr<ApplauncherIcon> &icon, const char *name,
			      bool select);
	virtual ~ApplauncherFolderPage();
	/* no page buttons on a folder's page (the shell's empty function 0x814098DC) */
	virtual void Delete(void)
	{
	}
	virtual void SetTheme(void)
	{
	}
	virtual void AddPage(void)
	{
	}
	virtual int GetIndex(void)
	{
		return m_index;
	}
	virtual paf::ui::Widget *GetWidget(void);
	virtual bool IsOpened(void);
	virtual paf::math::v2 GetSlotPos(int slot);

	/* PLACEHOLDER name: the folder's icons put on the page (0x810BB428) */
	void SetIcons(const LsdbIconListPtr *icons);

	ApplauncherPage *AsPage()
	{
		return static_cast<ApplauncherPage *>(static_cast<ApplauncherPageBase *>(this));
	}

	paf::common::SharedPtr<ApplauncherIcon> m_icon;	/* the folder's icon it opened from */
	int m_index;				/* the folder's page */
	paf::string m_focusTitle;		/* PLACEHOLDER: the title given the focus */
	bool m_select;				/* PLACEHOLDER: that icon selected too */
	bool m_opened;				/* PLACEHOLDER: its icons put on it */
	paf::ui::Widget *m_widget;
	paf::common::SharedPtr<paf::job::JobItem> m_job;	/* its icons read */
	paf::math::v4 m_iconPos;		/* where the folder's icon stood */
};

/* a page of type 2 read as the folder page it is */
static inline ApplauncherFolderPage *applauncherPageAsFolder(ApplauncherPageBase *page)
{
	return static_cast<ApplauncherFolderPage *>(page);
}

/* PLACEHOLDER: where the slot's icon stands on a folder page, without one (0x810BB24A), and on
   a page of the home, at the home's scale (0x810D3B90, in a file not written yet) */
paf::math::v2 applauncherFolderPageGetSlotPos(int slot);
paf::math::v2 applauncherPageGetSlotPos(int slot);
/* PLACEHOLDER: the folder renamed (0x810D00CE, in a file not written yet) */
void applauncherFolderRename(ApplauncherIcon *folder, paf::wstring &name);
/* PLACEHOLDER name: the timer's widget destroyed (its DestroyWidget; 0x810D40BC, the home
   page's file) */
void applauncherDestroyWidgetCB(SceInt32 type, paf::ui::Handler *self, paf::ui::Event *e,
				void *data);

/* the listener of the home's input (its methods out of line, 0x810C9B58; applauncher_input.cpp) */
class ApplauncherInputListener : public paf::inputdevice::InputListener {
public:
	ApplauncherInputListener();
	virtual void OnUpdate(const paf::inputdevice::Data *data);
};

/* the home screen (272 bytes, made by applauncherPageCreate) */
class ApplauncherHome {
public:
	ApplauncherHome(paf::Plugin *plugin);
	~ApplauncherHome();

	/* the home's edit mode entered (0x810A9EC4) */
	void EnterEditMode();
	/* the notice page's text set from the count of applications and the pages' room
	   (0x810AA258) */
	void UpdateNoticeText();
	/* the notice page updated, or made at the end when applications wait (0x810AA5AA) */
	void ShowNotice();
	/* an edit button faded out, deleted after its fade (0x810AA6B6) */
	void HideEditButton(paf::ui::Widget *button);
	/* the edit's open and decide buttons in the widget made or faded out by the edit state
	   (0x810AA81C) */
	void UpdateEditButtons(paf::ui::Widget *parent);
	/* the edit's back and cancel buttons made or faded out the same way (0x810AB1B6) */
	void UpdateBackButtons(paf::ui::Widget *parent);
	/* the edit state set: the edit's bar slid in or out and its buttons made for the state
	   (0x810ABB64) */
	void SetEditState(int state);
	/* the edit state asked for, set while a focus is shown, else none (0x810ACC5E) */
	void RequestEditState(int state);
	/* the home's edit mode left (0x810ACC98) */
	void ExitEditMode();
	/* the background's change begun: the view faded out (left when the image stays), the icons undimmed and the change timed (0x810ACFD8) */
	void BeginBackgroundChange(bool keepImage);
	/* the background changed to the page's when its image or wave differs (0x810AD11E) */
	void UpdateBackground(ApplauncherPage *page);
	/* the title's icon on a page, else on the application's page, and the page it is on
	   (0x810AE89C) */
	paf::common::SharedPtr<ApplauncherIcon> FindIcon(const paf::string &titleId,
							 ApplauncherPage **page);
	/* the title's icon on the application's page, else on a page or in a folder of one, the
	   folder and the page given back (0x810AEA46) */
	paf::common::SharedPtr<ApplauncherIcon> FindIconInFolder(const paf::string &titleId,
		paf::common::SharedPtr<ApplauncherIcon> *folder, ApplauncherPage **page);
	/* the title's application taken off: livespace_db told, its icon (or its folder) deleted,
	   and outside sorting the notice updated and the empty pages deleted (0x810AEBFA) */
	int RemoveApp(const paf::string &titleId);
	/* each page's icons given their data, the notice updated (0x810AF1CA) */
	void UpdatePageIcons(
		const paf::vector<paf::common::SharedPtr<LsdbIconList> > &pages);
	/* the view's shows held, once (0x810AF220), and let go of (0x810AF23A) */
	void HoldView();
	void ReleaseView();
	/* the overlay over the pages made once (0x810A9BE0; it reads the home through
	   g_applauncherHome, as its callers pass it) */
	void CreateOverlay();
	/* the background taken down (0x810B09B8), and shown (0x810B0A46) */
	void ClearBackground();
	void ShowBackground();
	/* the page's background loaded (0x810B0B28) */
	bool LoadBackground(ApplauncherPage *page);
	/* the application's icon on any page (0x810AFE82) */
	paf::common::SharedPtr<ApplauncherIcon> FindIcon(SceUInt64 appId);
	/* PLACEHOLDER name: the icon of a page whose data's state has the id (0x810B17F2) */
	paf::common::SharedPtr<ApplauncherIcon> FindIconById(int id);
	/* the snaps of the pages set: in idu mode only pages with a shown icon, else all (0x810AD1D2) */
	void UpdateSnaps();
	/* PLACEHOLDER name: the home updated each frame on the main thread (0x810B1D32) */
	void Update();
	/* the application's page opened from its icon, unless it is in view (0x810B04F8) */
	void OpenApp(const paf::common::SharedPtr<ApplauncherIcon> &icon, const paf::string &name,
		     bool animate);
	/* the title's icon given the focus, its page scrolled to; in a folder, the folder's
	   page opened (0x810B0578) */
	void FocusApp(const paf::string &titleId, bool select);
	/* the pages next to the one in view loaded, the others let go of */
	void UpdateLoadedPages(bool fast);

	paf::Plugin *m_plugin;
	ShellPluginInterface *m_topmenu;
	ShellPluginInterface *m_powerManage;
	ShellPluginInterface *m_livearea;	/* fetched when first wanted */
	ApplauncherPageScroller m_scroller;
	float m_scale;
	int m_layer;				/* topmenu's layer the home is drawn in */
	int m_snapId;				/* PLACEHOLDER: the snap ID of the page in view */
	int m_backgroundState;			/* PLACEHOLDER: 2 while the background changes */
	paf::intrusive_ptr<paf::cache::CacheImageInterface> m_task;	/* the background image's cache entry */
	paf::string m_imagePath;
	int m_waveType;
	float m_texU;				/* PLACEHOLDER: the image's area in its texture */
	float m_texV;
	float m_texScale[2];
	float m_alpha;
	paf::ui::Widget *m_background;
	paf::ui::ScrollView *m_scrollView;
	paf::ui::Widget *m_foreground;
	paf::ui::Widget *m_editBar;		/* PLACEHOLDER: the edit's bar of buttons */
	int m_editRequest;			/* PLACEHOLDER: the edit state asked for */
	int m_editState;			/* PLACEHOLDER: 2 opens the edit, 4 and 5 decide it */
	ApplauncherPageIndicator *m_indicator;
	bool m_ready;
	int m_mode;
	float m_fade[2];			/* PLACEHOLDER: the wave's fade, and eased (quad out) */
	float m_fadeTime;			/* PLACEHOLDER: seconds since the fade began (m_mode 0 in, 1 out) */
	paf::ui::Widget *m_overlay;
	bool m_homeFlags[3];
	int m_waitDialog;			/* the waiting dialog's ID, -1 without one */
	bool m_imageCached;			/* PLACEHOLDER: the background image taken from the surface pool */
	paf::math::v2 m_dragPos;
	int m_waitingAppCount;			/* PLACEHOLDER: the applications with no room yet */
	ApplauncherPage *m_noticePage;		/* PLACEHOLDER: the page saying why no more fit */
	int m_appCount;				/* PLACEHOLDER: the applications on the home */
	int m_folderCount;			/* PLACEHOLDER: the folders on the home (the full home's text) */
	bool m_viewHeld;			/* PLACEHOLDER: the view's shows held */
	bool m_waveUpdated;
	int m_dropIndex;
	int m_dropPage;
	bool m_openAnimate;			/* PLACEHOLDER: the opening kept animated */
	paf::string m_appName;
	ApplauncherPage *m_appPage;		/* PLACEHOLDER: the page of the application named */
	paf::common::SharedPtr<ApplauncherIcon> m_appIcon;	/* PLACEHOLDER: the closed page's focus */
	float m_appIconTime;			/* PLACEHOLDER: seconds it has waited for its widget */
	LsdbEventListener *m_lsdbListener;
};

/* the icon being dragged, while one is (0x81555254) */
extern paf::common::SharedPtr<ApplauncherIcon> g_applauncherDragItem;
/* PLACEHOLDER: the icon moved aside for the dragged one (0x8155525C, the page's file) */
extern paf::common::SharedPtr<ApplauncherIcon> g_applauncherMoveIcon;
/* PLACEHOLDER: the icon the dragged one is over (0x81555264, the page's file) */
extern paf::common::SharedPtr<ApplauncherIcon> g_applauncherDropTarget;
/* PLACEHOLDER names: the time the icons move aside from an opened one in, 375 ms
   (0x8150781C), the icons' move time, 275 ms (0x81507820), and the time they come back in,
   325 ms (0x81507824); read here and in the page's file, defined in a file of times not
   written yet */
extern const float g_applauncherIconAsideTime;
extern const float g_applauncherIconMoveTime;
extern const float g_applauncherIconRestoreTime;
/* PLACEHOLDER: the time a view is held for before its hold event, 1000 ms (0x81507DE8, the
   icon pool's file) */
extern float g_applauncherIconHoldTime;
/* PLACEHOLDER: the time an icon moves to its slot in the edit, 750 ms (0x81507478, in a file
   of shared times beside 0x8150746C-0x81507474, not written yet) */
extern const float g_applauncherEditMoveTime;
/* PLACEHOLDER: the depth the icons stand at in the edit, -200 (0x81507474, the same file) */
extern const float g_applauncherEditDepth;
/* PLACEHOLDER: the time an icon moves into a folder in, 250 ms (0x8150746C, the same file;
   PutInFolder and 0x810D2C58) */
extern const float g_applauncherFolderMoveTime;
/* PLACEHOLDER: the icons' tilt effect moved on a frame (0x810CF2AA), and the drag left
   when the dragged widget is in its drag mode (0x810D1DE6) */
void applauncherIconEffectUpdate(void);
void applauncherIconCancelDrag(void);
/* PLACEHOLDER: the pages' first load done (0x810B3502), and the home's start marked done
   once (0x810B3B36), both in the page's file */
bool applauncherPagesLoaded(void);
void applauncherPagesSetStarted(void);
/* PLACEHOLDER: the applauncher's request kept, outside the IDU mode and without one
   (0x810B8830, applauncher_page.cpp). Its one argument is not read: an icon's view gives it as
   an event callback (the event's type), the page views their page */
void applauncherPagesHoldRequest(void *arg);
/* PLACEHOLDER: the IconCacheManager, its manager first (0x815553B0, the icon cache's file) */
class ApplauncherIconCache {
public:
	/* (0x810BD248) */
	ApplauncherIconCache(paf::Plugin *plugin);

	paf::cache::CacheManager *m_manager;
};
extern ApplauncherIconCache *g_applauncherIconCache;
/* PLACEHOLDER: the dragged icon's widget put back in its icon mode and the home told whether a
   folder or an icon was let go (0x810BCB44) */
void applauncherIconWidgetLeaveDrag(ApplauncherIconView *view);
/* PLACEHOLDER: an icon made for the data, on the plugin (0x810CFD82, the icon's file) */
paf::common::SharedPtr<ApplauncherIcon> applauncherIconCreate(paf::Plugin *plugin,
							 const LsdbIconPtr &data);
/* PLACEHOLDER: an icon's size on a folder's page (0x810BBE8C) and on a page (0x810BBEAE), by
   the model; its label's colour from its page's (0x810BCB94); and its mark set by the sort
   (0x810BBE66) (the icon widget's file) */
float applauncherIconWidgetFolderSize(void);
float applauncherIconWidgetSize(void);
void applauncherIconWidgetUpdateLabel(ApplauncherIconView *view);
void applauncherIconWidgetUpdateMark(ApplauncherIconView *view);
/* PLACEHOLDER: a free view taken for the icon (0x810BBED0), and given back, shrunk away first
   when animated (0x810BC560) (the icon pool's file) */
ApplauncherIconView *applauncherIconPoolAlloc(const paf::common::SharedPtr<ApplauncherIcon> &icon);
void applauncherIconPoolFree(ApplauncherIconView *view, bool animate);
/* PLACEHOLDER: a view put in the edit, its pose taken when animated (0x810BC8D0), and out
   (0x810BCA36); its label set (0x810BCA56); dropped in (0x810BCA64); its icon's position
   (0x810BCA72); and its drag begun (0x810BCABA) (the icon pool's file) */
void applauncherIconWidgetBeginEdit(ApplauncherIconView *view, bool animate);
void applauncherIconWidgetEndEdit(ApplauncherIconView *view);
void applauncherIconWidgetSetLabel(ApplauncherIconView *view, const paf::wstring &label);
void applauncherIconWidgetDropIn(ApplauncherIconView *view);
void applauncherIconWidgetSetIconPos(ApplauncherIconView *view, const paf::math::v3 &pos);
void applauncherIconWidgetBeginDrag(ApplauncherIconView *view);

/* the home, while the plugin runs (0x81555154) */
extern ApplauncherHome *g_applauncherHome;

/* the page taken off the home, unless it is the last; the page after it (0x810A8ABE) */
ShellSnapList::iterator applauncherHomeDeletePage(ApplauncherPage *page);
/* a page made from the parameters (or the theme's) and put in after the other (0x810A9812) */
ApplauncherPage *applauncherHomeAddPage(const LsdbPage *param, ApplauncherPage *after,
					bool focus, bool save);

/* the page after it, the page itself at the end (0x810ACF7C) */
/* the page before the other; the page itself when it is the first (0x810A9B6A) */
ApplauncherPage *applauncherHomeGetPreviousPage(ApplauncherPage *page);
ApplauncherPage *applauncherHomeGetNextPage(ApplauncherPage *page);
/* the parameters of the page in view copied out (0x810ACFAA), and set (0x810AD192) */
void applauncherHomeGetPageParam(LsdbPage *param);
void applauncherHomeSetPageParam(const LsdbPage *param);
/* PLACEHOLDER: the background's change timed out: the page in view's background shown
   (0x810B1964) */
void applauncherHomeOnBackgroundTimeout(SceInt32 type, paf::ui::Handler *self, paf::ui::Event *e,
					void *data);
/* the application's page closed: the dots shown, its icons put back, the view shown, its focus
   kept when asked, and the overlay faded out outside the edit (0x810AD2CC) */
void applauncherHomeCloseAppPage(bool keepFocus);
/* the view moved to the page at once, unless it is the application's page (0x810AD466) */
void applauncherHomeJumpTo(ApplauncherPage *page);
/* PLACEHOLDER: the folder page closed, back into its icon (0x810BAEFC, ApplauncherFolderPage's
   file) */
void applauncherIconPoolCloseAppPage(ApplauncherPage *page);
/* PLACEHOLDER: the image of the path loaded by the DataCacheManager into the image's cache, on
   the queue (NULL: the manager's), read straight from the file when directLoad is set
   (0x810BA5A2, applauncher_datacache.cpp) */
int applauncherDataCacheLoad(const paf::string &path,
			     paf::intrusive_ptr<paf::cache::CacheImageInterface> *image,
			     const paf::cache::CacheImage::Option *option, paf::job::JobQueue *queue,
			     bool directLoad);
/* the background of the page in view loaded again (0x810B0CF6) */
void applauncherHomeReloadBackground(void);
/* the applications with no room yet set, the notice shown for them (0x810AFA08) */
void applauncherHomeSetWaitingAppCount(int count);
/* the title's icon given its new data, else its image loaded and given to topmenu
   (0x810AFA24) */
void applauncherHomeUpdateApp(const paf::string &titleId, const LsdbIconPtr &data);
/* the pages' background images listed (0x810AFC08) */
void applauncherHomeGetPageImages(paf::vector<paf::string> *paths);
/* the empty pages deleted, the last kept (0x810AFCA0) */
void applauncherHomeDeleteEmptyPages(void);
/* the application's page opened from its icon (0x810AFD30), and the one open (0x810AFDEA) */
ApplauncherPage *applauncherHomeOpenAppPage(const paf::common::SharedPtr<ApplauncherIcon> &icon,
					    const char *name, bool animate);
ApplauncherPage *applauncherHomeGetAppPage(void);
/* the closed application page's focus let go of when it is the icon (0x810AFDFA) */
void applauncherHomeClearAppIcon(const paf::common::SharedPtr<ApplauncherIcon> &icon);
/* the interface's RemoveApp: the title's application taken off (0x810AFF80) */
int applauncherInterfaceRemoveApp(const paf::string &titleId);
/* the interface's entry #4: the title's icon given the focus (0x810B071E); entry #5: its selected
   mark cleared (0x810B0736) */
void applauncherInterfaceFocusApp(const paf::string &titleId, bool select);
void applauncherInterfaceUnselectApp(const paf::string &titleId);
/* PLACEHOLDER names: the interface's entry #7 and entry #8, the icon effect of the title ended
   (0x810B07FC), and any (0x810B0804) */
void applauncherInterfaceEndIconEffect(const paf::string &titleId);
void applauncherInterfaceEndIconEffects(void);
/* the interface's entry #9 and entry #10: the view scrolled to the notice page (0x810B080C), and to
   the first page (0x810B0844) */
void applauncherInterfaceScrollToNotice(void);
void applauncherInterfaceScrollToFirst(void);
/* the interface's entry #11: the icon image of the application's folder loaded (0x810B088E) */
void applauncherInterfaceLoadIconImage(const char *dir,
				       paf::intrusive_ptr<paf::cache::CacheImageInterface> *image);
/* PLACEHOLDER: the interface's entry #3: the image of the title's icon, else the image asked of
   the pages; false without either (0x810B0896) */
bool applauncherInterfaceGetIconImage(paf::intrusive_ptr<paf::cache::CacheImageInterface> *image,
				      const paf::string &titleId, void *param, void *data);
/* PLACEHOLDER: the icon effect of the title ended (0x810CE48A), and any (0x810CE4E2) */
void applauncherIconEffectEnd(const paf::string &titleId);
void applauncherIconEffectEndAll(void);
/* PLACEHOLDER: the icon image (sce_sys/icon0.png) of the application's folder loaded into the
   icon cache (0x810D1F44) */
void applauncherIconLoadImage(const char *dir,
			      paf::intrusive_ptr<paf::cache::CacheImageInterface> *image);
/* PLACEHOLDER: the image of the title asked of the pages (0x810B350E, the page's file) */
void applauncherPagesRequestImage(const paf::string &titleId, void *param, void *data);

/* PLACEHOLDER names: the interface's entry #12, the title's icon marked for an update
   (0x810B0DB2), and entry #13, its data asked for again, of the pages without an icon
   (0x810B0EA2) */
int applauncherInterfaceMarkAppUpdate(const char *titleId);
int applauncherInterfaceRequestAppData(const char *titleId);
/* PLACEHOLDER: the title's data asked of the data cache, but for Welcome Park (0x810B33D6, the
   page's file) */
void applauncherPagesRequestData(const paf::string &titleId);

/* PLACEHOLDER names: the interface's entry #14, every page's icon images reloaded (0x810B0FD4);
   entry #15, the kept request's state cleared (0x810B1026); entry #17, the wave hidden and the view
   shown again (0x810B103E) */
void applauncherInterfaceReloadIcons(void);
void applauncherInterfaceClearRequest(void);
void applauncherHomeHideWave(void);

/*
 * PLACEHOLDER: what a title's change log opened over another application calls once it closes:
 * the title, the value and argument it was opened with, and 1
 */
typedef int (*ApplauncherChangeLogCallback)(const paf::string *titleId, int value, void *arg,
					    int closed);

/* PLACEHOLDER names: the interface's entry #18, the index of the page in focus, -1 without one
   (0x810B10E2); entry #19, the application's page closed (0x810B1110); entry #21, whether the title
   is on the home, as an icon or in a folder (0x810B1126); entry #22, the title's change log opened
   (0x810B12B4); entry #23, the theme reloaded and its UI refreshed on the main thread
   (0x810B12C6); entry #24, the theme reloaded (0x810B12FA) */
int applauncherInterfaceGetFocusedPage(void);
void applauncherInterfaceCloseAppPage(void);
bool applauncherInterfaceHasApp(const paf::string &titleId);
int applauncherInterfaceOpenChangeLog(const paf::string *titleId, void *loadArg,
				      ApplauncherChangeLogCallback callback, int value, void *arg);
int applauncherInterfaceRefreshTheme(void);
int applauncherInterfaceReloadTheme(void);
/* PLACEHOLDER name: the interface's entry #26, the kept request marked (0x810B17D8) */
int applauncherInterfaceSetRequest(void);
/* PLACEHOLDER: what a theme read by applauncherThemeReload gives the home: each page's
   background and each of its icons' data, in the order of shellThemeGetIconTitles */
struct ApplauncherThemeData {
	paf::vector<paf::common::SharedPtr<LsdbPage> > m_pages;
	paf::vector<LsdbIconPtr > m_apps;
};

/* PLACEHOLDER: the theme read last applied to the pages and icons, and let go of
   (0x810B1302) */
void applauncherHomeApplyTheme(void);
/* PLACEHOLDER names: the titles a theme gives icons for (twenty), their count given
   (0x813BC2DC); the IconCacheManager's image of the path cleared (0x810BD498), and room made in
   its surface pool (0x810BD4AE) */
const char **shellThemeGetIconTitles(SceUInt32 *count);
void applauncherIconCacheClearPath(const paf::string &path);
void applauncherIconCacheMakeSpace(void);

/* PLACEHOLDER: topmenu_plugin's function that gives the object held while a dialog of the home
   (the change log, "about this game") is open; its 28th virtual function lets it go */
#define TOPMENU_PLUGIN_FUNC_HOLD		51

struct ApplauncherTopmenuHold {
	void (**vtable)(ApplauncherTopmenuHold *hold);
};

#define APPLAUNCHER_TOPMENU_HOLD_RELEASE	27

/* PLACEHOLDER names: a title's change log opened over another application, the dialog's ID
   given back (0x810B965A); the icon's opened over the home (0x810B95CA); the open one closed
   (0x810B9882) */
int applauncherChangeLogOpen(const paf::string *titleId, void *loadArg,
			     ApplauncherChangeLogCallback callback, int value, void *arg);
class ApplauncherChangeLog *applauncherChangeLogOpenIcon(
	const paf::common::SharedPtr<ApplauncherIcon> &icon);
void applauncherChangeLogClose(void);
/* PLACEHOLDER names: the icon's "about this game" opened over the home, made with the icon
   (0x810CB864), and the open one closed (0x810CBAB4) */
class ApplauncherAboutThisGame *applauncherAboutThisGameOpen(
	const paf::common::SharedPtr<ApplauncherIcon> &icon);
void applauncherAboutThisGameClose(void);
/* PLACEHOLDER names: the modes of an icon's "about this game" (0x810C876C) and change log
   (0x810C8934) asked for, the icon kept for the mode */
void applauncherAboutThisGameEnter(const paf::common::SharedPtr<ApplauncherIcon> &icon);
void applauncherChangeLogEnter(const paf::common::SharedPtr<ApplauncherIcon> &icon);
/* PLACEHOLDER names: the icon's context menu opened over the home (0x810D4568), and closed
   (0x810D474A); its mode asked for, the icon kept for the mode (0x810C8C3E); and whether the
   game plugin's dialog hold is kept when the mode ends (0x810C8CF8) */
void applauncherContextMenuOpen(paf::common::SharedPtr<ApplauncherIcon> icon);
void applauncherContextMenuClose(void);
void applauncherContextMenuEnter(const paf::common::SharedPtr<ApplauncherIcon> &icon);
void applauncherContextMenuKeepHold(bool keep);
/* PLACEHOLDER names: an icon's deletion begun and ended, the edit kept on a power event while
   one is under way (0x810C8EF4, 0x810C8F04) */
void applauncherEditModeHold(void);
void applauncherEditModeRelease(void);
/* PLACEHOLDER names: the icon's delete dialog ("DeleteDialog") opened over the home, with what
   the context menu gave (a pointer it keeps, and a value) (0x810D61D0), and closed
   (0x810D62EA); its mode asked for, the three kept for the mode (0x810C9114) */
class ApplauncherDeleteDialog *applauncherDeleteDialogOpen(
	const paf::common::SharedPtr<ApplauncherIcon> &icon, void *context, int value);
void applauncherDeleteDialogClose(void);
void applauncherIconDelEnter(const paf::common::SharedPtr<ApplauncherIcon> &icon, void *context,
			     int value);
/* PLACEHOLDER names: the icon's information opened over the home, with the context menu's
   pointer (0x810D9C70), and closed (0x810D9E84); its mode asked for, the two kept for the mode
   (0x810C937C) */
class ApplauncherIconInfo *applauncherIconInfoOpen(
	const paf::common::SharedPtr<ApplauncherIcon> &icon, void *context);
void applauncherIconInfoClose(void);
void applauncherIconInfoEnter(const paf::common::SharedPtr<ApplauncherIcon> &icon,
			      void *context);

/* PLACEHOLDER: a version's change information: its app_ver as a number (1.23 is 123), and its
   text */
struct ApplauncherChangeInfoEntry {
	ApplauncherChangeInfoEntry(SceUInt32 version, const paf::string &text)
		: version(version), text(text)
	{
	}

	SceUInt32 version;
	paf::string text;
};

/*
 * PLACEHOLDER: an application's change information (its sce_sys/changeinfo XML), read in a job
 * and given as the RichText's marked-up text (applauncher_changeinfo.cpp, 0x810C9E24-0x810CAC4A).
 * The constructor is the vector's.
 */
class ApplauncherChangeInfo {
public:
	/* out of line in Sony's build (folded at 0x814097DC) */
	__attribute__((noinline)) ApplauncherChangeInfo()
	{
	}

	~ApplauncherChangeInfo();				/* 0x810C9E26 */
	/* the XML read in the buffer, the versions after since kept, newest first: 0, or a
	   negative error (0x810C9EB6) */
	int Parse(const char *buf, int size, SceUInt32 since);
	/* the XML of the language (else the default one) in the directory read, the versions after
	   the one arg gives kept: 0, or a negative error (0x810C9F82) */
	int Load(paf::job::JobItem *job, const char *dir, int language, void *arg);
	/* a version's text kept (0x810CA79C) */
	int AddEntry(SceUInt32 version, const paf::string &text);
	/* the version ("1.23") as the plugin's heading over its text shows it (0x810CA80A) */
	paf::string FormatVersion(const paf::string &version);
	/* each version's heading and text, one after the other (0x810CA930) */
	void GetText(paf::string *text);

	paf::vector<ApplauncherChangeInfoEntry *> m_entries;
};

/* PLACEHOLDER: the version as "1.23" (or "12.34"), a version over 99.99 an error (0x810CAE92) */
int applauncherChangeInfoVersionToString(SceUInt32 version, paf::string *text);

/*
 * PLACEHOLDER: a dialog's handler that calls the function with the dialog's value, the button and
 * its argument (0x810BA6FC, 0x810BA710)
 */
class ApplauncherDialogHandler : public CommonGuiDialogHandler {
public:
	ApplauncherDialogHandler(CommonGuiDialogFunction func, void *arg);

	virtual ~ApplauncherDialogHandler()
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button);

	CommonGuiDialogFunction m_func;
	void *m_arg;
};

/*
 * PLACEHOLDER: a message dialog's open handler: the dialog shows the text of the error its type
 * holds, with no default or cancel button, when the error has one of its own (0x810BA724,
 * 0x810BA76A)
 */
class ApplauncherErrorOpenHandler : public CommonGuiMessageDialogOpenHandler {
public:
	ApplauncherErrorOpenHandler();

	virtual void operator()(CommonGuiMessageDialogParam *param);
};

/* PLACEHOLDER: the resource ID of an error's text, the generic one for the others (0x810BA734) */
SceUInt32 applauncherGetErrorTextId(int error);
#define APPLAUNCHER_ERROR_TEXT_GENERIC		0xC51AA4A4
/* PLACEHOLDER: the theme's UI refreshed, on the main thread (0x810B28B0) */
void applauncherHomeOnRefreshThemeUI(void *data);
/* PLACEHOLDER: a job's end, the semaphore given released (0x810B28B8) */
class ApplauncherLsdbJob;
void applauncherHomeOnJobDone(ApplauncherLsdbJob *job, void *data);

/* PLACEHOLDER: the interface's entry #2: livearea_plugin waited for and its LiveArea opened, the
   icon of the title in focus faded in once its image is ready (0x810AFF96) */
int applauncherInterfaceOpenLiveArea(const LsdbAppInfoPtr *app, bool animate);
/* PLACEHOLDER: the icon's image ready or given up: the opening let go on (0x810B28C2) */
void applauncherHomeOnIconImage(SceInt32 type, paf::ui::Handler *self, paf::ui::Event *e,
				void *data);
/* livearea_plugin's GetInterface(1), fetched once (0x810A971E) */
ShellPluginInterface *applauncherHomeGetLivearea(void);
/* PLACEHOLDER: the folder page made from its icon, the one open closed first (0x810BAEBE,
   ApplauncherFolderPage's file) */
ApplauncherPage *applauncherIconPoolOpenAppPage(const paf::common::SharedPtr<ApplauncherIcon> &icon,
						const char *name, bool animate);
/* PLACEHOLDER: the IconCacheManager's image of the path, loaded (0x810BD3A6) and cleared
   (0x810BD482) */
int applauncherIconCacheLoad(const paf::string &path,
			     paf::intrusive_ptr<paf::cache::CacheImageInterface> *image,
			     const paf::cache::CacheImage::Option *option, paf::job::JobQueue *queue,
			     bool noSurface);
void applauncherIconCacheClear(paf::intrusive_ptr<paf::cache::CacheImageInterface> &image);

/* the overlay faded in (0x810AF160), and out (0x810AF184) */
void applauncherHomeShowOverlay(void);
void applauncherHomeHideOverlay(void);
/* PLACEHOLDER: the pages kept loaded set to the count (0x810B3338, the page's file) */
void applauncherPagesSetCount(int count);

/* PLACEHOLDER: the icon taken off its page (0x810B4408), and deleted (0x810CFE1C) */
void applauncherIconRemove(paf::common::SharedPtr<ApplauncherIcon> icon, bool fromFolder);
/* PLACEHOLDER: the icon taken off its page alone, no folder and no focus moves (0x810B4C02) */
void applauncherIconDetach(ApplauncherIcon *icon);
void applauncherIconDelete(paf::common::SharedPtr<ApplauncherIcon> &icon, bool removeEntry,
			   bool animate);
/* PLACEHOLDER: the folders' icons drawn again from their children and saved (0x810B7FE0) */
void applauncherPagesRemoveIcons(paf::vector<ApplauncherRemovedIcon> *icons);
/* PLACEHOLDER: livespace_db's export, not declared there yet: the title's application taken
   off, whole with its folder or not */
extern "C" int SceLsdb_DE0DA538(AppInfoSClient *client, const paf::string *titleId,
	LsdbIconPtr *icon, int *pageNo, bool theme);
/* PLACEHOLDER: livespace_db's export, not declared there yet: the icon image of the row, as a
   shared blob, the result in *pResult */
extern "C" paf::common::SharedPtr<DbBlob> SceLsdb_41185388(AppInfoSClient *client,
	const SceInt64 *rowid, int *pResult);
/* PLACEHOLDER: livespace_db's export, not declared there yet: the pages' rows read from the
   page given */
extern "C" int SceLsdb_8FAD093B(AppInfoSClient *client, int pageNo, unsigned int flags,
	LsdbPageList *pages);
/* PLACEHOLDER: livespace_db's export, not declared there yet: an icon's fields written, its
   image given */
/* PLACEHOLDER: livespace_db's export, not declared there yet: the pages' rows written */
extern "C" int SceLsdb_6769DBC4(AppInfoSClient *client, paf::vector<int> *pageNos,
	LsdbPageList *pages);
/* PLACEHOLDER: livespace_db's export, not declared there yet: icons moved */
extern "C" int SceLsdb_BF7AFB42(AppInfoSClient *client, paf::vector<int> *fromPages,
	paf::vector<int> *fromPositions, paf::vector<int> *toPages, paf::vector<int> *toPositions);
extern "C" int SceLsdb_C1A82AC8(AppInfoSClient *client, int pageNo, const LsdbIconPtr *icon,
	unsigned int fields, const void *image, int imageSize);
extern "C" int SceLsdb_B33FE18E(AppInfoSClient *client, const paf::string &titleId, bool whole, int flags);
/* PLACEHOLDER: livespace_db's exports, not declared there yet: a page added at a number, and a
   folder made of an icon */
extern "C" int SceLsdb_3387B5A3(AppInfoSClient *client, int pageNo, LsdbIconList *icons,
	LsdbPage *page);
extern "C" int SceLsdb_E786531F(AppInfoSClient *client, int pageNo, const LsdbIconPtr *icon,
	const LsdbIconPtr *folderIcon, const void *image, int imageSize);

/* PLACEHOLDER: an icon's type for the power icon, whose title ID the home reads as "power" */
#define APPLAUNCHER_ICON_TYPE_POWER	6

/* PLACEHOLDER: a system application's icon (NPXS10xxx below 150, PCSI90016, PS4 Link on a
   demo unit; any icon not an application's) (0x810CF650) */
int applauncherIconIsSystem(const LsdbIconPtr *icon);
/* PLACEHOLDER: the home's titles in their default order, ten to a page (0x810C996C) */
int applauncherDefaultLayout(paf::vector<paf::string> *titles);

/* PLACEHOLDER: the 42 icons of the pool dimmed or not (0x810BCC60, the icon pool's file) */
void applauncherIconPoolSetDim(bool dim);

/* a DeletePageJob for the page put on the queue (0x811C5986) */
paf::common::SharedPtr<paf::job::JobItem> applauncherDeletePageJobStart(paf::job::JobQueue *queue,
									int page, void *callback,
									void *userData);

/* a CreateNewPageJob for the page, its icons and parameters, put on the queue (0x811C5848) */
paf::common::SharedPtr<paf::job::JobItem> applauncherCreateNewPageJobStart(
	paf::job::JobQueue *queue, int page, const paf::vector<LsdbAppInfoPtr> &icons,
	const LsdbPage &param, void *callback, void *userData);

/* PLACEHOLDER: what the database jobs (0x811C5xxx) keep after paf's JobItem: the function
   called when done with its argument, and what the database returned */
class ApplauncherLsdbJob;
typedef void (*ApplauncherLsdbJobCallback)(ApplauncherLsdbJob *job, void *arg);

class ApplauncherLsdbJob : public paf::job::JobItem {
public:
	ApplauncherLsdbJobCallback m_callback;	/* called with the job when it is done */
	void *m_callbackArg;
	int m_ret;
	bool m_done;
};

/* PLACEHOLDER: the database jobs the pages read through, by the names Sony gives them: the pages
   counted (GetTotalPageJob), the icons of pages (GetPageLayoutJob), their parameters
   (GetPageThemeJob), the icons counted (GetIconNumJob), a title's icon (GetIconInfoJob) */
class GetTotalPageJob : public ApplauncherLsdbJob {
public:
	int m_total;
};

class GetPageLayoutJob : public ApplauncherLsdbJob {
public:
	int m_page;
	int m_count;
	LsdbPageIconLists m_icons;
};

class GetPageThemeJob : public ApplauncherLsdbJob {
public:
	int m_page;
	int m_count;
	LsdbPageList m_pages;
};

class GetIconNumJob : public ApplauncherLsdbJob {
public:
	int m_page;
	int m_count;
};

class GetIconInfoJob : public ApplauncherLsdbJob {
public:
	paf::string m_titleId;
	LsdbIconPtr m_icon;
};

/* PLACEHOLDER: Sony's job name (0x811C6A18 makes it, 0x811C6BF2 puts it on the queue): a
   folder made in the database, its icon the database gives back copied into the one given */
class CreateNewFolderJob : public ApplauncherLsdbJob {
public:
	int m_page;
	LsdbIconPtr m_icon;
	paf::vector<ApplauncherFolderSurface> m_images;	/* the folder icon's images */
	LsdbIconPtr m_folder;		/* the folder's icon the database made */
	LsdbIconPtr m_data;		/* the folder's icon given, filled when done */
};
/* PLACEHOLDER: a CreateNewFolderJob done: its folder's icon copied into the one given, and the
   icon of the same row updated (0x810D1FF6) */
void applauncherIconFolderCreated(ApplauncherLsdbJob *job, void *arg);
paf::common::SharedPtr<paf::job::JobItem> applauncherCreateNewFolderJobStart(
	paf::job::JobQueue *queue, int page, const LsdbIconPtr &icon, const LsdbIconPtr &folder,
	const paf::vector<ApplauncherFolderSurface> &images, ApplauncherLsdbJobCallback callback,
	void *arg);

/* PLACEHOLDER: those jobs put on the queue (0x811C5A3C, 0x811C4E4C, 0x811C5172, 0x811C4F52,
   0x811C5D92) */
paf::common::SharedPtr<paf::job::JobItem> applauncherGetTotalPageJobStart(
	paf::job::JobQueue *queue, ApplauncherLsdbJobCallback callback, void *arg);
paf::common::SharedPtr<paf::job::JobItem> applauncherGetPageLayoutJobStart(
	paf::job::JobQueue *queue, int page, int count, ApplauncherLsdbJobCallback callback,
	void *arg);
paf::common::SharedPtr<paf::job::JobItem> applauncherGetPageThemeJobStart(
	paf::job::JobQueue *queue, int page, int count, ApplauncherLsdbJobCallback callback,
	void *arg);
paf::common::SharedPtr<paf::job::JobItem> applauncherGetIconNumJobStart(
	paf::job::JobQueue *queue, int page, ApplauncherLsdbJobCallback callback, void *arg);
paf::common::SharedPtr<paf::job::JobItem> applauncherGetIconInfoJobStart(
	paf::job::JobQueue *queue, const paf::string &titleId, ApplauncherLsdbJobCallback callback,
	void *arg);
/* PLACEHOLDER: a SetIconInfoJob for the page's icon (the flag's use not read yet) put on the
   queue (0x811C5F38) */
paf::common::SharedPtr<paf::job::JobItem> applauncherSetIconInfoJobStart(
	paf::job::JobQueue *queue, int page, const LsdbIconPtr &icon, int flag,
	ApplauncherLsdbJobCallback callback, void *arg);

/*
 * The moves of icons saved, as livespace_db's AppDbMoveIcons takes them (Sony's job name,
 * SetPageLayoutJob; 0x811C53DA puts it on the queue): each icon's page and position before and
 * after, and the folders' icons taken off; its function run with it (PLACEHOLDER) before its
 * callback.
 */
class SetPageLayoutJob : public ApplauncherLsdbJob {
public:
	paf::vector<int> m_fromPages;
	paf::vector<int> m_fromPositions;
	paf::vector<int> m_toPages;
	paf::vector<int> m_toPositions;
	paf::vector<ApplauncherRemovedIcon> m_removed;
	ApplauncherLsdbJobCallback m_onRun;
};
paf::common::SharedPtr<paf::job::JobItem> applauncherSetPageLayoutJobStart(
	paf::job::JobQueue *queue, const paf::vector<int> &fromPages,
	const paf::vector<int> &fromPositions, const paf::vector<int> &toPages,
	const paf::vector<int> &toPositions, const paf::vector<ApplauncherRemovedIcon> &removed,
	ApplauncherLsdbJobCallback onRun, ApplauncherLsdbJobCallback callback, void *arg);
/* the pages' parameters saved (Sony's job name, SetPageThemeJob; 0x811C5278 puts it on the
   queue) */
paf::common::SharedPtr<paf::job::JobItem> applauncherSetPageThemeJobStart(
	paf::job::JobQueue *queue, const paf::vector<int> &pages, const LsdbPageList &params,
	ApplauncherLsdbJobCallback callback, void *arg);

/* the database jobs that put an application's row in (InsertAppInfoJob, 0x811C6110), rewrite
   it (UpdateAppInfoJob, 0x811C6340), rewrite what changed of it (UpdateAppInfoDiffJob,
   0x811C6568), or put it in on a new page (CreateNewPageAndInsertAppJob, 0x811C67E2), put on
   the queue */
paf::common::SharedPtr<paf::job::JobItem> applauncherInsertAppInfoJobStart(
	paf::job::JobQueue *queue, const LsdbAppInfoPtr &app,
	const paf::vector<LsdbAppInfoPtr> &icons, int page, const LsdbIconPtr &icon,
	ApplauncherLsdbJobCallback callback, void *arg);
paf::common::SharedPtr<paf::job::JobItem> applauncherUpdateAppInfoJobStart(
	paf::job::JobQueue *queue, const LsdbAppInfoPtr &app,
	const paf::vector<LsdbAppInfoPtr> &icons, const LsdbIconPtr &icon,
	ApplauncherLsdbJobCallback callback, void *arg);
paf::common::SharedPtr<paf::job::JobItem> applauncherUpdateAppInfoDiffJobStart(
	paf::job::JobQueue *queue, const LsdbAppInfoPtr &app,
	const paf::vector<LsdbAppInfoPtr> &icons, const LsdbIconPtr &icon, int diff,
	ApplauncherLsdbJobCallback callback, void *arg);
paf::common::SharedPtr<paf::job::JobItem> applauncherCreateNewPageAndInsertAppJobStart(
	paf::job::JobQueue *queue, int page, const LsdbPage &param, const LsdbAppInfoPtr &app,
	const paf::vector<LsdbAppInfoPtr> &icons, const LsdbIconPtr &icon,
	ApplauncherLsdbJobCallback callback, void *arg);

/* PLACEHOLDER: what the interface's entry #0 is given: an application to put on the home or to
   rewrite there */
struct ApplauncherInstallRequest {
	LsdbIconPtr icon;			/* its icon's row */
	LsdbAppInfoPtr app;			/* its parameters */
	paf::vector<LsdbAppInfoPtr> icons;	/* PLACEHOLDER: the rows written with it */
	int diff;				/* PLACEHOLDER: what changed, for mode 1 */
	bool focus;				/* its page shown and its icon given the focus */
	int mode;				/* PLACEHOLDER: 0 put in, 1 what changed rewritten,
						   2 rewritten */
};

/* PLACEHOLDER: livespace_db's exports, not declared there yet: an application's _eboot_attr,
   LAREA_TYPE and #_promote_attr */
extern "C" SceInt64 SceLsdb_45B38A03(const LsdbAppInfoPtr *app);
extern "C" SceInt64 sceLsdbGetMetaContentsPath(const LsdbAppInfoPtr *app);
extern "C" SceInt64 SceLsdb_0B16523B(const LsdbAppInfoPtr *app);


/* PLACEHOLDER: the pages read again (0x810B31DE, applauncher_pages.cpp) */
void applauncherPagesReload(void);
/* PLACEHOLDER: the wallpapers no page uses deleted (0x810B3482), and a folder's removed icons
   told to the database, its icon updated then (0x810B358C) */
void applauncherPagesCleanTabpaper(void);
void applauncherPagesReloadFolderIconCapture(const paf::vector<ApplauncherRemovedIcon> &icons,
					     SceUInt64 appId);
/* PLACEHOLDER: what the pages' database jobs call when done: the pages' icons (0x810B3606),
   parameters (0x810B37F8), the icons counted (0x810B38E8), the pages counted (0x810B38F2),
   the first pages' icons again (0x810B3A96), a title's icon row (0x810B3AAE) */
void applauncherPagesOnPageLayout(ApplauncherLsdbJob *job, void *arg);
void applauncherPagesOnPageTheme(ApplauncherLsdbJob *job, void *arg);
void applauncherPagesOnIconNum(ApplauncherLsdbJob *job, void *arg);
void applauncherPagesOnTotalPage(ApplauncherLsdbJob *job, void *arg);
void applauncherPagesOnPageIcons(ApplauncherLsdbJob *job, void *arg);
void applauncherPagesOnIconInfo(ApplauncherLsdbJob *job, void *arg);
/* PLACEHOLDER: the launch effect made and taken down (0x810CDDCA, 0x810CDE64), begun for the
   icon (0x810CE474), and whether it is not rising (0x810CF2BC) */
void applauncherLaunchEffectCreate(void);
void applauncherLaunchEffectDestroy(void);
void applauncherLaunchEffectBegin(const paf::common::SharedPtr<ApplauncherIcon> &icon);
bool applauncherLaunchEffectIsNotRising(void);

/* topmenu_plugin's functions the home calls (indexes in its PLACEHOLDER table) */
enum {
	TOPMENU_PLUGIN_FUNC_ADD_LAYER		= 0,	/* PLACEHOLDER names */
	TOPMENU_PLUGIN_FUNC_SET_APP_IMAGE	= 2,
	TOPMENU_PLUGIN_FUNC_GET_LAYER_ROOT	= 4,
	TOPMENU_PLUGIN_FUNC_GET_ICON_FRAME	= 8,	/* PLACEHOLDER name: an icon image's frame (a
							   const intrusive_ptr<graph::Surface> *) */
	TOPMENU_PLUGIN_FUNC_SET_LAUNCH_TITLE	= 12,
	TOPMENU_PLUGIN_FUNC_GET_JOB_QUEUE	= 13,
	TOPMENU_PLUGIN_FUNC_LAUNCH_BEGIN	= 20,	/* PLACEHOLDER names: the launch effect's */
	TOPMENU_PLUGIN_FUNC_LAUNCH_END	= 22,	/* start, end, and its icon launched */
	TOPMENU_PLUGIN_FUNC_LAUNCH_FIRE	= 23,
	TOPMENU_PLUGIN_FUNC_SET_LAYER_ENTER	= 25,
	TOPMENU_PLUGIN_FUNC_SET_LAYER_LEAVE	= 26,
	TOPMENU_PLUGIN_FUNC_SHOW_HOME		= 27,
	TOPMENU_PLUGIN_FUNC_EDIT_MODE_ENTERED	= 29,
	TOPMENU_PLUGIN_FUNC_EDIT_MODE_LEFT	= 30,
	TOPMENU_PLUGIN_FUNC_HOME_STARTED	= 32,
	TOPMENU_PLUGIN_FUNC_SET_SCROLL_VIEW	= 38,
	TOPMENU_PLUGIN_FUNC_IS_LAYER_ACTIVE	= 41,
	TOPMENU_PLUGIN_FUNC_IS_WOBBLE_ON	= 49,	/* PLACEHOLDER name */
	TOPMENU_PLUGIN_FUNC_HOME_READY	= 50,
	TOPMENU_PLUGIN_FUNC_MODE_ENTER		= 52,	/* PLACEHOLDER names: an applauncher mode */
	TOPMENU_PLUGIN_FUNC_MODE_LEAVE		= 53,	/* given to topmenu, and taken back */
	TOPMENU_PLUGIN_FUNC_IS_WAVE_SHOWN	= 57,
	TOPMENU_PLUGIN_FUNC_APP_DELETING	= 62,	/* PLACEHOLDER name: a title about to be
							   deleted, its application closed */
	TOPMENU_PLUGIN_FUNC_LIVEAREA_OPENED	= 85,
};

/* livearea_plugin's functions the home calls (indexes in its PLACEHOLDER table) */
enum {
	LIVEAREA_PLUGIN_FUNC_OPEN		= 19,	/* PLACEHOLDER names */
	LIVEAREA_PLUGIN_FUNC_APP_ADDED		= 20,
};

/* PLACEHOLDER: livearea_plugin (or the game plugin) did not come (0x80103301), a job could not be made
   (0x80103302), and without a title in focus or the theme's titles (0x80103303) */
#define APPLAUNCHER_ERROR_NO_LIVEAREA		((int)0x80103301)
#define APPLAUNCHER_ERROR_NO_MEMORY		((int)0x80103302)
#define APPLAUNCHER_ERROR_NO_TITLE		((int)0x80103303)
/* PLACEHOLDER: a job canceled (0x80103308), an application's change log missing from a
   directory the game plugin did not mount (0x8010330F), an empty change log (0x80103322) and its
   other errors (0x80103321, 0x80103323) */
#define APPLAUNCHER_ERROR_CANCELED		((int)0x80103308)
#define APPLAUNCHER_ERROR_NO_CHANGE_INFO_DIR	((int)0x8010330F)
#define APPLAUNCHER_ERROR_CHANGE_INFO_READ	((int)0x80103321)
#define APPLAUNCHER_ERROR_NO_CHANGE_INFO	((int)0x80103322)
#define APPLAUNCHER_ERROR_CHANGE_INFO_PARSE	((int)0x80103323)
/* PLACEHOLDER name: a file not there, as paf's LocalFile gives it (value only) */
#define APPLAUNCHER_ERROR_NOT_FOUND		((int)0x80AF5022)

/* the save data information AppMgr gives (28 bytes) */
struct ApplauncherSaveDataInfo {
	SceUInt32 headPad;
	SceUInt32 inUse;		/* the save data is in use */
	SceUInt32 midPad[2];
	SceUInt32 attr;			/* bit 0 picks the information's text */
	SceUInt32 tailPad[2];
};

#endif
