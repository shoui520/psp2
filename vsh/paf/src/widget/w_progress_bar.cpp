/*
 * paf::ui::ProgressBar (TOOL 0x811056d4..0x81109c78): a bar filled from its minimum to its value,
 * a back bar behind it in BAR_MODE_DOUBLE, a signal running along it, a glow at its end, the
 * shadows of the base and the bar, and a label of its rate. Its draw objects (the bars, the glow
 * and the signal) and the glow's shader; its resource and style parameters and its factories.
 * No record or string names the file; the draw objects' classes are Sony's ("GlowObj",
 * "ProgressSignalObj", "BarObj"), the names of the shader and the static functions ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <framework.h>
#include <graphics/drawobj/draw_obj.h>
#include <gxm.h>
#include <graphics/arl/arl_defines.h>
#include <graphics/color.h>
#include <graphics/arl/gxm/gxm_arl_frontend.h>
#include <graphics/arl/gxm/gxm_framebuffer_copy.h>
#include <graphics/arl/gxm/gxm_shader.h>
#include <graphics/arl/gxm/gxm_vertex_array.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/effects/wave_renderer.h>
#include <graphics/surface/surface.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <std/stdio.h>
#include <std/string.h>
#include <thread/thread.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/animlistener.h>
#include <widget/factory.h>
#include <widget/w_progress_bar.h>

/* the glow's and the signal's animations (no names found) */
extern "C" {
void ScePafWidget_43E9F757(paf::Timer *t, paf::ui::AnimListener *listener, void *data1,
			   void *data2, void *data3);
void ScePafWidget_A6EC5158(paf::Timer *t, paf::ui::AnimListener *listener, void *data1,
			   void *data2, void *data3);
}

namespace paf {
namespace ui {

/* the glow: a plane drawn as bright as its intensity */
class GlowObj : public graph::PlaneObj {
public:
	static const char m_drawobj_type[];

	GlowObj(const graph::DrawStyle &style) : graph::PlaneObj(style), m_intensity(1.0f)
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;

	virtual ~GlowObj()
	{
	}

	virtual void OnDraw(const math::matrix &parent, uint32_t color, float alpha,
			    graph::DrawInfo &info);

	/* kept between 0 and 1 (name ours) */
	void SetIntensity(float intensity)
	{
		if (intensity < 0.0f)
			intensity = 0.0f;
		m_intensity = intensity < 1.0f ? intensity : 1.0f;
	}

	/* how bright it is drawn (name ours) */
	float m_intensity;
};

/* the signal: a glow drawn with the glow shader, its texture scrolled along the bar */
class ProgressSignalObj : public GlowObj {
public:
	static const char m_drawobj_type[];

	ProgressSignalObj(const graph::DrawStyle &style);

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;

	virtual ~ProgressSignalObj()
	{
	}
};

/* a bar: a divided plane whose borders are narrowed where it is narrower than they are */
class BarObj : public graph::PlaneObj {
public:
	static const char m_drawobj_type[];

	BarObj(const graph::DrawStyle &style) : graph::PlaneObj(style), m_clipped(false)
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;

	virtual ~BarObj()
	{
	}

	virtual void OnDraw(const math::matrix &parent, uint32_t color, float alpha,
			    graph::DrawInfo &info);

	/* its borders narrowed at the last draw (name ours) */
	bool m_clipped;
};

/* paf/cg/drawobj/progress_signal.vp: the mesh moved by the model view projection, its texture
   coordinates offset and scaled into the region of the texture (class name ours) */
class ProgressSignalVertShader : public graph::arl::ShaderInterfaceBase {
public:
	ProgressSignalVertShader() : m_mvp(m_setup), m_uv_offset(m_setup), m_uv_scale(m_setup)
	{
	}

	virtual void Init()
	{
		m_mvp.init("MVP", graph::arl::ShaderParameterBase::UNIFORM, 0);
		m_uv_offset.init("UVOffset", graph::arl::ShaderParameterBase::UNIFORM, 0);
		m_uv_scale.init("UVScale", graph::arl::ShaderParameterBase::UNIFORM, 0);
	}

	virtual void Apply()
	{
		m_mvp.Apply();
		m_uv_offset.Apply();
		m_uv_scale.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/progress_signal.vp", 452);

	graph::arl::ShaderParameter<math::matrix> m_mvp;
	graph::arl::ShaderParameter<math::v2> m_uv_offset;
	graph::arl::ShaderParameter<math::v2> m_uv_scale;
};

/* paf/cg/drawobj/progress_signal.fp: the texture in the colour of the scene (class name ours) */
class ProgressSignalFragShader : public graph::arl::ShaderInterfaceBase {
public:
	ProgressSignalFragShader() : m_color(m_setup), m_texture0(m_setup)
	{
	}

	virtual void Init()
	{
		m_color.init("Color", graph::arl::ShaderParameterBase::UNIFORM, 0);
		m_texture0.init("Texture0", 1, 0);
	}

	virtual void Apply()
	{
		m_color.Apply();
		m_texture0.Apply();
	}

	virtual void Unused()
	{
	}

	WAVE_SHADER_BINARY("paf/cg/drawobj/progress_signal.fp", 404);

	graph::arl::ShaderParameter<math::v4> m_color;
	graph::arl::ShaderParameter<graph::arl::TextureSource> m_texture0;
};

/* the shader the signal is drawn with: its programs made the first time it draws (name ours) */
class GlowShader : public graph::Shader {
public:
	/* the vertex and fragment programs, together (name ours) */
	class Programs {
	public:
		graph::arl::ShaderInstance<ProgressSignalVertShader, graph::arl::VertShaderObject> m_vert;
		graph::arl::ShaderInstance<ProgressSignalFragShader, graph::arl::FragShaderObject> m_frag;
	};

	GlowShader() : m_programs(NULL)
	{
	}

	/* the programs given back; the pointer marked gone */
	virtual ~GlowShader()
	{
		delete m_programs;
		m_programs = (Programs *)-1;
	}

	virtual int32_t Draw(graph::PlaneObjDrawParams &params);

	Programs *m_programs;
};

/* the signal drawn with the glow's programs (name ours) */
static int32_t DrawGlow(GlowShader *shader, graph::PlaneObjDrawParams &params);

const char ProgressBar::m_widget_type[] = "progressbar";
const char ProgressBarParam::m_param_name[] = "progressbar_base";

const ProgressBarParam::TextureRef ProgressBarParam::c_textureList[] = {
	{ "texture_bar_fore", ProgressBar::OBJ_BAR_FORE },
	{ "texture_bar_back", ProgressBar::OBJ_BAR_BACK },
};

const char ProgressBarStyleParam::m_param_name[] = "style_progressbar";
const char GlowObj::m_drawobj_type[] = "GlowObj";
const char ProgressSignalObj::m_drawobj_type[] = "ProgressSignalObj";
const char BarObj::m_drawobj_type[] = "BarObj";

/* the two vectors multiplied lane by lane (name ours) */
/* the draw object shown or hidden, redrawn when it is shown (inlined in Sony's code, name ours) */
static inline void ShowDrawObj(graph::DrawObj *obj, bool show)
{
	if (obj->m_flags.m_visible != show) {
		obj->m_flags.m_visible = show;
		if (show && !graph::DrawObj::s_safety)
			obj->m_flags.m_dirty = graph::DrawObj::s_dirty_count;
	}
}

static inline math::v4 MulPerElem(math::v4 a, math::v4 b)
{
	return math::v4(sce::Vectormath::Simd::Aos::mulPerElem(a.getImpl(), b.getImpl()));
}

/* the vector's x and y, z and w 0 (name ours) */
static inline math::v4 V4(math::v2 xy)
{
	return math::v4(xy.extract_x(), xy.extract_y(), 0.0f, 0.0f);
}

/* the bar over the base, the base under it */
ProgressBar::ProgressBar(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_bar_mode(BAR_MODE_SINGLE), m_label_mode(LABEL_MODE_PERCENTAGE),
	  m_label_pos_mode(LABEL_POS_MODE_CENTER), m_label_margin(5.0f), m_max(100.0f), m_min(0.0f),
	  m_anim_time(150.0f), m_anim_func(39), m_signal_anim_time(300.0f), m_min_rate(0.0f),
	  m_dirty(0), m_async_max(-1.0f), m_async_min(-1.0f), m_glow_visible(false),
	  m_signal_anim(false), m_shadow_visible(false)
{
	m_value[BAR_FORE] = 0.0f;
	m_value[BAR_BACK] = 0.0f;
	m_shadow_param.m_offset = math::v2(0.0f, 0.0f);
	m_shadow_param.m_margin = math::v2(0.0f, 0.0f);
	m_rate[BAR_FORE] = 0.0f;
	m_rate[BAR_BACK] = 0.0f;
	m_async_value[BAR_FORE] = -1.0f;
	m_async_value[BAR_BACK] = -1.0f;
	m_async_anim[BAR_FORE] = false;
	m_async_anim[BAR_BACK] = false;
	m_event_enable |= 0x1000;
	m_hit_find_mode = 0;
	m_hit_detect_mode = 0;
	m_hit_detect_b_mode = 0;

	graph::DrawObj *bar = new BarObj(graph::PlaneObjStyle());

	if (bar != NULL) {
		common::SharedPtr<graph::DrawObj> ptr(bar);
		DrawObjAttr attr(OBJ_BAR_FORE);

		attr.m_flag1 = false;
		AddDrawObj(DrawObjEntry(ptr, attr), false);
	}

	graph::DrawObj *base = new graph::PlaneObj(graph::PlaneObjStyle());

	if (base != NULL) {
		common::SharedPtr<graph::DrawObj> ptr(base);

		AddDrawObj(DrawObjEntry(ptr, OBJ_BASE), false);
	}
	m_parts.resize(1);
	m_parts[0] = NULL;
}

ProgressBarParam::ProgressBarParam()
{
	for (uint32_t i = 0; i < sizeof(c_textureList) / sizeof(c_textureList[0]); i++)
		m_texture_list.push_back(IndexedRef(c_textureList[i].name, c_textureList[i].index));
	m_bar_mode = ProgressBar::BAR_MODE_SINGLE;
	m_label_mode = ProgressBar::LABEL_MODE_PERCENTAGE;
	m_label_pos_mode = ProgressBar::LABEL_POS_MODE_CENTER;
	m_label_margin = 5.0f;
	m_signal_anim = false;
	m_label.m_idhash = 0;
}

ProgressBarStyleParam::ProgressBarStyleParam()
{
	m_anim_time = 150.0f;
	m_anim_func = 39;
	m_signal_anim_time = 300.0f;
	m_shadow_param.m_offset = math::v2(0.0f, 0.0f);
	m_shadow_param.m_margin = math::v2(0.0f, 0.0f);
	m_obj[ProgressBar::OBJ_BASE].m_name = "base_obj";
	m_obj[ProgressBar::OBJ_BASE].m_index = ProgressBar::OBJ_BASE;
	m_drawobj_param_list.push_back(&m_obj[ProgressBar::OBJ_BASE]);
	m_obj[ProgressBar::OBJ_BAR_FORE].m_name = "bar_fore_obj";
	m_obj[ProgressBar::OBJ_BAR_FORE].m_index = ProgressBar::OBJ_BAR_FORE;
	m_drawobj_param_list.push_back(&m_obj[ProgressBar::OBJ_BAR_FORE]);
	m_obj[ProgressBar::OBJ_BAR_BACK].m_name = "bar_back_obj";
	m_obj[ProgressBar::OBJ_BAR_BACK].m_index = ProgressBar::OBJ_BAR_BACK;
	m_drawobj_param_list.push_back(&m_obj[ProgressBar::OBJ_BAR_BACK]);
	m_obj[ProgressBar::OBJ_SIGNAL].m_name = "signal_obj";
	m_obj[ProgressBar::OBJ_SIGNAL].m_index = ProgressBar::OBJ_SIGNAL;
	m_drawobj_param_list.push_back(&m_obj[ProgressBar::OBJ_SIGNAL]);
	m_glow_obj.m_name = "glow_obj";
	m_glow_obj.m_index = ProgressBar::OBJ_GLOW;
	m_drawobj_param_list.push_back(&m_glow_obj);
	m_shadow_obj[0].m_name = "base_shadow_obj";
	m_shadow_obj[0].m_index = ProgressBar::OBJ_BASE_SHADOW;
	m_drawobj_param_list.push_back(&m_shadow_obj[0]);
	m_shadow_obj[1].m_name = "bar_shadow_obj";
	m_shadow_obj[1].m_index = ProgressBar::OBJ_BAR_SHADOW;
	m_drawobj_param_list.push_back(&m_shadow_obj[1]);
}

bool ProgressBar::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *ProgressBar::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *ProgressBar::TypeName()
{
	return m_widget_type;
}

ProgressBar::~ProgressBar()
{
}

graph::DrawObj *ProgressBar::GetDrawObj(int32_t obj)
{
	int32_t child;
	Widget *widget = GetPartsWidget(obj, &child);

	if (widget == NULL)
		return NULL;
	if (widget == this)
		return Widget::GetDrawObj(obj);
	return widget->GetDrawObj(child);
}

common::SharedPtr<graph::DrawObj> ProgressBar::SwapDrawObj(common::SharedPtr<graph::DrawObj> obj,
							   int32_t id)
{
	int32_t child;
	Widget *widget = GetPartsWidget(id, &child);

	if (widget == NULL)
		return common::SharedPtr<graph::DrawObj>();
	return widget->SwapDrawObj(obj, child);
}

int32_t ProgressBar::SetObjColor(const math::v4 &color, int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return -1;
	draw->SetColor(color);
	return 0;
}

int32_t ProgressBar::GetObjColor(math::v4 &color, int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return -1;
	color = draw->GetColor();
	return 0;
}

void ProgressBar::ScePafWidget_536C8866()
{
	if (GetDrawObj(OBJ_BAR_BACK) != NULL)
		return;

	graph::DrawObj *bar = new BarObj(graph::PlaneObjStyle());

	if (bar == NULL)
		return;

	common::SharedPtr<graph::DrawObj> ptr(bar);
	DrawObjAttr attr(OBJ_BAR_BACK);

	attr.m_flag1 = false;
	/* under the fore bar */
	InsertDrawObj(FindDrawObj(OBJ_BAR_FORE), DrawObjEntry(ptr, attr), false);
	ShowDrawObj(bar, m_bar_mode == BAR_MODE_DOUBLE);
}

int32_t ProgressBar::SetBarMode(BarMode mode)
{
	if (mode < 0 || mode > 1)
		return 0x80AF008E;
	m_bar_mode = mode;
	if (mode == BAR_MODE_SINGLE) {
		graph::DrawObj *back = GetDrawObj(OBJ_BAR_BACK);

		if (back != NULL)
			ShowDrawObj(back, false);
	} else {
		ScePafWidget_536C8866();

		graph::DrawObj *back = GetDrawObj(OBJ_BAR_BACK);

		if (back != NULL)
			ShowDrawObj(back, true);
		SetValue_core(m_value[BAR_BACK], false, BAR_BACK);
	}
	return 0;
}

int32_t ProgressBar::SetMaxValue(float value)
{
	if (value <= 0.0f)
		return 0x80AF008E;
	m_max = value;
	s_async_lock.Lock();
	m_async_max = -1.0f;
	s_async_lock.Unlock();
	m_min_rate = m_min / m_max;
	SetValue_core(m_value[BAR_FORE], false, BAR_FORE);
	SetValue_core(m_value[BAR_BACK], false, BAR_BACK);
	return 0;
}

int32_t ProgressBar::SetMaxValueAsync(float value)
{
	if (value <= 0.0f)
		return 0x80AF008E;
	s_async_lock.Lock();
	m_async_max = value;
	s_async_lock.Unlock();
	return 0;
}

int32_t ProgressBar::SetMinValue(float value)
{
	if (value < 0.0f || value > m_max)
		return 0x80AF008E;
	m_min = value;
	s_async_lock.Lock();
	m_async_min = -1.0f;
	s_async_lock.Unlock();
	m_min_rate = m_min / m_max;
	SetValue_core(m_value[BAR_FORE], false, BAR_FORE);
	SetValue_core(m_value[BAR_BACK], false, BAR_BACK);
	return 0;
}

int32_t ProgressBar::SetMinValueAsync(float value)
{
	if (value < 0.0f)
		return 0x80AF008E;
	s_async_lock.Lock();
	m_async_min = value;
	s_async_lock.Unlock();
	return 0;
}

/* the value kept between the minimum and the maximum, the fore bar's under the back bar's */
int32_t ProgressBar::SetValue(float value, bool anim, BarId id)
{
	if (id < 0 || id > 1)
		return 0x80AF008E;
	if (id == BAR_BACK && m_bar_mode == BAR_MODE_SINGLE)
		return 0x80AF008E;

	/* the fore bar no further than the back one */
	float v = value;

	if (id == BAR_FORE && m_bar_mode == BAR_MODE_DOUBLE)
		v = m_value[BAR_BACK] > value ? value : m_value[BAR_BACK];
	v = m_max > v ? v : m_max;
	v = v < m_min ? m_min : v;
	if (v == m_value[id])
		return 0;
	return SetValue_core(value, anim, id);
}

int32_t ProgressBar::SetValueAsync(float value, bool anim, BarId id)
{
	if (id < 0 || id > 1)
		return 0x80AF008E;
	if (id == BAR_BACK && m_bar_mode == BAR_MODE_SINGLE)
		return 0x80AF008E;
	s_async_lock.Lock();
	m_async_value[id] = value;
	m_async_anim[id] = anim;
	s_async_lock.Unlock();
	return 0;
}

float ProgressBar::GetValue(BarId id)
{
	if ((int32_t)id < BAR_FORE || (int32_t)id > BAR_BACK)
		return (float)0x80AF0100u;
	return m_value[id];
}

int32_t ProgressBar::SetBarAnim(float time, int32_t func)
{
	if (time <= 0.0f)
		return 0x80AF008E;
	m_anim_time = time;
	m_anim_func = func;
	return 0;
}

void ProgressBar::ScePafWidget_16ACDAD6()
{
	if (GetDrawObj(OBJ_GLOW) != NULL)
		return;

	graph::DrawObj *glow = new GlowObj(graph::PlaneObjStyle());

	if (glow == NULL)
		return;

	common::SharedPtr<graph::DrawObj> ptr(glow);
	DrawObjAttr attr(OBJ_GLOW);

	attr.m_flag1 = false;
	AddDrawObj(DrawObjEntry(ptr, attr), false);
	ShowDrawObj(glow, m_glow_visible);
}

void ProgressBar::SetGlowVisible(bool visible)
{
	m_glow_visible = visible;

	graph::DrawObj *glow = GetDrawObj(OBJ_GLOW);

	if (glow != NULL)
		ShowDrawObj(glow, m_glow_visible);
}

/* the signal shown, its texture scrolled across it over and over */
void ProgressBar::SetSignalAnim(bool on_off)
{
	m_signal_anim = on_off;

	graph::DrawObj *signal = GetDrawObj(OBJ_SIGNAL);

	if (signal == NULL)
		return;
	ShowDrawObj(signal, on_off);
	KillAnim(ANIM_SIGNAL);
	if (on_off)
		SetAnimListener(ANIM_SIGNAL,
				new AnimListener(new Timer(m_signal_anim_time, Timer::FUNC_LINEAR,
							   Timer::REPEAT_LOOP, -1, NULL),
						 this, ::ScePafWidget_A6EC5158, NULL, signal),
				2);
}

/* the shader every signal shares, made again once the last let it go */
static common::WeakPtr<graph::Shader> s_glow_shader;

static __attribute__((noinline)) common::SharedPtr<graph::Shader> GetGlowShader()
{
	/* made again once the last signal let it go */
	if (s_glow_shader.m_ref_counter == NULL || s_glow_shader.m_ref_counter->m_counter == 0) {
		graph::Shader *glow = new GlowShader;
		common::SharedPtr<graph::Shader> shader(glow);

		s_glow_shader = shader;
		return shader;
	}
	return common::SharedPtr<graph::Shader>(s_glow_shader);
}

ProgressSignalObj::ProgressSignalObj(const graph::DrawStyle &style) : GlowObj(style)
{
	m_shader = GetGlowShader();
}

void ProgressBar::ScePafWidget_7BBE5D41()
{
	if (GetDrawObj(OBJ_SIGNAL) != NULL)
		return;

	ProgressSignalObj *signal = new ProgressSignalObj(graph::PlaneObjStyle());

	if (signal == NULL)
		return;
	/* its texture scrolled across it */
	signal->m_plane_flags.m_repeat_s = true;

	common::SharedPtr<graph::DrawObj> ptr(static_cast<graph::DrawObj *>(signal));
	vector<DrawObjEntry>::iterator it = FindDrawObj(OBJ_BAR_FORE);
	DrawObjAttr attr(OBJ_SIGNAL);

	attr.m_flag1 = false;
	/* over the fore bar */
	if (it == m_drawobjs.end())
		AddDrawObj(DrawObjEntry(ptr, attr), false);
	else
		InsertDrawObj(++it, DrawObjEntry(ptr, attr), false);
	SetSignalAnim(m_signal_anim);
}

bool ProgressBar::IsSignalAnim()
{
	return m_signal_anim;
}

void ProgressBar::SetSignalAnimTime(float time)
{
	m_signal_anim_time = time;
	if (m_signal_anim)
		SetSignalAnim(true);
}

void ProgressBar::ScePafWidget_1E040ECD()
{
	if (GetDrawObj(OBJ_BASE_SHADOW) != NULL)
		return;

	graph::DrawObj *shadow = new graph::PlaneObj(graph::PlaneObjStyle());

	if (shadow == NULL)
		return;

	common::SharedPtr<graph::DrawObj> ptr(shadow);
	DrawObjAttr attr(OBJ_BASE_SHADOW);

	attr.m_flag1 = false;
	/* under everything */
	InsertDrawObj(m_drawobjs.begin(), DrawObjEntry(ptr, attr), false);
	ShowDrawObj(shadow, m_shadow_visible);
	UpdateBaseShadow();
}

void ProgressBar::ScePafWidget_1FD5D71C()
{
	if (GetDrawObj(OBJ_BAR_SHADOW) != NULL)
		return;

	graph::DrawObj *shadow = new graph::PlaneObj(graph::PlaneObjStyle());

	if (shadow == NULL)
		return;

	common::SharedPtr<graph::DrawObj> ptr(shadow);
	vector<DrawObjEntry>::iterator it = FindDrawObj(OBJ_BASE_SHADOW);
	DrawObjAttr attr(OBJ_BAR_SHADOW);

	attr.m_flag1 = false;
	/* over the base's shadow */
	if (it == m_drawobjs.end())
		InsertDrawObj(m_drawobjs.begin(), DrawObjEntry(ptr, attr), false);
	else
		InsertDrawObj(++it, DrawObjEntry(ptr, attr), false);
	ShowDrawObj(shadow, m_shadow_visible);
	UpdateBar(BAR_FORE, false);
}

void ProgressBar::SetShadowVisible(bool visible)
{
	m_shadow_visible = visible;

	graph::DrawObj *shadow = GetDrawObj(OBJ_BASE_SHADOW);

	if (shadow != NULL)
		ShowDrawObj(shadow, m_shadow_visible);
	shadow = GetDrawObj(OBJ_BAR_SHADOW);
	if (shadow != NULL)
		ShowDrawObj(shadow, m_shadow_visible);
}

void ProgressBar::SetShadowParam(const ShadowParam &param)
{
	m_shadow_param = param;
	UpdateBaseShadow();
	UpdateBar(BAR_FORE, false);
}

int32_t ProgressBar::AttachLabelWidget(Widget *widget)
{
	if (widget == NULL)
		return 0x80AF008E;
	m_parts[0] = widget;
	m_dirty |= (1 << DIRTY_FLAG_LABEL) | (1 << DIRTY_FLAG_LABEL_POS);
	return 0;
}

int32_t ProgressBar::DetachLabelWidget()
{
	m_parts[0] = NULL;
	return 0;
}

Widget *ProgressBar::GetLabelWidget()
{
	return m_parts[0];
}

int32_t ProgressBar::SetLabelMode(LabelMode mode)
{
	if (mode < 0 || mode > 3)
		return 0x80AF008E;
	m_label_mode = mode;
	m_dirty |= 1 << DIRTY_FLAG_LABEL;
	return 0;
}

int32_t ProgressBar::SetLabelPosMode(LabelPosMode mode)
{
	if (mode < 0 || mode > 9)
		return 0x80AF008E;
	m_label_pos_mode = mode;
	m_dirty |= 1 << DIRTY_FLAG_LABEL_POS;
	return 0;
}

int32_t ProgressBar::SetLabelMargin(float margin)
{
	m_label_margin = margin;
	m_dirty |= 1 << DIRTY_FLAG_LABEL_POS;
	return 0;
}

int32_t ProgressBar::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				uint32_t index)
{
	int32_t child;
	Widget *widget = GetPartsWidget(obj, &child);

	if (widget == NULL)
		return 0x80AF4101;
	if (widget != this)
		return widget->SetTexture(surf, child, index);

	graph::DrawObj *draw = GetDrawObj(child);

	if (draw == NULL)
		return 0x80AF4101;
	draw->SetTexture(surf, index);
	return 0;
}

/* what the Async setters left applied, then what is dirty updated */
void ProgressBar::Update()
{
	if (m_hold_count > 0)
		return;
	Sync();
	CheckDirtyFlag();
	Widget::Update();
}

uint32_t ProgressBar::CheckState()
{
	if (m_hold_count > 0)
		return m_state;
	for (vector<Widget *>::iterator it = m_parts.begin(); it != m_parts.end(); ++it) {
		Widget *part = *it;

		if (part != NULL && part->m_state == 3)
			return 3;
	}
	if (m_state == 3 || m_state == 1) {
		if (!IsPrepareObjs())
			return 0;
	}
	return m_state;
}

void ProgressBar::UpdateSize()
{
	Widget::UpdateSize();
	UpdateBaseShadow();
	UpdateBar(BAR_FORE, false);
	UpdateBar(BAR_BACK, false);
	m_dirty |= 1 << DIRTY_FLAG_LABEL_POS;
}

Widget *ProgressBar::GetPartsWidget(int32_t obj, int32_t *child_obj)
{
	switch (obj) {
	case OBJ_BASE:
	case OBJ_BAR_FORE:
	case OBJ_BAR_BACK:
	case OBJ_SIGNAL:
	case OBJ_GLOW:
	case OBJ_BASE_SHADOW:
	case OBJ_BAR_SHADOW:
		if (child_obj != NULL)
			*child_obj = obj;
		return this;
	default:
		return NULL;
	}
}

int32_t ProgressBar::SetColor_core(const math::v4 &v)
{
	int32_t ret = Widget::SetColor_core(v);

	for (vector<Widget *>::iterator it = m_parts.begin(); it != m_parts.end(); ++it) {
		Widget *part = *it;

		if (part != NULL)
			part->SetColor(v);
	}
	return ret;
}

/* the value kept between the minimum and the maximum, the fore bar's under the back bar's; the
   bar updated now unless sync_later, and the fore bar kept under a back bar set below it */
int32_t ProgressBar::SetValue_core(float value, bool sync_later, BarId id)
{
	if (id == BAR_BACK && m_bar_mode == BAR_MODE_SINGLE)
		return 0x80AF008E;

	float v = value;

	if (id == BAR_FORE && m_bar_mode == BAR_MODE_DOUBLE) {
		v = m_value[BAR_BACK];
		if (value < v)
			v = value;
	}

	float max = m_max;

	if (m_min > v)
		v = m_min;
	if (max > v)
		max = v;
	m_value[id] = max;
	s_async_lock.Lock();
	m_async_value[id] = -1.0f;
	s_async_lock.Unlock();
	m_rate[id] = m_value[id] / m_max;
	m_dirty |= (1 << id) | (1 << DIRTY_FLAG_LABEL);
	if (!sync_later)
		UpdateBar(id, false);
	if (id == BAR_BACK && m_value[BAR_FORE] > m_value[BAR_BACK])
		SetValue(m_value[BAR_BACK], sync_later, BAR_FORE);
	return 0;
}

}
}

using namespace paf;
using namespace paf::ui;

/* the glow fading out as the timer runs, from the intensity it was given */
extern "C" void ScePafWidget_43E9F757(Timer *t, AnimListener *listener, void *data1, void *data2,
			   void *data3)
{
	graph::DrawObj *draw = ((Widget *)data1)->GetDrawObj(ProgressBar::OBJ_GLOW);

	if (draw == NULL)
		return;

	union {
		void *data;
		float intensity;
	} u;

	u.data = data2;
	((GlowObj *)draw)->SetIntensity((1.0f - t->m_mapped_x) * u.intensity);
}

/* the signal's texture scrolled along it as the timer runs, one texture wide per its width */
extern "C" void ScePafWidget_A6EC5158(Timer *t, AnimListener *listener, void *data1, void *data2,
			   void *data3)
{
	float pos = t->m_mapped_x;
	graph::PlaneObj *signal = (graph::PlaneObj *)data2;
	intrusive_ptr<graph::Surface> tex;

	signal->GetTexture(tex, 0);
	if (tex.get() != NULL)
		signal->SetTexRegion(math::v4(pos, 0.0f,
					      signal->m_size.extract_x() / (float)tex->GetWidth(),
					      1.0f));
}

namespace paf {
namespace ui {

/* the Async setters' values applied, in the thread that draws */
void ProgressBar::Sync()
{
	float value[2];
	bool anim[2];

	s_async_lock.Lock();

	float max = m_async_max;
	float min = m_async_min;

	m_async_max = -1.0f;
	m_async_min = -1.0f;
	for (int32_t i = 0; i < 2; i++) {
		value[i] = m_async_value[i];
		anim[i] = m_async_anim[i];
		m_async_value[i] = -1.0f;
	}
	s_async_lock.Unlock();
	if (max >= 0.0f)
		SetMaxValue(max);
	if (min >= 0.0f)
		SetMinValue(min);
	for (int32_t i = 0; i < 2; i++) {
		if (value[i] >= 0.0f)
			SetValue(value[i], anim[i], (BarId)i);
	}
}

void ProgressBar::CheckDirtyFlag()
{
	if (m_dirty == 0)
		return;
	for (int32_t i = 0; i < 2; i++) {
		if (m_dirty & (1 << i))
			UpdateBar((BarId)i, true);
	}
	if (m_dirty & (1 << DIRTY_FLAG_LABEL))
		UpdateLabel();
	if (m_dirty & (1 << DIRTY_FLAG_LABEL_POS))
		UpdateLabelPos();
}

/* the base's shadow moved by the offset and grown by the margin */
void ProgressBar::UpdateBaseShadow()
{
	graph::DrawObj *shadow = GetDrawObj(OBJ_BASE_SHADOW);

	if (shadow == NULL)
		return;
	shadow->SetPos(math::v3(m_shadow_param.m_offset.extract_x(), m_shadow_param.m_offset.extract_y(), 0.0f));
	shadow->SetSize(math::v3((m_size + V4(m_shadow_param.m_margin))
					 .getImpl()
					 .getXYZ()));
}

/* the bar from the minimum to its value, its shadow, the signal over it and the glow at its end,
   moved over the bar's animation with anim; the glow fades out once the bar is full */
void ProgressBar::UpdateBar(BarId id, bool anim)
{
	m_dirty &= ~(1 << id);

	int32_t obj = OBJ_BAR_FORE + id;
	graph::DrawObj *bar = GetDrawObj(obj);

	if (bar == NULL)
		return;

	ProgressSignalObj *signal = NULL;
	GlowObj *glow = NULL;
	graph::DrawObj *shadow = NULL;

	if (id == BAR_FORE) {
		graph::DrawObj *draw = GetDrawObj(OBJ_SIGNAL);

		if (draw != NULL && draw->IsInherit(ProgressSignalObj::m_drawobj_type))
			signal = (ProgressSignalObj *)draw;
		draw = GetDrawObj(OBJ_GLOW);
		if (draw != NULL && draw->IsInherit(GlowObj::m_drawobj_type))
			glow = (GlowObj *)draw;
		shadow = GetDrawObj(OBJ_BAR_SHADOW);
	}

	float rate = m_rate[id];

	KillObjAnim(obj, 0x20100);
	KillObjAnim(obj, 0x20200);

	math::v4 size = MulPerElem(m_size, math::v4(rate - m_min_rate, 1.0f, 1.0f, 1.0f));
	math::v4 pos = MulPerElem(m_size, math::v4(m_min_rate - 0.5f, 0.0f, 0.0f, 0.0f));

	/* from the minimum */
	pos.set_x(pos.extract_x() + 0.5f * size.extract_x());
	if (anim) {
		SetObjAnim(obj, 0x20100, pos, new Timer(m_anim_time, m_anim_func, 0, -1, NULL), 2);
		SetObjAnim(obj, 0x20200, size, new Timer(m_anim_time, m_anim_func, 0, -1, NULL), 2);
	} else {
		bar->SetPos(math::v3(pos.getImpl().getXYZ()));
		bar->SetSize(math::v3(size.getImpl().getXYZ()));
	}
	if (shadow != NULL) {
		KillObjAnim(OBJ_BAR_SHADOW, 0x20100);
		KillObjAnim(OBJ_BAR_SHADOW, 0x20200);

		math::v4 shadow_pos = pos + V4(m_shadow_param.m_offset);
		math::v4 shadow_size = size + V4(m_shadow_param.m_margin);

		if (anim) {
			SetObjAnim(OBJ_BAR_SHADOW, 0x20100, shadow_pos,
				   new Timer(m_anim_time, m_anim_func, 0, -1, NULL), 2);
			SetObjAnim(OBJ_BAR_SHADOW, 0x20200, shadow_size,
				   new Timer(m_anim_time, m_anim_func, 0, -1, NULL), 2);
		} else {
			shadow->SetPos(math::v3(shadow_pos.getImpl().getXYZ()));
			shadow->SetSize(math::v3(shadow_size.getImpl().getXYZ()));
		}
	}
	if (signal != NULL) {
		KillObjAnim(OBJ_SIGNAL, 0x20100);
		KillObjAnim(OBJ_SIGNAL, 0x20200);
		if (anim) {
			SetObjAnim(OBJ_SIGNAL, 0x20100, pos,
				   new Timer(m_anim_time, m_anim_func, 0, -1, NULL), 2);
			SetObjAnim(OBJ_SIGNAL, 0x20200, size,
				   new Timer(m_anim_time, m_anim_func, 0, -1, NULL), 2);
		} else {
			signal->SetPos(math::v3(pos.getImpl().getXYZ()));
			signal->SetSize(math::v3(size.getImpl().getXYZ()));
		}

		/* as bright as the bar is long, full at a fifth of the base */
		float width = m_size.extract_x();

		if (width < 1.0f)
			width = 1.0f;

		float intensity = size.extract_x() / (width * 0.2f);

		if (intensity < 0.0f)
			intensity = 0.0f;
		signal->SetIntensity(intensity < 1.0f ? intensity : 1.0f);
	}
	if (glow != NULL) {
		KillObjAnim(OBJ_GLOW, 0x20100);
		KillAnim(ANIM_GLOW);

		/* at the end of the bar */
		math::v4 glow_pos = pos;

		glow_pos.set_x(pos.extract_x() + 0.5f * size.extract_x());
		if (anim)
			SetObjAnim(OBJ_GLOW, 0x20100, glow_pos,
				   new Timer(m_anim_time, m_anim_func, 0, -1, NULL), 2);
		else
			glow->SetPos(math::v3(glow_pos.getImpl().getXYZ()));
		if (glow->IsDirty())
			glow->Update();

		/* as bright as the bar is long against the glow */
		graph::DrawObjBounds bounds = glow->GetBounds();
		float glow_width = (bounds.m_max - bounds.m_min).extract_x();

		if (glow_width < 1.0f)
			glow_width = 1.0f;

		float intensity = size.extract_x() / glow_width;

		if (intensity < 0.0f)
			intensity = 0.0f;
		intensity = intensity < 1.0f ? intensity : 1.0f;
		glow->SetIntensity(intensity);
		if (rate == 1.0f) {
			union {
				void *data;
				float intensity;
			} u;

			u.intensity = intensity;

			AnimListener *listener =
				new AnimListener(new Timer(400.0f, Timer::FUNC_FASTUP, 0, -1, NULL),
						 this, ::ScePafWidget_43E9F757, NULL, u.data);

			if (anim)
				SetAnimListener_withDelay(ANIM_GLOW, listener, m_anim_time, 0);
			else
				SetAnimListener(ANIM_GLOW, listener, 0);
		}
	}
}

/* the common string's text with "%1" the percentage and "%%" a percent sign, as the label's
   (inlined in Sony's code, name ours) */
static inline void SetPercentText(Widget *label, const wchar_t *src, int32_t percent)
{
	wstring text;

	for (;;) {
		const wchar_t *p = sce_paf_wcschr(src, L'%');

		if (p == NULL) {
			text.append(src, sce_paf_wcslen(src));
			label->SetString(text);
			break;
		}
		if (p[1] == L'%') {
			text.append(1, L'%');
		} else if (p[1] == L'1') {
			wchar_t buffer[32];

			sce_paf_swprintf(buffer, 32, L"%d", percent);
			text.append(buffer, sce_paf_wcslen(buffer));
		} else if (p[1] == 0) {
			/* a lone '%' ends it */
			label->SetString(text);
			break;
		}
		src = p + 2;
	}
}

/* the label's text: the rate as the common string's percentage, the value over the maximum,
   what the listeners of CB_PROGRESSBAR_LABEL set, or the page over the pages */
void ProgressBar::UpdateLabel()
{
	m_dirty &= ~(1 << DIRTY_FLAG_LABEL);

	Widget *label = m_parts[0];

	if (label == NULL)
		return;
	if (m_label_mode == LABEL_MODE_PERCENTAGE) {
		const wchar_t *src =
			Framework::Instance()->GetCommonString(IDParam("msg_variable_percent"));

		SetPercentText(label, src, (int32_t)(m_rate[BAR_FORE] * 100.0f));
	} else {
		switch (m_label_mode) {
		case LABEL_MODE_VALUE: {
			wchar_t buffer[32];

			sce_paf_swprintf(buffer, 32, L"%d/%d", (int32_t)m_value[BAR_FORE],
					 (int32_t)m_max);
			label->SetString(wstring(buffer));
			break;
		}
		case LABEL_MODE_PAGE: {
			int32_t max = (int32_t)m_max;

			if (max > 1) {
				if (!label->m_transition_complete) {
					label->m_transition = 0xff;
					label->m_transition_complete = true;
					label->DoTransition_core(0.0f, true);
				}
				ScePafWidget_FF5BC957(label, NULL, 2, 0.0f, NULL);

				int32_t page = (int32_t)m_value[BAR_FORE] + 1;

				if (page > max)
					page = max;

				wchar_t buffer[32];

				sce_paf_swprintf(buffer, 32, L"%d/%d", page, max);
				label->SetString(wstring(buffer));
			} else {
				label->m_transition_complete = false;
				label->m_transition = 0xff;
				label->DoTransitionReverse_core(0.0f, true);
				ScePafWidget_4588699E(label, NULL, 2, 0.0f, NULL);
			}
			break;
		}
		case LABEL_MODE_MANUAL: {
			Event ev(0x8000, Event::MODE_DISPATCH, Event::ROUTE_FORWARD, 0);

			DoEvent(CB_PROGRESSBAR_LABEL, &ev);
			break;
		}
		default:
			break;
		}
	}
}

/* the label placed by its position mode: off the centre by halves of the bar and margins */
void ProgressBar::UpdateLabelPos()
{
	Widget *label = m_parts[0];

	m_dirty &= ~(1 << DIRTY_FLAG_LABEL_POS);
	if (label == NULL)
		return;
	if (m_label_pos_mode == LABEL_POS_MODE_MANUAL)
		return;

	const LabelPos &entry = c_labelPosTable[m_label_pos_mode];

	label->SetPos(m_origin_offs +
		      MulPerElem(MulPerElem(V4(entry.m_pos), m_size),
				 math::v4(0.5f, 0.5f, 0.5f, 0.5f)) +
		      V4(entry.m_margin) * m_label_margin);
	label->SetAlign(entry.m_align_x, entry.m_align_y, 0, NULL);
}

const char *ProgressBarParam::GetParamName() const
{
	return m_param_name;
}

const char *ProgressBarParam::ParamName()
{
	return m_param_name;
}

int32_t ProgressBarParam::Create(int32_t mode)
{
	int32_t ret = WidgetParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL) {
		m_serial->AddParam("bar_mode", &m_bar_mode, ResourceParam::SEMANTIC_INT);
		if (m_serial != NULL) {
			m_serial->AddParam("w_label", &m_label, ResourceParam::SEMANTIC_WIDGETREF);
			if (m_serial != NULL) {
				m_serial->AddParam("label_mode", &m_label_mode,
						   ResourceParam::SEMANTIC_INT);
				if (m_serial != NULL) {
					m_serial->AddParam("label_pos_mode", &m_label_pos_mode,
							   ResourceParam::SEMANTIC_INT);
					if (m_serial != NULL) {
						m_serial->AddParam("label_margin", &m_label_margin,
								   ResourceParam::SEMANTIC_FLOAT);
						if (m_serial != NULL)
							m_serial->AddParam(
								"signal_anim", &m_signal_anim,
								ResourceParam::SEMANTIC_BOOL);
					}
				}
			}
		}
	}
	return 0;
}

int32_t ProgressBarParam::Set()
{
	int32_t ret = WidgetParam::Set();

	if (ret < 0)
		return ret;

	ProgressBar *bar = (ProgressBar *)m_widget;

	bar->SetBarMode((ProgressBar::BarMode)m_bar_mode);
	bar->SetLabelMode((ProgressBar::LabelMode)m_label_mode);
	bar->SetLabelPosMode((ProgressBar::LabelPosMode)m_label_pos_mode);
	bar->SetLabelMargin(m_label_margin);
	bar->SetSignalAnim(m_signal_anim);
	if (m_label.GetIDHash() != 0) {
		Widget *label = bar->FindChild(m_label, 0);

		if (label != NULL)
			bar->AttachLabelWidget(label);
	}
	return 0;
}

int32_t ProgressBarParam::Get()
{
	int32_t ret = WidgetParam::Get();

	if (ret < 0)
		return ret;

	ProgressBar *bar = (ProgressBar *)m_widget;

	m_bar_mode = bar->GetBarMode();
	m_label_mode = bar->GetLabelMode();
	m_label_pos_mode = bar->GetLabelPosMode();
	m_label_margin = bar->GetLabelMargin();
	m_signal_anim = bar->m_signal_anim;
	return 0;
}

const char *ProgressBarStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *ProgressBarStyleParam::ParamName()
{
	return m_param_name;
}

int32_t ProgressBarStyleParam::Create(int32_t mode)
{
	int32_t ret = WidgetStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL) {
		m_serial->AddParam("anim_time", &m_anim_time, ResourceParam::SEMANTIC_FLOAT);
		if (m_serial != NULL) {
			m_serial->AddParam("anim_func", &m_anim_func, ResourceParam::SEMANTIC_INT);
			if (m_serial != NULL) {
				m_serial->AddParam("signal_anim_time", &m_signal_anim_time,
						   ResourceParam::SEMANTIC_FLOAT);
				if (m_serial != NULL) {
					m_serial->AddParam("shadow_offset",
							   &m_shadow_param.m_offset,
							   ResourceParam::SEMANTIC_VEC2);
					if (m_serial != NULL)
						m_serial->AddParam("shadow_margin",
								   &m_shadow_param.m_margin,
								   ResourceParam::SEMANTIC_VEC2);
				}
			}
		}
	}
	return 0;
}

/* the draw objects the style names made, the glow shown and the shadows shown where they are
   made */
int32_t ProgressBarStyleParam::SetEach(Widget *widget)
{
	ProgressBar *bar = (ProgressBar *)widget;
	graph::DrawObjParam *param = GetDrawObjParam(ProgressBar::OBJ_BAR_BACK);

	if (param != NULL && param->m_id.GetIDHash() != 0)
		bar->ScePafWidget_536C8866();
	param = GetDrawObjParam(ProgressBar::OBJ_SIGNAL);
	if (param != NULL && param->m_id.GetIDHash() != 0)
		bar->ScePafWidget_7BBE5D41();
	param = GetDrawObjParam(ProgressBar::OBJ_GLOW);
	if (param != NULL && param->m_id.GetIDHash() != 0)
		bar->ScePafWidget_16ACDAD6();
	param = GetDrawObjParam(ProgressBar::OBJ_BASE_SHADOW);
	if (param != NULL && param->m_id.GetIDHash() != 0)
		bar->ScePafWidget_1E040ECD();
	param = GetDrawObjParam(ProgressBar::OBJ_BAR_SHADOW);
	if (param != NULL && param->m_id.GetIDHash() != 0)
		bar->ScePafWidget_1FD5D71C();

	int32_t ret = WidgetStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	bar->SetBarAnim(m_anim_time, m_anim_func);
	bar->m_signal_anim_time = m_signal_anim_time;
	if (bar->m_signal_anim)
		bar->SetSignalAnim(true);
	bar->m_shadow_param = m_shadow_param;
	bar->UpdateBaseShadow();
	bar->UpdateBar(ProgressBar::BAR_FORE, false);
	if (bar->GetDrawObj(ProgressBar::OBJ_GLOW) != NULL)
		bar->SetGlowVisible(true);
	if (bar->GetDrawObj(ProgressBar::OBJ_BASE_SHADOW) != NULL ||
	    bar->GetDrawObj(ProgressBar::OBJ_BAR_SHADOW) != NULL)
		bar->SetShadowVisible(true);
	return 0;
}

int32_t ProgressBarStyleParam::GetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;

	ProgressBar *bar = (ProgressBar *)widget;

	bar->GetBarAnim(&m_anim_time, &m_anim_func);
	m_signal_anim_time = bar->m_signal_anim_time;
	m_shadow_param = bar->m_shadow_param;
	return 0;
}

/* the signal's first texture in the colour of the scene, offset and scaled into its region:
   the programs made the first time */
static int32_t DrawGlow(GlowShader *shader, graph::PlaneObjDrawParams &params)
{
	graph::arl::ArlInstance *gl = graph::arl::Gl();
	graph::arl::ProgramState *state;

	if (shader->m_programs == NULL) {
		shader->m_programs = new GlowShader::Programs;
		if (shader->m_programs == NULL)
			return 0x80AF0022;

		common::SharedPtr<graph::arl::VertexArraySpec> spec = params.m_vertices->m_spec;

		state = gl;
		shader->m_programs->m_vert.m_object.init(
			*gl, state->m_patcher, spec, common::SharedPtr<graph::arl::VertexArraySpec>(),
			common::SharedPtr<graph::arl::VertexArraySpec>(),
			common::SharedPtr<graph::arl::VertexArraySpec>());
		state = gl;
		shader->m_programs->m_frag.m_object.init(*gl, state->m_patcher);
	}
	params.m_textures[0]->Texturize();
	shader->m_programs->m_vert.m_interface.m_mvp.Set(gl->GetModelViewProjection());

	shader->m_programs->m_vert.m_interface.m_uv_offset.Set(
		math::v2(params.m_tex_region->extract_x(), params.m_tex_region->extract_y()) *
		params.m_textures[0]->GetFactor());
	shader->m_programs->m_vert.m_interface.m_uv_scale.Set(
		math::v2(params.m_tex_region->extract_z(), params.m_tex_region->extract_w()) *
		params.m_textures[0]->GetFactor());
	shader->m_programs->m_frag.m_interface.m_color.Set(gl->m_color);
	shader->m_programs->m_frag.m_interface.m_texture0.Set(
		intrusive_ptr<graph::Surface>(params.m_textures[0]));

	shader->m_programs->m_vert.m_object.begin();
	shader->m_programs->m_frag.m_object.begin();
	for (uint32_t i = 0; i < params.m_instances; i++) {
		graph::arl::ShaderState *state;

		state = gl;
		state->m_dirty_vertex |= params.m_vertices[i].m_spec->m_kinds;
		state = gl;
		state->m_dirty_fragment |= 1;
		sceGxmSetVertexStream(gl->m_context, 0,
				      params.m_vertices[i].m_buffer->m_memory->m_addr);
		if (params.m_indices != NULL)
			params.m_indices[i].draw(*gl, graph::arl::TRIANGLES, params.m_count);
		else
			graph::arl::VertexArray::DrawArrays(*gl, graph::arl::TRIANGLE_STRIP, 0,
							    params.m_count);
		state = gl;
		state->m_dirty_vertex = 0;
		state = gl;
		state->m_dirty_fragment = 0;
	}
	shader->m_programs->m_frag.m_object.end();
	shader->m_programs->m_vert.m_object.end();
	return 0;
}

int32_t GlowShader::Draw(graph::PlaneObjDrawParams &params)
{
	if (params.m_textures == NULL || params.m_textures[0].get() == NULL ||
	    params.m_tex_region == NULL)
		return 0x80AF008F;
	return DrawGlow(this, params);
}

bool GlowObj::IsInherit(const char *type) const
{
	if (m_drawobj_type == type)
		return true;
	return graph::PlaneObj::IsInherit(type);
}

const char *GlowObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

void GlowObj::OnDraw(const math::matrix &parent, uint32_t color, float alpha,
		     graph::DrawInfo &info)
{
	/* as bright as its intensity */
	graph::PlaneObj::OnDraw(parent, color, alpha * m_intensity, info);
}

bool ProgressSignalObj::IsInherit(const char *type) const
{
	if (m_drawobj_type == type)
		return true;
	return GlowObj::IsInherit(type);
}

const char *ProgressSignalObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

bool BarObj::IsInherit(const char *type) const
{
	if (m_drawobj_type == type)
		return true;
	return graph::PlaneObj::IsInherit(type);
}

const char *BarObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

/* a divided bar narrower than its borders drawn with them narrowed to its width, its texture's
   ratios put back after */
void BarObj::OnDraw(const math::matrix &parent, uint32_t color, float alpha,
		    graph::DrawInfo &info)
{
	/* none without a texture */
	uint32_t mode = 0;

	if (m_divide != NULL)
		mode = m_divide->m_div_mode;
	else if (m_tex_surface[0].get() != NULL)
		mode = m_tex_surface[0]->GetDivideMode();
	if ((mode != 2 && mode != 3 && mode != 6 && mode != 7 && mode != 9) ||
	    m_tex_surface[0].get() == NULL ||
	    (!m_plane_flags.m_layout_dirty && !m_plane_flags.m_texcoord_dirty &&
	     !m_plane_flags.m_position_dirty)) {
		graph::PlaneObj::OnDraw(parent, color, alpha, info);
		return;
	}
	if (!m_flags.m_visible || m_shader.get() == NULL || !(alpha > 0.0f) ||
	    (m_color >> 24) <= 1)
		return;

	math::v2 ratio_h;
	math::v2 ratio_v;

	if (m_divide != NULL) {
		ratio_h = m_divide->m_div_border_ratio_h;
		ratio_v = m_divide->m_div_border_ratio_v;
	} else {
		ScePafGraphics_AFAE7807(m_tex_surface[0].get(), &ratio_h, &ratio_v);
	}

	uint32_t margin = m_tex_surface[0]->GetMarginLeft() + m_tex_surface[0]->GetMarginRight();
	math::v2 border = ratio_h * ((float)m_tex_surface[0]->GetWidth() * m_div_border_scale);
	float border_width = border.extract_x() + border.extract_y();
	float width = m_size.extract_x() + (float)margin;

	if (width >= border_width) {
		if (m_clipped) {
			m_plane_flags.m_texcoord_dirty = true;
			m_clipped = false;
		}
	} else {
		m_clipped = true;
		m_plane_flags.m_texcoord_dirty = true;
		ScePafGraphics_36960B24(m_tex_surface[0].get(), ratio_h * (width / border_width), ratio_v);
	}
	graph::PlaneObj::OnDraw(parent, color, alpha, info);
	ScePafGraphics_36960B24(m_tex_surface[0].get(), ratio_h, ratio_v);
}

}
}

static Widget *NewProgressBar(Widget *parent)
{
	return new ProgressBar(parent, NULL);
}

static WidgetParam *NewProgressBarParam()
{
	return new ProgressBarParam;
}

static VersatileParam *NewProgressBarLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewProgressBarFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewProgressBarStyleParam()
{
	return new ProgressBarStyleParam;
}

static VersatileParam *NewProgressBarHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewProgressBarSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryProgressBar = {
	ProgressBar::TypeName(),
	NewProgressBar,
	NewProgressBarParam,
	NewProgressBarLayoutParam,
	NewProgressBarFocusParam,
	NewProgressBarStyleParam,
	NewProgressBarHandlerParam,
	NewProgressBarSoundParam,
};

/* where each LabelPosMode puts the label */
const ProgressBar::LabelPos ProgressBar::c_labelPosTable[] = {
	{ math::v2(0.0f, 0.0f), math::v2(0.0f, 0.0f), 0, 0 },
	{ math::v2(-1.0f, 0.0f), math::v2(-1.0f, 0.0f), 2, 0 },
	{ math::v2(1.0f, 0.0f), math::v2(1.0f, 0.0f), 1, 0 },
	{ math::v2(0.0f, 1.0f), math::v2(0.0f, 1.0f), 0, 1 },
	{ math::v2(0.0f, -1.0f), math::v2(0.0f, -1.0f), 0, 2 },
	{ math::v2(-1.0f, 1.0f), math::v2(0.0f, 1.0f), 1, 1 },
	{ math::v2(-1.0f, -1.0f), math::v2(0.0f, -1.0f), 1, 2 },
	{ math::v2(1.0f, 1.0f), math::v2(0.0f, 1.0f), 2, 1 },
	{ math::v2(1.0f, -1.0f), math::v2(0.0f, -1.0f), 2, 2 },
};

thread::Mutex ProgressBar::s_async_lock("ScePafProgressBar", 0);
