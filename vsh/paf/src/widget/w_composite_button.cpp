/*
 * paf::ui::CompositeButton (DEBUGGER 0x810e67fc..0x810e76b8): a ButtonBase whose draw objects
 * its style lists. Made without a CreateParam it makes none of its own; its focus switch events
 * search from the focused widget. No record or string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <cxml/cxml.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <math/ngp/v2.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <versatileparam/resourceparam.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/focus.h>
#include <widget/factory.h>
#include <widget/w_composite_button.h>

namespace paf {
namespace ui {

/* what it is made with without a CreateParam: none of its own draw objects (name ours) */
static Widget::CreateParam *s_default_param = NULL;

static inline Widget::CreateParam *DefaultCreateParam()
{
	static Widget::CreateParam s_param;

	if (s_default_param == NULL) {
		s_default_param = &s_param;
		s_default_param->m_create_drawobj = false;
	}
	return s_default_param;
}

CompositeButton::CompositeButton(Widget *parent, CreateParam *param)
	: ButtonBase(parent, param != NULL ? param : DefaultCreateParam())
{
	m_composite_focus = true;
}

const char CompositeButton::m_widget_type[] = "composite_button";

bool CompositeButton::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return ButtonBase::IsInherit(type);
}

char *CompositeButton::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *CompositeButton::TypeName()
{
	return m_widget_type;
}

int32_t CompositeButton::InsertDrawObj(common::SharedPtr<graph::DrawObj> &obj, int32_t id,
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

void CompositeButton::DeleteDrawObj(int32_t id)
{
	SwapDrawObj(common::SharedPtr<graph::DrawObj>(), id);

	vector<DrawObjEntry>::iterator it = FindDrawObj(id);

	if (it != m_drawobjs.end())
		m_drawobjs.erase(it);
}

graph::DrawObj *CompositeButton::GetDrawObjByIndex(int32_t index)
{
	if (index >= 0 && (uint32_t)index < m_drawobjs.size())
		return m_drawobjs[index].m_obj.get();
	return NULL;
}

Widget *CompositeButton::SetFocusOption(const void *param, int32_t option)
{
	if (m_composite_focus && param != NULL) {
		FocusSearchInfo info(*(const FocusSearchInfo *)param);

		info.m_search_outside = false;
		m_composite_focus = false;
		Widget *ret = Widget::SetFocusOption(&info, 1);
		m_composite_focus = true;
		if (ret != NULL)
			return ret;
	}
	return Widget::SetFocusOption(param, option);
}

int32_t CompositeButton::HandleFocusSwitchEvent(Event *e)
{
	if (m_composite_focus) {
		Environment *env = m_env;

		if (env == NULL)
			return -1;

		uint32_t type = e->GetType();

		if (type != 17 && type != 19)
			return -1;

		uint32_t dir = (uint8_t)e->m_detail;

		if (dir < 1 || dir > 4)
			return -1;

		/* from the widget the focus is on */
		Widget *focus = env->m_focus_cursor.m_target;

		if (focus != NULL) {
			FocusSearchInfo info(dir);
			Widget *w;

			ScePafWidget_EA117CF8(&info, focus);
			if (m_focus_target && (w = SetFocusOption(&info, 1)) != NULL) {
				math::v2 direction(0.0f, 0.0f);
				SetFocusInfo set;

				switch (dir) {
				case 1:
					direction = math::v2(0.0f, 1.0f);
					break;
				case 2:
					direction = math::v2(0.0f, -1.0f);
					break;
				case 3:
					direction = math::v2(-1.0f, 0.0f);
					break;
				case 4:
					direction = math::v2(1.0f, 0.0f);
					break;
				}
				set.m_mode = 0;
				set.m_notify = true;
				set.m_direction = direction;
				set.m_keep = false;
				w->SetFocus(&set);
				e->Consume();
				return 0;
			}
		}
	}
	return Widget::HandleFocusSwitchEvent(e);
}

void CompositeButton::EnableFocusEvent(bool on_off)
{
	m_composite_focus = on_off;
	Widget::EnableFocusEvent(on_off);
}

CompositeButton::~CompositeButton()
{
}

const char CompositeButtonParam::m_param_name[] = "composite_button_base";

const char *CompositeButtonParam::GetParamName() const
{
	return m_param_name;
}

int32_t CompositeButtonParam::Read(const cxml::Element *elem)
{
	int32_t ret = VersatileParam::Read(elem);

	if (ret < 0)
		return ret;
	if (elem == NULL)
		return 0;

	IndexedRef ref("", 0);
	cxml::Element entity;
	cxml::Attribute attr;

	for (int32_t i = 0; elem->GetAttribute(i, &attr) == 0; i++) {
		const char *name = attr.GetName();
		uint32_t value;

		if (sce_paf_strncmp(name, "str", 3) == 0) {
			if (scePafResourceGetIdInt(&attr, &value, &entity) == 0) {
				ref.id.m_idhash = value;
				ref.index = sce_paf_atoi(name + 3);
				m_string_list.push_back(ref);
			}
		} else if (sce_paf_strncmp(name, "tex", 3) == 0) {
			if (scePafResourceGetIdInt(&attr, &value, &entity) == 0) {
				ref.id.m_idhash = value;
				ref.index = sce_paf_atoi(name + 3);
				m_texture_list.push_back(ref);
			}
		}
	}
	return 0;
}

const char CompositeButtonStyleParam::m_param_name[] = "style_composite_button";

const char *CompositeButtonStyleParam::GetParamName() const
{
	return m_param_name;
}

CompositeButtonStyleParam::~CompositeButtonStyleParam()
{
	for (vector<graph::DrawObjParam *>::iterator it = m_drawobj_param_list.begin();
	     it != m_drawobj_param_list.end(); ++it)
		delete *it;
}

int32_t CompositeButtonStyleParam::Create(int32_t mode)
{
	m_create_mode = mode;

	int32_t ret = ButtonBaseStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("glow_obj_id", &m_glow_obj_id, ResourceParam::SEMANTIC_INT);
	return 0;
}

int32_t CompositeButtonStyleParam::SetEach(Widget *widget)
{
	CompositeButton *button = (CompositeButton *)widget;

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
			button->InsertDrawObj(ptr, (*it)->m_index, false);
	}

	int32_t ret = ButtonBaseStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	if (button->GetDrawObj(m_glow_obj_id) != NULL &&
	    button->GetDrawObj(m_glow_obj_id)->IsInherit(graph::PlaneObj::DrawObjType()) &&
	    button->GetDrawObj(m_glow_obj_id) != NULL)
		button->SetGlowObj(m_glow_obj_id);
	return 0;
}

int32_t CompositeButtonStyleParam::Read(const cxml::Element *elem)
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

static Widget *NewCompositeButton(Widget *parent)
{
	return new CompositeButton(parent, NULL);
}

static WidgetParam *NewCompositeButtonParam()
{
	return new CompositeButtonParam;
}

static VersatileParam *NewCompositeButtonLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewCompositeButtonFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewCompositeButtonStyleParam()
{
	return new CompositeButtonStyleParam;
}

static VersatileParam *NewCompositeButtonHandlerParam()
{
	return new ButtonBaseHandlerParam;
}

static VersatileParam *NewCompositeButtonSoundParam()
{
	return new ButtonBaseSoundParam;
}

FactoryFuncTable g_factoryCompositeButton = {
	CompositeButton::TypeName(),
	NewCompositeButton,
	NewCompositeButtonParam,
	NewCompositeButtonLayoutParam,
	NewCompositeButtonFocusParam,
	NewCompositeButtonStyleParam,
	NewCompositeButtonHandlerParam,
	NewCompositeButtonSoundParam,
};
