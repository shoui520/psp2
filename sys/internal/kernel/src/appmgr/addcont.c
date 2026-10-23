/* Additional content. */

#include <scetypes.h>
#include <threadmgr/threadmgr.h>
#include "process_content_internal.h"
#include "addcont_internal.h"

extern SceUID g_appmgrAppMutex;

/* Whether the content's license may be read again (content flag 0x1000000). */
int appmgrCheckRifUpdatable(const AppmgrProcessContent *content)
{
	int result;

	if (content == NULL)
		return (int)0x80800001;
	result = sceKernelLockMutex(g_appmgrAppMutex, 1, NULL);
	if (result >= 0) {
		result = (int)0x80800042;
		if ((content->contentFlags & 0x1000000) != 0)
			result = 0;
		sceKernelUnlockMutex(g_appmgrAppMutex, 1);
	}
	return result;
}
