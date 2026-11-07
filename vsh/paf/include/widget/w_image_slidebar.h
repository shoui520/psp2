/*
 * paf::ui::ImageSlideBar: a bar over its base, stretched to its value of the maximum, set by a
 * touch along it or by the left and right keys. With its resource, style and sound parameters.
 * Names after the exports and the parameters' names (the enums after ScePaf-RE); the members'
 * names are ours.
 */
#ifndef PAF_WIDGET_W_IMAGE_SLIDEBAR_H
#define PAF_WIDGET_W_IMAGE_SLIDEBAR_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <math/ngp/v4.h>
#include <std/list>
#include <timer/timer.h>
#include <widget/core/widget.h>

namespace paf {
namespace ui {

class ImageSlideBar : public Widget {
public:
	enum {
		OBJ_BASE = 0,
		OBJ_BAR = 1,
		OBJ_IMAGE_SLIDEBAR_END = 2
	};

	enum {
		CB_IMAGE_SLIDEBAR_CHANGED = 0x10000000,
		CB_IMAGE_SLIDEBAR_DRAG_BEGIN = 0x10000001,
		CB_IMAGE_SLIDEBAR_DRAG_END = 0x10000002,
		CB_IMAGE_SLIDEBAR_END = 0x10000003
	};

	/* its sounds, "drag_begin" and "drag_end" (names ours) */
	enum {
		SOUND_DRAG_BEGIN = 0,
		SOUND_DRAG_END = 1,
		SOUND_IMAGE_SLIDEBAR_END = 2
	};

	/* whether the value goes from 0 or from 1 */
	enum BaseMode {
		BASE_MODE_ZERO_BASED = 0,
		BASE_MODE_ONE_BASED = 1
	};

	/* whether the bar shows its texture stretched or one step of it per value */
	enum TextureMode {
		TEXTURE_MODE_WHOLE = 0,
		TEXTURE_MODE_STEP = 1
	};

	ImageSlideBar(Widget *parent, CreateParam *param);
	virtual ~ImageSlideBar();

	virtual int32_t HandleKeycodeEvent(Event *e);
	virtual int32_t HandleTouchEvent(Event *e);
	virtual int32_t HandlePointEvent(Event *e);
	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;
	virtual uint32_t CheckState();
	virtual void UpdateSize();
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);
	using Widget::SetTexture;
	virtual WidgetSoundParam::Sound *GetSound(int32_t id);
	virtual void AddSound(uint32_t index, WidgetSoundParam::Sound &sound);

	static const char m_widget_type[];
	static const char *TypeName();

	virtual int32_t SetObjColor(const math::v4 &color, int32_t obj);
	virtual int32_t GetObjColor(math::v4 &color, int32_t obj);
	virtual int32_t SetBaseMode(BaseMode mode);
	virtual int32_t SetTextureMode(TextureMode mode);
	virtual int32_t SetMaxValue(int32_t max);
	virtual int32_t SetValue(int32_t value, bool anim);
	virtual int32_t SetBarAnim(float time, int32_t func);
	/* the value kept in its range, and its share of the maximum */
	virtual void AdjustParameter();
	virtual void UpdateBase();
	/* the bar sized and placed to the value, over the bar's animation with anim */
	virtual void UpdateBar(bool anim);
	virtual void OnTouch(Event *e);
	/* the value under the touch it follows */
	virtual void OnTouchUpdateValue();
	virtual void OnDragEnd(bool abort);

	/* the region of the plane's texture shown, over the timer's animation with one */
	void SetTexRegion(int32_t obj, const math::v4 &region, Timer *t, int32_t option);

	void GetBarAnim(float *time, int32_t *func)
	{
		if (time != NULL)
			*time = m_anim_time;
		if (func != NULL)
			*func = m_anim_func;
	}

	BaseMode GetBaseMode()
	{
		return m_base_mode;
	}

	TextureMode GetTextureMode()
	{
		return m_texture_mode;
	}

	int32_t GetValue()
	{
		return m_value;
	}

	BaseMode m_base_mode;
	TextureMode m_texture_mode;
	int32_t m_max;
	int32_t m_value;
	/* the bar's animation: its time and timer function */
	float m_anim_time;
	int32_t m_anim_func;
	/* the value's share of the maximum */
	float m_norm_value;
	/* the touch it follows, -1 for none */
	int32_t m_touch_id;
	/* the sounds added to it, by index */
	list<WidgetSoundParam::Sound> m_sound_list;
};

/* an ImageSlideBar's resource parameters: its bar's texture ("texture_bar"), base_mode and
   tex_mode */
class ImageSlideBarParam : public WidgetParam {
public:
	/* a texture the resource names, and the draw object it is for (name ours) */
	class TextureRef {
	public:
		const char *name;
		int32_t index;
	};

	static const char m_param_name[];
	static const TextureRef c_textureList[];
	static const char *ParamName();

	ImageSlideBarParam();
	virtual const char *GetParamName() const;
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);

	int32_t m_base_mode;
	int32_t m_tex_mode;
};

/* an ImageSlideBar's style: its base's and bar's planes and the bar's animation */
class ImageSlideBarStyleParam : public WidgetStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ImageSlideBarStyleParam();
	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	float m_anim_time;
	int32_t m_anim_func;
	graph::PlaneObjParam m_base_obj;
	graph::PlaneObjParam m_bar_obj;
};

/* an ImageSlideBar's two sounds */
class ImageSlideBarSoundParam : public WidgetSoundParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ImageSlideBarSoundParam();
	virtual const char *GetParamName() const;
};

}
}

#endif
