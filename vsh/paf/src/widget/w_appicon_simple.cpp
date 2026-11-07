/*
 * paf::ui::AppIconSimple (DEBUGGER 0x810dab28..0x810dbb74): an application icon of two planes,
 * the icon masked by the common 64-pixel icon mask and the highlight frame over it, which
 * shrinks and springs back when it is tapped. The common textures are loaded by the first icon
 * and released with the last. No record or string names the file; the names of the static
 * functions and objects are ours.
 */
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/event.h>
#include <common/resource_hooks.h>
#include <widget/factory.h>
#include <widget/w_appicon_simple.h>

namespace paf {
namespace ui {

/* the common textures, loaded while icons exist */
static intrusive_ptr<graph::Surface> s_mask_texture(NULL);
static intrusive_ptr<graph::Surface> s_highlight_texture(NULL);
/* the icons that exist */
static int32_t s_icon_count;

static intrusive_ptr<graph::Surface> &GetHighlightTexture()
{
	if (s_highlight_texture.get() == NULL && s_icon_count != 0)
		s_highlight_texture = ScePafCommon_F820A54B(IDParam("_common_texture_icon_highlight"));
	return s_highlight_texture;
}

static intrusive_ptr<graph::Surface> &GetMaskTexture()
{
	if (s_mask_texture.get() == NULL && s_icon_count != 0)
		s_mask_texture = ScePafCommon_F820A54B(IDParam("_common_texture_icon_mask64"));
	return s_mask_texture;
}

const char AppIconSimple::m_widget_type[] = "app_icon_simple";

/* draw objects 0 (the icon) and 2 (the frame, half transparent; hidden without its texture) */
AppIconSimple::AppIconSimple(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_frame(NULL), m_icon(NULL), m_overlay(NULL), m_tap_enabled(false)
{
	s_icon_count++;
	m_hit_find_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;

	m_icon = new graph::PlaneObj(graph::PlaneObjStyle());
	AddDrawObj(DrawObjEntry(m_icon, 0), false);
	graph::DrawStyle style = graph::DrawObjStyle();

	style.blend = 2;
	m_frame = new graph::PlaneObj(style);
	AddDrawObj(DrawObjEntry(m_frame, 2), false);
	m_frame->m_color = 0x80FFFFFF;

	intrusive_ptr<graph::Surface> &mask = GetMaskTexture();

	if (mask.get() != NULL)
		m_icon->SetTexture(mask, 1);

	intrusive_ptr<graph::Surface> &highlight = GetHighlightTexture();

	if (highlight.get() != NULL)
		m_frame->SetTexture(highlight, 0);
	else
		m_frame->SetVisible(false);
}

bool AppIconSimple::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *AppIconSimple::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *AppIconSimple::TypeName()
{
	return m_widget_type;
}

/* the size is fitted to the icon while the icon's draw lists are dirty */
void AppIconSimple::Update()
{
	if (m_icon->m_flags.m_dirty != 0)
		UpdateAdjustSize();
	Widget::Update();
}

/* the draw objects take the widget's size, and lose their offset, on the axes not adjusted to
   them */
void AppIconSimple::UpdateSize()
{
	math::v3 pos = m_icon->m_pos;
	math::v3 size = m_icon->m_size;

	switch (m_adjust_x) {
	case 0:
	case 1:
		pos.set_x(0.0f);
		size.set_x(m_size.extract_x());
		break;
	}
	switch (m_adjust_y) {
	case 0:
	case 1:
		pos.set_y(0.0f);
		size.set_y(m_size.extract_y());
		break;
	}
	switch (m_adjust_z) {
	case 0:
	case 1:
		size.set_z(m_size.extract_z());
		pos.set_z(0.0f);
		break;
	}
	m_icon->SetSize(size);
	m_icon->SetPos(pos);
	m_frame->SetSize(size);
	m_frame->SetPos(pos);
	if (m_overlay != NULL) {
		m_overlay->SetSize(size);
		m_overlay->SetPos(pos);
	}
}

/* on the axes adjusted to them (2 and 3), the widget takes the icon's bounds about its center */
void AppIconSimple::UpdateAdjustSize()
{
	if ((m_adjust_x | m_adjust_y | m_adjust_z) == 0)
		return;

	math::v4 size = m_size;
	graph::DrawObjBounds bounds = m_icon->GetBounds();
	math::v3 center = (bounds.m_max + bounds.m_min) * 0.5f;
	math::v3 fit = math::v3(sce::Vectormath::Simd::Aos::absPerElem(center.getImpl())) * 2.0f +
		       (bounds.m_max - bounds.m_min);

	if (m_adjust_x > 1 && m_adjust_x <= 3)
		size.set_x(fit.extract_x());
	if (m_adjust_y > 1 && m_adjust_y <= 3)
		size.set_y(fit.extract_y());
	if (m_adjust_z > 1 && m_adjust_z <= 3)
		size.set_z(fit.extract_z());
	if (!(m_size == size))
		SetSize_core(size, 0xFF02, false);
}

/* the widget's state, but 0 for states 1 and 3 while its draw objects are not ready */
uint32_t AppIconSimple::CheckState()
{
	bool prepared = IsPrepareObjs();
	uint32_t state = m_state;

	if ((state == 3 || state == 1) && !prepared)
		return 0;
	return state;
}

/* the texture is the icon's (and draw object 1's), whatever the object */
int32_t AppIconSimple::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				  uint32_t index)
{
	m_texture = surf;
	if (m_icon != NULL)
		m_icon->SetTexture(surf, index);
	if (m_overlay != NULL)
		m_overlay->SetTexture(surf, index);
	ChangeState(3, -1);
	return 0;
}

int32_t AppIconSimple::GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj,
				  uint32_t index)
{
	surf = m_texture;
	return 0;
}

void AppIconSimple::SetAppearance(uint32_t appearance)
{
	if (appearance != 0 && appearance != 3 && appearance != 2)
		return;
	if (m_overlay != NULL) {
		vector<DrawObjEntry>::iterator it = FindDrawObj(1);

		if (it != m_drawobjs.end())
			m_drawobjs.erase(it, it + 1);
		m_overlay = NULL;
	}
	m_icon->SetVisible(true);

	graph::PlaneObj *icon = m_icon;

	if (appearance == 2)
		icon->SetTexture(intrusive_ptr<graph::Surface>(), 1);
	else
		icon->SetTexture(GetMaskTexture(), 1);

	intrusive_ptr<graph::Surface> frame_texture;

	m_frame->GetTexture(frame_texture, 0);
	if (frame_texture.get() != NULL && appearance == 0)
		m_frame->SetVisible(true);
	else
		m_frame->SetVisible(false);
}

int32_t AppIconSimple::EnableTap(bool on_off)
{
	if (on_off) {
		if (!(m_event_enable & 0x400))
			EnableFocusEvent(true);
		m_event_enable |= 0x4F0;
		SetGestureMask(m_touch_gesture_mask | 1, true);
		m_enable_tap = true;
		m_tap_enabled = true;
		return 1;
	}
	if (m_event_enable & 0x400)
		EnableFocusEvent(false);
	m_event_enable &= ~0x4F0;
	SetGestureMask(m_touch_gesture_mask & ~1, true);
	m_tap_enabled = false;
	m_enable_tap = false;
	return ScePafWidget_931AD41E(this, false);
}

/* the frame dims while a touch holds the icon and fades back out when it is released */
void AppIconSimple::OnTouchCaptureChanged(uint32_t port, uint32_t id, int32_t arg)
{
	if (m_touch_captures.size() != 0) {
		KillObjAnim(2, 0x20300);
		SetObjAnim(2, 0x20300, math::v4(1.0f, 1.0f, 1.0f, 0.95f),
			   new Timer(150.0f, Timer::FUNC_QUAD_OUT), 0);
	} else {
		SetObjAnim(2, 0x20300, math::v4(1.0f, 1.0f, 1.0f, 0.5f),
			   new Timer(150.0f, Timer::FUNC_QUAD_OUT), 0);
	}
}

/* a tap (gesture 0) flashes the frame and is passed on as event 0x10000001; a hold (1) dims it */
int32_t AppIconSimple::HandleGestureEvent(Event *e)
{
	if (!m_tap_enabled)
		return 0;

	switch ((e->m_detail >> 8) & 0x7F) {
	case 0:
		DoEvent(0x10000001, e);
		KillObjAnim(2, 0x20300);
		SetObjAnim(2, 0x20300, math::v4(1.0f, 1.0f, 1.0f, 1.0f),
			   new Timer(100.0f, Timer::FUNC_SINE_OUT), 0);
		SetObjAnim(2, 0x20300, math::v4(1.0f, 1.0f, 1.0f, 0.5f),
			   new Timer(250.0f, Timer::FUNC_SINE_IN), 0);
		e->Consume();
		break;
	case 1:
		KillObjAnim(2, 0x20300);
		SetObjAnim(2, 0x20300, math::v4(1.0f, 1.0f, 1.0f, 0.75f),
			   new Timer(150.0f, Timer::FUNC_QUAD_OUT), 0);
		e->Consume();
		break;
	}
	return 0;
}

/* the last icon gone, the common textures go */
static __attribute__((noinline)) void ReleaseTextures()
{
	if (s_icon_count == 0) {
		s_mask_texture = NULL;
		s_highlight_texture = NULL;
	}
}

AppIconSimple::~AppIconSimple()
{
	SetTexture(intrusive_ptr<graph::Surface>(), 0, 0);
	if (s_icon_count != 0)
		s_icon_count--;
	ReleaseTextures();
}

const char AppIconSimpleParam::m_param_name[] = "app_icon_simple";

const char *AppIconSimpleParam::GetParamName() const
{
	return m_param_name;
}

int32_t AppIconSimpleParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL) {
		m_serial->AddParam("enable_tap", &m_enable_tap, ResourceParam::SEMANTIC_BOOL);
		if (m_serial != NULL)
			m_serial->AddParam("appearance", &m_appearance, ResourceParam::SEMANTIC_INT);
	}
	return 0;
}

/* appearance 1 is shown as 0 */
int32_t AppIconSimpleParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;

	AppIconSimple *icon = (AppIconSimple *)m_widget;
	uint32_t appearance;

	icon->EnableTap(m_enable_tap);
	appearance = m_appearance;
	if (appearance == 1)
		appearance = 0;
	icon->SetAppearance(appearance);
	return 0;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewAppIconSimple(Widget *parent)
{
	return new AppIconSimple(parent, NULL);
}

static WidgetParam *NewAppIconSimpleParam()
{
	return new AppIconSimpleParam;
}

static VersatileParam *NewAppIconSimpleLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewAppIconSimpleFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewAppIconSimpleStyleParam()
{
	return new WidgetStyleParam;
}

static VersatileParam *NewAppIconSimpleHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewAppIconSimpleSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryAppIconSimple = {
	AppIconSimple::TypeName(),
	NewAppIconSimple,
	NewAppIconSimpleParam,
	NewAppIconSimpleLayoutParam,
	NewAppIconSimpleFocusParam,
	NewAppIconSimpleStyleParam,
	NewAppIconSimpleHandlerParam,
	NewAppIconSimpleSoundParam,
};
