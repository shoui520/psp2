/* Native ScePafGraphics globals. */
#include <stdint.h>
#include "../compiler.h"
#include "../std/string.h"
#include "../std/new.h"
#include "../thread/mutex.h"
#include <gxm.h>
#include <arm_neon.h>
#include <sce_atomic.h>
#include <kernel/sysmem.h>
#include <display.h>

/* A global graphics pointer uses the SDK's acquire/release atomic counter. */
uint32_t *sce_paf_graphics_shared_reference_0f84;

uint32_t sce_paf_graphics_state_11e0;
uint32_t sce_paf_graphics_state_11fc;
uint8_t sce_paf_graphics_flag_06e8;
RMutex sce_paf_graphics_rmutex_1230;
void *sce_paf_graphics_selected_object_12a4;

typedef struct PafGraphicsRegistryAnchor {
	void *previous;
	void *next;
} PafGraphicsRegistryAnchor;

PafGraphicsRegistryAnchor sce_paf_graphics_registry_1270;
Mutex sce_paf_graphics_registry_mutex_1278;

typedef struct PafGraphicsItemNode {
	struct PafGraphicsItemNode *previous;
	struct PafGraphicsItemNode *next;
	void *value;
} PafGraphicsItemNode;

PafGraphicsItemNode *sce_paf_graphics_items_12a0;
Mutex sce_paf_graphics_items_mutex_12b0;

Mutex sce_paf_graphics_flag_mutex_1250;

uint32_t *sce_paf_graphics_current_ref_0f7c;
