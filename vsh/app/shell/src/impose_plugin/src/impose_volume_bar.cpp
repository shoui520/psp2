/*
 * impose_volume_bar.cpp: impose_plugin's volume bar: the gauge with the volume, the Bluetooth
 * device's keys, the AVLS limit and the mute (0x81145B00-0x81148088).
 * The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/stdio.h>
#include <versatileparam/idparam.h>
#include <timer/timer.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <widget/w_plane.h>
#include <graphics/drawobj/plane_obj.h>
#include "../../livespace_db/lsdb_keys.h"
#include "impose.h"

using namespace paf;

/* the bar's template and widgets (PLACEHOLDER names) */
#define IMPOSE_VBAR_TEMPLATE		0xD7189214
#define IMPOSE_VBAR_ROOT		0xFDAFB866
#define IMPOSE_VBAR_FRAME		0xAA80A824
#define IMPOSE_VBAR_BLUETOOTH		0x5FDDADF6
#define IMPOSE_VBAR_AVLS		0xB88B2270
#define IMPOSE_VBAR_GAUGE		0xEC46AB08
#define IMPOSE_VBAR_GAUGE_REST		0x1EC69C43
#define IMPOSE_VBAR_SPEAKER		0x4CD8AEA9
#define IMPOSE_VBAR_PLUS		0x77319D7F	/* the Bluetooth device's keys */
#define IMPOSE_VBAR_MINUS		0x8D6EE8DE
#define IMPOSE_VBAR_KEYS		0x9D79B4E3
#define IMPOSE_VBAR_TEX_SPEAKER		0xEDD9D029
#define IMPOSE_VBAR_TEX_SPEAKER_LIMIT	0xEA463483
#define IMPOSE_VBAR_FADE_TIMER		0x27		/* the gauge going down to 0 */

#define IMPOSE_VBAR_STEPS		30
#define IMPOSE_VBAR_GAUGE_WIDTH		540.0f

/* (0x81564D40) */
ImposeVolumeBar *g_imposeVolumeBar;

/* the child, when it is of the type */
#define IMPOSE_VBAR_FIND(parent, type, id)						\
	((parent)->FindChild(IDParam(id), 0) != NULL &&					\
	 (parent)->FindChild(IDParam(id), 0)->IsInherit(type::TypeName()) ?		\
	 (type *)(parent)->FindChild(IDParam(id), 0) : NULL)

/* the widget's draw object, when it is a plane object */
#define IMPOSE_VBAR_PLANE_OBJ(widget)							\
	((widget)->GetDrawObj(0) != NULL &&						\
	 (widget)->GetDrawObj(0)->IsInherit(graph::PlaneObj::DrawObjType()) ?		\
	 (graph::PlaneObj *)(widget)->GetDrawObj(0) : NULL)

static void imposeVolumeBarOnFade(SceInt32 type, ui::Handler *self, ui::Event *e, void *data);
static void imposeVolumeBarOnLimit(Timer *t, ui::AnimListener *listener, void *target,
				   void *userData, void *context);

/* (0x81145B00) */
ImposeVolumeBar::ImposeVolumeBar(Plugin *plugin)
	: m_plugin(plugin), m_root(NULL), m_frame(NULL), m_bluetoothIcon(NULL), m_avlsIcon(NULL),
	  m_gauge(NULL), m_gaugeRest(NULL), m_shownVolume(0), m_active(false), m_state(0)
{
	g_imposeVolumeBar = this;
}

/* (0x81145B24) */
ImposeVolumeBar::~ImposeVolumeBar()
{
	g_imposeVolumeBar = NULL;
}

/* the animations stopped, and the speaker shown as it is at rest (0x81145B34) */
void ImposeVolumeBar::Reset()
{
	if (m_root == NULL)
		return;

	m_active = false;
	m_gauge->KillAnim(ui::Widget::ANIM_COLOR);
	m_gaugeRest->KillAnim(ui::Widget::ANIM_COLOR);
	m_gaugeRest->KillIntervalEvent(IMPOSE_VBAR_FADE_TIMER);

	ui::Widget *speaker = m_root->FindChild(IDParam(IMPOSE_VBAR_SPEAKER), 0);

	if (speaker != NULL) {
		speaker->SetTexture(m_plugin->GetTexture(IDParam(IMPOSE_VBAR_TEX_SPEAKER)));
		speaker->KillAnim(ui::Widget::ANIM_COLOR);
		speaker->KillAnim(ui::Widget::ANIM_SCALE);
		speaker->SetScale(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

/* (0x81145CB4) */
int ImposeVolumeBar::Close()
{
	if (m_root == NULL)
		return 0;

	m_active = false;
	m_root->DestroyWidget();
	m_avlsIcon = NULL;
	m_gauge = NULL;
	m_gaugeRest = NULL;
	m_frame = NULL;
	m_bluetoothIcon = NULL;
	m_root = NULL;
	m_state = 0;
	return 1;
}

/* the bar made from its template over the plugin's second page (0x81145CDE) */
void ImposeVolumeBar::Open(int mode)
{
	if (m_state == mode)
		return;

	if (m_root != NULL) {
		m_active = false;
		m_root->DestroyWidget();
		m_avlsIcon = NULL;
		m_gauge = NULL;
		m_gaugeRest = NULL;
		m_frame = NULL;
		m_bluetoothIcon = NULL;
		m_root = NULL;
	}
	m_state = mode;
	if (mode == 1) {
		m_plugin->TemplateOpen(g_impose != NULL ? g_impose->m_page2 : NULL,
				       IDParam(IMPOSE_VBAR_TEMPLATE), Plugin::TemplateOpenParam());
		ui::Scene *page = g_impose != NULL ? g_impose->m_page2 : NULL;
		graph::PlaneObj *obj;

		m_root = page->FindChild(IDParam(IMPOSE_VBAR_ROOT), 0);
		m_root->m_hit_detect_mode = 2;
		m_root->m_hit_detect_b_mode = 2;
		ScePafWidget_66847A11(m_root, true);

		m_frame = m_root->FindChild(IDParam(IMPOSE_VBAR_FRAME), 0);
		m_frame->m_hit_detect_mode = 2;
		m_frame->m_hit_detect_b_mode = 2;

		m_bluetoothIcon = m_root->FindChild(IDParam(IMPOSE_VBAR_BLUETOOTH), 0);
		m_bluetoothIcon->m_hit_detect_mode = 2;
		m_bluetoothIcon->m_hit_detect_b_mode = 2;
		m_bluetoothIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));

		m_avlsIcon = m_root->FindChild(IDParam(IMPOSE_VBAR_AVLS), 0);
		m_avlsIcon->m_hit_detect_mode = 2;
		m_avlsIcon->m_hit_detect_b_mode = 2;
		m_avlsIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));

		m_gauge = IMPOSE_VBAR_FIND(m_root, ui::Plane, IMPOSE_VBAR_GAUGE);
		m_gauge->m_hit_detect_mode = 2;
		m_gauge->m_hit_detect_b_mode = 2;
		m_gauge->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
		obj = IMPOSE_VBAR_PLANE_OBJ(m_gauge);
		if (obj != NULL)
			obj->SetWrapS(graph::REPEAT, 0);

		m_gaugeRest = IMPOSE_VBAR_FIND(m_root, ui::Plane, IMPOSE_VBAR_GAUGE_REST);
		m_gaugeRest->m_hit_detect_mode = 2;
		m_gaugeRest->m_hit_detect_b_mode = 2;
		m_gaugeRest->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
		obj = IMPOSE_VBAR_PLANE_OBJ(m_gaugeRest);
		if (obj != NULL)
			obj->SetWrapS(graph::REPEAT, 0);
	}
	Reset();
}

/* (0x811463B8) */
int ImposeVolumeBar::IsOpen(int mode)
{
	return m_state == mode && m_frame != NULL;
}

/* the AVLS icon shown, unless it is blinking */
static inline void imposeVolumeBarShowAvls(ImposeVolumeBar *bar, int avls)
{
	if (bar->m_avlsIcon == NULL)
		return;
	if (avls) {
		if (bar->m_avlsIcon->FindAnim(ui::Widget::ANIM_COLOR) == NULL)
			bar->m_avlsIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	} else {
		bar->m_avlsIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	}
}

/* (0x811463CC) */
void ImposeVolumeBar::SetVolume(int volume, SceUInt32 flags)
{
	if (m_root == NULL || m_state != 1)
		return;

	float part = (float)volume / IMPOSE_VBAR_STEPS;
	bool bluetooth = (flags & IMPOSE_VOLUME_BLUETOOTH) != 0;
	bool avls = (flags & IMPOSE_VOLUME_AVLS) != 0;

	Reset();
	if (m_bluetoothIcon != NULL) {
		if (bluetooth)
			m_bluetoothIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		else
			m_bluetoothIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	}
	imposeVolumeBarShowAvls(this, avls);
	m_avlsShown = avls;

	float width = 636.0f;

	if (avls)
		width = 720.0f;
	else if (bluetooth)
		width = 668.0f;

	ui::Widget *child;

	child = m_frame->FindChild(IDParam(IMPOSE_VBAR_PLUS), 0);
	if (child != NULL)
		child->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	child = m_frame->FindChild(IDParam(IMPOSE_VBAR_MINUS), 0);
	if (child != NULL)
		child->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	child = m_frame->FindChild(IDParam(IMPOSE_VBAR_KEYS), 0);
	if (child != NULL)
		child->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	m_frame->SetSize(math::v4(width, 64.0f, 0.0f, 0.0f));

	child = m_root->FindChild(IDParam(IMPOSE_VBAR_SPEAKER), 0);
	if (child != NULL)
		child->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));

	if (m_gauge != NULL) {
		math::v4 size = *m_gauge->GetSize(0);

		size.set_x(part * IMPOSE_VBAR_GAUGE_WIDTH);
		m_gauge->SetSize(size);
		m_gauge->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		((graph::PlaneObj *)m_gauge->GetDrawObj(0))->SetTexRegion(
			math::v4(0.0f, 0.0f, (float)volume, 1.0f));
	}
	if (m_gaugeRest != NULL) {
		math::v4 size = *m_gaugeRest->GetSize(0);

		size.set_x((1.0f - part) * IMPOSE_VBAR_GAUGE_WIDTH);
		m_gaugeRest->SetSize(size);
		m_gaugeRest->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
		((graph::PlaneObj *)m_gaugeRest->GetDrawObj(0))->SetTexRegion(
			math::v4(0.0f, 0.0f, (float)(IMPOSE_VBAR_STEPS - volume), 1.0f));
	}
	m_shownVolume = volume;
}

/* the Bluetooth device's keys shown, the one pressed blinking (0x81146974) */
void ImposeVolumeBar::ShowBluetoothKey(int dir)
{
	Reset();
	if (m_bluetoothIcon != NULL)
		m_bluetoothIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	if (m_avlsIcon != NULL)
		m_avlsIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	m_avlsShown = false;
	if (m_gauge != NULL)
		m_gauge->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	if (m_gaugeRest != NULL) {
		m_gaugeRest->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
		m_gaugeRest->KillIntervalEvent(IMPOSE_VBAR_FADE_TIMER);
	}
	if (m_frame != NULL)
		m_frame->SetSize(math::v4(668.0f, 64.0f, 0.0f, 0.0f));

	ui::Widget *child;

	if (m_root != NULL) {
		child = m_root->FindChild(IDParam(IMPOSE_VBAR_SPEAKER), 0);
		if (child != NULL)
			child->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	}
	if (m_frame == NULL)
		return;

	child = m_frame->FindChild(IDParam(IMPOSE_VBAR_KEYS), 0);
	if (child != NULL)
		child->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));

	ui::Widget *plus = m_frame->FindChild(IDParam(IMPOSE_VBAR_PLUS), 0);
	ui::Widget *minus = m_frame->FindChild(IDParam(IMPOSE_VBAR_MINUS), 0);

	if (plus == NULL || minus == NULL)
		return;

	wchar_t text[8];

	minus->SetPos(math::v4(-65.0f, 4.0f, 0.0f, 0.0f));
	sce_paf_swprintf(text, 8, L"-");
	minus->SetString(wstring(text));
	plus->SetPos(math::v4(295.0f, 4.0f, 0.0f, 0.0f));
	sce_paf_swprintf(text, 8, L"+");
	plus->SetString(wstring(text));

	if (dir > 0) {
		minus->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.6f));
		plus->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f), NULL, ui::Widget::ANIM_COLOR, 2);
		for (int i = 0; i < 4; i++) {
			plus->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.2f),
				       new Timer(80.0f, Timer::FUNC_QUAD_IN));
			plus->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
				       new Timer(200.0f, Timer::FUNC_QUAD_OUT),
				       ui::Widget::ANIM_COLOR, 0, 40.0f);
		}
	} else if (dir < 0) {
		minus->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f), NULL, ui::Widget::ANIM_COLOR, 2);
		for (int i = 0; i < 4; i++) {
			minus->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.2f),
					new Timer(80.0f, Timer::FUNC_QUAD_IN));
			minus->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
					new Timer(200.0f, Timer::FUNC_QUAD_OUT),
					ui::Widget::ANIM_COLOR, 0, 40.0f);
		}
		plus->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.6f));
	}
}

/* the volume held at the AVLS limit: the gauge dimmed and going down, the speaker pulsing
   (0x811470AC) */
void ImposeVolumeBar::ShowAvlsLimit(int avls)
{
	if (m_active && avls == m_avlsShown)
		return;

	m_active = true;
	if (m_bluetoothIcon != NULL)
		m_bluetoothIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	imposeVolumeBarShowAvls(this, avls);
	m_avlsShown = avls != 0;

	if (m_gauge != NULL) {
		m_gauge->KillAnim(ui::Widget::ANIM_COLOR);
		m_gauge->SetColor(math::v4(0.6f, 0.6f, 0.6f, 0.8f), new Timer(60.0f),
				  ui::Widget::ANIM_COLOR, 2);
	}
	if (m_gaugeRest != NULL) {
		m_gaugeRest->KillAnim(ui::Widget::ANIM_COLOR);
		m_gaugeRest->SetColor(math::v4(0.6f, 0.6f, 0.6f, 0.8f), new Timer(60.0f),
				      ui::Widget::ANIM_COLOR, 2);
		m_gaugeRest->SetIntervalEvent(IMPOSE_VBAR_FADE_TIMER, 20.0f, imposeVolumeBarOnFade,
					      (void *)(m_shownVolume / 3));
	}

	if (m_frame == NULL)
		return;

	m_frame->SetSize(math::v4(avls != 0 ? 720.0f : 636.0f, 64.0f, 0.0f, 0.0f));

	ui::Widget *child = m_root->FindChild(IDParam(IMPOSE_VBAR_SPEAKER), 0);

	if (child != NULL) {
		child->KillAnim(ui::Widget::ANIM_COLOR);
		child->KillAnim(ui::Widget::ANIM_SCALE);
		child->SetScale(math::v4(0.5f, 0.5f, 1.0f, 1.0f), new Timer(60.0f),
				ui::Widget::ANIM_SCALE, 0);
		child->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.5f), new Timer(60.0f),
				ui::Widget::ANIM_COLOR, 2, 0.0f, imposeVolumeBarOnLimit, this);
	}
	child = m_frame->FindChild(IDParam(IMPOSE_VBAR_PLUS), 0);
	if (child != NULL)
		child->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	child = m_frame->FindChild(IDParam(IMPOSE_VBAR_MINUS), 0);
	if (child != NULL)
		child->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	child = m_frame->FindChild(IDParam(IMPOSE_VBAR_KEYS), 0);
	if (child != NULL)
		child->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
}

/* the AVLS icon blinking five times, or shown still (0x81147706) */
void ImposeVolumeBar::BlinkAvls(int on)
{
	if (m_avlsIcon == NULL)
		return;

	if (on) {
		if (m_avlsIcon->FindAnim(ui::Widget::ANIM_COLOR) == NULL) {
			m_avlsIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f), NULL,
					     ui::Widget::ANIM_COLOR, 2);
			for (int i = 0; i < 5; i++) {
				m_avlsIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f),
						     new Timer(200.0f), ui::Widget::ANIM_COLOR, 0);
				m_avlsIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f),
						     new Timer(120.0f), ui::Widget::ANIM_COLOR, 0);
			}
		}
	} else {
		m_avlsIcon->KillAnim(ui::Widget::ANIM_COLOR);
		m_avlsIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

/* the bar dimmed for the mute (0x811478C0) */
void ImposeVolumeBar::ShowMute(bool bluetooth)
{
	Reset();
	if (m_avlsIcon != NULL)
		m_avlsIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	m_avlsShown = false;
	if (m_frame == NULL)
		return;
	if (m_gaugeRest != NULL && m_gaugeRest->GetColor().extract_x() == 0.6f)
		return;

	m_frame->SetSize(math::v4(668.0f, 64.0f, 0.0f, 0.0f));

	ui::Widget *child = m_frame->FindChild(IDParam(IMPOSE_VBAR_KEYS), 0);

	if (child != NULL)
		child->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	child = m_root->FindChild(IDParam(IMPOSE_VBAR_SPEAKER), 0);
	if (child != NULL)
		child->SetColor(math::v4(0.6f, 0.6f, 0.6f, 0.8f));
	if (m_bluetoothIcon != NULL) {
		if (bluetooth)
			m_bluetoothIcon->SetColor(math::v4(0.6f, 0.6f, 0.6f, 0.8f));
		else
			m_bluetoothIcon->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	}
	if (m_gauge != NULL)
		m_gauge->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f));
	if (m_gaugeRest == NULL)
		return;

	math::v4 size = *m_gaugeRest->GetSize(0);

	size.set_x(IMPOSE_VBAR_GAUGE_WIDTH);
	m_gaugeRest->SetSize(size);
	m_gaugeRest->SetColor(math::v4(0.6f, 0.6f, 0.6f, 0.8f));
	((graph::PlaneObj *)m_gaugeRest->GetDrawObj(0))->SetTexRegion(
		math::v4(0.0f, 0.0f, (float)IMPOSE_VBAR_STEPS, 1.0f));
}

/* the gauge going down by the step, each interval, until it is empty (0x81147C50) */
void ImposeVolumeBar::OnFade(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	int step = (int)data;
	int volume;

	m_shownVolume -= step;
	volume = m_shownVolume;
	if (volume <= 0 || step == 0) {
		volume = 0;
		m_gaugeRest->KillIntervalEvent(IMPOSE_VBAR_FADE_TIMER);
	}

	float part = (float)volume / IMPOSE_VBAR_STEPS;
	math::v4 size = *m_gauge->GetSize(0);

	size.set_x(part * IMPOSE_VBAR_GAUGE_WIDTH);
	m_gauge->SetSize(size);
	((graph::PlaneObj *)m_gauge->GetDrawObj(0))->SetTexRegion(
		math::v4(0.0f, 0.0f, (float)volume, 1.0f));

	math::v4 rest = *m_gaugeRest->GetSize(0);

	rest.set_x((1.0f - part) * IMPOSE_VBAR_GAUGE_WIDTH);
	m_gaugeRest->SetSize(rest);
	((graph::PlaneObj *)m_gaugeRest->GetDrawObj(0))->SetTexRegion(
		math::v4(0.0f, 0.0f, (float)(IMPOSE_VBAR_STEPS - volume), 1.0f));
}

/* (0x81147DBA) */
static void imposeVolumeBarOnFade(SceInt32 type, ui::Handler *self, ui::Event *e, void *data)
{
	if (g_imposeVolumeBar != NULL)
		g_imposeVolumeBar->OnFade(type, self, e, data);
}

/* the speaker shown at the limit, coming back to its size (0x81147DDE) */
void ImposeVolumeBar::ShowLimit()
{
	ui::Widget *speaker = m_root->FindChild(IDParam(IMPOSE_VBAR_SPEAKER), 0);

	if (speaker != NULL) {
		speaker->SetTexture(m_plugin->GetTexture(IDParam(IMPOSE_VBAR_TEX_SPEAKER_LIMIT)));
		speaker->KillAnim(ui::Widget::ANIM_COLOR);
		speaker->KillAnim(ui::Widget::ANIM_SCALE);
		speaker->SetScale(math::v4(1.0f, 1.0f, 1.0f, 1.0f), new Timer(60.0f),
				  ui::Widget::ANIM_SCALE, 0);
		speaker->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f), new Timer(60.0f),
				  ui::Widget::ANIM_COLOR, 2);
	}
}

/* (0x81148004) */
static void imposeVolumeBarOnLimit(Timer *t, ui::AnimListener *listener, void *target,
				   void *userData, void *context)
{
	if (g_imposeVolumeBar != NULL)
		g_imposeVolumeBar->ShowLimit();
}
