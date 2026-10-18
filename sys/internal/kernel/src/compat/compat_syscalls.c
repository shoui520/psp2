/* Public PSP emulator syscalls; source filename inferred. */

#include <compat/compat.h>
#include <stddef.h>
#include <acmgr/acmgr.h>
#include <npdrm/npdrm.h>
#include <intrmgr/syscall.h>
#include <sysmem/sysmem.h>
#include <rtc/rtc.h>

int sceCompatCacheInternal(int mode, void *base, int size);
int compatAllocCdramWithHole(SceCompatCdram *cdram);
int compatFrameBufferInit(SceUID frameBuffer);
int compatIdStorageLookup(short leaf, unsigned int offset, void *buffer,
	unsigned int size);
int compatReadShared32(int location, int *value);
int compatWriteSharedCtrl(const SceCtrlDataPsp *padData);
int compatWriteShared32(int location, int value);
int compatSetDisplayConfig(int resolution, int colorSpaceMode);
int compatGetPeripheralState(int mode);
int compatDatRead(int index, unsigned int offset, void *buffer,
	unsigned int size);
int compatGetPspSystemSoftwareVersion(void);
int compatSuspendResume(int mode);
int compatGetDevInf(SceIoDevInfo *info);
int compatGetStatus(void);
int compatSetRifPath(const signed char *contentId);
int compatLCDCSync(void);
int compatSetSuspendSema(SceUID suspendSema, SceUID resumeSema);
int compatGetUpdateState(void);
int compatSetUpdateState(int state);
int compatGetTitleListForVsh(char *buffer, unsigned int size);
int compatAvailableColorSpaceSetting(void);
int compatCheckPocketStationRif(void);
extern unsigned char gCompatActivationReadBuffer[2048];
extern char gCompatTitleListBuffer[100];

int sceCompatCache(int mode, void *addr, SceSize size)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = sceCompatCacheInternal(mode, addr, size);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatAllocCdramWithHole(SceCompatCdram *cdram)
{
	SceCompatCdram localCdram;
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatAllocCdramWithHole(&localCdram);
		if (result >= 0)
			result = SceSysmemForDriver_6D88EF8A(cdram, &localCdram,
				sizeof(localCdram));
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatFrameBufferInit(void *framebuffer)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatFrameBufferInit((SceUID)framebuffer);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatIdStorageLookup(short leaf, unsigned int offset, void *buffer,
	unsigned int size)
{
	unsigned char localBuffer[512];
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		if (size >= sizeof(localBuffer))
			size = sizeof(localBuffer);
		if (offset >= sizeof(localBuffer) ||
		    size + offset > sizeof(localBuffer)) {
			sceSyscallLeave(syscall);
			return (int)0x8002000b;
		}
		result = compatIdStorageLookup(leaf, offset, localBuffer, size);
		if (result >= 0)
			result = SceSysmemForDriver_6D88EF8A(buffer, localBuffer, size);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatReadShared32(int location, int *value)
{
	int localValue;
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatReadShared32(location, &localValue);
		if (result >= 0)
			result = SceSysmemForDriver_6D88EF8A(value, &localValue,
				sizeof(localValue));
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatWriteSharedCtrl(SceCtrlDataPsp *pad_data)
{
	SceCtrlDataPsp localData;
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = SceSysmemForDriver_BC996A7A(&localData, pad_data,
			offsetof(SceCtrlDataPsp, Rsrv));
		if (result >= 0)
			result = compatWriteSharedCtrl(&localData);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatWriteShared32(int location, int value)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatWriteShared32(location, value);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatSetDisplayConfig(int resolution, int colorSpaceMode)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatSetDisplayConfig(resolution, colorSpaceMode);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatGetPeripheralState(int mode)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatGetPeripheralState(mode);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatDatRead(int index, unsigned int offset, void *buffer,
	unsigned int size)
{
	SceUInt32 syscall;
	int chunk;
	void *destination;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) != 1) {
		sceSyscallLeave(syscall);
		return (int)0x80010058;
	}
	if (index != 0) {
		sceSyscallLeave(syscall);
		return (int)0x80000100;
	}
	if (size <= sizeof(SceNpDrmActivationData) &&
	    offset < sizeof(SceNpDrmActivationData) &&
	    offset + size <= sizeof(SceNpDrmActivationData)) {
		do {
			if (size >= sizeof(gCompatActivationReadBuffer))
				chunk = sizeof(gCompatActivationReadBuffer);
			else
				chunk = size;
			result = compatDatRead(0, offset, gCompatActivationReadBuffer,
				chunk);
			destination = buffer;
			offset += chunk;
			buffer = (unsigned char *)buffer + chunk;
			if (result < 0)
				break;
			result = SceSysmemForDriver_6D88EF8A(destination,
				gCompatActivationReadBuffer, chunk);
			if (result < 0)
				break;
			size -= chunk;
		} while (size != 0);
	} else {
		sceSyscallLeave(syscall);
		return (int)0x8002000b;
	}
	sceSyscallLeave(syscall);
	return result;
}

int sceCompatGetPspSystemSoftwareVersion(void)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (sceSblACMgrIsSystemProgram(0) == 1) {
		result = compatGetPspSystemSoftwareVersion();
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatSuspendResume(int mode)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatSuspendResume(mode);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatGetDevInf(SceIoDevInfo *info)
{
	SceIoDevInfo localInfo;
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatGetDevInf(&localInfo);
		if (result == 0)
			result = SceSysmemForDriver_6D88EF8A(info, &localInfo,
				sizeof(localInfo));
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatGetStatus(void)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatGetStatus();
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatSetRif(const char *rif)
{
	signed char contentId[36];
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = SceSysmemForDriver_BC996A7A(contentId, rif, sizeof(contentId));
		if (result >= 0)
			result = compatSetRifPath(contentId);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatLCDCSync(void)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatLCDCSync();
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatSetSuspendSema(SceUID semaid1, SceUID semaid2)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatSetSuspendSema(semaid1, semaid2);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatGetUpdateState(void)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatGetUpdateState();
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatSetUpdateState(int state)
{
	SceUInt32 syscall;
	int result;

	(void)state;
	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatSetUpdateState(1);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatGetCurrentSecureTick(SceRtcTick *tick)
{
	SceRtcTick localTick;
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = SceRtcForDriver_401C0954(&localTick);
		if (result >= 0)
			result = SceSysmemForDriver_6D88EF8A(tick, &localTick,
				sizeof(localTick));
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatGetTitleList(void *buf, SceSize length)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (sceSblACMgrIsSystemProgram(0) == 1) {
		if (length != sizeof(gCompatTitleListBuffer)) {
			sceSyscallLeave(syscall);
			return (int)0x8002000b;
		}
		result = compatGetTitleListForVsh(gCompatTitleListBuffer, length);
		if (result == 0)
			result = SceSysmemForDriver_6D88EF8A(buf, gCompatTitleListBuffer,
				length);
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatAvailableColorSpaceSetting(void)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatAvailableColorSpaceSetting();
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}

int sceCompatCheckPocketStation(void)
{
	SceUInt32 syscall;
	int result;

	syscall = sceSyscallEnter();
	if (SceSblACMgrForDriver_FD00C72A(0) == 1) {
		result = compatCheckPocketStationRif();
		sceSyscallLeave(syscall);
		return result;
	}
	sceSyscallLeave(syscall);
	return (int)0x80010058;
}
