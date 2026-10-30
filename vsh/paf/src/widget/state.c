/* Standalone native widget state constructors and field operations. */
#include <stdint.h>
#include <arm_neon.h>
#include "../std/new.h"

int32x4_t *ScePafWidget_73875C21(int32x4_t *result)
{
	*result = vdupq_n_s32(0x42800000);
	return result;
}

uint32_t sce_paf_widget_state_155c;

static inline __attribute__((always_inline))
uint32_t *InitializeWidgetRecord(uint32_t *self, uint32_t payload)
{
	self[4] = payload;
	self[0] = 0;
	self[1] = 0;
	self[2] = 0;
	return self;
}

uint32_t *ScePafWidget_348741A7(uint32_t *self, uint32_t payload)
{
	return InitializeWidgetRecord(self, payload);
}

uint32_t *ScePafWidget_2C5E7C87(uint32_t *self, uint32_t payload)
{
	return InitializeWidgetRecord(self, payload);
}

uint32_t *ScePafWidget_E0F8DBA7(uint32_t *self, uint32_t payload)
{
	return InitializeWidgetRecord(self, payload);
}

uint32_t *ScePafWidget_E2F44822(uint32_t *self, uint32_t payload)
{
	return InitializeWidgetRecord(self, payload);
}

uint32_t *ScePafWidget_E983D809(uint32_t *self, uint32_t payload)
{
	return InitializeWidgetRecord(self, payload);
}

uint32_t *ScePafWidget_647784E1(uint32_t *self)
{
	self[0] = UINT32_MAX;
	self[3] = 0;
	return self;
}

uint32_t *ScePafWidget_688E163E(uint32_t *self)
{
	self[0] = UINT32_MAX;
	self[3] = 0;
	return self;
}

uint8_t *ScePafWidget_434B1CB8(uint8_t *self)
{
	++*(uint32_t *)(self + 1056);
	return self;
}

uint8_t *ScePafWidget_4A3F4C93(uint8_t *self)
{
	++*(uint32_t *)(self + 924);
	return self;
}

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

uint32_t ScePafWidget_BFF4C5A2(void)
{
	return sce_paf_widget_state_19ac;
}

uint32_t ScePafWidget_4AAE618E(void)
{
	return sce_paf_widget_state_19b0;
}

uint32_t ScePafWidget_74F3EF0E(void)
{
	return sce_paf_widget_state_19b4;
}

uint32_t ScePafWidget_A55215E5(void)
{
	return sce_paf_widget_state_19b8;
}

uint32_t ScePafWidget_02E8CF46(void)
{
	return sce_paf_widget_state_19bc;
}

uint32_t ScePafWidget_E638AA1F(void)
{
	return sce_paf_widget_state_19c0;
}

uint32_t ScePafWidget_03EC4830(void)
{
	return sce_paf_widget_state_1ce8;
}

uint32_t ScePafWidget_66EEB261(uint32_t value)
{
	sce_paf_widget_state_1ce8 = value;
	return value;
}

uint32_t ScePafWidget_887EF5E5(uint32_t value)
{
	sce_paf_widget_state_199c = value;
	return value;
}

uint32_t *ScePafWidget_88BB79F0(void)
{
	sce_paf_widget_state_142d4 = 0;
	return &sce_paf_widget_state_142d4;
}

uint32_t *ScePafWidget_37FBE9CE(void)
{
	sce_paf_widget_state_142d4 = 1;
	return &sce_paf_widget_state_142d4;
}

int ScePafWidget_59CA8E05(void)
{
	return sce_paf_widget_state_142d4 != 0;
}

int ScePafWidget_295F20D1(void)
{
	return sce_paf_widget_state_142d8 != 0;
}

int ScePafWidget_01EF18C7(const uint8_t *self, uint8_t *output)
{
	*output = self[648] & 1;
	return 0;
}

int ScePafWidget_027F9135(void *self, uint32_t *attributes)
{
	attributes[1] = (attributes[1] & 0xF0FFFFFFU) | 0x01000000U;
	return 0;
}

int ScePafWidget_BF3DEACA(const uint8_t *self)
{
	const uint8_t *value = *(const uint8_t *const *)(self + 636);

	return value ? *(const int32_t *)(value + 4) : -1;
}

int ScePafWidget_CEFCBAA6(const uint8_t *self)
{
	const uint8_t *value = *(const uint8_t *const *)(self + 364);

	return value ? *(const int32_t *)(value + 640) : 0;
}

int ScePafWidget_7B4B305A(const uint8_t *self)
{
	return (self[649] & 4) != 0;
}

int ScePafWidget_CEA84D74(uint8_t *self, uint8_t value)
{
	if (self[772] != value)
		self[772] = value;
	return 0;
}

uint32_t ScePafWidget_81FE70D6(const uint8_t *self, int32_t index)
{
	if (index < 0 || index > 2)
		return 0;
	return ((const uint32_t *)*(const void *const *)(self + 636))[index + 1];
}

int ScePafWidget_02A5FA8E(const uint8_t *self, int32_t index)
{
	if (index < 0 || index > 2)
		return -2136014706;
	((uint32_t *)*(void *const *)(self + 636))[index + 1] = 0;
	return 0;
}

int ScePafWidget_28D9B1FE(uint8_t *self, int32_t index)
{
	if (index < 0 || index > 1)
		return -2136014706;
	*(uint32_t *)(self + 652) = index;
	return 0;
}

int ScePafWidget_E636C722(const uint8_t *self, uint32_t *result,
			      int32_t index)
{
	if (index < 0 || index > 2)
		return -2136014706;
	*result = ((const uint32_t *)(self + 672))[index];
	return 0;
}

int ScePafWidget_B86ADE3E(const uint8_t *self, uint32_t *result,
			      int32_t index)
{
	if (index < 0 || index > 2)
		return -2136014706;
	*result = ((const uint32_t *)(self + 684))[index];
	return 0;
}

int ScePafWidget_C14E8BB4(uint8_t *self, int32_t index)
{
	if (index < 0 || index > 2)
		return -2136014706;
	*(uint32_t *)(self + 756) = index;
	return 0;
}

int ScePafWidget_CEBDCDB3(const uint8_t *self, const uint8_t *other)
{
	return other &&
		*(const uint32_t *)(self + 636) ==
		(uint8_t)*(const uint32_t *)(other + 8);
}

int ScePafWidget_B4A711BB(const uint8_t *self)
{
	return !*(const uint32_t *)(self + 44) &&
		!*(const uint32_t *)(self + 56) && self[213] != 0;
}

int ScePafWidget_439A6529(uint8_t *self, uint32_t value)
{
	if (!value)
		return -2136014706;
	**(uint32_t **)(self + 636) = value;
	*(uint32_t *)(self + 724) |= 12;
	return 0;
}

uint32_t ScePafWidget_C003A59B(const uint8_t *self, int32_t index)
{
	if (index < 0 || index >= *(const int32_t *)(self + 1064))
		return 0;
	return *(const uint32_t *)(
		((const uint8_t *const *)*(const void *const *)(self + 1060))[index]
		+ 12);
}

int ScePafWidget_75420B80(const uint8_t *self, int32_t index)
{
	uint32_t type;

	if (index < 0 || index >= *(const int32_t *)(self + 1064))
		return 0;
	type = *(const uint32_t *)(
		((const uint8_t *const *)*(const void *const *)(self + 1060))[index]
		+ 4);
	return type == 3 ? 1 : type;
}

int ScePafWidget_222D55C7(const uint8_t *self, uint8_t *output)
{
	*output = (self[648] & 2) != 0;
	return 0;
}

int ScePafWidget_CF11ECB0(const uint8_t *self, uint8_t *output)
{
	*output = (self[648] & 8) != 0;
	return 0;
}

int ScePafWidget_E0D414F7(const uint8_t *self, uint8_t *output)
{
	*output = (self[648] & 4) != 0;
	return 0;
}

uint32_t sce_paf_widget_global_counter_112dc;

uint32_t *ScePafWidget_10BEC7CF(uint32_t *self)
{
	++sce_paf_widget_global_counter_112dc;
	++*self;
	return self;
}

uint32_t *ScePafWidget_DB993F51(uint32_t *self)
{
	uint32_t count = *self;

	if (count) {
		if (sce_paf_widget_global_counter_112dc) {
			--sce_paf_widget_global_counter_112dc;
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

uint32_t ScePafWidget_7DA29D35(const uint8_t *self)
{
	uint32_t result = *(const uint32_t *)(self + 336);
	const uint8_t *state = *(const uint8_t *const *)(self + 640);

	if (state && !(state[52] & 16))
		return 0;
	return result;
}

void *ScePafWidget_FFD97FCD(uint8_t *self, const uint64_t *source)
{
	uint64_t upper = source[1];
	uint64_t *destination = (uint64_t *)(self + 1040);

	destination[0] = source[0];
	destination[1] = upper;
	return destination;
}

void *ScePafWidget_C6559467(uint64_t *destination, const uint8_t *self)
{
	const uint64_t *source = (const uint64_t *)(self + 1040);
	uint64_t upper = source[1];

	destination[0] = source[0];
	destination[1] = upper;
	return destination;
}

int ScePafWidget_074187EF(uint8_t *self, int32_t index)
{
	uint32_t flags;

	if (index < 0 || index > 3)
		return -2136014706;
	flags = *(uint32_t *)(self + 724) | 4;
	*(uint32_t *)(self + 656) = index;
	*(uint32_t *)(self + 724) = flags;
	return 0;
}

int ScePafWidget_316164E1(uint8_t *self, int32_t index)
{
	uint32_t flags;

	if (index < 0 || index > 9)
		return -2136014706;
	flags = *(uint32_t *)(self + 724) | 16;
	*(uint32_t *)(self + 760) = index;
	*(uint32_t *)(self + 724) = flags;
	return 0;
}

int ScePafWidget_7621796A(uint8_t *self, int32_t index)
{
	uint32_t flags;

	if (index < 0 || index > 9)
		return -2136014706;
	flags = *(uint32_t *)(self + 724) | 8;
	*(uint32_t *)(self + 660) = index;
	*(uint32_t *)(self + 724) = flags;
	return 0;
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

uint32_t *ScePafWidget_AB05B0A4(uint32_t *result)
{
	return ResetWidgetTransform(result);
}

uint32_t *ScePafWidget_ECB0651E(uint32_t *result)
{
	return ResetWidgetTransform(result);
}

int ScePafWidget_D2BF4CC7(uint8_t *self, const uint8_t *source)
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

int ScePafWidget_FFC580E3(const uint8_t *self, uint8_t *result)
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

uint8_t *ScePafWidget_F09837F9(uint8_t *self, uint32_t value)
{
	if (*(uint32_t *)(self + 644) != value) {
		*(uint32_t *)(self + 644) = value;
		uint8_t flags = self[405];
		if (value == 2)
			self[405] = flags & 0xFE;
		else
			self[405] = flags | 1;
	}
	return self;
}

uint8_t *ScePafWidget_2BACA6A8(uint8_t *self, const uint8_t *value)
{
	if (*(const uint8_t **)(self + 920) == value) {
		uint32_t state = value[36];

		*(uint32_t *)(self + 920) = 0;
		if (!state) {
			self = *(uint8_t **)(self + 532);
			if (self)
				self[12] |= 1;
		}
	}
	return self;
}

uint8_t *ScePafWidget_76BA63C3(uint8_t *self, uint8_t *value, int32_t mode)
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

uint8_t *ScePafWidget_0BEBC84F(uint8_t *self, uint8_t *item)
{
	int8_t flags = *(int8_t *)(item + 404);

	if (!(flags & 1)) {
		item[404] = flags | 1;
		self = *(uint8_t **)(self + 532);
		if (self)
			self[12] |= 1;
	}
	return self;
}

uint8_t *ScePafWidget_9C553208(uint8_t *self, uint8_t *item)
{
	int8_t flags = *(int8_t *)(item + 404);

	if (flags & 1) {
		item[404] = flags & ~1;
		self = *(uint8_t **)(self + 532);
		if (self)
			self[12] |= 1;
	}
	return self;
}

uint8_t *ScePafWidget_32053276(uint8_t *self, uint16_t mask)
{
	uint16_t flags = *(uint16_t *)(self + 574);

	if ((flags & (uint8_t)mask) && (flags & (mask & 0xFF00U))) {
		self = *(uint8_t **)(self + 536);
		if (self)
			*(uint16_t *)(self + 12) |= 1;
	}
	return self;
}

int ScePafWidget_939005A0(const uint8_t *self, uint32_t value)
{
	uint32_t count = *(const uint32_t *)(self + 1036);
	const uint32_t *cursor = *(const uint32_t *const *)(self + 1032);
	const uint32_t *end = cursor + count;

	while (cursor != end) {
		if (*cursor == value)
			break;
		if (cursor)
			++cursor;
	}
	return cursor != end;
}

uint8_t *ScePafWidget_7E86AC9D(const uint32_t *self, uint32_t value)
{
	const uint8_t *const *cursor = (const uint8_t *const *)self[0];
	const uint8_t *const *end = cursor + self[1];

	while (cursor != end) {
		if (*(const uint32_t *)(*cursor + 8) == value)
			return (uint8_t *)*cursor;
		if (cursor)
			++cursor;
	}
	return 0;
}

uint32_t *ScePafWidget_4DDDFF68(uint32_t *self, uint32_t payload)
{
	return InitializeWidgetRecord(self, payload);
}

int ScePafWidget_A8C5CE9E(void *self, uint32_t *attributes)
{
	attributes[1] = (attributes[1] & 0xF0FFFFFFU) | 0x01000000U;
	return 0;
}

uint32_t ScePafWidget_7B788425(int32_t index)
{
	return ((const uint32_t *)sce_paf_widget_state_155c)[index];
}

uint32_t ScePafWidget_3F295AA9(void)
{
	return sce_paf_widget_state_155c + 36;
}

/* Resource readiness is exposed through several widget compatibility aliases. */
extern int ScePafResource_598D91C1(const void *resource);

static inline __attribute__((always_inline))
int WidgetResourceReady(const void *resource)
{
	int result = ScePafResource_598D91C1(resource);

	if (result >= 0)
		return 0;
	return result;
}

int ScePafWidget_890AF221(const void *resource)
{
	return WidgetResourceReady(resource);
}

int ScePafWidget_CEC4F335(const void *resource)
{
	return WidgetResourceReady(resource);
}

int ScePafWidget_FC3C881A(const void *resource)
{
	return WidgetResourceReady(resource);
}

int ScePafWidget_13CF9C4E(const void *self, const char *tag)
{
	return tag == "cell_source";
}

void ScePafWidget_0A87E13F(uint32_t *self)
{
	*self = (uint32_t)-2127612860;
	PafDelete(self);
}

void ScePafWidget_346AE909(uint32_t *self)
{
	*self = (uint32_t)-2127593716;
	PafDelete(self);
}

int ScePafWidget_4BDA4BAA(void *self, uint32_t value)
{
	void *const *methods = *(void *const *const *)self;

	((void (*)(void *, uint32_t, uint32_t))methods[24])(
		self, 0x800000, value);
	return 0;
}

int ScePafWidget_FDB59013(void *self, const uint8_t *argument)
{
	void *const *methods = *(void *const *const *)self;

	((void (*)(void *, uint32_t))methods[24])(
		self, *(const uint32_t *)(argument + 16));
	return 0;
}

extern uint8_t *ScePafGraphics_A05227FA(void);

uint8_t *ScePafWidget_62D63539(void)
{
	sce_paf_widget_state_142d8 = 1;
	return ScePafGraphics_A05227FA();
}

int ScePafWidget_1D442DC0(void *self, uint32_t value)
{
	void *const *methods = *(void *const *const *)self;

	((void (*)(void *, uint32_t, uint32_t))methods[24])(
		self, 1048585, value);
	return 0;
}

int ScePafWidget_66D4EAE7(void *self, uint32_t value)
{
	void *const *methods = *(void *const *const *)self;

	((void (*)(void *, uint32_t, uint32_t))methods[24])(
		self, 65555, value);
	return 0;
}

int ScePafWidget_722CAC54(void *self, void *other)
{
	void *const *methods = *(void *const *const *)other;

	return ((int (*)(void *, void *))methods[95])(other, self);
}

void *ScePafWidget_64A10940(uint8_t *self)
{
	void *other = *(void **)(self + 24);
	void *const *methods = *(void *const *const *)other;

	((void (*)(void *))methods[27])(other);
	return self;
}

void *ScePafWidget_40F2A311(uint32_t *self)
{
	if (self[118] == 0) {
		void *const *methods = *(void *const *const *)self;
		return ((void *(*)(void *))methods[104])(self);
	}
	return self;
}

int ScePafWidget_A8F7D4C1(void *self, uint32_t value)
{
	void *const *methods = *(void *const *const *)self;

	return ((int (*)(void *, uint32_t, uint32_t, uint32_t))methods[63])(
		self, value, 1, 0);
}

int ScePafWidget_E38C03C5(void *self)
{
	void *const *methods = *(void *const *const *)self;

	return ((int (*)(void *, uint32_t, uint32_t))methods[152])(
		self, 268435458, 0);
}

uint64_t *ScePafWidget_3D6C290E(uint64_t *result, const uint8_t *self)
{
	const uint8_t *source = *(const uint8_t *const *)(self + 12);

	result[0] = *(const uint64_t *)(source + 712);
	result[1] = 0;
	return result;
}

uint64_t *ScePafWidget_6206FB7E(uint64_t *result, const uint8_t *self)
{
	const uint8_t *source = *(const uint8_t *const *)(self + 12);

	result[0] = *(const uint64_t *)(source + 720);
	result[1] = 0;
	return result;
}

extern int ScePafGraphics_A1B995E7(const uint8_t *self, uint32_t *output);
extern int ScePafGraphics_4EF76BA1(const uint8_t *self, uint32_t index,
					   uint8_t *output);
extern int ScePafGraphics_830D5848(const uint8_t *self, uint32_t index,
					   uint32_t *output);
extern int ScePafGraphics_D48AB212(const uint8_t *self, uint32_t *first,
					   uint32_t *second);

int ScePafWidget_68CE5A20(const uint8_t *self, uint32_t *output)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return ScePafGraphics_A1B995E7(graphics, output);
	return -2136014814;
}

int ScePafWidget_7F6181F1(const uint8_t *self, uint32_t index,
			   uint8_t *output)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return ScePafGraphics_4EF76BA1(graphics, index, output);
	return -2136014814;
}

int ScePafWidget_8403FC5B(const uint8_t *self, uint32_t index,
			   uint32_t *output)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return ScePafGraphics_830D5848(graphics, index, output);
	return -2136014814;
}

int ScePafWidget_85421D01(const uint8_t *self, uint32_t *first,
			   uint32_t *second)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return ScePafGraphics_D48AB212(graphics, first, second);
	return -2136014814;
}

extern int ScePafGraphics_BE22EED6(const uint8_t *self, int index,
					   uint64_t *output);
extern int ScePafGraphics_E73E0E33(const uint8_t *self, uint64_t *output);
extern int ScePafGraphics_5AF2E479(const uint8_t *self, int index,
					   uint32_t *output);
extern uint8_t *ScePafGraphics_4C4593FF(void);

int ScePafWidget_09B7286A(const uint8_t *self, int index, uint64_t *output)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return ScePafGraphics_BE22EED6(graphics, index, output);
	return -2136014814;
}

int ScePafWidget_4CABAAE0(const uint8_t *self, uint64_t *output)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return ScePafGraphics_E73E0E33(graphics, output);
	return -2136014814;
}

int ScePafWidget_B3BC5EE1(const uint8_t *self, int index,
			   uint32_t *output)
{
	const uint8_t *graphics = *(const uint8_t *const *)(self + 640);

	if (graphics)
		return ScePafGraphics_5AF2E479(graphics, index, output);
	return -2136014814;
}

uint32_t *ScePafWidget_AE0C4046(void)
{
	ScePafGraphics_4C4593FF();
	sce_paf_widget_state_142d8 = 0;
	return &sce_paf_widget_state_142d8;
}

int ScePafWidget_85A01D8F(void *self, unsigned int value)
{
	if (value <= 1) {
		if (value)
			return 1;
	} else if (value <= 2) {
		return 2;
	}
	return 0;
}

int ScePafWidget_2385B189(uint8_t *self, unsigned int index, int value)
{
	if (index >= 6)
		return -2135998193;
	*(uint32_t *)(self + 11592) |= 1U << index;
	*(int *)(self + 11600 + 4 * index) = value;
	return 0;
}

uint8_t *ScePafWidget_730AA895(uint8_t *self, uint32_t value,
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

uint32_t ScePafWidget_27278DC0(const uint32_t *self)
{
	uint32_t index = self[1];

	if (!index)
		return 0;
	return *(const uint32_t *)((const uint8_t *)self[0] + 8 * (index - 1));
}

__attribute__((noinline)) int ScePafWidget_8EDFDE19(const void *self, const char *tag)
{
	return tag == "widget_base";
}

int ScePafWidget_9B80FB29(const void *self, const char *tag)
{
	return tag == "segment_source";
}

int ScePafWidget_FC6F4E29(const void *self, const char *tag)
{
	return tag == "visualizer";
}

void ScePafWidget_D3AFE81E(uint32_t *self)
{
	*self = (uint32_t)-2127612828;
	PafDelete(self);
}

int ScePafWidget_F0D05847(uint32_t *self, uint32_t value)
{
	uint8_t *vtable = (uint8_t *)self[0];

	self[255] = value;
	return ((int (*)(uint32_t *))*(void **)(vtable + 572))(self);
}

int ScePafWidget_0521777F(const void *self, const char *tag)
{
	if (tag == "scrollbar2d")
		return 1;
	return ScePafWidget_8EDFDE19(self, tag);
}

int ScePafWidget_11F157B4(const void *self, const char *tag)
{
	if (tag == "group")
		return 1;
	return ScePafWidget_8EDFDE19(self, tag);
}

int ScePafWidget_A4E53390(uint8_t *self)
{
	uint8_t *vtable = *(uint8_t **)self;

	self[992] = 1;
	return ((int (*)(uint8_t *))*(void **)(vtable + 560))(self);
}

int ScePafWidget_CBB4D949(int a1, int a2, int a3, uint8_t *self)
{
	uint8_t *vtable = *(uint8_t **)self;

	return ((int (*)(uint8_t *, int, int, int))*(void **)(vtable + 592))(
		self, a1, a2, a3);
}

int ScePafWidget_1AC6007B(const void *self, const char *tag)
{
	if (tag == "camera")
		return 1;
	return ScePafWidget_8EDFDE19(self, tag);
}

__attribute__((noinline)) int ScePafWidget_31C1301C(const void *self, const char *tag)
{
	if (tag == "pageturn_base")
		return 1;
	return ScePafWidget_8EDFDE19(self, tag);
}

int ScePafWidget_61D34238(const void *self, const char *tag)
{
	if (tag == "app_icon_simple")
		return 1;
	return ScePafWidget_8EDFDE19(self, tag);
}

__attribute__((noinline)) int ScePafWidget_743F6B5A(const void *self, const char *tag)
{
	if (tag == "spin_base")
		return 1;
	return ScePafWidget_8EDFDE19(self, tag);
}

int ScePafWidget_766B09F4(const void *self, const char *tag)
{
	if (tag == "scroll_view_parent")
		return 1;
	return ScePafWidget_8EDFDE19(self, tag);
}

__attribute__((noinline)) int ScePafWidget_7BBA6B10(const void *self, const char *tag)
{
	if (tag == "text")
		return 1;
	return ScePafWidget_8EDFDE19(self, tag);
}

/* Type tags are compared by pointer identity in the native widget hierarchy. */
#define DEFINE_WIDGET_TYPE(name, tag, parent) \
__attribute__((noinline)) int ScePafWidget_##name(const void *self, const char *type) \
{ \
	if (type == tag) \
		return 1; \
	return ScePafWidget_##parent(self, type); \
}

DEFINE_WIDGET_TYPE(824BA45C, "button_base", 8EDFDE19)
DEFINE_WIDGET_TYPE(F0408EDF, "state_button_base", 824BA45C)
DEFINE_WIDGET_TYPE(D48B7AF1, "scrollbar", 8EDFDE19)
DEFINE_WIDGET_TYPE(E83DA42E, "scroll_view_base", 8EDFDE19)
DEFINE_WIDGET_TYPE(8097275D, "list_view", E83DA42E)
DEFINE_WIDGET_TYPE(4E7DAB19, "app_icon_base", 8EDFDE19)
DEFINE_WIDGET_TYPE(B4A0700B, "progressbar", 8EDFDE19)
DEFINE_WIDGET_TYPE(02B20FBB, "button", 824BA45C)
DEFINE_WIDGET_TYPE(1459FA05, "composite_button", 824BA45C)
DEFINE_WIDGET_TYPE(315650EC, "check_box", F0408EDF)
DEFINE_WIDGET_TYPE(42E04324, "image_button", 02B20FBB)
DEFINE_WIDGET_TYPE(0354FA10, "num_spin", 743F6B5A)
DEFINE_WIDGET_TYPE(483E2C1D, "item_spin", 743F6B5A)
DEFINE_WIDGET_TYPE(3AAD3226, "scrollbar_touch", D48B7AF1)
DEFINE_WIDGET_TYPE(43B03F9F, "scroll_view", E83DA42E)
DEFINE_WIDGET_TYPE(4C462EB0, "file_list", 8097275D)
DEFINE_WIDGET_TYPE(6436B60C, "app_icon_2d", 4E7DAB19)
DEFINE_WIDGET_TYPE(7D0C8076, "progressbar_touch", B4A0700B)
DEFINE_WIDGET_TYPE(87ECDABA, "list_item", 8EDFDE19)
DEFINE_WIDGET_TYPE(8E3BBEA4, "directbar", 8EDFDE19)
DEFINE_WIDGET_TYPE(9074BCF9, "plane_button", 824BA45C)
DEFINE_WIDGET_TYPE(915460AF, "radio_box", 8EDFDE19)
DEFINE_WIDGET_TYPE(941FE103, "sliding_drawer", E83DA42E)
DEFINE_WIDGET_TYPE(B7F6DF9C, "plane", 8EDFDE19)
DEFINE_WIDGET_TYPE(8F603B76, "box", B7F6DF9C)
DEFINE_WIDGET_TYPE(B954B379, "busyindicator", 8EDFDE19)
DEFINE_WIDGET_TYPE(C7D6AD89, "pageturn", 31C1301C)
DEFINE_WIDGET_TYPE(CC24A689, "rich_text", E83DA42E)
DEFINE_WIDGET_TYPE(CCB1364A, "image_slidebar", 8EDFDE19)
DEFINE_WIDGET_TYPE(D123A71C, "slidebar", 8EDFDE19)
DEFINE_WIDGET_TYPE(D24DB0D6, "corner_button", 02B20FBB)
DEFINE_WIDGET_TYPE(E07625DE, "dialog", 8F603B76)
DEFINE_WIDGET_TYPE(E972265A, "text_box", 7BBA6B10)
DEFINE_WIDGET_TYPE(F11BB524, "composite_drawer", 8EDFDE19)
DEFINE_WIDGET_TYPE(FA0BFE83, "supplier", B7F6DF9C)
DEFINE_WIDGET_TYPE(FA17CA06, "page", 8EDFDE19)
DEFINE_WIDGET_TYPE(FE7FF370, "radio_button", F0408EDF)

#undef DEFINE_WIDGET_TYPE

int ScePafWidget_9BE8F98D(uint32_t *self)
{
	uint8_t *vtable = (uint8_t *)self[0];

	((void (*)(uint32_t *))*(void **)(vtable + 612))(self);
	vtable = (uint8_t *)self[0];
	return ((int (*)(uint32_t *))*(void **)(vtable + 616))(self);
}

/* The native delete callback marks the dispatch scope in a shared word. */
uint32_t sce_paf_widget_dispatch_guard_142d4;

int ScePafWidget_0C7FE13F(uint32_t *self)
{
	uint8_t *vtable = (uint8_t *)self[0];

	sce_paf_widget_dispatch_guard_142d4 = 1;
	((void (*)(uint32_t *))*(void **)(vtable + 8))(self);
	sce_paf_widget_dispatch_guard_142d4 = 0;
	return 0;
}

int ScePafWidget_3C06E3D4(const uint8_t *self, uint64_t *output)
{
	const uint8_t *item = *(const uint8_t *const *)(self + 700);

	if (!item)
		return -2136014814;
	*output = *(const uint64_t *)(item + 48);
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

uint32_t *ScePafWidget_69498B64(uint32_t *self)
{
	return InitializeWidgetStatusRecord(self);
}

uint32_t *ScePafWidget_A48FBB8E(uint32_t *self)
{
	return InitializeWidgetStatusRecord(self);
}

uint32_t sce_paf_widget_token_counter_13cd8;

uint32_t *ScePafWidget_AEC1B38D(uint32_t *self, const uint64_t *value)
{
	uint32_t previous = sce_paf_widget_token_counter_13cd8;

	*(uint64_t *)self = *value;
	sce_paf_widget_token_counter_13cd8 = previous + 1;
	self[2] = previous;
	return self;
}

/* Native single-dispatch widget methods. */
static inline __attribute__((always_inline))
int WidgetInvokeSlot(void *self, uint32_t offset)
{
	const uint8_t *vtable = *(const uint8_t *const *)self;
	return ((int (*)(void *))*(void *const *)(vtable + offset))(self);
}

int ScePafWidget_001CA5F6(void *self)
{
	return WidgetInvokeSlot(self, 448);
}

int ScePafWidget_17BFAECF(void *self)
{
	return WidgetInvokeSlot(self, 468);
}

int ScePafWidget_33B0385F(void *self)
{
	return WidgetInvokeSlot(self, 140);
}

int ScePafWidget_37133F0F(void *self)
{
	return WidgetInvokeSlot(self, 416);
}

int ScePafWidget_3EFA0B3A(void *self)
{
	return WidgetInvokeSlot(self, 484);
}

int ScePafWidget_6E59DBBC(void *self)
{
	return WidgetInvokeSlot(self, 448);
}

int ScePafWidget_BDEF4D3F(void *self)
{
	return WidgetInvokeSlot(self, 552);
}

int ScePafWidget_BC080BD7(void *self)
{
	(void)((void (*)(void *))*(void *const *)
		(*(const uint8_t *const *)self + 20))(self);
	return 0;
}

uint8_t *ScePafWidget_4EA317B8(const uint8_t *self, const uint64_t *value)
{
	/* The native VLDM fetches the entire 128-bit pair before loading self. */
	const volatile uint32_t *words = (const volatile uint32_t *)value;
	uint32_t lo = words[0];
	uint32_t hi = words[1];
	volatile uint32_t unused2 = words[2];
	volatile uint32_t unused3 = words[3];
	uint8_t *target = *(uint8_t *const *)(self + 12);
	(void)unused2;
	(void)unused3;
	*(uint64_t *)(target + 720) = (uint64_t)lo | ((uint64_t)hi << 32);
	return target;
}

uint8_t *ScePafWidget_6CC95725(const uint8_t *self, const uint64_t *value)
{
	const volatile uint32_t *words = (const volatile uint32_t *)value;
	uint32_t lo = words[0];
	uint32_t hi = words[1];
	volatile uint32_t unused2 = words[2];
	volatile uint32_t unused3 = words[3];
	uint8_t *target = *(uint8_t *const *)(self + 12);
	(void)unused2;
	(void)unused3;
	*(uint64_t *)(target + 712) = (uint64_t)lo | ((uint64_t)hi << 32);
	return target;
}

int ScePafWidget_8552BF82(void *self, const uint8_t *argument)
{
	void *target = *(void *const *)(argument + 12);
	const uint8_t *vtable = *(const uint8_t *const *)target;
	return ((int (*)(void *))*(void *const *)(vtable + 108))(target);
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

uint32_t *ScePafWidget_006A5DE6(uint32_t *self)
{
	*self = (uint32_t)-2127593716;
	return self;
}

uint32_t *ScePafWidget_36F89F9C(uint32_t *self)
{
	*self = (uint32_t)-2127593716;
	return self;
}

uint32_t *ScePafWidget_61FC862B(uint32_t *self)
{
	*self = (uint32_t)-2127593716;
	return self;
}

uint32_t *ScePafWidget_4F5ED653(uint32_t *self)
{
	*self = (uint32_t)-2127612828;
	return self;
}

uint32_t *ScePafWidget_76C04679(uint32_t *self)
{
	*self = (uint32_t)-2127612860;
	return self;
}

uint32_t *ScePafWidget_9224D27B(uint32_t *self)
{
	*self = (uint32_t)-2127612860;
	return self;
}

uint32_t *ScePafWidget_9FBE52BB(uint32_t *self)
{
	*self = (uint32_t)-2127612828;
	return self;
}

uint32_t *ScePafWidget_B8CF2A93(uint32_t *self)
{
	*self = (uint32_t)-2127593716;
	return self;
}

int ScePafWidget_B5DACB0A(void *unused, const uint8_t *argument)
{
	void *target = *(void *const *)(argument + 8);

	if (target) {
		const uint8_t *vtable = *(const uint8_t *const *)target;
		return ((int (*)(void *))*(void *const *)(vtable + 108))(target);
	}
	return 0;
}

int ScePafWidget_A3EBC15F(const uint8_t *self)
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

int ScePafWidget_036BCE96(uint8_t *self, float value)
{
	uint32_t flags = *(const uint32_t *)(self + 724);

	*(float *)(self + 764) = value;
	*(uint32_t *)(self + 724) = flags | 0x10U;
	return 0;
}

int ScePafWidget_734468C9(uint8_t *self, float value)
{
	uint32_t flags = *(const uint32_t *)(self + 724);

	*(float *)(self + 664) = value;
	*(uint32_t *)(self + 724) = flags | 8U;
	return 0;
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

int ScePafWidget_C3D53C98(void *self, int first, int second)
{
	const uint8_t *vtable = *(const uint8_t *const *)self;

	return ((int (*)(void *, int, int, int))*(void *const *)(vtable + 272))(
		self, first, second, 0);
}

uint32_t sce_paf_widget_serial_13cd8;

void *ScePafWidget_CFA20521(uint8_t *result, const uint64_t *payload)
{
	uint32_t previous = sce_paf_widget_serial_13cd8;

	*(uint64_t *)result = *payload;
	sce_paf_widget_serial_13cd8 = previous + 1;
	*(uint32_t *)(result + 8) = previous;
	return result;
}

int ScePafWidget_F130F006(uint8_t *self, uint32_t value)
{
	if ((int32_t)value < 0 || (int32_t)value > 2)
		return -2136014706;
	*(uint32_t *)(self + 716) = value;
	return 0;
}

static inline __attribute__((always_inline))
void *DestroyWidgetResourceHandle(uint8_t *self)
{
	void *handle = *(void **)(self + 12);

	if (handle) {
		const uint8_t *vtable = *(const uint8_t *const *)handle;

		((void (*)(void *))*(void *const *)(vtable + 4))(handle);
	}
	*(uint32_t *)(self + 12) = UINT32_MAX;
	return self;
}

void *ScePafWidget_42D6C09C(uint8_t *self)
{
	return DestroyWidgetResourceHandle(self);
}

void *ScePafWidget_4C70D824(uint8_t *self)
{
	return DestroyWidgetResourceHandle(self);
}

int ScePafWidget_46E5C1ED(void *self)
{
	const uint8_t *vtable = *(const uint8_t *const *)self;

	return ((int (*)(void *, int, int))*(void *const *)(vtable + 444))(
		self, 1, 1);
}

float ScePafWidget_4983B3FC(uint8_t *self)
{
	const uint8_t *delegate = *(const uint8_t *const *)(self + 680);

	if (delegate)
		return *(const float *)(delegate + 668);
	return (float)*(const int32_t *)(self + 644);
}

const char *ScePafWidget_F3A13905(void)
{
	return "style_scroll_view";
}
