/* Miscellaneous ScePaf state words originally held in shared BSS. */
#include <stdint.h>

uint32_t sce_paf_misc_flags_ee98;
uint32_t sce_paf_misc_counter_ee9c;
uint32_t sce_paf_misc_value_ee6c;

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
