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
#include <std/set>
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
#include <widget/core/environment.h>
#include <widget/core/uiinputlistener.h>
#include <widget/wutil.h>
#include <widget/w_plane.h>
#include <widget/w_scroll_view_base.h>
#include <widget/core/widget_gesture.h>
#include "context_menu_info.h"
#include "web_ipc.h"

namespace paf {
namespace ui {

/* the results of the dialogs the view opens for the page, the dialog's owner (the web
   view's second base; names ours) */
class WebViewDialogClient {
public:
	virtual ~WebViewDialogClient()
	{
	}

	virtual void OnMessageDialogResult(int result) = 0;
	virtual void OnDialogResult(int result) = 0;
	virtual void OnPageEvent0() = 0;
	virtual void OnInputDialogResult(int result, int reopen) = 0;
	virtual void OnAuthDialogResult(int result) = 0;
	virtual void OnGeolocationDialogResult(int result) = 0;
	virtual void OnPageEvent4() = 0;
};

/* the IME's events for the page's text field (the third base, given to ImeOpen; named after the
   SCE_IME_VSH_EVENT_* and SCE_IME_EVENT_* each is called for, names ours); none does anything
   by default */
class WebViewInputClient {
public:
	virtual ~WebViewInputClient()
	{
	}

	virtual void OnImeUpdatePreedit(const string &text, int caret)
	{
	}
	virtual void OnImeCommit(const string &text, int caret)
	{
	}
	virtual void OnImeSetText(const string &text)
	{
	}
	virtual void OnImeBackspace()
	{
	}
	virtual void OnImeDelete()
	{
	}
	virtual void OnImeMoveCaret(int dx, int dy)
	{
	}
	virtual void OnImeClear()
	{
	}
	virtual void OnImePressEnter()
	{
	}
	virtual void OnImeOpen(int height)
	{
	}
	virtual void OnImePressClose()
	{
	}
};

/* the device's position, for the page's geolocation (the fourth base) */
class WebViewGeolocationClient {
public:
	virtual ~WebViewGeolocationClient()
	{
	}

	virtual void OnPosition(WebIpcGeolocationPosition position) = 0;
};

class WebView;

/* the device's location for a page (vtable 0x81025bec; names ours), made by CreateGeolocation
   (geolocation_monitor.cpp) */
class WebViewGeolocation {
public:
	/* the position sent to the view's client as it changes, from now on, and no more */
	virtual void Start() = 0;
	virtual void Stop() = 0;
	/* whether the page may have the location: 0 not known yet (the user is being asked),
	   1 allowed, 2 denied */
	virtual int GetPermission() = 0;
	/* the question to the user taken back */
	virtual void AbortPermission() = 0;
	virtual ~WebViewGeolocation()
	{
	}
};

/* a listener that calls a method of its object (vtable 0x81025204 for WebView; names ours) */
template <class T>
class WebViewMemberListener : public EventListener {
public:
	typedef void (T::*Func)();

	WebViewMemberListener(T *object, Func func) : m_object(object), m_func(func)
	{
	}

	virtual ~WebViewMemberListener()
	{
	}

	virtual int32_t Do(int32_t type, Handler *self, Event *e)
	{
		if (m_killed)
			return 0x80AF4101;
		(m_object->*m_func)();
		return 0;
	}

	T *m_object;
	Func m_func;
};

/* the system IME a page's text field opens (web_ime.cpp; names ours) */
struct WebViewIme;

/* the IME, made once (0x8100df60) */
WebViewIme *ImeGetInstance();

/* the IME's input type and enter label for a field's type ("password", "number", "tel",
   "email", "url", "search") */
void ImeGetInputType(const string &type, int *inputType, int *enterLabel);

/* opens the IME for the client (sceImeVshOpen), or gives it the new parameters when open */
int ImeOpen(WebViewIme *ime, WebViewInputClient *client, int inputType, int enterLabel,
	bool multiline);

/* closes the IME (sceImeVshClose) and frees its text */
int ImeClose(WebViewIme *ime);

/* the IME told of a change of the field's text: the length is ScePafLowlayer 5CE62E49's
   index */
int ImeTextChanged(WebViewIme *ime, int length);

/* the IME's frame (sceImeVshUpdate), while it is open */
int ImeUpdate(WebViewIme *ime);

/* the render parameters the page is made with (80 bytes at 0x81026548; names ours) */
struct WebViewRenderParam {
	uint32_t m_data[20];
};

/* the render parameters every page is made with */
WebViewRenderParam *GetRenderParam();

/* what the page's data request gives the application's data listener: its start with the
   MIME type and the length, each part of the data, and its end with the result (names ours) */
class WebViewDataEvent {
public:
	enum {
		TYPE_START = 0,
		TYPE_DATA = 1,
		TYPE_END = 2
	};

	WebViewDataEvent(int type)
		: m_type(type), m_data(NULL), m_size(0), m_length(0), m_result(0)
	{
	}

	virtual ~WebViewDataEvent()
	{
	}

	int m_type;
	string m_mimeType;
	string m_reserved;
	void *m_data;
	int m_size;
	int64_t m_length;
	int m_result;
};

/* the page's select popup as the application is given it: its groups of items (names ours) */
class WebViewSelectItem {
public:
	virtual ~WebViewSelectItem()
	{
	}

	string m_text;
	/* not a group's label */
	bool m_selectable;
	bool m_enabled;
};

}
}

PAF_VECTOR_INLINE_DTOR(paf::ui::WebViewSelectItem)
PAF_VECTOR_NOINLINE_PUSH_BACK(paf::ui::WebViewSelectItem, paf::allocator<paf::ui::WebViewSelectItem>)

namespace paf {
namespace ui {

class WebViewSelectGroup {
public:
	virtual ~WebViewSelectGroup()
	{
	}

	vector<WebViewSelectItem> m_items;
	string m_label;
	bool m_reserved;
};

}
}

PAF_VECTOR_INLINE_DTOR(paf::ui::WebViewSelectGroup)
PAF_VECTOR_NOINLINE_PUSH_BACK(paf::ui::WebViewSelectGroup, paf::allocator<paf::ui::WebViewSelectGroup>)

namespace paf {
namespace ui {

class WebViewSelectInfo {
public:
	virtual ~WebViewSelectInfo()
	{
	}

	vector<WebViewSelectGroup> m_groups;
	string m_title;
	bool m_multiple;
	bool m_multipleShown;
};

/* the application's listener of the page's data (wutil's ScePafWidget_E638AA1F; the slots
   not seen called are numbered, names ours) */
class WebViewDataListener {
public:
	WebViewDataListener();
	virtual ~WebViewDataListener();
	/* the page's select popup opened (name ours) */
	virtual void OnSelect(WebView *view, const WebViewSelectInfo &info) = 0;
	virtual void OnSlot3(WebView *view) = 0;
	virtual void OnSlot4() = 0;
	virtual void OnData(WebViewDataEvent event, WebView *view) = 0;
};

/* the view's listener, told of its page's events (the class name and the method names are
   ours, after the view's methods that call each; slots not yet seen called are numbered) */
class WebViewPageListener {
public:
	WebViewPageListener();
	virtual ~WebViewPageListener();
	/* the page was reset (name ours) */
	virtual void OnPageReset(WebView *view);
	virtual void OnLoadFinished(WebView *view, int state);
	virtual void OnLoadFailed(WebView *view, int error, const common::String &url);
	virtual void OnLoadCommitted(WebView *view);
	/* the page is laid out and its pending load started (name ours) */
	virtual void OnPageLayout(WebView *view);
	virtual void OnLoadProgress(WebView *view);
	virtual void OnTitleChanged(WebView *view, const common::String &title);
	virtual void OnUrlChanged(WebView *view, const Url &url);
	virtual void OnEvent8(WebView *view, int arg);
	/* the page's position in the view, and the view's top padding left showing above it
	   (names ours) */
	virtual void OnPagePos(WebView *view, const math::v2 &pos);
	virtual void OnPageTop(WebView *view, const float &top);
	virtual void OnSlot13();
	/* a long press on a link or an image: what it hit (name ours) */
	virtual void OnContextMenu(WebView *view, const ContextMenuInfo &info);
	/* the page asks for a new window at the URL, of the size: the view made for it, and
	   whether the listener took the request itself (names ours) */
	virtual void OnCreateWindow(WebView *view, const Url &url, int width, int height,
		WebView **window, bool *handled);
	virtual void OnSlot16(WebView *view);
	virtual void OnEvent11(WebView *view, int arg0, int arg1);
	virtual void OnEvent12(WebView *view, const Url &url);
	/* the page asks whether it may go to the URL: *allow, true unless the listener says no
	   (names ours) */
	virtual void OnNavigationRequest(WebView *view, const Url &url, int type, bool *allow);
	/* the page asks to download the URL (names ours) */
	virtual void OnDownloadRequest(WebView *view, const Url &url, const string &mimeType,
		int64_t length);
	/* a cursor key the page did not take: its direction bit (1 81, 2 54, 4 64, 8 74), and
	   whether it was let go (names ours) */
	virtual void OnCursorKey(WebView *view, int direction, bool up);
	virtual void OnEvent15(WebView *view, int arg);
	virtual void OnEvent16(WebView *view);
	virtual void OnHistoryChanged(WebView *view, bool canGoBack, bool canGoForward);
	virtual void OnEvent18(WebView *view, int arg0, int arg1);
	virtual void OnImeClosed(WebView *view, int arg0, int arg1);
	/* the page's confirm, with its text, before its dialog opens (names ours) */
	virtual void OnConfirm(WebView *view, const common::String &text);
};

/* the application's web view listener, which starts the WebKit process for the views (Sony's
   name, from its messages; wutil::GetWebViewListener) */
class WebViewListener {
public:
	WebViewListener();
	virtual ~WebViewListener();
	/* the view starting ("[WebViewListener] Initialize (by WebView: 0x%p)") */
	virtual void Initialize(WebView *view) = 0;
	/* the view going away */
	virtual void Terminate(WebView *view) = 0;
	virtual void OnSlot4() = 0;
	virtual void OnSlot5() = 0;
	virtual void OnSlot6() = 0;
	virtual void OnSlot7() = 0;
};

/* the GXM render target over a surface's memory the page is drawn into (0x8101613a; names ours) */
struct WebViewRenderTarget;

/* made for width x height at base (stride bytes a line), with the memory block it owns (-1:
   none); NULL on an error */
int CreateRenderTarget(WebViewRenderTarget **target, int width, int height, int stride,
	void *base, SceUID memblock);
int CreateRenderTarget(WebViewRenderTarget **target, int width, int height, int stride,
	void *base);
int DestroyRenderTarget(WebViewRenderTarget *target);
/* runs the drawing the page's process queued for the render target (name ours) */
int ProcessRenderCommands(WebViewRenderTarget *target);

/* the location for the client, NULL when sceLocation cannot be opened (0x81016844), and
   freed (NULL: none) */
WebViewGeolocation *CreateGeolocation(WebViewGeolocationClient *client);
void DestroyGeolocation(WebViewGeolocation *geolocation);

/* the view has no size yet, or its surface no memory (names ours) */
#define WEB_VIEW_ERROR_NO_SIZE		((int)0x80AF0084)
#define WEB_VIEW_ERROR_NO_MEMORY	((int)0x80AF0022)
#define WEB_VIEW_ERROR_NO_TEXTURE	((int)0x80AF008F)
/* the page's data did not come (name ours) */
#define WEB_VIEW_ERROR_DATA_FAILED	((int)0x80AF0024)

/* WebCore's cursor type of none */
#define WEB_VIEW_CURSOR_NONE		37
/* WebCore's cursor types of the pointer and of the hand, over a link */
/* what the page did with a press (names ours) */
#define WEB_VIEW_HIT_CONTEXT_MENU	1
#define WEB_VIEW_HIT_ZOOM		2

/* m_pageInfo: the page's content size and its viewport's scales are known (names ours) */
#define WEB_VIEW_PAGE_CONTENT_SIZE	1
#define WEB_VIEW_PAGE_VIEWPORT		2

#define WEB_VIEW_CURSOR_POINTER		0
#define WEB_VIEW_CURSOR_HAND		2

/* the page's parts the web view fills, the widgets its parameter names (names ours) */
enum {
	WEB_VIEW_PART_TITLE = 0,
	WEB_VIEW_PART_URL = 1,
	WEB_VIEW_PART_BACK = 2,
	WEB_VIEW_PART_FORWARD = 3,
	WEB_VIEW_PART_RELOAD = 4,
	WEB_VIEW_PART_PROGRESS = 5,
	/* every part */
	WEB_VIEW_PART_ALL = 6
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

class WebViewViewPlane;

/*
 * The listener a view plane's touch is smoothed with (constructor 0x81012e58, vtable 0x810259b0,
 * 0x114 bytes; names ours): it keeps the touch's last places and gives a place without the
 * panel's one-pixel jitter, each axis on its own (GetX, GetY).
 */
class WebViewTouchListener : public TouchGestureListener {
public:
	WebViewTouchListener(uint32_t port, uint32_t touch_id, void *user_data);
	virtual ~WebViewTouchListener();

	virtual int32_t OnUpdateTouch(uint32_t id);

	/* the smoothed place (0x81013a04, 0x81013a0a) */
	float GetX();
	float GetY();

	/* 0 until the touch is down, 1 while it is */
	int32_t m_state;
	/* the smoothed place GetX and GetY give */
	float m_pos[2];
	/* the touch's place now, and the smoothed place it gives */
	float m_cur[2];
	float m_out[2];
	/* the touch's places of the updates before, the last first */
	float m_history[14][2];
	/* the place the smoothed one moves with, now and the update before */
	float m_target[2];
	float m_targetPre[2];
	/* the smoothed place, now and the update before */
	float m_smoothed[2];
	float m_smoothedPre[2];
	/* the place held while the axis jitters, now and the update before */
	float m_base[2];
	float m_basePre[2];
	/* the updates the axis stayed at one place, now and the update before */
	int32_t m_stillCount[2];
	int32_t m_stillCountPre[2];
	/* how long the axis stayed at its last place, now and the update before */
	int32_t m_lastRun[2];
	int32_t m_lastRunPre[2];
	/* the axis jitters by a pixel (one of seven patterns of its last places), now and the
	   update before, and it began to */
	bool m_jitter[2];
	bool m_jitterPre[2];
	bool m_jitterOn[2];
	/* the touch moved on, in the time of each axis's last place */
	bool m_moved[2];
	/* the axis is held at m_base, now and the update before */
	bool m_hold[2];
	bool m_holdPre[2];
};

/*
 * The listener of a widget's two touches on the panel, with the L and R buttons (constructor
 * 0x81012780, vtable 0x81025984, 0x74 bytes; names ours): it sends the gestures of m_gesture
 * (40 to 44, set where it is made) as the touches and buttons change.
 */
class WebViewMultiTouchListener : public MultiTouchGestureListener {
public:
	WebViewMultiTouchListener(uint32_t port, uint32_t touch_id, void *user_data);
	virtual ~WebViewMultiTouchListener();

	virtual int32_t OnUpdateMultiTouch(uint32_t bitmap, uint32_t bitmap_pre, int32_t diff);

	uint32_t reserved38;		/* never touched */
	/* the gestures it sends: begin, move, and the three ends */
	int32_t m_gesture[5];
	uint32_t reserved50[2];		/* never touched */
	/* 0 until a touch is down, 1 while one is, 2 with L or R, 3 once they are let go */
	int32_t m_state;
	/* where the touch was pressed */
	int32_t m_x;
	int32_t m_y;
	/* L and R held, now and the update before */
	bool m_l;
	bool m_lPre;
	bool m_r;
	bool m_rPre;
	/* the button the gestures carry: 0 R, 1 L, 2 none */
	int32_t m_button;
	/* the touches it held the update before */
	int32_t m_countPre;
	/* the second touch is down, now and the update before */
	bool m_second;
	bool m_secondPre;
};

/* the widget shown, or hidden, at once (inlined in Sony's code; names ours) */
static inline void ShowNow(Widget *widget)
{
	if (!widget->m_transition_complete) {
		widget->m_transition = 0xff;
		widget->m_transition_complete = true;
		widget->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(widget, NULL, 2, 0.0f, NULL);
}

static inline void HideNow(Widget *widget)
{
	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(widget, NULL, 2, 0.0f, NULL);
}

/*
 * The plane that points where a rear touch is, one for every view, made once (0x810141b4; the
 * name is ours, after its draw object's): it draws the environment's three touch surfaces
 * (Environment's m_reserved_surface_a, b and c) on its planes a, c and b.
 */
class FingerPointer : public Plane {
public:
	/* its states: hidden, the press shown (plane b), the pointer shown on plane a or c */
	enum {
		STATE_HIDDEN = 0,
		STATE_PRESS = 1,
		STATE_POINT_A = 2,
		STATE_POINT_C = 3
	};

	FingerPointer(Widget *parent, CreateParam *param);
	virtual ~FingerPointer();

	virtual int32_t OnDraw(graph::DrawInfo &info);

	/* a surface with where it is anchored and its tip (x and y the anchor, z the tip, as
	   fractions of the surface), or its anchor alone (names ours) */
	void SetSurfaceA(intrusive_ptr<graph::Surface> surface, SceFColor region);
	void SetSurfaceB(intrusive_ptr<graph::Surface> surface, SceFVector2 anchor);
	void SetSurfaceC(intrusive_ptr<graph::Surface> surface, SceFColor region);
	void SetEnabled(bool enabled);
	/* the pointer shown at the place with the angle, moved there, and hidden at the place
	   (0x810144ce, 0x81014522, 0x81014530) */
	void Begin(float x, float y, float angle);
	void Move(float x, float y, float angle);
	void End(float x, float y);
	/* where its tip is from its place: its size along its angle (0x81014b90) */
	math::v2 GetTipOffset() const;
	/* its planes shown, sized and placed for its state, from its draw object's Draw */
	void DrawPointer();

	/* fields: names ours, from what the pointer's functions do with them */
	int32_t m_size;			/* surface a's tip length in pixels (160 until set) */
	int32_t m_defaultSize;		/* 84; nothing here reads it */
	int32_t m_state;		/* STATE_* */
	SceUInt64 m_stateTime;		/* when it was made */
	/* its place and angle (degrees) */
	float m_x;
	float m_y;
	float m_angle;
	float m_scale;			/* plane b's size scale: 1.0 */
	bool m_enabled;			/* plane b is shown with the pointer (SetEnabled) */
	uint8_t reserved[164];		/* never touched by the pointer's functions */
	Plane *m_planeA;		/* the planes surfaces a, c and b are drawn on */
	Plane *m_planeC;
	Plane *m_planeB;
	math::v4 m_sizeA;		/* each plane's size */
	math::v4 m_sizeC;
	math::v4 m_sizeB;
};

/*
 * The plane in the scene over a view plane (0x81010bec; names ours): it takes the rear touches
 * and the presses on its view plane, and shows the finger pointer.
 */
class WebViewRenderPlane : public Plane {
public:
	WebViewRenderPlane(Widget *parent, WebViewViewPlane *viewPlane);
	virtual ~WebViewRenderPlane();

	virtual int32_t OnDraw(graph::DrawInfo &info);
	/* it and the finger pointer shown, or hidden, at once */
	virtual int32_t Show();
	virtual int32_t Hide();
	virtual void OnUpdate();

	/* a touch on the panel (port) with its ID, passed on from the widget */
	void OnTouch(uint32_t port, uint32_t id, Handler *widget);

	float m_pixelPerMm;		/* the front panel's */
	Widget *m_root;			/* the parent it was made in */
	Environment *m_rootEnv;
	Camera *m_camera;		/* the parent's scene's */
	WebViewViewPlane *m_viewPlane;
	int m_reserved;			/* only cleared */
	/* a press is held on the view plane, and how many */
	bool m_pressed;
	int m_pressCount;
	int m_reserved2[2];		/* only cleared */
	uint8_t reserved[20];
	FingerPointer *m_indicator;
	/* only released (what it holds is not known; a draw object, as the plane's) */
	common::SharedPtr<graph::DrawObj> m_reserved3;
	bool m_enabled;
};

/*
 * The plane the page's tiles are drawn on ("view_plane"; the class name is ours): it passes the
 * front touches and gestures on it to its view, and has a render plane in the scene that draws
 * the tiles.
 */
class WebViewViewPlane : public Widget {
public:
	static const char m_widget_type[];

	WebViewViewPlane(Widget *parent, CreateParam *param);
	virtual ~WebViewViewPlane();

	virtual int32_t HandleTouchEvent(Event *e);
	virtual int32_t HandleGestureEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void UpdateSize();

	/* where the front panel's first touch line crosses the plane, in its space */
	math::v2 GetLinePos();

	/* where the touch is on the plane: the hit's, or on its line where the hit is not the
	   front panel's */
	math::v2 GetTouchPos()
	{
		HitInfo hit;

		if (m_env->GetHitStack(&hit, this) == 0 && hit.m_port == 0)
			return hit.m_touch_pos;
		return GetLinePos();
	}

	/* in the widget's tail padding */
	WebView *m_view;
	WebViewRenderPlane *m_renderer;
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
/* an origin's answer to the page's location request, kept for its next request: ordered by
   the origin's length, then the origin; three are kept, the one used longest ago dropped for
   a new one (names ours) */
struct WebViewGeolocationPermission {
	WebViewGeolocationPermission(const char *origin)
		: m_state(2), m_origin(origin), m_length(m_origin.size()), m_time(0)
	{
	}

	bool operator<(const WebViewGeolocationPermission &rhs) const
	{
		if (m_length != rhs.m_length)
			return m_length < rhs.m_length;
		return m_origin < rhs.m_origin;
	}

	int m_state;		/* 1 allowed, 2 denied */
	string m_origin;
	size_t m_length;
	uint32_t m_time;	/* the use count when it was last asked */
};

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

/* the widget of one of the page's parts the view fills (names ours) */
struct WebViewPart {
	WebViewPart(int part, Widget *widget, bool attach)
		: m_part(part), m_widget(widget), m_attach(attach)
	{
	}

	int m_part;
	Widget *m_widget;
	/* the view was given the widget: it destroys it with the part */
	bool m_attach;
};

/* an animation the web view runs on the page (names ours) */
class WebViewAnim {
public:
	WebViewAnim(const math::v2 &pos, float scale, int state = 0)
		: m_pos(pos), m_scale(scale), m_state(state)
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

	/* fades it out; a fade in progress ends shown (name ours) */
	void Hide();
	/* fades it in over the time its delay settings give; shown again (state 2) when it
	   is already (name ours) */
	void Show();
	/* shown at once, then faded out over 500 ticks; done again (state 3) when it is fading
	   (name ours) */
	void Flash();
	/* the colour of the style (see m_style; name ours) */
	void SetStyle(int style);

	/* the fade asked for while it was fading, done now (name ours) */
	void RunPending()
	{
		int state = m_state;

		if (state == 2) {
			m_state = 0;
			Show();
		} else if (state == 3) {
			m_state = 0;
			Flash();
		} else {
			m_state = 0;
		}
	}

	/* whether it is shown; 2 and 3 while it fades */
	int m_state;
	int m_delay;
	/* its colour, and with delay 1 how long it fades in: 0 grey over 30 ticks, 1 blue over
	   200 (name ours) */
	int m_style;

private:
	void FadeIn(float time);
	void ShowNow();
	void FadeOut(float time);
};

/* the web view's style ("style_web_view"): a widget style of its own name */
class WebViewStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	WebViewStyleParam()
	{
	}

	virtual const char *GetParamName() const;
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

class WebView : public ScrollView, public WebViewDialogClient, public WebViewInputClient,
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
	int SetPart(Widget *widget, int part, bool attach);
	/* loads the page at the URL */
	int LoadUrl(const Url &url);
	/* loads the data, given as source with its scheme, from a file URL of the rest (names
	   ours) */
	int LoadData(const string &source, bool flag, const string &type);
	/* the page goes back and forward in its history, reloads, stops loading (names ours) */
	int GoBack();
	int GoForward();
	int Reload();
	int Stop();
	/* sends 0x1050 (name ours) */
	int Request1050();
	/* the zoom's top padding, its x offset, and whether the view clips its children; the
	   layout rectangle kept to the screen below the top (names ours) */
	int SetZoomTop(const math::v2 &top, bool clip, float x, bool zoomed);
	/* the zoom's bottom padding and its y offset (names ours) */
	int SetZoomOffset(float bottom, float y);
	/* the bool settings (names ours) */
	int SetSetting1031(bool value);
	int SetSetting1048(bool value);
	int SetSetting1053(bool value);
	/* hides the loading plane, then OnImePressClose (name ours) */
	int HideLoading();
	/* the loading plane's delay before it shows (name ours) */
	int SetLoadingDelay(int delay);
	/* the view's options (name ours) */
	int SetOptions(int options);
	/* the inputs the view takes (4: pinch), and the view plane's pinch mode with them (names
	   ours) */
	void SetInputMask(int mask);
	void AddInputMask(int mask);
	void RemoveInputMask(int mask);
	/* whether the view has a size, a content scale and a scale to zoom by (names ours) */
	bool CanZoom();
	/* zooms by the factor, within the page's scale limits (name ours) */
	void ZoomBy(float factor);
	/* the zoom actions: 1 fits the page's width, 2 to the page's top, 4 in, 8 out (names
	   ours) */
	void Zoom(int action);
	/* back to the page's own scale, about the point (names ours) */
	void ResetZoom(const math::v2 &pos);
	void ResetZoom();
	/* taps the page at the point; the loading plane shows (name ours) */
	void Tap(const math::v2 &pos, int button);
	/* the view plane's points to the page's: scaled by the page's width over the plane's, y
	   down, about the page's centre (name ours) */
	sce::Vectormath::Simd::Aos::Matrix3 GetPageTransform();
	/* the page's points to the view plane's, the inverse of GetPageTransform (name ours) */
	sce::Vectormath::Simd::Aos::Matrix3 GetViewTransform();
	/* the cursor moved to the page's point, kept in the visible part, once the keys queued
	   are dropped (name ours) */
	void MoveCursorToPagePos(int x, int y);
	/* the page gone: its surface dropped and the view's texture cleared (name ours) */
	__attribute__((noinline)) void ClearSurface();
	/* taps the page where the drag started (name ours) */
	void TapAtDragStart();
	/* sends 0x1070 (name ours) */
	void Request1070();
	/* asks the page for its data: the receive buffer is made the size (name ours) */
	void RequestData(int64_t id, int64_t size);
	/* the select popup's items let go (name ours) */
	void ClearSelect();
	/* the select popup closes on the item (name ours) */
	int CloseSelect(int index);
	/* the select popup chooses the item, at once or when it closes; a disabled item only
	   closes it (name ours) */
	int ChooseSelect(uint32_t index, bool force, bool close);
	/* sends the two strings (name ours) */
	int SendStringPair(const string &first, const string &second);
	/* the IME's type and text, sent to the page (name ours) */
	int SetImeText(int type, const string &text);
	/* sends 0x1051 (name ours) */
	int Request1051();
	/* scrolls the page; kept once the page takes it (name ours) */
	int SetScrollY(int y);
	/* the view's focus, moved so that the view's box round it lies in the view (above the
	   bottom padding); with offset, y from the page's top (names ours) */
	math::v2 GetFittedViewFocus(bool offset);
	/* queues message 0x21 / 0x1056 for the view's update (names ours) */
	void QueueMessage21();
	void QueueMessage1056();
	/* the message the page sent, queued; the web process gone; the page taking the view's input
	   (names ours) */
	void ReceiveMessage(const WebIpcPacket *packet);
	void OnProcessExit();
	void SetPageHandlesInput(bool on);
	/* the page asks for a new window (name ours) */
	void OnCreateWindow(const WebIpcCreatePageWithView *message);
	/* the cursor moved by the step within the view (name ours) */
	math::v2 MoveCursorInView(const math::v2 &step, bool ime, bool scroll);
	/* the page's texture moved past the top padding or the IME (name ours) */
	void UpdateTexRegion();
	/* the view's part of the page sent to the page, and the cursor kept in it (name ours) */
	void UpdateViewport();
	/* whether a page may ask for the device's location (name ours) */
	void SetGeolocationEnabled(bool enabled);
	/* the origin asks for the device's location: asked of the user when no permission is
	   kept for it (name ours) */
	void RequestGeolocationPermission(const string &origin);
	/* the page's data: its start, each part kept in the receive buffer, given to the
	   application when the buffer is full, and its end (names ours) */
	void OnDataStart(int length, const string &mimeType);
	void OnData(const WebIpcEvent2030 *message);
	void OnDataEnd(bool success);
	/* the page asks whether it may go to the URL; the listener's answer is sent back in the
	   message (name ours) */
	void OnNavigationRequest(WebIpcEvent2027 *message);
	/* the view focused on the page's point; the top padding kept for a point at the top
	   (name ours) */
	void ScrollToPagePos(int x, int y);
	/* the dialog request kept while a message dialog is open (name ours) */
	bool QueueDialog(WebIpcMessage *message);
	/* the page asks for a user name and password: the dialog opened, or the request
	   declined when there is none; queued while another dialog is open (name ours) */
	bool OnAuthRequest(WebIpcEvent2025 *message);
	/* the page asks for a date or time: the IME's texts and range kept and the dialog
	   opened, or the request declined; queued while another dialog is open (name ours) */
	bool OnDateTimeRequest(WebIpcEvent2033 *message);
	/* the page asks for a message dialog of kind 0 or 1 (kind 1 not while one is open):
	   opened, or the request cancelled; queued while another dialog is open (name ours) */
	bool OnDialogRequest(WebIpcEvent2034 *message);
	/* the page's prompt, confirm and alert: their dialog opened, or with no dialog the
	   answer sent back at once; queued while another dialog is open (names ours) */
	bool OnPromptRequest(WebIpcEvent2015 *message);
	bool OnConfirmRequest(WebIpcEvent2014 *message);
	bool OnAlertRequest(WebIpcEvent2013 *message);
	/* the messages the page sent, taken from the IPC queue and handled in order; those a
	   dialog keeps are not freed (name ours) */
	void ProcessIpcData();
	/* the view animated to the page point at the scale: the plane sized to the scale, or at
	   scale 1 (or when the page may not be scaled) only scrolled to it (name ours) */
	void ZoomTo(const math::v2 &pos, float scale);
	/* the end of ZoomTo's animation: the tap animations dropped (name ours) */
	static void OnZoomDone(Timer *timer, AnimListener *listener, void *arg, void *data,
		void *reserved);
	/* the touch at the view point sent to the page for the port; a move to where the port
	   already is is not sent, and after an up the port has no point (name ours) */
	void SendTouch(const math::v2 &pos, int action, int port);
	/* a pointer event at the view point: the animations stop, a tap (2, 4), a cursor move
	   (29) or a touch (65..67, 71) goes to the page; 0 moves the page's pointer, 13 and 15
	   hide the loading plane, 25 shows it (name ours) */
	void Touch(const math::v2 &pos, uint16_t type, uint8_t port);
	/* the view focus for pos at the scale, moved into the view above the IME (name ours) */
	math::v2 GetFittedFocus(const math::v2 &pos, float scale);
	/* the page's text field zoomed into the view above the IME: fitted to it up to 1.75 and
	   the page's largest scale, where the page may be scaled (name ours) */
	void ZoomToIme();
	/* the page's block of the size given zoomed to fill the view, at the scale the view
	   allows, centred at pos (name ours) */
	void ZoomToRect(const math::v2 &pos, const math::v2 &size);
	/* the select popup's items given to the application (name ours) */
	void ShowSelect(const vector<string> &items);
	/* one of the select popup's items received; the popup shown with the last (name ours) */
	void AddSelectItem(const string &text, bool enabled, bool label);
	/* the select popup started: the count of its items to come (name ours) */
	void StartSelect(int count, int reserved, bool multiple);
	/* the view plane sized to the page's content at its scale, its pinch kept within the
	   scales the page allows, and the view put back on the page (name ours) */
	void UpdateContentSize();
	/* the page asks for the URL to be downloaded, with its MIME type (name ours) */
	void RequestDownload(const char *url, const char *mimeType);
	/* the page's event 0x2022: told to the application's listener (name ours) */
	__attribute__((noinline)) void OnEvent22();
	/* the loading plane put over the page's rect (its centre and size in a v4) and shown
	   when asked or when there is a first text; the texts choose its style (names ours) */
	void UpdateLoadingPlane(const math::v4 &rect, bool show, string first, string second);
	/* what a press on the page hit: the loading plane put over it, then the element zoomed
	   to (WEB_VIEW_HIT_ZOOM) or its context menu given to the listener
	   (WEB_VIEW_HIT_CONTEXT_MENU) (name ours) */
	void OnHit(const WebViewHitResult *hit, int action, bool show, const string &linkUrl,
		const string &imageUrl, bool contextMenuShown);
	/* the key sent to the page, or queued while the page has not replied to the last one
	   (name ours) */
	void SendKey(int keycode, bool down);
	/* the cursor's texture for WebCore's cursor type, hidden for none (name ours) */
	void SetCursorType(int type);
	/* the origin's permission, once the location says whether it is known (0x8100c74e;
	   name ours) */
	void CheckGeolocationPermission();
	/* the string of message 0x1023 sent to the page (export B1A078CD; name ours) */
	void SendString1023(const string &value);
	/* the view focus kept where a zoom takes it while the view plane grows to its scale
	   (the plane's UpdateSize calls it; name ours) */
	void UpdateZoomFocus();
	/* the file the user chose for the page's file field, its path and name, sent to it; the
	   cursor shows again (export B8CFECAE; name ours) */
	void SetChosenFile(const common::String &path, const common::String &name);
	/* an export that does nothing but return 0 (name ours) */
	int Reserved3C88A96E();
	/* the page's part the view shows, in the page's whole pixels: x, y, width, height
	   (name ours) */
	math::v4 GetVisiblePageRect();
	/* the cursor plane's point (its hot spot), from the view's focus with offset (name ours) */
	math::v2 GetCursorPos(bool offset);
	/* sends the cursor's point on the page, kept in the page's visible part (name ours) */
	void SendCursorPos();
	/* the cursor shown (with the focus let go) or faded out (name ours) */
	void SetCursorMode(int mode);
	/* the cursor's textures and its hot spot; the cursor plane takes the arrow's size (name
	   ours) */
	int SetCursorTextures(const intrusive_ptr<graph::Surface> &arrow,
			      const intrusive_ptr<graph::Surface> &arrowAlt,
			      const intrusive_ptr<graph::Surface> &hand,
			      const intrusive_ptr<graph::Surface> &handAlt, const math::v2 &offset);

	/* the surface the page is drawn into, the view's size, and its render target */
	int CreateSurface();

	/* the page made for the view at its size, with WebKit's view of the page made before
	   (posted) or a new one (sent, while there is none) */
	void CreatePageForView();
	int CreatePage();
	/* the web view listener told of the view, and its page made if it has none */
	void Start();

	/* the part's widget dropped (WEB_VIEW_PART_ALL: every one), destroyed when it was given
	   to the view */
	void RemovePart(int part);
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

	/* the analog sticks' input, from its analog listener: the left stick moves the cursor,
	   faster after a second at its edge; the right stick held keeps the view from scrolling */
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
	virtual void OnLoadFailed(int error, const common::String &url);
	virtual void OnLoadCommitted();
	virtual void OnPageLayout();
	virtual void OnLoadProgress(int progress);
	virtual void OnTitleChanged(const common::String &title);
	virtual void OnUrlChanged(const Url &url);
	virtual void OnEvent8(int arg);
	virtual void UpdatePageRect(const math::v2 &focus);
	/* the page opens a window at the URL: the listener makes it (name ours) */
	virtual void OnOpenWindow(const Url &url);
	virtual void OnEvent11(int arg0, int arg1);
	virtual void OnEvent12(const Url &url);
	virtual void OnNavigationRequest(const Url &url, int type, bool *allow);
	/* the page asks to download the URL: told to the listener (names ours) */
	virtual void OnDownloadRequest(const Url &url, const string &mimeType, int64_t length);
	virtual void OnEvent15(int arg);
	virtual void OnEvent16();
	virtual void OnHistoryChanged(bool canGoBack, bool canGoForward);
	virtual void OnEvent18(int arg0, int arg1);
	virtual void OnEvent19(int arg0, int arg1);
	/* a key the page did not take, given to the listener as a cursor direction (name ours) */
	virtual void OnEvent20(int keycode, int down);

	/* WebViewDialogClient */
	virtual void OnMessageDialogResult(int result);
	virtual void OnDialogResult(int result);
	virtual void OnPageEvent0()
	{
	}
	virtual void OnInputDialogResult(int result, int reopen);
	virtual void OnAuthDialogResult(int result);
	virtual void OnGeolocationDialogResult(int result);
	virtual void OnPageEvent4()
	{
	}

	/* WebViewInputClient */
	virtual void OnImeUpdatePreedit(const string &text, int caret);
	virtual void OnImeCommit(const string &text, int caret);
	virtual void OnImeSetText(const string &text);
	virtual void OnImeBackspace();
	virtual void OnImeDelete();
	virtual void OnImeMoveCaret(int dx, int dy);
	virtual void OnImeClear();
	virtual void OnImePressEnter();
	virtual void OnImeOpen(int height);
	virtual void OnImePressClose();

	/* WebViewGeolocationClient */
	virtual void OnPosition(WebIpcGeolocationPosition position);

	/* the receive buffer of the page's data */
	class Buffer {
	public:
		Buffer(int size) : m_size(size), m_length(0), m_data(new char[size])
		{
		}

		virtual ~Buffer();

		int m_size;
		int m_length;
		char *m_data;
	};

	Buffer *m_receiveBuffer;		/* 1100 */
	int m_viewId;				/* 1104: the page's ID with the WebKit process */
	int m_pageState;			/* 1108: WEB_VIEW_PAGE_* */
	WebViewPageListener *m_listener;	/* 1112 */
	WebViewLoadingPlane *m_loadingPlane;	/* 1116 */
	WebViewDataListener *m_appListener;	/* 1120: wutil's listener */
	/* the messages kept until the view's update reads them */
	vector<WebIpcMessage *> m_ipcData;	/* 1124 */
	int m_reserved1140;
	thread::RMutex m_ipcDataLock;		/* 1144 */
	intrusive_ptr<graph::Surface> m_surface;	/* 1176 */
	graph::SurfacePool *m_surfacePool;	/* 1180 */
	/* the page's pixels the surface is copied from (name ours) */
	const void *m_surfaceData;		/* 1184 */
	WebViewIme *m_ime;			/* 1188: the IME the page opened */
	WebViewRenderTarget *m_renderTarget;	/* 1192 */
	/* whether a page may ask for the device's location */
	bool m_geolocationEnabled;		/* 1196 */
	int m_cursorType;			/* 1200: WebCore's cursor type */
	WebViewGeolocation *m_geolocation;	/* 1204 */
	/* the origin asking for the location, while its permission is decided */
	string m_geolocationOrigin;		/* 1208 */
	/* the origins' answers kept */
	set<WebViewGeolocationPermission> m_geolocationPermissions;	/* 1220 */
	int m_loadProgress;			/* 1232 */
	int m_loadState;			/* 1236 */
	bool m_canReload;			/* 1240 */
	bool m_canGoBack;			/* 1241 */
	bool m_canGoForward;			/* 1242 */
	Url m_url;				/* 1244 */
	common::String m_title;			/* 1324 */
	int m_imeType;				/* 1348 */
	string m_imeText;			/* 1352 */
	/* the view's options (bit 0: a page's new window opens without asking; names ours) */
	int m_options;				/* 1364 */
	bool m_imeOpen;				/* 1368 */
	/* the page is laid out for the first time: its rect starts at 0 (name ours) */
	bool m_firstLayout;			/* 1369 */
	math::v2 m_contentSize;		/* 1376 */
	float m_scale;				/* 1384 */
	float m_maxScale;			/* 1388 */
	float m_minScale;			/* 1392 */
	bool m_userScalable;			/* 1396 */
	/* the scale that fits the page's layout width into the view (name ours) */
	float m_fitScale;			/* 1400 */
	/* what the page has told of itself, WEB_VIEW_PAGE_* (name ours) */
	int m_pageInfo;				/* 1404 */
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
	/* the message dialog the page asked for, opened through wutil's listener */
	MessageDialogParam m_dialogParam;	/* 1604 */
	/* the page's dialog requests waiting while another dialog is open */
	vector<WebIpcMessage *> m_pendingDialogs;	/* 1704 */
	int m_selectCount;			/* 1720 */
	int m_selectReceived;			/* 1724: the items received so far */
	vector<string> m_selectItems;		/* 1728 */
	vector<bool> m_selectEnabled;		/* 1744 */
	vector<bool> m_selectLabels;		/* 1760: the item is a group's label */
	bool m_selectMultiple;			/* 1776 */
	bool m_startWithParam;			/* 1777 */
	/* the rectangle of the text field the IME was opened for (name ours) */
	math::v4 m_imeRect;			/* 1784 */
	float m_tapScale;			/* 1800 */
	bool m_tapping;				/* 1804 */
	/* the page takes the view's input (name ours) */
	bool m_pageHandlesInput;		/* 1805 */
	int m_inputMask;			/* 1808 */
	math::v2 m_dragStart;			/* 1816 */
	math::v2 m_cursorPos[8];		/* 1824 */
	math::v2 m_cursorMove;			/* 1888 */
	bool m_cursorEnabled;			/* 1896 */
	common::SharedPtr<inputdevice::InputListener> m_keyListener;	/* 1900 */
	common::SharedPtr<inputdevice::InputListener> m_analogListener;	/* 1908 */
	int m_reserved1916;
	/* when the left stick reached its edge, for the cursor's speed (name ours) */
	SceUInt64 m_analogEdgeTime;		/* 1920 */
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
	/* where ZoomTo's scale moves the view focus, from the focus (name ours) */
	math::v2 m_zoomOffset;
	WebViewLoadRequest m_loadRequest;	/* 2016 */
	list<WebViewKeyEvent> m_keyQueue;	/* 2052: the keys waiting for the last one's reply */
	bool m_keyReady;			/* 2064: no key waits for a reply */
	WebViewKeyEvent m_lastKey;		/* 2068 */
	list<WebViewPart> m_parts;		/* 2080 */
};

}
}

#endif
