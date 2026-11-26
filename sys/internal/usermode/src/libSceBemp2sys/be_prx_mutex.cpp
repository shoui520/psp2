#include "be_mp2sys_internal.h"

BePrxMutex::BePrxMutex()
{
	m_locked = 0;
	m_initialized = 0;
	m_condCount = 0;
	m_condIds = 0;
}

int BePrxMutex::Release()
{
	if (!m_initialized) {
		beLogPrintf(2, "[DEBUG] %s : Mutex is not yet initialized! [this=0x%08x]\n", "Release", this);
		return 1;
	}

	if (m_condCount != 0) {
		for (uint32_t i = 0; i < m_condCount; ++i) {
			int err = sceKernelDeleteCond(m_condIds[i]);
			if (err != 0) {
				beLogPrintf(3, "[ERROR] %s : sceKernelDeleteCond() failed. (%08X) [this=0x%08x](%d)\n",
					"Release", err, this, i);
			}
		}
		delete[] m_condIds;
	}

	SceUID mutexId = m_mutexId;
	m_condIds = 0;
	m_condCount = 0;

	int err = sceKernelDeleteMutex(mutexId);
	if (err != 0) {
		beLogPrintf(3, "[ERROR] %s : sceKernelDeleteMutex() failed. (%08X) [this=0x%08x]\n",
			"Release", err, this);
	}
	m_initialized = 0;
	return err;
}

BePrxMutex::~BePrxMutex()
{
	if (m_initialized)
		Release();
}

int BePrxMutex::Initialize(uint32_t condCount)
{
	if (m_initialized) {
		beLogPrintf(2, "[DEBUG] %s : Mutex is already Initialized!\n", "Initialize");
		return 1;
	}

	m_mutexId = sceKernelCreateMutex("SceBePrxMutex", 0x2002u, 0, 0);
	if (m_mutexId < 0) {
		beLogPrintf(3, "[ERROR] %s : sceKernelCreateMutex() failed. %08X [this=0x%08x]\n",
			"Initialize", m_mutexId, this);
		return m_mutexId;
	}

	if (condCount != 0) {
		if (m_condIds != 0)
			delete[] m_condIds;
		m_condIds = new SceUID[condCount];
		if (m_condIds == 0) {
			sceKernelDeleteMutex(m_mutexId);
			beLogPrintf(2, "[DEBUG] %s : Malloc failed\n", "Initialize");
			return 1;
		}

		for (uint32_t i = 0; i < condCount; ++i) {
			m_condIds[i] = sceKernelCreateCond("SceBePrxMutexCond", 0x2000u, m_mutexId, 0);
			if (m_condIds[i] < 0) {
				beLogPrintf(3, "[ERROR] %s : sceKernelCreateCond() failed. (%08X) [this=0x%08x](%d)\n",
					"Initialize", m_condIds[i], this, i);
				for (uint32_t j = 0; j < i; ++j)
					sceKernelDeleteCond(m_condIds[j]);
				sceKernelDeleteMutex(m_mutexId);
				delete[] m_condIds;
				m_condIds = 0;
				return 1;
			}
		}
	}

	m_condCount = condCount;
	m_initialized = 1;
	m_locked = 0;
	return 0;
}

int BePrxMutex::Lock(uint32_t timeout)
{
	SceUInt timeoutVal = timeout;
	int err;

	if (timeout != 0)
		err = sceKernelLockMutex(m_mutexId, 1, &timeoutVal);
	else
		err = sceKernelLockMutex(m_mutexId, 1, 0);

	if (err != 0) {
		if (err != SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
			beLogPrintf(3, "[ERROR] %s : sceKernelLockMutex() failed. (%08X) [this=0x%08x]\n",
				"Lock", err, this);
		}
	} else {
		m_locked = 1;
	}
	return err;
}

int BePrxMutex::TryLock()
{
	int err = sceKernelTryLockMutex(m_mutexId, 1);
	if (err == 0)
		m_locked = 1;
	return err;
}

int BePrxMutex::Unlock()
{
	int err = sceKernelUnlockMutex(m_mutexId, 1);
	if (err != 0) {
		beLogPrintf(3, "[ERROR] %s : sceKernelUnlockMutex() failed. (%08X) [this=0x%08x]\n",
			"Unlock", err, this);
	} else if (m_locked) {
		m_locked = 0;
	}
	return err;
}

int BePrxMutex::Wait(uint32_t condIndex, uint32_t timeout)
{
	SceUInt timeoutVal = timeout;

	if (condIndex >= m_condCount) {
		beLogPrintf(2, "[DEBUG] %s : cond index[%u:%u] is not valid. [this=0x%08x]\n",
			"Wait", condIndex, m_condCount, this);
		return 1;
	}

	int err;
	if (timeout != 0)
		err = sceKernelWaitCond(m_condIds[condIndex], &timeoutVal);
	else
		err = sceKernelWaitCond(m_condIds[condIndex], 0);

	if (err != 0 && err != SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
		beLogPrintf(3, "[ERROR] %s : sceKernelWaitCond() failed. (%08X) [this=0x%08x](%u)\n",
			"Wait", err, this, condIndex);
	}
	return err;
}

int BePrxMutex::Signal(uint32_t condIndex)
{
	if (condIndex >= m_condCount) {
		beLogPrintf(2, "[DEBUG] %s : cond index[%u:%u] is not valid. [this=0x%08x]\n",
			"Signal", condIndex, m_condCount, this);
		return 1;
	}

	int err = sceKernelSignalCond(m_condIds[condIndex]);
	if (err != 0) {
		beLogPrintf(3, "[ERROR] %s : sceKernelSignalCond() failed. (%08X) [this=0x%08x](%u)\n",
			"Signal", err, this, condIndex);
	}
	return err;
}
