/*
 * mtp_client_context.cpp: the MTP client's calls under way (0x81138994-0x81138F80; the
 * file's name is ours, Sony's messages tag it CLTCNT and name its methods): a ticket per
 * call holds the call's request, and each ticket has its own client event flag, set when
 * the call's result or its data arrives.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <std/stdlib.h>

#include "mtp_client_lib.h"

/* Sony's message: the component, the reporting function and Sony's source line */
#define MTP_CLTCNT_PRINT_ERROR(line, fmt, ...)					\
	sceClibPrintf("[MTP %s] Error %s(%u): " fmt, "CLTCNT", __FUNCTION__, line,	\
		      ##__VA_ARGS__)

#define MTP_CLIENT_TICKET_FREE		-1
#define MTP_CLIENT_TICKET_USED		1
#define MTP_CLIENT_SERIAL_END		0xFFFFFF	/* an id is the ticket and a serial */

/* what each ticket's event flag is set with */
#define MTP_CLIENT_EVENT_RESULT		0x1
#define MTP_CLIENT_EVENT_SEND_RECV	0x2

int MtpClientContextMgr::s_serial = 1;

/* the ticket an id names */
#define MTP_CLIENT_TICKET(id)		((SceUInt8)(id))

MtpClientContextMgr::~MtpClientContextMgr()
{
}

int MtpClientContextMgr::Initialize(int count, SceUInt32 eventFlagBase)
{
	int *tickets;
	int ret;
	int i;

	if (m_tickets != NULL) {
		MTP_CLTCNT_PRINT_ERROR(33, "already initialized\n");
		return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
	}
	tickets = (int *)sce_paf_malloc(sizeof(*tickets) * count);
	if (tickets == NULL) {
		MTP_CLTCNT_PRINT_ERROR(39, "no memory\n");
		return MTP_CLIENT_ERROR_OUT_OF_MEMORY;
	}
	for (i = 0; i < count; i++)
		tickets[i] = MTP_CLIENT_TICKET_FREE;
	ret = sceKernelCreateLwMutex(&m_lock, "SceMtpClientContextMgr",
				     SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO, 0, NULL);
	if (ret < 0) {
		MTP_CLTCNT_PRINT_ERROR(48, "CreateLwMutex failed: 0x%x\n", ret);
		sce_paf_free(tickets);
		return ret;
	}
	m_count = count;
	m_tickets = tickets;
	m_eventFlagBase = eventFlagBase;
	return 0;
}

int MtpClientContextMgr::Finalize(void)
{
	int ret;

	if (m_tickets != NULL) {
		sce_paf_free(m_tickets);
		m_tickets = NULL;
	}
	ret = sceKernelDeleteLwMutex(&m_lock);
	if (ret < 0)
		MTP_CLTCNT_PRINT_ERROR(69, "DeleteLwMutex failed: 0x%x\n", ret);
	return ret;
}

inline void MtpClientContextMgr::lock(void)
{
	int ret;

	ret = sceKernelLockLwMutex(&m_lock, 1, NULL);
	if (ret < 0)
		MTP_CLTCNT_PRINT_ERROR(220, "LockLwMutex failed: 0x%x\n", ret);
}

inline void MtpClientContextMgr::unlock(void)
{
	int ret;

	ret = sceKernelUnlockLwMutex(&m_lock, 1);
	if (ret < 0)
		MTP_CLTCNT_PRINT_ERROR(229, "UnlockLwMutex failed: 0x%x\n", ret);
}

int MtpClientContextMgr::Create(int *id, IPMI::EventFlagInfo *eventFlag)
{
	int serial;
	int i;

	lock();
	for (i = 0; i < m_count; i++) {
		if (m_tickets[i] == MTP_CLIENT_TICKET_FREE) {
			m_tickets[i] = MTP_CLIENT_TICKET_USED;
			break;
		}
	}
	if (i == m_count) {
		MTP_CLTCNT_PRINT_ERROR(86, "no tickets\n");
		unlock();
		return MTP_CLIENT_ERROR_NO_TICKET;
	}
	eventFlag->index = i + m_eventFlagBase;
	serial = s_serial;
	eventFlag->bitPattern = MTP_CLIENT_EVENT_RESULT;
	*id = i | (serial << 8);
	serial++;
	s_serial = serial;
	if (serial == MTP_CLIENT_SERIAL_END)
		s_serial = 1;
	unlock();
	return 0;
}

int MtpClientContextMgr::Set(int id, int requestId)
{
	if (MTP_CLIENT_TICKET(id) < m_count) {
		m_tickets[MTP_CLIENT_TICKET(id)] = requestId;
		return 0;
	}
	MTP_CLTCNT_PRINT_ERROR(110, "invalid id: 0x%x\n", id);
	return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
}

int MtpClientContextMgr::Get(int id, int *requestId)
{
	int ticket;

	if (MTP_CLIENT_TICKET(id) >= m_count) {
		MTP_CLTCNT_PRINT_ERROR(121, "invalid id: 0x%x\n", id);
		return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
	}
	ticket = m_tickets[MTP_CLIENT_TICKET(id)];
	if (ticket == MTP_CLIENT_TICKET_FREE) {
		MTP_CLTCNT_PRINT_ERROR(125, "invalid id: 0x%x\n", id);
		return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
	}
	*requestId = ticket;
	return 0;
}

int MtpClientContextMgr::WaitResult(IPMI::Client *client, int id, int *requestId)
{
	int ret;

	if (MTP_CLIENT_TICKET(id) >= m_count) {
		MTP_CLTCNT_PRINT_ERROR(136, "invalid id: 0x%x\n", id);
		return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
	}
	if (m_tickets[MTP_CLIENT_TICKET(id)] == MTP_CLIENT_TICKET_FREE) {
		MTP_CLTCNT_PRINT_ERROR(140, "invalid id: 0x%x\n", id);
		return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
	}
	ret = client->waitEventFlag(MTP_CLIENT_TICKET(id) + m_eventFlagBase,
				    MTP_CLIENT_EVENT_RESULT,
				    SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				    NULL, NULL);
	if (ret < 0)
		MTP_CLTCNT_PRINT_ERROR(150, "waitEventFlag failed: 0x%x\n", ret);
	*requestId = m_tickets[MTP_CLIENT_TICKET(id)];
	return ret;
}

int MtpClientContextMgr::Delete(int id)
{
	if (MTP_CLIENT_TICKET(id) < m_count) {
		m_tickets[MTP_CLIENT_TICKET(id)] = MTP_CLIENT_TICKET_FREE;
		return 0;
	}
	MTP_CLTCNT_PRINT_ERROR(162, "invalid id: 0x%x\n", id);
	return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
}

int MtpClientContextMgr::CreateSendRecvEvent(int id, IPMI::EventFlagInfo *eventFlag)
{
	lock();
	if (MTP_CLIENT_TICKET(id) >= m_count) {
		MTP_CLTCNT_PRINT_ERROR(174, "invalid id: 0x%x\n", id);
		unlock();
		return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
	}
	if (m_tickets[MTP_CLIENT_TICKET(id)] == MTP_CLIENT_TICKET_FREE) {
		MTP_CLTCNT_PRINT_ERROR(179, "invalid id: 0x%x\n", id);
		unlock();
		return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
	}
	eventFlag->bitPattern = MTP_CLIENT_EVENT_SEND_RECV;
	eventFlag->index = MTP_CLIENT_TICKET(id) + m_eventFlagBase;
	unlock();
	return 0;
}

int MtpClientContextMgr::WaitSendRecvResult(IPMI::Client *client, int id)
{
	int ret;

	if (MTP_CLIENT_TICKET(id) >= m_count) {
		MTP_CLTCNT_PRINT_ERROR(196, "invalid id: 0x%x\n", id);
		return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
	}
	if (m_tickets[MTP_CLIENT_TICKET(id)] == MTP_CLIENT_TICKET_FREE) {
		MTP_CLTCNT_PRINT_ERROR(200, "invalid id: 0x%x\n", id);
		return MTP_CLIENT_ERROR_INVALID_ARGUMENT;
	}
	ret = client->waitEventFlag(MTP_CLIENT_TICKET(id) + m_eventFlagBase,
				    MTP_CLIENT_EVENT_SEND_RECV,
				    SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				    NULL, NULL);
	if (ret < 0)
		MTP_CLTCNT_PRINT_ERROR(210, "waitEventFlag failed: 0x%x\n", ret);
	return ret;
}
