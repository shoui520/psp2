/*
 * Clipboard validation and ShellUtil forwarding.
 * Text consists of UTF-16 code units; isolated surrogate units are rejected.
 * ShellUtil entry names retain NIDs until their public names are established.
 */
#include <stdint.h>
#include <stddef.h>

#define CLIPBOARD_INVALID_POINTER	0x80104601U
#define CLIPBOARD_INVALID_LENGTH	0x80104602U
#define CLIPBOARD_INVALID_TEXT		0x80104603U
#define CLIPBOARD_MAX_LENGTH		0x1ffdU

extern int SceShellUtil_C4810C56(const uint16_t *text, uint32_t length);
extern int SceShellUtil_1B186905(uint16_t *text, uint32_t capacity,
			      uint32_t *length);
extern void *sceClibMemset(void *buffer, int value, size_t size);
extern char *sceClibStrncpy(char *destination, const char *source, size_t size);
extern int sceAppMgrSendNotificationRequest(const void *request, uint32_t flags);

static volatile uint32_t clipboardInitialized;

int module_start(uint32_t argumentSize, void *arguments)
{
	clipboardInitialized = 1;
	return 0;
}

int module_stop(uint32_t argumentSize, void *arguments)
{
	return 0;
}

int module_exit(uint32_t argumentSize, void *arguments)
{
	return 0;
}

static int isSurrogateCodeUnit(uint16_t codeUnit)
{
	return codeUnit >= 0xd800 && codeUnit <= 0xdfff;
}

int sceClipboardSetText(const uint16_t *text, uint32_t length)
{
	uint8_t notification[64];
	uint32_t index;
	int result;

	if (!text)
		return (int)CLIPBOARD_INVALID_POINTER;
	if (length > CLIPBOARD_MAX_LENGTH)
		return (int)CLIPBOARD_INVALID_LENGTH;
	for (index = 0; index < length; index++) {
		if (isSurrogateCodeUnit(text[index]))
			return (int)CLIPBOARD_INVALID_TEXT;
	}
	result = SceShellUtil_C4810C56(text, length);
	if (result)
		return result;
	sceClibMemset(notification, 0, sizeof(notification));
	sceClibStrncpy((char *)notification + 4, "Clipboard:\nSet Text", 59);
	sceAppMgrSendNotificationRequest(notification, 0);
	return result;
}

int sceClipboardGetText(uint16_t *text, uint32_t capacity, uint32_t *length)
{
	if (!text || !length)
		return (int)CLIPBOARD_INVALID_POINTER;
	return SceShellUtil_1B186905(text, capacity, length);
}
