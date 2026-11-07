/*
 * paf::ui::CompositeDrawer: a widget made of the draw objects its style lists, each a plane or a
 * text object at the index the style gives it, as CompositeButton's are. With its style. Names
 * after the exports (InsertDrawObj, DeleteDrawObj, SetTexture, GetTexture and CheckState by NID
 * hash); GetDrawObjByIndex after CompositeButton's, the style class's name is ours, after the
 * widget's.
 */
#ifndef PAF_WIDGET_W_COMPOSITE_DRAWER_H
#define PAF_WIDGET_W_COMPOSITE_DRAWER_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/draw_obj.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class CompositeDrawer : public Widget {
public:
	CompositeDrawer(Widget *parent, CreateParam *param);

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;

	static const char m_widget_type[];
	static const char *TypeName();

	/* the draw object at the ID, added or put in place of the one there; -1 when it has 1024 */
	virtual int32_t InsertDrawObj(common::SharedPtr<graph::DrawObj> &obj, int32_t id,
				      bool update);
	virtual void DeleteDrawObj(int32_t id);
	/* its index-th draw object, NULL past the end */
	virtual graph::DrawObj *GetDrawObjByIndex(int32_t index);
	/* the texture of the draw object with the ID; 0x80AF4101 without one */
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	virtual int32_t GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj, uint32_t index);
	/* 3 (ready) for states 0, 1 and 3 while its draw objects still prepare, else 0 */
	virtual uint32_t CheckState();
	virtual ~CompositeDrawer();
};

/* a CompositeDrawer's style: the plane and text objects its element lists */
class CompositeDrawerStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];

	CompositeDrawerStyleParam() : m_create_mode(0)
	{
	}

	virtual const char *GetParamName() const;
	virtual ~CompositeDrawerStyleParam();
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t Read(const cxml::Element *elem);

	/* the mode it was created with, which the draw object parameters it reads are created
	   with */
	int32_t m_create_mode;
};

}
}

#endif
