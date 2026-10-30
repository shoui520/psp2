/* Standalone native widget state constructors and field operations. */
#include <stdint.h>

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
