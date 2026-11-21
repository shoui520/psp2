/*
 * impose_touch_emulation.cpp: impose_plugin's touch emulation of the PS TV and the pointers it
 * shows (0x81141E44-0x81144204). The file's and the names are ours (PLACEHOLDER) but the
 * thread's ("SceShellImposeTouchEmulation").
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/math.h>
#include <versatileparam/idparam.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/core/environment.h>
#include <widget/w_plane.h>
#include <widget/w_scene.h>
#include <inputdevice/inputdevice.h>
#include <kernel/threadmgr.h>
#include <regmgr/regmgr.h>
#include <display.h>
#include <ctrl.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include "../../livespace_db/livespace_db_internal.h"
#include "../../livespace_db/lsdb_keys.h"
#include "../../applauncher_plugin/src/applauncher.h"
#include "../../../shell.h"
#include "../../../plugins.h"
#include "impose.h"

using namespace paf;

/* PLACEHOLDER: the livespace_db type of a system application, and the attribute bit of an
   application the touch emulation leaves alone */
#define IMPOSE_LSDB_TYPE_SYSTEM		2
#define IMPOSE_LSDB_TYPE_SYSTEM2	3
#define IMPOSE_LSDB_ATTR_NO_EMULATION	0x10000000

/* the application parameter whose bit 0 asks for the touch emulation (appmgr's, name ours) */
#define IMPOSE_APP_PARAM_TOUCH		17

/*
 * PLACEHOLDER names: a pointer the controller moves: four planes, the ring around it (outer,
 * inner) and its icon (two), grown when the point is touched and shrunk when it is let go
 * (0x81530B14, 696 bytes)
 */
class ImposeTouchPointer : public ui::Plane {
public:
	ImposeTouchPointer(ui::Widget *parent, ui::Widget::CreateParam *param);
	virtual ~ImposeTouchPointer();
	virtual int32_t SetColor_core(const math::v4 &color);
	virtual void Update();

	void SetState(int state);
	void Expand();
	void Shrink();
	void Blink();

	float m_x;
	float m_y;
	float m_drawnX;
	float m_drawnY;
	ui::Widget *m_outer;
	ui::Widget *m_inner;
	ui::Widget *m_icon;
	ui::Widget *m_icon2;
	bool m_blink;
	int m_shape;		/* 0: grown, 1: shrunk */
	int m_state;		/* the icon shown, 4: none */
	int m_nextShape;
	int m_nextState;
	bool m_visible;
};

/* PLACEHOLDER names: the planes' sizes and places, by the state shown (0x81564C00-0x81564D10) */
static math::v4 s_iconSize(88.0f, 88.0f, 0.0f, 0.0f);
static math::v4 s_iconSizeLarge(96.0f, 96.0f, 0.0f, 0.0f);
static math::v4 s_icon2Size(26.0f, 46.0f, 0.0f, 0.0f);
static math::v4 s_outerSize(54.0f, 54.0f, 0.0f, 0.0f);
static math::v4 s_outerSizeGrown = s_outerSize;
static math::v4 s_innerSize(34.0f, 34.0f, 0.0f, 0.0f);
static math::v4 s_innerSizeShrunk = s_innerSize * 0.8f;
static math::v4 s_iconSizeSmall(28.0f, 28.0f, 0.0f, 0.0f);
static math::v4 s_outerSize3(74.0f, 74.0f, 0.0f, 0.0f);
static math::v4 s_outerSize3Grown = s_outerSize3;
static math::v4 s_innerSize3(42.0f, 42.0f, 0.0f, 0.0f);
static math::v4 s_innerSize3Grown = s_innerSize3 * 1.2f;
static math::v4 s_iconPos0(-20.0f, -28.0f, 0.0f, 0.0f);
static math::v4 s_iconPos1(-32.0f, -8.0f, 0.0f, 0.0f);
static math::v4 s_iconPos2(-14.0f, 27.0f, 0.0f, 0.0f);
static math::v4 s_icon2Offset(12.0f, -18.0f, 0.0f, 0.0f);
static math::v4 s_iconPos3(0.0f, 0.0f, 0.0f, 0.0f);
static math::v4 s_ringColor(1.0f, 168.0f / 255.0f, 0.0f, 1.0f);

/* PLACEHOLDER names: the icons, by the state shown */
#define IMPOSE_TOUCH_TEX_ICON1		0xDF534FC0
#define IMPOSE_TOUCH_TEX_ICON2		0x9D73BFA1
#define IMPOSE_TOUCH_TEX_ICON2_HELD	0x244EB722
#define IMPOSE_TOUCH_TEX_ICON2_SUB	0xC33C6D58
#define IMPOSE_TOUCH_TEX_ICON3		0x519A7FA6
#define IMPOSE_TOUCH_TEX_ICON0		0xA19A0F94
#define IMPOSE_TOUCH_TEX_INNER		0xE5F8590C
#define IMPOSE_TOUCH_TEX_INNER3		0x1927A571
#define IMPOSE_TOUCH_TEX_OUTER		0x61D86118
#define IMPOSE_TOUCH_TEX_OUTER3		0x9C3F565F

/* the icon of a state shown, and the ring's textures and sizes (0x81141E44) */
void ImposeTouchPointer::SetState(int state)
{
	if (m_state == state)
		return;
	m_state = state;
	m_icon2->SetRot(math::v4(0.0f, 0.0f, 0.0f, 0.0f), 0, NULL, ANIM_ROT, 0, 0.0f, NULL, NULL);
	if (m_show_alpha != 1.0f) {
		m_transition_complete = false;
		m_transition = 0xff;
		DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(this, NULL, 2, 0.0f, NULL);
	}
	if (state != 4)
		g_imposeNotification->MakeSpaceIfUsed();
	switch (state) {
	case 1:
		m_icon->SetTexture(g_imposePlugin->GetTexture(IDParam(IMPOSE_TOUCH_TEX_ICON1)));
		m_icon->SetPos(s_iconPos1);
		m_icon->SetSize(s_iconSize);
		m_icon->SetMetaAlpha(1.0f);
		m_icon2->SetTexture(intrusive_ptr<graph::Surface>());
		m_icon2->SetMetaAlpha(0.0f);
		break;
	case 2:
		m_icon->SetTexture(g_imposePlugin->GetTexture(IDParam(IMPOSE_TOUCH_TEX_ICON2)));
		m_icon->SetPos(s_iconPos2);
		m_icon->SetSize(s_iconSizeLarge);
		m_icon->SetMetaAlpha(1.0f);
		m_icon2->SetTexture(g_imposePlugin->GetTexture(IDParam(IMPOSE_TOUCH_TEX_ICON2_SUB)));
		m_icon2->SetPos(s_iconPos2 + s_icon2Offset);
		m_icon2->SetSize(s_icon2Size);
		m_icon2->SetMetaAlpha(0.0f);
		break;
	case 3:
		m_icon->SetTexture(intrusive_ptr<graph::Surface>());
		m_icon->SetMetaAlpha(0.0f);
		m_icon2->SetTexture(g_imposePlugin->GetTexture(IDParam(IMPOSE_TOUCH_TEX_ICON3)));
		m_icon2->SetPos(s_iconPos3);
		m_icon2->SetSize(s_iconSizeSmall);
		m_icon2->SetMetaAlpha(1.0f);
		break;
	case 4:
		m_transition_complete = false;
		m_transition = 0xff;
		DoTransitionReverse_core(0.0f, true);
		ScePafWidget_4588699E(this, NULL, 2, 0.0f, NULL);
		m_icon->SetTexture(intrusive_ptr<graph::Surface>());
		m_icon2->SetTexture(intrusive_ptr<graph::Surface>());
		m_inner->SetTexture(intrusive_ptr<graph::Surface>());
		m_outer->SetTexture(intrusive_ptr<graph::Surface>());
		return;
	default:
		m_icon->SetTexture(intrusive_ptr<graph::Surface>());
		m_icon->SetMetaAlpha(0.0f);
		m_icon2->SetTexture(g_imposePlugin->GetTexture(IDParam(IMPOSE_TOUCH_TEX_ICON0)));
		m_icon2->SetPos(s_iconPos0);
		m_icon2->SetSize(s_iconSize);
		m_icon2->SetMetaAlpha(1.0f);
		break;
	}

	bool three = m_state == 3;

	m_inner->SetTexture(g_imposePlugin->GetTexture(
		IDParam(three ? IMPOSE_TOUCH_TEX_INNER3 : IMPOSE_TOUCH_TEX_INNER)));
	m_outer->SetTexture(g_imposePlugin->GetTexture(
		IDParam(three ? IMPOSE_TOUCH_TEX_OUTER3 : IMPOSE_TOUCH_TEX_OUTER)));

	math::v4 innerShrunk = three ? s_innerSize3 : s_innerSizeShrunk;
	math::v4 innerGrown = three ? s_innerSize3Grown : s_innerSize;
	math::v4 outerShrunk = three ? s_outerSize3 : s_outerSize;
	math::v4 outerGrown = three ? s_outerSize3Grown : s_outerSizeGrown;

	m_inner->SetSize(m_shape == 0 ? innerGrown : innerShrunk);
	m_outer->SetSize(m_shape == 0 ? outerGrown : outerShrunk);
}

/* the ring grown: the point touched (0x811426EA) */
void ImposeTouchPointer::Expand()
{
	math::v4 innerShrunk = m_state == 3 ? s_innerSize3 : s_innerSizeShrunk;
	math::v4 innerGrown = m_state == 3 ? s_innerSize3Grown : s_innerSize;
	math::v4 outerShrunk = m_state == 3 ? s_outerSize3 : s_outerSize;
	math::v4 outerGrown = m_state == 3 ? s_outerSize3Grown : s_outerSizeGrown;

	m_inner->SetSize(innerShrunk);
	m_inner->SetMetaAlpha(0.5f);
	m_outer->SetSize(outerShrunk);
	m_outer->SetMetaAlpha(0.0f);
	m_inner->SetSize(innerGrown, new Timer(150.0f, Timer::FUNC_SLOWDOWN));
	m_inner->SetMetaAlpha(0.95f, new Timer(150.0f));
	m_outer->SetSize(outerGrown, new Timer(150.0f));
	m_outer->SetMetaAlpha(0.95f, new Timer(150.0f));
	if (m_state == 2) {
		m_icon->SetTexture(g_imposePlugin->GetTexture(IDParam(IMPOSE_TOUCH_TEX_ICON2_HELD)));
		m_icon2->SetMetaAlpha(1.0f);
	}
}

/* the ring shrunk: the point let go (0x81142AE8) */
void ImposeTouchPointer::Shrink()
{
	math::v4 innerShrunk = m_state == 3 ? s_innerSize3 : s_innerSizeShrunk;
	math::v4 innerGrown = m_state == 3 ? s_innerSize3Grown : s_innerSize;
	math::v4 outerShrunk = m_state == 3 ? s_outerSize3 : s_outerSize;
	math::v4 outerGrown = m_state == 3 ? s_outerSize3Grown : s_outerSizeGrown;

	m_inner->SetSize(innerGrown);
	m_inner->SetMetaAlpha(0.95f);
	m_outer->SetSize(outerGrown);
	m_outer->SetMetaAlpha(0.95f);
	m_inner->SetSize(innerShrunk, new Timer(150.0f, Timer::FUNC_SLOWDOWN));
	m_inner->SetMetaAlpha(0.5f, new Timer(150.0f));
	m_outer->SetSize(outerShrunk, new Timer(150.0f));
	m_outer->SetMetaAlpha(0.0f, new Timer(150.0f));
	if (m_state == 2) {
		m_icon->SetTexture(g_imposePlugin->GetTexture(IDParam(IMPOSE_TOUCH_TEX_ICON2)));
		m_icon2->SetMetaAlpha(0.0f);
	}
}

/* the pointer shown a while and faded out, while it does not move (0x81142EE0) */
void ImposeTouchPointer::Blink()
{
	if (m_blink) {
		SetMetaAlpha(1.0f, NULL, ANIM_META_ALPHA, 2);
		SetMetaAlpha(1.0f, new Timer(3000.0f));
		SetMetaAlpha(0.0f, new Timer(150.0f));
	}
}

/* (0x81142FFC) */
ImposeTouchPointer::ImposeTouchPointer(ui::Widget *parent, ui::Widget::CreateParam *param)
	: ui::Plane(parent, param), m_x(0.0f), m_y(0.0f), m_drawnX(0.0f), m_drawnY(0.0f),
	  m_blink(true), m_shape(2), m_state(4), m_nextShape(2), m_nextState(4), m_visible(false)
{
	m_hit_detect_mode = 2;
	m_hit_detect_b_mode = 2;

	ui::Widget *plane;

	plane = new ui::Plane(this, NULL);
	m_icon = plane;
	plane->m_hit_detect_mode = 2;
	plane->m_hit_detect_b_mode = 2;
	m_icon->SetSize(s_iconSize);
	m_icon->SetMetaAlpha(1.0f);

	plane = new ui::Plane(this, NULL);
	m_outer = plane;
	plane->m_hit_detect_mode = 2;
	plane->m_hit_detect_b_mode = 2;
	m_outer->SetSize(s_outerSize);
	m_outer->SetMetaAlpha(0.0f);

	plane = new ui::Plane(this, NULL);
	m_inner = plane;
	plane->m_hit_detect_mode = 2;
	plane->m_hit_detect_b_mode = 2;
	m_inner->SetSize(s_innerSizeShrunk);
	m_inner->SetMetaAlpha(0.5f);

	plane = new ui::Plane(this, NULL);
	m_icon2 = plane;
	plane->m_hit_detect_mode = 2;
	plane->m_hit_detect_b_mode = 2;
	m_icon2->SetSize(s_iconSize);
	m_icon2->SetMetaAlpha(1.0f);

	if (m_shape != 1) {
		m_shape = 1;
		Shrink();
	}
	m_nextShape = 1;
	SetState(4);
	ScePafWidget_66847A11(this, true);
}

/* (0x81143244) */
ImposeTouchPointer::~ImposeTouchPointer()
{
	m_outer = NULL;
	m_inner = NULL;
	m_icon = NULL;
	m_icon2 = NULL;
}

/* the ring in its own color, the icon in the color given (0x811432A6) */
int32_t ImposeTouchPointer::SetColor_core(const math::v4 &color)
{
	m_outer->SetColor(s_ringColor);
	m_inner->SetColor(s_ringColor);
	m_icon->SetColor(color);
	m_icon2->SetColor(color);
	return ui::Plane::SetColor_core(color);
}

/* each frame: moved to the point, shown or hidden, and its shape and icon changed (0x8114333A) */
void ImposeTouchPointer::Update()
{
	if (m_x != m_drawnX || m_y != m_drawnY) {
		SetPos(math::v4(m_x, m_y, 0.0f, 0.0f));
		m_drawnX = m_x;
		m_drawnY = m_y;
		if (m_nextState == m_state)
			Blink();
		ScePafWidget_66847A11(this, false);
	} else {
		ScePafWidget_66847A11(this, true);
	}
	if (m_visible) {
		if (m_show_alpha != 1.0f) {
			if (!m_transition_complete) {
				m_transition = 0xff;
				m_transition_complete = true;
				DoTransition_core(0.0f, true);
			}
			ScePafWidget_FF5BC957(this, NULL, 2, 0.0f, NULL);
		}
	} else if (m_show_alpha != 0.0f) {
		Timer *timer = new Timer(100.0f);
		float time = timer != NULL ? timer->GetTotalTime() : 0.0f;

		m_transition_complete = false;
		m_transition = 0xff;
		DoTransitionReverse_core(time, true);
		ScePafWidget_4588699E(this, timer, 2, 0.0f, NULL);
	}
	if (m_shape == m_nextShape) {
		if (m_shape == 0)
			Blink();
	} else {
		m_shape = m_nextShape;
		if (m_nextShape == 1)
			Shrink();
		else
			Expand();
	}
	SetState(m_nextState);
	ui::Plane::Update();
}

/* the touch emulation, for its thread and the application events (0x815448D0) */
ImposeTouchEmulation *g_imposeTouchEmulation;

static void imposeTouchEmulationOnAppEvent(int code, const void *data, SceSize size, void *arg);

/* (0x8114353E) */
ImposeTouchEmulation::ImposeTouchEmulation(Plugin *plugin)
	: m_plugin(plugin), m_root(NULL), m_pointers(), m_points(), m_running(false), m_front(),
	  m_area(), m_width(0), m_height(0), m_lockCount(0), m_isGame(false), m_needsTouch(false),
	  m_thread(-1), m_sema(-1), m_device(NULL)
{
	g_imposeTouchEmulation = this;
	SceShell_83D16CA8(SHELL_APP_EVENT_DEACTIVATED, imposeTouchEmulationOnAppEvent, NULL);
	SceShell_83D16CA8(SHELL_APP_EVENT_ACTIVATED, imposeTouchEmulationOnAppEvent, NULL);
	m_device = new ImposeTouchDevice();
}

/* (0x811435C8) */
ImposeTouchEmulation::~ImposeTouchEmulation()
{
	SceShell_E3D3A162(SHELL_APP_EVENT_DEACTIVATED, imposeTouchEmulationOnAppEvent, NULL);
	SceShell_E3D3A162(SHELL_APP_EVENT_ACTIVATED, imposeTouchEmulationOnAppEvent, NULL);
	delete m_device;
	g_imposeTouchEmulation = NULL;
}

static int imposeTouchEmulationThread(SceSize args, void *argp);

/* the widget hidden, over the timer's time */
static inline void imposeTouchHide(ui::Widget *widget, Timer *timer)
{
	float time = timer != NULL ? timer->GetTotalTime() : 0.0f;

	widget->m_transition_complete = false;
	widget->m_transition = 0xff;
	widget->DoTransitionReverse_core(time, true);
	ScePafWidget_4588699E(widget, timer, 2, 0.0f, NULL);
}

/* the pointers made over the plugin's second page, and the thread started (0x81143606) */
void ImposeTouchEmulation::Start()
{
	if (m_root != NULL)
		return;

	ui::Scene *root = g_impose != NULL ? g_impose->m_page2 : NULL;

	m_pointers[1] = new ImposeTouchPointer(root, NULL);
	m_pointers[1]->SetColor(math::v4(1.0f, 0.0f, 0.047f, 1.0f));
	m_pointers[1]->SetMetaAlpha(0.0f);
	m_pointers[1]->SetRot(math::v4(0.0f, 3.14159265f, 0.0f, 0.0f), 0, NULL,
			      ui::Widget::ANIM_ROT, 0, 0.0f, NULL, NULL);
	imposeTouchHide(m_pointers[1], NULL);

	m_pointers[0] = new ImposeTouchPointer(root, NULL);
	m_pointers[0]->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	m_pointers[0]->m_blink = false;
	m_pointers[0]->SetMetaAlpha(1.0f);
	imposeTouchHide(m_pointers[0], NULL);

	m_root = root;

	ui::SceneResolution *resolution = ScePafWidget_CDC90BA2(root->m_env_param,
								 root->m_resolution_id);

	m_front = m_device->m_front;
	m_area = m_device->m_area;
	m_width = resolution->m_width;
	m_height = resolution->m_height;
	for (int i = 0; i < 2; i++) {
		m_points[i].x = (m_front.maxX - m_front.minX) / 2;
		m_points[i].y = (m_front.maxY - m_front.minY) / 2;
		m_points[i].state = 0;
	}
	m_device->Reset();
	if (m_root != NULL) {
		m_sema = sceKernelCreateSema("SceShellImposeTouchEmulation", 0, 0, 1, NULL);
		m_thread = sceKernelCreateThread("SceShellImposeTouchEmulation",
						 imposeTouchEmulationThread, 0x100000EA, 0x1000, 0,
						 0, NULL);
		if (m_thread > 0 && sceKernelStartThread(m_thread, 0, NULL) < 0)
			sceKernelDeleteThread(m_thread);
	}
}

/* the semaphore deleted, the pointers deleted (0x811438CC) */
void ImposeTouchEmulation::Stop()
{
	if (m_root != NULL) {
		sceKernelDeleteSema(m_sema);
		m_thread = -1;
	}
	for (int i = 0; i < 2; i++) {
		delete m_pointers[i];
		m_pointers[i] = NULL;
	}
	m_root = NULL;
}

/* the emulation run if the setting, the application or both ask for it (0x81143902) */
void ImposeTouchEmulation::Enable(bool reset)
{
	if (m_root == NULL)
		return;

	int value = 0;
	int ret = sceRegMgrGetKeyInt("/CONFIG/SHELL", "touch_emulation", &value);
	bool enabled = ret == 0 && value == 1;

	if ((m_needsTouch || (enabled && m_isGame)) && m_lockCount == 0) {
		if (reset) {
			for (int i = 0; i < 2; i++) {
				m_points[i].x = (m_front.maxX - m_front.minX) / 2;
				m_points[i].y = (m_front.maxY - m_front.minY) / 2;
				m_points[i].state = 0;
			}
			m_device->Reset();
		}
		if (m_needsTouch) {
			if (m_isGame && enabled)
				m_device->SetMode(2);
			else
				m_device->SetMode(1);
		} else {
			m_device->SetMode(0);
		}
		if (!m_running) {
			sceKernelSignalSema(m_sema, 1);
			m_running = true;
		}
	}
}

/* the emulation stopped, the thread waited for (0x811439DA) */
void ImposeTouchEmulation::Disable(bool reset)
{
	if (m_root == NULL)
		return;
	if (reset) {
		for (int i = 0; i < 2; i++) {
			m_points[i].x = (m_front.maxX - m_front.minX) / 2;
			m_points[i].y = (m_front.maxY - m_front.minY) / 2;
			m_points[i].state = 0;
		}
		m_device->Reset();
		m_device->SetMode(0);
	}
	if (m_running) {
		SceUInt32 timeout = 1000000;

		sceKernelWaitSema(m_sema, 1, &timeout);
		m_running = false;
	}
}

/* the emulation held off, counted (0x81143A60) */
void ImposeTouchEmulation::Lock()
{
	if (m_root == NULL)
		return;
	if (m_lockCount < 32) {
		if (++m_lockCount == 1)
			Disable(false);
	} else {
		sce_paf_printf_dummy();
	}
}

/* and let back (0x81143A82) */
void ImposeTouchEmulation::Unlock()
{
	if (m_root == NULL)
		return;
	if (m_lockCount > 0) {
		if (--m_lockCount == 0)
			Enable(false);
	} else {
		sce_paf_printf_dummy();
	}
}

/* the pointers moved to the points, from the panel's units to the screen's (0x81143AA2) */
void ImposeTouchEmulation::UpdatePointers()
{
	for (int i = 0; i < 2; i++) {
		ImposeTouchPoint *point = &m_points[i];
		ImposeTouchPointer *pointer = m_pointers[i];

		if (point->state == 2 || point->state == 1) {
			float width = m_front.maxX - m_front.minX;
			float height = m_front.maxY - m_front.minY;
			float x = ((m_points[i].x - width * 0.5f) - m_front.minX) / width * m_width;
			float y = (m_front.minY - (m_points[i].y - height * 0.5f)) / height * m_height;

			pointer->m_x = sce_paf_floor(x + 0.5f);
			pointer->m_y = sce_paf_floor(y + 0.5f);

			int state = 0;

			if (point->panel == 2)
				state = 2;
			if (point->panel == 1)
				state = 1;
			m_pointers[i]->m_nextState = state;
			m_pointers[i]->m_nextShape = point->state != 2;
			m_pointers[i]->m_visible = true;
		} else {
			pointer->m_visible = false;
		}
	}
}

/* one frame of the thread: the points read unless the input is taken; 1 once it is to end
   (0x81143BD2) */
int ImposeTouchEmulation::Poll()
{
	int ret;

	if (sceKernelPollSema(m_sema, 1) < 0) {
		ImposeTouchDevice *device = m_device;

		m_pointers[0]->m_nextState = 4;
		m_pointers[0]->m_visible = false;
		m_pointers[1]->m_nextState = 4;
		m_pointers[1]->m_visible = false;
		device->Suspend();
		ret = sceKernelWaitSema(m_sema, 1, NULL);
		if (ret < 0)
			return ret;
		m_device->Resume();
	}
	ret = sceKernelSignalSema(m_sema, 1);
	if (ret < 0)
		return ret;
	ret = sceDisplayWaitVblankStart();
	if (ret < 0)
		return ret;

	bool remapped = false;
	SceAppMgrAppStatus status;

	if (sceAppMgrGetStatusByAppId(SHELL_APP_ID_FRONT, &status) == 0) {
		int ctrlStatus = 0;

		if (sceCtrlGetProcessStatus(status.processId, &ctrlStatus) == 0)
			remapped = ctrlStatus == 1;
	}

	bool taken = remapped || inputdevice::IsOwnershipGotten(0x6F, 0);

	if (taken) {
		if (m_device->IsSuspended() == 0) {
			ImposeTouchDevice *device = m_device;

			m_pointers[0]->m_nextState = 4;
			m_pointers[0]->m_visible = false;
			m_pointers[1]->m_nextState = 4;
			m_pointers[1]->m_visible = false;
			device->Suspend();
		}
	} else {
		if (m_device->IsSuspended() != 0)
			m_device->Resume();
		m_device->Update(m_points, 2);
		UpdatePointers();
		if (m_thread == -1)
			return 1;
	}
	return 0;
}

/* the thread: frames until it is to end, then the points and the pointers put back (0x81143CE8) */
static int imposeTouchEmulationThread(SceSize args, void *argp)
{
	ImposeTouchEmulation *self = g_imposeTouchEmulation;

	while (self->Poll() == 0)
		;
	ImposeTouchPoint *point = self->m_points;

	for (int i = 2; i != 0; i--, point++) {
		point->x = (self->m_front.maxX - self->m_front.minX) / 2;
		point->y = (self->m_front.maxY - self->m_front.minY) / 2;
		point->state = 0;
	}
	self->m_device->Reset();
	self->m_pointers[0]->m_nextState = 4;
	self->m_pointers[0]->m_visible = false;
	self->m_pointers[1]->m_nextState = 4;
	self->m_pointers[1]->m_visible = false;
	return sceKernelExitDeleteThread(0);
}

/* the titles that need the touch emulation, by their title ID or their parameter (0x81143D52) */
static __attribute__((noinline)) bool imposeTouchEmulationIsNeeded(const char *titleId, SceUID processId)
{
	if (sce_paf_strncmp(titleId, "PCSG00074", 9) == 0 ||
	    sce_paf_strncmp(titleId, "PCSG00205", 9) == 0 ||
	    sce_paf_strncmp(titleId, "PCSG90038", 9) == 0 ||
	    sce_paf_strncmp(titleId, "PCSG90089", 9) == 0 ||
	    sce_paf_strncmp(titleId, "PCSG90118", 9) == 0)
		return true;

	int value = 0;

	if (sceAppMgrAppParamGetInt(processId, IMPOSE_APP_PARAM_TOUCH, &value) == 0 &&
	    (value & 1) != 0)
		return true;
	return false;
}

/* a game the setting can give the touch emulation: not a system application (0x81143DFE) */
static __attribute__((noinline)) bool imposeTouchEmulationIsGame(const char *titleId)
{
	ShellPluginInterface *topmenu = (ShellPluginInterface *)
		applauncherPluginGetInterface("topmenu_plugin", 1);

	if (topmenu == NULL)
		return false;

	LsdbAppInfoPtr app;

	((void (*)(LsdbAppInfoPtr *, const char *))
	 topmenu->func[TOPMENU_PLUGIN_FUNC_GET_APP_INFO])(&app, titleId);
	if (app.get() == NULL)
		return false;

	SceInt64 attribute = sceLsdbGetAttribute(&app);
	int type = sceLsdbGetType(&app);

	if (SceLsdb_41019D89(&app) || type == IMPOSE_LSDB_TYPE_SYSTEM2 ||
	    type == IMPOSE_LSDB_TYPE_SYSTEM || (attribute & IMPOSE_LSDB_ATTR_NO_EMULATION) != 0)
		return false;
	return true;
}

/* an application activated: the emulation run if it is for it; deactivated: stopped
   (0x81143F32) */
static void imposeTouchEmulationOnAppEvent(int code, const void *data, SceSize size, void *arg)
{
	const ShellAppEvent *event = (const ShellAppEvent *)data;

	if (g_imposeTouchEmulation == NULL)
		return;
	switch (code) {
	case SHELL_APP_EVENT_ACTIVATED:
		if (imposeTouchEmulationIsGame(event->titleId))
			g_imposeTouchEmulation->m_isGame = true;
		if (imposeTouchEmulationIsNeeded(event->titleId, event->processId))
			g_imposeTouchEmulation->m_needsTouch = true;
		if (g_imposeTouchEmulation->m_isGame || g_imposeTouchEmulation->m_needsTouch)
			g_imposeTouchEmulation->Enable(true);
		break;
	case SHELL_APP_EVENT_DEACTIVATED:
		g_imposeTouchEmulation->m_isGame = false;
		g_imposeTouchEmulation->m_needsTouch = false;
		g_imposeTouchEmulation->Disable(true);
		break;
	}
}
