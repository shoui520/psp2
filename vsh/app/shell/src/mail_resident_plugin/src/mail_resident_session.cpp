/*
 * SceShell mail_resident_plugin: MailResidentSession
 * 3.65-CEX: 0x811C867C - 0x811C8858
 */

#include <stddef.h>
#include <scetypes.h>
#include <ipmi/server.h>
#include <ipmi/session.h>

#include "mail_resident.h"

extern "C" {
extern int sceAppMgrGetStatusByAppId(SceUID appId, void *statusBuf);
}

/* 0x811C867C */
MailResidentSession::MailResidentSession()
	: m_session(NULL), m_sessionBuf(NULL), m_pid(-1), m_state(6),
	  m_mutex("SceMailResidentMaSession", 0)
{
}

/* 0x811C86B8 */
void MailResidentSession::Close()
{
	m_mutex.Lock();
	m_pid = -1;
	if (m_session != NULL) {
		m_session->destroy();
		m_session = NULL;
		if (m_sessionBuf != NULL) {
			mailResidentFree(m_sessionBuf);
			m_sessionBuf = NULL;
		}
	}
	m_mutex.Unlock();
}

/* 0x811C86EE / 0x811C8746 */
MailResidentSession::~MailResidentSession()
{
	Close();
}

/* 0x811C87A2 */
int MailResidentSession::Open(IPMI::Server *server, const IPMI::Session::Config *cfg,
	IPMI::Session **outSession)
{
	m_sessionBuf = mailResidentMalloc(cfg->bufferSize);
	if (m_sessionBuf == NULL)
		return -2;
	*outSession = NULL;
	if (server->createSession(&m_session, this, m_sessionBuf) != 0)
		return -1;
	*outSession = m_session;
	m_pid = m_session->getProcessId();
	return 0;
}

/* 0x811C87F2 */
void MailResidentSession::NotifyZero()
{
	m_mutex.Lock();
	char status[128];
	if (sceAppMgrGetStatusByAppId(m_session->getAppId(), status) == 0) {
		if (m_session != NULL) {
			int msg = 0;
			m_session->emptyMsgQueue(0);
			m_session->sendMsg(0, &msg, 4, NULL);
		}
	}
	m_mutex.Unlock();
}
