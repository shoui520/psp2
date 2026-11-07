/*
 * paf::ui::SlideBar (DEBUGGER 0x81126504..0x81128d50): a slider over its base, placed as the
 * value is between 0 and the maximum, with a bar filled up to it and labels at the slider and
 * the ends. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <std/stdio.h>
#include <std/string.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/environment.h>
#include <widget/factory.h>
#include <widget/w_plane.h>
#include <widget/w_slidebar.h>
#include "../std/math.h"

namespace paf {
namespace ui {

/* the slider: a plane whose draw objects, the glow, take its plane's size (name ours) */
class SlideBarSlider : public Plane {
public:
	static const char m_widget_type[];

	SlideBarSlider(Widget *parent, CreateParam *param) : Plane(parent, param)
	{
	}

	virtual ~SlideBarSlider()
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void UpdateSize();
	/* up and down only while its focus directions are off */
	virtual int32_t GetFocusDirectionMask();
};

/* the two vectors multiplied lane by lane (name ours) */
static inline math::v4 MulPerElem(math::v4 a, math::v4 b)
{
	return math::v4(sce::Vectormath::Simd::Aos::mulPerElem(a.getImpl(), b.getImpl()));
}

/* the draw object shown or hidden, redrawn when it is shown (inlined in Sony's code, name ours) */
static inline void ShowDrawObj(graph::DrawObj *obj, bool show)
{
	if (obj->m_flags.m_visible != show) {
		obj->m_flags.m_visible = show;
		if (show && !graph::DrawObj::s_safety)
			obj->m_flags.m_dirty = graph::DrawObj::s_dirty_count;
	}
}

/* the widget's focus events turned on (inlined in Sony's code, name ours) */
static inline void EnableFocus(Widget *widget)
{
	if (!(widget->m_event_enable & 0x400))
		widget->EnableFocusEvent(true);
}

/* the base on the bar itself, the slider a widget of its own; no labels yet */
SlideBar::SlideBar(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_touch_mode(TOUCH_MODE_BAR), m_max_value(100), m_value(0),
	  m_slider_anim_time(150.0f), m_slider_anim_func(0), m_value_rate(0.0f), m_touch_id(-1),
	  m_key(-1), m_key_step(1), m_enabled(true), m_enable_color(NULL), m_disable_color(NULL)
{
	for (int32_t i = 0; i < 3; i++) {
		m_label_mode[i] = LABEL_MODE_VALUE;
		m_label_pos_mode[i] = LABEL_POS_MODE_SIDE;
		m_label_margin[i] = 5.0f;
	}
	EnableFocus(this);
	m_hit_find_mode = 0;
	m_event_enable |= 0xFFF0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_pre_dispatch = true;
	m_hit_check = 0;
	/* it plays its two sounds */
	m_sound_mask = 3;
	AddDrawObj(DrawObjEntry(new graph::PlaneObj(graph::PlaneObjStyle()), OBJ_BASE), false);

	CreateParam create;

	create.m_parent_protected = true;

	Widget *slider = new SlideBarSlider(this, &create);

	if (slider != NULL) {
		EnableFocus(slider);
		slider->m_hit_check = 3;
		slider->m_event_enable |= 0x14C0;
		slider->SetDragMode(0x120);
		slider->AddEventListener(0x2000D, new EventCBListener(OnDragSliderEvent, this), false);
		slider->AddEventListener(0x2000E, new EventCBListener(OnDragSliderEvent, this), false);
		slider->AddEventListener(0x2000F, new EventCBListener(OnDragSliderEvent, this), false);
	}
	m_parts.resize(4);
	m_parts[0] = slider;
	m_parts[1 + LABEL_ID_SLIDER] = NULL;
	m_parts[1 + LABEL_ID_MIN] = NULL;
	m_parts[1 + LABEL_ID_MAX] = NULL;
}

const SlideBarParam::TextureRef SlideBarParam::c_textureList[] = {
	{ "texture_bar", SlideBar::OBJ_BAR },
	{ "texture_slider", SlideBar::OBJ_SLIDER },
};

SlideBarParam::SlideBarParam()
{
	for (uint32_t i = 0; i < sizeof(c_textureList) / sizeof(c_textureList[0]); i++)
		m_texture_list.push_back(IndexedRef(c_textureList[i].name, c_textureList[i].index));
	m_touch_mode = SlideBar::TOUCH_MODE_BAR;
	m_slider_size = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	for (int32_t i = 0; i < 3; i++) {
		m_label_mode[i] = SlideBar::LABEL_MODE_VALUE;
		m_label_pos_mode[i] = SlideBar::LABEL_POS_MODE_SIDE;
		m_label_margin[i] = 5.0f;
	}
	m_label[SlideBar::LABEL_ID_SLIDER].m_idhash = 0;
	m_label[SlideBar::LABEL_ID_MIN].m_idhash = 0;
	m_label[SlideBar::LABEL_ID_MAX].m_idhash = 0;
}

SlideBarStyleParam::SlideBarStyleParam()
{
	m_anim_time = 150.0f;
	m_anim_func = 0;
	m_obj[SlideBar::OBJ_BASE].m_name = "base_obj";
	m_obj[SlideBar::OBJ_BASE].m_index = SlideBar::OBJ_BASE;
	m_drawobj_param_list.push_back(&m_obj[SlideBar::OBJ_BASE]);
	m_obj[SlideBar::OBJ_BAR].m_name = "bar_obj";
	m_obj[SlideBar::OBJ_BAR].m_index = SlideBar::OBJ_BAR;
	m_drawobj_param_list.push_back(&m_obj[SlideBar::OBJ_BAR]);
	m_obj[SlideBar::OBJ_SLIDER].m_name = "slider_obj";
	m_obj[SlideBar::OBJ_SLIDER].m_index = SlideBar::OBJ_SLIDER;
	m_drawobj_param_list.push_back(&m_obj[SlideBar::OBJ_SLIDER]);
	m_obj[SlideBar::OBJ_SLIDER_GLOW].m_name = "slider_glow_obj";
	m_obj[SlideBar::OBJ_SLIDER_GLOW].m_index = SlideBar::OBJ_SLIDER_GLOW;
	m_drawobj_param_list.push_back(&m_obj[SlideBar::OBJ_SLIDER_GLOW]);
}

/* the sounds its sound parameter names, by index */
static const struct {
	const char *name;
	uint32_t index;
} s_slidebar_sounds[] = {
	{ "drag_begin", SlideBar::SOUND_DRAG_BEGIN },
	{ "drag_end", SlideBar::SOUND_DRAG_END },
};

SlideBarSoundParam::SlideBarSoundParam()
{
	for (uint32_t i = 0; i < 2; i++)
		m_sound_list.push_back(Sound(s_slidebar_sounds[i].name, s_slidebar_sounds[i].index));
}

const char SlideBar::m_widget_type[] = "slidebar";

bool SlideBar::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *SlideBar::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *SlideBar::TypeName()
{
	return m_widget_type;
}

SlideBar::~SlideBar()
{
	if (m_enable_color != NULL)
		delete m_enable_color;
	m_enable_color = (math::v4 *)-1;
	if (m_disable_color != NULL)
		delete m_disable_color;
	m_disable_color = (math::v4 *)-1;
}

graph::DrawObj *SlideBar::GetDrawObj(int32_t obj)
{
	int32_t child;
	Widget *widget = GetPartsWidget(obj, &child);

	if (widget == NULL)
		return NULL;
	if (widget == this)
		return Widget::GetDrawObj(obj);
	return widget->GetDrawObj(child);
}

common::SharedPtr<graph::DrawObj> SlideBar::SwapDrawObj(common::SharedPtr<graph::DrawObj> obj,
							int32_t id)
{
	int32_t child;
	Widget *widget = GetPartsWidget(id, &child);

	if (widget == NULL)
		return common::SharedPtr<graph::DrawObj>();
	return widget->SwapDrawObj(obj, child);
}

void SlideBar::CreateBar()
{
	if (GetDrawObj(OBJ_BAR) != NULL)
		return;

	graph::DrawObj *bar = new graph::PlaneObj(graph::PlaneObjStyle());

	if (bar == NULL)
		return;

	common::SharedPtr<graph::DrawObj> ptr(bar);
	vector<DrawObjEntry>::iterator it = FindDrawObj(OBJ_BASE);
	DrawObjAttr attr(OBJ_BAR);

	attr.m_flag1 = false;
	/* over the base */
	if (it == m_drawobjs.end())
		InsertDrawObj(m_drawobjs.begin(), DrawObjEntry(ptr, attr), false);
	else
		InsertDrawObj(++it, DrawObjEntry(ptr, attr), false);
	UpdateSlider(false);
}

int32_t SlideBar::SetObjColor(const math::v4 &color, int32_t obj)
{
	if (obj == OBJ_BAR)
		CreateBar();

	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return -1;
	draw->SetColor(color);
	return 0;
}

int32_t SlideBar::GetObjColor(math::v4 &color, int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return -1;
	color = draw->GetColor();
	return 0;
}

int32_t SlideBar::SetObjSize(const math::v4 &size, int32_t obj)
{
	Widget *widget = GetPartsWidget(obj, NULL);

	if (widget == NULL)
		return -1;

	int32_t ret = widget->SetSize(size);

	if (obj == OBJ_SLIDER)
		UpdateLabelPos(LABEL_ID_SLIDER);
	return ret;
}

int32_t SlideBar::GetObjSize(math::v4 &size, int32_t obj)
{
	Widget *widget = GetPartsWidget(obj, NULL);

	if (widget == NULL)
		return -1;
	size = *widget->GetSize(0);
	return 0;
}

int32_t SlideBar::SetTouchMode(TouchMode mode)
{
	if ((int32_t)mode < 0 || (int32_t)mode > TOUCH_MODE_SLIDER)
		return 0x80AF008E;
	m_touch_mode = mode;
	return 0;
}

int32_t SlideBar::SetMaxValue(int32_t value)
{
	if (value <= 0)
		return 0x80AF008E;
	m_max_value = value;
	SetValue(m_value, false);
	UpdateLabel(LABEL_ID_MAX);
	return 0;
}

int32_t SlideBar::SetValue(int32_t value, bool anim)
{
	m_value = value;
	AdjustParameter();
	UpdateSlider(anim);
	return 0;
}

int32_t SlideBar::SetSliderAnim(float time, int32_t func)
{
	if (time <= 0.0f)
		return 0x80AF008E;
	m_slider_anim_time = time;
	m_slider_anim_func = func;
	return 0;
}

int32_t SlideBar::AttachLabelWidget(Widget *label, LabelId id)
{
	if (label == NULL || (int32_t)id < 0 || (int32_t)id > LABEL_ID_MAX)
		return 0x80AF008E;
	m_parts[1 + id] = label;
	if (id == LABEL_ID_SLIDER) {
		Widget *slider = m_parts[0];

		if (slider != NULL)
			label->SetParent(slider, NULL, 0);
	}
	UpdateLabel(id);
	UpdateLabelPos(id);
	return 0;
}

int32_t SlideBar::DetachLabelWidget(LabelId id)
{
	if ((int32_t)id < 0 || (int32_t)id > LABEL_ID_MAX)
		return 0x80AF008E;
	m_parts[1 + id] = NULL;
	return 0;
}

Widget *SlideBar::GetLabelWidget(LabelId id)
{
	if ((int32_t)id < 0 || (int32_t)id > LABEL_ID_MAX)
		return NULL;
	return m_parts[1 + id];
}

int32_t SlideBar::SetLabelMode(LabelMode mode, LabelId id)
{
	if ((int32_t)mode < 0 || (int32_t)mode > LABEL_MODE_EVENT || (int32_t)id < 0 ||
	    (int32_t)id > LABEL_ID_MAX)
		return 0x80AF008E;
	m_label_mode[id] = mode;
	UpdateLabel(id);
	return 0;
}

int32_t SlideBar::GetLabelMode(LabelMode &mode, LabelId id)
{
	if ((int32_t)id < 0 || (int32_t)id > LABEL_ID_MAX)
		return 0x80AF008E;
	mode = m_label_mode[id];
	return 0;
}

int32_t SlideBar::SetLabelPosMode(LabelPosMode mode, LabelId id)
{
	if ((int32_t)mode < 0 || (int32_t)mode > LABEL_POS_MODE_NONE || (int32_t)id < 0 ||
	    (int32_t)id > LABEL_ID_MAX)
		return 0x80AF008E;
	m_label_pos_mode[id] = mode;
	UpdateLabelPos(id);
	return 0;
}

int32_t SlideBar::GetLabelPosMode(LabelPosMode &mode, LabelId id)
{
	if ((int32_t)id < 0 || (int32_t)id > LABEL_ID_MAX)
		return 0x80AF008E;
	mode = m_label_pos_mode[id];
	return 0;
}

int32_t SlideBar::SetLabelMargin(float margin, LabelId id)
{
	if ((int32_t)id < 0 || (int32_t)id > LABEL_ID_MAX)
		return 0x80AF008E;
	m_label_margin[id] = margin;
	UpdateLabelPos(id);
	return 0;
}

int32_t SlideBar::GetLabelMargin(float &margin, LabelId id)
{
	if ((int32_t)id < 0 || (int32_t)id > LABEL_ID_MAX)
		return 0x80AF008E;
	margin = m_label_margin[id];
	return 0;
}

int32_t SlideBar::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
			     uint32_t index)
{
	int32_t child;
	Widget *widget = GetPartsWidget(obj, &child);

	if (widget == NULL)
		return -1;
	if (widget != this)
		return widget->SetTexture(surf, child, index);
	if (surf.get() != NULL && obj == OBJ_BAR)
		CreateBar();

	graph::DrawObj *draw = Widget::GetDrawObj(obj);

	if (draw == NULL)
		return 0x80AF4101;
	draw->SetTexture(surf, index);
	return 0;
}

void SlideBar::AddSound(uint32_t index, WidgetSoundParam::Sound &sound)
{
	if (index < SOUND_SLIDEBAR_END) {
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

WidgetSoundParam::Sound *SlideBar::GetSound(int32_t id)
{
	for (list<WidgetSoundParam::Sound>::iterator it = m_sound_list.begin();
	     it != m_sound_list.end(); ++it) {
		if (it->m_index == (uint32_t)id)
			return &*it;
	}
	return FindSound(id);
}

void SlideBar::CreateSliderGlow()
{
	if (GetDrawObj(OBJ_SLIDER_GLOW) != NULL)
		return;

	Widget *slider = m_parts[0];

	if (slider == NULL)
		return;

	graph::PlaneObj *glow = new graph::PlaneObj(graph::PlaneObjStyle());

	{
		common::SharedPtr<graph::PlaneObj> ptr(glow);

		slider->AddDrawObj(DrawObjEntry(ptr, OBJ_SLIDER_GLOW), false);
	}
	/* shown while the slider holds a touch */
	ShowGlow(OBJ_SLIDER_GLOW, slider->m_touch_captures.size() != 0);
}

uint32_t SlideBar::CheckState()
{
	if (m_hold_count > 0)
		return m_state;
	if (IsPrepareObjs())
		return 3;
	for (vector<Widget *>::iterator it = m_parts.begin(); it != m_parts.end(); ++it) {
		Widget *part = *it;

		if (part != NULL && part != this && part->m_state == 3)
			return 3;
	}
	return 0;
}

void SlideBar::UpdateSize()
{
	Widget::UpdateSize();
	UpdateSlider(false);
	UpdateLabelPos(LABEL_ID_MIN);
	UpdateLabelPos(LABEL_ID_MAX);
}

int32_t SlideBar::HandlePointEvent(Event *e)
{
	if (!m_enabled)
		return -1;
	OnTouch(e);
	return Handler::HandlePointEvent(e);
}

int32_t SlideBar::HandleTouchEvent(Event *e)
{
	if (!m_enabled)
		return -1;
	OnTouch(e);
	return Widget::HandleTouchEvent(e);
}

/* the left and right keys move the value by the key step, as a drag of the slider */
int32_t SlideBar::HandleKeycodeEvent(Event *e)
{
	int32_t key = (uint8_t)e->m_detail;

	if (key != 3 && key != 4)
		return Widget::HandleKeycodeEvent(e);
	if (!m_enabled) {
		e->Consume();
		return -1;
	}

	int32_t step = m_key_step;

	if (step == 0)
		step = 1;

	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

	ev.m_data[0] = 1;
	switch (e->GetType()) {
	case 0x11:
	case 0x13: {
		int32_t value = m_value;
		int32_t next;

		if (key == 3) {
			next = value - step;
			if (next <= 0)
				next = 0;
		} else {
			next = value + step;
			if (next >= m_max_value)
				next = m_max_value;
		}
		if (next != value) {
			SetValue(next, true);
			if (m_key < 0) {
				m_key = key;
				ShowGlow(OBJ_SLIDER_GLOW, true);
				DoEvent(CB_SLIDEBAR_DRAG_BEGIN, &ev);
			}
			DoEvent(CB_SLIDEBAR_CHANGED, &ev);
		}
		if (m_key >= 0)
			e->Consume();
		break;
	}
	case 0x14:
		/* cancelled */
		ev.m_data[1] = 1;
		/* fall through */
	case 0x12:
		if (m_key >= 0) {
			m_key = -1;
			ShowGlow(OBJ_SLIDER_GLOW, false);
			DoEvent(CB_SLIDEBAR_DRAG_END, &ev);
			e->Consume();
		}
		break;
	}
	return Widget::HandleKeycodeEvent(e);
}

int32_t SlideBar::HandleFocusEvent(Event *e)
{
	switch (e->GetType()) {
	case 0x401: {
		Widget *slider = GetPartsWidget(OBJ_SLIDER, NULL);

		if (slider != NULL) {
			slider->SetFocus(NULL);
			e->Consume();
		}
		break;
	}
	case 0x404:
		if (m_key >= 0) {
			m_key = -1;
			ShowGlow(OBJ_SLIDER_GLOW, false);

			Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

			ev.m_data[0] = 1;
			ev.m_data[1] = 1;
			DoEvent(CB_SLIDEBAR_DRAG_END, &ev);
			e->Consume();
		}
		break;
	}
	return Handler::HandleFocusEvent(e);
}

void SlideBar::EnableFocusEvent(bool on_off)
{
	if (m_parts.size() != 0) {
		Widget *slider = GetPartsWidget(OBJ_SLIDER, NULL);

		if (slider != NULL)
			slider->EnableFocusEvent(on_off);
	}
	Widget::EnableFocusEvent(on_off);
}

void SlideBar::Enable()
{
	if (m_enabled)
		return;
	m_enabled = true;
	SetColor(*m_enable_color);
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_hit_find_mode = 0;
}

void SlideBar::Disable()
{
	if (!m_enabled)
		return;
	m_enabled = false;
	ScePafWidget_931AD41E(this, false);
	ReleasePadCaptureDir(255, false);
	ReleasePadCaptureButton(255, false);
	if (m_enable_color != NULL)
		*m_enable_color = GetColor();
	else
		m_enable_color = new math::v4(GetColor());

	math::v4 color = m_disable_color != NULL ? *m_disable_color
						 : math::v4(0.8f, 0.8f, 0.8f, 0.3f);

	SetColor(color);
	m_hit_detect_b_mode = 3;
	m_hit_detect_mode = 3;
	m_hit_find_mode = 1;
}

Widget *SlideBar::GetPartsWidget(int32_t obj, int32_t *child_obj)
{
	int32_t index = -1;
	int32_t child = 0;

	switch (obj) {
	case OBJ_BASE:
	case OBJ_BAR:
		child = obj;
		break;
	case OBJ_SLIDER:
		index = 0;
		break;
	case OBJ_SLIDER_GLOW:
		index = 0;
		child = obj;
		break;
	default:
		return NULL;
	}
	if (child_obj != NULL)
		*child_obj = child;
	if (index >= 0)
		return m_parts[index];
	return this;
}

void SlideBar::ShowGlow(int32_t obj, bool show)
{
	graph::DrawObj *glow = GetDrawObj(obj);

	if (glow != NULL)
		ShowDrawObj(glow, show);
}

int32_t SlideBar::SetColor_core(const math::v4 &v)
{
	int32_t ret = Widget::SetColor_core(v);

	for (vector<Widget *>::iterator it = m_parts.begin(); it != m_parts.end(); ++it) {
		Widget *part = *it;

		if (part != NULL && part != this)
			part->SetColor(v);
	}
	return ret;
}

void SlideBar::AdjustParameter()
{
	int32_t value = m_value < 0 ? 0 : m_value;
	int32_t max = m_max_value;

	if (max > value)
		max = value;
	m_value = max;
	m_value_rate = (float)max / (float)m_max_value;
	UpdateLabel(LABEL_ID_SLIDER);
}

void SlideBar::UpdateSlider(bool anim)
{
	Widget *slider = m_parts[0];
	graph::DrawObj *bar = GetDrawObj(OBJ_BAR);

	if (slider != NULL) {
		const math::v4 *slider_size = slider->GetSize(0);
		math::v2 focus_size(slider_size->extract_x(), slider_size->extract_y());

		slider->KillAnim(ANIM_MOVE);

		math::v4 size = m_size;
		math::v4 origin = m_origin_offs;
		math::v4 half = MulPerElem(math::v4(0.5f, 0.0f, 0.0f, 0.0f), size);
		math::v4 pos = origin + MulPerElem(size, math::v4(m_value_rate - 0.5f, 0.0f, 0.0f, 0.0f));

		/* dragged only along the bar */
		slider->SetDragLimitMin(origin - half);
		slider->SetDragLimitMax(m_origin_offs + half);
		if (anim)
			slider->SetPos(pos, new Timer(m_slider_anim_time, m_slider_anim_func, 0, -1, NULL),
				       ANIM_MOVE, 2);
		else
			slider->SetPos(pos);

		/* the focus cursor a frame around the slider */
		FocusShape shape;

		shape.m_shape = 4;
		shape.m_size = focus_size;
		ScePafWidget_13B246E0(slider, &shape);
	}
	if (bar != NULL) {
		KillObjAnim(OBJ_BAR, 0x20100);
		KillObjAnim(OBJ_BAR, 0x20200);

		math::v4 size = m_size;
		math::v4 bar_size = MulPerElem(size, math::v4(m_value_rate, 1.0f, 1.0f, 1.0f));
		math::v4 bar_pos = MulPerElem(math::v4(-size.getImpl()), math::v4(0.5f, 0.0f, 0.0f, 0.0f));

		/* from the start of the base */
		bar_pos.set_x(bar_pos.extract_x() + 0.5f * bar_size.extract_x());
		if (anim) {
			SetObjAnim(OBJ_BAR, 0x20100, bar_pos,
				   new Timer(m_slider_anim_time, m_slider_anim_func, 0, -1, NULL), 2);
			SetObjAnim(OBJ_BAR, 0x20200, bar_size,
				   new Timer(m_slider_anim_time, m_slider_anim_func, 0, -1, NULL), 2);
		} else {
			bar->SetPos(math::v3(bar_pos.getImpl().getXYZ()));
			bar->SetSize(math::v3(bar_size.getImpl().getXYZ()));
		}
	}
	UpdateLabelPos(LABEL_ID_SLIDER);
}

void SlideBar::UpdateLabel(LabelId id)
{
	Widget *label = m_parts[1 + id];

	if (label == NULL)
		return;

	LabelMode mode = m_label_mode[id];

	if (mode == LABEL_MODE_VALUE) {
		int32_t value = 0;

		switch (id) {
		case LABEL_ID_SLIDER:
			value = m_value;
			break;
		case LABEL_ID_MAX:
			value = m_max_value;
			break;
		default:
			break;
		}

		wchar_t buffer[32];

		sce_paf_swprintf(buffer, 32, L"%d", value);
		label->SetString(wstring(buffer));
	} else if (mode == LABEL_MODE_EVENT) {
		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		ev.m_data[0] = id;
		DoEvent(CB_SLIDEBAR_LABEL, &ev);
	}
}

void SlideBar::UpdateLabelPos(LabelId id)
{
	Widget *label = m_parts[1 + id];

	if (label == NULL)
		return;

	LabelPosMode mode = m_label_pos_mode[id];

	if (mode == LABEL_POS_MODE_NONE)
		return;

	math::v4 dir(0.0f, 0.0f, 0.0f, 0.0f);
	float margin = m_label_margin[id];
	math::v4 size(0.0f, 0.0f, 0.0f, 0.0f);
	math::v4 offs(0.0f, 0.0f, 0.0f, 0.0f);
	math::v4 base;
	uint32_t align_x;
	uint32_t align_y;

	switch (id) {
	case LABEL_ID_MIN:
		base = m_origin_offs;
		align_x = 1;
		dir = math::v4(-0.5f, 0.0f, 0.0f, 0.0f);
		size = m_size;
		break;
	case LABEL_ID_MAX:
		base = m_origin_offs;
		align_x = 2;
		dir = math::v4(0.5f, 0.0f, 0.0f, 0.0f);
		size = m_size;
		break;
	default:
		/* on the slider */
		base = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
		GetObjSize(size, OBJ_SLIDER);
		align_x = 0;
		break;
	}
	switch (mode) {
	case LABEL_POS_MODE_SIDE:
		align_y = 0;
		switch (id) {
		case LABEL_ID_MIN:
			offs = math::v4(-1.0f, 0.0f, 0.0f, 0.0f);
			align_x = 2;
			break;
		case LABEL_ID_MAX:
			offs = math::v4(1.0f, 0.0f, 0.0f, 0.0f);
			align_x = 1;
			break;
		default:
			break;
		}
		break;
	case LABEL_POS_MODE_BELOW:
		align_y = 2;
		dir.set_y(-0.5f);
		offs = math::v4(0.0f, -1.0f, 0.0f, 0.0f);
		break;
	default:
		align_y = 1;
		dir.set_y(0.5f);
		offs = math::v4(0.0f, 1.0f, 0.0f, 0.0f);
		break;
	}
	label->SetPos(base + MulPerElem(dir, size) + offs * margin);
	label->SetAlign(align_x, align_y, 0, NULL);
}

void SlideBar::OnTouch(Event *e)
{
	if (m_touch_mode == TOUCH_MODE_SLIDER || m_touch_id >= 0)
		return;
	switch (e->GetType()) {
	case 0x21:
	case 0x41:
		break;
	default:
		return;
	}

	Widget *slider = m_parts[0];

	if (slider != NULL)
		ScePafWidget_D3D0A7C3(slider, (e->m_detail >> 20) & 0xF, (uint8_t)e->m_detail, true,
				      true);
	e->Consume();
}

void SlideBar::OnDragSliderEvent(int32_t type, Handler *self, Event *e, void *data)
{
	SlideBar *bar = (SlideBar *)data;

	switch (type) {
	case 0x2000D:
		bar->OnDragBeginSlider(e);
		break;
	case 0x2000E:
		bar->OnDragMoveSlider(e);
		break;
	case 0x2000F:
		bar->OnDragEndSlider(e, (e->m_detail & 0x8000) != 0);
		break;
	}
}

/* the slider taken to the touch on the bar, when the touch is not on the slider */
void SlideBar::OnDragBeginSlider(Event *e)
{
	uint32_t id = (uint8_t)e->m_detail;
	Widget *slider = m_parts[0];

	m_touch_id = id;
	if (slider != NULL) {
		Environment *env = m_env;
		HitInfo *hit = NULL;

		if (id < 8)
			hit = &env->m_touch_hit[id];
		if ((hit == NULL || hit->m_widget != slider) && id < 8) {
			env->m_touch_state[id].m_state = 0;
			env->m_touch_state[id].m_id = id;

			TouchState *touch = &env->m_touch_state[id];

			if (touch != NULL) {
				HitInfo info;

				CalcHitLineToPlane(touch->m_line_start, touch->m_line_end,
						   math::v4(0.0f, 0.0f, 0.0f, 0.0f),
						   math::v4(0.0f, 0.0f, 1.0f, 0.0f), &info);

				SceFVector3 *limit = slider->m_drag_limit;
				math::v4 min(0.0f, 0.0f, 0.0f, 0.0f);

				if (limit != NULL)
					min = math::v4(limit[0].x, limit[0].y, limit[0].z, 0.0f);

				math::v4 max(0.0f, 0.0f, 0.0f, 0.0f);

				if (limit != NULL)
					max = math::v4(limit[1].x, limit[1].y, limit[1].z, 0.0f);

				math::v4 pos = slider->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

				pos.set_x(info.m_touch_pos.extract_x() + m_origin_offs.extract_x());
				pos = math::v4(sce::Vectormath::Simd::Aos::maxPerElem(sce::Vectormath::Simd::Aos::minPerElem(pos.getImpl(), max.getImpl()), min.getImpl()));
				slider->SetPos_core(pos, 0xFF01, false);
			}
		}
	}
	ScePafWidget_2B1A1493(this, SOUND_DRAG_BEGIN);
	ShowGlow(OBJ_SLIDER_GLOW, true);

	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

	DoEvent(CB_SLIDEBAR_DRAG_BEGIN, &ev);
	OnDragSlider();
}

void SlideBar::OnDragMoveSlider(Event *e)
{
	OnDragSlider();
}

void SlideBar::OnDragEndSlider(Event *e, bool cancel)
{
	m_touch_id = -1;
	UpdateSlider(true);
	ScePafWidget_2B1A1493(this, SOUND_DRAG_END);
	ShowGlow(OBJ_SLIDER_GLOW, false);

	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

	ev.m_data[1] = cancel;
	DoEvent(CB_SLIDEBAR_DRAG_END, &ev);
}

void SlideBar::OnDragSlider()
{
	Widget *slider = m_parts[0];

	if (slider == NULL)
		return;

	math::v4 pos = slider->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)) - m_origin_offs;

	slider->GetSize(0);

	graph::DrawObj *bar = GetDrawObj(OBJ_BAR);
	math::v4 size = m_size;

	if (bar != NULL) {
		/* filled up to the slider */
		float width = pos.extract_x() + 0.5f * size.extract_x();
		math::v4 bar_pos = MulPerElem(math::v4(-size.getImpl()), math::v4(0.5f, 0.0f, 0.0f, 0.0f));
		math::v4 bar_size = size;

		bar_pos.set_x(bar_pos.extract_x() + width * 0.5f);
		bar->SetPos(math::v3(bar_pos.getImpl().getXYZ()));
		bar_size.set_x(width);
		bar->SetSize(math::v3(bar_size.getImpl().getXYZ()));
		size = m_size;
	}

	float rate = 0.0f;

	if (size.extract_x() >= 1.0f)
		rate = pos.extract_x() / size.extract_x() + 0.5f;

	int32_t old = m_value;

	m_value = (int32_t)sce_paf_roundf(rate * (float)m_max_value);
	AdjustParameter();
	if (m_value != old) {
		Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		DoEvent(CB_SLIDEBAR_CHANGED, &ev);
	}
}

const char SlideBarParam::m_param_name[] = "slidebar_base";

const char *SlideBarParam::GetParamName() const
{
	return m_param_name;
}

const char *SlideBarParam::ParamName()
{
	return m_param_name;
}

/* each label's parameters' names, by its LabelId */
static const struct {
	const char *widget;
	const char *mode;
	const char *pos_mode;
	const char *margin;
} s_slidebar_label_params[] = {
	{ "w_slider_label", "slider_label_mode", "slider_label_pos_mode", "slider_label_margin" },
	{ "w_min_label", "min_label_mode", "min_label_pos_mode", "min_label_margin" },
	{ "w_max_label", "max_label_mode", "max_label_pos_mode", "max_label_margin" },
};

int32_t SlideBarParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL) {
		m_serial->AddParam("touch_mode", &m_touch_mode, ResourceParam::SEMANTIC_INT);
		if (m_serial != NULL)
			m_serial->AddParam("slider_size", &m_slider_size,
					   ResourceParam::SEMANTIC_VEC3);
	}
	for (int32_t i = 0; i < 3; i++) {
		if (m_serial != NULL) {
			m_serial->AddParam(s_slidebar_label_params[i].widget, &m_label[i],
					   ResourceParam::SEMANTIC_WIDGETREF);
			if (m_serial != NULL) {
				m_serial->AddParam(s_slidebar_label_params[i].mode, &m_label_mode[i],
						   ResourceParam::SEMANTIC_INT);
				if (m_serial != NULL) {
					m_serial->AddParam(s_slidebar_label_params[i].pos_mode,
							   &m_label_pos_mode[i],
							   ResourceParam::SEMANTIC_INT);
					if (m_serial != NULL)
						m_serial->AddParam(s_slidebar_label_params[i].margin,
								   &m_label_margin[i],
								   ResourceParam::SEMANTIC_FLOAT);
				}
			}
		}
	}
	return 0;
}

int32_t SlideBarParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;

	SlideBar *bar = (SlideBar *)m_widget;

	bar->SetTouchMode((SlideBar::TouchMode)m_touch_mode);
	bar->SetObjSize(math::v4(math::v3(m_slider_size.getImpl().getXYZ()), 0.0f),
			SlideBar::OBJ_SLIDER);
	for (int32_t i = 0; i < 3; i++) {
		bar->SetLabelMode((SlideBar::LabelMode)m_label_mode[i], (SlideBar::LabelId)i);
		bar->SetLabelPosMode((SlideBar::LabelPosMode)m_label_pos_mode[i],
				     (SlideBar::LabelId)i);
		bar->SetLabelMargin(m_label_margin[i], (SlideBar::LabelId)i);
	}
	for (int32_t i = 0; i < 3; i++) {
		if (m_label[i].GetIDHash() != 0) {
			Widget *label = bar->FindChild(m_label[i], 0);

			if (label != NULL)
				bar->AttachLabelWidget(label, (SlideBar::LabelId)i);
		}
	}
	return 0;
}

int32_t SlideBarParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret < 0)
		return ret;

	SlideBar *bar = (SlideBar *)m_widget;
	math::v4 size;

	m_touch_mode = bar->m_touch_mode;
	if (bar->GetObjSize(size, SlideBar::OBJ_SLIDER) >= 0)
		m_slider_size = size;
	for (int32_t i = 0; i < 3; i++) {
		SlideBar::LabelMode label_mode = SlideBar::LABEL_MODE_VALUE;

		bar->GetLabelMode(label_mode, (SlideBar::LabelId)i);
		m_label_mode[i] = label_mode;

		SlideBar::LabelPosMode pos_mode = SlideBar::LABEL_POS_MODE_SIDE;

		bar->GetLabelPosMode(pos_mode, (SlideBar::LabelId)i);
		m_label_pos_mode[i] = pos_mode;
		bar->GetLabelMargin(m_label_margin[i], (SlideBar::LabelId)i);
	}
	return 0;
}

const char SlideBarStyleParam::m_param_name[] = "style_slidebar";

const char *SlideBarStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *SlideBarStyleParam::ParamName()
{
	return m_param_name;
}

int32_t SlideBarStyleParam::Create(int32_t mode)
{
	int32_t ret = WidgetStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL) {
		m_serial->AddParam("anim_time", &m_anim_time, ResourceParam::SEMANTIC_FLOAT);
		if (m_serial != NULL)
			m_serial->AddParam("anim_func", &m_anim_func, ResourceParam::SEMANTIC_INT);
	}
	return 0;
}

/* the bar and the glow made where the style names them */
int32_t SlideBarStyleParam::SetEach(Widget *widget)
{
	SlideBar *bar = (SlideBar *)widget;
	graph::DrawObjParam *fill = GetDrawObjParam(SlideBar::OBJ_BAR);

	if (fill != NULL && fill->m_id.GetIDHash() != 0)
		bar->CreateBar();

	graph::DrawObjParam *glow = GetDrawObjParam(SlideBar::OBJ_SLIDER_GLOW);

	if (glow != NULL && glow->m_id.GetIDHash() != 0)
		bar->CreateSliderGlow();

	int32_t ret = WidgetStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	bar->SetSliderAnim(m_anim_time, m_anim_func);
	return 0;
}

int32_t SlideBarStyleParam::GetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;
	((SlideBar *)widget)->GetSliderAnim(&m_anim_time, &m_anim_func);
	return 0;
}

const char SlideBarSoundParam::m_param_name[] = "sound_slidebar";

const char *SlideBarSoundParam::GetParamName() const
{
	return m_param_name;
}

const char *SlideBarSoundParam::ParamName()
{
	return m_param_name;
}

const char SlideBarSlider::m_widget_type[] = "slider";

bool SlideBarSlider::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Plane::IsInherit(type);
}

char *SlideBarSlider::GetTypeName() const
{
	return (char *)m_widget_type;
}

int32_t SlideBarSlider::GetFocusDirectionMask()
{
	return m_focus_direction_off ? 2 : 7;
}

/* its glow sized to its plane */
void SlideBarSlider::UpdateSize()
{
	Plane::UpdateSize();

	math::v3 size = m_plane->m_size;

	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id != 0 && it->m_attr.m_flag1)
			it->m_obj->SetSize(size);
	}
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewSlideBar(Widget *parent)
{
	return new SlideBar(parent, NULL);
}

static WidgetParam *NewSlideBarParam()
{
	return new SlideBarParam;
}

static VersatileParam *NewSlideBarLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewSlideBarFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewSlideBarStyleParam()
{
	return new SlideBarStyleParam;
}

static VersatileParam *NewSlideBarHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewSlideBarSoundParam()
{
	return new SlideBarSoundParam;
}

FactoryFuncTable g_factorySlideBar = {
	SlideBar::TypeName(),
	NewSlideBar,
	NewSlideBarParam,
	NewSlideBarLayoutParam,
	NewSlideBarFocusParam,
	NewSlideBarStyleParam,
	NewSlideBarHandlerParam,
	NewSlideBarSoundParam,
};
