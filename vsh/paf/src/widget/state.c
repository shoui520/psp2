/* Standalone native widget state constructors and field operations. */
#include <stdint.h>
#include <arm_neon.h>
#include "../std/new.h"
#include "../std/string.h"

/* the screen size of each resolution, qHD, HD and full HD (EnvironmentParam's rodata; declared in
   widget/core/environment.h; until its file is C) */
const uint32_t g_screen_width_qhd = 960;
const uint32_t g_screen_height_qhd = 544;
const uint32_t g_screen_width_hd = 1280;
const uint32_t g_screen_height_hd = 725;
const uint32_t g_screen_width_full_hd = 1920;
const uint32_t g_screen_height_full_hd = 1088;

int ScePafWidget_9A7BE3B0(uint8_t *self, uint32_t value, uint8_t flag)
{
	self[205] = flag;
	*(uint32_t *)(self + 184) = value;
	return 0;
}

uint8_t *ScePafWidget_1F9DB221(uint8_t *self)
{
	if (self[640])
		self[640] = 0;
	return self;
}

int ScePafWidget_E8F862FC(void *self, const void *value)
{
	return value ? 0 : -1;
}


/* paf::graph::s_wait_vblank_count (surface_display.cpp) */
extern uint32_t _ZN3paf5graph19s_wait_vblank_countE;

uint32_t *ScePafWidget_10BEC7CF(uint32_t *self)
{
	++_ZN3paf5graph19s_wait_vblank_countE;
	++*self;
	return self;
}

uint32_t *ScePafWidget_DB993F51(uint32_t *self)
{
	uint32_t count = *self;

	if (count) {
		if (_ZN3paf5graph19s_wait_vblank_countE) {
			--_ZN3paf5graph19s_wait_vblank_countE;
			count = *self;
		}
		*self = count - 1;
	}
	return self;
}

uint32_t ScePafWidget_3546C104(const uint8_t *self, uint8_t filter)
{
	uint32_t flags = *(const uint32_t *)(self + 852);

	if (filter)
		return flags & ~*(const uint32_t *)(self + 864);
	return flags;
}


/* Resource readiness is exposed through several widget compatibility aliases. */
extern int _ZN3paf2ui11WidgetParam3GetEv(const void *resource);

static inline __attribute__((always_inline))
int WidgetResourceReady(const void *resource)
{
	int result = _ZN3paf2ui11WidgetParam3GetEv(resource);

	if (result >= 0)
		return 0;
	return result;
}




int ScePafWidget_A4E53390(uint8_t *self)
{
	uint8_t *vtable = *(uint8_t **)self;

	self[992] = 1;
	return ((int (*)(uint8_t *))*(void **)(vtable + 560))(self);
}

/* Text's is C++ (w_text.cpp) */
int _ZNK3paf2ui4Text9IsInheritEPKc(const void *self, const char *tag);

/* Type tags are compared by pointer identity in the native widget hierarchy. */
/* Box's, in w_box.cpp; Dialog's calls it */
int _ZNK3paf2ui3Box9IsInheritEPKc(const void *self, const char *type);

#define DEFINE_WIDGET_TYPE(name, tag, parent) \
__attribute__((noinline)) int name(const void *self, const char *type) \
{ \
	if (type == tag) \
		return 1; \
	return parent(self, type); \
}

/* ButtonBase's is C++ (w_button_base.cpp) */
int _ZNK3paf2ui10ButtonBase9IsInheritEPKc(const void *self, const char *type);
/* StateButtonBase's is C++ (w_state_button_base.cpp) */
int _ZNK3paf2ui15StateButtonBase9IsInheritEPKc(const void *self, const char *type);
/* ScrollBar's is C++ (w_scrollbar.cpp) */
int _ZNK3paf2ui9ScrollBar9IsInheritEPKc(const void *self, const char *type);
/* SpinBase's is C++ (w_spin_base.cpp) */
int _ZNK3paf2ui8SpinBase9IsInheritEPKc(const void *self, const char *type);
/* ScrollViewBase's is C++ (w_scroll_view_base.cpp) */
int _ZNK3paf2ui14ScrollViewBase9IsInheritEPKc(const void *self, const char *type);
/* ListView's is C++ (w_list_view.cpp) */
int _ZNK3paf2ui8ListView9IsInheritEPKc(const void *self, const char *type);
/* ProgressBar's is C++ (w_progress_bar.cpp) */
int _ZNK3paf2ui11ProgressBar9IsInheritEPKc(const void *self, const char *type);
/* Button's is C++ (w_button.cpp) */
int _ZNK3paf2ui6Button9IsInheritEPKc(const void *self, const char *type);
/* CompositeButton's is C++ (w_composite_button.cpp) */
int _ZNK3paf2ui15CompositeButton9IsInheritEPKc(const void *self, const char *type);
/* CheckBox's is C++ (w_check_box.cpp) */
int _ZNK3paf2ui8CheckBox9IsInheritEPKc(const void *self, const char *type);
/* ImageButton's is C++ (w_image_button.cpp) */
int _ZNK3paf2ui11ImageButton9IsInheritEPKc(const void *self, const char *type);
/* ProgressBarTouch's is C++ (w_progress_bar_touch.cpp) */
/* PlaneButton's is C++ (w_plane_button.cpp) */
/* RadioBox's is C++ (w_radio_box.cpp) */
/* SlidingDrawer's is C++ (w_sliding_drawer.cpp) */
/* Plane's is C++ (w_plane.cpp) */
int _ZNK3paf2ui5Plane9IsInheritEPKc(const void *self, const char *type);
/* PageTurn's is C++ (w_pageturn.cpp) */
/* RichText's is C++ (w_rich_text.cpp) */
/* ImageSlideBar's is C++ (w_image_slidebar.cpp) */
int _ZNK3paf2ui13ImageSlideBar9IsInheritEPKc(const void *self, const char *type);
/* SlideBar's is C++ (w_slidebar.cpp) */
/* CornerButton's is C++ (w_corner_button.cpp) */
/* Dialog's is C++ (w_dialog.cpp) */
/* TextBox's is C++ (w_text_box.cpp) */
/* CompositeDrawer's is C++ (w_composite_drawer.cpp) */
int _ZNK3paf2ui15CompositeDrawer9IsInheritEPKc(const void *self, const char *type);
/* Supplier's is C++ (w_supplier.cpp) */
/* Scene's is C++ (w_scene.cpp) */
/* RadioButton's is C++ (w_radio_button.cpp) */

#undef DEFINE_WIDGET_TYPE


/* Native single-dispatch widget methods. */
static inline __attribute__((always_inline))
int WidgetInvokeSlot(void *self, uint32_t offset)
{
	const uint8_t *vtable = *(const uint8_t *const *)self;
	return ((int (*)(void *))*(void *const *)(vtable + offset))(self);
}

int ScePafWidget_BDEF4D3F(void *self)
{
	return WidgetInvokeSlot(self, 552);
}

int ScePafWidget_6B4F7595(void *unused0, void *unused1,
			  void *unused2, void *unused3, void *target)
{
	if (target) {
		const uint8_t *vtable = *(const uint8_t *const *)target;
		return ((int (*)(void *))*(void *const *)(vtable + 108))(target);
	}
	return 0;
}

void *ScePafWidget_1D7D7860(uint8_t *self)
{
	void *target = *(void **)(self + 24);
	const uint8_t *vtable = *(const uint8_t *const *)target;

	((void (*)(void *))*(void *const *)(vtable + 108))(target);
	return self;
}


/* Test membership in the indexed widget selector table. */
__attribute__((noinline))
int ScePafWidget_9F450B64(const uint8_t *self, uint32_t value)
{
	const uint32_t *list = *(const uint32_t *const *)(self + 12);
	const uint32_t *cursor = *(const uint32_t *const *)list;
	const uint32_t *previous = cursor;
	const uint32_t *end = cursor + list[1];

	for (; cursor != end; previous = cursor) {
		if (*cursor == value)
			break;
		if (cursor)
			++cursor;
	}
	return previous != end;
}

int ScePafWidget_730C0464(const uint8_t *self, uint32_t index)
{
	if (ScePafWidget_9F450B64(self, index))
		return index;
	return 255;
}

int _ZN3paf2ui6Widget17SetDrawObjMaskBitEPKvi(void *self, int first, int second)
{
	const uint8_t *vtable = *(const uint8_t *const *)self;

	return ((int (*)(void *, int, int, int))*(void *const *)(vtable + 272))(
		self, first, second, 0);
}


int ScePafWidget_97020684(uint8_t *self)
{
	if (!*(uint32_t *)(self + 4))
		return 0;
	if (*(void **)(self + 8))
		return (*(int (**)(void))(self + 8))();
	return 0;
}

uint8_t *ScePafWidget_0DCB8EA5(uint8_t *self, float seconds)
{
	if (seconds < 0.0f)
		seconds = 200.0f;
	*(float *)(self + 976) = seconds;
	return self;
}

