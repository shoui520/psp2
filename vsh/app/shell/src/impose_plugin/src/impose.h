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
#include <common/shared_ptr.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>
#include <graphics/drawobj/draw_obj.h>
#include <cache/cache_image.h>
#include "../../../plugins.h"

/* the plugin's interface and the plugin itself (impose.cpp) */
void imposeInterfaceRegister(paf::Plugin *plugin);
extern ImposePluginInterface *g_imposeInterface;
extern paf::Plugin *g_imposePlugin;

/* PLACEHOLDER: an empty function the plugin's exit calls (the shell's empty function) */
void imposeExitReserved(void);

/* (0x811450A4, 76 bytes) */
class ImposePart811450A4 {
public:
	ImposePart811450A4();
	~ImposePart811450A4();
	void Impose_811451A4();
	void Impose_8114592E();

	SceUInt8 data[76];
};

/* the cache manager ("impose_plugin_CacheManager"; 0x81148260, 36 bytes) */
class ImposePart81148260 {
public:
	ImposePart81148260(paf::Plugin *plugin);
	~ImposePart81148260();
	void Impose_811497A0(ImposeNotice *notice);
	void Impose_811497DA();
	void Impose_811499A6();
	void Impose_811499CE();

	SceUInt8 data[36];
};

/* (0x81149A7C, 16 bytes) */
class ImposePart81149A7C {
public:
	ImposePart81149A7C(paf::Plugin *plugin);
	~ImposePart81149A7C();
	void Impose_81149AAC();

	SceUInt8 data[16];
};

/* (0x8115013C, 16 bytes) */
class ImposePart8115013C {
public:
	ImposePart8115013C(paf::Plugin *plugin);
	~ImposePart8115013C();
	void Impose_81150164();
	void Impose_811501D0();
	void Impose_811503CE(int arg);

	SceUInt8 data[16];
};

/* (0x81150574, 120 bytes) */
class ImposePart81150574 {
public:
	ImposePart81150574(paf::Plugin *plugin);
	~ImposePart81150574();
	void Impose_81150630();
	void Impose_811508FE();
	void Impose_81151192();

	SceUInt8 data[120];
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

/* (0x8114B0DC, 188 bytes) */
class ImposePart8114B0DC {
public:
	ImposePart8114B0DC(paf::Plugin *plugin);
	~ImposePart8114B0DC();
	void Impose_8114C460();
	void Impose_8114AA16();
	int Impose_81135260();
	void Impose_8114F232(bool arg);
	void Impose_8114DDBE(bool arg);

	SceUInt8 data[188];
};

/* (0x8114F348, 56 bytes) */
class ImposePart8114F348 {
public:
	ImposePart8114F348(paf::Plugin *plugin);
	~ImposePart8114F348();
	void Impose_8114F4BE();
	void Impose_8114FAC4();

	SceUInt8 data[56];
};

/* (0x8114FD14, 16 bytes) */
class ImposePart8114FD14 {
public:
	ImposePart8114FD14(paf::Plugin *plugin);
	~ImposePart8114FD14();
	void Impose_8114FD50(int arg1, int arg2);

	SceUInt8 data[16];
};

/* (0x8115000C, 4 bytes) */
class ImposePart8115000C {
public:
	ImposePart8115000C(paf::Plugin *plugin);
	~ImposePart8115000C();
	void Impose_8115002C();

	paf::Plugin *plugin;
};

/* (0x811516E4, 8 bytes) */
class ImposePart811516E4 {
public:
	ImposePart811516E4();
	~ImposePart811516E4();
	void Impose_811518CA(int arg);
	void Impose_8115171C();

	SceUInt8 data[8];
};

/* the accessibility zoom (0x81151B08, 16 bytes) */
class ImposePart81151B08 {
public:
	ImposePart81151B08();
	~ImposePart81151B08();
	void Impose_81151DC0();
	void Impose_81152072();
	void Impose_81152130();
	void Impose_811521DC(int x, int y);
	void Impose_81152468(int x, int y);
	int Impose_81152718();

	SceUInt8 data[16];
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
	ImposePart811450A4 m_part811450A4;
	ImposePart81148260 m_cacheManager;
	ImposePart81149A7C m_part81149A7C;
	ImposePart8115013C m_part8115013C;
	ImposePart81150574 m_part81150574;
	ImposeParentalControl m_parentalControl;
	ImposePart8114B0DC m_part8114B0DC;
	ImposePart8114F348 m_part8114F348;
	ImposePart8114FD14 m_part8114FD14;
	ImposePart8115000C m_part8115000C;
	ImposePart811516E4 m_part811516E4;
	ImposePart81151B08 m_zoom;
	ImposeTouchEmulation m_touchEmulation;
};

/* the cache manager (0x81564D68) */
extern ImposePart81148260 *g_imposePart81148260;

/* PLACEHOLDER: a shader the shell keeps (0x8139303C, in the shell's own code) */
const paf::common::SharedPtr<paf::graph::Shader> &Shell_8139303C(void);

/* the plugin's object (impose.cpp) */
extern Impose *g_impose;

#endif
