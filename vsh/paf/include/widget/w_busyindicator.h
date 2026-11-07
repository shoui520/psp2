/*
 * paf::ui::BusyIndicator: the busy indicator, a ball that circles the widget's centre and
 * leaves a trail of fading balls, grown in when it starts and shrunk away when it stops. The
 * classes, the functions and the strings of the resource parameters are Sony's (export NIDs,
 * ScePaf-RE's header after them, the parameter names); the names of the members other than
 * m_ball_size are ours.
 */
#ifndef PAF_WIDGET_W_BUSYINDICATOR_H
#define PAF_WIDGET_W_BUSYINDICATOR_H

#include <stdint.h>
#include <math/ngp/v4.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class BusyIndicator : public Widget {
public:
	/* what m_anim_state is (names ours) */
	enum AnimState {
		ANIM_STOPPED = 0,
		ANIM_STARTING = 1,
		ANIM_RUNNING = 2,
		ANIM_STOPPING = 3
	};

	static const char m_widget_type[];
	static const char *TypeName();

	BusyIndicator(Widget *parent, CreateParam *param);
	virtual ~BusyIndicator();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual void DestroyWidget();
	virtual uint32_t CheckState();
	virtual void UpdateSize();
	virtual void Update();
	virtual int32_t OnDraw(graph::DrawInfo &info);
	/* the texture of every ball */
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf);
	/* the color of the ball obj, or of every ball for 0; each keeps its alpha */
	virtual int32_t SetObjColor(const math::v4 &color, int32_t obj);
	virtual int32_t GetObjColor(math::v4 &color, int32_t obj);

	void SetBallSize(float size);
	void Start();
	void Stop();
	void SendEvent(int32_t type, int32_t d0, int32_t d1, int32_t d2, int32_t d3);

	/* where the indicator is in its start and stop (name ours) */
	int32_t m_anim_state;
	/* zeroed by the constructor, read by nothing (name ours) */
	math::v4 m_unused;
	/* the angle the head moves on before a trail ball is left behind, 2 pi / 10 (name ours) */
	float m_trail_spacing;
	/* the head's angle, a sixth of it, from 0 to pi (name ours) */
	float m_rotation;
	/* the time since the start or stop began (name ours) */
	float m_anim_time;
	/* the head's angle when it left the last trail ball (name ours) */
	float m_last_trail_angle;
	float m_ball_size;
};

/* a BusyIndicator's resource parameters: WidgetParam's, nothing of its own */
class BusyIndicatorParam : public WidgetParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	BusyIndicatorParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);
};

/* a BusyIndicator's style: its ball's size and color */
class BusyIndicatorStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	BusyIndicatorStyleParam();
	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	float m_ball_size;
	math::v4 m_ball_color;
};

}
}

extern "C" {
/* does nothing (export C371ECA3; no name found) */
void ScePafWidget_C371ECA3(void);
}

#endif
