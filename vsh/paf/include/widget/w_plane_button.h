/*
 * paf::ui::PlaneButton: a ButtonBase drawn as an image plane (0) with a glow (1); where it
 * adjusts it is sized to its planes and the margins of its adjust parameter, its style animator
 * laying them out, and it plays the image's texture animation as it is pressed and released.
 * With its resource, style and handler parameters. The classes, TypeName, the parameter names
 * and the functions the mangled exports name are Sony's ("plane_button", "style_plane_button",
 * "handler_plane_button"); the members, the style animators and the function the exports name
 * only by NID are ours.
 */
#ifndef PAF_WIDGET_W_PLANE_BUTTON_H
#define PAF_WIDGET_W_PLANE_BUTTON_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v4.h>
#include <widget/core/layouter.h>
#include <widget/w_button_base.h>

namespace paf {
namespace ui {

class PlaneButton : public ButtonBase {
public:
	enum {
		/* the image and the glow (names ours, after the style's "image_obj" and
		   "glow_obj") */
		OBJ_IMAGE = 0,
		OBJ_GLOW = 1
	};

	enum StyleType {
		STYLE_MANUAL = 0,
		STYLE_AUTO = 1
	};

	static const char m_widget_type[];
	static const char *TypeName();

	PlaneButton(Widget *parent, CreateParam *param);

	virtual ~PlaneButton()
	{
	}

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void UpdateAdjustSize();
	using ButtonBase::SetTexture;
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf)
	{
		return SetTexture(surf, 0, 0);
	}
	/* the image or the glow made where it is missing */
	virtual graph::DrawObj *CreateDrawObj(int32_t obj);
	virtual uint32_t ChangeButtonState(ButtonState state);
	virtual bool UpdateObjsSize();
	virtual void UpdateButtonSize();
	virtual void SetStyleAnimator(StyleType style_type);

	/* the texture animation of the image, forward as it is pressed (no name found) */
	void ScePafWidget_8AA684F6(bool forward);
	void GetFitGeometry(math::v4 *size, math::v4 *offs) const;
	math::v4 GetLayoutSize(const math::v4 &fit_size, const math::v4 &offs) const;

	/* cleared by the constructor; nothing in PlaneButton's file reads them (names ours) */
	uint32_t m_unused0 : 4;
	uint32_t m_unused1 : 4;
	uint32_t m_unused2 : 4;
	StyleType m_style_type;
	/* the image's texture animation: its frames and the time of each (the style's
	   "frame_num", "frame_time") */
	int32_t m_frame_num;
	float m_frame_time;
};

/* a PlaneButton's resource parameters: ButtonBase's, under its name ("button_base") */
class PlaneButtonParam : public ButtonBaseParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	PlaneButtonParam();
	virtual const char *GetParamName() const;
};

/* a PlaneButton's style: its two draw objects, how it lays them out and its texture
   animation */
class PlaneButtonStyleParam : public ButtonBaseStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	PlaneButtonStyleParam() : m_adjust(NULL)
	{
		InitParam();
		m_adjust = new AdjustParam;
		m_adjust->m_ref_count++;
	}

	virtual const char *GetParamName() const;

	virtual ~PlaneButtonStyleParam()
	{
		if (m_adjust != NULL) {
			if (m_adjust->m_ref_count-- == 1)
				delete m_adjust;
			m_adjust = NULL;
		}
	}

	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	int32_t Init();
	void InitParam();

	graph::PlaneObjParam m_image_obj;
	graph::PlaneObjParam m_glow_obj;
	PlaneButton::StyleType m_style_type;
	AdjustParam *m_adjust;
	int32_t m_frame_num;
	float m_frame_time;
};

class PlaneButtonHandlerParam : public ButtonBaseHandlerParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	PlaneButtonHandlerParam()
	{
	}

	virtual const char *GetParamName() const;
};

/* lays a widget out when its adjust changes or its animator is marked dirty: Button's
   StyleAnimator again, in PlaneButton's file (class name ours) */
class PlaneButtonStyleAnimatorBase : public Animator {
public:
	PlaneButtonStyleAnimatorBase() : Animator(NULL, NULL)
	{
	}

	virtual ~PlaneButtonStyleAnimatorBase()
	{
	}

	virtual int32_t OnUpdate_core();
	/* nonzero once laid out (name ours) */
	virtual int32_t Layout() = 0;

	/* the widget's adjust, and as it was at the last layout (names ours) */
	uint32_t m_adjust_x;
	uint32_t m_adjust_y;
	uint32_t m_adjust_z;
	uint32_t m_last_adjust_x;
	uint32_t m_last_adjust_y;
	uint32_t m_last_adjust_z;
};

/* a plane button's: its planes sized to it (class name ours) */
class PlaneButtonStyleAnimator : public PlaneButtonStyleAnimatorBase {
public:
	PlaneButtonStyleAnimator()
	{
	}

	virtual ~PlaneButtonStyleAnimator()
	{
	}

	virtual int32_t Layout()
	{
		int32_t ret = 1;
		PlaneButton *button =
			m_widget != NULL && m_widget->IsInherit(PlaneButton::m_widget_type) ?
				(PlaneButton *)m_widget :
				NULL;

		if (button != NULL) {
			graph::DrawObj::EnterSafety();
			ret = button->UpdateObjsSize();
			graph::DrawObj::FinishSafety();
		}
		return ret;
	}
};

}
}

#endif
