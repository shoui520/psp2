/*
 * paf::ui::ImageSlideBar (DEBUGGER 0x810f1f10..0x810f3464): a bar over its base, stretched to
 * its value of the maximum, set by a touch along it or by the left and right keys. No record or
 * string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/animlistener.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/factory.h>
#include <widget/w_image_slidebar.h>
#include "../std/math.h"

namespace paf {
namespace ui {

/* the vectors multiplied element by element (name ours) */
static inline math::v4 MulPerElem(const math::v4 &a, const math::v4 &b)
{
	return math::v4(sce::Vectormath::Simd::Aos::mulPerElem(a.getImpl(), b.getImpl()));
}

/* the region of the plane's texture shown: all of it until one is set (inlined in Sony's code,
   name ours) */
static inline math::v4 GetTexRegion(const graph::PlaneObj *plane)
{
	if (plane->m_tex_region != NULL)
		return *plane->m_tex_region;
	return math::v4(0.0f, 0.0f, 1.0f, 1.0f);
}

/* animates the region of a plane's texture from one to another (name ours; its vtable is
   ImageSlideBar's) */
class TexRegionAnimListener : public AnimListener {
public:
	TexRegionAnimListener(Timer *t, Widget *w, graph::PlaneObj *plane, const math::v4 &src,
			      const math::v4 &dst)
		: AnimListener(t, w, NULL, NULL, NULL), m_plane(plane), m_src(src), m_dst(dst)
	{
	}

	virtual ~TexRegionAnimListener()
	{
	}

	virtual void OnTick(void *data1, void *data2)
	{
		if (m_plane != NULL) {
			float rate = m_timer->m_mapped_x;

			m_plane->SetTexRegion(m_src * (1.0f - rate) + m_dst * rate);
		}
	}

	graph::PlaneObj *m_plane;
	math::v4 m_src;
	math::v4 m_dst;
};

/* the base (OBJ_BASE) and the bar (OBJ_BAR), their textures repeated across */
ImageSlideBar::ImageSlideBar(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_base_mode(BASE_MODE_ONE_BASED), m_texture_mode(TEXTURE_MODE_WHOLE),
	  m_max(5), m_value(1), m_anim_time(150.0f), m_anim_func(0), m_norm_value(0.0f),
	  m_touch_id(-1)
{
	/* through the vtable, as Sony's */
	Widget *self = this;

	if (!(m_event_enable & 0x400))
		self->EnableFocusEvent(true);
	m_event_enable |= 0xFFF0;
	m_hit_find_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_hit_check = 3;
	m_sound_mask = 3;

	graph::PlaneObj *base = new graph::PlaneObj(graph::PlaneObjStyle());

	if (base != NULL) {
		base->m_plane_flags.m_repeat_s = true;
		AddDrawObj(DrawObjEntry(base, OBJ_BASE), false);
	}

	graph::PlaneObj *bar = new graph::PlaneObj(graph::PlaneObjStyle());

	if (bar != NULL) {
		bar->m_plane_flags.m_repeat_s = true;

		common::SharedPtr<graph::DrawObj> ptr(bar);
		DrawObjAttr attr(OBJ_BAR);

		attr.m_flag1 = false;
		AddDrawObj(DrawObjEntry(ptr, attr), false);
	}
}

const ImageSlideBarParam::TextureRef ImageSlideBarParam::c_textureList[] = {
	{ "texture_bar", ImageSlideBar::OBJ_BAR },
};

ImageSlideBarParam::ImageSlideBarParam()
{
	for (uint32_t i = 0; i < sizeof(c_textureList) / sizeof(c_textureList[0]); i++)
		m_texture_list.push_back(IndexedRef(c_textureList[i].name, c_textureList[i].index));
	m_base_mode = ImageSlideBar::BASE_MODE_ONE_BASED;
	m_tex_mode = ImageSlideBar::TEXTURE_MODE_WHOLE;
}

ImageSlideBarStyleParam::ImageSlideBarStyleParam()
{
	m_anim_time = 150.0f;
	m_anim_func = 0;
	m_base_obj.m_name = "base_obj";
	m_base_obj.m_index = ImageSlideBar::OBJ_BASE;
	m_drawobj_param_list.push_back(&m_base_obj);
	m_bar_obj.m_name = "bar_obj";
	m_bar_obj.m_index = ImageSlideBar::OBJ_BAR;
	m_drawobj_param_list.push_back(&m_bar_obj);
}

/* the sounds a slidebar's sound parameter names, by index */
struct SoundDef {
	const char *name;
	uint32_t index;
};

extern const SoundDef s_image_slidebar_sounds[] = {
	{ "drag_begin", ImageSlideBar::SOUND_DRAG_BEGIN },
	{ "drag_end", ImageSlideBar::SOUND_DRAG_END },
};

ImageSlideBarSoundParam::ImageSlideBarSoundParam()
{
	for (uint32_t i = 0; i < 2; i++)
		m_sound_list.push_back(
			Sound(s_image_slidebar_sounds[i].name, s_image_slidebar_sounds[i].index));
}

const char ImageSlideBar::m_widget_type[] = "image_slidebar";

bool ImageSlideBar::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *ImageSlideBar::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *ImageSlideBar::TypeName()
{
	return m_widget_type;
}

ImageSlideBar::~ImageSlideBar()
{
}

int32_t ImageSlideBar::SetObjColor(const math::v4 &color, int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return -1;
	draw->SetColor(color);
	return 0;
}

int32_t ImageSlideBar::GetObjColor(math::v4 &color, int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return -1;
	color = draw->GetColor();
	return 0;
}

int32_t ImageSlideBar::SetBaseMode(BaseMode mode)
{
	if (mode < BASE_MODE_ZERO_BASED || mode > BASE_MODE_ONE_BASED)
		return 0x80AF008E;	/* no name found */
	m_base_mode = mode;
	SetValue(m_value, false);
	return 0;
}

int32_t ImageSlideBar::SetTextureMode(TextureMode mode)
{
	if (mode < TEXTURE_MODE_WHOLE || mode > TEXTURE_MODE_STEP)
		return 0x80AF008E;
	m_texture_mode = mode;
	UpdateBase();
	UpdateBar(false);
	return 0;
}

int32_t ImageSlideBar::SetMaxValue(int32_t max)
{
	if (max <= 0)
		return 0x80AF008E;
	m_max = max;
	AdjustParameter();
	UpdateBase();
	UpdateBar(false);
	return 0;
}

int32_t ImageSlideBar::SetValue(int32_t value, bool anim)
{
	m_value = value;
	AdjustParameter();
	UpdateBar(anim);
	return 0;
}

int32_t ImageSlideBar::SetBarAnim(float time, int32_t func)
{
	if (time <= 0.0f)
		return 0x80AF008E;
	m_anim_time = time;
	m_anim_func = func;
	return 0;
}

int32_t ImageSlideBar::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				  uint32_t index)
{
	graph::DrawObj *draw = Widget::GetDrawObj(obj);

	if (draw == NULL)
		return 0x80AF4101;	/* no name found */
	draw->SetTexture(surf, index);
	return 0;
}

void ImageSlideBar::AddSound(uint32_t index, WidgetSoundParam::Sound &sound)
{
	if (index < SOUND_IMAGE_SLIDEBAR_END) {
		/* the sound it had of the index replaced */
		for (list<WidgetSoundParam::Sound>::iterator it = m_sound_list.begin();
		     it != m_sound_list.end(); ++it) {
			if (it->m_index == index) {
				m_sound_list.erase(it);
				break;
			}
		}
		sound.m_index = index;
		m_sound_list.push_back(sound);
	}
}

WidgetSoundParam::Sound *ImageSlideBar::GetSound(int32_t id)
{
	for (list<WidgetSoundParam::Sound>::iterator it = m_sound_list.begin();
	     it != m_sound_list.end(); ++it) {
		if (it->m_index == (uint32_t)id)
			return &*it;
	}
	return FindSound(id);
}

uint32_t ImageSlideBar::CheckState()
{
	if (m_hold_count > 0)
		return m_state;
	return IsPrepareObjs() ? 3 : 0;
}

void ImageSlideBar::UpdateSize()
{
	Widget::UpdateSize();
	UpdateBar(false);
}

int32_t ImageSlideBar::HandlePointEvent(Event *e)
{
	OnTouch(e);
	return Handler::HandlePointEvent(e);
}

int32_t ImageSlideBar::HandleTouchEvent(Event *e)
{
	OnTouch(e);
	return Widget::HandleTouchEvent(e);
}

/* key 3 a step down, key 4 a step up, at its press (17) or its repeat (19), which also takes the
   value at the end of the range */
int32_t ImageSlideBar::HandleKeycodeEvent(Event *e)
{
	int32_t min = m_base_mode == BASE_MODE_ONE_BASED;
	int32_t value = -1;

	switch ((uint8_t)e->m_detail) {
	case 3:
		switch (e->GetType()) {
		case 19:
			if (m_value == min)
				value = min;
			/* fall through */
		case 17:
			if (m_value > min)
				value = m_value - 1;
			break;
		}
		break;
	case 4:
		switch (e->GetType()) {
		case 19:
			if (m_value == m_max)
				value = m_max;
			/* fall through */
		case 17:
			if (m_value < m_max)
				value = m_value + 1;
			break;
		}
		break;
	}
	if (value >= 0) {
		SetValue(value, true);
		e->Consume();
	}
	return Widget::HandleKeycodeEvent(e);
}

void ImageSlideBar::AdjustParameter()
{
	int32_t min = m_base_mode == BASE_MODE_ONE_BASED;
	int32_t max = m_max;
	int32_t value = m_value;

	if (min > value)
		value = min;
	m_value = max > value ? value : max;
	m_norm_value = (float)m_value / (float)max;
}

void ImageSlideBar::SetTexRegion(int32_t obj, const math::v4 &region, Timer *t, int32_t option)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL || !draw->IsInherit(graph::PlaneObj::DrawObjType()))
		return;

	graph::PlaneObj *plane = (graph::PlaneObj *)draw;

	if (t == NULL)
		plane->SetTexRegion(region);
	else
		SetAnimListener(0x1000C,
				new TexRegionAnimListener(t, this, plane, GetTexRegion(plane), region),
				option);
}

/* the base's texture shown whole, or one step of it per value of the maximum */
void ImageSlideBar::UpdateBase()
{
	float width = 1.0f;

	if (m_texture_mode != TEXTURE_MODE_WHOLE)
		width = (float)m_max;
	SetTexRegion(OBJ_BASE, math::v4(0.0f, 0.0f, width, 1.0f), NULL, 0);
}

void ImageSlideBar::UpdateBar(bool anim)
{
	math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);

	if (m_base_mode == BASE_MODE_ONE_BASED) {
		SetHitBox(4, zero, zero);
	} else {
		/* half a step more than the widget, from half a step before its left */
		float max = (float)m_max;
		math::v4 scale(0.5f / max + 1.0f, 1.0f, 1.0f, 1.0f);
		math::v4 offset(-0.25f / max, 0.0f, 0.0f, 0.0f);
		math::v4 center = MulPerElem(m_size, offset);
		math::v4 size = ScePafWidget_301AA80F(this, MulPerElem(m_size, scale));

		SetHitBox(0, size, center);
	}

	graph::DrawObj *bar = GetDrawObj(OBJ_BAR);

	if (bar == NULL)
		return;
	KillObjAnim(OBJ_BAR, 0x20100);
	KillObjAnim(OBJ_BAR, 0x20200);

	/* the bar its share of the widget, from its left */
	math::v4 size = MulPerElem(math::v4(m_norm_value, 1.0f, 1.0f, 1.0f), m_size);
	math::v4 pos = MulPerElem(math::v4(-m_size.getImpl()), math::v4(0.5f, 0.0f, 0.0f, 0.0f));

	pos.set_x(pos.extract_x() + size.extract_x() * 0.5f);

	float width = m_norm_value;

	if (m_texture_mode != TEXTURE_MODE_WHOLE)
		width = (float)m_value;

	math::v4 region(0.0f, 0.0f, width, 1.0f);

	if (anim) {
		SetObjAnim(OBJ_BAR, 0x20100, pos, new Timer(m_anim_time, m_anim_func), 2);
		SetObjAnim(OBJ_BAR, 0x20200, size, new Timer(m_anim_time, m_anim_func), 2);
		SetTexRegion(OBJ_BAR, region, new Timer(m_anim_time, m_anim_func), 2);
	} else {
		bar->SetPos(math::v3(pos.getImpl().getXYZ()));
		bar->SetSize(math::v3(size.getImpl().getXYZ()));
		SetTexRegion(OBJ_BAR, region, NULL, 0);
	}
}

/* a touch down takes the touch it follows and begins a drag, its moves set the value, its release
   or cancel ends the drag */
void ImageSlideBar::OnTouch(Event *e)
{
	int32_t id = (uint8_t)e->m_detail;
	int32_t touch_id = m_touch_id;

	if (touch_id >= 0 && touch_id != id)
		return;
	switch (e->GetType()) {
	case 0x21:
	case 0x25:
	case 0x41:
	case 0x44:
		if (touch_id < 0) {
			m_touch_id = id;
			ScePafWidget_2B1A1493(this, SOUND_DRAG_BEGIN);

			Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

			DoEvent(CB_IMAGE_SLIDEBAR_DRAG_BEGIN, &ev);
		}
		OnTouchUpdateValue();
		e->Consume();
		break;
	case 0x23:
	case 0x43:
		if (touch_id < 0)
			break;
		OnTouchUpdateValue();
		e->Consume();
		break;
	case 0x22:
	case 0x42:
		OnDragEnd(false);
		e->Consume();
		break;
	case 0x28:
	case 0x47:
		OnDragEnd(true);
		e->Consume();
		break;
	}
}

/* the value at the touch's place along the widget: where it hit the widget, or where its line
   crosses the widget's plane once it has left it */
void ImageSlideBar::OnTouchUpdateValue()
{
	uint32_t id = m_touch_id;
	Environment *env = m_env;
	HitInfo *hit = NULL;
	float x;

	if (id < 8)
		hit = &env->m_touch_hit[0][id];
	if (hit != NULL && hit->m_widget == this) {
		x = hit->m_touch_pos.extract_x();
	} else {
		TouchState *touch = NULL;

		if (id >= 8)
			return;
		env->m_touch_state[0][id].m_port = 0;
		env->m_touch_state[0][id].m_id = id;
		touch = &env->m_touch_state[0][id];
		if (touch == NULL)
			return;

		HitInfo info;

		CalcHitLineToPlane(touch->m_line_start, touch->m_line_end,
				   math::v4(0.0f, 0.0f, 0.0f, 0.0f), math::v4(0.0f, 0.0f, 1.0f, 0.0f),
				   &info);
		x = info.m_touch_pos.extract_x();
	}

	int32_t max = m_max;
	float width = m_size.extract_x();
	float rate = 0.0f;

	if (width >= 1.0f)
		rate = x / width + 0.5f;

	float value = sce_paf_ceilf(rate * (float)max);
	int32_t old = m_value;

	SetValue((int32_t)value, false);
	if (m_value != old) {
		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		DoEvent(CB_IMAGE_SLIDEBAR_CHANGED, &ev);
	}
}

void ImageSlideBar::OnDragEnd(bool abort)
{
	if (m_touch_id < 0)
		return;
	ScePafWidget_2B1A1493(this, SOUND_DRAG_END);

	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

	ev.m_data[0] = abort;
	DoEvent(CB_IMAGE_SLIDEBAR_DRAG_END, &ev);
	m_touch_id = -1;
}

const char ImageSlideBarParam::m_param_name[] = "image_slidebar_base";

const char *ImageSlideBarParam::GetParamName() const
{
	return m_param_name;
}

const char *ImageSlideBarParam::ParamName()
{
	return m_param_name;
}

int32_t ImageSlideBarParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("base_mode", &m_base_mode, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("tex_mode", &m_tex_mode, ResourceParam::SEMANTIC_INT);
	return 0;
}

int32_t ImageSlideBarParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;

	ImageSlideBar *bar = (ImageSlideBar *)m_widget;

	bar->SetBaseMode((ImageSlideBar::BaseMode)m_base_mode);
	bar->SetTextureMode((ImageSlideBar::TextureMode)m_tex_mode);
	return 0;
}

int32_t ImageSlideBarParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret < 0)
		return ret;

	ImageSlideBar *bar = (ImageSlideBar *)m_widget;

	m_base_mode = bar->GetBaseMode();
	m_tex_mode = bar->GetTextureMode();
	return 0;
}

const char ImageSlideBarStyleParam::m_param_name[] = "style_image_slidebar";

const char *ImageSlideBarStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *ImageSlideBarStyleParam::ParamName()
{
	return m_param_name;
}

int32_t ImageSlideBarStyleParam::Create(int32_t mode)
{
	int32_t ret = WidgetStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("anim_time", &m_anim_time, ResourceParam::SEMANTIC_FLOAT);
	if (m_serial != NULL)
		m_serial->AddParam("anim_func", &m_anim_func, ResourceParam::SEMANTIC_INT);
	return 0;
}

int32_t ImageSlideBarStyleParam::SetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	((ImageSlideBar *)widget)->SetBarAnim(m_anim_time, m_anim_func);
	return 0;
}

int32_t ImageSlideBarStyleParam::GetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;
	((ImageSlideBar *)widget)->GetBarAnim(&m_anim_time, &m_anim_func);
	return 0;
}

const char ImageSlideBarSoundParam::m_param_name[] = "sound_image_slidebar";

const char *ImageSlideBarSoundParam::GetParamName() const
{
	return m_param_name;
}

const char *ImageSlideBarSoundParam::ParamName()
{
	return m_param_name;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewImageSlideBar(Widget *parent)
{
	return new ImageSlideBar(parent, NULL);
}

static WidgetParam *NewImageSlideBarParam()
{
	return new ImageSlideBarParam;
}

static VersatileParam *NewImageSlideBarLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewImageSlideBarFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewImageSlideBarStyleParam()
{
	return new ImageSlideBarStyleParam;
}

static VersatileParam *NewImageSlideBarHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewImageSlideBarSoundParam()
{
	return new ImageSlideBarSoundParam;
}

FactoryFuncTable g_factoryImageSlideBar = {
	ImageSlideBar::TypeName(),
	NewImageSlideBar,
	NewImageSlideBarParam,
	NewImageSlideBarLayoutParam,
	NewImageSlideBarFocusParam,
	NewImageSlideBarStyleParam,
	NewImageSlideBarHandlerParam,
	NewImageSlideBarSoundParam,
};
