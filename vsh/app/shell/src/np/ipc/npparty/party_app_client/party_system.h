/*
 * The party app client's state (party_system.cpp): the shell's IPMI connection, the memory
 * block it is made in, the party app's event handler and the lock Sony names
 * "SceNpPartySystemLwMutex". The class name follows the lock's, as np_party's
 * PartyGameSystem does; InitMutex and TermMutex are np_party's names (0.990's records) for the
 * same methods; the other names are ours.
 */
#ifndef PARTY_APP_CLIENT_PARTY_SYSTEM_H
#define PARTY_APP_CLIENT_PARTY_SYSTEM_H

#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include "party_app_client.h"
#include "../../../../../../../lowlayer/src/ipmi_wrapper/ipmi_wrapper_client.h"

class PartySystem {
public:
	bool m_initialized;
	IpmiWrapper::Client *m_client;
	SceUID m_memBlockUid;
	void *m_mspace;
	void *m_clientMemory;		/* the client's buffer, freed after create */
	SceKernelLwMutexWork m_mutex;
	SceNpPartyAppEventHandler m_eventHandler;
	void *m_userdata;

	PartySystem()
	{
		m_initialized = false;
		m_client = NULL;
		m_memBlockUid = 0;
		m_mspace = NULL;
		m_clientMemory = NULL;
		m_eventHandler = NULL;
		m_userdata = NULL;
		sceClibMemset(&m_mutex, 0, sizeof(m_mutex));
	}

	void initialize();
	int reset();
	int InitMutex();
	int TermMutex();
};

extern PartySystem g_partySystem;

#endif
