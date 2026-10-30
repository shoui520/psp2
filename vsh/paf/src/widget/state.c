/* Standalone native widget state constructors and field operations. */
#include <stdint.h>
#include "../std/new.h"

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
