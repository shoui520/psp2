/*
 * swagner's key-bin bridge: thin wrappers between DTCPIP-MW and
 * SWKeyManager (libswkeystore), mapping key-manager status codes into the 13001..13019
 * middleware status space.
 */

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>

/* global key-bin handle and cached 64-bit key-store values */
int g_swKeyBinHandle;
static unsigned long long g_swKeyBinDeviceId = 31556926ULL;
static unsigned long long g_swKeyBinSeed;

/* SWKeyManager/KeyManager/libkm/libswkeystore.c entry points */
int swKeyStoreOpen(int *outHandle);
int swKeyStoreWriteCert(int handle, const void *data, int type);
int swKeyStoreImportParamTbl(const void *data, int size);
int swKeyStoreReadBlock(int handle, void *buffer, int size, int *outSize);
int swKeyStoreWriteBlock(int handle, const void *buffer, int size);
int swKeyStoreReadRecord(int handle, void *buffer, int size, int *outSize);
int swKeyStoreWriteRecord(int handle, const void *buffer, int size, int type);
int swKeyStoreClose(int handle);
int swKeyStoreReadExtra(int handle, void *buffer, int size, int *outSize);
int swKeyStoreGetPort(unsigned short *outPort);
int kmRemoveParamTbl(void);
void swKeyStoreGetDeviceId(unsigned long long *outId);
void swKeyStoreGetTimeOffset(unsigned long long *outOffset);

/* certificate descriptor passed to SwMwCoreKeyBinWrite (SwMwManager.h) */
struct SwMwKeyBinCertDesc {
	void *templatePtr;		/* 0x00 */
	unsigned char device[3];	/* 0x04 */
	unsigned char padding[129];	/* 0x07 */
	int type;			/* 0x88 */
	int size;			/* 0x8c */
};

/* map SWKeyManager status codes to middleware status codes */
int SwMwKeyStatusMap(int status)
{
	switch (status) {
	case 0:
		return 0;
	case 1:
		return 13016;
	case 13001:
		return 13001;
	case 13002:
		return 13002;
	case 13003:
		return 13003;
	case 13004:
		return 13004;
	case 13005:
		return 13017;
	case 13101:
		return 13018;
	case 13102:
	case 13103:
	case 13104:
		return 13019;
	case 13201:
		return 13005;
	case 13202:
		return 13006;
	case 13203:
		return 13005;
	case 13204:
		return 13008;
	case 13205:
		return 13009;
	case 13206:
		return 13010;
	case 13207:
		return 13011;
	case 13208:
		return 13012;
	case 13209:
		return 13017;
	case 13210:
		return 13014;
	case 13211:
		return 13015;
	default:
		return 13005;
	}
}

/* cached 64-bit key-store identifiers */
unsigned long long SwMwKeyBinGetDeviceId(void)
{
	return g_swKeyBinDeviceId;
}

unsigned long long SwMwKeyBinGetSeed(void)
{
	return g_swKeyBinSeed;
}

/* open the key store and cache its identifiers */
int SwMwCoreKeyBinCreate(void **outKeyBin)
{
	int handle = 0;
	int status;

	status = swKeyStoreOpen(&handle);
	if (status == 0) {
		swKeyStoreGetDeviceId(&g_swKeyBinDeviceId);
		swKeyStoreGetTimeOffset(&g_swKeyBinSeed);
		*outKeyBin = (void *)(uintptr_t)handle;
		g_swKeyBinHandle = handle;
	}
	return SwMwKeyStatusMap(status);
}

/* close the key store */
int SwMwManagerCloseKeyBin(void *keyBin)
{
	int status;

	status = swKeyStoreClose((int)(uintptr_t)keyBin);
	g_swKeyBinHandle = 0;
	return SwMwKeyStatusMap(status);
}

/* import a downloaded parameter table */
int SwMwCoreImportKeyBin(const void *data, int size)
{
	int status = swKeyStoreImportParamTbl(data, size);

	return SwMwKeyStatusMap(status);
}

/* read the key store's key ID / port word */
int SwMwCoreGetPort(unsigned short *port)
{
	int status = swKeyStoreGetPort(port);

	return SwMwKeyStatusMap(status);
}

/* reset key-store state */
int SwMwCoreInitKey(void)
{
	int status = kmRemoveParamTbl();

	return SwMwKeyStatusMap(status);
}

/* read a key-bin block */
int SwMwCoreKeyBinReadBlock(void *keyBin, void *buffer, int size, int *outSize)
{
	int status = swKeyStoreReadBlock((int)(uintptr_t)keyBin, buffer, size, outSize);

	return SwMwKeyStatusMap(status);
}

/* write a key-bin block */
int SwMwCoreKeyBinWriteBlock(void *keyBin, const void *buffer, int size)
{
	int status = swKeyStoreWriteBlock((int)(uintptr_t)keyBin, buffer, size);

	return SwMwKeyStatusMap(status);
}

/* read a stored SRM/record block */
int SwMwCoreKeyBinRead(void *keyBin, void *buffer, int size, int *outSize)
{
	int status = swKeyStoreReadRecord((int)(uintptr_t)keyBin, buffer, size, outSize);

	return SwMwKeyStatusMap(status);
}

/* write an SRM/record block */
int SwMwCoreRecordWrite(void *keyBin, const void *buffer, int size, int type)
{
	int status = swKeyStoreWriteRecord((int)(uintptr_t)keyBin, buffer, size, type);

	return SwMwKeyStatusMap(status);
}

/* read an extra key-bin record */
int SwMwCoreKeyBinReadExtra(void *keyBin, void *buffer, int size, int *outSize)
{
	int status;

	(void)keyBin;
	status = swKeyStoreReadExtra(0, buffer, size, outSize);
	return SwMwKeyStatusMap(status);
}

/* write the device certificate descriptor to the key store */
int SwMwCoreKeyBinWrite(void *keyBin, const struct SwMwKeyBinCertDesc *desc, int size)
{
	int status;

	(void)size;
	if (desc->type == 0)
		return 0;
	status = swKeyStoreWriteCert((int)(uintptr_t)keyBin, desc->device, desc->type);
	return SwMwKeyStatusMap(status);
}
