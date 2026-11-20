/*
 * exclusive_func.cpp: the exclusive function's base (0x810E4B28-0x810E4BBC): its kind, whether it
 * waits in the queue, and the state bits the manager and the function set. The file's and the
 * names are ours (PLACEHOLDER), after Sony's "ExclusiveFuncMtp" and "ExclusiveFuncPhotoImport".
 */
#include <stddef.h>
#include <scetypes.h>
#include "exclusive_func.h"

/* (0x810E4B28) */
ExclusiveFunc::ExclusiveFunc()
	: m_type(1), m_queueable(false), m_state(0)
{
}

/* (0x810E4B40) */
ExclusiveFunc::ExclusiveFunc(int type, bool queueable)
	: m_type(type), m_queueable(queueable), m_state(0)
{
}

/* (0x810E4B58, 0x810E4B66) */
ExclusiveFunc::~ExclusiveFunc()
{
}

/* (0x81409E2C, folded with the other functions returning 0) */
int ExclusiveFunc::Notify(int events)
{
	return 0;
}

/* (0x810E4B7C) */
void ExclusiveFunc::SetState(int state)
{
	m_state |= state;
}

/* (0x810E4B86) */
void ExclusiveFunc::SetStarted()
{
	m_state |= EXCLUSIVE_FUNC_STATE_STARTED;
}

/* (0x810E4B92) */
void ExclusiveFunc::SetStopped()
{
	m_state |= EXCLUSIVE_FUNC_STATE_STOPPED;
}

/* (0x810E4B9E) */
int ExclusiveFunc::GetType()
{
	return m_type;
}

/* (0x810E4BA4) */
bool ExclusiveFunc::IsQueueable()
{
	return m_queueable;
}

/* (0x810E4BA8) */
void ExclusiveFunc::ClearState(int state)
{
	m_state &= ~state;
}

/* (0x810E4BB4) */
int ExclusiveFunc::GetState()
{
	return m_state;
}
