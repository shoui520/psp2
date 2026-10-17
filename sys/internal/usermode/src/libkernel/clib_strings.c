#include <psp2/kernel/clib.h>
#include <sysmem/compiler.h>

extern SceSize sceClibStrlcpy(char *destination, const char *source,
		SceSize capacity);
extern SceSize sceClibStrlcat(char *destination, const char *source,
		SceSize capacity);

SceSize sceClibStrlcpyChk(char *destination, const char *source, SceSize count,
		SceSize capacity)
{
	if (capacity < count)
		__breakpoint(BKPT_CHECK_FAILED);
	return sceClibStrlcpy(destination, source, count);
}

SceSize sceClibStrlcatChk(char *destination, const char *source, SceSize count,
		SceSize capacity)
{
	if (capacity < count)
		__breakpoint(BKPT_CHECK_FAILED);
	return sceClibStrlcat(destination, source, count);
}
