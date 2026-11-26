#include "be_mp2sys_internal.h"

BePrxMutexLight::BePrxMutexLight()
{
	m_locked = 0;
	m_initialized = 0;
	m_lwMutex = 0;
	m_lwConds = 0;
	m_mutexRawMem = 0;
	m_condCount = 0;
}

int BePrxMutexLight::Release()
{
	if (!m_initialized) {
		beLogPrintf(2, "[DEBUG] %s : MutexLight is not yet initialized! [this=0x%08x]\n", "Release", this);
		return 1;
	}

	if (m_condCount != 0) {
		for (uint32_t i = 0; i < m_condCount; ++i) {
			int err = sceKernelDeleteLwCond(&m_lwConds[i]);
			if (err != 0) {
				beLogPrintf(3, "[ERROR] %s : sceKernelDeleteLwCond() failed. (%08X) [this=0x%08x](%d)\n",
					"Release", err, this, i);
			}
		}
		delete[] m_lwConds;
	}

	SceKernelLwMutexWork *lwMutex = m_lwMutex;
	m_lwConds = 0;
	m_condCount = 0;

	int err = sceKernelDeleteLwMutex(lwMutex);
	if (err != 0) {
		beLogPrintf(3, "[ERROR] %s : sceKernelDeleteLwMutex() failed. (%08X) [this=0x%08x]\n",
			"Release", err, this);
	}

	if (m_mutexRawMem != 0)
		delete[] (uint8_t *)m_mutexRawMem;
	m_mutexRawMem = 0;
	m_lwMutex = 0;
	m_initialized = 0;
	return err;
}

BePrxMutexLight::~BePrxMutexLight()
{
	if (m_initialized)
		Release();
}

int BePrxMutexLight::Initialize(uint32_t condCount)
{
	if (m_initialized) {
		beLogPrintf(2, "[DEBUG] %s : MutexLight is already Initialized! [this=0x%08x]\n", "Initialize", this);
		return 1;
	}

	if (m_mutexRawMem != 0) {
		delete[] (uint8_t *)m_mutexRawMem;
		m_mutexRawMem = 0;
		m_lwMutex = 0;
	}

	uint8_t *raw = new uint8_t[sizeof(SceKernelLwMutexWork) + 8];
	m_mutexRawMem = raw;
	if (raw == 0)
		return 1;

	m_lwMutex = (SceKernelLwMutexWork *)(((uintptr_t)raw & ~(uintptr_t)7) + 8);
	int err = sceKernelCreateLwMutex(m_lwMutex, "SceBePrxMutexLight", 0x2002u, 0, 0);
	if (err != 0) {
		beLogPrintf(3, "[ERROR] %s : sceKernelCreateLwMutex() failed. %08X [this=0x%08x]\n",
			"Initialize", err, this);
		delete[] (uint8_t *)m_mutexRawMem;
		m_mutexRawMem = 0;
		m_lwMutex = 0;
		return err;
	}

	if (condCount != 0) {
		if (m_lwConds != 0)
			delete[] m_lwConds;
		m_lwConds = new SceKernelLwCondWork[condCount];
		if (m_lwConds == 0) {
			sceKernelDeleteLwMutex(m_lwMutex);
			beLogPrintf(2, "[DEBUG] %s : Malloc failed\n", "Initialize");
			delete[] (uint8_t *)m_mutexRawMem;
			m_mutexRawMem = 0;
			m_lwMutex = 0;
			return 1;
		}

		for (uint32_t i = 0; i < condCount; ++i) {
			err = sceKernelCreateLwCond(&m_lwConds[i], "SceBePrxMutexLightCond", 0x2000u, m_lwMutex, 0);
			if (err != 0) {
				beLogPrintf(3, "[ERROR] %s : sceKernelCreateLwCond() failed. (%08X) [this=0x%08x](%d)\n",
					"Initialize", err, this, i);
				for (uint32_t j = 0; j < i; ++j)
					sceKernelDeleteLwCond(&m_lwConds[j]);
				sceKernelDeleteLwMutex(m_lwMutex);
				delete[] (uint8_t *)m_mutexRawMem;
				m_mutexRawMem = 0;
				m_lwMutex = 0;
				delete[] m_lwConds;
				m_lwConds = 0;
				return err;
			}
		}
	}

	m_condCount = condCount;
	m_initialized = 1;
	m_locked = 0;
	return 0;
}

int BePrxMutexLight::Lock(uint32_t timeout)
{
	SceUInt timeoutVal = timeout;
	int err;

	if (timeout != 0)
		err = sceKernelLockLwMutex(m_lwMutex, 1, &timeoutVal);
	else
		err = sceKernelLockLwMutex(m_lwMutex, 1, 0);

	if (err != 0) {
		if (err != SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			beLogPrintf(3, "[ERROR] %s : sceKernelLockLwMutex() failed. (%08X) [this=0x%08x]\n",
				"Lock", err, this);
		}
	} else {
		m_locked = 1;
	}
	return err;
}

int BePrxMutexLight::TryLock()
{
	int err = sceKernelTryLockLwMutex(m_lwMutex, 1);
	if (err == 0)
		m_locked = 1;
	return err;
}

int BePrxMutexLight::Unlock()
{
	int err = sceKernelUnlockLwMutex(m_lwMutex, 1);
	if (err != 0) {
		beLogPrintf(3, "[ERROR] %s : sceKernelUnlockLwMutex() failed. (%08X) [this=0x%08x]\n",
			"Unlock", err, this);
	} else if (m_locked) {
		m_locked = 0;
	}
	return err;
}

int BePrxMutexLight::Wait(uint32_t condIndex, uint32_t timeout)
{
	SceUInt timeoutVal = timeout;

	if (condIndex >= m_condCount) {
		beLogPrintf(2, "[DEBUG] %s : lwcond index[%u:%u] is not valid. [this=0x%08x]\n",
			"Wait", condIndex, m_condCount, this);
		return 1;
	}

	int err;
	if (timeout != 0)
		err = sceKernelWaitLwCond(&m_lwConds[condIndex], &timeoutVal);
	else
		err = sceKernelWaitLwCond(&m_lwConds[condIndex], 0);

	if (err != 0 && err != SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
		beLogPrintf(3, "[ERROR] %s : sceKernelWaitLwCond() failed. (%08X) [this=0x%08x](%u)\n",
			"Wait", err, this, condIndex);
	}
	return err;
}

int BePrxMutexLight::Signal(uint32_t condIndex)
{
	if (condIndex >= m_condCount) {
		beLogPrintf(2, "[DEBUG] %s : lwcond index[%u:%u] is not valid. [this=0x%08x]\n",
			"Signal", condIndex, m_condCount, this);
		return 1;
	}

	int err = sceKernelSignalLwCond(&m_lwConds[condIndex]);
	if (err != 0) {
		beLogPrintf(3, "[ERROR] %s : sceKernelSignalLwCond() failed. (%08X) [this=0x%08x](%u)\n",
			"Signal", err, this, condIndex);
	}
	return err;
}
