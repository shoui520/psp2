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
#include <std/memory>
#include <math/ngp/v2.h>
#include <versatileparam/idparam.h>
#include <common/shared_ptr.h>
#include <graphics/surface/surface.h>
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

class ApplauncherPage;

/* an icon's data is livespace_db's LsdbIcon (its folder's LsdbFolder), as the data cache gives
   it */

/* PLACEHOLDER: an icon of a page (its class at 0x810D0xxx, not written yet; fields read here
   only) */
class ApplauncherIcon {
public:
	/* PLACEHOLDER name: the drag ended, the icon put back and the drag let go of
	   (0x810D0900) */
	void Drop();
	/* PLACEHOLDER names: the child of the title taken out of the folder (0x810D192E), and
	   the folder's look updated (0x810CF7DE) */
	void RemoveChild(const char *titleId);
	/* PLACEHOLDER name: the child of the title put in the folder (0x810D17C0) */
	void AddChild(const char *titleId);
	void Update(bool animate);
	/* PLACEHOLDER names: the icon given its application's new data (0x810D1502), and a
	   folder's children's data taken in (0x810D1C90) */
	void SetData(const LsdbIconPtr &data);
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
	void PutInFolder(const paf::common::SharedPtr<ApplauncherIcon> &folder);

	SceUInt32 reserved0[4];
	paf::ui::AppIconBase **m_icon;		/* PLACEHOLDER: the icon's widget, first of its view */
	ApplauncherPage *m_page;		/* PLACEHOLDER: the page it is on */
	SceUInt8 reserved1[4];
	bool m_selected;			/* PLACEHOLDER: marked selected (Select, Unselect) */
	bool m_hidden;				/* PLACEHOLDER: not shown (left out of the snaps) */
	SceUInt8 reserved2;
	bool m_added;				/* PLACEHOLDER: just put on the home by func0 */
	int m_slot;				/* PLACEHOLDER: its slot on the page */
	ApplauncherPage *m_movePage;		/* PLACEHOLDER: the page its move began on */
	int m_moveSlot;				/* PLACEHOLDER: the slot its move began in */
	int m_homeSlot;				/* PLACEHOLDER: the slot it goes back to when moved aside */
	paf::intrusive_ptr<paf::cache::CacheImageInterface> m_image;	/* PLACEHOLDER: its image */
	LsdbIconPtr m_data;			/* PLACEHOLDER: its data */
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
class ShellSnapListItem {
public:
	/* PLACEHOLDER: the applauncher page's focused icon */
	paf::common::SharedPtr<ApplauncherIcon> m_focus;
	SceUInt32 reserved0[2];
	paf::ui::Widget *m_widget;		/* what the view snaps to */
	SceUInt32 reserved1[7];
	int m_index;				/* its place in the list */
	SceUInt32 reserved2[3];
	int m_key;				/* the key of its listeners */
};

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

	SceUInt32 m_listeners[3];		/* PLACEHOLDER: the listeners' map, its type not read */
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
 * PLACEHOLDER: a page of the home (its code in the file after this one, 0x810B297C on): its
 * icons' slots, its state and its place in the scroller. The virtual functions the home calls
 * are named; the others are placeholders.
 */
/* PLACEHOLDER: a page's widgets, taken from the page pool (0x810C4A3A) and given back to it
   (0x810C4E74), the pool's file */
/* PLACEHOLDER: a page's widgets from the page pool (its class at 0x810C4xxx, not written yet;
   fields read here only) */
class ApplauncherPageView {
public:
	/* PLACEHOLDER names: the view's empty mark shown or hidden (0x810C5156), and its widgets
	   brought up to the home's page count (0x810C5A0E) */
	void SetEmpty(bool empty);
	void Refresh();
	/* PLACEHOLDER names: the edit begun on the view, empty or not (0x810C5688), and ended
	   (0x810C51E2); the slot's frame faded in (0x810C5C08) and out (0x810C5D0C) */
	void BeginEdit(bool empty);
	void EndEdit();
	void ShowSlotFrame(int slot, bool animate);
	void HideSlotFrame(int slot, bool animate);

	SceUInt32 reserved[2];
	paf::ui::Widget *m_root;		/* PLACEHOLDER: the widget its icons go on */
};
void applauncherPageViewRelease(ApplauncherPageView *view);
class ApplauncherPage;
ApplauncherPageView *applauncherPageViewGet(ApplauncherPage *page, bool fast);

/* the slots of a page */
#define APPLAUNCHER_PAGE_SLOTS		10

class ApplauncherPageBase {
public:
	/* the page of the type, its slots empty (0x810B8516) */
	ApplauncherPageBase(int type);
	virtual ~ApplauncherPageBase();
	/* PLACEHOLDER name: the page's view shown empty, or not (0x810B7DD4) */
	virtual void SetEmpty(bool empty);
	virtual void Refresh(void);		/* PLACEHOLDER name */
	virtual void reserved4(void);
	virtual void reserved5(void);
	virtual void reserved6(void);
	virtual int GetIndex(void);		/* PLACEHOLDER name */
	virtual paf::ui::Widget *GetWidget(void);
	virtual bool IsOpened(void);		/* PLACEHOLDER name: the application page's opening done */
	/* PLACEHOLDER name: where the slot's icon stands on the page */
	virtual paf::math::v2 GetSlotPos(int slot);
	virtual void UpdateFade(void);		/* PLACEHOLDER name: the wave's fade applied each frame */

	int m_type;				/* 1: the last, empty page */
	paf::common::SharedPtr<ApplauncherIcon> m_slots[APPLAUNCHER_PAGE_SLOTS];
	int m_state;				/* 1: shown in place */
	int m_moveSlot;				/* PLACEHOLDER: the slot an icon's move began in */
	ApplauncherPageView *m_view;		/* PLACEHOLDER: its widgets, from the page pool */
	bool m_changed;			/* PLACEHOLDER: an icon taken off it */
	bool m_iconsLoaded;			/* PLACEHOLDER: its icons' widgets taken */
	SceUInt8 reserved7[2];

	/* its icons' widgets and its own given back to the pool (0x810B3BBA) */
	void Unload();
	/* PLACEHOLDER name: the widget its icons go on, NULL without its view (0x810B46F6) */
	paf::ui::Widget *GetRoot();
};

class ApplauncherPage : public ApplauncherPageBase, public ShellSnapListItem {
public:
	/* the page made on the plugin, from the parameters (0x810D3BB8) */
	ApplauncherPage(paf::Plugin *plugin, const LsdbPage *param, int type);
	/* PLACEHOLDER name: the edit begun on the page: its view refreshed below ten pages, its
	   icons and view put in the edit, its slots' frames set (0x810B86B4) */
	void BeginEdit();
	/* PLACEHOLDER name: the slots' frames shown where no folder stands in place, hidden
	   under the others (0x810B85C6) */
	void UpdateSlotFrames(bool animate);

	LsdbPage m_param;		/* what it was made from */
	paf::ui::Widget *m_noticeText;		/* PLACEHOLDER: the notice page's text */
	SceUInt32 reserved3[27];		/* PLACEHOLDER: the rest of the page (320 bytes) */

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

/* the home's page indicator: the dots under the pages (0x810C24D8; names ours) */
class ApplauncherPageIndicator {
public:
	ApplauncherPageIndicator(paf::Plugin *plugin, paf::ui::Widget *widget);
	~ApplauncherPageIndicator();
	void Update();
	/* PLACEHOLDER name: the dots shown after the delay (0x810C2C2C) */
	void Show(float delay);
	/* PLACEHOLDER name: the dots hidden after the delay (0x810C2CD6) */
	void Hide(float delay);

	SceUInt32 reserved[5];
};

/*
 * PLACEHOLDER: the page a folder's icon opens over the home, its icons read from the folder's
 * page in the database (160 bytes, vtable 0x8152F2D4). The home keeps it as its ApplauncherPage
 * (m_appPage), whose m_focus is this page's m_icon: in Sony's build both are made on
 * ApplauncherPageBase, and the page methods it calls (AddIcon, UpdateFocusLinks, Load,
 * BeginEdit) read only that base.
 */
class ApplauncherFolderPage : public ApplauncherPageBase {
public:
	/* the page opened from the icon, the icon of the title given the focus (0x810BA824) */
	ApplauncherFolderPage(const paf::common::SharedPtr<ApplauncherIcon> &icon, const char *name,
			      bool select);
	virtual ~ApplauncherFolderPage();
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

/* PLACEHOLDER: where the slot's icon stands on a folder page, without one (0x810BB24A) */
paf::math::v2 applauncherFolderPageGetSlotPos(int slot);
/* PLACEHOLDER: the timer's widget destroyed (its DestroyWidget), in a file not written yet
   (0x810D40BC) */
void applauncherDestroyWidgetCB(SceInt32 type, paf::ui::Handler *self, paf::ui::Event *e,
				void *data);

/* the listener of the home's touches (its methods out of line, 0x810C9B58) */
class ApplauncherInputListener : public paf::inputdevice::InputListener {
public:
	ApplauncherInputListener();
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
	bool m_flags0[3];
	int m_waitDialog;			/* the waiting dialog's ID, -1 without one */
	bool m_imageCached;			/* PLACEHOLDER: the background image taken from the surface pool */
	paf::math::v2 m_dragPos;
	int m_waitingAppCount;			/* PLACEHOLDER: the applications with no room yet */
	ApplauncherPage *m_noticePage;		/* PLACEHOLDER: the page saying why no more fit */
	int m_appCount;				/* PLACEHOLDER: the applications on the home */
	int m_noticeKind;			/* PLACEHOLDER: which text the full home shows */
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
/* PLACEHOLDER: the icon the dragged one is over (0x81555264, the page's file) */
extern paf::common::SharedPtr<ApplauncherIcon> g_applauncherDropTarget;
/* PLACEHOLDER: the icons' move time, 275 ms (0x81507820, the page's file, beside 375 and 325) */
extern const float g_applauncherIconMoveTime;
/* PLACEHOLDER: the icons' tilt effect moved on a frame (0x810CF2AA), and the drag left
   when the dragged widget is in its drag mode (0x810D1DE6) */
void applauncherIconEffectUpdate(void);
void applauncherIconCancelDrag(void);
/* PLACEHOLDER: the pages' first load done (0x810B3502), and the home's start marked done
   once (0x810B3B36), both in the page's file */
bool applauncherPagesLoaded(void);
void applauncherPagesSetStarted(void);
/* PLACEHOLDER: the applauncher's request kept, outside the IDU mode and without one
   (0x810B8830, applauncher_page.cpp) */
void applauncherPagesHoldRequest(void);
/* PLACEHOLDER: the IconCacheManager, its manager first (0x815553B0, the icon cache's file) */
struct ApplauncherIconCache {
	paf::cache::CacheManager *m_manager;
};
extern ApplauncherIconCache *g_applauncherIconCache;
/* PLACEHOLDER: the dragged icon's widget put back in its icon mode and the home told whether a
   folder or an icon was let go (0x810BCB44) */
void applauncherIconWidgetLeaveDrag(paf::ui::AppIconBase **icon);
/* PLACEHOLDER: an icon made for the data, on the plugin (0x810CFD82, the icon's file) */
paf::common::SharedPtr<ApplauncherIcon> applauncherIconCreate(paf::Plugin *plugin,
							 const LsdbIconPtr &data);
/* PLACEHOLDER: an icon's size on a folder's page (0x810BBE8C) and on a page (0x810BBEAE), by
   the model; its label's colour from its page's (0x810BCB94); and its mark set by the sort
   (0x810BBE66) (the icon widget's file) */
float applauncherIconWidgetFolderSize(void);
float applauncherIconWidgetSize(void);
void applauncherIconWidgetUpdateLabel(paf::ui::AppIconBase **icon);
void applauncherIconWidgetUpdateMark(paf::ui::AppIconBase **icon);

/* the home, while the plugin runs (0x81555154) */
extern ApplauncherHome *g_applauncherHome;

/* a page made from the parameters (or the theme's) and put in after the other (0x810A9812) */
ApplauncherPage *applauncherHomeAddPage(const LsdbPage *param, ApplauncherPage *after,
					bool focus, bool save);

/* the page after it, the page itself at the end (0x810ACF7C) */
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
/* the interface's func4: the title's icon given the focus (0x810B071E); func5: its selected
   mark cleared (0x810B0736) */
void applauncherInterfaceFocusApp(const paf::string &titleId, bool select);
void applauncherInterfaceUnselectApp(const paf::string &titleId);
/* PLACEHOLDER names: the interface's func7 and func8, the icon effect of the title ended
   (0x810B07FC), and any (0x810B0804) */
void applauncherInterfaceEndIconEffect(const paf::string &titleId);
void applauncherInterfaceEndIconEffects(void);
/* the interface's func9 and func10: the view scrolled to the notice page (0x810B080C), and to
   the first page (0x810B0844) */
void applauncherInterfaceScrollToNotice(void);
void applauncherInterfaceScrollToFirst(void);
/* the interface's func11: the icon image of the application's folder loaded (0x810B088E) */
void applauncherInterfaceLoadIconImage(const char *dir,
				       paf::intrusive_ptr<paf::cache::CacheImageInterface> *image);
/* PLACEHOLDER: the interface's func3: the image of the title's icon, else the image asked of
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

/* PLACEHOLDER names: the interface's func12, the title's icon marked for an update
   (0x810B0DB2), and func13, its data asked for again, of the pages without an icon
   (0x810B0EA2) */
int applauncherInterfaceMarkAppUpdate(const char *titleId);
int applauncherInterfaceRequestAppData(const char *titleId);
/* PLACEHOLDER: the title's data asked of the data cache, but for Welcome Park (0x810B33D6, the
   page's file) */
void applauncherPagesRequestData(const paf::string &titleId);

/* PLACEHOLDER names: the interface's func14, every page's icon images reloaded (0x810B0FD4);
   func15, the kept request's state cleared (0x810B1026); func17, the wave hidden and the view
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

/* PLACEHOLDER names: the interface's func18, the index of the page in focus, -1 without one
   (0x810B10E2); func19, the application's page closed (0x810B1110); func21, whether the title
   is on the home, as an icon or in a folder (0x810B1126); func22, the title's change log opened
   (0x810B12B4); func23, the theme reloaded and its UI refreshed on the main thread
   (0x810B12C6); func24, the theme reloaded (0x810B12FA) */
int applauncherInterfaceGetFocusedPage(void);
void applauncherInterfaceCloseAppPage(void);
bool applauncherInterfaceHasApp(const paf::string &titleId);
int applauncherInterfaceOpenChangeLog(const paf::string *titleId, void *loadArg,
				      ApplauncherChangeLogCallback callback, int value, void *arg);
int applauncherInterfaceRefreshTheme(void);
int applauncherInterfaceReloadTheme(void);
/* PLACEHOLDER name: the interface's func26, the kept request marked (0x810B17D8) */
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

/* PLACEHOLDER names: a title's change log opened over another application, the dialog's ID
   given back (0x810B965A); the icon's opened over the home (0x810B95CA); the open one closed
   (0x810B9882) */
int applauncherChangeLogOpen(const paf::string *titleId, void *loadArg,
			     ApplauncherChangeLogCallback callback, int value, void *arg);
class ApplauncherChangeLog *applauncherChangeLogOpenIcon(
	const paf::common::SharedPtr<ApplauncherIcon> &icon);
void applauncherChangeLogClose(void);

/*
 * PLACEHOLDER: an application's change information (its sce_sys/changeinfo XML), read in a job
 * and given as the RichText's marked-up text; its file (0x810C9E26-) is not converted yet. The
 * constructor is the vector's.
 */
class ApplauncherChangeInfo {
public:
	/* out of line in Sony's build (folded at 0x814097DC) */
	__attribute__((noinline)) ApplauncherChangeInfo()
	{
	}

	~ApplauncherChangeInfo();				/* 0x810C9E26 */
	/* the XML of the language (else the default one) in the directory read: 0, or a negative
	   error (0x810C9F82) */
	int Load(paf::job::JobItem *job, const char *dir, int language, void *arg);
	/* each version's heading and text, one after the other (0x810CA930) */
	void GetText(paf::string *text);

	paf::vector<void *> m_entries;
};

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

/* PLACEHOLDER: the interface's func2: livearea_plugin waited for and its LiveArea opened, the
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
			     const void *option, int priority, bool reload);
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
void applauncherIconDelete(const paf::common::SharedPtr<ApplauncherIcon> &icon, bool animate,
			   bool save);
/* PLACEHOLDER: the folders' icons drawn again from their children and saved (0x810B7FE0) */
void applauncherPagesRemoveIcons(paf::vector<ApplauncherRemovedIcon> *icons);
/* PLACEHOLDER: livespace_db's export, not declared there yet: the title's application taken
   off, whole with its folder or not */
extern "C" int SceLsdb_DE0DA538(AppInfoSClient *client, const paf::string *titleId,
	LsdbIconPtr *icon, int *pageNo, bool theme);
/* PLACEHOLDER: livespace_db's exports, not declared there yet: the pages counted, and the pages'
   rows read from the page given */
extern "C" int SceLsdb_94128795(AppInfoSClient *client);
extern "C" int SceLsdb_8FAD093B(AppInfoSClient *client, int pageNo, unsigned int flags,
	LsdbPageList *pages);
/* PLACEHOLDER: livespace_db's exports, not declared there yet: the icons of the pages (or of
   a folder, by its id) from the page given, and an icon's fields written, its image given */
extern "C" int SceLsdb_D74AC354(AppInfoSClient *client, int pageNo, unsigned int count,
	LsdbPageIconLists *pages);
extern "C" int SceLsdb_C1A82AC8(AppInfoSClient *client, int pageNo, const LsdbIconPtr *icon,
	unsigned int fields, const void *image, int imageSize);
extern "C" int SceLsdb_B33FE18E(AppInfoSClient *client, const paf::string &titleId, bool whole, int flags);

/* PLACEHOLDER: the 42 icons of the pool dimmed or not (0x810BCC60, the icon pool's file) */
void applauncherIconPoolSetDim(bool dim);

/* a DeletePageJob for the page put on the queue (0x811C5986) */
paf::common::SharedPtr<paf::job::JobItem> applauncherDeletePageJobStart(paf::job::JobQueue *queue,
									int page, void *arg1,
									void *arg2);

/* a CreateNewPageJob for the page, its icons and parameters, put on the queue (0x811C5848) */
paf::common::SharedPtr<paf::job::JobItem> applauncherCreateNewPageJobStart(
	paf::job::JobQueue *queue, int page, const paf::vector<LsdbAppInfoPtr> &icons,
	const LsdbPage &param, void *arg1, void *arg2);

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

/* PLACEHOLDER: what the interface's func0 is given: an application to put on the home or to
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

/* PLACEHOLDER: the applauncher's state another file keeps (0x81555408) */
struct ApplauncherSettings {
	SceUInt32 reserved;
	int m_sortEnabled;
};
struct ApplauncherState {
	SceUInt32 reserved[3];
	ApplauncherSettings *m_settings;
	SceUInt32 reserved1[3];
	int m_requestState;			/* PLACEHOLDER: cleared while a request is kept */
	int m_requestId;			/* PLACEHOLDER: -1 without a request */
};
extern ApplauncherState *g_applauncherState;

/* PLACEHOLDER: an icon's type for the power button (a PlayStation TV's) */
#define APPLAUNCHER_ICON_TYPE_POWER	6

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
/* PLACEHOLDER: the home's drag object made and taken down (0x810CDDCA, 0x810CDE64) */
void applauncherDragCreate(void);
void applauncherDragDestroy(void);

/* topmenu_plugin's functions the home calls (indexes in its PLACEHOLDER table) */
enum {
	TOPMENU_PLUGIN_FUNC_ADD_LAYER		= 0,	/* PLACEHOLDER names */
	TOPMENU_PLUGIN_FUNC_SET_APP_IMAGE	= 2,
	TOPMENU_PLUGIN_FUNC_GET_LAYER_ROOT	= 4,
	TOPMENU_PLUGIN_FUNC_GET_JOB_QUEUE	= 13,
	TOPMENU_PLUGIN_FUNC_SET_LAYER_ENTER	= 25,
	TOPMENU_PLUGIN_FUNC_SET_LAYER_LEAVE	= 26,
	TOPMENU_PLUGIN_FUNC_SHOW_HOME		= 27,
	TOPMENU_PLUGIN_FUNC_EDIT_MODE_ENTERED	= 29,
	TOPMENU_PLUGIN_FUNC_EDIT_MODE_LEFT	= 30,
	TOPMENU_PLUGIN_FUNC_HOME_STARTED	= 32,
	TOPMENU_PLUGIN_FUNC_SET_SCROLL_VIEW	= 38,
	TOPMENU_PLUGIN_FUNC_IS_LAYER_ACTIVE	= 41,
	TOPMENU_PLUGIN_FUNC_HOME_READY	= 50,
	TOPMENU_PLUGIN_FUNC_IS_WAVE_SHOWN	= 57,
	TOPMENU_PLUGIN_FUNC_LIVEAREA_OPENED	= 85,
};

/* livearea_plugin's functions the home calls (indexes in its PLACEHOLDER table) */
enum {
	LIVEAREA_PLUGIN_FUNC_OPEN		= 19,	/* PLACEHOLDER names */
	LIVEAREA_PLUGIN_FUNC_APP_ADDED		= 20,
};

/* PLACEHOLDER: livearea_plugin did not come (0x80103301), a job could not be made
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

#endif
