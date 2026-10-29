#include <kermit.h>

const char g_idStorageName[] = "SceIdStorage";

__attribute__((noipa)) int sceIdStorageInit(void);
__attribute__((noipa)) int sceIdStorageEnd(void);

int module_start(SceSize args, void *argp)
{
	sceIdStorageInit();
	return 0;
}

int module_reboot_before(SceSize args, void *argp)
{
	sceIdStorageEnd();
	return 0;
}

int sceIdStorageInit(void)
{
	return 0;
}

int sceIdStorageEnd(void)
{
	return 0;
}

int sceIdStorageGetLeafSize(void)
{
	return 0x200;
}

int sceIdStorageIsFormatted(void)
{
	return 1;
}

#define IDSTORAGE_UNIMPL(fn) \
int fn(void) \
{ \
	Kprintf("Not implemented (only PSP VSH usage) %s\n", #fn); \
	for (;;) { \
		sceKernelDelayThread(1000); \
	} \
}

IDSTORAGE_UNIMPL(sceIdStorageIsReadOnly)
IDSTORAGE_UNIMPL(sceIdStorageIsDirty)
IDSTORAGE_UNIMPL(sceIdStorageFormat)
IDSTORAGE_UNIMPL(sceIdStorageUnformat)
IDSTORAGE_UNIMPL(sceIdStorageReadLeaf)
IDSTORAGE_UNIMPL(sceIdStorageWriteLeaf)
IDSTORAGE_UNIMPL(sceIdStorageFlush)
IDSTORAGE_UNIMPL(sceIdStorageGetFreeLeaves)
IDSTORAGE_UNIMPL(sceIdStorageEnumId)
IDSTORAGE_UNIMPL(sceIdStorageCreateLeaf)
IDSTORAGE_UNIMPL(sceIdStorageCreateAtomicLeaves)
IDSTORAGE_UNIMPL(sceIdStorageDeleteLeaf)

int sceIdStorageLookup(u32 key, u32 offset, void *buf, u32 len)
{
	return sceKermitPeripheral_driver_C6F8B4E1(key & 0xFFFFu, offset, buf, len);
}

IDSTORAGE_UNIMPL(sceIdStorageUpdate)
