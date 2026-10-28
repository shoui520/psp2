#include "frame_count.h"

static uint32_t s_frame_count;

void IncrFrameCount(void)
{
	++s_frame_count;
}

void ResetFrameCount(void)
{
	s_frame_count = 0;
}

uint32_t FrameCount(void)
{
	return s_frame_count;
}
