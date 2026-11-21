/*
 * The finger pointer: the plane that points where a rear touch is, with its draw object
 * (FingerPointerObj, Sony's class name). Sony's file name is not known (the module has no debug
 * records); this one and the names in it other than FingerPointerObj's are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/math.h>
#include <std/stdlib.h>
#include <timer/timer.h>
#include <widget/w_camera.h>
#include <graphics/drawobj/draw_env.h>
#include <graphics/drawobj/draw_info.h>
#include <graphics/drawobj/plane_obj.h>
#include "web_view.h"

namespace paf {
namespace ui {

/*
 * The finger pointer's draw object (Sony's class name, its draw object type): a plane object
 * that draws nothing itself, and has its pointer drawn in its place.
 */
class FingerPointerObj : public graph::PlaneObj {
public:
	static const char m_drawobj_type[];

	/* what a queued draw calls: the object's Draw (Sony's name, as PlaneObj's) */
	static void DrawCallback(const DrawCallbackArg &arg)
	{
		arg.self->Draw(*arg.world_mat, arg.color, arg.alpha);
	}

	FingerPointerObj(FingerPointer *owner) : graph::PlaneObj(graph::PlaneObjStyle()), m_owner(owner)
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual const char *GetDrawObjType() const;

	virtual ~FingerPointerObj()
	{
	}

	/* queued for the frame under the draw info's key, while it has its pointer */
	virtual void OnDraw(const math::matrix &parent, uint32_t color, float alpha,
			    graph::DrawInfo &info)
	{
		if (m_owner == NULL)
			return;

		uint32_t key = (info.m_layer << 12) + (info.m_order << 8) + info.m_sub_layer;
		DrawCallbackArg arg;

		arg.self = this;
		arg.world_mat = &parent;
		arg.color = color;
		arg.alpha = alpha;
		graph::g_scene_draw_layer.m_ptr->Push(key & 0x7fffffff, DrawCallback, arg);
		info.m_order++;
	}

	/* its pointer drawn: its owner's DrawPointer */
	virtual void Draw(const math::matrix &world, uint32_t color, float alpha);

	FingerPointer *m_owner;
};

inline void FingerPointerObj::Draw(const math::matrix &world, uint32_t color, float alpha)
{
	m_owner->DrawPointer();
}

FingerPointer::FingerPointer(Widget *parent, CreateParam *param)
	: Plane(parent, param), m_size(160), m_defaultSize(84), m_state(STATE_HIDDEN),
	  m_stateTime(get_time_internal()), m_x(0.0f), m_y(0.0f), m_angle(0.0f), m_scale(1.0f),
	  m_enabled(true)
{
	Camera *camera = GetCamera();

	ScePafWidget_72F479CA(camera);
	SetSize(math::v4(camera->m_screen_width, camera->m_screen_height, 0.0f, 0.0f));
	SetColor(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	m_hit_detect_b_mode = 2;
	m_hit_detect_mode = 2;

	FingerPointerObj *obj = new FingerPointerObj(this);
	common::SharedPtr<graph::DrawObj> ptr(obj);

	Plane *plane;

	Plane::SwapDrawObj(ptr, 0);
	plane = new Plane(this, NULL);
	m_planeA = plane;
	plane->m_hit_detect_mode = 2;
	plane->m_hit_detect_b_mode = 2;
	plane = new Plane(this, NULL);
	m_planeC = plane;
	plane->m_hit_detect_mode = 2;
	plane->m_hit_detect_b_mode = 2;
	plane = new Plane(this, NULL);
	m_planeB = plane;
	plane->m_hit_detect_mode = 2;
	plane->m_hit_detect_b_mode = 2;
}

FingerPointer::~FingerPointer()
{
}

void FingerPointer::Begin(float x, float y, float angle)
{
	m_x = x;
	m_y = y;
	m_angle = angle;
	m_state = STATE_POINT_A;
	ShowNow(this);
}

void FingerPointer::Move(float x, float y, float angle)
{
	m_x = x;
	m_y = y;
	m_angle = angle;
}

void FingerPointer::End(float x, float y)
{
	m_x = x;
	m_y = y;
	m_state = STATE_HIDDEN;
	if (m_planeA != NULL)
		HideNow(m_planeA);
	if (m_planeC != NULL)
		HideNow(m_planeC);
	if (m_planeB != NULL)
		HideNow(m_planeB);
	HideNow(this);
}

void FingerPointer::SetSurfaceA(intrusive_ptr<graph::Surface> surface, SceFColor region)
{
	if (surface.get() == NULL || m_planeA == NULL)
		return;

	m_planeA->SetTexture(surface, 0, 0);

	float width = (float)surface->GetWidth();
	float height = (float)surface->GetHeight();

	m_size = (int32_t)(height * (region.g - region.a));
	math::v4 size(0.0f, 0.0f, 0.0f, 0.0f);

	size.set_x(width);
	size.set_y(height);
	m_sizeA = size;
	m_planeA->SetSize(m_sizeA);
	m_planeA->SetAlignOffset(math::v4(m_planeA->GetSize(0)->extract_x() * region.r -
					  m_planeA->GetSize(0)->extract_x() * 0.5f,
					  m_planeA->GetSize(0)->extract_y() * region.g -
					  m_planeA->GetSize(0)->extract_y() * 0.5f, 0.0f, 0.0f),
				 NULL, ANIM_ALIGN, 0, 0.0f, NULL, NULL);
	m_planeA->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	HideNow(m_planeA);
}

void FingerPointer::SetSurfaceC(intrusive_ptr<graph::Surface> surface, SceFColor region)
{
	if (surface.get() == NULL || m_planeC == NULL)
		return;

	m_planeC->SetTexture(surface, 0, 0);

	float width = (float)surface->GetWidth();
	float height = (float)surface->GetHeight();
	/* drawn so that its tip is as long as surface a's */
	float scale = (float)m_size / (height * (region.g - region.a));

	math::v4 size(0.0f, 0.0f, 0.0f, 0.0f);

	size.set_x(width);
	size.set_y(height);
	m_sizeC = size * scale;
	m_planeC->SetSize(m_sizeC);
	m_planeC->SetAlignOffset(math::v4(m_planeC->GetSize(0)->extract_x() * region.r -
					  m_planeC->GetSize(0)->extract_x() * 0.5f,
					  m_planeC->GetSize(0)->extract_y() * region.g -
					  m_planeC->GetSize(0)->extract_y() * 0.5f, 0.0f, 0.0f),
				 NULL, ANIM_ALIGN, 0, 0.0f, NULL, NULL);
	m_planeC->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	HideNow(m_planeC);
}

void FingerPointer::SetSurfaceB(intrusive_ptr<graph::Surface> surface, SceFVector2 anchor)
{
	if (surface.get() == NULL || m_planeB == NULL)
		return;

	m_planeB->SetTexture(surface, 0, 0);
	math::v4 size(0.0f, 0.0f, 0.0f, 0.0f);

	size.set_x((float)surface->GetWidth());
	size.set_y((float)surface->GetHeight());
	m_sizeB = size;
	m_planeB->SetSize(m_sizeB);
	m_planeB->SetAlignOffset(math::v4(m_planeB->GetSize(0)->extract_x() * (anchor.x - 0.5f),
					  m_planeB->GetSize(0)->extract_y() * (anchor.y - 0.5f),
					  0.0f, 0.0f),
				 NULL, ANIM_ALIGN, 0, 0.0f, NULL, NULL);
	m_planeB->SetColor(math::v4(1.0f, 1.0f, 1.0f, 1.0f));
	HideNow(m_planeB);
}

void FingerPointer::SetEnabled(bool enabled)
{
	m_enabled = enabled;
}

math::v2 FingerPointer::GetTipOffset() const
{
	float angle = m_angle * -3.14f / 180.0f;

	return math::v2(sce_paf_sinf(angle) * (float)m_size, sce_paf_cosf(angle) * (float)m_size);
}

int32_t FingerPointer::OnDraw(graph::DrawInfo &info)
{
	return Widget::OnDraw(info);
}

/* plane b at the place, in its size by the scale and the color */
static inline void ShowPress(FingerPointer *pointer, const math::v4 &pos, const math::v4 &color)
{
	pointer->m_planeB->SetSize(pointer->m_sizeB * pointer->m_scale);
	pointer->m_planeB->SetColor(color);
	pointer->m_planeB->SetPos(pos);
	ShowNow(pointer->m_planeB);
}

/* plane a or c (the member) at the place, turned to the angle */
#define SHOW_POINT(plane, pos)							\
	do {								\
		(plane)->SetRot(math::v4(0.0f, 0.0f, m_angle / 180.0f * 3.1415927f, 0.0f), 0,	\
				NULL, ANIM_ROT, 0, 0.0f, NULL, NULL);		\
		(plane)->SetPos(pos);					\
		ShowNow(plane);						\
	} while (0)

void FingerPointer::DrawPointer()
{
	switch (m_state) {
	case STATE_HIDDEN:
		if (m_planeA != NULL)
			HideNow(m_planeA);
		if (m_planeC != NULL)
			HideNow(m_planeC);
		if (m_planeB != NULL)
			HideNow(m_planeB);
		break;
	case STATE_PRESS:
		if (m_planeA != NULL)
			HideNow(m_planeA);
		if (m_planeC != NULL)
			HideNow(m_planeC);
		if (m_enabled && m_planeB != NULL)
			ShowPress(this, math::v4(m_x, m_y, 0.0f, 0.0f),
				  math::v4(1.0f, 1.0f, 1.0f, 0.4f));
		break;
	case STATE_POINT_A: {
		math::v4 pos(m_x, m_y, 0.0f, 0.0f);

		if (m_planeC != NULL)
			HideNow(m_planeC);
		if (m_planeA != NULL)
			SHOW_POINT(m_planeA, pos);
		if (m_planeB != NULL) {
			if (m_enabled)
				ShowPress(this, pos, math::v4(1.0f, 1.0f, 1.0f, 1.0f));
			else
				HideNow(m_planeB);
		}
		break;
	}
	case STATE_POINT_C: {
		math::v4 pos(m_x, m_y, 0.0f, 0.0f);

		if (m_planeA != NULL)
			HideNow(m_planeA);
		if (m_planeC != NULL)
			SHOW_POINT(m_planeC, pos);
		if (m_planeB != NULL) {
			if (m_enabled)
				ShowPress(this, pos, math::v4(1.0f, 1.0f, 1.0f, 1.0f));
			else
				HideNow(m_planeB);
		}
		break;
	}
	/* a fifth state, drawn as nothing (Sony's switch has its case) */
	case STATE_POINT_C + 1:
		break;
	}
}

#undef SHOW_POINT

const char FingerPointerObj::m_drawobj_type[] = "FingerPointerObj";

bool FingerPointerObj::IsInherit(const char *type) const
{
	if (type == m_drawobj_type)
		return true;
	return PlaneObj::IsInherit(type);
}

const char *FingerPointerObj::GetDrawObjType() const
{
	return m_drawobj_type;
}

} /* namespace ui */
} /* namespace paf */
