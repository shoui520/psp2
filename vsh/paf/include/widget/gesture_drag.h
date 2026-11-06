/*
 * The drags the widget's gesture handling follows: GestureDrag keeps where a drag started and
 * where it is, on one axis or both, and (for TYPE_RANGE) the range it has covered, pulling the
 * start point toward the far end of that range as the drag goes on. GestureDragRatio adds how far
 * the drag has gone between a near and a far distance, as a ratio from 0 to 1 that eases in
 * (cubic), signed by the direction. No Sony record or string names these classes, their methods
 * or members: the names are ours.
 */
#ifndef PAF_WIDGET_GESTURE_DRAG_H
#define PAF_WIDGET_GESTURE_DRAG_H

#include <stddef.h>
#include <stdint.h>

namespace paf {
namespace ui {

/* 0x30 bytes */
class GestureDrag {
public:
	/* which coordinates the drag follows */
	enum {
		AXIS_X = 0,
		AXIS_Y = 1,
		AXIS_XY = 2
	};

	/* TYPE_RANGE keeps the range covered and moves the start point with it */
	enum {
		TYPE_RANGE = 1
	};

	/* 0x811c2014 (TOOL) */
	GestureDrag(int32_t axis, int32_t type);
	/* 0x811c203c */
	virtual ~GestureDrag();

	/* 0x811c206c: the drag from (start_x, start_y), now at (x, y) */
	virtual bool Begin(float start_x, float start_y, float x, float y);
	/* 0x811c210a */
	virtual bool Move(float x, float y);
	/* 0x811c2214 */
	virtual bool End(float x, float y);

	virtual bool IsActive()
	{
		return m_active;
	}

	virtual float GetStartX()
	{
		return m_start_x;
	}

	virtual float GetStartY()
	{
		return m_start_y;
	}

	virtual float GetX()
	{
		return m_x;
	}

	virtual float GetY()
	{
		return m_y;
	}

	virtual int32_t GetAxis()
	{
		return m_axis;
	}

	virtual int32_t GetType()
	{
		return m_type;
	}

	int32_t m_axis;
	int32_t m_type;
	bool m_active;
	float m_start_x;
	float m_start_y;
	float m_x;
	float m_y;
	/* the range the drag has covered (TYPE_RANGE) */
	float m_min_x;
	float m_min_y;
	float m_max_x;
	float m_max_y;
};

/* the distances GestureDragRatio's ratio starts at, before its scale */
class GestureDragRatioParam {
public:
	GestureDragRatioParam() : m_near_x(5.0f), m_near_y(5.0f)
	{
	}

	virtual ~GestureDragRatioParam()
	{
	}

	float m_near_x;
	float m_near_y;
};

/* GestureDragRatioTouch's */
class GestureDragRatioTouchParam : public GestureDragRatioParam {
};

/* 0x4c bytes */
class GestureDragRatio : public GestureDrag {
public:
	/* 0x811af944 (TOOL) */
	GestureDragRatio(int32_t axis, int32_t type, float width, float height, float scale,
			 float far_x, float far_y);
	/* 0x811afa4e */
	virtual ~GestureDragRatio();

	/* 0x811afaa6: how far the drag has gone, from -1 to 1 on each axis */
	virtual int32_t GetRatio(float *ratio_x, float *ratio_y);

	virtual bool IsEnabled()
	{
		return m_enabled;
	}

	/* 0x811afc82 */
	bool SetFarX(float far_x);

	/* set when the size and the scale are not 0 */
	bool m_enabled;
	float m_width;
	float m_height;
	const GestureDragRatioParam *m_param;
	float m_scale;
	float m_far_x;
	float m_far_y;
};

/* the same, with its own parameter */
class GestureDragRatioTouch : public GestureDragRatio {
public:
	/* 0x811afca0 (TOOL) */
	GestureDragRatioTouch(int32_t axis, int32_t type, float width, float height, float scale,
			      float far_x, float far_y);
	/* 0x811afd24 */
	virtual ~GestureDragRatioTouch();
};

}
}

#endif
