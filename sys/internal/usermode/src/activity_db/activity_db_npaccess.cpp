/*
 * The network daemon's connection to the NP activity service: one request runs on it at a
 * time, and the daemon can abort the one running.
 */
#include "activity_db_internal.h"

ActivityDbNpAccess::ActivityDbNpAccess()
	: m_connection(0), m_lock("SceLsdbActNpAcc", 0), m_busy(false)
{
}

ActivityDbNpAccess::~ActivityDbNpAccess()
{
}

int ActivityDbNpAccess::Open()
{
	int result;

	m_lock.Lock();
	if (m_connection != 0) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	result = SceNpActivityNet_F8049E41(&m_connection);
	if (result == 0)
		m_busy = false;
	m_lock.Unlock();
	return result;
}

void ActivityDbNpAccess::Close()
{
	m_lock.Lock();
	if (m_connection != 0) {
		SceNpActivityNet_72346DA7(m_connection);
		m_busy = false;
		m_connection = 0;
	}
	m_lock.Unlock();
}

int ActivityDbNpAccess::Begin(int mode, int *connection)
{
	int result;

	m_lock.Lock();
	*connection = 0;
	if (m_connection == 0) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	if (m_busy == true) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	result = SceNpActivityNet_BE4CC2D1(m_connection, mode);
	if (result != 0) {
		*connection = 0;
		m_lock.Unlock();
		return result;
	}
	*connection = m_connection;
	m_busy = true;
	m_lock.Unlock();
	return result;
}

int ActivityDbNpAccess::End()
{
	int result;

	m_lock.Lock();
	if (m_connection == 0) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	if (!m_busy) {
		m_lock.Unlock();
		return ACTIVITY_DB_ERROR_NOT_READY;
	}
	result = SceNpActivityNet_F3586055(m_connection);
	m_busy = false;
	m_lock.Unlock();
	return result;
}

void ActivityDbNpAccess::Abort()
{
	if (m_connection != 0 && m_busy)
		SceNpActivityNet_35B49CA4(m_connection);
}
