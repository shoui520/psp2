/*
 * paf::ui::Widget, as far as it is converted: the rest of its functions are
 * still ScePafWidget's.
 */
#include <stddef.h>
#include <graphics/drawobj/draw_obj.h>
#include <widget/core/widget.h>
#include <widget/w_scene.h>

namespace paf {
namespace ui {

/* the nearest widget up the tree, this one included, that holds the focus of its children,
   short of the scene (name ours) */
static inline Widget *FindFocusRoot(Widget *widget)
{
	for (Widget *root = widget; root != NULL; root = root->m_parent) {
		if (root->m_focus_root != NULL)
			return root;
		if (static_cast<Widget *>(widget->m_scene) == root)
			return NULL;
	}
	return NULL;
}

/* whether it is the widget its focus root focuses first */
bool Widget::IsInitFocus()
{
	Widget *root = FindFocusRoot(this);
	bool init = false;

	if (root != NULL && root->m_focus_root != NULL && root->m_focus_root->m_init_focus == this)
		init = true;
	return init;
}


/* the draw object drawn to a surface as the widget is, with its scene's font flags (name
   ours) */
static inline void SetDrawObjSurfaceDraw(Widget *widget, graph::DrawObj *obj)
{
	graph::SurfaceDrawParam param;

	param.m_font_flags = widget->m_scene != NULL ? widget->m_scene->m_font_flags : 128;
	param.m_surface_draw = widget->m_surface_draw;
	obj->SetSurfaceDraw(param);
}

void Widget::AddDrawObj(const DrawObjEntry &entry, bool update)
{
	if (m_drawobjs.capacity() <= m_drawobjs.size())
		ReserveDrawObj(m_drawobjs.capacity() + 2);
	m_drawobjs.push_back(entry);
	if (entry.m_obj.get() != NULL)
		SetDrawObjSurfaceDraw(this, entry.m_obj.get());
	if (update)
		UpdateSize();
}

void Widget::ReplaceDrawObj(const DrawObjEntry &entry, bool update)
{
	vector<DrawObjEntry>::iterator it;

	for (it = m_drawobjs.begin(); it != m_drawobjs.end(); ++it) {
		if (it->m_attr.m_id == entry.m_attr.m_id)
			break;
	}
	if (it == m_drawobjs.end())
		return;
	*it = entry;
	if (entry.m_obj.get() != NULL)
		SetDrawObjSurfaceDraw(this, entry.m_obj.get());
	if (update)
		UpdateSize();
}

}
}
