/* ScePaf's module-load parameters and shared heap configuration. */
#ifndef PAF_MODULE_H
#define PAF_MODULE_H

#include <scetypes.h>

typedef struct ScePafInit {
	SceUInt32 global_heap_size;
	SceInt32 reserved[2];
	SceInt32 cdlg_mode;
	SceInt32 global_heap_alignment;
	SceInt32 global_heap_disable_assert_on_alloc_failure;
} ScePafInit;

typedef char ScePafInitSizeCheck[sizeof(ScePafInit) == 0x18 ? 1 : -1];

extern SceUInt32 global_heap_size;
extern SceInt32 cdlg_mode;
extern SceInt32 global_heap_alignment;
extern SceInt32 global_heap_disable_assert_on_alloc_failure;

#endif
