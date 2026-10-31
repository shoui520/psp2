/* Miscellaneous ScePaf state words originally held in shared BSS. */
#include <stdint.h>
#include <stddef.h>
#include "../compiler.h"
#include <kernel/threadmgr.h>

extern int32_t sceBtUnregisterCallback(void);

int32_t sce_paf_bt_callback_uid;
int32_t sce_paf_bt_callback_state;

int ScePafMisc_2825FFFD(void)
{
	int result;

	if (sce_paf_bt_callback_uid == 0)
		return -2135994015;
	result = sceBtUnregisterCallback();
	if (result >= 0) {
		result = sceKernelDeleteCallback(sce_paf_bt_callback_uid);
		if (result >= 0) {
			sce_paf_bt_callback_uid = 0;
			sce_paf_bt_callback_state = 0;
		}
	}
	return result;
}

uint32_t sce_paf_misc_flags_ee98;
uint32_t sce_paf_misc_counter_ee9c;
uint32_t sce_paf_misc_value_ee6c;
uint32_t sce_paf_misc_limit_eea0;
uint32_t sce_paf_misc_counter_c224;
uint32_t sce_paf_misc_value_ef04;
int8_t sce_paf_misc_flag_eef5;
uint32_t sce_paf_misc_value_1078;
float sce_paf_misc_float_1178;
uint64_t sce_paf_misc_pair_1158;
uint32_t sce_paf_misc_shared_f3c8;
uint64_t sce_paf_misc_pair_1170;

uint32_t sce_paf_misc_mode_d570;
extern uint32_t sce_paf_misc_counters_d200[7];
extern int sceAppMgrSetSystemImposeState2(int state);

int ScePafMisc_F68553A5(int value)
{
	uint32_t mode = value == 1 ? 2U : 1U;
	uint32_t i;
	uint32_t mask = 0;

	if (sce_paf_misc_mode_d570 != mode) {
		sce_paf_misc_mode_d570 = mode;
		for (i = 0; i < 7; ++i) {
			if ((int32_t)sce_paf_misc_counters_d200[i] > 0)
				mask |= 1U << i;
		}
		if (mask & 0x6fU)
			return sceAppMgrSetSystemImposeState2((int)mode);
	}
	return (int)mode;
}

uint32_t sce_paf_misc_counters_d200[7];
uint32_t sce_paf_misc_limits_d21c[7];

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

int ScePafMisc_ED289E89(uint32_t requested, int require_all)
{
	uint32_t active = 0;
	uint32_t selected;
	uint32_t i;

	for (i = 0; i < 7; i++)
		if ((int32_t)sce_paf_misc_counters_d200[i] > 0)
			active |= 1U << i;
	selected = active & requested;
	return require_all ? selected == requested : selected != 0;
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

uint32_t *ScePafMisc_7E6CA20C(void)
{
	sce_paf_misc_counter_c224++;
	return &sce_paf_misc_counter_c224;
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

/* Sum outstanding work in the three optional job queues. */
struct PafJobWaitState;
extern int _ZNK3paf3job8JobQueue8NumItemsEv(struct PafJobWaitState *self);

struct PafJobWaitState *sce_paf_misc_job_queue_103c;
struct PafJobWaitState *sce_paf_misc_job_queue_1040;
struct PafJobWaitState *sce_paf_misc_job_queue_1044;

int ScePafMisc_52C7998E(void)
{
	int count = 0;

	if (sce_paf_misc_job_queue_103c)
		count = _ZNK3paf3job8JobQueue8NumItemsEv(sce_paf_misc_job_queue_103c);
	if (sce_paf_misc_job_queue_1040)
		count += _ZNK3paf3job8JobQueue8NumItemsEv(sce_paf_misc_job_queue_1040);
	if (sce_paf_misc_job_queue_1044)
		count += _ZNK3paf3job8JobQueue8NumItemsEv(sce_paf_misc_job_queue_1044);
	return count;
}
