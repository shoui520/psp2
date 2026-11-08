/*
 * paf::ui::PageTurn: a PageTurnBase that draws the page turning over: its plane, the page turn
 * object that folds its corner and a label; the corner follows the drag, springs back when it
 * is let go and wobbles when the page moves quickly. With its resource and style parameters.
 * The class and parameter names, TypeName and the parameter names are Sony's (the mangled
 * exports and "pageturn", "style_pageturn"); the members, the animation and object IDs and the
 * functions the exports name only by NID are ours.
 */
#ifndef PAF_WIDGET_W_PAGETURN_H
#define PAF_WIDGET_W_PAGETURN_H

#include <stdint.h>
#include <graphics/drawobj/page_turn_obj.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <widget/core/animlistener.h>
#include <widget/w_pageturn_base.h>

namespace paf {
namespace ui {

class PageTurn : public PageTurnBase {
public:
	enum {
		/* the draw objects (names ours) */
		OBJ_TURN = 0,
		OBJ_LABEL = 1,
		OBJ_PLANE = 2
	};

	enum {
		/* the animations of m_turn and m_turn_offset (names ours) */
		ANIM_TURN = 0x1000C,
		ANIM_TURN_OFFSET = 0x1000D
	};

	static const char m_widget_type[];
	static const char *TypeName();

	PageTurn(Widget *parent, CreateParam *param);
	virtual ~PageTurn();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	/* the page comes in: shown at once and moved up from 200 behind, its corner folding in */
	virtual float DoTransition_core(float parameter, bool set);
	/* the page goes out: the corner turned over the whole page, the page moved 200 behind and
	   faded out */
	virtual float DoTransitionReverse_core(float parameter, bool set);
	/* the corner sized again, and the plane's and the turn object's sizes the page's */
	virtual void UpdateSize();
	/* the corner follows the drag; a quick move of the page shakes it */
	virtual void UpdateAfterChildren();
	/* the plane drawn opaque only while the page is, and the turn object given the corner */
	virtual int32_t OnDraw(graph::DrawInfo &info);
	/* the turn object's texture is the plane's too */
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	virtual int32_t GetTexture(intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	/* the corner animated for the state: turned back to its place, opened half for the
	   disabled page, or held where the drag takes it */
	virtual int32_t ScePafWidget_A4245073(int32_t state);

	/* the corner moved to the position, animated by the timer where there is one (no name
	   found) */
	int32_t ScePafWidget_B0E5E134(const math::v4 &turn, Timer *t, uint32_t id, float delay,
				      int32_t slot, AnimCB cb, void *data);
	/* the corner's offset the same way (no name found) */
	int32_t ScePafWidget_AD9098A3(const math::v4 &offset, Timer *t, uint32_t id, float delay,
				      int32_t slot, AnimCB cb, void *data);
	/* the corner's size as a part of the page's height and its width to its height; the corner
	   moved there unless it is dragged (no name found) */
	void ScePafWidget_F378C87D(float ratio, float aspect);

	/* the corner's height as a part of the page's height, and its width to its height */
	float m_fold_ratio;
	float m_fold_aspect;
	/* the page moved quickly, the corner is shaking */
	bool m_shaking;
	/* where the corner was from the drag's start when the drag began */
	math::v2 m_drag_offset;
	/* the corner, and how far it is shaken */
	math::v2 m_turn;
	math::v2 m_turn_offset;
	/* where the page was the last frame, how fast it moved and how much faster than before */
	math::v2 m_last_pos;
	math::v2 m_velocity;
	math::v2 m_acceleration;
	graph::PageTurnObj *m_turn_obj;
	graph::PlaneObj *m_plane;
};

/* a PageTurn's resource parameters: its label ("label") and its texture ("texture") */
class PageTurnParam : public WidgetParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	PageTurnParam();
	virtual const char *GetParamName() const;
};

/* a PageTurn's style: the turn object and the label */
class PageTurnStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	PageTurnStyleParam()
	{
		InitParam();
	}

	virtual const char *GetParamName() const;
	int32_t Init();
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	void InitParam();

	graph::PlaneObjParam m_turn_obj;
	graph::TextObjParam m_label_obj;
};

}
}

extern "C" {
/* the animated corner and offset set and read (no names found) */
void ScePafWidget_6CC95725(paf::ui::AnimListener *anim, paf::math::v4 data);
paf::math::v4 ScePafWidget_3D6C290E(paf::ui::AnimListener *anim);
void ScePafWidget_4EA317B8(paf::ui::AnimListener *anim, paf::math::v4 data);
paf::math::v4 ScePafWidget_6206FB7E(paf::ui::AnimListener *anim);
}

#endif
