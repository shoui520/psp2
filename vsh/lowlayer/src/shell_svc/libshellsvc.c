/*
 * SceShellSvc: module start and stop. The shell starts the library with a one-byte
 * argument that is non-zero; it creates the text clipboard and needs no ShellSvc client.
 * The SceIpmi variables are still skeleton placeholders.
 */

#include <skeleton.h>
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

STUB_VAR(SceIpmi_1B0607E7)
STUB_VAR(SceIpmi_28CF9138)
STUB_VAR(SceIpmi_41D521B4)
STUB_VAR(SceIpmi_7DEDA068)
STUB_VAR(SceIpmi_89E30D08)
STUB_VAR(SceIpmi_9DA8C301)
STUB_VAR(SceIpmi_C4379635)
