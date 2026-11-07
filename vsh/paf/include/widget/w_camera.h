/*
 * paf::ui::Camera: what a scene is seen through. It looks at the screen plane from its
 * distance, turned by its roll, and draws the scene's widgets through its view and projection:
 * the widgets outside the view are culled, and the clip and scissor rectangles they ask for
 * are kept on two stacks. Names after ScePaf-RE (widget/w_camera.h) where it has them; the
 * fields it leaves unnamed are named from their uses (names ours).
 */
#ifndef PAF_WIDGET_W_CAMERA_H
#define PAF_WIDGET_W_CAMERA_H

#include <stdint.h>
#include <math/ngp/matrix.h>
#include <math/ngp/v2.h>
#include <math/rect.h>
#include <widget/core/animlistener.h>
#include <widget/core/widget.h>

namespace paf {

class Timer;

namespace graph {
class DrawInfo;
class ScreenSurface;
}

namespace ui {

class Screen;

class Camera : public Widget {
public:
	enum {
		DRAW_ORDER_ZSORT = 0,
		DRAW_ORDER_TABLE = 1
	};

	enum {
		ANIM_ROLL = 0x1000C
	};

	/* a scissor rectangle and the surface it is set on */
	class DrawScissorArg {
	public:
		DrawScissorArg(const math::rect &rect, graph::ScreenSurface *surface)
			: rect(rect), surface(surface)
		{
		}

		math::rect rect;
		graph::ScreenSurface *surface;
	};

	static const char *TypeName();

	Camera(Widget *parent, CreateParam *param);
	virtual ~Camera();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;

	/* the widget culled against the view, and drawn when it is in it */
	int32_t add_obj(Widget *root, graph::DrawInfo &info);
	/* the view and the clip and scissor stacks made from the screen: -1 while the camera is
	   held or without a screen */
	int32_t Setup(Screen *scr);

	void SetRollScreen(float roll, Timer *t);
	/* the roll, or with opt the target of a running roll animation */
	float GetRollScreen(int32_t opt);
	/* the position, rotation and roll of the other camera taken over */
	void SyncPosture(Camera *other);

	int32_t SetDrawOrder(int32_t policy)
	{
		m_draw_order = policy;
		return 0;
	}

	void UpdateZList()
	{
		m_update_zlist = true;
	}

	void GetClipRange(float &near_z, float &far_z) const
	{
		near_z = m_near_z;
		far_z = m_far_z;
	}

	/* the screen's sizes taken over again, the distance kept (0x810e47a2) */
	void UpdateScreenSize(Timer *t);
	/* the screen size and the distance, or with no distance the one the field of view
	   gives; the camera moved to it (0x810e46a4) */
	void SetScreenSize(Timer *t, float width, float height, float distance);

	/* m_roll's range kept within a turn */
	void SetRollScreen_core(float roll);

	/* the rectangle snapped to the screen's pixels (mode 0 outward, 1 inward, 2 nearest)
	   and pushed on the scissor stack (0x810e3cf4) */
	const math::rect &PushScissor(math::rect rect, int32_t mode);
	/* the rectangle seen through the view: turned by the roll and scaled (0x810e4090) */
	math::rect ProjectRect(math::rect rect, bool env_roll);
	/* how far the display area scale moves the view up (0x810e420c) */
	float GetDisplayAreaOffset();
	/* the rectangle scaled by the display area scale, where the scene shows scaled and the
	   camera does not snap (0x810e48be) */
	math::rect ScaleRect(math::rect rect);

	/* the begin and end of the scene's draws, queued in the scene's draw layer
	   (0x810e4d9e, 0x810e4eea) */
	static void DrawBegin(Camera *const &camera);
	static void DrawEnd(Camera *const &camera);

	/* the scissor and the clip stacks popped (0x810e543c, 0x810e5564) */
	static const math::rect &PopScissor();
	static const math::rect &PopClip();

	/* the scissor set on the surface */
	static void DrawScissor(const DrawScissorArg &arg);

	static const char m_widget_type[];

	/* the projection is made again at the next UpdateProjection */
	bool m_projection_dirty;
	Screen *m_screen;
	/* the roll, and the absolute sine of it */
	float m_roll;
	float m_roll_abs_sin;
	/* the screen size, the distance from the screen plane and the field of view */
	float m_screen_width;
	float m_screen_height;
	float m_distance;
	float m_fov;
	/* the size the projection is made for, and the axis the field of view is taken on */
	float m_view_width;
	float m_view_height;
	int32_t m_fov_axis;
	float m_near_z;
	float m_far_z;
	math::matrix m_view;
	math::matrix m_inv_view;
	math::matrix m_projection;
	/* one over the camera's scale */
	math::v2 m_inv_scale;
	int32_t m_draw_order;
	bool m_update_zlist;
	int32_t m_mipbase;
	int32_t m_mipoffs;
	/* the scissor rectangles are snapped to the screen's pixels */
	bool m_pixel_snap;
};

/* animates the camera's roll from m_anim_data[0] to m_anim_data[1] (Sony's name from its
   vtable) */
class RollListener : public AnimListenerF1 {
public:
	RollListener(Timer *t, Camera *camera, float start, float target)
		: AnimListenerF1(t, camera, start, target, NULL, NULL, NULL)
	{
		m_flags = FLAG_NONE;
		m_type = Camera::ANIM_ROLL;
	}

	virtual ~RollListener()
	{
	}

	virtual void OnTick(void *data1, void *data2);

	virtual float *GetTargetF()
	{
		return &m_anim_data[1];
	}
};

}
}

/* No Sony name for these exports: */
/* the projection made again from the distance and the view size, where it changed */
extern "C" void ScePafWidget_72F479CA(paf::ui::Camera *camera);
/* the screen the camera draws to, its sizes taken over */
extern "C" void ScePafWidget_8EDC2803(paf::ui::Camera *camera, paf::ui::Screen *scr);
/* the field of view, or the clip range, of the other camera taken over */
extern "C" void ScePafWidget_6AEB7291(paf::ui::Camera *camera, paf::ui::Camera *other);
extern "C" void ScePafWidget_83AD517E(paf::ui::Camera *camera, paf::ui::Camera *other);
/* the scissor rectangle queued for the screen, snapped to pixels or not, and the clip
   rectangle */
extern "C" void ScePafWidget_EFED66D2(paf::ui::Camera *camera, const paf::math::rect &rect,
				      paf::graph::DrawInfo &info);
extern "C" void ScePafWidget_4D02799A(paf::ui::Camera *camera, const paf::math::rect &rect,
				      paf::graph::DrawInfo &info);
/* the widget's clip and scissor rectangles pushed before its children are drawn, and
   popped after */
extern "C" void ScePafWidget_56B8240B(paf::ui::Camera *camera, paf::ui::Widget *widget,
				      paf::graph::DrawInfo &info);
extern "C" void ScePafWidget_249D56BF(paf::ui::Camera *camera, paf::ui::Widget *widget,
				      paf::graph::DrawInfo &info);
/* the scissor set on the surface, snapped to pixels */
extern "C" void ScePafWidget_C14CCBD4(const paf::ui::Camera::DrawScissorArg &arg);
/* the widget and its children culled and drawn, the clip and scissor rectangles it asks for
   pushed around them */
extern "C" void ScePafWidget_1A6D5164(paf::ui::Camera *camera, paf::ui::Widget *widget,
				      paf::graph::DrawInfo &info);
/* the scene's widgets drawn through the camera: the tree walked and the draws queued */
extern "C" void ScePafWidget_80946FB9(paf::ui::Camera *camera);

#endif
