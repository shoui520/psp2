/*
 * impose.h: impose_plugin (the quick menu and the notices over every screen), as the shell
 * builds it into itself. The plugin is one object of 688 bytes holding its parts; each part's
 * class is defined by the file that holds its code. The folder's, the files' and all names here
 * are ours (PLACEHOLDER); the parts are named by their constructors' addresses until their
 * files are written.
 */
#ifndef IMPOSE_H
#define IMPOSE_H

#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/memory>
#include <std/string>
#include <common/shared_ptr.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/inputlistener.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <widget/w_plane.h>
#include <widget/w_plane_button.h>
#include <graphics/drawobj/draw_obj.h>
#include <cache/cache_image.h>
#include <cache/cache_manager.h>
#include <std/vector>
#include <versatileparam/idparam.h>
#include <job/job.h>
#include <thread/thread.h>
#include <widget/w_button_base.h>
#include <widget/w_slidebar.h>
#include <widget/w_check_box.h>
#include <widget/w_text.h>
#include <widget/w_composite_drawer.h>
#include <rtc/rtc.h>
#include "../../livespace_db/livespace_db_internal.h"
#include "impose_shell_event.h"
#include "../../../plugins.h"

/* the events of a slide bar's up and down keys, which no header names (PLACEHOLDER names) */
#define PSMENU_EVENT_KEY_UP		0x10001
#define PSMENU_EVENT_KEY_DOWN		0x10003

/* the child, when it is of the type */
#define PSMENU_FIND(parent, type, id)							\
	((parent)->FindChild(paf::IDParam(id), 0) != NULL &&					\
	 (parent)->FindChild(paf::IDParam(id), 0)->IsInherit(type::TypeName()) ?		\
	 (type *)(parent)->FindChild(paf::IDParam(id), 0) : NULL)

/* PLACEHOLDER names: listeners calling a method of a part of the menu, with an argument or without */
template<class T>
class ImposePsMenuListener : public paf::ui::EventListener {
public:
	ImposePsMenuListener(T *obj, void (T::*func)()) : m_obj(obj), m_func(func)
	{
	}

	virtual int32_t Do(int32_t type, paf::ui::Handler *self, paf::ui::Event *e)
	{
		if (m_killed)
			return 0x80AF4101;
		(m_obj->*m_func)();
		return 0;
	}

	T *m_obj;
	void (T::*m_func)();
};

template<class T, class A>
class ImposePsMenuArgListener : public paf::ui::EventListener {
public:
	ImposePsMenuArgListener(T *obj, void (T::*func)(A), A arg)
		: m_obj(obj), m_func(func), m_arg(arg)
	{
	}

	virtual int32_t Do(int32_t type, paf::ui::Handler *self, paf::ui::Event *e)
	{
		if (m_killed)
			return 0x80AF4101;
		(m_obj->*m_func)(m_arg);
		return 0;
	}

	T *m_obj;
	void (T::*m_func)(A);
	A m_arg;
};

/* the plugin's interface and the plugin itself (impose.cpp) */
void imposeInterfaceRegister(paf::Plugin *plugin);
extern ImposePluginInterface *g_imposeInterface;
extern paf::Plugin *g_imposePlugin;

/* PLACEHOLDER: an empty function the plugin's exit calls (the shell's empty function) */
void imposeExitReserved(void);

/*
 * PLACEHOLDER names: the volume bar shown while the volume keys are pressed; its methods are
 * named by their addresses until its file is written (0x81145B00, 40 bytes)
 */
class ImposeVolumeBar {
public:
	ImposeVolumeBar(paf::Plugin *plugin);
	~ImposeVolumeBar();
	void Reset();					/* 0x81145B34 */
	int Close();					/* 0x81145CB4: 0 if it was not shown */
	void Open(int mode);				/* 0x81145CDE */
	int IsOpen(int mode);				/* 0x811463B8 */
	void SetVolume(int volume, SceUInt32 flags);	/* 0x811463CC */
	void ShowBluetoothKey(int dir);			/* 0x81146974 */
	void ShowAvlsLimit(int avls);			/* 0x811470AC */
	void BlinkAvls(int on);				/* 0x81147706 */
	void ShowMute(bool bluetooth);			/* 0x811478C0 */
	void OnFade(SceInt32 type, paf::ui::Handler *self, paf::ui::Event *e,
		    void *data);			/* 0x81147C50 */
	void ShowLimit();				/* 0x81147DDE */

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_root;
	paf::ui::Widget *m_frame;
	paf::ui::Widget *m_bluetoothIcon;
	paf::ui::Widget *m_avlsIcon;
	paf::ui::Plane *m_gauge;		/* the volume's part of the gauge */
	paf::ui::Plane *m_gaugeRest;		/* the rest of it */
	SceInt32 m_shownVolume;
	bool m_active;
	bool m_avlsShown;
	SceInt32 m_state;
};

/* the volume control's flags for a press (PLACEHOLDER names) */
#define IMPOSE_VOLUME_HEADSET	0x1	/* the Bluetooth headset takes the keys */
#define IMPOSE_VOLUME_BLUETOOTH	0x2	/* the Bluetooth device's volume */
#define IMPOSE_VOLUME_AVLS	0x4	/* limited to IMPOSE_VOLUME_AVLS_MAX */
#define IMPOSE_VOLUME_FIXED	0x8	/* the volume does not change */

/*
 * PLACEHOLDER names: the volume control: the volume keys, repeated while held, the volume bar,
 * the AV configuration's callback (impose_volume.cpp; 0x811450A4, 76 bytes)
 */
class ImposeVolume {
public:
	ImposeVolume(paf::Plugin *plugin);
	~ImposeVolume();
	void Start();
	void VolumeDown(SceUInt32 flags);
	void VolumeUp(SceUInt32 flags);
	void Update(const paf::inputdevice::Data *data);
	bool IsFixed();

	paf::common::SharedPtr<paf::inputdevice::InputListener> m_listener;
	float m_repeatTime;		/* since the last step */
	float m_muteTime;		/* both keys held */
	bool m_firstRepeat;
	bool m_external;		/* the volume changed elsewhere: read again, not set */
	SceInt32 m_volume;
	SceUInt32 reserved;
	SceInt32 m_device;		/* the output's controls in the last frame */
	ImposeVolumeBar m_bar;
	bool m_avls;
	bool m_fixed;
};

/* the volume control and its bar (0x81564D3C, 0x81564D40) */
extern ImposeVolume *g_imposeVolume;
extern ImposeVolumeBar *g_imposeVolumeBar;

/* a notice waiting or shown, copied from its ImposeNotice (0x81148088, 116 bytes; PLACEHOLDER
   names, after ImposeNotice's) */
class ImposeNotificationItem {
public:
	ImposeNotificationItem(const ImposeNotice &notice);
	~ImposeNotificationItem();

	paf::string m_name;
	SceInt32 m_priority;
	SceInt32 m_reserved1;
	paf::string m_pluginName;
	paf::IDParam m_icon;
	paf::string m_iconPath;
	SceInt32 m_iconMode;
	paf::wstring m_message;
	paf::string m_reservedText;		/* never set or read */
	paf::IDParam m_matchId;			/* never set: compared with the shown one's to tell
						   a repeat */
	void (*m_callback)(int result, void *data);
	void *m_callbackData;
	SceUInt32 m_flags;
};

/*
 * the notices shown one at a time at the screen's corner, and the cache manager that loads their
 * icons ("impose_plugin_CacheManager"; 0x81148260, 36 bytes; PLACEHOLDER names, after its
 * "notificationPush" and "notificationCancel")
 */
class ImposeNotification {
public:
	ImposeNotification(paf::Plugin *plugin);
	~ImposeNotification();
	void ShowNext();
	void Close();
	void Push(ImposeNotificationItem *item);
	void Cancel(paf::string *name);
	void OnTimeout(void *data);
	int Show(ImposeNotice *notice);
	void Update();
	void MakeSpaceIfUsed();
	void MakeSpace();

	paf::Plugin *m_plugin;
	paf::vector<ImposeNotificationItem *> m_items;
	ImposeNotificationItem *m_current;
	paf::ui::Widget *m_widget;
	SceInt32 m_reserved;			/* zeroed, read by nothing */
	paf::cache::CacheManager *m_cache;
};

/* the screenshot: the flash shown while the shell takes it, and its errors told (PLACEHOLDER
   names; impose_screenshot.cpp) */
class ImposeScreenshot {
public:
	ImposeScreenshot(paf::Plugin *plugin);
	~ImposeScreenshot();
	void Open();
	void ShowResult(int error);
	void ShowCapturing();
	void HideCapturing();

	paf::Plugin *m_plugin;
	paf::ui::Scene *m_page;			/* the plugin's second page, once the flash is made */
	SceInt32 m_reserved;			/* -1, read by nothing */
	bool m_capturing;
};

extern ImposeScreenshot *g_imposeScreenshot;	/* (0x815448E8) */

struct ShellIncomingDialogRequest;
struct ShellPluginInterface;

/*
 * Sony's SceIncomingDialogMgr (its job's name and its messages): the shell's incoming dialog
 * requests taken one at a time and shown as a dialog over the home screen, or as an indicator
 * while an application is in front; its sound played meanwhile; the application launched when
 * it is accepted. Its member names are ours (PLACEHOLDER).
 */
class IncomingDialogMgr {
public:
	/* what a request is at */
	enum {
		STATE_STOPPED = 0,
		STATE_IDLE = 1,
		STATE_DIALOG = 2,
		STATE_INDICATOR = 3
	};

	IncomingDialogMgr(paf::Plugin *plugin);
	~IncomingDialogMgr();
	void Start();
	/* the request's sound stopped, and its new event written */
	void StopSound();
	void PostNoticeEvent();
	/* the request given back with its result */
	int Close(int result);
	int Stop();
	int OpenDialog();
	int ShowIndicator();
	void PlaySound();
	/* the dialog or the indicator, as the shell now is */
	void Update();
	/* each frame */
	void Tick();
	/* the launch's argument made */
	void PrepareLaunch();
	void OnDialogButton(SceInt32 id, SceInt32 button);
	void OnEvent(int code, const void *data, SceSize size);

	/* the indicator taken off, and the dialog closed */
	void HideIndicator();
	void CloseDialog();

	paf::Plugin *m_plugin;
	ShellPluginInterface *m_topmenu;
	SceInt32 m_state;
	ShellIncomingDialogRequest *m_request;
	SceInt32 m_dialog;
	bool m_inApp;			/* shell events 7 and 6 */
	bool m_onStartScreen;		/* shell events 2 and 3 */
	bool m_indicatorShown;
	bool m_soundPlaying;
	char m_launchTitleId[16];
	bool m_launchFlag;
	bool m_launchPending;
	paf::string m_launchArg;
	char m_mountPoint[64];		/* the application's folder, mounted for its sound */
};

/*
 * PLACEHOLDER names: the parental control's lock page, opened over everything while the
 * console is locked; it launches the parental control settings (NPXS10094) and tells of a
 * wrong passcode (impose_parental_control.cpp; 0x81141400, 8 bytes)
 */
class ImposeParentalControl {
public:
	ImposeParentalControl(paf::Plugin *plugin);
	~ImposeParentalControl();
	void SetIcon(const paf::intrusive_ptr<paf::cache::CacheImageInterface> &icon);
	void Open(int keepMessage);
	void Close();
	void ShowNotice(int count);

	paf::Plugin *m_plugin;
	paf::ui::Scene *m_page;
};

/* the parental control's lock page (0x81564BD8) */
extern ImposeParentalControl *g_imposeParentalControl;

/*
 * The flight mode switched as a job ("SceImposeFlightModeJob", Sony's name for it; its
 * callbacks "SceImposeBtDriverEventCallback", "SceImposeWlanDriverEventCallback"): the job
 * waits for the Wi-Fi and Bluetooth drivers, with the dialog shown meanwhile. Its code and its
 * other members are in the file after impose_psmenu_wireless.cpp (0x8115A13A-0x8115A9DE); the
 * class's and the members' names are ours (PLACEHOLDER).
 */
class ImposeFlightModeJob : public paf::job::JobItem {
public:
	ImposeFlightModeJob(paf::ui::Scene *scene, char on, SceInt32 dialog);
	virtual ~ImposeFlightModeJob();
	virtual int Run();
	virtual void Finish(int result);

	paf::ui::Scene *m_scene;
	char m_on;			/* '1' to turn the flight mode on, '0' off */
	SceInt32 m_dialog;
	SceUID m_btCallback;
	SceUID m_wlanCallback;
	paf::thread::Mutex m_mutex;
	/* PLACEHOLDER: an object of its own vtable (0x81530F64), read with the job's file */
	void *m_waiter;
	paf::thread::Cond m_cond;
	SceInt32 m_state;
};

/* PLACEHOLDER names: the job's functions the menu's switches call, in the job's file: */
bool Impose_8115AA1E(bool on);		/* the flight mode set */
bool Impose_8115AA80(void);		/* Wi-Fi cannot be switched (its configuration's 0x200) */
bool Impose_8115AA94(bool on);		/* Wi-Fi set */
bool Impose_8115AB10(void);		/* the mobile network is there (telephony_plugin) */
bool Impose_8115AB46(void);		/* the mobile network cannot be switched (0x400) */
bool Impose_8115AB5A(bool on);		/* the mobile network set */
bool Impose_8115AC0C(bool on);		/* Bluetooth set */
bool Impose_8115AC7C(void);		/* the flight mode is on */
bool Impose_8115ACC2(void);		/* Wi-Fi is on */
bool Impose_8115ACD6(void);		/* the mobile network is on */
bool Impose_8115AD9C(void);		/* Bluetooth is on */
bool Impose_8115ADB0(void);		/* the switches can be used */
void Impose_8115A080(bool on);		/* the menu's event flag told of Wi-Fi */

/*
 * The menu's wireless switches (impose_psmenu_wireless.cpp; PLACEHOLDER names): the flight
 * mode, Wi-Fi, the mobile network and Bluetooth, each a button whose icon and sound show its
 * state.
 */
class ImposePsMenuWireless {
public:
	ImposePsMenuWireless(paf::Plugin *plugin);
	~ImposePsMenuWireless();
	void SetButtonState(paf::ui::Widget *button, bool on);
	void UpdateButtons();
	void Layout(paf::ui::Widget *button, float width);
	void Open(paf::ui::Widget *box);
	void Close();
	void SetEnabled(bool enabled);
	void OnFlightMode(paf::ui::Widget *button);
	void OnWlan(paf::ui::Widget *button);
	void OnMobile(paf::ui::Widget *button);
	void OnBluetooth(paf::ui::Widget *button);
	void OnLayout(paf::ui::Widget *button);
	void Update();

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_box;
	paf::ui::ButtonBase *m_flightMode;
	paf::ui::ButtonBase *m_wlan;
	paf::ui::ButtonBase *m_mobile;
	paf::ui::ButtonBase *m_bluetooth;
	bool m_enabled;
};

/* the menu's controller settings: the vibration, the port's number and battery shown while a
   controller is on a port, the port given another number, the controller disconnected
   (impose_psmenu_controller.cpp; PLACEHOLDER names) */
class ImposePsMenuController {
public:
	ImposePsMenuController(paf::Plugin *plugin);
	~ImposePsMenuController();
	void Open(paf::ui::Widget *box);
	void Close();
	void StartBlink();
	void StopBlink();
	void UpdateBattery();
	void Update();
	void UpdatePortLabels();
	void Tick();
	void Blink(paf::ui::CompositeDrawer *battery);
	void Disconnect();
	void OpenAssign();
	void OnVibration(paf::ui::CheckBox *check);

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_box;
	int m_port;
	int m_count;		/* the ticks left to wait for a controller */
	SceUInt8 m_battery;
};

/* an icon applauncher has for the menu, set on the widget it was asked for (impose_psmenu.cpp;
   PLACEHOLDER name) */
void imposePsMenuOnIcon(const paf::intrusive_ptr<paf::cache::CacheImageInterface> &icon,
			void *data);

/* the menu's music player controls: the playing track's title, album and artist, the play,
   previous and next buttons (held to seek) and the volume, after the shell's music player
   (impose_psmenu_bgm.cpp; PLACEHOLDER names) */
class ImposePsMenuBgm {
public:
	ImposePsMenuBgm(paf::Plugin *plugin);
	~ImposePsMenuBgm();
	void Disable(bool disable);
	void UpdateApp();
	void Open(paf::ui::Widget *box);
	void Close();
	void SetPlaying(bool playing);
	void Update();
	void SetActive(bool active);
	void OnPlay(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *data);

	static void OnVolume(int32_t type, paf::ui::Handler *self, paf::ui::Event *e, void *data);

	/* the buttons' listeners */
	static void OnPlayDecide(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
				 void *data)
	{
		((ImposePsMenuBgm *)data)->OnPlay(type, self, e, data);
	}

	static void OnPrevDecide(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
				 void *data);
	static void OnNextDecide(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
				 void *data);
	static void OnPrevRepeat(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
				 void *data);
	static void OnNextRepeat(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
				 void *data);
	static void OnSeekEnd(int32_t type, paf::ui::Handler *self, paf::ui::Event *e,
			      void *data);

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_box;
	paf::ui::Text *m_title;
	paf::ui::Text *m_album;		/* the album and the artist */
	paf::ui::PlaneButton *m_play;
	paf::ui::PlaneButton *m_prev;
	paf::ui::PlaneButton *m_next;
	paf::ui::Plane *m_pauseIcon;
	paf::ui::Plane *m_playIcon;
	paf::ui::Plane *m_prevIcon;
	paf::ui::Plane *m_nextIcon;
	paf::ui::SlideBar *m_volume;
	bool m_active;
	SceUInt8 m_state;		/* the player's state shown */
	SceUInt16 m_seekCount;		/* the repeats of the button held */
	paf::string m_trackTitle;	/* the title shown */
};

/* the menu's volume slide bar: the master volume set as it moves and written to the registry by
   a job when it is let go (impose_psmenu_volume.cpp; PLACEHOLDER names) */
class ImposePsMenuVolume {
public:
	ImposePsMenuVolume(paf::Plugin *plugin);
	~ImposePsMenuVolume();
	void Open(paf::ui::Widget *box);
	void Save();
	void Close();
	void OnChange();
	void OnRelease();

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_box;
	paf::ui::SlideBar *m_slider;
	int m_volume;
};

/* the menu's accessibility settings: inverted colours and the remapped buttons, the latter
   only with the console's own pad (impose_psmenu_accessibility.cpp; PLACEHOLDER names) */
class ImposePsMenuAccessibility {
public:
	ImposePsMenuAccessibility(paf::Plugin *plugin);
	~ImposePsMenuAccessibility();
	void Open(paf::ui::Widget *box);
	void Close();
	void OnInvertColor(paf::ui::CheckBox *check);
	void OnKeyRemap(paf::ui::CheckBox *check);

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_box;
};

/*
 * PLACEHOLDER names: the jobs the menu puts on the default queue, Sony's names for them
 * ("ImposePsMenuCheckAppMenuReady", "ImposePsMenuOpenAppMenu"; 0x81159EB4, 0x81159DF0)
 */
class ImposePsMenuCheckAppMenuReady : public paf::job::JobItem {
public:
	ImposePsMenuCheckAppMenuReady();
	virtual int Run();
	virtual void Finish(int result);
};

class ImposePsMenuOpenAppMenu : public paf::job::JobItem {
public:
	ImposePsMenuOpenAppMenu();
	virtual int Run();
	virtual void Finish(int result);
};

/*
 * PLACEHOLDER names: the menu the PS button opens over an application ("ImposePsMenu", Sony's
 * job names): the application's menu, the brightness, the microphone and the voice chat, the
 * parental time left and the touch emulation (impose_psmenu.cpp; 0x8114B0DC, 188 bytes)
 */
class ImposePsMenu {
public:
	ImposePsMenu(paf::Plugin *plugin);
	~ImposePsMenu();
	void Close();
	void ShowQuitDialog();
	void SetAppMenuReady(bool ready);
	int CheckAppMenuReady();
	void UpdateAudioButtons();
	void SetFocusButton(paf::ui::Widget *button);
	bool GetApplication(LsdbAppInfoPtr *app);
	void UpdateParentalIcon();
	void UpdatePlayTime();
	void Open();
	bool IsOpen();
	void Impose_8114DDBE(bool arg);
	void ShowTouchEmulationHelp();
	void OnTouchEmulation(SceInt32 type, paf::ui::Handler *self, paf::ui::Event *e, void *data);
	void OnAppMenu(SceInt32 type, paf::ui::Handler *self, paf::ui::Event *e, void *data);
	void LayoutPrioritizeInfo(paf::ui::Widget *text);
	void ShowPrioritizeInfo();
	void Impose_8114F232(bool arg);
	void SetIcon(const paf::intrusive_ptr<paf::cache::CacheImageInterface> &icon,
		     paf::ui::Widget *widget);

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_page;
	paf::ui::SlideBar *m_brightness;
	paf::ui::CheckBox *m_micMute;
	paf::ui::CheckBox *m_voicePriority;
	SceInt32 m_brightnessValue;
	SceInt32 m_prioritizeDialog;
	SceInt32 m_helpDialog;
	SceInt32 m_quitDialog;
	paf::common::SharedPtr<paf::job::JobItem> m_job;
	bool m_checking;
	paf::ui::Widget *m_focusButton;
	ImposePsMenuWireless m_wireless;
	ImposePsMenuController m_controller;
	ImposePsMenuBgm m_bgm;
	ImposePsMenuVolume m_volume;
	ImposePsMenuAccessibility m_accessibility;
};

/* the menu (0x81564DC0) */
extern ImposePsMenu *g_imposePsMenu;

/* PLACEHOLDER names: the console is a PS TV (0x8114BB84); a text child laid out on lines to
   the width (0 keeps it) when the system asks for it (0x8114B894) */
bool imposeIsDolce(void);
void imposeLayoutText(paf::ui::Widget *parent, const paf::IDParam &id, float width);

/* the warning line of a development or testing kit not activated or expiring, and the
   manufacturing mode's (0x8114F348, 56 bytes; PLACEHOLDER names, after its notice's name
   "ActivationWarning") */
class ImposeActivationWarning {
public:
	ImposeActivationWarning(paf::Plugin *plugin);
	~ImposeActivationWarning();
	void SetMessage(const wchar_t *message, const paf::math::v4 &color);
	void Open();
	void Update();

	paf::Plugin *m_plugin;
	paf::ui::Widget *m_page;
	paf::ui::Text *m_text;		/* the line, made the first time */
	bool m_counting;		/* the activation's time left counted down */
	SceUInt64 m_timeLeft;		/* in seconds, as last shown */
	paf::rtc::Tick m_tick;		/* when it was last shown */
	SceUInt64 m_timeLimit;		/* the time left at the start, in seconds */
	paf::rtc::Tick m_startTick;	/* the start */
	SceUInt32 m_isTestingKit;	/* vshSblAimgrIsDEX(): a testing kit, else a development kit */
};

extern ImposeActivationWarning *g_imposeActivationWarning;

/* a black screen faded in over everything with a busy indicator, and a function called when
   it is black (0x8114FD14, 16 bytes; PLACEHOLDER names) */
class ImposeBusyScreen {
public:
	ImposeBusyScreen(paf::Plugin *plugin);
	~ImposeBusyScreen();
	int Show(void (*onShown)(void *data), void *data);

	paf::Plugin *m_plugin;
	paf::ui::Scene *m_scene;
	void (*m_onShown)(void *data);
	void *m_onShownData;
};

/* the notice that the zoom is not available here (0x8115000C, 4 bytes; PLACEHOLDER names,
   after the notice's name "ps_insensitive") */
class ImposeZoomRefuse {
public:
	ImposeZoomRefuse(paf::Plugin *plugin);
	~ImposeZoomRefuse();
	void ShowRefused();

	paf::Plugin *m_plugin;
};

/* (0x811516E4, 8 bytes) */
/* the overlay pages: one of three open at a time (impose_overlay.cpp, 8 bytes; PLACEHOLDER
   names) */
class ImposeOverlay {
public:
	ImposeOverlay();
	~ImposeOverlay();
	void Open(int kind);
	void Close();

	paf::ui::Scene *m_page;
	int m_kind;
};

/* the screen zoom: the page and the plane the capture is shown on (impose_zoom.cpp, 16 bytes;
   PLACEHOLDER names) */
class ImposeZoom {
public:
	ImposeZoom();
	~ImposeZoom();
	paf::ui::Scene *Acquire();
	void Release();
	void Begin();
	void End();
	void Prepare();
	void Finish();
	void Show(int x, int y);
	void Hide(int x, int y);
	int IsBlocked();

	paf::ui::Scene *m_page;
	paf::intrusive_ptr<paf::graph::Surface> m_surface;
	paf::ui::Widget *m_plane;
	int m_count;		/* its users */
};

/*
 * PLACEHOLDER names: a point the touch device reports, in the front panel's units (16 bytes);
 * the panel it touches: 0 the front, 1 the back (its place moved there), 2 both, 3 the front
 * (a touch pad's)
 */
struct ImposeTouchPoint {
	SceInt16 x;
	SceInt16 y;
	SceInt32 state;		/* 1 and 2: shown (2: touched) */
	SceInt32 panel;
	SceUInt8 frontId;	/* the touch's report IDs */
	SceUInt8 backId;
	SceUInt8 reserved[2];
};

/* PLACEHOLDER names: a panel's active area (16 bytes) */
struct ImposeTouchArea {
	SceInt32 minX;
	SceInt32 maxX;
	SceInt32 minY;
	SceInt32 maxY;
};

struct SceCtrlData2;
struct SceTouchData;

/*
 * PLACEHOLDER names: the touch device the controller drives: the points moved by the analog
 * sticks or set from a touch pad, given to the touch driver as the panels' touches
 * (impose_touch_device.cpp; 0x81144204, 104 bytes)
 */
class ImposeTouchDevice {
public:
	ImposeTouchDevice();
	virtual ~ImposeTouchDevice();
	void Reset();
	void SetTouchData(const ImposeTouchPoint *points, int count);
	SceInt16 MovePoint(int stick, float *speed, SceInt16 min, SceInt16 max, SceInt16 pos);
	void UpdateFromPad(const SceCtrlData2 *pad, ImposeTouchPoint *points, int count);
	void UpdateFromTouchPad(const SceTouchData *touch, bool touchFront, bool touchBack,
				ImposeTouchPoint *points, int count);
	void UpdateFromTouch(const SceTouchData *touch, bool touchFront, bool touchBack,
			     ImposeTouchPoint *points, int count);
	void UpdateState(const SceCtrlData2 *pad, const SceTouchData *touch);
	void Update(ImposeTouchPoint *points, int count);
	void Suspend();
	void Resume();
	int IsSuspended();
	void SetMode(int mode);

	SceInt32 m_state;		/* 0 off, 1 the left stick, 2 the right, 3 both, 4 a touch pad */
	ImposeTouchArea m_front;	/* the front panel's */
	ImposeTouchArea m_area;		/* where the front and the back panel meet */
	ImposeTouchArea m_frontPanel;
	ImposeTouchArea m_backPanel;
	SceInt32 m_reportId;		/* the last touch's report ID */
	SceUInt32 m_buttons;		/* the buttons of the previous sample */
	SceUInt32 m_touchButtons[2];	/* the buttons that touch with each point */
	SceUInt32 m_cancelButtons;	/* the button that turns the pointers off */
	SceInt32 m_suspendState;	/* 1 suspended, 2 resumed until the next sample */
	bool m_cancelHeld;		/* the cancel button held since the resume */
	bool m_touchHeld;		/* the first point's buttons held since the resume */
	SceInt32 m_mode;		/* 1: the sticks off, 1 and 2: the touch pad off */
};

class ImposeTouchPointer;

/*
 * PLACEHOLDER names: the touch emulation of the PS TV ("SceShellImposeTouchEmulation"), the
 * controller moving two pointers over the screen for the applications that need the touch
 * screen (impose_touch_emulation.cpp; 0x8114353E, 116 bytes)
 */
class ImposeTouchEmulation {
public:
	ImposeTouchEmulation(paf::Plugin *plugin);
	~ImposeTouchEmulation();
	void Start();
	void Stop();
	void Enable(bool reset);
	void Disable(bool reset);
	void Lock();
	void Unlock();
	void UpdatePointers();
	int Poll();

	paf::Plugin *m_plugin;
	paf::ui::Scene *m_root;
	ImposeTouchPointer *m_pointers[2];
	ImposeTouchPoint m_points[2];
	bool m_running;
	ImposeTouchArea m_front;
	ImposeTouchArea m_area;
	SceInt32 m_width;
	SceInt32 m_height;
	SceInt32 m_lockCount;
	bool m_isGame;
	bool m_needsTouch;
	SceUID m_thread;
	SceUID m_sema;
	ImposeTouchDevice *m_device;
	SceUInt8 reserved[4];
};

/* the plugin: its two pages and its parts (688 bytes) */
class Impose {
public:
	Impose(paf::Plugin *plugin);
	static void Create(paf::Plugin *plugin);
	~Impose();
	static void Destroy();
	static void Start();
	static void Stop();

	void OpenPages();

	paf::Plugin *m_plugin;
	paf::ui::Scene *m_page;
	paf::ui::Scene *m_page2;
	ImposeVolume m_volume;
	ImposeNotification m_notification;
	ImposeScreenshot m_screenshot;
	ImposeShellEvent m_shellEvent;
	IncomingDialogMgr m_incomingDialog;
	ImposeParentalControl m_parentalControl;
	ImposePsMenu m_psMenu;
	ImposeActivationWarning m_activationWarning;
	ImposeBusyScreen m_busyScreen;
	ImposeZoomRefuse m_zoomRefuse;
	ImposeOverlay m_overlay;
	ImposeZoom m_zoom;
	ImposeTouchEmulation m_touchEmulation;
};

/* the notices (0x81564D68) */
extern ImposeNotification *g_imposeNotification;

/* the notice queued for the main thread: the interface's showNotice (0x81149918) */
int imposeNotificationShowNotice(ImposeNotice *notice);
extern ImposeTouchEmulation *g_imposeTouchEmulation;

/* PLACEHOLDER: a shader the shell keeps (0x8139303C, in the shell's own code) */
const paf::common::SharedPtr<paf::graph::Shader> &Shell_8139303C(void);

/* the plugin's object (impose.cpp) */
extern Impose *g_impose;

#endif
