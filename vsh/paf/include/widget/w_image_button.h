/*
 * paf::ui::ImageButton: a Button with an image (and the image behind it, which the image fades
 * out over when the texture changes), laid out beside its label, in front of it or behind it,
 * across and down. With its resource, style and handler parameters. Names after the exports
 * (CreateDrawObj, SetObjsParam, GetFrontPosX, GetFrontPosY, GetBackPosX and GetBackPosY by NID
 * hash) and the style's parameter names; the other members' names are ours.
 */
#ifndef PAF_WIDGET_W_IMAGE_BUTTON_H
#define PAF_WIDGET_W_IMAGE_BUTTON_H

#include <stdint.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <timer/timer.h>
#include <widget/w_button.h>

namespace paf {
namespace ui {

class ImageButton : public Button {
public:
	enum {
		/* the image behind and the image (names ours) */
		OBJ_IMAGE_BACK = 2,
		OBJ_IMAGE = 3
	};

	/* where the image is laid out, across and down: over the label, before it or after it
	   (names ours, after OverlayHorizontal, FrontHorizontal and BackHorizontal) */
	enum LayoutType {
		LAYOUT_OVERLAY = 0,
		LAYOUT_FRONT = 1,
		LAYOUT_BACK = 2
	};

	ImageButton(Widget *parent, CreateParam *param);
	virtual ~ImageButton();

	virtual bool IsInherit(const char *type) const;
	virtual char *GetTypeName() const;

	static const char m_widget_type[];
	static const char *TypeName();

	virtual void Update();
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				   uint32_t index);

	/* the image's */
	virtual int32_t SetTexture(const intrusive_ptr<graph::Surface> &surf)
	{
		return SetTexture(surf, OBJ_IMAGE, 0);
	}

	/* the label's */
	virtual int32_t SetString(const wstring &str)
	{
		return SetString(str, OBJ_LABEL);
	}
	using Button::SetString;
	virtual int32_t GetString(wstring &str)
	{
		return GetString(str, OBJ_LABEL);
	}
	using Button::GetString;

	virtual graph::DrawObj *CreateDrawObj(int32_t obj);
	virtual bool UpdateObjsSize();

	/* the size of both images */
	void SetTextureSize(const math::v3 &size);
	/* the fade from the image behind to the image over the time, by the timer function */
	void SetTextureCrossFadeAnim(float time, int32_t func);

	/* the image, the image behind and the label sized and placed as laid out */
	void SetObjsParam();
	void OverlayHorizontal();
	void FrontHorizontal();
	void BackHorizontal();
	void OverlayVertical();
	void FrontVertical();
	void BackVertical();

	/* the position of the size's centre at the start or at the end of the total, about the
	   contents' centre, or pos for a negative size */
	float GetFrontPosX(const float &total, const float &size, const float &pos) const;
	float GetFrontPosY(const float &total, const float &size, const float &pos) const;
	float GetBackPosX(const float &total, const float &size, const float &pos) const;
	float GetBackPosY(const float &total, const float &size, const float &pos) const;

	/* the style's layout_type_x and layout_type_y */
	int32_t m_layout_type_x;
	int32_t m_layout_type_y;
	intrusive_ptr<graph::Surface> m_image_texture;
	intrusive_ptr<graph::Surface> m_image_back_texture;
	/* the fade between them, and its time (0: no fade) */
	Timer m_crossfade_timer;
	float m_crossfade_time;
	/* the draw objects, found by UpdateObjsSize */
	graph::PlaneObj *m_image_obj;
	graph::PlaneObj *m_image_back_obj;
	graph::TextObj *m_label_obj;
	/* the image's and the label's size and position, as laid out */
	math::v3 m_image_size;
	math::v3 m_image_pos;
	math::v3 m_label_size;
	math::v3 m_label_pos;
	/* the size inside the margins, and the offset of its centre */
	math::v4 m_content_size;
	math::v2 m_content_offset;
	/* the space between the image and the label (the style's image_margin) */
	float m_image_margin;
};

/* an ImageButton's resource parameters: its textures ("texture_image", "texture_image_back") */
class ImageButtonParam : public ButtonParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ImageButtonParam();
	virtual const char *GetParamName() const;
};

/* an ImageButton's style: its images and how they are laid out */
class ImageButtonStyleParam : public ButtonStyleParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ImageButtonStyleParam()
	{
		InitParam();
	}

	virtual const char *GetParamName() const;
	virtual int32_t Create(int32_t mode);
	virtual int32_t SetEach(Widget *widget);
	virtual int32_t GetEach(Widget *widget);

	int32_t Init();
	void InitParam();

	graph::PlaneObjParam m_image_obj;
	graph::PlaneObjParam m_image_back_obj;
	int32_t m_layout_type_x;
	int32_t m_layout_type_y;
	float m_image_margin;
};

class ImageButtonHandlerParam : public ButtonBaseHandlerParam {
public:
	static const char m_param_name[];
	static const char *ParamName();

	ImageButtonHandlerParam()
	{
	}

	virtual const char *GetParamName() const;
};

}
}

extern "C" {
/* the label's width and height, its bounds where it adjusts to them (no name found) */
float ScePafWidget_06D0F83A(paf::ui::ImageButton *button);
float ScePafWidget_BEE530D5(paf::ui::ImageButton *button);
}

#endif
