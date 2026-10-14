/*
 * fw2 runtime: the rings, fw1's (fw1/common/ring.h) as FW2 changed them. FW2
 * opens two readers at most, and its writer waits only for the first: what the
 * second reader has not read yet may be written over. The range checks of fw1's
 * cursor lookup and of the writer's functions are gone.
 */
#pragma once

#include "../../fw1/common/ring.h"

#define VIP_RING_READERS	2	/* the cursors FW2 opens: the writer's, then these */
#define VIP_RING_FIRST_READER	1	/* the reader the writer waits for */

int getRingWritableBeforeFirstReader(VipRingHandle *handle, VipRingSegment *segments,
	uint32_t count);
int getRingSpaceBeforeFirstReader(VipRingHandle *handle);
int getRingSpaceBeforeReader(VipRingHandle *handle);
int isRingSpaceEmpty(VipRingHandle *handle, int space);
uint32_t getRingWriterPosition(VipRingHandle *handle);
