/*
 * SceLibMp4Recorder placement new helper.
 */
#include "mp4rec_internal.h"

void *operator new(size_t size, void *ptr)
{
	(void)size;
	return ptr;
}
