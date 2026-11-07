/*
 * paf::ui::SpeechBalloon: a plane drawn as a balloon whose tail points to one side, with a
 * shadow plane (draw object 1) its style may give, and sized to its widest and tallest child
 * where it adjusts to them. The class, TypeName, the parameter names and the exports are Sony's
 * (export NIDs hash as them); the members' names are ours.
 */
#ifndef PAF_WIDGET_W_SPEECH_BALLOON_H
#define PAF_WIDGET_W_SPEECH_BALLOON_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <widget/w_plane.h>

namespace paf {
namespace ui {

class SpeechBalloon : public Plane {
public:
	/* the side its tail is on, which of its texture's margins is the tail (names ours) */
	enum TailDir {
		TAIL_NONE = 0,
		TAIL_LEFT = 1,
		TAIL_RIGHT = 2,
		TAIL_BOTTOM = 3,
		TAIL_TOP = 4
	};

	/* the draw object of its shadow (name ours) */
	enum {
		OBJ_SHADOW = 1
	};

	static const char m_widget_type[];
	static const char *TypeName();

	SpeechBalloon(Widget *parent, CreateParam *param);

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	/* Plane's, and 3 while it prepares */
	virtual uint32_t CheckState();
	/* Widget's, not Plane's */
	virtual void UpdateSize();
	/* the width and height it adjusts to (adjust 2) taken from its widest and tallest child */
	virtual void UpdateAdjustSize();

	/* the length of its tail: its texture's margin on the tail's side, 0 without a texture */
	float GetTailLength();
	/* where along its side the tail is: the plane's division centre */
	void SetTailPos(float pos);
	/* its shadow plane, made at draw object 1 when it has none (name ours) */
	graph::PlaneObj *CreateShadowObj();

	/* the side its tail is on (TailDir) */
	int32_t m_tail_dir;
};

/* a speech balloon's style: its plane's and its shadow's */
class SpeechBalloonStyleParam : public PlaneStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	SpeechBalloonStyleParam();
	virtual const char *GetParamName() const;
	/* the shadow made where the style gives one, then WidgetStyleParam's */
	virtual int32_t SetEach(Widget *widget);

	graph::PlaneObjParam m_shadow_obj;
};

}
}

#endif
