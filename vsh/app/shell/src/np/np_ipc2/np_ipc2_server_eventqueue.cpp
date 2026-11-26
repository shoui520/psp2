/*
 * np_ipc2_server_eventqueue.cpp: Per-client ring-buffer event queue array
 * ("%sEqEvf%08x") for NpIpc2 services.
 * Sony path: vsh/app/common/src/np/np_ipc_ps4/np_ipc2/server/np_ipc2_server_eventqueue.cpp
 * 3.65-CEX: 0x813B93A0..0x813B976B
 */
#include "np_ipc2_internal.h"

namespace sce {
namespace Np {
namespace Ipc2 {

EventQueueServer::EventQueueServer() : m_server(NULL) {}

EventQueueServer::~EventQueueServer()
{
	m_server = NULL;
}

int EventQueueServer::Init(ServiceServer *server)
{
	m_server = server;
	return 0;
}

EventQueueSession::EventQueueSession(EventQueueServer *server)
	: m_server(server), m_numQueues(0), m_channelPool(NULL), m_channels(NULL)
{
	SceNpCommonPs4_945AC0FC(&m_eventFlag);
	SceNpCommonPs4_4266D870(&m_mutex);
}

EventQueueSession::~EventQueueSession()
{
	if (m_channels != NULL) {
		SceNpCommonPs4_E7D29E99(&m_eventFlag);
		for (SceUInt32 index = 0; index < m_numQueues; ++index)
			SceNpCommonPs4_A264C490(&m_channels[index]->ringBuf);
		Free(m_channels);
		m_channels = NULL;
		Free(m_channelPool);
		m_channelPool = NULL;
		SceNpCommonPs4_63EED478(&m_mutex);
	}
	SceNpCommonPs4_294F0B1A(&m_mutex);
	SceNpCommonPs4_4C3E58CA(&m_eventFlag);
}

int EventQueueSession::Init(const char *serviceName, ServiceSession *session,
			    const EventQueueSessionConfig *config)
{
	NP_IPC2_ASSERT(m_channels == NULL, "Init", "np_ipc2_server_eventqueue.cpp", 141,
		       "!IsInit()");

	if (config->numQueues > NP_IPC2_MAX_EVENT_QUEUES)
		return SCE_NP_IPC2_ERROR_INVALID_CONFIG;

	int ret = SceNpCommonPs4_8AA7E995(&m_mutex, "SceNpIpc2ServerEventQueue", 0);
	if (ret < 0)
		return ret;

	m_channelPool = static_cast<EventQueueChannel *>(
		Alloc(sizeof(EventQueueChannel) * config->numQueues));
	if (m_channelPool == NULL) {
		SceNpCommonPs4_63EED478(&m_mutex);
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;
	}

	m_channels = static_cast<EventQueueChannel **>(
		Alloc(sizeof(EventQueueChannel *) * config->numQueues));
	if (m_channels == NULL) {
		Free(m_channelPool);
		m_channelPool = NULL;
		SceNpCommonPs4_63EED478(&m_mutex);
		return SCE_NP_IPC2_ERROR_OUT_OF_MEMORY;
	}

	for (SceUInt32 index = 0; index < config->numQueues; ++index) {
		EventQueueChannel *chan = &m_channelPool[index];
		sceClibMemset(chan, 0, sizeof(*chan));
		SceNpCommonPs4_3245630F(&chan->ringBuf, GetAllocatorWithError());
		m_channels[index] = chan;
	}
	m_numQueues = config->numQueues;

	for (SceUInt32 index = 0; index < config->numQueues; ++index) {
		SceUInt32 itemSize = config->queues[index].itemSize;
		SceUInt32 itemCount = config->queues[index].itemCount;
		if (itemSize > NP_IPC2_MAX_EVENT_SIZE) {
			ret = SCE_NP_IPC2_ERROR_INVALID_CONFIG;
			break;
		}
		EventQueueChannel *chan = m_channels[index];
		ret = SceNpCommonPs4_530564F5(&chan->ringBuf, itemCount * itemSize);
		if (ret < 0)
			break;
		chan->maxItems = itemCount;
		chan->itemSize = itemSize;
	}

	if (ret >= 0) {
		SceUID clientPid = session->GetIpmiSession()->getProcessId();
		char evfName[32];
		sceClibSnprintf(evfName, sizeof(evfName), "%sEqEvf%08x", serviceName,
				static_cast<unsigned int>(clientPid));
		ret = SceNpCommonPs4_909FBD24(&m_eventFlag, evfName);
	}

	if (ret < 0) {
		SceNpCommonPs4_E7D29E99(&m_eventFlag);
		for (SceUInt32 index = 0; index < m_numQueues; ++index)
			SceNpCommonPs4_A264C490(&m_channels[index]->ringBuf);
		Free(m_channels);
		m_channels = NULL;
		Free(m_channelPool);
		m_channelPool = NULL;
		m_numQueues = 0;
		SceNpCommonPs4_63EED478(&m_mutex);
		return ret;
	}

	return 0;
}

int EventQueueSession::Dequeue(SceUInt32 queueIndex, void *outBuf, SceSize size)
{
	NP_IPC2_ASSERT(m_channels != NULL, "Dequeue", "np_ipc2_server_eventqueue.cpp", 268,
		       "IsInit()");
	NP_IPC2_ASSERT(queueIndex < m_numQueues, "Dequeue", "np_ipc2_server_eventqueue.cpp", 269,
		       "queueIndex < m_numEventQueue");

	EventQueueChannel *chan = m_channels[queueIndex];
	if (size != chan->itemSize)
		return SCE_NP_IPC2_ERROR_INVALID_ARGUMENT;
	if (SceNpCommonPs4_B599A560(&chan->ringBuf) != 0)
		return SCE_NP_IPC2_ERROR_EVENT_QUEUE_EMPTY;

	int ret = SceNpCommonPs4_DC5607D0(&chan->ringBuf, outBuf, size);
	return (ret >= 0) ? 0 : ret;
}

} /* namespace Ipc2 */
} /* namespace Np */
} /* namespace sce */
