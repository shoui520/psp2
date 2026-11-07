/*
 * paf::ui::ImageButton (DEBUGGER 0x810f0338..0x810f1f10): a button with an image beside its label,
 * the image fading in over the one behind it when its texture changes. No record or string names
 * the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/drawobj/plane_obj.h>
#include <graphics/drawobj/text_obj.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>
#include <plugin.h>
#include <timer/timer.h>
#include <versatileparam/resourceparam.h>
#include <widget/factory.h>
#include <widget/w_image_button.h>

namespace paf {
namespace ui {

/* a plane for the image or the image behind, kept to its texture's aspect (name ours) */
static graph::PlaneObj *CreatePlane(ImageButton *button, int32_t id)
{
	graph::DrawStyle style;

	style.blend = 1;
	style.depth = 1;
	style.lighting = 1;
	style.face_cull = 0;
	style.scissor = 0;
	style.deepen_color = 0;
	style.reduce_transparency = 0;
	style.surface_draw = 0;
	style.reserved1 = 0;

	graph::PlaneObj *obj = new graph::PlaneObj(style);

	if (obj != NULL) {
		obj->SetScaleMode(graph::PlaneObj::SCALE_ASPECT_SIZE, graph::PlaneObj::SCALE_ASPECT_SIZE);

		common::SharedPtr<graph::DrawObj> ptr(obj);
		Widget::DrawObjAttr attr(id);

		attr.m_flag0 = false;
		attr.m_flag1 = false;
		button->InsertDrawObj(button->LowerBoundDrawObj(id), Widget::DrawObjEntry(ptr, attr),
				      false);
	}
	return obj;
}

ImageButton::ImageButton(Widget *parent, CreateParam *param)
	: Button(parent, param), m_layout_type_x(0), m_layout_type_y(0), m_image_texture(),
	  m_image_back_texture(), m_crossfade_timer(0.0f), m_crossfade_time(0.0f), m_image_obj(NULL),
	  m_image_back_obj(NULL), m_label_obj(NULL), m_image_size(0.0f, 0.0f, 0.0f),
	  m_image_pos(0.0f, 0.0f, 0.0f), m_label_size(0.0f, 0.0f, 0.0f), m_label_pos(0.0f, 0.0f, 0.0f),
	  m_content_size(0.0f, 0.0f, 0.0f, 0.0f), m_content_offset(0.0f, 0.0f), m_image_margin(0.0f)
{
	/* through the vtable, as Sony's */
	Button *self = this;

	if (param == NULL || param->m_create_drawobj) {
		CreatePlane(this, OBJ_IMAGE_BACK);
		CreatePlane(this, OBJ_IMAGE);
	}
	m_crossfade_timer.DoTick();
	self->SetStyleAnimator(self->m_style_type);
}

static const struct {
	const char *name;
	int32_t index;
} s_image_button_textures[] = {
	{ "texture_image", ImageButton::OBJ_IMAGE },
	{ "texture_image_back", ImageButton::OBJ_IMAGE_BACK },
};

ImageButtonParam::ImageButtonParam()
{
	for (uint32_t i = 0; i < sizeof(s_image_button_textures) / sizeof(s_image_button_textures[0]);
	     i++)
		m_texture_list.push_back(IndexedRef(s_image_button_textures[i].name,
						    s_image_button_textures[i].index));
}

/* the style's draw object parameters by name (name ours) */
static const char *const s_image_button_objs[] = {
	"image_obj",
	"image_back_obj",
};

void ImageButtonStyleParam::InitParam()
{
	m_layout_type_x = 0;
	m_layout_type_y = 0;
	m_image_margin = 0.0f;
	m_image_obj.m_name = s_image_button_objs[0];
	m_image_obj.m_index = ImageButton::OBJ_IMAGE;
	m_drawobj_param_list.push_back(&m_image_obj);
	m_image_back_obj.m_name = s_image_button_objs[1];
	m_image_back_obj.m_index = ImageButton::OBJ_IMAGE_BACK;
	m_drawobj_param_list.push_back(&m_image_back_obj);
}

const char ImageButton::m_widget_type[] = "image_button";

bool ImageButton::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Button::IsInherit(type);
}

char *ImageButton::GetTypeName() const
{
	return (char *)m_widget_type;
}

const char *ImageButton::TypeName()
{
	return m_widget_type;
}

/* the draw object's alpha, keeping its color */
static inline void SetObjAlpha(graph::DrawObj *obj, float alpha)
{
	if (obj != NULL) {
		math::v4 color = obj->GetColor();

		color.set_w(alpha);
		obj->SetColor(color);
	}
}

void ImageButton::Update()
{
	ButtonBase::Update();
	if (!m_crossfade_timer.IsFinished()) {
		m_crossfade_timer.DoTick();

		float rate = m_crossfade_timer.m_mapped_x;

		SetObjAlpha(GetDrawObj(OBJ_IMAGE_BACK), 1.0f - rate);
		SetObjAlpha(GetDrawObj(OBJ_IMAGE), rate);
	}
}

void ImageButton::SetObjsParam()
{
	if (m_image_obj != NULL) {
		m_image_obj->SetSize(m_image_size);
		m_image_obj->SetPos(m_image_pos);
		if (m_image_back_obj != NULL) {
			m_image_back_obj->SetSize(m_image_size);
			m_image_back_obj->SetPos(m_image_pos);
		}
	}
	if (m_label_obj != NULL) {
		m_label_obj->SetSize(m_label_size);
		m_label_obj->SetPos(m_label_pos);
	}
}

}
}

using namespace paf;
using namespace paf::ui;

float ScePafWidget_BEE530D5(ImageButton *button)
{
	if (button->m_label_obj == NULL)
		return 0.0f;

	math::v2 size(0.0f, 0.0f);
	math::v2 bounds(0.0f, 0.0f);
	bool adjust;

	button->m_label_obj->GetLayoutAttribute(graph::TextLayoutAttribute_Size, size);
	adjust = false;
	button->m_label_obj->GetBounds(bounds);
	button->m_label_obj->GetLayoutAttribute(graph::TextLayoutAttribute_AdjustY, adjust);
	if (adjust)
		size.set_y(bounds.extract_y());
	return size.extract_y();
}

namespace paf {
namespace ui {

void ImageButton::OverlayVertical()
{
	switch (m_adjust_y) {
	case 0:
	case 1:
		m_image_size.set_y(m_content_size.extract_y());
		if (m_image_size.extract_y() >= 0.0f)
			m_image_pos.set_y(m_content_offset.extract_y());
		m_label_size.set_y(m_content_size.extract_y());
		if (m_label_size.extract_y() >= 0.0f)
			m_label_pos.set_y(m_content_offset.extract_y());
		break;
	case 2:
	case 3: {
		float offset = m_content_offset.extract_y();

		m_label_size.set_y(ScePafWidget_BEE530D5(this));
		m_label_pos.set_y(offset);
		m_image_pos.set_y(offset);
		break;
	}
	}
	SetObjsParam();
}

float ImageButton::GetBackPosY(const float &total, const float &size, const float &pos) const
{
	float ret = pos;

	if (size >= 0.0f)
		ret = m_content_offset.extract_y() + (size - total) * 0.5f;
	return ret;
}

float ImageButton::GetFrontPosY(const float &total, const float &size, const float &pos) const
{
	float ret = pos;

	if (size >= 0.0f)
		ret = m_content_offset.extract_y() + (total - size) * 0.5f;
	return ret;
}

void ImageButton::BackVertical()
{
	float total = 0.0f;
	float height = 0.0f;

	switch (m_adjust_y) {
	case 0:
	case 1:
		/* the label in what the image leaves */
		total = m_content_size.extract_y();
		height = total - (m_image_size.extract_y() + m_image_margin);
		break;
	case 2:
	case 3:
		/* the label at its own size */
		total = m_label_size.extract_y() + m_image_size.extract_y() + m_image_margin;
		height = ScePafWidget_BEE530D5(this);
		break;
	}
	m_label_size.set_y(height);
	m_label_pos.set_y(GetFrontPosY(total, m_label_size.extract_y(), m_label_pos.extract_y()));
	m_image_pos.set_y(GetBackPosY(total, m_image_size.extract_y(), m_image_pos.extract_y()));
	SetObjsParam();
}

void ImageButton::FrontVertical()
{
	float total = 0.0f;
	float height = 0.0f;

	switch (m_adjust_y) {
	case 0:
	case 1:
		/* the label in what the image leaves */
		total = m_content_size.extract_y();
		height = total - (m_image_size.extract_y() + m_image_margin);
		break;
	case 2:
	case 3:
		/* the label at its own size */
		total = m_label_size.extract_y() + m_image_size.extract_y() + m_image_margin;
		height = ScePafWidget_BEE530D5(this);
		break;
	}
	m_label_size.set_y(height);
	m_label_pos.set_y(GetBackPosY(total, m_label_size.extract_y(), m_label_pos.extract_y()));
	m_image_pos.set_y(GetFrontPosY(total, m_image_size.extract_y(), m_image_pos.extract_y()));
	SetObjsParam();
}

}
}

float ScePafWidget_06D0F83A(ImageButton *button)
{
	if (button->m_label_obj == NULL)
		return 0.0f;

	math::v2 size(0.0f, 0.0f);
	math::v2 bounds(0.0f, 0.0f);
	bool adjust;

	button->m_label_obj->GetLayoutAttribute(graph::TextLayoutAttribute_Size, size);
	adjust = false;
	button->m_label_obj->GetBounds(bounds);
	button->m_label_obj->GetLayoutAttribute(graph::TextLayoutAttribute_AdjustX, adjust);
	if (adjust)
		size.set_x(bounds.extract_x());
	return size.extract_x();
}

namespace paf {
namespace ui {

void ImageButton::OverlayHorizontal()
{
	switch (m_adjust_x) {
	case 0:
	case 1:
		m_image_size.set_x(m_content_size.extract_x());
		if (m_image_size.extract_x() >= 0.0f)
			m_image_pos.set_x(m_content_offset.extract_x());
		m_label_size.set_x(m_content_size.extract_x());
		if (m_label_size.extract_x() >= 0.0f)
			m_label_pos.set_x(m_content_offset.extract_x());
		break;
	case 2:
	case 3: {
		float offset = m_content_offset.extract_x();

		m_label_size.set_x(ScePafWidget_06D0F83A(this));
		m_label_pos.set_x(offset);
		m_image_pos.set_x(offset);
		break;
	}
	}
	SetObjsParam();
}

float ImageButton::GetBackPosX(const float &total, const float &size, const float &pos) const
{
	float ret = pos;

	if (size >= 0.0f)
		ret = m_content_offset.extract_x() + (total - size) * 0.5f;
	return ret;
}

float ImageButton::GetFrontPosX(const float &total, const float &size, const float &pos) const
{
	float ret = pos;

	if (size >= 0.0f)
		ret = m_content_offset.extract_x() + (size - total) * 0.5f;
	return ret;
}

void ImageButton::BackHorizontal()
{
	float total = 0.0f;
	float width = 0.0f;

	switch (m_adjust_x) {
	case 0:
	case 1:
		/* the label in what the image leaves */
		total = m_content_size.extract_x();
		width = total - (m_image_size.extract_x() + m_image_margin);
		break;
	case 2:
	case 3:
		/* the label at its own size */
		total = m_label_size.extract_x() + m_image_size.extract_x() + m_image_margin;
		width = ScePafWidget_06D0F83A(this);
		break;
	}
	m_label_size.set_x(width);
	m_label_pos.set_x(GetFrontPosX(total, m_label_size.extract_x(), m_label_pos.extract_x()));
	m_image_pos.set_x(GetBackPosX(total, m_image_size.extract_x(), m_image_pos.extract_x()));
	SetObjsParam();
}

void ImageButton::FrontHorizontal()
{
	float total = 0.0f;
	float width = 0.0f;

	switch (m_adjust_x) {
	case 0:
	case 1:
		/* the label in what the image leaves */
		total = m_content_size.extract_x();
		width = total - (m_image_size.extract_x() + m_image_margin);
		break;
	case 2:
	case 3:
		/* the label at its own size */
		total = m_label_size.extract_x() + m_image_size.extract_x() + m_image_margin;
		width = ScePafWidget_06D0F83A(this);
		break;
	}
	m_label_size.set_x(width);
	m_label_pos.set_x(GetBackPosX(total, m_label_size.extract_x(), m_label_pos.extract_x()));
	m_image_pos.set_x(GetFrontPosX(total, m_image_size.extract_x(), m_image_pos.extract_x()));
	SetObjsParam();
}

/* the draw object with the ID when it is a plane, else NULL (inlined in Sony's code, name ours) */
static inline graph::PlaneObj *GetPlaneObj(Widget *widget, int32_t id)
{
	if (widget->GetDrawObj(id) != NULL &&
	    widget->GetDrawObj(id)->IsInherit(graph::PlaneObj::DrawObjType()))
		return (graph::PlaneObj *)widget->GetDrawObj(id);
	return NULL;
}

/* the same for a text (inlined in Sony's code, name ours) */
static inline graph::TextObj *GetTextObj(Widget *widget, int32_t id)
{
	if (widget->GetDrawObj(id) != NULL &&
	    widget->GetDrawObj(id)->IsInherit(graph::TextObj::DrawObjType()))
		return (graph::TextObj *)widget->GetDrawObj(id);
	return NULL;
}

bool ImageButton::UpdateObjsSize()
{
	const AdjustParam *adjust = &ScePafWidget_BE1CEA2E;

	if (m_adjust_param != NULL)
		adjust = m_adjust_param;

	math::v4 margin(adjust->m_right_margin + adjust->m_left_margin,
			adjust->m_top_margin + adjust->m_bottom_margin, 0.0f, 0.0f);

	m_content_size = math::v4(sce::Vectormath::Simd::Aos::maxPerElem(
		math::v4(0.0f, 0.0f, 0.0f, 0.0f).getImpl(), (*GetSize(0) - margin).getImpl()));
	m_content_offset = math::v2(adjust->m_left_margin - adjust->m_right_margin,
				    adjust->m_bottom_margin - adjust->m_top_margin) /
			   2.0f;

	m_image_back_obj = GetPlaneObj(this, OBJ_IMAGE_BACK);
	m_image_obj = GetPlaneObj(this, OBJ_IMAGE);
	m_label_obj = GetTextObj(this, OBJ_LABEL);

	m_image_size = m_image_obj != NULL ? m_image_obj->m_size : math::v3(0.0f, 0.0f, 0.0f);
	m_image_pos = m_image_obj != NULL ? m_image_obj->m_pos : math::v3(0.0f, 0.0f, 0.0f);
	m_label_size = m_label_obj != NULL ? m_label_obj->m_size : math::v3(0.0f, 0.0f, 0.0f);
	m_label_pos = m_label_obj != NULL ? m_label_obj->m_pos : math::v3(0.0f, 0.0f, 0.0f);

	int32_t layout_x = m_layout_type_x;
	int32_t layout_y = m_layout_type_y;

	switch (layout_x) {
	case LAYOUT_FRONT:
		FrontHorizontal();
		break;
	case LAYOUT_BACK:
		BackHorizontal();
		break;
	default:
		OverlayHorizontal();
		break;
	}
	switch (layout_y) {
	case LAYOUT_FRONT:
		FrontVertical();
		break;
	case LAYOUT_BACK:
		BackVertical();
		break;
	default:
		OverlayVertical();
		break;
	}
	return true;
}

graph::DrawObj *ImageButton::CreateDrawObj(int32_t obj)
{
	graph::DrawObj *draw = GetDrawObj(obj);

	if (draw != NULL)
		return draw;
	switch (obj) {
	case OBJ_IMAGE_BACK:
		return CreatePlane(this, OBJ_IMAGE_BACK);
	case OBJ_IMAGE:
		return CreatePlane(this, OBJ_IMAGE);
	}
	return Button::CreateDrawObj(obj);
}

int32_t ImageButton::SetTexture(const intrusive_ptr<graph::Surface> &surf, int32_t obj,
				uint32_t index)
{
	if (obj == OBJ_IMAGE && m_crossfade_time != 0.0f) {
		/* the old image behind, the new one fading in over it */
		m_crossfade_timer.ResetTimer();
		m_image_back_texture = m_image_texture;
		m_image_texture = surf;
		Button::SetTexture(m_image_back_texture, OBJ_IMAGE_BACK, index);
		SetObjAlpha(GetDrawObj(OBJ_IMAGE_BACK), 1.0f);
		SetObjAlpha(GetDrawObj(OBJ_IMAGE), 0.0f);
	} else if (obj == OBJ_IMAGE_BACK) {
		m_image_back_texture = surf;
	} else if (obj == OBJ_IMAGE) {
		m_image_texture = surf;
	}
	return Button::SetTexture(surf, obj, index);
}

void ImageButton::SetTextureSize(const math::v3 &size)
{
	for (int32_t i = OBJ_IMAGE_BACK; i <= OBJ_IMAGE; i++) {
		graph::PlaneObj *plane = GetPlaneObj(this, i);

		if (plane != NULL)
			plane->SetSize(size);
	}
}

void ImageButton::SetTextureCrossFadeAnim(float time, int32_t func)
{
	m_crossfade_timer.InitTimer(time, func, Timer::REPEAT_NONE, -1, NULL);
	m_crossfade_timer.m_curr = time * 0.001f;
	m_crossfade_timer.DoTick();
	if (m_crossfade_time != 0.0f && time == 0.0f) {
		/* no fade any more: the image shown */
		SetObjAlpha(GetDrawObj(OBJ_IMAGE_BACK), 0.0f);
		SetObjAlpha(GetDrawObj(OBJ_IMAGE), 1.0f);
	}
	m_crossfade_time = time;
}

ImageButton::~ImageButton()
{
}

const char ImageButtonParam::m_param_name[] = "image_button_base";

const char *ImageButtonParam::GetParamName() const
{
	return m_param_name;
}

const char *ImageButtonParam::ParamName()
{
	return m_param_name;
}

const char ImageButtonStyleParam::m_param_name[] = "style_image_button";

const char *ImageButtonStyleParam::GetParamName() const
{
	return m_param_name;
}

const char *ImageButtonStyleParam::ParamName()
{
	return m_param_name;
}

int32_t ImageButtonStyleParam::Init()
{
	int32_t ret = ButtonStyleParam::Init();

	if (ret < 0)
		return ret;
	return 0;
}

int32_t ImageButtonStyleParam::Create(int32_t mode)
{
	int32_t ret = ButtonStyleParam::Create(mode);

	if (ret < 0)
		return ret;
	if (m_serial != NULL)
		m_serial->AddParam("layout_type_x", &m_layout_type_x, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("layout_type_y", &m_layout_type_y, ResourceParam::SEMANTIC_INT);
	if (m_serial != NULL)
		m_serial->AddParam("image_margin", &m_image_margin, ResourceParam::SEMANTIC_FLOAT);
	return 0;
}

int32_t ImageButtonStyleParam::SetEach(Widget *widget)
{
	/* without an ID of its own, an image is not drawn */
	if (m_image_obj.m_id.m_idhash == 0)
		m_image_obj.m_color.set_w(0.0f);
	if (m_image_back_obj.m_id.m_idhash == 0)
		m_image_back_obj.m_color.set_w(0.0f);

	int32_t ret = ButtonStyleParam::SetEach(widget);

	if (ret < 0)
		return ret;
	if (widget != NULL) {
		ImageButton *button = (ImageButton *)widget;

		button->m_layout_type_x = m_layout_type_x;
		button->m_layout_type_y = m_layout_type_y;
		button->m_image_margin = m_image_margin;
		if (m_image_back_obj.m_plugin != NULL)
			widget->SetTexture(m_image_back_obj.m_plugin->GetTexture(
						   m_image_back_obj.m_texture[0]),
					   ImageButton::OBJ_IMAGE_BACK, 0);
		if (m_image_obj.m_plugin != NULL)
			widget->SetTexture(m_image_obj.m_plugin->GetTexture(m_image_obj.m_texture[0]),
					   ImageButton::OBJ_IMAGE, 0);
	}
	return 0;
}

int32_t ImageButtonStyleParam::GetEach(Widget *widget)
{
	int32_t ret = ButtonStyleParam::GetEach(widget);

	if (ret < 0)
		return ret;
	if (widget != NULL) {
		ImageButton *button = (ImageButton *)widget;

		m_layout_type_x = button->m_layout_type_x;
		m_layout_type_y = button->m_layout_type_y;
		m_image_margin = button->m_image_margin;
	}
	return 0;
}

const char ImageButtonHandlerParam::m_param_name[] = "handler_image_button";

const char *ImageButtonHandlerParam::GetParamName() const
{
	return m_param_name;
}

const char *ImageButtonHandlerParam::ParamName()
{
	return m_param_name;
}

}
}

static Widget *NewImageButton(Widget *parent)
{
	return new ImageButton(parent, NULL);
}

static WidgetParam *NewImageButtonParam()
{
	return new ImageButtonParam;
}

static VersatileParam *NewImageButtonLayoutParam()
{
	return new WidgetLayoutParam;
}

static VersatileParam *NewImageButtonFocusParam()
{
	return new WidgetFocusParam;
}

static VersatileParam *NewImageButtonStyleParam()
{
	return new ImageButtonStyleParam;
}

static VersatileParam *NewImageButtonHandlerParam()
{
	return new ImageButtonHandlerParam;
}

static VersatileParam *NewImageButtonSoundParam()
{
	return new ButtonBaseSoundParam;
}

FactoryFuncTable g_factoryImageButton = {
	ImageButton::TypeName(),
	NewImageButton,
	NewImageButtonParam,
	NewImageButtonLayoutParam,
	NewImageButtonFocusParam,
	NewImageButtonStyleParam,
	NewImageButtonHandlerParam,
	NewImageButtonSoundParam,
};
