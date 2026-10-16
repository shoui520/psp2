/* Registry-backed system parameters exposed by the application manager. */

#include <psp2kern/kernel/acmgr.h>
#include <psp2kern/kernel/threadmgr/mutex.h>
#include <sce/regmgr.h>
#include <sce/syscall.h>
#include <sce/threadmgr.h>
#include <stddef.h>
#include <string.h>

extern int SceSblACMgrForDriver_0924896F(SceUID pid);
extern int ksceRegMgrSystemParamGetStr(int id, char *buffer, SceSize size);
extern SceUID g_appmgrAppMutex;

/* The configuration is stored and transferred as a complete opaque value. */
typedef struct AppmgrDateTimeConf {
	unsigned char bytes[20];
} AppmgrDateTimeConf;

static AppmgrDateTimeConf dateTimeConf = { { 0 } };

int appmgrSystemParamDateTimeGetConf(void *conf)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result = ksceKernelLockMutex(g_appmgrAppMutex, 1, NULL);

	if (result >= 0) {
		if (conf != NULL)
			*(AppmgrDateTimeConf *)conf = dateTimeConf;
		else
			result = (int)0x80801001;
		ksceKernelUnlockMutex(g_appmgrAppMutex, 1);
	}
	sceSyscallLeave(syscallState);
	return result;
}

int ksceAppMgrSystemParamDateTimeSetConf(const void *conf)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result = ksceKernelLockMutex(g_appmgrAppMutex, 1, NULL);

	if (result >= 0) {
		dateTimeConf = *(const AppmgrDateTimeConf *)conf;
		ksceKernelUnlockMutex(g_appmgrAppMutex, 1);
	}
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrSystemParamGetInt(int parameter, int *value)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (ksceSblACMgrIsGameProgram(0)) {
		result = (int)0x80800009;
		goto out;
	}
	if (value == NULL) {
		result = (int)0x80800001;
		goto out;
	}
	*value = 0;
	switch (parameter) {
	case 2:
		result = ksceRegMgrSystemParamGetInt(0x008e3939, value);
		break;
	case 4:
		result = ksceRegMgrSystemParamGetInt(0x00971fa1, value);
		break;
	case 5:
		result = ksceRegMgrSystemParamGetInt(0x002fdfb4, value);
		break;
	case 7:
		result = ksceRegMgrSystemParamGetInt(0x00023fc2, value);
		break;
	case 8:
		result = ksceRegMgrSystemParamGetInt(0x00611dc9, value);
		break;
	case 9:
		result = ksceRegMgrSystemParamGetInt(0x008a2ad7, value);
		break;
	default:
		result = (int)0x80800001;
		break;
	}
out:
	sceSyscallLeave(syscallState);
	return result;
}

int appmgrSystemParamGetString(int parameter, char *value, SceSize size)
{
	SceUInt32 syscallState = sceSyscallEnter();
	int result;

	if (ksceSblACMgrIsGameProgram(0)) {
		result = (int)0x80800009;
		goto out;
	}
	if (value == NULL) {
		result = (int)0x80800001;
		goto out;
	}
	if (parameter == 3) {
		if (size <= 16) {
			result = (int)0x80800001;
			goto out;
		}
		memset(value, 0, 17);
		result = ksceRegMgrSystemParamGetStr(0x00033818, value, 17);
	} else if (parameter == 6) {
		if (!SceSblACMgrForDriver_0924896F(0)) {
			result = (int)0x80800009;
			goto out;
		}
		if (size <= 255) {
			result = (int)0x80800001;
			goto out;
		}
		memset(value, 0, 256);
		result = ksceRegMgrSystemParamGetStr(0x00988b81, value, 256);
	} else if (parameter == 1 && size > 2) {
		value[0] = 0;
		value[1] = 0;
		value[2] = 0;
		result = ksceRegMgrSystemParamGetStr(0x008f94f9, value, 3);
	} else {
		result = (int)0x80800001;
	}
out:
	sceSyscallLeave(syscallState);
	return result;
}
