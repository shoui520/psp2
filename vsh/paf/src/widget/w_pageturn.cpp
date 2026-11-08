/*
 * paf::ui::PageTurn (DEBUGGER 0x81101170..0x81102c1c): the page turning over: its plane, the
 * turn object folding its corner and its label, the corner's animations, its transitions and
 * its resource and style parameters, and its factories. No record or string names the file;
 * the names of the static functions are ours.
 */
/* Sony's file has no copy of the arl tile size (arl_defines.h) */
#define PAF_ARL_NO_TILE_SIZE

#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/page_turn_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <widget/core/animlistener.h>
#include <widget/core/widget.h>
#include <widget/factory.h>
#include <widget/w_pageturn.h>
#include <widget/w_scene.h>

using namespace sce::Vectormath::Simd::Aos;

namespace paf {
namespace ui {

/* how the corner and its offset are animated (names ours) */
static AnimPropertyV4 s_turn_prop = {
	PageTurn::ANIM_TURN,
	ScePafWidget_6CC95725,
	ScePafWidget_3D6C290E,
};

static AnimPropertyV4 s_turn_offset_prop = {
	PageTurn::ANIM_TURN_OFFSET,
	ScePafWidget_4EA317B8,
	ScePafWidget_6206FB7E,
};

/* the vector with the two as its x and y (name ours) */
static inline math::v4 XY(const Vector2 &xy)
{
	return math::v4(Vector4(xy, Vector2(0.0f)));
}

/* where the corner rests: the page's top right, less the corner's width and height, the
   height the part of the page's height and the width the aspect of it (name ours) */
static inline Vector2 CornerPos(const Vector2 &size, float ratio, float aspect)
{
	float height = (float)size.getY() * ratio;

	return size * 0.5f - Vector2(height * aspect, height);
}

/* whether the two are equal, compared lane by lane (name ours) */
static inline bool IsSame(const Vector2 &a, const Vector2 &b)
{
	return ((a.getX() == b.getX()) & (a.getY() == b.getY())).getAsBool();
}

int32_t PageTurn::ScePafWidget_B0E5E134(const math::v4 &turn, Timer *t, uint32_t id, float delay,
					int32_t slot, AnimCB cb, void *data)
{
	if (t == NULL) {
		KillAnimType(ANIM_TURN);
		m_turn = math::v2(turn.getImpl().getXY());
		return 0;
	}

	AnimListenerV4 *anim = new AnimListenerV4(t, this, XY(m_turn.getImpl()), turn,
						  &s_turn_prop, cb, data);

	if (id & 0xFF0000)
		id = (id & 0xFF000000) | ANIM_TURN;
	SetAnimListener_withDelay(id, anim, delay, slot);
	return 0;
}

void PageTurn::ScePafWidget_F378C87D(float ratio, float aspect)
{
	Vector2 size = m_size.getImpl().getXY();

	m_fold_ratio = ratio;
	m_fold_aspect = aspect;

	float height = (float)size.getY() * ratio;

	m_corner_width = height + 50.0f;
	m_corner_height = height + 50.0f;
	m_corner_margin = height * 0.5f - 10.0f;
	if (m_turn_state != ST_PAGETURN_TOUCH && m_turn_state != ST_PAGETURN_KEY)
		ScePafWidget_B0E5E134(XY(size * 0.5f - Vector2(height * aspect, height)), NULL,
				      ANIM_TURN, 0.0f, 2, NULL, NULL);
}

PageTurn::PageTurn(Widget *parent, CreateParam *param)
	: PageTurnBase(parent, param), m_fold_ratio(0.15f), m_fold_aspect(0.8f), m_shaking(false),
	  m_drag_offset(0.0f, 0.0f), m_turn(0.0f, 0.0f), m_turn_offset(0.0f, 0.0f),
	  m_last_pos(0.0f, 0.0f), m_velocity(0.0f, 0.0f), m_acceleration(0.0f, 0.0f),
	  m_turn_obj(NULL), m_plane(NULL)
{
	graph::DrawStyle style;

	style.blend = 1;
	style.depth = 1;
	style.lighting = 0;
	style.face_cull = 0;
	style.scissor = 0;
	style.deepen_color = 0;
	style.reduce_transparency = 0;
	style.surface_draw = 0;
	style.reserved1 = 0;

	graph::PlaneObj *plane = new graph::PlaneObj(style);

	if (plane != NULL) {
		AddDrawObj(DrawObjEntry(plane, OBJ_PLANE), false);
		plane->SetVisible(false);
		m_plane = plane;
	}

	graph::PageTurnObj *turn = new graph::PageTurnObj(style);

	if (turn != NULL) {
		AddDrawObj(DrawObjEntry(turn, OBJ_TURN), false);
		m_turn_obj = turn;
	}

	graph::DrawStyle label_style;

	label_style.blend = 1;
	label_style.depth = 1;
	label_style.lighting = 0;
	label_style.face_cull = 0;
	label_style.scissor = 0;
	label_style.deepen_color = 0;
	label_style.reduce_transparency = 0;
	label_style.surface_draw = 0;
	label_style.reserved1 = 0;

	graph::TextObj *label = new graph::TextObj(label_style);

	if (label != NULL) {
		common::SharedPtr<graph::DrawObj> ptr((graph::DrawObj *)label);
		DrawObjAttr attr(OBJ_LABEL);

		attr.m_flag1 = false;
		AddDrawObj(DrawObjEntry(ptr, attr), false);
	}
	ScePafWidget_F378C87D(0.15f, 0.8f);
}

/* the strings and the textures a page's resource gives its draw objects */
static const struct {
	const char *name;
	int32_t index;
} s_pageturn_strings[] = {
	{ "label", PageTurn::OBJ_LABEL },
};

static const struct {
	const char *name;
	int32_t index;
} s_pageturn_textures[] = {
	{ "texture", PageTurn::OBJ_TURN },
};

PageTurnParam::PageTurnParam()
{
	for (uint32_t i = 0; i < sizeof(s_pageturn_strings) / sizeof(s_pageturn_strings[0]); i++)
		m_string_list.push_back(IndexedRef(s_pageturn_strings[i].name,
						   s_pageturn_strings[i].index));
	for (uint32_t i = 0; i < sizeof(s_pageturn_textures) / sizeof(s_pageturn_textures[0]); i++)
		m_texture_list.push_back(IndexedRef(s_pageturn_textures[i].name,
						    s_pageturn_textures[i].index));
}

/* the parameter named and indexed, among the style's draw object parameters (inlined in Sony's
   code, name ours) */
static inline __attribute__((always_inline)) void AddObjParam(WidgetStyleParam *style,
							      graph::DrawObjParam *param,
							      const char *name, int32_t index)
{
	param->m_name = name;
	param->m_index = index;
	style->m_drawobj_param_list.push_back(param);
}

/* the style's draw object parameters by name */
static const char *const s_pageturn_objs[] = {
	"turn_obj",
	"label_obj",
};

void PageTurnStyleParam::InitParam()
{
	AddObjParam(this, &m_turn_obj, s_pageturn_objs[0], PageTurn::OBJ_TURN);
	AddObjParam(this, &m_label_obj, s_pageturn_objs[1], PageTurn::OBJ_LABEL);
}

const char PageTurn::m_widget_type[] = "pageturn";

bool PageTurn::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return PageTurnBase::IsInherit(type);
}

char *PageTurn::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *PageTurn::TypeName()
{
	return m_widget_type;
}

PageTurn::~PageTurn()
{
}

int32_t PageTurn::ScePafWidget_AD9098A3(const math::v4 &offset, Timer *t, uint32_t id,
					float delay, int32_t slot, AnimCB cb, void *data)
{
	if (t == NULL) {
		KillAnimType(ANIM_TURN_OFFSET);
		m_turn_offset = math::v2(offset.getImpl().getXY());
		return 0;
	}

	AnimListenerV4 *anim = new AnimListenerV4(t, this, XY(m_turn_offset.getImpl()),
						  offset, &s_turn_offset_prop, cb, data);

	if (id & 0xFF0000)
		id = (id & 0xFF000000) | ANIM_TURN_OFFSET;
	SetAnimListener_withDelay(id, anim, delay, slot);
	return 0;
}

void PageTurn::UpdateAfterChildren()
{
	int32_t state = m_turn_state;

	if (state == ST_PAGETURN_TOUCH || state == ST_PAGETURN_KEY) {
		m_turn = m_current + m_drag_offset + (m_current - m_start) * 0.7f;
	}
	PageTurnBase::UpdateAfterChildren();
	if (m_turn_state == ST_PAGETURN_DISABLE)
		return;

	float rate = 1.0f / Timer::s_tick_interval;
	Vector2 pos = GetWorldMatrix().w().getImpl().getXY();
	Vector2 last_velocity = m_velocity.getImpl();
	Vector2 velocity = (pos - m_last_pos.getImpl()) * rate;

	m_last_pos = math::v2(pos);
	m_velocity = math::v2(velocity);

	Vector2 acceleration = velocity - last_velocity;

	m_acceleration = math::v2(acceleration);

	float accel = acceleration.getX();
	float strength = __builtin_fabsf(accel);

	/* a quick move shakes the corner the other way, a part of the way to the page's corner */
	if (strength > 50.0f) {
		KillAnim(ANIM_TURN_OFFSET);

		Vector2 size = m_size.getImpl().getXY();
		float excess = accel + 50.0f;

		if (accel > 0.0f)
			excess = accel - 50.0f;

		float scale = excess * 0.01f;

		if (scale < -1.0f)
			scale = -1.0f;
		scale = scale < 1.0f ? scale : 1.0f;

		Vector2 corner = CornerPos(size, m_fold_ratio, m_fold_aspect);
		Vector2 offset = (corner - size) * 0.06f * scale;

		ScePafWidget_AD9098A3(XY(offset), new Timer(200.0f, Timer::FUNC_EXPO_OUT_29),
				      ANIM_TURN_OFFSET, 0.0f, 0, NULL, NULL);
		m_shaking = true;
	}
	/* and settles once it slows down */
	if (m_shaking && strength < 3.15f) {
		KillAnim(ANIM_TURN_OFFSET);
		ScePafWidget_AD9098A3(XY(Vector2(0.0f)),
				      new Timer(1800.0f, Timer::FUNC_ELASTIC_OUT), ANIM_TURN_OFFSET,
				      0.0f, 0, NULL, NULL);
		m_shaking = false;
	}
}

void PageTurn::UpdateSize()
{
	ScePafWidget_F378C87D(m_fold_ratio, m_fold_aspect);
	if (m_turn_obj != NULL)
		m_turn_obj->SetSize(math::v3(m_size.getImpl().getXYZ()));
	if (m_plane != NULL)
		m_plane->SetSize(math::v3(m_size.getImpl().getXYZ()));
}

int32_t PageTurn::OnDraw(graph::DrawInfo &info)
{
	Vector2 size = m_size.getImpl().getXY();
	graph::PageTurnObj *turn_obj = m_turn_obj;
	uint32_t color = 0xFFFFFFFF;

	/* the plane is drawn opaque only while the page is */
	if (m_color_alpha * m_meta_a_sum * m_show_a_sum < 0.999f)
		color = 0x00FFFFFF;
	m_plane->m_color = color;

	Vector2 scale = divPerElem(Vector2(1.0f),
				   maxPerElem(Vector2(1.0f),
					      minPerElem(Vector2(3.4028236e18f), size)));
	Vector2 turn = mulPerElem(m_turn.getImpl() + size * 0.5f, scale);
	Vector2 offset = mulPerElem(m_turn_offset.getImpl(), scale);

	if (turn_obj != NULL) {
		if (!IsSame(turn, turn_obj->m_turn.getImpl())) {
			turn_obj->m_turn = math::v2(turn);
			turn_obj->m_turn_changed = true;
		}
		if (!IsSame(offset, turn_obj->m_turn_offset.getImpl())) {
			turn_obj->m_turn_offset = math::v2(offset);
			turn_obj->m_turn_changed = true;
		}
	}
	return Widget::OnDraw(info);
}

float PageTurn::DoTransition_core(float parameter, bool set)
{
	float time = 0.0f;

	if (m_plane != NULL && !m_plane->m_flags.m_visible)
		return 0.0f;
	KillAnim(ANIM_TURN_OFFSET);
	if (set && parameter <= 0.0f) {
		time = 0.0f;
		SetMetaAlpha(1.0f, NULL, ANIM_META_ALPHA, 0, time);

		math::v4 pos = m_pos;

		pos.set_z(0.0f);
		SetPos(pos, NULL, ANIM_MOVE, 0, time);
		ScePafWidget_B0E5E134(XY(CornerPos(m_size.getImpl().getXY(), m_fold_ratio,
						   m_fold_aspect)),
				      NULL, ANIM_TURN, time, 2, NULL, NULL);
	} else {
		if (!set)
			time = 600.0f;

		float half = time * 0.5f;
		float delay = half + parameter;

		SetMetaAlpha(1.0f, new Timer(half, Timer::FUNC_QUAD_IN), ANIM_META_ALPHA, 2,
			     parameter);

		math::v4 from = m_pos;

		from.set_z(200.0f);
		SetPos(from, NULL, ANIM_MOVE, 0, 0.0f);

		math::v4 to = m_pos;

		to.set_z(0.0f);
		SetPos(to, new Timer(half, Timer::FUNC_SINE_OUT), ANIM_MOVE, 2, delay);
		ScePafWidget_B0E5E134(XY(CornerPos(m_size.getImpl().getXY(), m_fold_ratio,
						   m_fold_aspect)),
				      new Timer(half, Timer::FUNC_QUAD_OUT), ANIM_TURN, delay, 2,
				      NULL, NULL);
	}
	return parameter + time;
}

float PageTurn::DoTransitionReverse_core(float parameter, bool set)
{
	float time = 0.0f;

	if (m_plane != NULL && !m_plane->m_flags.m_visible)
		return 0.0f;

	SceneResolution *resolution = ScePafWidget_CDC90BA2(m_scene->m_env_param,
							    m_scene->m_resolution_id);

	KillAnim(ANIM_TURN_OFFSET);

	/* the corner turned over the whole page: twice the screen, its axes swapped */
	Vector2 turn((float)-resolution->m_height * 2.0f, (float)-resolution->m_width * 2.0f);

	if (m_scene != NULL) {
		Vector2 size = m_scene->GetSize(0)->getImpl().getXY();

		turn = Vector2(size.getY(), size.getX()) * -2.0f;
	}
	if (set && parameter <= 0.0f) {
		time = 0.0f;
		SetMetaAlpha(time, NULL, ANIM_META_ALPHA, 0, time);

		math::v4 pos = m_pos;

		pos.set_z(pos.extract_z() + 200.0f);
		SetPos(pos, NULL, ANIM_MOVE, 0, time);
		ScePafWidget_B0E5E134(XY(turn), NULL, ANIM_TURN, time, 2, NULL, NULL);
	} else {
		if (!set)
			time = 600.0f;
		ScePafWidget_B0E5E134(XY(turn), new Timer(time * 0.8333333f, Timer::FUNC_SINE_OUT),
				      ANIM_TURN, parameter, 2, NULL, NULL);

		math::v4 to = m_pos;

		to.set_z(200.0f);
		SetPos(to, new Timer(time * 0.5f, Timer::FUNC_EXPO_IN_15), ANIM_MOVE, 2, parameter);
		SetMetaAlpha(0.0f, new Timer(time * 0.5f, Timer::FUNC_QUAD_OUT), ANIM_META_ALPHA, 2,
			     time * 0.45f + parameter);
	}
	return parameter + time;
}

int32_t PageTurn::GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return 0x80AF4101;
	draw->GetTexture(surf, index);
	return 0;
}

int32_t PageTurn::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
			     uint32_t index)
{
	if (obj == OBJ_TURN && m_plane != NULL)
		m_plane->SetTexture(surf, 0);

	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw != NULL)
		draw->SetTexture(surf, index);
	return 0;
}

}
}

using namespace paf;
using namespace paf::ui;

void ScePafWidget_6CC95725(AnimListener *anim, math::v4 data)
{
	((PageTurn *)anim->m_widget)->m_turn = math::v2(data.getImpl().getXY());
}

math::v4 ScePafWidget_3D6C290E(AnimListener *anim)
{
	return XY(((PageTurn *)anim->m_widget)->m_turn.getImpl());
}

void ScePafWidget_4EA317B8(AnimListener *anim, math::v4 data)
{
	((PageTurn *)anim->m_widget)->m_turn_offset = math::v2(data.getImpl().getXY());
}

math::v4 ScePafWidget_6206FB7E(AnimListener *anim)
{
	return XY(((PageTurn *)anim->m_widget)->m_turn_offset.getImpl());
}

namespace paf {
namespace ui {

int32_t PageTurn::ScePafWidget_A4245073(int32_t state)
{
	if (m_turn_state != state) {
		switch (state) {
		case ST_PAGETURN_IDLE: {
			/* back to its place: the further it was dragged, the longer it takes */
			Vector2 size = m_size.getImpl().getXY();
			Vector2 half = size * 0.5f;

			m_start = math::v2(-1.0f, -1.0f);
			m_drag_offset = math::v2(0.0f, 0.0f);

			float ratio = m_fold_ratio;
			float aspect = m_fold_aspect;
			float spring = ((float)length(m_current.getImpl() - half) /
						(float)size.getY() -
					ratio) *
				       1.5f;
			float time = 400.0f * ((spring + 0.5f) / 1.5f);
			Vector2 target = CornerPos(size, ratio, aspect);
			Vector2 offset = (CornerPos(size, ratio * 0.25f, aspect) - target) * spring;

			ScePafWidget_B0E5E134(XY(target), new Timer(time, Timer::FUNC_QUAD_OUT),
					      ANIM_TURN, 0.0f, 2, NULL, NULL);
			KillAnim(ANIM_TURN_OFFSET);
			ScePafWidget_AD9098A3(XY(offset),
					      new Timer(time * 0.9f, Timer::FUNC_QUAD_OUT),
					      ANIM_TURN_OFFSET, 0.0f, 0, NULL, NULL);
			ScePafWidget_AD9098A3(XY(Vector2(0.0f)),
					      new Timer(1800.0f, Timer::FUNC_ELASTIC_OUT),
					      ANIM_TURN_OFFSET, 0.0f, 0, NULL, NULL);
			break;
		}
		case ST_PAGETURN_TOUCH:
		case ST_PAGETURN_KEY:
			m_drag_offset = math::v2(m_turn.getImpl() - m_start.getImpl());
			KillAnim(ANIM_TURN);
			KillAnim(ANIM_TURN_OFFSET);
			ScePafWidget_AD9098A3(XY(Vector2(0.0f)),
					      new Timer(300.0f, Timer::FUNC_LINEAR),
					      ANIM_TURN_OFFSET, 0.0f, 2, NULL, NULL);
			break;
		case ST_PAGETURN_DISABLE:
			/* opened to the middle */
			KillAnim(ANIM_TURN);
			KillAnim(ANIM_TURN_OFFSET);
			ScePafWidget_AD9098A3(XY(Vector2(0.0f)),
					      new Timer(300.0f, Timer::FUNC_LINEAR),
					      ANIM_TURN_OFFSET, 0.0f, 2, NULL, NULL);
			ScePafWidget_B0E5E134(math::v4(m_size.getImpl() * 0.495f),
					      new Timer(300.0f, Timer::FUNC_QUAD_OUT), ANIM_TURN,
					      0.0f, 2, NULL, NULL);
			break;
		}
	}
	PageTurnBase::ScePafWidget_A4245073(state);
	return m_turn_state;
}

const char PageTurnParam::m_param_name[] = "pageturn";

const char *PageTurnParam::GetParamName() const
{
	return m_param_name;
}

const char *PageTurnParam::ParamName()
{
	return m_param_name;
}

const char PageTurnStyleParam::m_param_name[] = "style_pageturn";

const char *PageTurnStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *PageTurnStyleParam::ParamName()
{
	return m_param_name;
}

int32_t PageTurnStyleParam::Init()
{
	int32_t ret = WidgetStyleParam::Init();

	if (ret < 0)
		return ret;
	return 0;
}

int32_t PageTurnStyleParam::Create(int32_t mode)
{
	int32_t ret = WidgetStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	return 0;
}

int32_t PageTurnStyleParam::SetEach(Widget *widget)
{
	/* without an ID of its own, an object is not drawn */
	if (m_turn_obj.m_id.m_idhash == 0)
		m_turn_obj.m_color.set_w(0.0f);
	if (m_label_obj.m_id.m_idhash == 0)
		m_label_obj.m_color.set_w(0.0f);

	int32_t ret = WidgetStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	return 0;
}

int32_t PageTurnStyleParam::GetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;
	return 0;
}

}
}

static Widget *NewPageTurn(Widget *parent)
{
	return new PageTurn(parent, NULL);
}

static WidgetParam *NewPageTurnParam()
{
	return new PageTurnParam;
}

static VersatileParam *NewPageTurnLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewPageTurnFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewPageTurnStyleParam()
{
	return new PageTurnStyleParam;
}

static VersatileParam *NewPageTurnHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewPageTurnSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryPageTurn = {
	PageTurn::TypeName(),
	NewPageTurn,
	NewPageTurnParam,
	NewPageTurnLayoutParam,
	NewPageTurnFocusParam,
	NewPageTurnStyleParam,
	NewPageTurnHandlerParam,
	NewPageTurnSoundParam,
};
