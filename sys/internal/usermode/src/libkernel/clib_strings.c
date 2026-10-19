#include <libkernel/libkernel.h>
#include <sysmem/compiler.h>

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
