/*
 * paf::ui::ProgressBarTouch (TOOL 0x81109c78..0x8110b81c): a ProgressBar with a slider on the
 * fore bar's end, dragged by touch or moved by the left and right keys to set the value, with a
 * label on the slider. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/draw_obj.h>
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
#include <widget/w_progress_bar.h>

namespace paf {
namespace ui {

/* the slider: a plane whose draw objects, the glow, take its plane's size (name ours) */
class ProgressBarTouchSlider : public Plane {
public:
	static const char m_widget_type[];

	ProgressBarTouchSlider(Widget *parent, CreateParam *param) : Plane(parent, param)
	{
	}

	virtual ~ProgressBarTouchSlider()
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void UpdateSize();
	/* up and down only while its focus directions are off */
	virtual int32_t GetFocusDirectionMask();
};

const char ProgressBarTouch::m_widget_type[] = "progressbar_touch";
const char ProgressBarTouchParam::m_param_name[] = "progressbar_touch_base";

const ProgressBarTouchParam::TextureRef ProgressBarTouchParam::c_textureList[] = {
	{ "texture_slider", ProgressBarTouch::OBJ_SLIDER },
};

const char ProgressBarTouchStyleParam::m_param_name[] = "style_progressbar_touch";
const char ProgressBarTouchSoundParam::m_param_name[] = "sound_progressbar_touch";

/* the sounds its sound parameter names, by index */
struct SoundDef {
	const char *name;
	uint32_t index;
};

extern const SoundDef s_progressbar_touch_sounds[] = {
	{ "drag_begin", ProgressBarTouch::SOUND_DRAG_BEGIN },
	{ "drag_end", ProgressBarTouch::SOUND_DRAG_END },
};

const char ProgressBarTouchSlider::m_widget_type[] = "slider";

/* the two vectors multiplied lane by lane (name ours) */
static inline math::v4 MulPerElem(math::v4 a, math::v4 b)
{
	return math::v4(sce::Vectormath::Simd::Aos::mulPerElem(a.getImpl(), b.getImpl()));
}

/* the vector's x and y, z and w 0 (name ours) */
static inline math::v4 V4(math::v2 xy)
{
	return math::v4(xy.extract_x(), xy.extract_y(), 0.0f, 0.0f);
}

/* a float in an event's data word (name ours) */
static inline void SetEventFloat(Event &ev, int32_t index, float value)
{
	union {
		float f;
		int32_t i;
	} bits;

	bits.f = value;
	ev.m_data[index] = bits.i;
}

/* the slider's label shown, and hidden (inlined in Sony's code, names ours) */
static inline void ShowLabelSlider(Widget *label)
{
	if (!label->m_transition_complete) {
		label->m_transition = 0xff;
		label->m_transition_complete = true;
		label->DoTransition_core(0.0f, true);
	}
	ScePafWidget_FF5BC957(label, NULL, 2, 0.0f, NULL);
}

static inline void HideLabelSlider(Widget *label)
{
	label->m_transition_complete = false;
	label->m_transition = 0xff;
	label->DoTransitionReverse_core(0.0f, true);
	ScePafWidget_4588699E(label, NULL, 2, 0.0f, NULL);
}

/* the slider a widget of its own, taking the touches; no label on it yet */
ProgressBarTouch::ProgressBarTouch(Widget *parent, CreateParam *param)
	: ProgressBar(parent, param), m_slider_mode(SLIDER_MODE_BAR),
	  m_label_slider_mode(LABEL_MODE_PERCENTAGE), m_label_slider_pos_mode(LABEL_POS_MODE_CENTER),
	  m_label_slider_margin(5.0f), m_drag_touch_id(-1), m_key(-1), m_key_step(1.0f)
{
	m_event_enable |= 0xFBF0;
	m_hit_find_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;
	m_pre_dispatch = true;
	m_hit_check = 0;
	/* it plays its two sounds */
	m_sound_mask = 3;

	CreateParam create;

	create.m_parent_protected = true;

	Widget *slider = new ProgressBarTouchSlider(this, &create);

	if (slider != NULL) {
		slider->m_event_enable |= 0x10C0;
		slider->m_hit_check = 1;
		slider->SetDragMode(0x120);
		slider->AddEventListener(0x2000D, new EventCBListener(OnDragSliderEvent, this), false);
		slider->AddEventListener(0x2000E, new EventCBListener(OnDragSliderEvent, this), false);
		slider->AddEventListener(0x2000F, new EventCBListener(OnDragSliderEvent, this), false);
	}
	m_parts.resize(PARTS_PROGRESSBAR_TOUCH_END);
	m_parts[PARTS_SLIDER] = slider;
	m_parts[PARTS_LABEL_SLIDER] = NULL;
}

ProgressBarTouchParam::ProgressBarTouchParam()
{
	for (uint32_t i = 0; i < sizeof(c_textureList) / sizeof(c_textureList[0]); i++)
		m_texture_list.push_back(IndexedRef(c_textureList[i].name, c_textureList[i].index));
	m_slider_mode = ProgressBarTouch::SLIDER_MODE_BAR;
	m_slider_size = math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	m_label_slider_mode = ProgressBar::LABEL_MODE_PERCENTAGE;
	m_label_slider_pos_mode = ProgressBar::LABEL_POS_MODE_CENTER;
	m_label_slider_margin = 5.0f;
	m_label_slider.m_idhash = 0;
}

ProgressBarTouchStyleParam::ProgressBarTouchStyleParam()
{
	m_slider_obj[0].m_name = "slider_obj";
	m_slider_obj[0].m_index = ProgressBarTouch::OBJ_SLIDER;
	m_drawobj_param_list.push_back(&m_slider_obj[0]);
	m_slider_obj[1].m_name = "slider_glow_obj";
	m_slider_obj[1].m_index = ProgressBarTouch::OBJ_SLIDER_GLOW;
	m_drawobj_param_list.push_back(&m_slider_obj[1]);
}

ProgressBarTouchSoundParam::ProgressBarTouchSoundParam()
{
	for (uint32_t i = 0; i < 2; i++)
		m_sound_list.push_back(Sound(s_progressbar_touch_sounds[i].name,
					     s_progressbar_touch_sounds[i].index));
}

bool ProgressBarTouch::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return ProgressBar::IsInherit(type);
}

char *ProgressBarTouch::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *ProgressBarTouch::TypeName()
{
	return m_widget_type;
}

ProgressBarTouch::~ProgressBarTouch()
{
}

int32_t ProgressBarTouch::SetObjSize(const math::v4 &size, int32_t obj)
{
	Widget *widget = GetPartsWidget(obj, NULL);

	if (widget == NULL)
		return -1;

	int32_t ret = widget->SetSize(size);

	if (obj == OBJ_SLIDER)
		m_dirty |= 1 << DIRTY_FLAG_LABEL_SLIDER_POS;
	return ret;
}

int32_t ProgressBarTouch::GetObjSize(math::v4 &size, int32_t obj)
{
	Widget *widget = GetPartsWidget(obj, NULL);

	if (widget == NULL)
		return -1;
	size = *widget->GetSize(0);
	return 0;
}

int32_t ProgressBarTouch::SetSliderMode(SliderMode mode)
{
	if (mode < SLIDER_MODE_BAR || mode > SLIDER_MODE_BAR_BACK)
		return 0x80AF008E;
	m_slider_mode = mode;
	UpdateSliderDragRange();
	return 0;
}

int32_t ProgressBarTouch::SetValue(float value, bool anim, BarId id)
{
	if (id == BAR_FORE && (m_drag_touch_id >= 0 || m_key >= 0))
		return 0x80AF0021;
	return ProgressBar::SetValue(value, anim, id);
}

int32_t ProgressBarTouch::AttachLabelSliderWidget(Widget *widget)
{
	if (widget == NULL)
		return 0x80AF008E;
	m_parts[PARTS_LABEL_SLIDER] = widget;

	Widget *slider = m_parts[PARTS_SLIDER];

	if (slider != NULL)
		widget->SetParent(slider, NULL, 0);
	/* shown while the slider is dragged */
	HideLabelSlider(widget);
	m_dirty |= 1 << DIRTY_FLAG_LABEL_SLIDER_POS;
	return 0;
}

int32_t ProgressBarTouch::DetachLabelSliderWidget()
{
	m_parts[PARTS_LABEL_SLIDER] = NULL;
	return 0;
}

Widget *ProgressBarTouch::GetLabelSliderWidget()
{
	return m_parts[PARTS_LABEL_SLIDER];
}

int32_t ProgressBarTouch::SetLabelSliderMode(LabelMode mode)
{
	if (mode < LABEL_MODE_PERCENTAGE || mode > LABEL_MODE_MANUAL)
		return 0x80AF008E;
	m_label_slider_mode = mode;
	return 0;
}

int32_t ProgressBarTouch::SetLabelSliderPosMode(LabelPosMode mode)
{
	if (mode < LABEL_POS_MODE_CENTER || mode > LABEL_POS_MODE_MANUAL)
		return 0x80AF008E;
	m_label_slider_pos_mode = mode;
	m_dirty |= 1 << DIRTY_FLAG_LABEL_SLIDER_POS;
	return 0;
}

int32_t ProgressBarTouch::SetLabelSliderMargin(float margin)
{
	m_label_slider_margin = margin;
	m_dirty |= 1 << DIRTY_FLAG_LABEL_SLIDER_POS;
	return 0;
}

void ProgressBarTouch::AddSound(uint32_t index, WidgetSoundParam::Sound &sound)
{
	if (index < SOUND_PROGRESSBAR_TOUCH_END) {
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

WidgetSoundParam::Sound *ProgressBarTouch::GetSound(int32_t id)
{
	for (list<WidgetSoundParam::Sound>::iterator it = m_sound_list.begin();
	     it != m_sound_list.end(); ++it) {
		if (it->m_index == (uint32_t)id)
			return &*it;
	}
	return FindSound(id);
}

void ProgressBarTouch::CreateSliderGlow()
{
	if (GetDrawObj(OBJ_SLIDER_GLOW) != NULL)
		return;

	Widget *slider = m_parts[PARTS_SLIDER];

	if (slider == NULL)
		return;

	graph::PlaneObj *glow = new graph::PlaneObj(graph::PlaneObjStyle());

	{
		common::SharedPtr<graph::PlaneObj> ptr(glow);

		slider->AddDrawObj(DrawObjEntry(ptr, OBJ_SLIDER_GLOW), false);
	}
	/* lit while the slider holds a touch */
	ShowGlow(OBJ_SLIDER_GLOW, slider->m_touch_captures.size() != 0);
}

void ProgressBarTouch::ShowGlow(int32_t obj, bool show)
{
	graph::DrawObj *glow = GetDrawObj(obj);

	if (glow == NULL)
		return;

	float alpha = 0.7f;

	if (show)
		alpha = 1.0f;

	math::v4 color = glow->GetColor();

	color.set_w(alpha);
	glow->SetColor(color);
}

int32_t ProgressBarTouch::HandlePointEvent(Event *e)
{
	OnTouch(e);
	return Handler::HandlePointEvent(e);
}

int32_t ProgressBarTouch::HandleTouchEvent(Event *e)
{
	OnTouch(e);
	return Widget::HandleTouchEvent(e);
}

int32_t ProgressBarTouch::HandleKeycodeEvent(Event *e)
{
	int32_t key = (uint8_t)e->m_detail;

	if (key != 3 && key != 4)
		return Widget::HandleKeycodeEvent(e);

	float step = m_key_step;

	if (step == 0.0f)
		step = 1.0f;

	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

	switch (e->GetType()) {
	case 0x11:
	case 0x13: {
		float value;

		if (key == 3) {
			value = GetValue() - step;
			if (value <= 0.0f)
				value = 0.0f;
		} else {
			value = GetValue() + step;
			if (value >= m_max)
				value = m_max;
		}
		if (value != GetValue()) {
			SetEventFloat(ev, 0, value);
			SetValue_core(value, true, BAR_FORE);
			if (m_key < 0) {
				m_key = key;
				ShowGlow(OBJ_SLIDER_GLOW, true);
				DoEvent(CB_PROGRESSBAR_TOUCH_DRAG_BEGIN, &ev);
			} else {
				DoEvent(CB_PROGRESSBAR_TOUCH_DRAG_MOVE, &ev);
			}
			UpdateLabelSlider(value);
			DoEvent(CB_PROGRESSBAR_TOUCH_CHANGED, &ev);
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
			SetEventFloat(ev, 0, GetValue());
			DoEvent(CB_PROGRESSBAR_TOUCH_DRAG_END, &ev);

			Widget *label = m_parts[PARTS_LABEL_SLIDER];

			if (label != NULL)
				HideLabelSlider(label);
			e->Consume();
		}
		break;
	}
	return Widget::HandleKeycodeEvent(e);
}

int32_t ProgressBarTouch::HandleFocusEvent(Event *e)
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

			SetEventFloat(ev, 0, GetValue());
			ev.m_data[1] = 1;
			DoEvent(CB_PROGRESSBAR_TOUCH_DRAG_END, &ev);
			e->Consume();
		}
		break;
	}
	return Handler::HandleFocusEvent(e);
}

void ProgressBarTouch::EnableFocusEvent(bool on_off)
{
	if (m_parts.size() > PARTS_SLIDER) {
		Widget *slider = GetPartsWidget(OBJ_SLIDER, NULL);

		if (slider != NULL)
			slider->EnableFocusEvent(on_off);
	}
	Widget::EnableFocusEvent(on_off);
}

Widget *ProgressBarTouch::GetPartsWidget(int32_t obj, int32_t *child_obj)
{
	int32_t child = 0;

	switch (obj) {
	case OBJ_SLIDER:
		break;
	case OBJ_SLIDER_GLOW:
		child = obj;
		break;
	default:
		return ProgressBar::GetPartsWidget(obj, child_obj);
	}
	if (child_obj != NULL)
		*child_obj = child;
	return m_parts[PARTS_SLIDER];
}

void ProgressBarTouch::CheckDirtyFlag()
{
	ProgressBar::CheckDirtyFlag();
	if (m_dirty & (1 << DIRTY_FLAG_LABEL_SLIDER_POS))
		UpdateLabelSliderPos();
}

/* the bar, then the slider at the fore bar's end or its range to the back bar's, the focus
   cursor a frame around the slider */
void ProgressBarTouch::UpdateBar(BarId id, bool anim)
{
	ProgressBar::UpdateBar(id, anim);
	if (id == BAR_FORE)
		UpdateSliderPos(anim);
	else
		UpdateSliderDragRange();

	Widget *slider = m_parts[PARTS_SLIDER];

	if (slider == NULL)
		return;

	FocusShape shape;
	const math::v4 *size = slider->GetSize(0);

	shape.m_shape = 4;
	shape.m_size = math::v2(size->extract_x(), size->extract_y());
	ScePafWidget_13B246E0(slider, &shape);
}

void ProgressBarTouch::UpdateSliderPos(bool anim)
{
	Widget *slider = m_parts[PARTS_SLIDER];

	if (slider == NULL)
		return;
	slider->KillAnim(ANIM_MOVE);

	math::v4 pos = m_origin_offs +
		       MulPerElem(math::v4(m_rate[BAR_FORE] - 0.5f, 0.0f, 0.0f, 0.0f), m_size);

	if (anim)
		slider->SetPos(pos, new Timer(m_anim_time, m_anim_func, 0, -1, NULL), ANIM_MOVE, 2);
	else
		slider->SetPos(pos);
}

void ProgressBarTouch::UpdateSliderDragRange()
{
	Widget *slider = m_parts[PARTS_SLIDER];

	if (slider == NULL)
		return;

	math::v4 min;
	math::v4 max;

	if (m_slider_mode == SLIDER_MODE_BAR_BACK && m_bar_mode == BAR_MODE_DOUBLE) {
		min = math::v4(m_min_rate - 0.5f, 0.0f, 0.0f, 0.0f);
		max = math::v4(m_rate[BAR_BACK] - 0.5f, 0.0f, 0.0f, 0.0f);
	} else {
		min = math::v4(-0.5f, 0.0f, 0.0f, 0.0f);
		max = math::v4(0.5f, 0.0f, 0.0f, 0.0f);
	}
	slider->SetDragLimitMin(m_origin_offs + MulPerElem(min, m_size));
	slider->SetDragLimitMax(m_origin_offs + MulPerElem(max, m_size));
}

void ProgressBarTouch::UpdateLabelSlider(float value)
{
	Widget *label = m_parts[PARTS_LABEL_SLIDER];

	if (label == NULL)
		return;
	ShowLabelSlider(label);
	if (m_label_slider_mode == LABEL_MODE_PERCENTAGE ||
	    m_label_slider_mode == LABEL_MODE_VALUE) {
		wchar_t buffer[32];

		switch (m_label_slider_mode) {
		case LABEL_MODE_PERCENTAGE:
			sce_paf_swprintf(buffer, 32, L"%d%%", (int32_t)(value / m_max * 100.0f));
			break;
		case LABEL_MODE_VALUE:
			sce_paf_swprintf(buffer, 32, L"%d", (int32_t)value);
			break;
		default:
			break;
		}
		label->SetString(wstring(buffer));
	}
}

/* the label placed by its position mode: off the slider's centre by halves of its size and
   margins */
void ProgressBarTouch::UpdateLabelSliderPos()
{
	m_dirty &= ~(1 << DIRTY_FLAG_LABEL_SLIDER_POS);

	Widget *slider = m_parts[PARTS_SLIDER];
	Widget *label = m_parts[PARTS_LABEL_SLIDER];

	if (slider == NULL || label == NULL)
		return;
	if (m_label_slider_pos_mode == LABEL_POS_MODE_MANUAL)
		return;

	const LabelPos &entry = c_labelPosTable[m_label_slider_pos_mode];

	label->SetPos(MulPerElem(MulPerElem(V4(entry.m_pos), *slider->GetSize(0)),
				 math::v4(0.5f, 0.5f, 0.5f, 0.5f)) +
		      V4(entry.m_margin) * m_label_slider_margin);
	label->SetAlign(entry.m_align_x, entry.m_align_y, 0, NULL);
}

void ProgressBarTouch::OnTouch(Event *e)
{
	if (m_drag_touch_id >= 0)
		return;
	switch (e->GetType()) {
	case 0x21:
	case 0x41:
		break;
	default:
		return;
	}

	Widget *slider = m_parts[PARTS_SLIDER];

	if (slider != NULL)
		ScePafWidget_D3D0A7C3(slider, (e->m_detail >> 20) & 0xF, (uint8_t)e->m_detail, true,
				      true);
	e->Consume();
}

void ProgressBarTouch::OnDragSliderEvent(int32_t type, Handler *self, Event *e, void *data)
{
	ProgressBarTouch *bar = (ProgressBarTouch *)data;

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
void ProgressBarTouch::OnDragBeginSlider(Event *e)
{
	uint32_t id = (uint8_t)e->m_detail;
	Widget *slider = m_parts[PARTS_SLIDER];

	m_drag_touch_id = id;
	if (slider != NULL) {
		Environment *env = m_env;
		HitInfo *hit = NULL;

		if (id < 8)
			hit = &env->m_touch_hit[0][id];
		if ((hit == NULL || hit->m_widget != slider) && id < 8) {
			env->m_touch_state[0][id].m_port = 0;
			env->m_touch_state[0][id].m_id = id;

			TouchInfo *touch = &env->m_touch_state[0][id];

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
				pos = math::v4(sce::Vectormath::Simd::Aos::maxPerElem(
					sce::Vectormath::Simd::Aos::minPerElem(pos.getImpl(), max.getImpl()),
					min.getImpl()));
				slider->SetPos_core(pos, 0xFF01, false);
			}
		}
	}
	ScePafWidget_2B1A1493(this, SOUND_DRAG_BEGIN);
	ShowGlow(OBJ_SLIDER_GLOW, true);
	OnDragSlider(CB_PROGRESSBAR_TOUCH_DRAG_BEGIN, false);
}

void ProgressBarTouch::OnDragMoveSlider(Event *e)
{
	OnDragSlider(CB_PROGRESSBAR_TOUCH_DRAG_MOVE, false);
}

void ProgressBarTouch::OnDragEndSlider(Event *e, bool cancel)
{
	ScePafWidget_2B1A1493(this, SOUND_DRAG_END);
	ShowGlow(OBJ_SLIDER_GLOW, false);
	OnDragSlider(CB_PROGRESSBAR_TOUCH_DRAG_END, cancel);

	Widget *label = m_parts[PARTS_LABEL_SLIDER];

	if (label != NULL)
		HideLabelSlider(label);
	m_drag_touch_id = -1;
}

void ProgressBarTouch::OnDragSlider(int32_t type, bool cancel)
{
	Widget *slider = m_parts[PARTS_SLIDER];

	if (slider == NULL)
		return;

	math::v4 pos = slider->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)) - m_origin_offs;
	math::v4 size = m_size;
	float value = 0.0f;

	if (!(size.extract_x() < 1.0f))
		value = (pos.extract_x() / size.extract_x() + 0.5f) * m_max;

	Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

	SetEventFloat(ev, 0, value);
	ev.m_data[1] = cancel;
	DoEvent(type, &ev);
	UpdateLabelSlider(value);

	float old = GetValue();

	SetValue_core(value, false, BAR_FORE);
	if (type != CB_PROGRESSBAR_TOUCH_DRAG_END && GetValue() != old) {
		Event changed(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

		DoEvent(CB_PROGRESSBAR_TOUCH_CHANGED, &changed);
	}
}

const char *ProgressBarTouchParam::GetParamName() const
{
	return m_param_name;
}

const char *ProgressBarTouchParam::ParamName()
{
	return m_param_name;
}

int32_t ProgressBarTouchParam::Create(int32_t mode)
{
	int32_t ret = ProgressBarParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL) {
		m_serial->AddParam("slider_mode", &m_slider_mode, ResourceParam::SEMANTIC_INT);
		if (m_serial != NULL) {
			m_serial->AddParam("slider_size", &m_slider_size,
					   ResourceParam::SEMANTIC_VEC3);
			if (m_serial != NULL) {
				m_serial->AddParam("w_label_slider", &m_label_slider,
						   ResourceParam::SEMANTIC_WIDGETREF);
				if (m_serial != NULL) {
					m_serial->AddParam("label_slider_mode", &m_label_slider_mode,
							   ResourceParam::SEMANTIC_INT);
					if (m_serial != NULL) {
						m_serial->AddParam("label_slider_pos_mode",
								   &m_label_slider_pos_mode,
								   ResourceParam::SEMANTIC_INT);
						if (m_serial != NULL)
							m_serial->AddParam(
								"label_slider_margin",
								&m_label_slider_margin,
								ResourceParam::SEMANTIC_FLOAT);
					}
				}
			}
		}
	}
	return 0;
}

int32_t ProgressBarTouchParam::Set()
{
	int32_t ret = ProgressBarParam::Set();

	if (ret < 0)
		return ret;

	ProgressBarTouch *bar = (ProgressBarTouch *)m_widget;

	bar->SetSliderMode((ProgressBarTouch::SliderMode)m_slider_mode);
	bar->SetObjSize(math::v4(math::v3(m_slider_size.getImpl().getXYZ()), 0.0f),
			ProgressBarTouch::OBJ_SLIDER);
	bar->SetLabelSliderMode((ProgressBar::LabelMode)m_label_slider_mode);
	bar->SetLabelSliderPosMode((ProgressBar::LabelPosMode)m_label_slider_pos_mode);
	bar->SetLabelSliderMargin(m_label_slider_margin);
	if (m_label_slider.GetIDHash() != 0) {
		Widget *label = bar->FindChild(m_label_slider, 0);

		if (label != NULL)
			bar->AttachLabelSliderWidget(label);
	}
	return 0;
}

int32_t ProgressBarTouchParam::Get()
{
	int32_t ret = ProgressBarParam::Get();

	if (ret < 0)
		return ret;

	ProgressBarTouch *bar = (ProgressBarTouch *)m_widget;
	math::v4 size;

	m_slider_mode = bar->GetSliderMode();
	if (bar->GetObjSize(size, ProgressBarTouch::OBJ_SLIDER) >= 0)
		m_slider_size = size;
	m_label_slider_mode = bar->GetLabelSliderMode();
	m_label_slider_pos_mode = bar->GetLabelSliderPosMode();
	m_label_slider_margin = bar->GetLabelSliderMargin();
	return 0;
}

const char *ProgressBarTouchStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *ProgressBarTouchStyleParam::ParamName()
{
	return m_param_name;
}

int32_t ProgressBarTouchStyleParam::SetEach(Widget *widget)
{
	ProgressBarTouch *bar = (ProgressBarTouch *)widget;
	graph::DrawObjParam *glow = GetDrawObjParam(ProgressBarTouch::OBJ_SLIDER_GLOW);

	if (glow != NULL && glow->m_id.GetIDHash() != 0)
		bar->CreateSliderGlow();

	int32_t ret = ProgressBarStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	bar->ShowGlow(ProgressBarTouch::OBJ_SLIDER_GLOW, false);
	return 0;
}

const char *ProgressBarTouchSoundParam::GetParamName() const
{
	return m_param_name;
}

const char *ProgressBarTouchSoundParam::ParamName()
{
	return m_param_name;
}

bool ProgressBarTouchSlider::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Plane::IsInherit(type);
}

char *ProgressBarTouchSlider::GetTypeName() const
{
	return (char *)m_widget_type;
}

int32_t ProgressBarTouchSlider::GetFocusDirectionMask()
{
	return m_focus_direction_off ? 2 : 7;
}

/* its glow sized to its plane */
void ProgressBarTouchSlider::UpdateSize()
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

static Widget *NewProgressBarTouch(Widget *parent)
{
	return new ProgressBarTouch(parent, NULL);
}

static WidgetParam *NewProgressBarTouchParam()
{
	return new ProgressBarTouchParam;
}

static VersatileParam *NewProgressBarTouchLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewProgressBarTouchFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewProgressBarTouchStyleParam()
{
	return new ProgressBarTouchStyleParam;
}

static VersatileParam *NewProgressBarTouchHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewProgressBarTouchSoundParam()
{
	return new ProgressBarTouchSoundParam;
}

FactoryFuncTable g_factoryProgressBarTouch = {
	ProgressBarTouch::TypeName(),
	NewProgressBarTouch,
	NewProgressBarTouchParam,
	NewProgressBarTouchLayoutParam,
	NewProgressBarTouchFocusParam,
	NewProgressBarTouchStyleParam,
	NewProgressBarTouchHandlerParam,
	NewProgressBarTouchSoundParam,
};
