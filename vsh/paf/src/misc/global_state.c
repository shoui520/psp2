/* Miscellaneous ScePaf state words originally held in shared BSS. */
#include <stdint.h>

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
