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

/* Shared PAF-widget registration and state slots. */
uint32_t sce_paf_widget_state_199c;
uint32_t sce_paf_widget_state_19a8;
uint32_t sce_paf_widget_state_19ac;
uint32_t sce_paf_widget_state_19b0;
uint32_t sce_paf_widget_state_19b4;
uint32_t sce_paf_widget_state_19b8;
uint32_t sce_paf_widget_state_19bc;
uint32_t sce_paf_widget_state_19c0;
uint32_t sce_paf_widget_state_1ce8;
uint32_t sce_paf_widget_state_142d4;
uint32_t sce_paf_widget_state_142d8;

uint32_t ScePafWidget_1EE9D37E(void)
{
	return sce_paf_widget_state_19a8;
}

uint32_t _ZN3paf2ui5wutil15GetEditListenerEv(void)
{
	return sce_paf_widget_state_19ac;
}

uint32_t _ZN3paf2ui5wutil18GetWebEditListenerEv(void)
{
	return sce_paf_widget_state_19b0;
}

uint32_t _ZN3paf2ui5wutil24GetMessageDialogListenerEv(void)
{
	return sce_paf_widget_state_19b4;
}

uint32_t ScePafWidget_A55215E5(void)
{
	return sce_paf_widget_state_19b8;
}

uint32_t _ZN3paf2ui5wutil18GetWebViewListenerEv(void)
{
	return sce_paf_widget_state_19bc;
}

uint32_t ScePafWidget_E638AA1F(void)
{
	return sce_paf_widget_state_19c0;
}

uint32_t _ZN3paf2ui6Widget19GetBaseDestroyCountEv(void)
{
	return sce_paf_widget_state_1ce8;
}

uint32_t _ZN3paf2ui6Widget19SetBaseDestroyCountEi(uint32_t value)
{
	sce_paf_widget_state_1ce8 = value;
	return value;
}

uint32_t _ZN3paf2ui5wutil6SetEnvEPNS0_11EnvironmentE(uint32_t value)
{
	sce_paf_widget_state_199c = value;
	return value;
}

uint32_t *_ZN3paf2ui8Layouter6FinishEv(void)
{
	sce_paf_widget_state_142d4 = 0;
	return &sce_paf_widget_state_142d4;
}

uint32_t *_ZN3paf2ui8Layouter5EnterEv(void)
{
	sce_paf_widget_state_142d4 = 1;
	return &sce_paf_widget_state_142d4;
}

int _ZN3paf2ui8Layouter7IsEnterEv(void)
{
	return sce_paf_widget_state_142d4 != 0;
}

int _ZN3paf2ui8Animator7IsEnterEv(void)
{
	return sce_paf_widget_state_142d8 != 0;
}

int _ZN3paf2ui4Text16GetSelectionModeERb(const uint8_t *self, uint8_t *output)
{
	*output = self[648] & 1;
	return 0;
}

int _ZNK3paf2ui6Widget9GetCameraEv(const uint8_t *self)
{
	const uint8_t *value = *(const uint8_t *const *)(self + 364);

	return value ? *(const int32_t *)(value + 640) : 0;
}

int _ZN3paf2ui7TextBox9IsEditingEv(const uint8_t *self)
{
	return (self[649] & 4) != 0;
}

int _ZN3paf2ui7TextBox25SetEditAutoTransitionModeEb(uint8_t *self, uint8_t value)
{
	if (self[772] != value)
		self[772] = value;
	return 0;
}


int _ZN3paf2ui11Environment15IsTermAvailableEv(const uint8_t *self)
{
	return !*(const uint32_t *)(self + 44) &&
		!*(const uint32_t *)(self + 56) && self[213] != 0;
}

int ScePafWidget_222D55C7(const uint8_t *self, uint8_t *output)
{
	*output = (self[648] & 2) != 0;
	return 0;
}

int _ZN3paf2ui7TextBox11GetEditModeERb(const uint8_t *self, uint8_t *output)
{
	*output = (self[648] & 8) != 0;
	return 0;
}

int ScePafWidget_E0D414F7(const uint8_t *self, uint8_t *output)
{
	*output = (self[648] & 4) != 0;
	return 0;
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

uint32_t _ZN3paf2ui4Text10CheckStateEv(const uint8_t *self)
{
	uint32_t result = *(const uint32_t *)(self + 336);
	const uint8_t *state = *(const uint8_t *const *)(self + 640);

	if (state && !(state[52] & 16))
		return 0;
	return result;
}




uint32_t *ScePafWidget_5245A0F2(uint32_t *result, const uint32_t *source)
{
	uint32_t first = source[0];
	uint32_t second = source[1];

	result[0] = source[0];
	uint32_t third = source[2];
	result[1] = second;
	result[2] = third;
	result[10] = first;
	result[11] = second;
	result[12] = first;
	result[13] = second;
	result[14] = 1062045612u;
	return result;
}

static inline __attribute__((always_inline))
uint32_t *ResetWidgetTransform(uint32_t *result)
{
	result[5] = 0;
	result[6] = 0;
	result[7] = 0;
	result[8] = 0;
	result[9] = 0;
	result[10] = 0;
	result[11] = 0;
	result[12] = 0;
	result[13] = 0;
	result[14] = 0;
	result[15] = 0;
	result[16] = 0;
	result[17] = 0;
	result[18] = 0;
	result[19] = 0;
	result[20] = 0;
	result[22] = 0;
	result[21] = 0;
	return result;
}

uint32_t *_ZN3paf2ui5wutil9InitParamC2Ev(uint32_t *result)
{
	return ResetWidgetTransform(result);
}

uint32_t *_ZN3paf2ui5wutil9InitParamC1Ev(uint32_t *result)
{
	return ResetWidgetTransform(result);
}

int _ZN3paf2ui7TextBox11SetImeParamERKNS1_8ImeParamE(uint8_t *self, const uint8_t *source)
{
	uint8_t value;

	*(uint32_t *)(self + 744) = *(const uint32_t *)(source + 4);
	*(uint32_t *)(self + 752) = *(const uint32_t *)(source + 12);
	value = source[8];
	*(uint32_t *)(self + 756) = *(const uint32_t *)(source + 16);
	self[748] = value;
	uint32_t trailing = *(const uint32_t *)(source + 24);
	*(uint32_t *)(self + 760) = *(const uint32_t *)(source + 20);
	*(uint32_t *)(self + 764) = trailing;
	return 0;
}

int _ZN3paf2ui7TextBox11GetImeParamERNS1_8ImeParamE(const uint8_t *self, uint8_t *result)
{
	uint8_t value;
	uint32_t tail, extra;

	*(uint32_t *)(result + 4) = *(const uint32_t *)(self + 744);
	*(uint32_t *)(result + 12) = *(const uint32_t *)(self + 752);
	value = self[748];
	*(uint32_t *)(result + 16) = *(const uint32_t *)(self + 756);
	result[8] = value;
	tail = *(const uint32_t *)(self + 760);
	extra = *(const uint32_t *)(self + 764);
	*(uint32_t *)(result + 20) = tail;
	*(uint32_t *)(result + 24) = extra;
	return 0;
}

uint8_t *_ZN3paf2ui6Widget9FindChildEPS1_i(uint8_t *self, uint8_t *value, int32_t mode)
{
	uint8_t *node;

	if (!value || *(int32_t *)(value + 36) > 0)
		return 0;
	if (mode == 2) {
		if (*(uint8_t **)(value + 360) != self)
			return 0;
	} else {
		node = value;
		while (node != self) {
			node = *(uint8_t **)(node + 360);
			if (!node)
				return 0;
		}
	}
	return value;
}

uint8_t *_ZN3paf2ui6Widget19NotifyAnimatorDirtyEj(uint8_t *self, uint16_t mask)
{
	uint16_t flags = *(uint16_t *)(self + 574);

	if ((flags & (uint8_t)mask) && (flags & (mask & 0xFF00U))) {
		self = *(uint8_t **)(self + 536);
		if (self)
			*(uint16_t *)(self + 12) |= 1;
	}
	return self;
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

int _ZN3paf2ui18SlidingDrawerParam3GetEv(const void *resource)
{
	return WidgetResourceReady(resource);
}

void _ZN3paf2ui19TextBoxEditListenerD0Ev(uint32_t *self)
{
	*self = (uint32_t)-2127593716;
	PafDelete(self);
}

int _ZN3paf2ui7Handler17HandleSystemEventEPNS0_5EventE(void *self, uint32_t value)
{
	void *const *methods = *(void *const *const *)self;

	((void (*)(void *, uint32_t, uint32_t))methods[24])(
		self, 0x800000, value);
	return 0;
}

int _ZN3paf2ui7Handler16HandleTimerEventEPNS0_5EventE(void *self, const uint8_t *argument)
{
	void *const *methods = *(void *const *const *)self;

	((void (*)(void *, uint32_t))methods[24])(
		self, *(const uint32_t *)(argument + 16));
	return 0;
}

extern uint8_t *_ZN3paf5graph7DrawObj11EnterSafetyEv(void);

uint8_t *_ZN3paf2ui8Animator5EnterEv(void)
{
	sce_paf_widget_state_142d8 = 1;
	return _ZN3paf5graph7DrawObj11EnterSafetyEv();
}

int _ZN3paf2ui7Handler18HandleCommandEventEPNS0_5EventE(void *self, uint32_t value)
{
	void *const *methods = *(void *const *const *)self;

	((void (*)(void *, uint32_t, uint32_t))methods[24])(
		self, 1048585, value);
	return 0;
}

int _ZN3paf2ui7Handler16HandleInputEventEPNS0_5EventE(void *self, uint32_t value)
{
	void *const *methods = *(void *const *const *)self;

	((void (*)(void *, uint32_t, uint32_t))methods[24])(
		self, 65555, value);
	return 0;
}

void *ScePafWidget_40F2A311(uint32_t *self)
{
	if (self[118] == 0) {
		void *const *methods = *(void *const *const *)self;
		return ((void *(*)(void *))methods[104])(self);
	}
	return self;
}


extern int _ZN3paf5graph7TextObj12GetLineCountERj(const uint8_t *self, uint32_t *output);
extern int _ZN3paf5graph7TextObj11GetCharInfoEjRNS0_18TextLayoutCharInfoE(const uint8_t *self, uint32_t index,
					   uint8_t *output);
extern int _ZN3paf5graph7TextObj15GetNextRightHitEjRj(const uint8_t *self, uint32_t index,
					   uint32_t *output);
extern int _ZN3paf5graph7TextObj17GetSelectionRangeERjS2_(const uint8_t *self, uint32_t *first,
					   uint32_t *second);

int _ZN3paf2ui4Text12GetLineCountERj(const uint8_t *self, uint32_t *output)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return _ZN3paf5graph7TextObj12GetLineCountERj(graphics, output);
	return -2136014814;
}

int _ZN3paf2ui4Text11GetCharInfoEjRNS_5graph18TextLayoutCharInfoE(const uint8_t *self, uint32_t index,
			   uint8_t *output)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return _ZN3paf5graph7TextObj11GetCharInfoEjRNS0_18TextLayoutCharInfoE(graphics, index, output);
	return -2136014814;
}

int _ZN3paf2ui4Text15GetNextRightHitEjRj(const uint8_t *self, uint32_t index,
			   uint32_t *output)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return _ZN3paf5graph7TextObj15GetNextRightHitEjRj(graphics, index, output);
	return -2136014814;
}

int _ZN3paf2ui4Text17GetSelectionRangeERjS2_(const uint8_t *self, uint32_t *first,
			   uint32_t *second)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return _ZN3paf5graph7TextObj17GetSelectionRangeERjS2_(graphics, first, second);
	return -2136014814;
}

extern int _ZN3paf5graph7TextObj18GetLayoutAttributeENS0_19TextLayoutAttributeERNS_4math2v2E(const uint8_t *self, int index,
					   uint64_t *output);
extern int _ZN3paf5graph7TextObj9GetBoundsERNS_4math2v2E(const uint8_t *self, uint64_t *output);
extern int _ZN3paf5graph7TextObj18GetLayoutAttributeENS0_19TextLayoutAttributeERi(const uint8_t *self, int index,
					   uint32_t *output);
extern uint8_t *_ZN3paf5graph7DrawObj12FinishSafetyEv(void);

int _ZN3paf2ui4Text18GetLayoutAttributeENS_5graph19TextLayoutAttributeERNS_4math2v2E(const uint8_t *self, int index, uint64_t *output)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return _ZN3paf5graph7TextObj18GetLayoutAttributeENS0_19TextLayoutAttributeERNS_4math2v2E(graphics, index, output);
	return -2136014814;
}

int _ZN3paf2ui4Text9GetBoundsERNS_4math2v2E(const uint8_t *self, uint64_t *output)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return _ZN3paf5graph7TextObj9GetBoundsERNS_4math2v2E(graphics, output);
	return -2136014814;
}

int _ZN3paf2ui4Text18GetLayoutAttributeENS_5graph19TextLayoutAttributeERi(const uint8_t *self, int index,
			   uint32_t *output)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return _ZN3paf5graph7TextObj18GetLayoutAttributeENS0_19TextLayoutAttributeERi(graphics, index, output);
	return -2136014814;
}

uint32_t *_ZN3paf2ui8Animator6FinishEv(void)
{
	_ZN3paf5graph7DrawObj12FinishSafetyEv();
	sce_paf_widget_state_142d8 = 0;
	return &sce_paf_widget_state_142d8;
}

int _ZN3paf2ui11Environment25RegisterTouchDeviceRegionEjPNS0_6WidgetE(uint8_t *self, unsigned int index, int value)
{
	if (index >= 6)
		return -2135998193;
	*(uint32_t *)(self + 11592) |= 1U << index;
	*(int *)(self + 11600 + 4 * index) = value;
	return 0;
}

uint8_t *_ZN3paf2ui6Widget14SetGestureMaskEjb(uint8_t *self, uint32_t value,
				uint8_t toggle)
{
	if (*(uint32_t *)(self + 408) != value) {
		*(uint32_t *)(self + 408) = value;
		if (toggle) {
			if (value) {
				if (!self[500])
					self[500] = 1;
			} else if (self[500]) {
				self[500] = 0;
			}
		}
	}
	return self;
}

__attribute__((noinline)) int _ZNK3paf2ui6Widget9IsInheritEPKc(const void *self, const char *tag)
{
	return tag == "widget_base";
}

int _ZN3paf2ui13SlidingDrawer19SetMovableDirectionEj(uint32_t *self, uint32_t value)
{
	uint8_t *vtable = (uint8_t *)self[0];

	self[255] = value;
	return ((int (*)(uint32_t *))*(void **)(vtable + 572))(self);
}

int ScePafWidget_A4E53390(uint8_t *self)
{
	uint8_t *vtable = *(uint8_t **)self;

	self[992] = 1;
	return ((int (*)(uint8_t *))*(void **)(vtable + 560))(self);
}

__attribute__((noinline)) int _ZNK3paf2ui4Text9IsInheritEPKc(const void *self, const char *tag)
{
	if (tag == "text")
		return 1;
	return _ZNK3paf2ui6Widget9IsInheritEPKc(self, tag);
}

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
DEFINE_WIDGET_TYPE(_ZNK3paf2ui13SlidingDrawer9IsInheritEPKc, "sliding_drawer",
		   _ZNK3paf2ui14ScrollViewBase9IsInheritEPKc)
/* Plane's is C++ (w_plane.cpp) */
int _ZNK3paf2ui5Plane9IsInheritEPKc(const void *self, const char *type);
/* PageTurn's is C++ (w_pageturn.cpp) */
/* RichText's is C++ (w_rich_text.cpp) */
/* ImageSlideBar's is C++ (w_image_slidebar.cpp) */
int _ZNK3paf2ui13ImageSlideBar9IsInheritEPKc(const void *self, const char *type);
/* SlideBar's is C++ (w_slidebar.cpp) */
/* CornerButton's is C++ (w_corner_button.cpp) */
/* Dialog's is C++ (w_dialog.cpp) */
DEFINE_WIDGET_TYPE(_ZNK3paf2ui7TextBox9IsInheritEPKc, "text_box",
		   _ZNK3paf2ui4Text9IsInheritEPKc)
/* CompositeDrawer's is C++ (w_composite_drawer.cpp) */
int _ZNK3paf2ui15CompositeDrawer9IsInheritEPKc(const void *self, const char *type);
DEFINE_WIDGET_TYPE(_ZNK3paf2ui8Supplier9IsInheritEPKc, "supplier",
		   _ZNK3paf2ui5Plane9IsInheritEPKc)
DEFINE_WIDGET_TYPE(_ZNK3paf2ui5Scene9IsInheritEPKc, "page",
		   _ZNK3paf2ui6Widget9IsInheritEPKc)
/* RadioButton's is C++ (w_radio_button.cpp) */

#undef DEFINE_WIDGET_TYPE

/* The native delete callback marks the dispatch scope in a shared word. */
uint32_t sce_paf_widget_dispatch_guard_142d4;

int _ZN3paf2ui8Layouter8OnUpdateEv(uint32_t *self)
{
	uint8_t *vtable = (uint8_t *)self[0];

	sce_paf_widget_dispatch_guard_142d4 = 1;
	((void (*)(uint32_t *))*(void **)(vtable + 8))(self);
	sce_paf_widget_dispatch_guard_142d4 = 0;
	return 0;
}

int ScePafWidget_4608F7EF(const uint8_t *self, uint64_t *output)
{
	const uint8_t *item = *(const uint8_t *const *)(self + 712);

	if (!item)
		return -2136014814;
	*output = *(const uint64_t *)(item + 48);
	return 0;
}

static inline __attribute__((always_inline))
uint32_t *InitializeWidgetStatusRecord(uint32_t *self)
{
#if defined(FW_TYPE_CEX)
	((volatile uint32_t *)self)[0] = 0x812f05e0U;
#else
	((volatile uint32_t *)self)[0] = 0x812f1298U;
#endif
	((volatile uint32_t *)self)[1] = 1114112U;
	((volatile uint32_t *)self)[8] = 0;
	((volatile uint32_t *)self)[2] = 0;
	return self;
}

uint32_t *_ZN3paf2ui5EventC1Ev(uint32_t *self)
{
	return InitializeWidgetStatusRecord(self);
}

uint32_t *_ZN3paf2ui5EventC2Ev(uint32_t *self)
{
	return InitializeWidgetStatusRecord(self);
}

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

int _ZN3paf2ui7Handler14HandleKeyEventEPNS0_5EventE(void *self)
{
	(void)((void (*)(void *))*(void *const *)
		(*(const uint8_t *const *)self + 20))(self);
	return 0;
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

uint32_t *_ZN3paf2ui19TextBoxEditListenerD2Ev(uint32_t *self)
{
	*self = (uint32_t)-2127593716;
	return self;
}

uint32_t *_ZN3paf2ui19TextBoxEditListenerC1Ev(uint32_t *self)
{
	*self = (uint32_t)-2127593716;
	return self;
}

uint32_t *_ZN3paf2ui19TextBoxEditListenerD1Ev(uint32_t *self)
{
	*self = (uint32_t)-2127593716;
	return self;
}

uint32_t *_ZN3paf2ui19TextBoxEditListenerC2Ev(uint32_t *self)
{
	*self = (uint32_t)-2127593716;
	return self;
}

int _ZN3paf2ui12AnimListener12PropColorSetEPS1_NS_4math2v4E(const uint8_t *self)
{
	void *target = *(void *const *)(self + 12);
	const uint8_t *vtable = *(const uint8_t *const *)target;

	return ((int (*)(void *))*(void *const *)(vtable + 152))(target);
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

int ScePafWidget_3E4AABD3(uint8_t *self, void *replacement)
{
	void *previous = *(void **)(self + 716);

	if (previous != replacement) {
		if (previous) {
			const uint8_t *methods = *(const uint8_t *const *)previous;

			((void (*)(void *, void *))*(void *const *)(methods + 12))(
				previous, self);
		}
		*(void **)(self + 716) = replacement;
	}
	return 0;
}

int _ZN3paf2ui17EventListenerList4FindEPNS0_13EventListenerE(const uint8_t *self, int32_t requested)
{
	const uint8_t *root = *(const uint8_t *const *)(self + 4);
	const uint8_t *node = root ? *(const uint8_t *const *)(root + 4) : NULL;

	while (node != root) {
		if (*(const int32_t *)(node + 8) == requested)
			return requested;
		if (node)
			node = *(const uint8_t *const *)(node + 4);
	}
	return 0;
}

void *ScePafWidget_55D83675(uint8_t *self, uint32_t *replacement)
{
	uint32_t *previous = *(uint32_t **)(self + 528);
	const uint8_t *vtable;

	if (previous == replacement)
		return previous;
	if (previous && --*previous == 0)
		PafDelete(previous);
	*(uint32_t **)(self + 528) = replacement;
	++*replacement;
	vtable = *(const uint8_t *const *)self;
	return ((void *(*)(void *))*(void *const *)(vtable + 160))(self);
}


int ScePafWidget_97020684(uint8_t *self)
{
	if (!*(uint32_t *)(self + 4))
		return 0;
	if (*(void **)(self + 8))
		return (*(int (**)(void))(self + 8))();
	return 0;
}

uint32x4_t *_ZN3paf2ui15AnimListenerObj13PropObjRotGetEPNS0_12AnimListenerE(uint32x4_t *result,
					  const uint8_t *self)
{
	const uint8_t *value = *(const uint8_t *const *)(self + 80);
	const uint32x4_t *source = *(const uint32x4_t *const *)(value + 40);

	*result = source ? *source : vdupq_n_u32(0);
	return result;
}

uint8_t *ScePafWidget_0DCB8EA5(uint8_t *self, float seconds)
{
	if (seconds < 0.0f)
		seconds = 200.0f;
	*(float *)(self + 976) = seconds;
	return self;
}

int ScePafWidget_B9F2249C(uint8_t *self, const uint64_t *value)
{
	uint8_t *target = *(uint8_t **)(self + 712);

	if (!target)
		return -2136014814;
	*(uint64_t *)(target + 40) = *value;
	return 0;
}

uint8_t *_ZN3paf2ui6Widget15InitCreateParamEPKNS1_11CreateParamE(uint8_t *self, const uint8_t *flags)
{
	if (flags) {
		uint8_t value = (self[404] & 0xfe) | ((flags[0] & 2) != 0);

		self[404] = value;
		self[404] = (value & 0xfd) | (2 * ((flags[0] & 4) != 0));
	}
	return self;
}

uint8_t *_ZN3paf2ui6Widget17NotifyLayoutDirtyEj(uint8_t *self, uint16_t flags)
{
	uint8_t *result = self;

	if (((uint8_t)flags & *(uint16_t *)(self + 572)) &&
	    (*(uint16_t *)(self + 572) & flags & 0xff00)) {
		uint8_t *parent = *(uint8_t **)(self + 360);

		if (parent) {
			uint8_t *parentObject = *(uint8_t **)(parent + 532);

			if (parentObject)
				parentObject[12] |= 1;
		}
		if (flags & 2) {
			result = *(uint8_t **)(self + 532);
			if (result)
				result[12] |= 1;
		}
	}
	return result;
}

uint32_t *ScePafWidget_D0D78958(uint32_t *self, uint32_t *first,
				uint32_t *second, uint32_t *third,
				uint32_t *fourth)
{
	uint32_t *value = (uint32_t *)self[11];
	uint32_t additional;

	if (value) {
		*second = value[0];
		additional = value[1];
	} else {
		*second = 0;
		additional = 0;
	}
	*fourth = additional;
	*first = self[9];
	*third = self[10];
	return self;
}

