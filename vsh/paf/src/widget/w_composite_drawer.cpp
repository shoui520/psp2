/*
 * paf::ui::CompositeDrawer (DEBUGGER 0x810e76b8..0x810e8288): a widget whose draw objects its
 * style lists, as CompositeButton's are. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <cxml/cxml.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <std/string.h>
#include <widget/factory.h>
#include <widget/w_composite_drawer.h>

namespace paf {
namespace ui {

CompositeDrawer::CompositeDrawer(Widget *parent, CreateParam *param) : Widget(parent, param)
{
}

const char CompositeDrawer::m_widget_type[] = "composite_drawer";

bool CompositeDrawer::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Widget::IsInherit(type);
}

char *CompositeDrawer::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *CompositeDrawer::TypeName()
{
	return m_widget_type;
}

int32_t CompositeDrawer::InsertDrawObj(common::SharedPtr<graph::DrawObj> &obj, int32_t id,
				       bool update)
{
	if (FindDrawObj(id) != m_drawobjs.end()) {
		vector<DrawObjEntry>::iterator it = FindDrawObj(id);

		if (it != m_drawobjs.end())
			it->m_attr.m_flag1 = update;
		SwapDrawObj(obj, id);
	} else {
		if (m_drawobjs.size() >= 0x400)
			return -1;

		common::SharedPtr<graph::DrawObj> ptr(obj);
		DrawObjAttr attr(id);

		attr.m_flag1 = update;
		Widget::InsertDrawObj(LowerBoundDrawObj(id), DrawObjEntry(ptr, attr), update);
	}
	return 0;
}

void CompositeDrawer::DeleteDrawObj(int32_t id)
{
	SwapDrawObj(common::SharedPtr<graph::DrawObj>(), id);

	vector<DrawObjEntry>::iterator it = FindDrawObj(id);

	if (it != m_drawobjs.end())
		m_drawobjs.erase(it);
}

graph::DrawObj *CompositeDrawer::GetDrawObjByIndex(int32_t index)
{
	if (index >= 0 && (uint32_t)index < m_drawobjs.size())
		return m_drawobjs[index].m_obj.get();
	return NULL;
}

int32_t CompositeDrawer::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				    uint32_t index)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return 0x80AF4101;
	draw->SetTexture(surf, index);
	return 0;
}

int32_t CompositeDrawer::GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj,
				    uint32_t index)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw == NULL)
		return 0x80AF4101;
	draw->GetTexture(surf, index);
	return 0;
}

uint32_t CompositeDrawer::CheckState()
{
	uint32_t state = m_state;

	if (state < 2 || state == 3) {
		state = 0;
		if (IsPrepareObjs())
			return 3;
	}
	return state;
}

CompositeDrawer::~CompositeDrawer()
{
}

const char CompositeDrawerStyleParam::m_param_name[] = "style_composite_drawer";

const char *CompositeDrawerStyleParam::GetParamName() const
{
	return m_param_name;
}

CompositeDrawerStyleParam::~CompositeDrawerStyleParam()
{
	for (vector<graph::DrawObjParam *>::iterator it = m_drawobj_param_list.begin();
	     it != m_drawobj_param_list.end(); ++it)
		delete *it;
}

int32_t CompositeDrawerStyleParam::Create(int32_t mode)
{
	m_create_mode = mode;

	int32_t ret = WidgetStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	return 0;
}

int32_t CompositeDrawerStyleParam::SetEach(Widget *widget)
{
	CompositeDrawer *drawer = (CompositeDrawer *)widget;

	for (vector<graph::DrawObjParam *>::iterator it = m_drawobj_param_list.begin();
	     it != m_drawobj_param_list.end(); ++it) {
		graph::DrawObjParam *param = *it;
		graph::DrawObj *obj = NULL;

		if (sce_paf_strcmp(param->GetParamName(), graph::PlaneObjParam::ParamName()) == 0)
			obj = new graph::PlaneObj(graph::PlaneObjStyle());
		else if (sce_paf_strcmp(param->GetParamName(), graph::TextObjParam::ParamName()) == 0)
			obj = new graph::TextObj(graph::TextObjStyle());

		common::SharedPtr<graph::DrawObj> ptr(obj);

		if (ptr.get() != NULL)
			drawer->InsertDrawObj(ptr, (*it)->m_index, false);
	}

	int32_t ret = WidgetStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	return 0;
}

int32_t CompositeDrawerStyleParam::Read(const cxml::Element *elem)
{
	int32_t ret = WidgetStyleParam::Read(elem);

	if (ret < 0)
		return ret;
	if (elem != NULL) {
		cxml::Element child = elem->GetFirstChild();

		while (child.doc != NULL && child.offset >= 0) {
			const char *name = child.GetName();
			graph::DrawObjParam *param = NULL;

			if (sce_paf_strcmp(name, "planeobj") == 0)
				param = new graph::PlaneObjParam;
			else if (sce_paf_strcmp(name, "textobj") == 0)
				param = new graph::TextObjParam;
			if (param != NULL) {
				param->m_index = m_drawobj_param_list.size();
				param->m_name = "";
				m_drawobj_param_list.push_back(param);
				param->AttachPlugin(m_plugin);
				param->Create(m_create_mode);
				param->Read(&child);
			}

			cxml::Element next = child.GetNextSibling();

			child = next;
		}
	}
	return 0;
}

}
}

using namespace paf;
using namespace paf::ui;

static Widget *NewCompositeDrawer(Widget *parent)
{
	return new CompositeDrawer(parent, NULL);
}

static WidgetParam *NewCompositeDrawerParam()
{
	return new WidgetParam;
}

static VersatileParam *NewCompositeDrawerLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewCompositeDrawerFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewCompositeDrawerStyleParam()
{
	return new CompositeDrawerStyleParam;
}

static VersatileParam *NewCompositeDrawerHandlerParam()
{
	return new WidgetHandlerParam;
}

static VersatileParam *NewCompositeDrawerSoundParam()
{
	return new WidgetSoundParam;
}

FactoryFuncTable g_factoryCompositeDrawer = {
	CompositeDrawer::TypeName(),
	NewCompositeDrawer,
	NewCompositeDrawerParam,
	NewCompositeDrawerLayoutParam,
	NewCompositeDrawerFocusParam,
	NewCompositeDrawerStyleParam,
	NewCompositeDrawerHandlerParam,
	NewCompositeDrawerSoundParam,
};
