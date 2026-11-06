/*
 * SceClipboard (Sony's clipboard.h): the system clipboard of UTF-16 text, which the shell keeps
 * (SceShellUtil's text clipboard). Setting it checks that the text holds no surrogate code unit
 * and tells the shell to show a notice.
 */
#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <clipboard.h>
#include <ces/libces_ucs.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include <appmgr/appmgr.h>
#include "../../../../../vsh/lowlayer/src/shell_svc/text_clipboard.h"

static int s_isInitialized;

int module_start(SceSize args, const void *argp)
{
	s_isInitialized = 1;
	return SCE_OK;
}

int module_stop(SceSize args, const void *argp)
{
	return SCE_OK;
}

int module_exit(SceSize args, const void *argp)
{
	return SCE_OK;
}

SceInt32 sceClipboardSetText(const SceWChar16 *newContents, SceUInt length)
{
	AppmgrNotificationRequest notification;
	const SceWChar16 *p;
	SceUInt i;
	int ret;

	if (newContents == NULL)
		return SCE_CLIPBOARD_ERROR_NULL_POINTER;
	if (length > SCE_CLIPBOARD_MAX_STRING_LENGTH)
		return SCE_CLIPBOARD_ERROR_TEXT_TOO_LONG;
	for (i = 0, p = newContents; i < length; i++, p++) {
		if (sceCesUcs2CheckCode(*p) != SCE_CES_OK)
			return SCE_CLIPBOARD_ERROR_UNSUPPORTED_CHARACTER;
	}
	ret = SceShellUtil_C4810C56(newContents, length);
	if (ret == SCE_OK) {
		sceClibMemset(&notification, 0, sizeof(notification));
		sceClibStrncpy((char *)notification.payload, "Clipboard:\nSet Text",
			       sizeof(notification.payload) - 1);
		sceAppMgrSendNotificationRequest(&notification, 0);
	}
	return ret;
}

SceInt32 sceClipboardGetText(SceWChar16 *buffer, SceUInt maxLength, SceUInt *pLength)
{
	if (buffer == NULL || pLength == NULL)
		return SCE_CLIPBOARD_ERROR_NULL_POINTER;
	return SceShellUtil_1B186905(buffer, maxLength, pLength);
}
