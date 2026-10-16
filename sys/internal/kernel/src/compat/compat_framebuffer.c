/* Framebuffer object lifetime; source filename inferred. */

#include <psp2kern/types.h>
#include <psp2kern/kernel/sysmem/uid_guid.h>

extern SceUID gCompatFrameBuffer;

int compatReleaseFrameBuffer(void)
{
	ksceGUIDReleaseObject(gCompatFrameBuffer);
	gCompatFrameBuffer = 0;
	return 0;
}
