#include "update_internal.h"
#include <sysmem/sysroot.h>

int DeciUtilInformUpdateStarted(int number1, int number2,
	const char *str, SceSize length)
{
	return SceSysrootForDriver_9A486846(number1, number2, str, length);
}

int DeciUtilInformUpdateOngoing(int number1, int number2)
{
	return SceSysrootForDriver_9EC02A41(number1, number2);
}

int DeciUtilInformUpdateFinished(int number, const char *str, SceSize length)
{
	return SceSysrootForDriver_217B2871(number, str, length);
}

int DeciUtilSetSwInfoInt(const char *name, SceSize nameLength, uint32_t value)
{
	return SceSysrootForDriver_631141E2(name, nameLength, value);
}

int DeciUtilSetSwInfoStr(const char *name, SceSize nameLength,
	const char *str, SceSize strLength)
{
	return SceSysrootForDriver_17DD213C(name, nameLength, str, strLength);
}

int DeciUtilSetSwInfoBin(const char *name, SceSize nameLength,
	const void *data, SceSize dataSize)
{
	return SceSysrootForDriver_9E96D990(name, nameLength, data, dataSize);
}
