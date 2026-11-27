/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * ListEdge widget and StyleListEdge style parameters ("list_edge",
 * "style_list_edge"), drawing upper and lower gradient-faded edge overlays
 * matched to a target background plane.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/surface/surface.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/widget.h>
#include <widget/factory.h>
#include "signin_ext_internal.h"

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::ui;

const char ListEdge::m_widget_type[] = "list_edge";
const char StyleListEdge::m_param_name[] = "style_list_edge";

ListEdge::ListEdge(Widget *parent, CreateParam *param)
	: Widget(parent, param), m_targetWidget(NULL), m_topMargin(0.0f), m_bottomMargin(0.0f)
{
	m_hit_find_mode = 0;
	m_hit_detect_mode = 2;
	m_hit_detect_b_mode = 2;

	ListEdgeObj *upper = new ListEdgeObj(PlaneObjStyle());
	upper->m_edgeSide = ListEdgeObj::EDGE_UPPER;
	DrawObjEntry upperEntry(upper, 0);
	upperEntry.m_attr.m_flag1 = false;
	m_drawobjs.push_back(upperEntry);

	ListEdgeObj *lower = new ListEdgeObj(PlaneObjStyle());
	lower->m_edgeSide = ListEdgeObj::EDGE_LOWER;
	DrawObjEntry lowerEntry(lower, 1);
	lowerEntry.m_attr.m_flag1 = false;
	m_drawobjs.push_back(lowerEntry);
}

ListEdge::~ListEdge()
{
}

bool ListEdge::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *ListEdge::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *ListEdge::TypeName()
{
	return m_widget_type;
}

void ListEdge::UpdateEdges(void)
{
	if (m_targetWidget == NULL)
		return;

	ListEdgeObj *upper = static_cast<ListEdgeObj *>(GetDrawObj(0));
	ListEdgeObj *lower = static_cast<ListEdgeObj *>(GetDrawObj(1));

	math::v4 zero(0.0f, 0.0f, 0.0f, 0.0f);
	math::v4 myPos = GetPos(0, m_parent, zero);
	math::v2 myPos2(myPos.extract_x(), myPos.extract_y());
	float myW = m_size.extract_x();
	float myH = m_size.extract_y();

	math::v4 targetPos = m_targetWidget->GetPosCenter(0, m_targetWidget->m_parent, zero);
	math::v2 targetPos2(targetPos.extract_x(), targetPos.extract_y());
	math::v4 targetSize4 = *m_targetWidget->GetSize(0);
	math::v2 targetSize2(targetSize4.extract_x(), targetSize4.extract_y());

	if (myW < 1.0f || myH < 1.0f ||
	    targetSize2.extract_x() < 1.0f || targetSize2.extract_y() < 1.0f) {
		math::v2 z(0.0f, 0.0f);
		upper->SetBasePlane(z, z);
		lower->SetBasePlane(z, z);
		return;
	}

	intrusive_ptr<Surface> surf;
	m_targetWidget->GetTexture(surf);
	if (surf.get() != NULL) {
		float ml = (float)surf->GetMarginLeft();
		float mb = (float)surf->GetMarginBottom();
		float mr = (float)surf->GetMarginRight();
		float mt = (float)surf->GetMarginTop();
		targetPos2 = math::v2(targetPos2.extract_x() + (mr - ml) * 0.5f,
				      targetPos2.extract_y() + (mt - mb) * 0.5f);
		targetSize2 = math::v2(targetSize2.extract_x() + (ml + mr),
				       targetSize2.extract_y() + (mb + mt));
	}

	if (m_topMargin > 0.0f) {
		math::v2 rel(targetPos2.extract_x() - myPos2.extract_x(),
			     targetPos2.extract_y() - myPos2.extract_y());
		upper->SetBasePlane(rel, targetSize2);
		upper->SetPos(math::v3(0.0f, (myH - m_topMargin) * 0.5f, 0.0f));
		upper->SetSize(math::v3(myW, m_topMargin, 0.0f));
	} else {
		math::v2 z(0.0f, 0.0f);
		upper->SetBasePlane(z, z);
	}

	if (m_bottomMargin > 0.0f) {
		math::v2 rel(targetPos2.extract_x() - myPos2.extract_x(),
			     targetPos2.extract_y() - myPos2.extract_y());
		lower->SetBasePlane(rel, targetSize2);
		lower->SetPos(math::v3(0.0f, (m_bottomMargin - myH) * 0.5f, 0.0f));
		lower->SetSize(math::v3(myW, m_bottomMargin, 0.0f));
	} else {
		math::v2 z(0.0f, 0.0f);
		lower->SetBasePlane(z, z);
	}
}

void ListEdge::AttachTarget(Widget *target)
{
	if (target == NULL)
		return;
	DrawObj *obj = target->GetDrawObj(0);
	if (obj == NULL || !obj->IsInherit(PlaneObj::DrawObjType()))
		return;

	m_targetWidget = target;

	intrusive_ptr<Surface> surf;
	target->GetTexture(surf);
	SetTexture(surf, 0, 0);
	SetTexture(surf, 1, 0);

	math::v4 color = target->GetColor();
	SetColor(color, NULL, ANIM_COLOR, 0, 0.0f, NULL, NULL);

	PlaneObjParam param;
	param.GetEach(obj);
	param.m_scale_mode_x = PlaneObj::SCALE_SIZE;
	param.m_scale_mode_y = PlaneObj::SCALE_SIZE;
	param.m_blend = 1;
	param.m_color = math::v4(1.0f, 1.0f, 1.0f, 1.0f);

	param.SetEach(GetDrawObj(0));
	param.SetEach(GetDrawObj(1));
	UpdateEdges();
}

int32_t ListEdge::SetTexture(const intrusive_ptr<Surface> &surf, int32_t obj, uint32_t index)
{
	DrawObj *draw = GetDrawObj(obj);
	if (draw == NULL)
		return 0x80AF4101;
	ChangeState(3, -1);
	draw->SetTexture(surf, index);
	return 0;
}

uint32_t ListEdge::CheckState()
{
	uint32_t state = m_state;
	if ((state == 3 || state == 1) && !IsPrepareObjs())
		return 0;
	return state;
}

void ListEdge::UpdateSize()
{
	UpdateEdges();
}

/* ---- StyleListEdge ---- */

StyleListEdge::StyleListEdge()
	: m_topMargin(0.0f), m_topGradient(1.0f), m_topIntercept(0.0f),
	  m_bottomMargin(0.0f), m_bottomGradient(1.0f), m_bottomIntercept(0.0f)
{
	m_upperObj.m_name = "upper_obj";
	m_upperObj.m_index = 0;
	m_drawobj_param_list.push_back(&m_upperObj);

	m_lowerObj.m_name = "lower_obj";
	m_lowerObj.m_index = 1;
	m_drawobj_param_list.push_back(&m_lowerObj);
}

StyleListEdge::~StyleListEdge()
{
}

const char *StyleListEdge::GetParamName() const
{
	return m_param_name;
}

int32_t StyleListEdge::Create(int32_t mode)
{
	int32_t ret = WidgetStyleParam::Create(mode);
	if (ret < 0)
		return ret;

	if (m_serial != NULL) {
		m_serial->AddParam("top_margin", &m_topMargin, ResourceParam::SEMANTIC_FLOAT);
		m_serial->AddParam("top_gradient", &m_topGradient, ResourceParam::SEMANTIC_FLOAT);
		m_serial->AddParam("top_intercept", &m_topIntercept, ResourceParam::SEMANTIC_FLOAT);
		m_serial->AddParam("bottom_margin", &m_bottomMargin, ResourceParam::SEMANTIC_FLOAT);
		m_serial->AddParam("bottom_gradient", &m_bottomGradient, ResourceParam::SEMANTIC_FLOAT);
		m_serial->AddParam("bottom_intercept", &m_bottomIntercept, ResourceParam::SEMANTIC_FLOAT);
	}
	return 0;
}

int32_t StyleListEdge::SetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::SetEach(widget);
	if (ret < 0)
		return ret;

	ListEdge *edge = static_cast<ListEdge *>(widget);
	edge->m_topMargin = m_topMargin;
	edge->m_bottomMargin = m_bottomMargin;
	edge->UpdateEdges();

	ListEdgeObj *upper = static_cast<ListEdgeObj *>(edge->GetDrawObj(0));
	upper->m_gradient = m_topGradient;
	upper->m_intercept = m_topIntercept;

	ListEdgeObj *lower = static_cast<ListEdgeObj *>(edge->GetDrawObj(1));
	lower->m_gradient = m_bottomGradient;
	lower->m_intercept = m_bottomIntercept;
	return 0;
}

int32_t StyleListEdge::GetEach(Widget *widget)
{
	int32_t ret = WidgetStyleParam::GetEach(widget);
	if (ret < 0)
		return ret;

	ListEdge *edge = static_cast<ListEdge *>(widget);
	m_topMargin = edge->m_topMargin;
	m_bottomMargin = edge->m_bottomMargin;

	ListEdgeObj *upper = static_cast<ListEdgeObj *>(edge->GetDrawObj(0));
	m_topGradient = upper->m_gradient;
	m_topIntercept = upper->m_intercept;

	ListEdgeObj *lower = static_cast<ListEdgeObj *>(edge->GetDrawObj(1));
	m_bottomGradient = lower->m_gradient;
	m_bottomIntercept = lower->m_intercept;
	return 0;
}

/* ---- Factory table ---- */

static Widget *NewListEdge(Widget *parent)
{
	return new ListEdge(parent, NULL);
}

static WidgetParam *NewListEdgeParam(void)
{
	return new WidgetParam();
}

static VersatileParam *NewListEdgeLayoutParam(void)
{
	return new WidgetLayoutParam();
}

static VersatileParam *NewListEdgeFocusParam(void)
{
	return new WidgetFocusParam();
}

static VersatileParam *NewListEdgeStyleParam(void)
{
	return new StyleListEdge();
}

static VersatileParam *NewListEdgeHandlerParam(void)
{
	return new WidgetHandlerParam();
}

static VersatileParam *NewListEdgeSoundParam(void)
{
	return new WidgetSoundParam();
}

FactoryFuncTable g_factoryListEdge = {
	ListEdge::TypeName(),
	NewListEdge,
	NewListEdgeParam,
	NewListEdgeLayoutParam,
	NewListEdgeFocusParam,
	NewListEdgeStyleParam,
	NewListEdgeHandlerParam,
	NewListEdgeSoundParam,
};
