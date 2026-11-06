/*
 * The party game client's state (party_game_system.cpp): the shell's IPMI connection, the
 * memory block it is made in, the application's event handlers and the lock Sony names
 * "SceNpPartyGameSystemLwMutex". InitMutex and TermMutex are Sony's names (0.990's records); the
 * class name follows the lock's, the other names are ours.
 */
#ifndef PARTY_GAME_CLIENT_PARTY_GAME_SYSTEM_H
#define PARTY_GAME_CLIENT_PARTY_GAME_SYSTEM_H

#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <np/np_party.h>
#include "../../../../../../../lowlayer/src/ipmi_wrapper/ipmi_wrapper_client.h"

class PartyGameSystem {
public:
	bool m_initialized;
	IpmiWrapper::Client *m_client;
	SceUID m_memBlockUid;
	void *m_mspace;
	void *m_clientMemory;		/* the client's buffer, freed after create */
	SceKernelLwMutexWork m_mutex;
	SceNpPartyRoomEventHandler m_roomEventHandler;
	SceNpPartyVoiceEventHandler m_voiceEventHandler;
	SceNpPartyGameSessionEventHandler m_gameSessionEventHandler;
	void *m_userdata;
	SceUInt32 m_sdkVersion;		/* the application's, from sceNpPartyInit */

	PartyGameSystem()
	{
		m_initialized = false;
		m_client = NULL;
		m_memBlockUid = 0;
		m_mspace = NULL;
		m_clientMemory = NULL;
		m_roomEventHandler = NULL;
		m_voiceEventHandler = NULL;
		m_gameSessionEventHandler = NULL;
		m_userdata = NULL;
		m_sdkVersion = 0;
		sceClibMemset(&m_mutex, 0, sizeof(m_mutex));
	}

	void initialize(SceUInt32 sdkVersion);
	int reset();
	int InitMutex();
	int TermMutex();
};

extern PartyGameSystem g_partyGameSystem;

#endif
