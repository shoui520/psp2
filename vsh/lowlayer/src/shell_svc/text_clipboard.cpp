#include <libkernel/libkernel.h>
#include <kernel/processmgr.h>
#include <sysmem/memblock.h>
#include <sysmem/sysmem.h>
#include "shell_util.h"
#include "text_clipboard.h"

/*
 * The text clipboard: one UTF-16 string in a memory block the shell creates
 * ("SceTextClipboardStorage") and every other process opens, under a semaphore. Sony's
 * messages name it [TextClipboard]; the file and function names are ours.
 */

#define TEXT_CLIPBOARD_SEMA_NAME	"SceTextClipboardSema"
#define TEXT_CLIPBOARD_STORAGE_NAME	"SceTextClipboardStorage"
#define TEXT_CLIPBOARD_STORAGE_SIZE	0x4000
#define TEXT_CLIPBOARD_MAX_LENGTH	0x1FFD

/* The memory block: the length in characters, then the terminated text. */
struct TextClipboardStorage {
	SceUInt32 length;
	SceWChar16 text[(TEXT_CLIPBOARD_STORAGE_SIZE - sizeof(SceUInt32)) / sizeof(SceWChar16)];
};

static SceUID s_semaId = -1;
static SceUID s_memBlockId = -1;
static SceWChar16 *s_pText = 0;
static TextClipboardStorage *s_pStorage = 0;

int SceShellUtil_C4810C56(const SceWChar16 *pText, SceUInt32 length)
{
	if (s_pText == NULL)
		return SHELL_UTIL_ERROR_NO_STORAGE;
	if (length > TEXT_CLIPBOARD_MAX_LENGTH)
		return SHELL_UTIL_ERROR_TOO_LONG;
	if (sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT) != 0)
		__breakpoint(0);
	if (sceKernelWaitSema(s_semaId, 1, NULL) != 0)
		__breakpoint(0);
	sceClibMemcpy_safe(s_pText, pText, length * sizeof(SceWChar16));
	s_pText[length] = 0;
	s_pStorage->length = length;
	if (sceKernelSignalSema(s_semaId, 1) != 0)
		__breakpoint(0);
	if (sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT) != 0)
		__breakpoint(0);
	return 0;
}

int SceShellUtil_1B186905(SceWChar16 *pText, SceUInt32 maxLength, SceUInt32 *pLength)
{
	SceUInt32 length;

	if (s_pText == NULL)
		return SHELL_UTIL_ERROR_NO_STORAGE;
	if (sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT) != 0)
		__breakpoint(0);
	if (sceKernelWaitSema(s_semaId, 1, NULL) != 0)
		__breakpoint(0);
	length = s_pStorage->length;
	if (length <= maxLength)
		maxLength = length;
	sceClibMemcpy_safe(pText, s_pText, maxLength * sizeof(SceWChar16));
	pText[maxLength] = 0;
	if (pLength != NULL)
		*pLength = maxLength;
	if (sceKernelSignalSema(s_semaId, 1) != 0)
		__breakpoint(0);
	if (sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT) != 0)
		__breakpoint(0);
	return 0;
}

int SceShellUtil_D0DDEDBC(void)
{
	SceUInt32 length;

	if (sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT) != 0)
		__breakpoint(0);
	if (sceKernelWaitSema(s_semaId, 1, NULL) != 0)
		__breakpoint(0);
	length = s_pStorage->length;
	if (sceKernelSignalSema(s_semaId, 1) != 0)
		__breakpoint(0);
	if (sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT) != 0)
		__breakpoint(0);
	return length;
}

/* The shell creates the storage; every other process opens it. */
int textClipboardInitialize(bool isShell)
{
	MemBlockUserOpt opt;
	void *pBase;
	SceUID uid;
	SceUInt32 i;
	int ret;

	if (isShell) {
		ret = sceKernelCreateSema(TEXT_CLIPBOARD_SEMA_NAME, 0x2080, 1, 1, NULL);
		if (ret < 0)
			return ret;
		s_semaId = ret;
		sceClibMemset(&opt, 0, sizeof(opt));
		opt.size = sizeof(opt);
		opt.attr = ALLOC_ATTR_CLIENT_INFO | ALLOC_ATTR_HAS_CLIENT_INFO;
		opt.openFlags = MEMBLOCK_OPEN_RW;
		for (i = 0; i < 8; i++)
			opt.capabilities[i] = 0;
		uid = sceKernelAllocMemBlock(TEXT_CLIPBOARD_STORAGE_NAME,
					     SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
					     TEXT_CLIPBOARD_STORAGE_SIZE,
					     (const SceKernelAllocMemBlockOpt *)&opt);
		if (uid < 0) {
			sceKernelDeleteSema(s_semaId);
			s_semaId = -1;
			return uid;
		}
		s_memBlockId = uid;
		if (sceKernelGetMemBlockBase(uid, &pBase) != 0)
			__breakpoint(0);
		sceClibMemset(pBase, 0, TEXT_CLIPBOARD_STORAGE_SIZE);
		uid = s_memBlockId;
	} else {
		ret = sceKernelOpenSema(TEXT_CLIPBOARD_SEMA_NAME);
		if (ret < 0) {
			sceClibPrintf("[TextClipboard] *WARNING* The service is unavailable.\n");
			return ret;
		}
		s_semaId = ret;
		uid = sceKernelOpenMemBlock(TEXT_CLIPBOARD_STORAGE_NAME, MEMBLOCK_OPEN_RW);
		if (uid <= 0)
			__breakpoint(0);
		s_memBlockId = uid;
	}
	if (sceKernelGetMemBlockBase(uid, (void **)&s_pStorage) != 0)
		__breakpoint(0);
	s_pText = s_pStorage->text;
	return 0;
}

int textClipboardFinalize(void)
{
	SceUID uid;

	uid = s_semaId;
	if (uid >= 0) {
		s_semaId = -1;
		sceKernelCloseSema(uid);
	}
	uid = s_memBlockId;
	if (uid >= 0) {
		s_memBlockId = -1;
		sceKernelCloseMemBlock(uid);
	}
	s_pText = NULL;
	s_pStorage = NULL;
	return 0;
}
