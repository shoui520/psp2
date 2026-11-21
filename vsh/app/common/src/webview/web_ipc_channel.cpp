/*
 * The channel to the WebKit process: the message pipe a view's packets are received on, read by
 * the channel's thread, and the WebKit process's pipe they are sent on. Sony's file name is not
 * known (the module has no debug records); this one and every name in it are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/error.h>
#include <kernel/threadmgr.h>
#include <std/stdlib.h>
#include <std/string.h>
#include "web_ipc.h"

namespace paf {
namespace ui {

/* how long a packet waits for room in the other process's pipe, and a thread for its end */
#define WEB_IPC_SEND_TIMEOUT		2000000
#define WEB_IPC_THREAD_END_TIMEOUT	100000

/* the events the receiving thread waits for: the other process's open, then its packets */
#define WEB_IPC_WAIT_CONNECT	(SCE_KERNEL_EVENT_OPEN | SCE_KERNEL_EVENT_DELETE)
#define WEB_IPC_WAIT_RECEIVE	(SCE_KERNEL_EVENT_DATA_EXIST | SCE_KERNEL_EVENT_CLOSE | \
				 SCE_KERNEL_EVENT_ERROR | SCE_KERNEL_EVENT_DELETE)

/* a Send's answer is in m_packet */
#define WEB_IPC_SYNC_EVENT_ANSWER	0x80000000U

/* the packet into the other process's pipe */
static inline int SendPacket(SceUID pipe, WebIpcPacket *packet)
{
	int ret = SCE_ERROR_ERRNO_EINVAL;

	if (pipe > 0) {
		SceUInt32 timeout = WEB_IPC_SEND_TIMEOUT;

		ret = sceKernelSendMsgPipe(pipe, packet, sizeof(*packet),
					   SCE_KERNEL_MSG_PIPE_MODE_FULL, NULL, &timeout);
	}
	return ret;
}

/* the pipe this channel receives on, under its name */
static inline int CreateReceivePipe(WebIpcChannel *channel)
{
	SceUID pipe = sceKernelCreateMsgPipe(channel->m_name.c_str(),
					     SCE_KERNEL_MSG_PIPE_TYPE_USER_MAIN,
					     SCE_KERNEL_EVENT_ATTR_AUTO_RESET | SCE_KERNEL_ATTR_OPENABLE |
					     SCE_KERNEL_MSG_PIPE_ATTR_TH_FIFO,
					     sizeof(WebIpcPacket), NULL);

	if (pipe <= 0) {
		sce_paf_printf_dummy();
		sce_paf_printf_dummy();
		return pipe;
	}
	sce_paf_printf_dummy();
	channel->m_receivePipe = pipe;
	return 0;
}

static inline void DeleteReceivePipe(WebIpcChannel *channel)
{
	if (channel->m_receivePipe > 0)
		sceKernelDeleteMsgPipe(channel->m_receivePipe);
	channel->m_receivePipe = -1;
}

int WebIpcChannel::Close()
{
	int ret = SCE_ERROR_ERRNO_EINVAL;

	if (m_sendPipe > 0)
		ret = sceKernelCloseMsgPipe(m_sendPipe);
	m_sendPipe = -1;
	return ret;
}

WebIpcChannel::~WebIpcChannel()
{
	if (m_sendPipe > 0) {
		sceKernelCloseMsgPipe(m_sendPipe);
		m_sendPipe = -1;
	}
	if (m_thread > 0) {
		int ret;
		SceInt32 status = 0;
		SceUInt32 timeout = WEB_IPC_THREAD_END_TIMEOUT;

		for (int i = 0; i < 3; i++) {
			if (m_receivePipe > 0)
				sceKernelCancelEvent(m_receivePipe, NULL);
			ret = sceKernelWaitThreadEnd(m_thread, &status, &timeout);
			if (ret == 0)
				break;
			/* never: the loop ends before its fourth try */
			if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT && i == 3)
				sceKernelDeleteThread(m_thread);
		}
		m_thread = -1;
	}
	DeleteReceivePipe(this);
	if (m_syncEvent > 0) {
		sceKernelDeleteSimpleEvent(m_syncEvent);
		m_syncEvent = -1;
	}
}

int WebIpcChannel::Open(const char *name)
{
	int ret = SCE_ERROR_ERRNO_EINVAL;

	sce_paf_printf_dummy();
	if (name == NULL)
		return ret;
	if (m_sendPipe > 0)
		return 0;
	for (int i = 0; i < 5; i++) {
		ret = sceKernelOpenMsgPipe(name);
		if (ret > 0) {
			m_sendPipe = ret;
			return 0;
		}
		sce_paf_printf_dummy();
		sceKernelDelayThread(10000);
	}
	return ret;
}

int WebIpcChannel::Send(WebIpcPacket *packet)
{
	int ret = SCE_ERROR_ERRNO_EINVAL;

	if (m_sendPipe > 0) {
		packet->m_kind = WEB_IPC_KIND_SEND;
		ret = SendPacket(m_sendPipe, packet);
	}
	if (ret != 0)
		sce_paf_printf_dummy();
	return ret;
}

int WebIpcChannel::Post(WebIpcPacket *packet)
{
	packet->m_kind = WEB_IPC_KIND_POST;
	return SendPacket(m_sendPipe, packet);
}

static SceInt32 WebIpcThread(SceSize argSize, void *pArgBlock);

int WebIpcChannel::StartThread()
{
	WebIpcChannel *self;
	int ret;
	string name("SceWebIpcThread" + m_name);

	m_thread = sceKernelCreateThread(name.c_str(), WebIpcThread, 191,
					 m_smallStack ? 0x8000 : 0x10000, 0, 0, NULL);
	if (m_thread <= 0) {
		sce_paf_printf_dummy();
		ret = m_thread;
	} else {
		self = this;
		ret = sceKernelStartThread(m_thread, sizeof(self), &self);
	}
	return ret;
}

WebIpcChannel::WebIpcChannel(const char *name, bool smallStack, WebIpcListener *listener)
	: m_thread(-1), m_sendPipe(-1), m_receivePipe(-1), m_name(name), m_smallStack(smallStack),
	  m_syncEvent(-1)
{
	SceUID event;

	if (listener != NULL)
		m_listener = listener;
	sce_paf_memset(&m_packet, 0, sizeof(m_packet));
	event = sceKernelCreateSimpleEvent("SceWebIpcSyncEvent", SCE_KERNEL_EVENT_ATTR_AUTO_RESET,
					   0, NULL);
	if (event > 0)
		m_syncEvent = event;
	if (CreateReceivePipe(this) == 0)
		StartThread();
}

void WebIpcChannel::Run()
{
	int ret;

	if (m_receivePipe <= 0)
		return;
	for (;;) {
		SceUInt32 result;

connect:
		result = 0;
		ret = sceKernelWaitEvent(m_receivePipe, WEB_IPC_WAIT_CONNECT, &result, NULL, NULL);
		if (ret != 0 || (result & SCE_KERNEL_EVENT_OPEN) == 0) {
			if (ret != 0)
				sce_paf_printf_dummy();
			sce_paf_printf_dummy();
			return;
		}
		sce_paf_printf_dummy();
		if (m_listener != NULL)
			m_listener->OnConnect();
		for (;;) {
			SceUInt64 userData = 0;

			result = 0;
			ret = sceKernelWaitEvent(m_receivePipe, WEB_IPC_WAIT_RECEIVE, &result, &userData,
						 NULL);
			if (ret != 0) {
				sce_paf_printf_dummy();
				sce_paf_printf_dummy();
				if (m_listener != NULL)
					m_listener->OnDisconnect();
				if (ret == SCE_KERNEL_ERROR_WAIT_CANCEL)
					goto connect;
				break;
			}
			if (result & SCE_KERNEL_EVENT_DATA_EXIST) {
				WebIpcPacket packet;
				SceSize received = 0;

				ret = sceKernelTryReceiveMsgPipe(m_receivePipe, &packet, sizeof(packet),
								 SCE_KERNEL_MSG_PIPE_MODE_FULL, &received);
				if (ret != 0) {
					sce_paf_printf_dummy();
				} else if (received == sizeof(packet)) {
					switch (packet.m_kind) {
					case WEB_IPC_KIND_SEND:
						if (m_listener != NULL)
							m_listener->OnPost(&packet);
						break;
					case WEB_IPC_KIND_REQUEST:
						if (m_listener != NULL)
							m_listener->OnRequest(&packet);
						else
							Post(&packet);
						break;
					case WEB_IPC_KIND_POST:
						m_packet = packet;
						sceKernelSetEvent(m_syncEvent, WEB_IPC_SYNC_EVENT_ANSWER, 0);
						break;
					}
				}
			}
			if (result & SCE_KERNEL_EVENT_CLOSE) {
				sce_paf_printf_dummy();
				sce_paf_printf_dummy();
				if (m_listener != NULL)
					m_listener->OnDisconnect();
				break;
			}
			if (result & SCE_KERNEL_EVENT_ERROR) {
				int error = (int)userData;

				sce_paf_printf_dummy();
				sce_paf_printf_dummy();
				if (error == SCE_KERNEL_ERROR_WAIT_CANCEL) {
					sce_paf_printf_dummy();
					if (m_listener != NULL)
						m_listener->OnDisconnect();
					break;
				}
			} else if (result & SCE_KERNEL_EVENT_DELETE) {
				sce_paf_printf_dummy();
				sce_paf_printf_dummy();
				if (m_listener != NULL)
					m_listener->OnDisconnect();
				DeleteReceivePipe(this);
				CreateReceivePipe(this);
				break;
			} else if (result & ~WEB_IPC_WAIT_RECEIVE) {
				sce_paf_printf_dummy();
			}
			if (ret != 0)
				return;
		}
		if (ret != 0)
			return;
	}
}

/* the receiving thread: its argument is the channel */
static SceInt32 WebIpcThread(SceSize argSize, void *pArgBlock)
{
	if (pArgBlock != NULL)
		(*(WebIpcChannel **)pArgBlock)->Run();
	return sceKernelExitDeleteThread(0);
}

} /* namespace ui */
} /* namespace paf */
