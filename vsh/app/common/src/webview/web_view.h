/*
 * paf's web view widget ("webview"): a scroll view showing a page the WebKit process
 * (SceWebKitProcess) draws into tiles, which it reads through the tile backend and the web IPC.
 * It passes the page's events to the app's listener and the app's input to the page. The email
 * app links the same code. The widget, parameter and style names follow paf's pattern for the
 * type and parameter names Sony's strings give ("webview", "web_view_base", "style_web_view");
 * every other name here is ours.
 */
#ifndef WEBVIEW_WEB_VIEW_H
#define WEBVIEW_WEB_VIEW_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <std/list>
#include <std/map>
#include <std/memory>
#include <url/url.h>
#include <math/rect.h>
#include <common/string_util.h>
#include <common/shared_ptr.h>
#include <datetime/datetime.h>
#include <thread/thread.h>
#include <timer/timer.h>
#include <graphics/surface/surface.h>
#include <inputdevice/inputlistener.h>
#include <widget/core/widget.h>
#include <widget/core/uiinputlistener.h>
#include <widget/wutil.h>
#include <widget/w_plane.h>
#include <widget/w_scroll_view_base.h>
#include "context_menu_info.h"
#include "web_ipc.h"

namespace paf {
namespace ui {

/* the page's events, as the WebKit process reports them (the web view's second base) */
class WebViewPageClient {
public:
	virtual ~WebViewPageClient()
	{
	}

	virtual void OnPageCreated(int result) = 0;
	virtual void OnPageClosed(int reason) = 0;
	virtual void OnPageEvent0() = 0;
	virtual void OnPageEvent1() = 0;
	virtual void OnPageEvent2() = 0;
	virtual void OnPageEvent3() = 0;
	virtual void OnPageEvent4() = 0;
};

/* the text input the page asks for (the third base); none does anything by default */
class WebViewInputClient {
public:
	virtual ~WebViewInputClient()
	{
	}

	virtual void OnInput0()
	{
	}
	virtual void OnInput1()
	{
	}
	virtual void OnInput2()
	{
	}
	virtual void OnInput3()
	{
	}
	virtual void OnInput4()
	{
	}
	virtual void OnInput5()
	{
	}
	virtual void OnInput6()
	{
	}
	virtual void OnInput7()
	{
	}
	virtual void OnInput8()
	{
	}
	virtual void OnInput9()
	{
	}
};

/* the device's position, for the page's geolocation (the fourth base) */
class WebViewGeolocationClient {
public:
	virtual ~WebViewGeolocationClient()
	{
	}

	virtual void OnPosition(double latitude, double longitude, double altitude, double accuracy,
		double altitudeAccuracy, double heading, double speed, double timestamp, int flags,
		int valid) = 0;
};

class WebView;
class WebViewPopupMenu;

/* the system IME a page's text field opens (0x8100e298; names ours) */
struct WebViewIme;

/* closes the IME (sceImeVshClose) and frees its text */
int ImeClose(WebViewIme *ime);

/* the render parameters the page is made with (80 bytes at 0x81026548; names ours) */
struct WebViewRenderParam {
	uint32_t m_data[20];
};

/* the render parameters every page is made with */
WebViewRenderParam *GetRenderParam();

/* the view's listener, told of its page's events (the class name and the method names are
   ours, after the view's methods that call each; slots not yet seen called are numbered) */
class WebViewPageListener {
public:
	virtual ~WebViewPageListener();
	virtual void OnSlot2();
	virtual void OnLoadFinished(WebView *view, int state);
	virtual void OnLoadFailed(WebView *view, int error, int arg);
	virtual void OnLoadCommitted();
	virtual void OnSlot6();
	virtual void OnLoadProgress();
	virtual void OnTitleChanged(WebView *view, int arg);
	virtual void OnUrlChanged(WebView *view, const char *url);
	virtual void OnEvent8(WebView *view, int arg);
	virtual void OnSlot11();
	virtual void OnSlot12();
	virtual void OnSlot13();
	virtual void OnSlot14();
	virtual void OnEvent10(WebView *view, int arg, int arg1, int arg2, int *out0,
		bool *out1);
	virtual void OnSlot16();
	virtual void OnEvent11(WebView *view, int arg0, int arg1);
	virtual void OnEvent12(WebView *view, int arg);
	virtual void OnEvent13(WebView *view, int arg0, int arg1, int arg2);
	virtual void OnEvent14(WebView *view, int arg0, int arg1, int arg2, int arg3);
	virtual void OnSlot21();
	virtual void OnEvent15(WebView *view, int arg);
	virtual void OnEvent16();
	virtual void OnHistoryChanged();
	virtual void OnEvent18(WebView *view, int arg0, int arg1);
	virtual void OnImeClosed(WebView *view, int arg0, int arg1);
};

/* the application's web view listener, which starts the WebKit process for the views (Sony's
   name, from its messages; wutil::GetWebViewListener) */
class WebViewListener {
public:
	virtual ~WebViewListener();
	/* the view starting ("[WebViewListener] Initialize (by WebView: 0x%p)") */
	virtual void Initialize(WebView *view);
	/* the view going away */
	virtual void Terminate(WebView *view);
};

/* the GXM render target over a surface's memory the page is drawn into (0x8101613a; names ours) */
struct WebViewRenderTarget;

/* made for width x height at base (stride bytes a line), with the memory block it owns (-1:
   none); NULL on an error */
int CreateRenderTarget(WebViewRenderTarget **target, int width, int height, int stride,
	void *base, SceUID memblock);
int CreateRenderTarget(WebViewRenderTarget **target, int width, int height, int stride,
	void *base);
void DestroyRenderTarget(WebViewRenderTarget *target);

/* closes the popup menu (NULL: none) */
void DestroyPopupMenu(WebViewPopupMenu *menu);

/* the error a parameter gives without its widget (name ours) */
#define WEB_VIEW_ERROR_NO_WIDGET	((int)0x80AF0001)
/* the view has no size yet, or its surface no memory (names ours) */
#define WEB_VIEW_ERROR_NO_SIZE		((int)0x80AF0084)
#define WEB_VIEW_ERROR_NO_MEMORY	((int)0x80AF0022)

/* every key held (names ours) */
#define WEB_VIEW_KEY_ALL		6
/* WebCore's cursor type of none */
#define WEB_VIEW_CURSOR_NONE		37

/* the page's parts the web view fills, the widgets its parameter names (names ours) */
enum {
	WEB_VIEW_PART_TITLE = 0,
	WEB_VIEW_PART_URL = 1,
	WEB_VIEW_PART_BACK = 2,
	WEB_VIEW_PART_FORWARD = 3,
	WEB_VIEW_PART_RELOAD = 4,
	WEB_VIEW_PART_PROGRESS = 5
};

/* the analog sticks' listener of a web view: it passes their input to the view it is bound to
   (names ours) */
class WebViewAnalogListener : public UIInputGestureListener {
public:
	WebViewAnalogListener(Environment *env) : UIInputGestureListener(env)
	{
		m_type = inputdevice::DEVICE_TYPE_ANALOG;
	}

	virtual ~WebViewAnalogListener()
	{
	}

	virtual void OnUpdate(const inputdevice::Data *data);
};

/* the plane the page's tiles are drawn on ("view_plane"; the class name is ours) */
class WebViewViewPlane : public Widget {
public:
	WebViewViewPlane(Widget *parent, CreateParam *param);
	virtual ~WebViewViewPlane();

	/* in the widget's tail padding */
	WebView *m_view;
	void *m_renderer;
	int m_reserved644;
};

/* a load the web view keeps until its page is created: a URL, or data with its base URL
   (names ours) */
class WebViewLoadRequest {
public:
	enum {
		TYPE_NONE = -1,
		TYPE_URL = 0,
		TYPE_DATA = 1
	};

	WebViewLoadRequest() : m_type(TYPE_NONE), m_flag(true)
	{
	}

	virtual ~WebViewLoadRequest()
	{
	}

	string m_url;
	int m_type;
	bool m_flag;
	string m_data;
};

/* a key the web view sends the page, pressed or released (names ours) */
class WebViewKeyEvent {
public:
	WebViewKeyEvent(int keycode = 0, bool down = false) : m_keycode(keycode), m_down(down)
	{
	}

	virtual ~WebViewKeyEvent()
	{
	}

	int m_keycode;
	bool m_down;
};

/* a key held down on the page (names ours) */
struct WebViewHeldKey {
	int m_keycode;
	Widget *m_widget;
	/* the widget was given the key: it is told of the release */
	bool m_sent;
};

/* an animation the web view runs on the page (names ours) */
class WebViewAnim {
public:
	WebViewAnim(const math::v2 &pos, float scale) : m_pos(pos), m_scale(scale), m_state(0)
	{
	}

	virtual ~WebViewAnim()
	{
	}

	/* where the view moves to, at the scale */
	math::v2 m_pos;
	float m_scale;
	int m_state;
};

/* a plane the web view lays over the page while it loads: it fades the page out */
class WebViewLoadingPlane : public Plane {
public:
	WebViewLoadingPlane(Widget *parent);
	virtual ~WebViewLoadingPlane()
	{
	}

	/* whether it is shown; 2 and 3 while it fades */
	int m_state;
	int m_delay;
	int m_reserved;
};

/* the web view's resource parameters: the page it loads first, and the widgets of the page's
   parts it fills (Sony's names, from the resource's) */
class WebViewParam : public WidgetParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	WebViewParam();

	virtual const char *GetParamName() const;
	virtual ~WebViewParam();
	virtual int32_t Set();
	virtual int32_t Create(int32_t mode);

	const char *m_home_url;
	/* nothing reads it */
	uint32_t m_reserved;
	IDParam m_title_field;
	IDParam m_url_field;
	IDParam m_back_btn;
	IDParam m_forward_btn;
	IDParam m_reload_btn;
	IDParam m_progressbar;
};

class WebView : public ScrollView, public WebViewPageClient, public WebViewInputClient,
	public WebViewGeolocationClient {
public:
	static const char m_widget_type[];
	static const char *TypeName();

	WebView(Widget *parent, CreateParam *param);
	virtual ~WebView();

	virtual bool IsInherit(const char *name) const;
	virtual char *GetTypeName() const;

	/* the widget of one of the page's parts (WEB_VIEW_PART_*), its title, URL, buttons and
	   progress bar */
	void SetPart(Widget *widget, int part, bool attach);
	/* loads the page at the URL */
	void LoadUrl(const Url &url);

	/* the surface the page is drawn into, the view's size, and its render target */
	int CreateSurface();

	/* the page made for the view at its size, with WebKit's view of the page made before
	   (posted) or a new one (sent, while there is none) */
	void CreatePageForView();
	int CreatePage();
	/* the web view listener told of the view, and its page made if it has none */
	void Start();

	/* the held key let go (WEB_VIEW_KEY_ALL: every one), its widget told */
	void ReleaseHeldKey(int keycode);
	/* the cursor shown where the view has one, unless the page's cursor is none; and so
	   after it was hidden for the page */
	void ShowCursor();
	void UnhideCursor();

	/* the IME closed, the view's padding for it taken away and its zoom put back */
	void CloseIme();

	/* moves the cursor to pos, its tip at it, over the time */
	void MoveCursor(math::v2 *pos, float time);

	/* deletes the web IPC's data the page sent */
	void ClearIpcData();

	/* the analog sticks' input, from its analog listener */
	void HandleAnalog(const inputdevice::Data *data);

	virtual int32_t HandleKeycodeEvent(Event *e);
	virtual int32_t HandleGestureEvent(Event *e);
	virtual int32_t EnableTouchRearEvent(bool on_off);
	virtual void DestroyWidget();
	virtual void Update();
	virtual void SetVerticalScrollBar(ScrollBar *scroll_bar);
	virtual void SetHorizontalScrollBar(ScrollBar *scroll_bar);
	virtual void SetViewFocus(const math::v2 &pos, Timer *timer, bool flag_a, bool flag_b);
	virtual void OnPartsLayoutUpdated(bool is_dirty);

	/*
	 * The page's events, from the WebKit process; most pass to the view's listener. The names
	 * and the arguments are provisional, from what each passes on.
	 */
	virtual void ResetPage();
	virtual void OnLoadFinished();
	virtual void OnLoadFailed(int error, int arg);
	virtual void OnLoadCommitted();
	virtual void OnPageLayout(int arg);
	virtual void OnLoadProgress(int progress);
	virtual void OnTitleChanged(const string &title);
	virtual void OnUrlChanged(const char *url);
	virtual void OnEvent8(int arg);
	virtual void UpdatePageRect();
	virtual void OnEvent10(int arg);
	virtual void OnEvent11(int arg0, int arg1);
	virtual void OnEvent12(int arg);
	virtual void OnEvent13(int arg0, int arg1, int arg2);
	virtual void OnEvent14(int arg0, int arg1, int arg2, int arg3, int arg4);
	virtual void OnEvent15(int arg);
	virtual void OnEvent16();
	virtual void OnHistoryChanged(bool canGoBack, bool canGoForward);
	virtual void OnEvent18(int arg0, int arg1);
	virtual void OnEvent19(int arg0, int arg1);
	virtual void OnEvent20(int arg0, int arg1);

	/* WebViewPageClient */
	virtual void OnPageCreated(int result);
	virtual void OnPageClosed(int reason);
	virtual void OnPageEvent0()
	{
	}
	virtual void OnPageEvent1();
	virtual void OnPageEvent2();
	virtual void OnPageEvent3();
	virtual void OnPageEvent4()
	{
	}

	/* WebViewInputClient */
	virtual void OnInput0();
	virtual void OnInput1();
	virtual void OnInput2();
	virtual void OnInput3();
	virtual void OnInput4();
	virtual void OnInput5();
	virtual void OnInput6();
	virtual void OnInput7();
	virtual void OnInput8();
	virtual void OnInput9();

	/* WebViewGeolocationClient */
	virtual void OnPosition(double latitude, double longitude, double altitude, double accuracy,
		double altitudeAccuracy, double heading, double speed, double timestamp, int flags,
		int valid);

	/* the receive buffer of the page's data */
	class Buffer {
	public:
		virtual ~Buffer();

		SceSize m_size;
		SceSize m_length;
		void *m_data;
	};

	/* an IPC message the page sent, kept until the view's update reads it */
	class IpcData {
	public:
		virtual ~IpcData();
	};

	Buffer *m_receiveBuffer;		/* 1100 */
	int m_viewId;				/* 1104: the page's ID with the WebKit process */
	int m_pageState;			/* 1108: WEB_VIEW_PAGE_* */
	WebViewPageListener *m_listener;	/* 1112 */
	WebViewLoadingPlane *m_loadingPlane;	/* 1116 */
	void *m_appListener;			/* 1120: wutil's listener */
	vector<IpcData *> m_ipcData;		/* 1124 */
	int m_reserved1140;
	thread::RMutex m_ipcDataLock;		/* 1144 */
	intrusive_ptr<graph::Surface> m_surface;	/* 1176 */
	graph::SurfacePool *m_surfacePool;	/* 1180 */
	int m_surfaceWidth;			/* 1184 */
	WebViewIme *m_ime;			/* 1188: the IME the page opened */
	WebViewRenderTarget *m_renderTarget;	/* 1192 */
	bool m_isSecure;			/* 1196 */
	int m_cursorType;			/* 1200: WebCore's cursor type */
	WebViewPopupMenu *m_popupMenu;		/* 1204 */
	string m_popupText;			/* 1208 */
	map<string, string> m_popupItems;		/* 1220 */
	int m_loadProgress;			/* 1232 */
	int m_loadState;			/* 1236 */
	bool m_loadCanceled;			/* 1240 */
	bool m_canGoBack;			/* 1241 */
	bool m_canGoForward;			/* 1242 */
	Url m_url;				/* 1244 */
	common::String m_title;			/* 1324 */
	int m_imeType;				/* 1348 */
	string m_imeText;			/* 1352 */
	int m_imeMaxLength;			/* 1364 */
	bool m_imeOpen;				/* 1368 */
	bool m_imeEnabled;			/* 1369 */
	math::v2 m_contentScale;		/* 1376 */
	float m_scale;				/* 1384 */
	float m_maxScale;			/* 1388 */
	float m_minScale;			/* 1392 */
	bool m_userScalable;			/* 1396 */
	int m_scrollX;				/* 1400 */
	int m_imeState;				/* 1404 */
	int m_scrollY;				/* 1408 */
	math::v4 m_pageRect;			/* 1416 */
	math::v4 m_lastPageRect;		/* 1432 */
	bool m_zoomed;				/* 1448 */
	float m_zoomX;				/* 1452 */
	float m_zoomY;				/* 1456 */
	math::v4 m_zoomRect;			/* 1464 */
	bool m_contextMenuShown;		/* 1480 */
	common::String m_imeTitle;		/* 1484 */
	common::String m_imeInitialText;	/* 1508 */
	datetime::DateTime m_imeMinDate;	/* 1532 */
	datetime::DateTime m_imeMaxDate;	/* 1556 */
	common::String m_imeDefault;		/* 1580 */
	int m_dialogType;			/* 1604 */
	int m_dialogState;			/* 1608 */
	int m_dialogButtons;			/* 1612 */
	int m_dialogResult;			/* 1616 */
	int m_dialogFlags;			/* 1620 */
	int m_dialogId;				/* 1624 */
	common::String m_dialogMessage;		/* 1628 */
	common::String m_dialogDefault;		/* 1652 */
	common::String m_dialogInput;		/* 1676 */
	int m_dialogReply;			/* 1700 */
	vector<int> m_selectIndices;		/* 1704 */
	int m_selectCount;			/* 1720 */
	int m_selectFocus;			/* 1724 */
	vector<string> m_selectItems;		/* 1728 */
	vector<bool> m_selectEnabled;		/* 1744 */
	vector<bool> m_selectChecked;		/* 1760 */
	bool m_selectMultiple;			/* 1776 */
	bool m_startWithParam;			/* 1777 */
	math::v4 m_tapRect;			/* 1784 */
	float m_tapScale;			/* 1800 */
	bool m_tapping;				/* 1804 */
	bool m_tapButton;			/* 1805 */
	int m_inputMask;			/* 1808 */
	math::v2 m_dragStart;			/* 1816 */
	math::v2 m_cursorPos[8];		/* 1824 */
	math::v2 m_cursorMove;			/* 1888 */
	bool m_cursorEnabled;			/* 1896 */
	common::SharedPtr<inputdevice::InputListener> m_keyListener;	/* 1900 */
	common::SharedPtr<inputdevice::InputListener> m_analogListener;	/* 1908 */
	int m_reserved1916;
	int m_reserved1920;
	int m_reserved1924;
	/* the cursor's textures: the arrow and the hand, each with a second one the view switches
	   to and from while the page is touched (names ours) */
	intrusive_ptr<graph::Surface> m_arrowTexture;		/* 1928 */
	intrusive_ptr<graph::Surface> m_arrowAltTexture;	/* 1932 */
	intrusive_ptr<graph::Surface> m_handTexture;		/* 1936 */
	intrusive_ptr<graph::Surface> m_handAltTexture;		/* 1940 */
	Widget *m_cursorPlane;			/* 1944 */
	int m_cursorMode;			/* 1948 */
	bool m_cursorHidden;			/* 1952 */
	math::v2 m_cursorOffset;		/* 1960 */
	list<WebViewAnim> m_scrollAnims;		/* 1968 */
	list<WebViewAnim> m_zoomAnims;			/* 1980 */
	list<WebViewAnim> m_tapAnims;			/* 1992 */
	math::v2 m_reserved2008;
	WebViewLoadRequest m_loadRequest;	/* 2016 */
	list<WebViewKeyEvent> m_keyQueue;	/* 2052: the keys waiting for the last one's reply */
	bool m_keyReady;			/* 2064: no key waits for a reply */
	WebViewKeyEvent m_lastKey;		/* 2068 */
	list<WebViewHeldKey> m_heldKeys;	/* 2080 */
};

}
}

#endif
