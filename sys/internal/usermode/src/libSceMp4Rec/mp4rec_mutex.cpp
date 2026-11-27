/*
 * SceLibMp4Recorder LwMutex wrapper.
 */
#include "mp4rec_internal.h"

Mp4RecMutex::Mp4RecMutex(int32_t *outErr, const char *name, uint8_t attrFlags)
{
	uint32_t attr = 0x2000;
	if ((attrFlags & 2) != 0)
		attr = 0x2002;
	*outErr = sceKernelCreateLwMutex(&m_work, name, attr, 0, NULL);
}

Mp4RecMutex::~Mp4RecMutex()
{
	sceKernelDeleteLwMutex(&m_work);
}

int32_t Mp4RecMutex::Lock()
{
	return sceKernelLockLwMutex(&m_work, 1, NULL);
}

int32_t Mp4RecMutex::Unlock()
{
	return sceKernelUnlockLwMutex(&m_work, 1);
}

int32_t Mp4RecMutex::TryLock()
{
	return sceKernelTryLockLwMutex(&m_work, 1);
}
