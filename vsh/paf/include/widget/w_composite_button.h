/*
 * paf::ui::CompositeButton: a button made of the draw objects its style lists, each a plane or a
 * text object at the index the style gives it. Its focus switch events move the focus among the
 * parts around the focused widget. With its resource parameters (its textures and strings by index)
 * and its style. Names after ScePaf-RE and the exports (InsertDrawObj, DeleteDrawObj and
 * GetDrawObjByIndex by NID hash); the parameter classes' names are ours, after the widget's.
 */
#ifndef PAF_WIDGET_W_COMPOSITE_BUTTON_H
#define PAF_WIDGET_W_COMPOSITE_BUTTON_H

#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/draw_obj.h>
#include <widget/w_button_base.h>

namespace paf {
namespace ui {

class CompositeButton : public ButtonBase {
public:
	CompositeButton(Widget *parent, CreateParam *param);

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
	/* searched first with the search outside off, then as Widget's */
	virtual Widget *SetFocusOption(const void *param, int32_t option);
	/* its own focus switch: from the focused widget in the direction */
	virtual int32_t HandleFocusSwitchEvent(Event *e);
	/* Widget's, and whether it takes its focus switch events itself */
	virtual void EnableFocusEvent(bool on_off);
	virtual ~CompositeButton();
};

/* a CompositeButton's resource parameters: the "tex" and "str" attributes give the textures and
   strings of its draw objects by the index after the prefix */
class CompositeButtonParam : public ButtonBaseParam {
public:
	static const char m_param_name[];

	virtual const char *GetParamName() const;
	virtual int32_t Read(const cxml::Element *elem);
};

/* a CompositeButton's style: the plane and text objects its element lists, and the one its glow
   is (the "glow_obj_id") */
class CompositeButtonStyleParam : public ButtonBaseStyleParam {
public:
	static const char m_param_name[];

	CompositeButtonStyleParam() : m_create_mode(0), m_glow_obj_id(-1)
	{
	}

	virtual const char *GetParamName() const;
	virtual ~CompositeButtonStyleParam();
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t Read(const cxml::Element *elem);

	/* the mode it was created with, which the draw object parameters it reads are created
	   with */
	int32_t m_create_mode;
	int32_t m_glow_obj_id;
};

}
}

#endif
