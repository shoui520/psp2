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
#include <widget/w_scroll_view_base.h>
#include <inputdevice/inputlistener.h>
#include <job/job.h>
#include <cache/cache_image.h>
#include "../../../plugins.h"
#include "../../livespace_db/livespace_db_internal.h"

class ApplauncherPage;

/* PLACEHOLDER: an application's state that its icon shows (its class not read) */
struct ApplauncherAppState {
	SceUInt32 reserved[4];
	int m_state;				/* 1: being deleted with its whole folder */
	int m_id;				/* what the page's saved order keeps of it */
};

/* PLACEHOLDER: an icon's data (the data cache's entry, its class not read) */
struct ApplauncherAppData {
	SceUInt64 m_appId;			/* PLACEHOLDER: what the pages' icons are found by */
	SceUInt32 reserved0[4];
	paf::string m_iconPath;			/* PLACEHOLDER: the icon's image */
	SceUInt32 reserved2;
	paf::string m_titleId;
	SceUInt32 reserved1[6];
	ApplauncherAppState *m_state;
};

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
	void Update(bool animate);
	/* PLACEHOLDER names: the icon given its application's new data (0x810D1502), and a
	   folder's children's data taken in (0x810D1C90) */
	void SetData(const paf::common::SharedPtr<ApplauncherAppData> &data);
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
	/* the icon's widget, NULL without one (0x810D0006) */
	paf::ui::Widget *GetWidget();
	/* PLACEHOLDER name: the icon's widget faded in (0x810D1D38) */
	void Show();

	SceUInt32 reserved0[4];
	paf::ui::Widget **m_icon;		/* PLACEHOLDER: the icon's widget */
	ApplauncherPage *m_page;		/* PLACEHOLDER: the page it is on */
	SceUInt8 reserved1[5];
	bool m_hidden;				/* PLACEHOLDER: not shown (left out of the snaps) */
	SceUInt8 reserved2[2];
	int m_slot;				/* PLACEHOLDER: its slot on the page */
	SceUInt32 reserved3[3];
	paf::intrusive_ptr<paf::cache::CacheImageInterface> m_image;	/* PLACEHOLDER: its image */
	ApplauncherAppData *m_data;		/* PLACEHOLDER: its data */
};

typedef paf::common::SharedPtr<ApplauncherAppData> ApplauncherAppDataPtr;
/* PLACEHOLDER: the data of a page's icons, as the data cache gives them */
typedef paf::vector<ApplauncherAppDataPtr> ApplauncherAppDataList;

/* PLACEHOLDER: an icon taken off a page, as the page's saved order is told of it */
struct ApplauncherRemovedIcon {
	int m_id;
	int m_pageIndex;
	int m_slot;
};
PAF_VECTOR_INLINE_DTOR(ApplauncherRemovedIcon)

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

/* what a page is made from: its background, wave and the sizes of its image */
struct ApplauncherPageParam {
	paf::string m_imagePath;		/* PLACEHOLDER: the background shown, its image */
	int m_waveType;				/* PLACEHOLDER: and its wave */
	int m_imageWidth;
	int m_imageHeight;
	int m_thumbnailWidth;
	int m_thumbnailHeight;
	SceUInt32 m_fontColor;

	ApplauncherPageParam() : m_imageWidth(0), m_imageHeight(0), m_thumbnailWidth(0),
		m_thumbnailHeight(0), m_fontColor(0x1FFFFFF)
	{
	}
};

/*
 * PLACEHOLDER: a page of the home (its code in the file after this one, 0x810B297C on): its
 * icons' slots, its state and its place in the scroller. The virtual functions the home calls
 * are named; the others are placeholders.
 */
class ApplauncherPageBase {
public:
	virtual ~ApplauncherPageBase();
	virtual void reserved2(void);
	virtual void Refresh(void);		/* PLACEHOLDER name */
	virtual void reserved4(void);
	virtual void reserved5(void);
	virtual void reserved6(void);
	virtual int GetIndex(void);		/* PLACEHOLDER name */
	virtual paf::ui::Widget *GetWidget(void);

	int m_type;				/* 1: the last, empty page */
	paf::common::SharedPtr<ApplauncherIcon> m_slots[10];
	int m_state;				/* 1: shown in place */
	SceUInt32 reserved0[3];
};

class ApplauncherPage : public ApplauncherPageBase, public ShellSnapListItem {
public:
	/* the page made on the plugin, from the parameters (0x810D3BB8) */
	ApplauncherPage(paf::Plugin *plugin, const ApplauncherPageParam *param, int type);
	/* the page saved to the database (0x810B86B4) */
	void Save();

	ApplauncherPageParam m_param;		/* what it was made from */
	paf::ui::Widget *m_noticeText;		/* PLACEHOLDER: the notice page's text */
	SceUInt32 reserved3[27];		/* PLACEHOLDER: the rest of the page (320 bytes) */

	/* an icon in every slot (0x810B4B0C) */
	bool IsFull();
	/* PLACEHOLDER name: the page out of the edit, its state and its slots' marks cleared
	   (0x810B871E) */
	void EndEdit();
	/* no icon in any slot (0x810B43F0) */
	bool IsEmpty();
	/* its icons let go of (0x810B3BBA), or loaded (0x810B8752) */
	void Unload();
	void Load(bool fast);
	/* PLACEHOLDER names: the page's parameters copied out (0x810D2716), and set
	   (0x810D274C) */
	void GetParam(ApplauncherPageParam *param);
	void SetParam(const ApplauncherPageParam *param, bool save);
	/* PLACEHOLDER name: the icons put back in their slots, the focused one from its drag
	   (0x810D364E) */
	void RestoreIcons(const paf::common::SharedPtr<ApplauncherIcon> &focus);
	/* PLACEHOLDER: 1 when the application's page goes with its focus (0x810B5506) */
	int IsAppRemoved();
	/* PLACEHOLDER name: the icons given their new data (0x810D2B40) */
	void UpdateIcons(ApplauncherAppDataList *list);
	/* PLACEHOLDER names: the icon of the title in a slot (0x810B7DEE), or in a slot or a
	   folder of one, the folder given back (0x810D3A64) */
	paf::common::SharedPtr<ApplauncherIcon> FindIcon(const paf::string &titleId);
	paf::common::SharedPtr<ApplauncherIcon> FindIcon(const paf::string &titleId,
		paf::common::SharedPtr<ApplauncherIcon> *folder);
	/* PLACEHOLDER names: the icon of the application in a slot (0x810D3996), and the icons
	   moved aside from the one opened (0x810D319A) */
	paf::common::SharedPtr<ApplauncherIcon> FindIcon(SceUInt64 appId);
	void MoveIconsAside(const paf::common::SharedPtr<ApplauncherIcon> &icon);
	/* PLACEHOLDER name: the selected marks of its icons cleared (0x810B7EF2) */
	void UnselectAll();
	/* PLACEHOLDER names: each icon's effect resumed (0x810B7F7A), and each icon's image
	   reloaded (0x810D2C3E) */
	void ResumeIcons();
	void ReloadIcons();
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

/* the listener of the home's touches (its methods out of line, 0x810C9B58) */
class ApplauncherInputListener : public paf::inputdevice::InputListener {
public:
	ApplauncherInputListener();
};

/* PLACEHOLDER: the home's task, ended when the home goes (its class not read) */
class ApplauncherHomeTask {
public:
	virtual void reserved0(void);
	virtual void reserved1(void);
	virtual void reserved2(void);
	virtual void reserved3(void);
	virtual void reserved4(void);
	virtual void reserved5(void);
	virtual void reserved6(void);
	virtual void Finish(void);
};

class ApplauncherHomeTaskRef {
public:
	ApplauncherHomeTaskRef() : m_task(NULL)
	{
	}

	~ApplauncherHomeTaskRef()
	{
		if (m_task != NULL)
			m_task->Finish();
	}

	ApplauncherHomeTask *m_task;
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
		const paf::vector<paf::common::SharedPtr<ApplauncherAppDataList> > &pages);
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
	ApplauncherHomeTaskRef m_task;
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
	float m_folderScale[2];
	int m_reserved7;
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
	int m_reserved13;
	LsdbEventListener *m_lsdbListener;
};

/* the icon being dragged, while one is (0x81555254) */
extern paf::common::SharedPtr<ApplauncherIcon> g_applauncherDragItem;

/* the home, while the plugin runs (0x81555154) */
extern ApplauncherHome *g_applauncherHome;

/* a page made from the parameters (or the theme's) and put in after the other (0x810A9812) */
ApplauncherPage *applauncherHomeAddPage(const ApplauncherPageParam *param, ApplauncherPage *after,
					bool focus, bool save);

/* the page after it, the page itself at the end (0x810ACF7C) */
ApplauncherPage *applauncherHomeGetNextPage(ApplauncherPage *page);
/* the parameters of the page in view copied out (0x810ACFAA), and set (0x810AD192) */
void applauncherHomeGetPageParam(ApplauncherPageParam *param);
void applauncherHomeSetPageParam(const ApplauncherPageParam *param);
/* PLACEHOLDER: the background's change timed out: the page in view's background shown
   (0x810B1964) */
void applauncherHomeOnBackgroundTimeout(SceInt32 type, paf::ui::Handler *self, paf::ui::Event *e,
					void *data);
/* the snaps of the pages set: in idu mode only pages with a shown icon, else all (0x810AD1D2) */
void applauncherHomeUpdateSnaps(void);
/* the application's page closed: the dots shown, its icons put back, the view shown, its focus
   kept when asked, and the overlay faded out outside the edit (0x810AD2CC) */
void applauncherHomeCloseAppPage(bool keepFocus);
/* the view moved to the page at once, unless it is the application's page (0x810AD466) */
void applauncherHomeJumpTo(ApplauncherPage *page);
/* PLACEHOLDER: the application page's icons put back (0x810BAEFC, the icon pool's file) */
void applauncherIconPoolCloseAppPage(ApplauncherPage *page);
/* PLACEHOLDER: the image of the path loaded by the DataCacheManager, its task kept
   (0x810BA5A2, the data cache's file) */
int applauncherDataCacheLoad(const paf::string &path, ApplauncherHomeTaskRef *task,
			     const void *option, int priority, bool reload);
/* the background of the page in view loaded again (0x810B0CF6) */
void applauncherHomeReloadBackground(void);
/* the applications with no room yet set, the notice shown for them (0x810AFA08) */
void applauncherHomeSetWaitingAppCount(int count);
/* the title's icon given its new data, else its image loaded and given to topmenu
   (0x810AFA24) */
void applauncherHomeUpdateApp(const paf::string &titleId, const ApplauncherAppDataPtr &data);
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

/* PLACEHOLDER: the interface's func2: livearea_plugin waited for and its LiveArea opened, the
   icon of the title in focus faded in once its image is ready (0x810AFF96) */
int applauncherInterfaceOpenLiveArea(const LsdbAppInfoPtr *app, bool animate);
/* PLACEHOLDER: the icon's image ready or given up: the opening let go on (0x810B28C2) */
void applauncherHomeOnIconImage(SceInt32 type, paf::ui::Handler *self, paf::ui::Event *e,
				void *data);
/* livearea_plugin's GetInterface(1), fetched once (0x810A971E) */
ShellPluginInterface *applauncherHomeGetLivearea(void);
/* PLACEHOLDER: the application's page made from its icon (0x810BAEBE, the icon pool's file) */
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
void applauncherIconRemove(paf::common::SharedPtr<ApplauncherIcon> icon, bool animate);
void applauncherIconDelete(const paf::common::SharedPtr<ApplauncherIcon> &icon, bool animate,
			   bool save);
/* PLACEHOLDER: the pages' saved order told of icons taken off (0x810B7FE0) */
void applauncherPagesRemoveIcons(paf::vector<ApplauncherRemovedIcon> *icons);
/* PLACEHOLDER: livespace_db's export, not declared there yet: the title's application taken
   off, whole with its folder or not */
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
	const ApplauncherPageParam &param, void *arg1, void *arg2);

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

/* PLACEHOLDER: the icons' data reloaded once the pages exist (0x810B31DE, the next file) */
void applauncherPagesReload(void);
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
	TOPMENU_PLUGIN_FUNC_SET_SCROLL_VIEW	= 38,
	TOPMENU_PLUGIN_FUNC_IS_WAVE_SHOWN	= 57,
	TOPMENU_PLUGIN_FUNC_LIVEAREA_OPENED	= 85,
};

/* livearea_plugin's functions the home calls (indexes in its PLACEHOLDER table) */
enum {
	LIVEAREA_PLUGIN_FUNC_OPEN		= 19,	/* PLACEHOLDER name */
};

/* PLACEHOLDER: the interface's func2 when livearea_plugin does not come (0x80103301), and
   without a title in focus (0x80103303) */
#define APPLAUNCHER_ERROR_NO_LIVEAREA		((int)0x80103301)
#define APPLAUNCHER_ERROR_NO_TITLE		((int)0x80103303)

#endif
