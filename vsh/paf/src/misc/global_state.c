/* Miscellaneous ScePaf state words originally held in shared BSS. */
#include <stdint.h>
#include <stddef.h>

uint32_t sce_paf_misc_flags_ee98;
uint32_t sce_paf_misc_counter_ee9c;
uint32_t sce_paf_misc_value_ee6c;
uint32_t sce_paf_misc_limit_eea0;
uint32_t sce_paf_misc_ticks_d550;
uint32_t sce_paf_misc_limit_d554;
uint8_t sce_paf_misc_active_1090;
uint32_t sce_paf_misc_flags_d548;
uint32_t sce_paf_misc_value_cf08;
uint32_t sce_paf_misc_counter_c224;
uint32_t sce_paf_misc_value_d41c;
uint32_t sce_paf_misc_value_1088;
uint32_t sce_paf_misc_value_d510;
uint32_t sce_paf_misc_value_d414;
uint32_t sce_paf_misc_value_ef04;
int8_t sce_paf_misc_flag_eef5;
uint32_t sce_paf_misc_value_1078;
float sce_paf_misc_float_1178;
uint64_t sce_paf_misc_pair_1158;
uint32_t sce_paf_misc_slots_d57c[2];
uint32_t sce_paf_misc_flags_ede0[2];
uint32_t sce_paf_misc_counters_edf0[2];
uint32_t sce_paf_misc_limits_edf8[2];
uint8_t sce_paf_misc_cells_dc60[2][8][32] __attribute__((aligned(32)));
uint8_t sce_paf_misc_cells_de60[2][8][32] __attribute__((aligned(32)));
uint32_t sce_paf_misc_shared_f3c8;
uint64_t sce_paf_misc_pair_1170;
float sce_paf_misc_float_112c;
float sce_paf_misc_float_1130;
float sce_paf_misc_float_1144;
float sce_paf_misc_float_1148;
float sce_paf_misc_float_114c;
float sce_paf_misc_float_1150;
uint32_t sce_paf_misc_mode_d570;
uint32_t sce_paf_misc_value_d410;
uint32_t sce_paf_misc_flags_ee00[2];
uint32_t sce_paf_misc_cells_da60[2][8][8];
uint32_t sce_paf_misc_counters_d200[7];
uint32_t sce_paf_misc_limits_d21c[7];
uint32_t sce_paf_misc_masks_d498[5][6];
volatile uint32_t sce_paf_misc_selector_103c[3];

uint32_t ScePafMisc_75AFD25C(int32_t index)
{
	switch (index) {
	case 0:
		return sce_paf_misc_selector_103c[0];
	case 1:
		return sce_paf_misc_selector_103c[1];
	case 2:
		return sce_paf_misc_selector_103c[2];
	default:
		return 0;
	}
}

uint32_t ScePafMisc_0D1B028E(uint32_t value)
{
	uint32_t ticks = ++sce_paf_misc_ticks_d550;
	sce_paf_misc_value_d410 = value;
	uint32_t limit = sce_paf_misc_limit_d554;
	sce_paf_misc_active_1090 = 1;
	if ((int32_t)ticks > (int32_t)limit)
		sce_paf_misc_limit_d554 = limit + 1;
	return sce_paf_misc_value_d414;
}

uint32_t ScePafMisc_57080F75(int32_t mode)
{
	uint32_t ticks = ++sce_paf_misc_ticks_d550;
	sce_paf_misc_active_1090 = 1;
	uint32_t limit = sce_paf_misc_limit_d554;
	if (mode >= 0 && mode < 5)
		sce_paf_misc_value_1088 = (uint32_t)mode;
	if ((int32_t)ticks > (int32_t)limit)
		sce_paf_misc_limit_d554 = limit + 1;
	return sce_paf_misc_value_1088;
}

uint32_t ScePafMisc_4E93C051(void)
{
	return sce_paf_misc_flags_ee98;
}

uint32_t ScePafMisc_26BCDBCA(uint32_t mask)
{
	sce_paf_misc_flags_ee98 |= mask;
	return sce_paf_misc_flags_ee98;
}

void ScePafMisc_9FFF47AE(void)
{
	sce_paf_misc_counter_ee9c++;
}

uint32_t ScePafMisc_C11453F8(uint32_t new_value)
{
	uint32_t old_value = sce_paf_misc_value_ee6c;
	sce_paf_misc_value_ee6c = new_value;
	return old_value;
}

uint32_t ScePafMisc_8D33865E(void)
{
	uint32_t limit = sce_paf_misc_limit_eea0;
	if ((int32_t)sce_paf_misc_counter_ee9c > (int32_t)limit) {
		limit++;
		sce_paf_misc_limit_eea0 = limit;
	}
	return limit;
}

int ScePafMisc_8DA260AE(void)
{
	return (int32_t)sce_paf_misc_counter_ee9c >
		(int32_t)sce_paf_misc_limit_eea0;
}

int ScePafMisc_8D0DC873(void)
{
	return (int32_t)sce_paf_misc_ticks_d550 >
		(int32_t)sce_paf_misc_limit_d554;
}

uint32_t ScePafMisc_E4F9C1B6(void)
{
	uint32_t limit = sce_paf_misc_limit_d554;
	if ((int32_t)sce_paf_misc_ticks_d550 > (int32_t)limit) {
		limit++;
		sce_paf_misc_limit_d554 = limit;
	}
	return limit;
}

int ScePafMisc_874753FD(void)
{
	sce_paf_misc_ticks_d550++;
	sce_paf_misc_active_1090 = 1;
	return 1;
}

uint32_t ScePafMisc_FD24539D(void)
{
	return sce_paf_misc_flags_d548;
}

uint32_t ScePafMisc_2DFE8975(uint32_t mask)
{
	sce_paf_misc_flags_d548 |= mask;
	return sce_paf_misc_flags_d548;
}

uint32_t ScePafMisc_E9A4171E(void)
{
	return sce_paf_misc_value_cf08;
}

int ScePafMisc_951B7B59(uint32_t value)
{
	sce_paf_misc_value_cf08 = value;
	return 1;
}

uint32_t *ScePafMisc_7E6CA20C(void)
{
	sce_paf_misc_counter_c224++;
	return &sce_paf_misc_counter_c224;
}

uint32_t ScePafMisc_7F98B644(uint32_t value)
{
	uint32_t previous = sce_paf_misc_value_d41c;
	sce_paf_misc_value_d41c = value;
	return previous;
}

uint32_t ScePafMisc_333C86DC(void)
{
	return sce_paf_misc_value_1088;
}

uint32_t ScePafMisc_8C27F38A(void)
{
	return sce_paf_misc_value_d510;
}

uint32_t ScePafMisc_9CB5D72E(void)
{
	return sce_paf_misc_value_d414;
}

uint32_t ScePafMisc_AF4FC3F4(void)
{
	return sce_paf_misc_value_ef04;
}

int ScePafMisc_C51EF81D(void)
{
	return sce_paf_misc_flag_eef5;
}

int ScePafMisc_862C8916(void)
{
	return sce_paf_misc_value_1078 != 0;
}

float *ScePafMisc_F8D63B8E(float value)
{
	sce_paf_misc_float_1178 = value;
	return &sce_paf_misc_float_1178;
}

uint64_t ScePafMisc_A8133C0A(void)
{
	return sce_paf_misc_pair_1158;
}

uint32_t ScePafMisc_DFEE9387(uint32_t index)
{
	return index < 2 ? sce_paf_misc_flags_ede0[index] : 0;
}

uint32_t ScePafMisc_CBAF777C(uint32_t index, uint32_t value)
{
	uint32_t previous;
	if (index >= 2)
		return 0;
	previous = sce_paf_misc_slots_d57c[index];
	sce_paf_misc_slots_d57c[index] = value;
	return previous;
}

uint32_t ScePafMisc_0D4B4B67(uint32_t index)
{
	if (index >= 2)
		return index;
	sce_paf_misc_counters_edf0[index]++;
	return index << 2;
}

uint32_t ScePafMisc_FFE73747(uint32_t index, uint32_t mask)
{
	if (index >= 2)
		return 0;
	sce_paf_misc_flags_ede0[index] |= mask;
	return sce_paf_misc_flags_ede0[index];
}

int ScePafMisc_3DD77645(uint32_t index)
{
	return index < 2 &&
		sce_paf_misc_counters_edf0[index] > sce_paf_misc_limits_edf8[index];
}

uint32_t ScePafMisc_15613EA2(uint32_t index)
{
	uint32_t limit;
	if (index >= 2)
		return index;
	limit = sce_paf_misc_limits_edf8[index];
	if (sce_paf_misc_counters_edf0[index] > limit) {
		limit++;
		sce_paf_misc_limits_edf8[index] = limit;
	}
	return limit;
}

void *ScePafMisc_2BE3A49D(uint32_t group, uint32_t cell, void **output)
{
	void *address = NULL;
	if (group < 2 && cell < 8)
		address = sce_paf_misc_cells_dc60[group][cell];
	*output = address;
	return address;
}

void *ScePafMisc_B5C459E2(uint32_t group, uint32_t cell, void **output)
{
	void *address = NULL;
	if (group < 2 && cell < 8)
		address = sce_paf_misc_cells_de60[group][cell];
	*output = address;
	return address;
}

int ScePafMisc_49BD531A(uint32_t group, uint32_t cell,
			float *first, float *second)
{
	const int32_t *values;
	int32_t a, b;
	if (group >= 2 || cell >= 8)
		return -1;
	values = (const int32_t *)sce_paf_misc_cells_de60[group][cell];
	a = values[2];
	b = values[3];
	*first = (float)a;
	*second = (float)b;
	return 0;
}

int ScePafMisc_4E922BF6(uint32_t group, uint32_t cell,
			float *first, float *second)
{
	const int32_t *values;
	int32_t a, b;
	if (group >= 2 || cell >= 8)
		return -1;
	values = (const int32_t *)sce_paf_misc_cells_de60[group][cell];
	a = values[6];
	b = values[7];
	*first = (float)a;
	*second = (float)b;
	return 0;
}

void *ScePafMisc_405C4F0A(uint32_t *object, uint32_t first)
{
	volatile uint32_t *words = (volatile uint32_t *)object;
	words[0] = first;
	words[1] = 0;
	words[2] = UINT32_MAX;
	words[3] = UINT32_MAX;
	words[6] = 0;
	words[4] = 0x3f;
	words[5] = (uint32_t)(uintptr_t)&sce_paf_misc_shared_f3c8;
	return object;
}

void *ScePafMisc_7616DF6C(uint32_t *object, uint32_t first)
{
	volatile uint32_t *words = (volatile uint32_t *)object;
	words[0] = first;
	words[1] = 0;
	words[2] = UINT32_MAX;
	words[3] = UINT32_MAX;
	words[6] = 0;
	words[4] = 0x3f;
	words[5] = (uint32_t)(uintptr_t)&sce_paf_misc_shared_f3c8;
	return object;
}

uint32_t ScePafMisc_A08B8D8A(uint32_t value)
{
	sce_paf_misc_pair_1170 = value;
	return value;
}

void ScePafMisc_DA9F40ED(uint32_t *first, uint32_t *second)
{
	float a = sce_paf_misc_float_112c;
	float b = sce_paf_misc_float_1130;
	uint32_t x = (uint32_t)a;
	uint32_t y = (uint32_t)b;
	*first = x;
	*second = y;
}

int ScePafMisc_0495CE33(void)
{
	uint32_t mode = sce_paf_misc_mode_d570;
	uint32_t index;
	for (index = 0; index < 2; ++index) {
		if (mode == 2)
			sce_paf_misc_flags_ee00[index] ^= 0x80;
	}
	return 0;
}

int ScePafMisc_DD2F6FFC(uint32_t group)
{
	uint32_t index;
	if (group >= 2)
		return 1;
	for (index = 0; index < 8; ++index) {
		uint32_t type = sce_paf_misc_cells_da60[group][index][5];
		if (type == 1 || type == 3)
			return 0;
	}
	return 1;
}

uint32_t ScePafMisc_2FBC2819(uint32_t mask)
{
	uint32_t index;
	for (index = 0; index < 7; ++index) {
		if ((mask & (1U << index)) &&
		    (int32_t)sce_paf_misc_counters_d200[index] >
		    (int32_t)sce_paf_misc_limits_d21c[index])
			sce_paf_misc_limits_d21c[index]++;
	}
	return mask;
}

uint32_t *ScePafMisc_68657766(void)
{
	uint32_t index;
	for (index = 0; index < 7; ++index) {
		if (((1U << index) & 0x10U) &&
		    (int32_t)sce_paf_misc_counters_d200[index] >
		    (int32_t)sce_paf_misc_limits_d21c[index])
			sce_paf_misc_limits_d21c[index]++;
	}
	return sce_paf_misc_limits_d21c + 7;
}

uint32_t ScePafMisc_C4E8CBE6(uint32_t mask)
{
	uint32_t flags = sce_paf_misc_flags_d548;
	int32_t mode = (int32_t)sce_paf_misc_value_d510;
	const uint32_t *entry = mode < 5 ?
		sce_paf_misc_masks_d498[mode] : sce_paf_misc_masks_d498[0];
	uint32_t result = (mask & entry[2]) | flags;
	sce_paf_misc_flags_d548 = result;
	return result;
}
