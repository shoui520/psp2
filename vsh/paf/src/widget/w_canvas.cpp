/*
 * paf::ui::Canvas (DEBUGGER 0x810e5fd4..0x810e64b4): a plane that shows its painter's picture,
 * and the front touch's down, move and up events made into the painter's strokes, the touch
 * position taken from the plane's size to the picture's 1920x1088 half pixels. No record or
 * string names the file; the names of the static functions are ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <graphics/surface/surface.h>
#include <std/stdio.h>
#include <widget/core/environment.h>
#include <widget/core/event.h>
#include <widget/core/handler.h>
#include <widget/w_canvas.h>

namespace paf {
namespace ui {

static void OnTouchDown(int32_t type, Handler *self, Event *e, void *data);
static void OnTouchUp(int32_t type, Handler *self, Event *e, void *data);
static void OnTouchMove(int32_t type, Handler *self, Event *e, void *data);

const char Canvas::m_widget_type[] = "canvas";

bool Canvas::IsInherit(const char *type) const
{
	if (m_widget_type == type)
		return true;
	return Plane::IsInherit(type);
}

char *Canvas::GetTypeName() const
{
	return (char *)m_widget_type;
}

Canvas::Canvas(Widget *parent, CreateParam *param) : Plane(parent, param)
{
	int32_t width;
	int32_t height;
	int32_t pitch;
	SceGxmTexture *texture;

	m_painter = new CanvasPainter();
	m_painter->GetTexture(&texture, &width, &height, &pitch);
	m_surface = intrusive_ptr<graph::Surface>(new graph::Surface(texture, width, height));
	SetTexture(m_surface, 0, 0);
	m_painter->SetPressureSensitive(true);
	AddEventListener(CB_TOUCH_DOWN, new EventCBListener(OnTouchDown, this), false);
	AddEventListener(CB_TOUCH_UP, new EventCBListener(OnTouchUp, this), false);
	AddEventListener(CB_TOUCH_MOVE, new EventCBListener(OnTouchMove, this), false);
	if (!(m_event_enable & 0x400))
		EnableFocusEvent(true);
	m_event_enable |= 0xFFF0;
}

}
}

using namespace paf;
using namespace paf::ui;

math::v4 ScePafWidget_ED270613(Canvas *canvas)
{
	float r;
	float g;
	float b;
	float a;

	canvas->m_painter->GetColor(&r, &g, &b, &a);
	return math::v4(r, g, b, a);
}

void ScePafWidget_DBB1C54C(Canvas *canvas, const math::v4 &color)
{
	canvas->m_painter->SetColor(color.extract_x(), color.extract_y(), color.extract_z(),
				    color.extract_w());
}

float ScePafWidget_5D806B1E(Canvas *canvas)
{
	return canvas->m_painter->GetSize();
}

void ScePafWidget_D844AC3F(Canvas *canvas, float size)
{
	canvas->m_painter->SetSize(size);
}

void ScePafWidget_FD3E7E8A(Canvas *canvas)
{
	canvas->m_painter->Empty1();
}

void ScePafWidget_710ECEC2(Canvas *canvas)
{
	canvas->m_painter->Empty2();
}

namespace paf {
namespace ui {

Canvas::~Canvas()
{
	sce_paf_printf_dummy();
	if (m_painter != NULL)
		delete m_painter;
	m_painter = NULL;
}

/* the touch's position, its state taken, and the pressure the strokes are drawn with */
__attribute__((noinline)) static void GetTouch(Canvas *canvas, Event *e, float *x, float *y, float *pressure)
{
	Environment *env = canvas->m_env;
	uint32_t id = e->m_detail & 0xff;
	HitInfo *hit = NULL;

	if (id < 8) {
		env->m_touch_state[id].m_state = 0;
		env->m_touch_state[id].m_id = id;
		hit = &env->m_touch_hit[id];
	}
	*x = hit->m_touch_pos.extract_x();
	*y = hit->m_touch_pos.extract_y();
	*pressure = 0.5f;
}

/* the position from the plane's centre, y up, to the picture's half pixels from its top left */
__attribute__((noinline)) static void ToPicture(Canvas *canvas, float *x, float *y)
{
	math::v4 size = *canvas->GetSize(0);

	*x = (*x / size.extract_x() + 0.5f) * 1920.0f;
	*y = (-*y / size.extract_y() + 0.5f) * 1088.0f;
}

static void OnTouchDown(int32_t type, Handler *self, Event *e, void *data)
{
	Canvas *canvas = (Canvas *)data;
	float x;
	float y;
	float pressure;

	GetTouch(canvas, e, &x, &y, &pressure);
	ToPicture(canvas, &x, &y);
	canvas->m_painter->BeginStroke(x, y, pressure);
}

static void OnTouchMove(int32_t type, Handler *self, Event *e, void *data)
{
	Canvas *canvas = (Canvas *)data;
	float x;
	float y;
	float pressure;

	GetTouch(canvas, e, &x, &y, &pressure);
	ToPicture(canvas, &x, &y);
	canvas->m_painter->ContinueStroke(x, y, pressure);
}

static void OnTouchUp(int32_t type, Handler *self, Event *e, void *data)
{
	Canvas *canvas = (Canvas *)data;
	float x;
	float y;
	float pressure;

	GetTouch(canvas, e, &x, &y, &pressure);
	ToPicture(canvas, &x, &y);
	canvas->m_painter->EndStroke(x, y);
}

}
}
