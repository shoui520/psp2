/* Shared exception context definitions. */
#pragma once

/* copy the frame onto the exception stack (the stub already stored d0-d31) */
void excpCopyFrame(SceExcpmgrContext *frame, const SceExcpmgrContext *from)
{
	const unsigned int *src = (const unsigned int *)from;
	unsigned int *dst = (unsigned int *)frame;
	unsigned int i;

	for (i = 0; i < 0x100 / 4; i++)
		dst[i] = src[i];
	memcpy(frame->waypoint, from->waypoint, sizeof(frame->waypoint));
}

