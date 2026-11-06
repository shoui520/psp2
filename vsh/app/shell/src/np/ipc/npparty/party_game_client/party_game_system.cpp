/*
 * The party game client's state. Sony's file: party_game_system.cpp, beside
 * party_game_client.cpp (0.990's records, which name InitMutex and TermMutex).
 */
#include <stddef.h>
#include "party_game_system.h"

PartyGameSystem g_partyGameSystem;

void PartyGameSystem::initialize(SceUInt32 sdkVersion)
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
	m_sdkVersion = sdkVersion;
	sceClibMemset(&m_mutex, 0, sizeof(m_mutex));
}

int PartyGameSystem::reset()
{
	sceClibMemset(&m_mutex, 0, sizeof(m_mutex));
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
	return 0;
}

int PartyGameSystem::InitMutex()
{
	return sceKernelCreateLwMutex(&m_mutex, "SceNpPartyGameSystemLwMutex",
				      SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO, 0, NULL);
}

int PartyGameSystem::TermMutex()
{
	return sceKernelDeleteLwMutex(&m_mutex);
}
