/*
 * paf::ui::CornerButton: a button in a corner of the screen, with a plane (the corner object) whose
 * texture sizes it, and the key focus frame of the corner. Its layout type says which corner it
 * sits in and turns its planes to face it. With its resource, layout, style and handler parameters.
 * Names after the exports (InitParam, UpdateObjsSize, CreateDrawObj, SetTexture and the parameter
 * names by NID hash); the layout type's setter keeps its link name, the layout parameter class's
 * name and the members' are ours, after the style's "layout_type".
 */
#ifndef PAF_WIDGET_W_CORNER_BUTTON_H
#define PAF_WIDGET_W_CORNER_BUTTON_H

#include <stdint.h>
#include <common/transition.h>
#include <graphics/drawobj/plane_obj.h>
#include <widget/core/handler.h>
#include <widget/w_button.h>

namespace paf {
namespace ui {

class CornerButton : public Button {
public:
	enum {
		/* the corner object (name ours) */
		OBJ_CORNER = 69
	};

	CornerButton(Widget *parent, CreateParam *param);

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;

	static const char m_widget_type[];
	static const char *TypeName();

	/* shown with the fifth popup transition; its button end event when it is shown */
	virtual float DoTransition_core(float parameter, bool set)
	{
		float time = ScePafCommon_6CF3A3C8(parameter, this, common::transition::Type_Popup5,
						   false, set);

		SetTimeoutEvent(0xFEFF, time, OnTransitionEnd, (void *)0);
		return time;
	}

	/* hidden the same way */
	virtual float DoTransitionReverse_core(float parameter, bool set)
	{
		float time = ScePafCommon_B48C9BDC(parameter, this, common::transition::Type_Popup5,
						   false, set);

		SetTimeoutEvent(0xFEFF, time, OnTransitionEnd, (void *)1);
		return time;
	}

	virtual void UpdatePost();
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);

	/* the corner object's */
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf)
	{
		return SetTexture(surf, OBJ_CORNER, 0);
	}

	/* first in the focus search up and down */
	virtual int32_t GetFocusPriority(int32_t dir)
	{
		if (m_focus_priority && (dir == 1 || dir == 2))
			return 1;
		return 0;
	}

	virtual graph::DrawObj *CreateDrawObj(int32_t obj);
	virtual bool UpdateObjsSize();
	virtual ~CornerButton();

	/* the timeout after a transition: the button end event, with 0 after it is shown and 1
	   after it is hidden (name ours) */
	static void OnTransitionEnd(int32_t type, Handler *self, Event *e, void *data);

	/* the corner (the low three bits) and whether it is moved in by 32 (8) */
	int32_t m_layout_type;
	/* released by the destructor; nothing in its file sets it */
	intrusive_ptr<graph::Surface> m_corner_texture;
	/* cleared by the constructor; nothing in its file reads it */
	int32_t reserved;
	/* first in the focus search up and down */
	bool m_focus_priority;
};

/* a CornerButton's resource parameters: the corner object's texture ("texture_corner") */
class CornerButtonParam : public ButtonParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	CornerButtonParam();
	virtual const char *GetParamName() const;
};

/* a CornerButton's layout: 90 by 90, in the top left corner of the screen */
class CornerButtonLayoutParam : public WidgetLayoutParam {
public:
	static const char m_param_name[];

	CornerButtonLayoutParam();
	virtual const char *GetParamName() const;
};

/* a CornerButton's style: its corner object and its layout type ("layout_type") */
class CornerButtonStyleParam : public ButtonStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	CornerButtonStyleParam()
	{
		InitParam();
	}

	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	int32_t Init();
	void InitParam();

	graph::PlaneObjParam m_corner_obj;
	int32_t m_layout_type;
};

class CornerButtonHandlerParam : public ButtonBaseHandlerParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	CornerButtonHandlerParam()
	{
	}

	virtual const char *GetParamName() const;
};

}
}

extern "C" {
/* the layout type: the button moved to its corner and its planes and focus frame turned to face
   it (no name found) */
void ScePafWidget_DFEF98E2(paf::ui::CornerButton *button, int32_t type);
}

#endif
