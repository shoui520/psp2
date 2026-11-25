/*
 * SceShell mail_resident_plugin: middleware process manager
 * (0x811C8858 - 0x811C8CD4)
 */

#include "mail_resident.h"

#include <std/string.h>
#include <kernel.h>
#include <appmgr.h>

extern "C" {
extern int sceKernelPowerTick(int type);
extern int sceAppMgrLaunchAppByName2(const char *name, const char *param,
	void *optParam);
extern int sceAppMgrGetStatusByName(const char *name, void *status);
extern int sceAppMgrDestroyAppByName(const char *name);
}

/* 0x811C8858 */
MailResidentMwManager::MailResidentMwManager()
	: m_destroyed(0), m_evf(-1), m_client(NULL), m_keyCount(0), m_keys(NULL)
{
	m_evf = sceKernelCreateEventFlag("ScePsnMailMiddleInvoke", 0x80, 0, NULL);
}

/* 0x811C8884 */
MailResidentMwManager::~MailResidentMwManager()
{
	if (m_keys != NULL) {
		mailResidentFree(m_keys);
		m_keys = NULL;
	}
	sceKernelDeleteEventFlag(m_evf);
}

/* 0x811C88A0 */
int MailResidentMwManager::Connect()
{
	if (m_client != NULL)
		return -1;

	IPMI::Client::Config cfg;
	sce_paf_strncpy(cfg.serviceName, "SceMwIPMIServer", 0x10);
	cfg.budget = 2;

	SceSize bufSize = IPMI::Client::getBufferSize(&cfg);
	void *buf = mailResidentMalloc(bufSize);
	if (buf == NULL)
		return -1;

	if (IPMI::Client::create(&m_client, &cfg, this, buf) != 0)
		return -1;

	int connectRes = 0;
	SceUInt32 param[4];
	param[0] = 1;
	param[1] = 1;
	int ret = m_client->connect(param, 16, &connectRes);
	if ((connectRes | ret) != 0) {
		if (ret != 0) {
			if (ret == (int)0x80020583) {
				ret = -1;
			} else if (ret == (int)0x80020581) {
				ret = connectRes;
			}
		} else {
			ret = connectRes;
		}
		m_client->destroy();
		mailResidentFree(m_client);
		m_client = NULL;
		return ret;
	}
	return 0;
}

/* 0x811C89B2 */
void MailResidentMwManager::Disconnect(SceUInt8 graceful)
{
	if (m_client != NULL) {
		if (graceful == 0)
			m_client->terminateConnection();
		else
			m_client->disconnect();
		m_client->destroy();
		mailResidentFree(m_client);
		m_client = NULL;
	}
}

/* 0x811C89EA */
int MailResidentMwManager::IsRunning(int *outStatus)
{
	SceUInt8 status[0x80];
	do {
		sce_paf_memset(status, 0, sizeof(status));
		*outStatus = sceAppMgrGetStatusByName("NPXS10063", status);
		sceKernelDelayThread(64000);
	} while ((*outStatus | *(int *)(status + 0x3C)) == 0);
	return (*outStatus == 0);
}

/* 0x811C8A42 */
int MailResidentMwManager::SignalReady()
{
	if (m_destroyed == 0)
		sceKernelSetEventFlag(m_evf, 0xFFFFFFFF);
	m_destroyed = 0;
	return 0;
}

/* 0x811C8A5A */
void MailResidentMwManager::LaunchAndConnect()
{
	SceUID cb = sceKernelCreateCallback(
		"SceMailResidentMwManager", 0, LaunchCallback, this);

	SceUInt32 opt[16];
	sce_paf_memset(opt, 0, sizeof(opt));
	opt[6] = 3;
	for (SceUInt32 i = 0; i < m_keyCount; ++i) {
		if ((SceUInt32)((SceUInt64)m_keys[i] >> 32) == 0) {
			opt[6] = 2;
			break;
		}
	}
	opt[3] = (SceUInt32)cb;
	opt[4] = (SceUInt32)-1;
	opt[0] = sizeof(opt);

	sceKernelClearEventFlag(m_evf, 0);
	if (sceAppMgrLaunchAppByName2("NPXS10063", NULL, opt) >= 0) {
		SceUInt32 outBits = 0;
		for (SceUInt32 tries = 0; tries <= 0x1E; ++tries) {
			SceUInt timeout = 1000000;
			sceKernelPowerTick(1);
			if (sceKernelWaitEventFlag(m_evf, 0xFFFFFFFF, 3, &outBits, &timeout) !=
				(int)0x80028005)
				break;
		}

		SceUInt8 status[0x80];
		int st;
		do {
			sce_paf_memset(status, 0, sizeof(status));
			st = sceAppMgrGetStatusByName("NPXS10063", status);
			sceKernelDelayThread(64000);
		} while ((st | *(int *)(status + 0x3C)) == 0);

		if (st == 0) {
			if (outBits == 1) {
				m_destroyed = 0;
				Connect();
			} else {
				m_destroyed = 1;
				sceAppMgrDestroyAppByName("NPXS10063");
			}
		}
	}
	sceKernelDeleteCallback(cb);
}

/* 0x811C8B94 */
void MailResidentMwManager::AddRefKey(SceInt64 key)
{
	for (SceUInt32 i = 0; i < m_keyCount; ++i) {
		if (m_keys[i] == key)
			return;
	}
	m_keyCount++;
	m_keys = (SceInt64 *)mailResidentRealloc(m_keys, sizeof(SceInt64) * m_keyCount);
	m_keys[m_keyCount - 1] = key;
}

/* 0x811C8BEC */
void MailResidentMwManager::RemoveRefKey(SceInt64 key)
{
	for (SceUInt32 i = 0; i < m_keyCount; ++i) {
		if (m_keys[i] == key) {
			sce_paf_memmove(&m_keys[i], &m_keys[i + 1],
				sizeof(SceInt64) * (m_keyCount - i - 1));
			m_keyCount--;
			m_keys = (SceInt64 *)mailResidentRealloc(m_keys, sizeof(SceInt64) * m_keyCount);
			if (m_keyCount == 0) {
				Disconnect(1);
				m_destroyed = 1;
				sceAppMgrDestroyAppByName("NPXS10063");
			}
			return;
		}
	}
}

/* 0x811C8C94 */
int MailResidentMwManager::SyncCall(SceUInt32 methodId, const void *inBuf, SceSize inSize,
	int *outRes, void *outBuf, SceSize *outSize, SceSize maxOut)
{
	if (m_client != NULL)
		return m_client->invokeSyncMethod(methodId, inBuf, inSize, outRes, outBuf, outSize, maxOut);
	return -1;
}

/* 0x811C8CBE */
int MailResidentMwManager::LaunchCallback(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	(void)notifyId;
	(void)notifyCount;
	if (userData != NULL && notifyArg < 0) {
		MailResidentMwManager *self = (MailResidentMwManager *)userData;
		sceKernelSetEventFlag(self->m_evf, 0xFFFFFFFF);
	}
	return 0;
}
