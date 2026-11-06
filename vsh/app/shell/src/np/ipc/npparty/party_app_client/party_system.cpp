/*
 * The party app client's state. Sony's build keeps it after party_app_client.cpp, as np_party
 * keeps party_game_system.cpp after party_game_client.cpp; the file's name is ours.
 */
#include <stddef.h>
#include "party_system.h"

PartySystem g_partySystem;

void PartySystem::initialize()
{
	m_initialized = false;
	m_client = NULL;
	m_memBlockUid = 0;
	m_mspace = NULL;
	m_clientMemory = NULL;
	m_userdata = NULL;
	m_eventHandler = NULL;
	sceClibMemset(&m_mutex, 0, sizeof(m_mutex));
}

int PartySystem::reset()
{
	sceClibMemset(&m_mutex, 0, sizeof(m_mutex));
	m_initialized = false;
	m_client = NULL;
	m_memBlockUid = 0;
	m_mspace = NULL;
	m_clientMemory = NULL;
	m_eventHandler = NULL;
	m_userdata = NULL;
	return 0;
}

int PartySystem::InitMutex()
{
	return sceKernelCreateLwMutex(&m_mutex, "SceNpPartySystemLwMutex",
				      SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO, 0, NULL);
}

int PartySystem::TermMutex()
{
	return sceKernelDeleteLwMutex(&m_mutex);
}
