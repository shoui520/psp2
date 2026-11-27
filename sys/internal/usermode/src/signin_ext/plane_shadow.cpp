/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * PlaneShadow widget and StylePlaneShadow style parameters ("plane_shadow",
 * "style_plane_shadow"), extending Plane with optional shadow (obj 1) and
 * highlight (obj 2) plane draw objects.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v3.h>
#include <widget/core/widget.h>
#include <widget/factory.h>
#include <widget/w_plane.h>
#include "signin_ext_internal.h"

using namespace paf;
using namespace paf::graph;
using namespace paf::ui;

const char PlaneShadow::m_widget_type[] = "plane_shadow";
const char StylePlaneShadow::m_param_name[] = "style_plane_shadow";

PlaneShadow::PlaneShadow(Widget *parent, CreateParam *param)
	: Plane(parent, param)
{
}

PlaneShadow::~PlaneShadow()
{
}

bool PlaneShadow::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Plane::IsInherit(type);
}

char *PlaneShadow::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *PlaneShadow::TypeName()
{
	return m_widget_type;
}

void PlaneShadow::EnsureShadowObj(void)
{
	if (GetDrawObj(1) != NULL)
		return;

	PlaneObj *shadow = new PlaneObj(PlaneObjStyle());
	DrawObjEntry entry(shadow, 1);
	m_drawobjs.insert(m_drawobjs.begin(), entry);
}

void PlaneShadow::EnsureHighlightObj(void)
{
	if (GetDrawObj(2) != NULL)
		return;

	vector<DrawObjEntry>::iterator it = FindDrawObj(0);
	if (it != m_drawobjs.end())
		++it;

	PlaneObj *highlight = new PlaneObj(PlaneObjStyle());
	DrawObjEntry entry(highlight, 2);
	m_drawobjs.insert(it, entry);
}

void PlaneShadow::UpdateSize()
{
	Plane::UpdateSize();

	math::v3 size = m_plane->m_size;
	for (vector<DrawObjEntry>::iterator it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id != 0 && it->m_attr.m_flag1)
			it->m_obj->SetSize(size);
	}
}

/* ---- StylePlaneShadow ---- */

StylePlaneShadow::StylePlaneShadow()
{
	m_shadowObj.m_name = "shadow_obj";
	m_shadowObj.m_index = 1;
	m_drawobj_param_list.push_back(&m_shadowObj);

	m_highlightObj.m_name = "highlight_obj";
	m_highlightObj.m_index = 2;
	m_drawobj_param_list.push_back(&m_highlightObj);
}

StylePlaneShadow::~StylePlaneShadow()
{
}

const char *StylePlaneShadow::GetParamName() const
{
	return m_param_name;
}

int32_t StylePlaneShadow::SetEach(Widget *widget)
{
	PlaneShadow *ps = static_cast<PlaneShadow *>(widget);
	DrawObjParam *shadowParam = GetDrawObjParam(1);
	if (shadowParam != NULL && shadowParam->m_serial != NULL)
		ps->EnsureShadowObj();

	DrawObjParam *hlParam = GetDrawObjParam(2);
	if (hlParam != NULL && hlParam->m_serial != NULL)
		ps->EnsureHighlightObj();

	return WidgetStyleParam::SetEach(widget);
}

/* ---- Factory table ---- */

static Widget *NewPlaneShadow(Widget *parent)
{
	return new PlaneShadow(parent, NULL);
}

static WidgetParam *NewPlaneShadowParam(void)
{
	return new PlaneParam();
}

static VersatileParam *NewPlaneShadowLayoutParam(void)
{
	return new WidgetLayoutParam();
}

static VersatileParam *NewPlaneShadowFocusParam(void)
{
	return new WidgetFocusParam();
}

static VersatileParam *NewPlaneShadowStyleParam(void)
{
	return new StylePlaneShadow();
}

static VersatileParam *NewPlaneShadowHandlerParam(void)
{
	return new WidgetHandlerParam();
}

static VersatileParam *NewPlaneShadowSoundParam(void)
{
	return new WidgetSoundParam();
}

FactoryFuncTable g_factoryPlaneShadow = {
	PlaneShadow::TypeName(),
	NewPlaneShadow,
	NewPlaneShadowParam,
	NewPlaneShadowLayoutParam,
	NewPlaneShadowFocusParam,
	NewPlaneShadowStyleParam,
	NewPlaneShadowHandlerParam,
	NewPlaneShadowSoundParam,
};
