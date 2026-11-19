/*
 * The accessibility settings (/CONFIG/ACCESSIBILITY): inverted colours and the button remapping
 * applied whenever the registry changes (the callback SceShellAccessibilityMgrReg), the closed
 * caption settings moved from the Settings application's settings.ini into the registry once,
 * and the zoom: PS and square (or circle while zoomed) magnify the screen 960/704 times around the
 * viewport's centre, the directions and the front touch panel move it. The thread name is
 * Sony's; the file's, the classes' and the functions' names are ours.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/stdio.h>
#include <std/string.h>
#include <framework.h>
#include <plugin.h>
#include <timer/timer.h>
#include <system/system.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <graphics/surface/surface_display.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/inputlistener.h>
#include <inputdevice/pad/pad.h>
#include <inputdevice/touchscreen/touchscreen.h>
#include <common/shared_ptr.h>
#include <kernel/threadmgr.h>
#include <regmgr/regmgr.h>
#include <display/display.h>
#include <appmgr/appmgr.h>
#include <vshbridge/vshbridge.h>
#include <ctrl/ctrl_remap.h>
#include <ini_file_processor/ini_file_processor.h>
#include "shell.h"
#include "plugins.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "src/livespace_db/lsdb_keys.h"

using namespace paf;

/* sent to every widget once the system parameters were read again (value only) */
#define ACCESSIBILITY_EVENT_SYSTEM_PARAM_CHANGED	0x2002

/* the zoom's magnification, and its viewport */
#define ZOOM_SCALE		(960.0f / 704.0f)
#define ZOOM_SCREEN_WIDTH	960
#define ZOOM_SCREEN_HEIGHT	544
#define ZOOM_VIEWPORT_WIDTH	704
#define ZOOM_VIEWPORT_HEIGHT	399
/* a step of the directions, and the grid the viewport's offset snaps to */
#define ZOOM_STEP_X		64
#define ZOOM_STEP_Y		49

/* the devices the zoom holds while it is on: all but the BD remote */
#define ZOOM_DEVICES	(inputdevice::DEVICE_PAD | inputdevice::DEVICE_TOUCHSCREEN_FRONT |	\
			 inputdevice::DEVICE_TOUCHSCREEN_BACK | inputdevice::DEVICE_MOTIONSENSOR |	\
			 inputdevice::DEVICE_HIDRM | inputdevice::DEVICE_HIDKEYBOARD)

/* the front panel's resolution, 1920 by 1088 */
#define ZOOM_TOUCH_SCALE_X	(960.0f / 1919.0f)
#define ZOOM_TOUCH_SCALE_Y	(544.0f / 1087.0f)

/* a stick pushed this far from its centre counts as a direction */
#define ZOOM_STICK_THRESHOLD	79

/* the key repeat of the directions: its first delay, then its interval, faster after ten */
#define ZOOM_REPEAT_DELAY	300.0f
#define ZOOM_REPEAT_INTERVAL	80.0f
#define ZOOM_REPEAT_FAST	40.0f
#define ZOOM_REPEAT_FAST_AFTER	10.0f

/* livespace_db's type of the applications the zoom is held off for (value only) */
#define ZOOM_LSDB_TYPE_HELD	3

class AccessibilityMgr;
class ZoomMgr;

__attribute__((noinline)) static void accessibilityRegistryChanged(void);
static void zoomOnAppEvent(int code, const void *data, SceSize size, void *arg);

static AccessibilityMgr *s_accessibilityMgr = NULL;

/* the closed caption settings, as the registry keeps them (-1 in enable_cc: not set yet) */
static RegMgrKeyInfo s_closedCaptionKeys[12] = {
	{ sizeof(RegMgrKeyInfo), "enable_cc", 0, 4, { 0 } },
	{ sizeof(RegMgrKeyInfo), "content_specific", 0, 4, { 0 } },
	{ sizeof(RegMgrKeyInfo), "char_color", 0, 4, { 0 } },
	{ sizeof(RegMgrKeyInfo), "char_opacity", 0, 4, { 0 } },
	{ sizeof(RegMgrKeyInfo), "char_size", 0, 4, { 0 } },
	{ sizeof(RegMgrKeyInfo), "font_type", 0, 4, { 0 } },
	{ sizeof(RegMgrKeyInfo), "char_edge", 0, 4, { 0 } },
	{ sizeof(RegMgrKeyInfo), "char_edge_color", 0, 4, { 0 } },
	{ sizeof(RegMgrKeyInfo), "char_bg_color", 0, 4, { 0 } },
	{ sizeof(RegMgrKeyInfo), "char_bg_opacity", 0, 4, { 0 } },
	{ sizeof(RegMgrKeyInfo), "window_color", 0, 4, { 0 } },
	{ sizeof(RegMgrKeyInfo), "window_opacity", 0, 4, { 0 } },
};

static ZoomMgr *s_zoomMgr = NULL;

/* the viewport last set: none at first */
static int s_viewportX = -1;
static int s_viewportY = -1;
static float s_viewportScale = -1.0f;

/* the registry callback, for as long as the shell runs */
class AccessibilityMgr {
public:
	AccessibilityMgr() : m_callbackId(-1)
	{
		m_callbackId = sceKernelCreateCallback("SceShellAccessibilityMgrReg", 0,
						       (SceKernelCallbackFunction)accessibilityOnRegistry,
						       NULL);
		if (m_callbackId >= 0 && sceRegMgrRegisterCallback(m_callbackId) >= 0 &&
		    sceRegMgrAddRegistryCallback(m_callbackId, "/CONFIG/ACCESSIBILITY") >= 0)
			sceRegMgrStartCallback(m_callbackId);
	}

	virtual ~AccessibilityMgr();

	static int accessibilityOnRegistry(int notifyId, int notifyCount, int notifyArg,
					   void *common);

	SceUID m_callbackId;
};

/* made once (0x810A7616) */
int shellAccessibilityMgrCreate(void)
{
	int ret = SHELL_ERROR_ALREADY_CREATED;

	if (s_accessibilityMgr == NULL) {
		ret = SHELL_ERROR_NO_MEMORY;
		s_accessibilityMgr = new AccessibilityMgr();
		if (s_accessibilityMgr != NULL)
			ret = 0;
	}
	return ret;
}

/* the colours inverted or not, and the buttons remapped as the settings say (0x810A76AA) */
void shellAccessibilityApply(void)
{
	int value;
	bool invert = false;
	SceUInt32 merge = 2;
	const struct ctrlButtonRemappingInfo *remapping = NULL;
	struct ctrlButtonRemappingInfo info;

	if (sceRegMgrGetKeyInt("/CONFIG/ACCESSIBILITY", "invert_color", &value) == 0 &&
	    value == 1) {
		merge = 0;
		invert = true;
	}
	sceAppMgrSetDisplayMergeConf(merge, 256);
	vshDisplaySetInvertColors(0, invert);
	vshDisplaySetInvertColors(1, invert);
	if (sceRegMgrGetKeyInt("/CONFIG/ACCESSIBILITY", "keyremap_enable", &value) == 0 &&
	    value == 1 &&
	    sceRegMgrGetKeyBin("/CONFIG/ACCESSIBILITY", "keyremap_data", &info,
			       sizeof(info)) == 0 &&
	    (info.buttons[0] != 0 || info.buttons[1] != 0))
		remapping = &info;
	sceCtrlSetButtonRemappingInfo(remapping);
}

/*
 * The closed caption settings, while the registry has none yet (enable_cc -1), taken out of the
 * Settings application's ur0:user/<account>/savedata/NPXS10010/settings.ini, which keeps them
 * no longer, and written to /CONFIG/ACCESSIBILITY/CC (0x810A7760).
 */
void shellClosedCaptionMigrate(void)
{
	int account;
	RegMgrKeyInfo keys[12];
	char value[16];
	char path[64];

	memcpy(keys, s_closedCaptionKeys, sizeof(keys));
	if (sceRegMgrGetKeys("/CONFIG/ACCESSIBILITY/CC", keys, 12) != 0 || keys[0].value != -1)
		return;
	keys[0].value = 0;
	if (sceRegMgrGetKeyInt("/CONFIG/NP2", "current_account2", &account) != 0)
		account = 0;
	else if (account < 0 || account > 99)
		goto out;

	{
		IniFileProcessor *ini = new IniFileProcessor();

		if (ini != NULL) {
			IniConfig config;

			if (ini->initialize(&config) == 0) {
				sce_paf_snprintf(path, sizeof(path),
						 "ur0:user/%02d/savedata/NPXS10010/settings.ini",
						 account);
				if (ini->open(path, "rw", 0) == 0) {
					const char *names[12] = {
						"ClosedCaption", "CCSpecified", "CCCharColor",
						"CCCharOpacity", "CCCharSize", "CCCharFont",
						"CCCharEdge", "CCCharEdgeColor", "CCCharBgColor",
						"CCCharBgOpacity", "CCWindowColor", "CCWindowOpacity",
					};

					for (SceUInt32 i = 0; i < 12; i++) {
						if (ini->get(names[i], value, sizeof(value), 0) == 0) {
							ini->remove(names[i], 0);
							keys[i].value = sce_paf_atoi(value);
						}
					}
					ini->close();
				}
				ini->terminate();
			}
			delete ini;
		}
	}
out:
	sceRegMgrSetKeys("/CONFIG/ACCESSIBILITY/CC", keys, 12);
}

/* (0x810A78A4, deleting 0x810A78C8) */
AccessibilityMgr::~AccessibilityMgr()
{
	if (m_callbackId >= 0) {
		sceRegMgrUnregisterCallback(m_callbackId);
		sceKernelDeleteCallback(m_callbackId);
	}
}

/*
 * The registry changed: the settings applied, the zoom turned on or off, and, when the system
 * parameters read again changed, every widget told (0x810A78F0).
 */
int AccessibilityMgr::accessibilityOnRegistry(int notifyId, int notifyCount, int notifyArg,
					      void *common)
{
	shellAccessibilityApply();
	accessibilityRegistryChanged();
	Framework::Instance();
	if (ScePafToplevel_072EE507()) {
		ui::Event event(ACCESSIBILITY_EVENT_SYSTEM_PARAM_CHANGED, ui::Event::MODE_BROADCAST,
				ui::Event::ROUTE_TERM, 0);

		Framework::Instance()->GetEnvironmentInstance()->m_ev_queue.PushBack(event);
	}
	return 0;
}

/* the power_manage_plugin callback the zoom adds while it holds the system: nothing */
inline void zoomOnPower(void *arg)
{
}

/*
 * The zoom: its scale (1, or 960/704 when zoomed in) and the viewport's offset from the centre, how
 * many hold the input devices for it (the PS button, the zoom), and how many hold it off.
 */
class ZoomMgr {
public:
	ZoomMgr();
	virtual ~ZoomMgr();

	void GetViewportOffset(int *x, int *y);
	void ApplyViewport(bool toSurface);
	void Reset();
	void ZoomIn();
	void ZoomOut();
	void Toggle();
	void Move(int dx, int dy);
	void Step(int dx, int dy);

	/* the listener added to the input devices, or taken off them */
	void SetEnabled(bool enable)
	{
		if (enable) {
			inputdevice::AddInputListener(m_listener);
			SceShell_83D16CA8(SHELL_APP_EVENT_DEACTIVATED, zoomOnAppEvent, NULL);
			SceShell_83D16CA8(SHELL_APP_EVENT_ACTIVATED, zoomOnAppEvent, NULL);
		} else {
			inputdevice::DelInputListener(m_listener);
			SceShell_E3D3A162(SHELL_APP_EVENT_DEACTIVATED, zoomOnAppEvent, NULL);
			SceShell_E3D3A162(SHELL_APP_EVENT_ACTIVATED, zoomOnAppEvent, NULL);
		}
	}

	/* as the zoom setting says */
	void Load()
	{
		int zoom;

		if (sceRegMgrGetKeyInt("/CONFIG/ACCESSIBILITY", "zoom", &zoom) == 0)
			SetEnabled(zoom == 1);
	}

	/*
	 * The input devices taken for the zoom by the first holder; one that holds the system
	 * (hold) also keeps power_manage_plugin from it, once.
	 */
	void Lock(bool hold)
	{
		m_lockCount++;

		ShellPluginInterface *power = (ShellPluginInterface *)
			Plugin::Find("power_manage_plugin")->GetInterface(1);

		if (hold && power != NULL && !m_powerHeld) {
			power->func[POWER_MANAGE_PLUGIN_FUNC_GAME_HOLD]();
			((int (*)(void (*)(void *), void *))
			 power->func[POWER_MANAGE_PLUGIN_FUNC_ADD_CALLBACK])(zoomOnPower, this);
			m_powerHeld = true;
		}
		if (m_lockCount == 1) {
			if (!inputdevice::IsOwnershipGotten(ZOOM_DEVICES, 0))
				inputdevice::SetImposeMode(1);
			inputdevice::GetOwnership(ZOOM_DEVICES);
		}
	}

	/* the input devices, and the system, given back by the last holder */
	void Unlock()
	{
		if (m_lockCount > 0) {
			m_lockCount--;
			if (m_lockCount == 0) {
				ShellPluginInterface *power = (ShellPluginInterface *)
					Plugin::Find("power_manage_plugin")->GetInterface(1);

				if (power != NULL && m_powerHeld) {
					power->func[POWER_MANAGE_PLUGIN_FUNC_GAME_RELEASE]();
					((int (*)(void (*)(void *), void *))
					 power->func[POWER_MANAGE_PLUGIN_FUNC_REMOVE_CALLBACK])(
						zoomOnPower, this);
					m_powerHeld = false;
				}
				inputdevice::ReleaseOwnershipImmediate(ZOOM_DEVICES);
				inputdevice::SetImposeMode(0);
			}
		}
	}

	/* the zoom held off (and ended), and let back */
	void Disable()
	{
		m_disableCount++;
		Reset();
	}

	void Enable()
	{
		if (m_disableCount > 0)
			m_disableCount--;
	}

	/* the viewport's offset kept inside the screen */
	void Clamp()
	{
		int maxX = (int)((ZOOM_SCREEN_WIDTH - ZOOM_SCREEN_WIDTH / m_scale) * 0.5f);
		int maxY = (int)((ZOOM_SCREEN_HEIGHT - ZOOM_SCREEN_HEIGHT / m_scale) * 0.5f);

		if (m_x < -maxX)
			m_x = -maxX;
		if (m_x > maxX)
			m_x = maxX;
		if (m_y < -maxY)
			m_y = -maxY;
		if (m_y > maxY)
			m_y = maxY;
	}

	common::SharedPtr<inputdevice::InputListener> m_listener;
	bool m_reserved;		/* written false by the constructor, read nowhere */
	float m_scale;
	int m_x;
	int m_y;
	int m_lockCount;
	bool m_powerHeld;
	int m_disableCount;
};

/*
 * The zoom's input: PS with square toggles it; while zoomed, circle toggles it too, PS
 * released alone ends it, the directions (or a stick) move the viewport a step, repeated while
 * held, and a finger on the front panel drags it.
 */
class ZoomInputListener : public inputdevice::InputListener {
public:
	ZoomInputListener(ZoomMgr *owner)
		: InputListener(inputdevice::DEVICE_TYPE_PAD), m_owner(owner), m_toggleHeld(false),
		  m_cancelHeld(false), m_repeatDelay(ZOOM_REPEAT_DELAY),
		  m_repeatInterval(ZOOM_REPEAT_INTERVAL), m_repeatCount(0), m_repeatKey(0),
		  m_useStick(true), m_prevButtons(0)
	{
	}

	/* the direction a stick points to, if it is pushed far enough */
	static uint32_t StickDirection(const uint16_t &x, const uint16_t &y)
	{
		int dx = sce_paf_abs(x - inputdevice::pad::Data::STICK_CENTER);
		int dy = sce_paf_abs(y - inputdevice::pad::Data::STICK_CENTER);
		uint32_t direction = 0;

		if (dx >= ZOOM_STICK_THRESHOLD) {
			if (x < inputdevice::pad::Data::STICK_CENTER && dx > dy)
				direction = inputdevice::pad::Data::PAD_LEFT;
			if (x > inputdevice::pad::Data::STICK_CENTER && dx > dy)
				direction |= inputdevice::pad::Data::PAD_RIGHT;
		}
		if (dy >= ZOOM_STICK_THRESHOLD) {
			if (y < inputdevice::pad::Data::STICK_CENTER && dx < dy)
				direction |= inputdevice::pad::Data::PAD_UP;
			if (y > inputdevice::pad::Data::STICK_CENTER && dx < dy)
				direction |= inputdevice::pad::Data::PAD_DOWN;
		}
		return direction;
	}

	/* the viewport moved a step the way the key says, when zoomed */
	void StepByKey(uint32_t key)
	{
		if (m_owner->m_scale != 1.0f) {
			if (key == inputdevice::pad::Data::PAD_UP)
				m_owner->Step(0, 1);
			if (key == inputdevice::pad::Data::PAD_DOWN)
				m_owner->Step(0, -1);
			if (key == inputdevice::pad::Data::PAD_LEFT)
				m_owner->Step(-1, 0);
			if (key == inputdevice::pad::Data::PAD_RIGHT)
				m_owner->Step(1, 0);
		}
	}

	virtual void OnUpdate(const inputdevice::Data *data)
	{
		const inputdevice::pad::Data *pad = data->m_pad_data;
		uint32_t buttons = pad->buttons & ~inputdevice::pad::Data::PAD_KEY_OPEN;
		uint32_t previous = m_prevButtons |
			(data->m_pad_data_pre->buttons & ~inputdevice::pad::Data::PAD_KEY_OPEN);

		if (m_useStick) {
			if ((pad->buttons & inputdevice::pad::Data::PAD_DIRECTION) == 0)
				buttons |= StickDirection(pad->lx, pad->ly);
			if ((buttons & inputdevice::pad::Data::PAD_DIRECTION) == 0)
				buttons |= StickDirection(data->m_pad_data->rx,
							  data->m_pad_data->ry);
		}

		uint32_t pressed = buttons & ~previous;
		uint32_t released = previous & ~buttons;

		m_prevButtons = buttons;
		if (pressed != 0) {
			uint32_t key = 1;

			while ((pressed & key) == 0)
				key <<= 1;
			m_repeatKey = key;
			StepByKey(key);
			m_repeatDelay = ZOOM_REPEAT_DELAY;
			m_repeatInterval = ZOOM_REPEAT_INTERVAL;
			m_repeatCount = 0;
		}

		uint32_t key = m_repeatKey;

		if (released != 0 && (released & key) != 0) {
			m_repeatKey = 0;
		} else if (key != 0) {
			float elapsed = Timer::s_tick_unit * Timer::s_tick_interval;

			if (m_repeatDelay > 0.0f) {
				m_repeatDelay -= elapsed * 1000.0f;
				if (m_repeatDelay <= 0.0f) {
					StepByKey(key);
					m_repeatCount++;
				}
			} else {
				m_repeatInterval -= elapsed * 1000.0f;
				if (m_repeatInterval <= 0.0f) {
					StepByKey(key);
					m_repeatCount++;
					if ((float)m_repeatCount < ZOOM_REPEAT_FAST_AFTER)
						m_repeatInterval = ZOOM_REPEAT_INTERVAL;
					else
						m_repeatInterval = ZOOM_REPEAT_FAST;
				}
			}
		}

		/* PS pressed: the devices held while it is */
		if ((data->m_pad_data_pre->buttons & inputdevice::pad::Data::PAD_PS) == 0 &&
		    (data->m_pad_data->buttons & inputdevice::pad::Data::PAD_PS) != 0)
			m_owner->Lock(false);

		uint32_t now = data->m_pad_data->buttons;
		uint32_t before = data->m_pad_data_pre->buttons;

		if (((now & inputdevice::pad::Data::PAD_PS) != 0 &&
		     (before & inputdevice::pad::Data::PAD_DISPLAY) == 0 &&
		     (now & inputdevice::pad::Data::PAD_DISPLAY) != 0) ||
		    ((before & inputdevice::pad::Data::PAD_PS) == 0 &&
		     (now & inputdevice::pad::Data::PAD_PS) != 0 &&
		     (now & inputdevice::pad::Data::PAD_DISPLAY) != 0)) {
			if (!m_toggleHeld) {
				m_owner->Lock(true);
				m_toggleHeld = true;
			}
			m_owner->Toggle();
		}

		if (m_owner->m_scale != 1.0f) {
			if ((data->m_pad_data_pre->buttons & inputdevice::pad::Data::PAD_ESCAPE) == 0 &&
			    (data->m_pad_data->buttons & inputdevice::pad::Data::PAD_ESCAPE) != 0) {
				m_owner->Lock(true);
				m_cancelHeld = true;
				m_owner->Toggle();
			}
			if ((data->m_pad_data_pre->buttons & inputdevice::pad::Data::PAD_PS) != 0 &&
			    (data->m_pad_data->buttons & inputdevice::pad::Data::PAD_PS) == 0 &&
			    !m_toggleHeld)
				m_owner->Toggle();

			const inputdevice::touchscreen::Data *touch = data->m_touch_data;

			if (touch->state == inputdevice::touchscreen::Data::STATE_MOVE) {
				const inputdevice::touchscreen::Data *touchPre = data->m_touch_data_pre;
				int dx = (int)((float)(touchPre->center_orig_x - touch->center_orig_x) *
					       ZOOM_TOUCH_SCALE_X);
				int dy = (int)((float)(touchPre->center_orig_y - touch->center_orig_y) *
					       ZOOM_TOUCH_SCALE_Y);

				m_owner->Move((int)(dx * 1.5f), (int)(dy * 1.5f));
			}
		}

		/* PS released: the devices given back, square's hold too when it is up */
		if ((data->m_pad_data_pre->buttons & inputdevice::pad::Data::PAD_PS) != 0 &&
		    (data->m_pad_data->buttons & inputdevice::pad::Data::PAD_PS) == 0) {
			if (m_toggleHeld &&
			    (data->m_pad_data->buttons & inputdevice::pad::Data::PAD_DISPLAY) == 0) {
				m_owner->Unlock();
				m_toggleHeld = false;
			}
			m_owner->Unlock();
		}
		if ((data->m_pad_data_pre->buttons & inputdevice::pad::Data::PAD_DISPLAY) != 0 &&
		    (data->m_pad_data->buttons & inputdevice::pad::Data::PAD_DISPLAY) == 0 &&
		    m_toggleHeld &&
		    (data->m_pad_data->buttons & inputdevice::pad::Data::PAD_PS) == 0) {
			m_owner->Unlock();
			m_toggleHeld = false;
		}
		if ((data->m_pad_data_pre->buttons & inputdevice::pad::Data::PAD_ESCAPE) != 0 &&
		    (data->m_pad_data->buttons & inputdevice::pad::Data::PAD_ESCAPE) == 0 &&
		    m_cancelHeld) {
			m_owner->Unlock();
			m_cancelHeld = false;
		}
	}

	ZoomMgr *m_owner;
	bool m_toggleHeld;		/* PS and square toggled the zoom */
	bool m_cancelHeld;		/* circle toggled it */
	float m_repeatDelay;		/* milliseconds to the first repeat */
	float m_repeatInterval;		/* milliseconds to the next */
	uint32_t m_repeatCount;
	uint32_t m_repeatKey;		/* the direction repeated, or 0 */
	bool m_useStick;		/* the sticks count as the directions */
	uint32_t m_prevButtons;
};

/*
 * The viewport's offset from the centre as the screen shows it, x to the right and y up: the
 * left edge snapped to the step's grid of 64 pixels, the top to 49 lines (0x810A795C).
 */
void ZoomMgr::GetViewportOffset(int *x, int *y)
{
	float scale = m_scale;
	float marginY = ZOOM_SCREEN_HEIGHT - ZOOM_SCREEN_HEIGHT / scale;
	float marginX = ZOOM_SCREEN_WIDTH - ZOOM_SCREEN_WIDTH / scale;
	int top = (int)(marginY * 0.5f - m_y);
	int left = (int)(m_x + marginX * 0.5f);

	top = (top + ZOOM_STEP_Y / 2) / ZOOM_STEP_Y * ZOOM_STEP_Y;
	left = (left + ZOOM_STEP_X / 2) / ZOOM_STEP_X * ZOOM_STEP_X;
	*x = (int)(left - marginX * 0.5f);
	*y = (int)(-top + marginY * 0.5f);
}

/*
 * The viewport set where the zoom is, on both planes of the LCD (and the HDMI head of a
 * PlayStation TV), or on the root surface's display (toSurface), unless it is there already
 * (0x810A7A10).
 */
void ZoomMgr::ApplyViewport(bool toSurface)
{
	int x;
	int y;
	SceDisplayViewportConf conf;

	if (s_viewportX == m_x && s_viewportY == m_y && s_viewportScale == m_scale)
		return;
	s_viewportX = m_x;
	s_viewportY = m_y;
	s_viewportScale = m_scale;
	GetViewportOffset(&x, &y);
	x = (int)(x + (ZOOM_SCREEN_WIDTH - ZOOM_SCREEN_WIDTH / m_scale) * 0.5f);
	y = (int)((ZOOM_SCREEN_HEIGHT - ZOOM_SCREEN_HEIGHT / m_scale) * 0.5f - y);
	conf.size = sizeof(conf);
	conf.x = x;
	conf.y = y;
	conf.width = m_scale == 1.0f ? ZOOM_SCREEN_WIDTH : ZOOM_VIEWPORT_WIDTH;
	conf.height = m_scale == 1.0f ? ZOOM_SCREEN_HEIGHT : ZOOM_VIEWPORT_HEIGHT;
	if (toSurface) {
		if (ui::Screen::s_root_surface != NULL)
			ui::Screen::s_root_surface->SetDisplayViewport(y, x, conf.width,
								       conf.height);
	} else {
		vshDisplaySetViewportConf(0, 0, &conf);
		vshDisplaySetViewportConf(0, 1, &conf);
		if (system::Model::GetType() == 1) {
			vshDisplaySetViewportConf(1, 0, &conf);
			vshDisplaySetViewportConf(1, 1, &conf);
		}
	}
}

/* the zoom ended at once, without impose_plugin's transition (0x810A7B84) */
void ZoomMgr::Reset()
{
	if (m_scale != 1.0f) {
		m_x = 0;
		m_scale = 1.0f;
		m_y = 0;
		ApplyViewport(false);

		ImposePluginInterface *impose = (ImposePluginInterface *)
			Plugin::Find("impose_plugin")->GetInterface(1);

		if (impose != NULL)
			impose->zoomEnd();
		Unlock();
	}
}

/* (0x810A7C14) */
ZoomMgr::ZoomMgr()
	: m_listener(new ZoomInputListener(this)), m_reserved(false), m_scale(1.0f), m_x(0),
	  m_y(0), m_lockCount(0), m_powerHeld(false), m_disableCount(0)
{
	Load();
}

/* made once, when the shell boots (0x810A7D5A) */
int shellZoomMgrCreate(void)
{
	int ret = SHELL_ERROR_ALREADY_CREATED;

	if (s_zoomMgr == NULL) {
		ret = SHELL_ERROR_NO_MEMORY;
		s_zoomMgr = new ZoomMgr();
		if (s_zoomMgr != NULL)
			ret = 0;
	}
	return ret;
}

/* the zoom setting read again (0x810A7D9C) */
static void accessibilityRegistryChanged(void)
{
	ZoomMgr *zoom = s_zoomMgr;

	if (zoom != NULL)
		zoom->Load();
}

/* (0x810A7E32, deleting 0x810A7EBA) */
ZoomMgr::~ZoomMgr()
{
	SetEnabled(false);
}

/* zoomed in, around the centre (0x810A7ECC) */
void ZoomMgr::ZoomIn()
{
	if (m_scale == 1.0f) {
		ImposePluginInterface *impose = (ImposePluginInterface *)
			Plugin::Find("impose_plugin")->GetInterface(1);

		if (impose != NULL) {
			int x;
			int y;

			Lock(true);
			impose->zoomBegin();
			impose->zoomPrepare();
			m_scale = ZOOM_SCALE;
			m_x = 0;
			m_y = 0;
			x = 0;
			y = 0;
			GetViewportOffset(&x, &y);
			impose->zoomShow(x, y);
			ApplyViewport(true);
		}
	}
}

/* zoomed out (0x810A7FA8) */
void ZoomMgr::ZoomOut()
{
	if (m_scale != 1.0f) {
		ImposePluginInterface *impose = (ImposePluginInterface *)
			Plugin::Find("impose_plugin")->GetInterface(1);

		if (impose != NULL) {
			int x;
			int y;

			impose->zoomPrepare();
			x = m_x;
			y = m_y;
			GetViewportOffset(&x, &y);
			m_x = 0;
			m_y = 0;
			m_scale = 1.0f;
			ApplyViewport(true);
			impose->zoomHide(x, y);
			impose->zoomEnd();
			Unlock();
		}
	}
}

/*
 * Zoomed in or out; refused while it is held off, or while a PlayStation TV's HDMI output has
 * 1080 lines (0x810A8074).
 */
void ZoomMgr::Toggle()
{
	SceDisplayResolutionInfo info;
	int head;

	if (m_disableCount <= 0 &&
	    ((head = sceDisplayGetPrimaryHead()) != 1 ||
	     (info.size = sizeof(info), sceDisplayGetResolutionInfoInternal(head, &info) != 0) ||
	     info.height < 1080)) {
		ImposePluginInterface *impose = (ImposePluginInterface *)
			Plugin::Find("impose_plugin")->GetInterface(1);

		if (impose == NULL || impose->zoomIsBlocked() == 0) {
			if (m_scale != 1.0f)
				ZoomOut();
			else
				ZoomIn();
		}
	} else {
		ImposePluginInterface *impose = (ImposePluginInterface *)
			Plugin::Find("impose_plugin")->GetInterface(1);

		if (impose != NULL)
			impose->zoomRefuse();
	}
}

/* the viewport moved by pixels (0x810A8118) */
void ZoomMgr::Move(int dx, int dy)
{
	m_x += dx;
	m_y += dy;
	Clamp();
	ApplyViewport(false);
}

/*
 * The viewport moved a step; when the display did not follow (its viewport moved less than two
 * pixels), a second step (0x810A819A).
 */
void ZoomMgr::Step(int dx, int dy)
{
	int stepX = dx * ZOOM_STEP_X;
	int stepY = dy * ZOOM_STEP_Y;
	SceDisplayViewportConf before;

	m_x += stepX;
	m_y += stepY;
	Clamp();
	before.size = sizeof(before);
	before.x = 0;
	before.y = 0;
	before.width = 0;
	before.height = 0;
	vshDisplayGetActualViewportConf(0, 0, &before);
	ApplyViewport(false);
	if (before.width != 0 && before.height != 0) {
		SceDisplayViewportConf after;

		after.size = sizeof(after);
		after.x = 0;
		after.y = 0;
		after.width = 0;
		after.height = 0;
		vshDisplayGetActualViewportConf(0, 0, &after);
		if (sce_paf_abs(after.x - before.x) < 2 && sce_paf_abs(after.y - before.y) < 2) {
			m_x += stepX;
			m_y += stepY;
			Clamp();
			ApplyViewport(false);
		}
	}
}

/* the zoom held off, and ended (0x810A832A) */
void shellZoomDisable(void)
{
	ZoomMgr *zoom = s_zoomMgr;

	if (zoom != NULL)
		zoom->Disable();
}

/* the zoom let back (0x810A8344) */
void shellZoomEnable(void)
{
	ZoomMgr *zoom = s_zoomMgr;

	if (zoom != NULL)
		zoom->Enable();
}

/* the zoom ended (0x810A835C) */
void shellZoomReset(void)
{
	ZoomMgr *zoom = s_zoomMgr;

	if (zoom != NULL) {
		zoom->Disable();
		zoom->Enable();
	}
}

/* an application of livespace_db's type 3 activated or deactivated: the zoom held off or let
   back (0x810A8382) */
static void zoomOnAppEvent(int code, const void *data, SceSize size, void *arg)
{
	const ShellAppEvent *event = (const ShellAppEvent *)data;
	LsdbAppInfoPtr app;
	ShellPluginInterface *topmenu = (ShellPluginInterface *)
		Plugin::Find("topmenu_plugin")->GetInterface(1);

	if (topmenu != NULL)
		((void (*)(LsdbAppInfoPtr *, const char *))
		 topmenu->func[TOPMENU_PLUGIN_FUNC_GET_APP_INFO])(&app, event->titleId);
	if (app.get() == NULL)
		return;
	if (sceLsdbGetType(&app) != ZOOM_LSDB_TYPE_HELD)
		return;

	ZoomMgr *zoom = s_zoomMgr;

	if (zoom == NULL)
		return;
	switch (code) {
	case SHELL_APP_EVENT_ACTIVATED:
		zoom->Disable();
		break;
	case SHELL_APP_EVENT_DEACTIVATED:
		zoom->Enable();
		break;
	}
}
