/*
 * SceShellSvc: module start and stop. The shell starts the library with a one-byte
 * argument that is non-zero; it creates the text clipboard and needs no ShellSvc client.
 */

#include <kernel/modulemgr.h>
#include "shellsvc.h"
#include "text_clipboard.h"

int module_start(SceSize argSize, const void *pArgBlock)
{
	bool isShell = false;

	if (argSize == 1)
		isShell = *(const bool *)pArgBlock;
	textClipboardInitialize(isShell);
	if (isShell)
		return SCE_KERNEL_START_SUCCESS;
	return SceShellSvc_E91AEAD4();
}

int module_stop(SceSize argSize, const void *pArgBlock)
{
	textClipboardFinalize();
	return shellSvcFinalize();
}

/*
 * A 4-byte SceIpmi variable nothing in the library reads (0x8101305c in .bss). Its other
 * SceIpmi variables are the IPMI classes' vtables, exported under their own names.
 */
int SceIpmi_9DA8C301;
