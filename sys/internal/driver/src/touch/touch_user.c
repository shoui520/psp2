/* SceTouch, SceTouchDummy: the user API (library SceTouch), each export a system call around the kernel's.
 * Private names without a recovered SDK name remain unknown.
 */
#include "private.h"

/* the kernel's reads land here before they are copied out, one mutex and buffer per kind */
struct userBuffers {
	int readMutex __attribute__((aligned(8)));
	SceTouchData readBuffer[64];
	int peekMutex;
	SceTouchData peekBuffer[64];
};

static struct userBuffers userBuffers;

int initializeUser(void) /* name unknown */
{
	userBuffers.peekMutex = -1;
	userBuffers.readMutex = -1;
	userBuffers.readMutex = sceKernelCreateMutex("SceTouch1", 0, 0, NULL);
	if (userBuffers.readMutex >= 0) {
		userBuffers.peekMutex = sceKernelCreateMutex("SceTouch2", 0, 0, NULL);
		if (userBuffers.peekMutex >= 0)
			return 0;
		if (userBuffers.readMutex >= 0)
			sceKernelDeleteMutex(userBuffers.readMutex);
	}
	if (userBuffers.peekMutex >= 0)
		sceKernelDeleteMutex(userBuffers.peekMutex);
	return SCE_TOUCH_ERROR_FATAL;
}

enum userReadKind {
	USER_READ,
	USER_PEEK,
	USER_READ_REGION,
	USER_PEEK_REGION,
	USER_READ_REGION_EXT,
	USER_PEEK_REGION_EXT,
};

/* the reads of the user API, inside their system call: the kernel's read into a buffer, then copied out */
static inline __attribute__((always_inline)) int userRead(unsigned port, void *out, unsigned count,
	unsigned regions, enum userReadKind kind) /* name unknown */
{
	SceTouchData *buffer;
	int *mutex, ret, copied, unlocked;

	if (!out || count - 1 > 63 || port > 1)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	if (kind == USER_READ || kind == USER_READ_REGION) {
		mutex = &userBuffers.readMutex;
		buffer = userBuffers.readBuffer;
	} else {
		mutex = &userBuffers.peekMutex;
		buffer = userBuffers.peekBuffer;
	}
	ret = sceKernelLockMutex(*mutex, 1, NULL);
	if (ret < 0)
		return ret;
	switch (kind) {
	case USER_READ:
		ret = sceTouchRead(port, buffer, count);
		break;
	case USER_PEEK:
		ret = sceTouchPeek(port, buffer, count);
		break;
	case USER_READ_REGION:
		ret = sceTouchReadRegion(port, buffer, count, regions);
		break;
	case USER_PEEK_REGION:
		ret = sceTouchPeekRegion(port, buffer, count, regions);
		break;
	case USER_READ_REGION_EXT:
		ret = readRegionExt(port, buffer, count, regions);
		break;
	default:
		ret = peekRegionExt(port, buffer, count, regions);
		break;
	}
	if (ret > 0) {
		copied = sceKernelCopyToUser(out, buffer, ret * sizeof(SceTouchData));
		if (copied < 0) {
			sceKernelUnlockMutex(*mutex, 1);
			return copied;
		}
	}
	unlocked = sceKernelUnlockMutex(*mutex, 1);
	if (unlocked < 0)
		return unlocked;
	return ret;
}

int sceTouchReadForUser(SceUInt32 port, SceTouchData *out, SceUInt32 count)
{
	unsigned t = sceSyscallEnter();
	int ret = userRead(port, out, count, 1, USER_READ);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchPeekForUser(SceUInt32 port, SceTouchData *out, SceUInt32 count)
{
	unsigned t = sceSyscallEnter();
	int ret = userRead(port, out, count, 1, USER_PEEK);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchReadRegionForUser(unsigned port, void *out, unsigned count, unsigned regions)
{
	unsigned t = sceSyscallEnter();
	int ret = userRead(port, out, count, regions, USER_READ_REGION);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchPeekRegionForUser(unsigned port, void *out, unsigned count, unsigned regions)
{
	unsigned t = sceSyscallEnter();
	int ret = userRead(port, out, count, regions, USER_PEEK_REGION);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchReadRegionExt(unsigned port, void *out, unsigned count, unsigned regions)
{
	unsigned t = sceSyscallEnter();
	int ret = userRead(port, out, count, regions, USER_READ_REGION_EXT);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchPeekRegionExt(unsigned port, void *out, unsigned count, unsigned regions)
{
	unsigned t = sceSyscallEnter();
	int ret = userRead(port, out, count, regions, USER_PEEK_REGION_EXT);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchSetSamplingStateForUser(SceUInt32 port, SceUInt32 state)
{
	unsigned t = sceSyscallEnter();
	int ret = sceTouchSetSamplingState(port, state);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchGetSamplingStateForUser(SceUInt32 port, SceUInt32 *out)
{
	unsigned value = 0, t;
	int ret;

	if (!out)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	t = sceSyscallEnter();
	ret = sceTouchGetSamplingState(port, &value);
	if (ret >= 0)
		ret = sceKernelCopyToUser(out, &value, sizeof(value));
	sceSyscallLeave(t);
	return ret;
}

int sceTouchSetSamplingStateExt(unsigned port, unsigned state)
{
	unsigned t = sceSyscallEnter();
	int ret = setSamplingStateExt(port, state);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchGetSamplingStateExt(unsigned port, void *out)
{
	unsigned value = 0, t;
	int ret;

	if (!out)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	t = sceSyscallEnter();
	ret = getSamplingStateExt(port, &value);
	if (ret >= 0)
		ret = sceKernelCopyToUser(out, &value, sizeof(value));
	sceSyscallLeave(t);
	return ret;
}

int sceTouchEnableTouchForceForUser(SceUInt32 port)
{
	unsigned t = sceSyscallEnter();
	int ret = sceTouchEnableTouchForce(port);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchEnableTouchForceExt(unsigned port)
{
	unsigned t = sceSyscallEnter();
	int ret = enableTouchForceExt(port);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchDisableTouchForceForUser(SceUInt32 port)
{
	unsigned t = sceSyscallEnter();
	int ret = sceTouchDisableTouchForce(port);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchDisableTouchForceExt(unsigned port)
{
	unsigned t = sceSyscallEnter();
	int ret = disableTouchForceExt(port);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchGetPanelInfoForUser(SceUInt32 port, SceTouchPanelInfo *out)
{
	SceTouchPanelInfo info;
	unsigned t;
	int ret;

	if (!out)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	t = sceSyscallEnter();
	ret = sceTouchGetPanelInfo(port, &info);
	if (ret >= 0)
		ret = sceKernelCopyToUser(out, &info, sizeof(info));
	sceSyscallLeave(t);
	return ret;
}

int sceTouchGetPixelDensity(void *front, void *back)
{
	float frontDensity = 0, backDensity = 0;
	unsigned t = sceSyscallEnter();
	int ret;

	if (!back || !front) {
		sceSyscallLeave(t);
		return SCE_TOUCH_ERROR_INVALID_ARG;
	}
	ret = getPixelDensity(&frontDensity, &backDensity);
	if (ret >= 0) {
		ret = sceKernelCopyToUser(front, &frontDensity, sizeof(float));
		if (ret >= 0)
			ret = sceKernelCopyToUser(back, &backDensity, sizeof(float));
	}
	sceSyscallLeave(t);
	return ret;
}

int sceTouchGetPixelDensity2(void *frontX, void *frontY, void *backX, void *backY)
{
	float frontDensityX = 0, backDensityX = 0, frontDensityY = 0, backDensityY = 0;
	unsigned t = sceSyscallEnter();
	int ret;

	if (!frontY || !frontX || !backY || !backX) {
		sceSyscallLeave(t);
		return SCE_TOUCH_ERROR_INVALID_ARG;
	}
	ret = getPixelDensity2(&frontDensityX, &frontDensityY, &backDensityX, &backDensityY);
	if (ret >= 0) {
		ret = sceKernelCopyToUser(frontX, &frontDensityX, sizeof(float));
		if (ret >= 0) {
			ret = sceKernelCopyToUser(frontY, &frontDensityY, sizeof(float));
			if (ret >= 0) {
				ret = sceKernelCopyToUser(backX, &backDensityX, sizeof(float));
				if (ret >= 0)
					ret = sceKernelCopyToUser(backY, &backDensityY, sizeof(float));
			}
		}
	}
	sceSyscallLeave(t);
	return ret;
}

int sceTouchSetRegionForUser(unsigned port, unsigned region, const void *rect, unsigned attr)
{
	short copy[4];
	unsigned t;
	int ret;

	if (!rect)
		return SCE_TOUCH_ERROR_INVALID_ARG;
	t = sceSyscallEnter();
	ret = sceKernelCopyFromUser(copy, rect, sizeof(copy));
	if (!ret)
		ret = sceTouchSetRegion(port, region, copy, attr);
	sceSyscallLeave(t);
	return ret;
}

int sceTouchClearRegion(unsigned port, unsigned region)
{
	unsigned t = sceSyscallEnter();
	int ret = SceTouchForDriver_E1C5D95F(port, region);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchActivateRegionForUser(unsigned port, unsigned region, unsigned enable)
{
	unsigned t = sceSyscallEnter();
	int ret = sceTouchActivateRegion(port, region, enable);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchSetRegionAttrForUser(unsigned port, unsigned region, unsigned attr)
{
	unsigned t = sceSyscallEnter();
	int ret = sceTouchSetRegionAttr(port, region, attr);

	sceSyscallLeave(t);
	return ret;
}

int sceTouchSetProcessPrivilege(unsigned privilege)
{
	unsigned t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_TOUCH_ERROR_PRIV_REQUIRED;
	}
	ret = setProcessPrivilege(privilege);
	sceSyscallLeave(t);
	return ret;
}

int sceTouchGetProcessInfoForUser(unsigned port, void *out)
{
	struct touchProcessInfo info;
	unsigned t = sceSyscallEnter();
	int ret;

	if (!out) {
		sceSyscallLeave(t);
		return SCE_TOUCH_ERROR_INVALID_ARG;
	}
	ret = sceTouchGetProcessInfo(port, &info);
	if (ret >= 0)
		ret = sceKernelCopyToUser(out, &info, sizeof(info));
	sceSyscallLeave(t);
	return ret;
}

int sceTouchEnableIdleTimerCancelSettingForUser(unsigned port, int enable)
{
	unsigned t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_TOUCH_ERROR_PRIV_REQUIRED;
	}
	ret = sceTouchEnableIdleTimerCancelSetting(port, enable);
	sceSyscallLeave(t);
	return ret;
}

int sceTouchGetDeviceInfoForUser(unsigned port, void *out)
{
	struct touchDeviceInfo info;
	unsigned t = sceSyscallEnter();
	int ret;

	if (!sceSblACMgrIsSystemProgram(0)) {
		sceSyscallLeave(t);
		return SCE_TOUCH_ERROR_PRIV_REQUIRED;
	}
	ret = sceTouchGetDeviceInfo(port, &info);
	if (ret >= 0)
		ret = sceKernelCopyToUser(out, &info, sizeof(info));
	sceSyscallLeave(t);
	return ret;
}

int sceTouchRead2(unsigned device, unsigned port, void *out, unsigned count)
{
	unsigned t = sceSyscallEnter();
	int ret;

	if (device)
		ret = readVirtualWait(device, port, out, count);
	else
		ret = userRead(port, out, count, 1, USER_READ);
	sceSyscallLeave(t);
	return ret;
}

int sceTouchPeek2(unsigned device, unsigned port, void *out, unsigned count)
{
	unsigned t = sceSyscallEnter();
	int ret;

	if (device)
		ret = readVirtualPeek(device, port, out, count);
	else
		ret = userRead(port, out, count, 1, USER_PEEK);
	sceSyscallLeave(t);
	return ret;
}
