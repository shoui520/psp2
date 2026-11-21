/*
 * The library's instance: its setup and teardown, what the exports ask of it, and the state
 * machine the event thread steps every 10 ms, from the search through the connection and the
 * login to the text the companion types.
 *
 * The handlers get, as their event: 2 the hosts found, 3 the PS4's hello, 5 the login result,
 * 6 the host ID it accepted, 7 the connection, 8 a failure (0: refused, 1: a receive error,
 * 2: no PS4, 3: no hello, 4: no login answer), 9 a text field opened, 10 it closed, 16 no text
 * field, 17 the text typed, 18 and 19 what packets 22 and 24 carry.
 */
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <net/socket.h>
#include <net/inet.h>
#include "companion_util_internal.h"

namespace SceCompanionUtil {

CCompanionUtil *g_pCompanionUtil = NULL;
bool g_started = false;

static CompanionUtilSearchResult s_searchResult;


void CCompanionUtil::Reserved(void)
{
}

int CCompanionUtil::Shutdown(void)
{
	SceUInt32 i;
	int ret;

	if (m_callbackLock.IsInCallback())
		return COMPANION_UTIL_ERROR_IN_CALLBACK;
	if (m_terminate == 0)
		return COMPANION_UTIL_ERROR_NOT_STOPPED;
	if (m_threadCount != 0)
		return COMPANION_UTIL_ERROR_NOT_STOPPED;

	m_connectMode = -1;
	sceClibMemset(&m_target, 0, sizeof(m_target));
	m_targetSet = false;
	m_searchList.Release();
	if (m_hostIds != NULL) {
		for (i = 0; i < m_hostIdCount; i++)
			Free(i < m_hostIdCount ? m_hostIds[i] : m_hostIds[0]);
		if (m_hostIds != NULL) {
			Free(m_hostIds);
			m_hostIds = NULL;
		}
	}

	if (m_eventThread >= 0) {
		sceKernelWaitThreadEnd(m_eventThread, NULL, NULL);
		sceKernelDeleteThread(m_eventThread);
		m_eventThread = -1;
	}
	if (m_sendThread >= 0) {
		sceKernelWaitThreadEnd(m_sendThread, NULL, NULL);
		sceKernelDeleteThread(m_sendThread);
		m_sendThread = -1;
	}
	if (m_recvThread >= 0) {
		sceKernelWaitThreadEnd(m_recvThread, NULL, NULL);
		sceKernelDeleteThread(m_recvThread);
		m_recvThread = -1;
	}
	if (g_udpSocket >= 0)
		CUdpSocket::Close();
	if (g_tcpSocket >= 0)
		CTcpSocket::Close();
	sceClibMemset(&m_systemCallback, 0, sizeof(m_systemCallback) + sizeof(m_handlers));

	ret = DeleteMutex();
	if (ret >= 0)
		g_started = false;
	return ret;
}

/* a thread of the library, started */
static inline SceUID CreateThread(const char *name, SceKernelThreadEntry entry, int priority,
				  SceSize stackSize, int cpuAffinityMask)
{
	SceUID thid;
	int ret;

	thid = sceKernelCreateThread(name, entry, priority, stackSize, 0, cpuAffinityMask, NULL);
	if (thid >= 0) {
		ret = sceKernelStartThread(thid, 0, NULL);
		if (ret < 0)
			thid = ret;
	}
	return thid;
}

int CCompanionUtil::Start(const CompanionUtilInitParam *param)
{
	int priority, cpuAffinityMask;
	SceSize stackSize;
	int ret;

	ret = CreateMutex();
	if (ret < 0)
		return ret;

	cpuAffinityMask = param->threadCpuAffinityMask;
	m_terminate = 0;
	m_systemCallback = param->systemCallback;
	priority = param->threadPriority;
	stackSize = param->threadStackSize;
	m_wakeupRequested = false;
	m_launchRequested = false;
	if (priority == 0)
		priority = SCE_KERNEL_DEFAULT_PRIORITY_USER;
	if (stackSize == 0)
		stackSize = 0x4000;
	if (cpuAffinityMask == 0)
		cpuAffinityMask = 0;

	m_eventThread = CreateThread("SceCompanionUtilEventThread", EventThread, priority,
				     stackSize, cpuAffinityMask);
	if (m_eventThread < 0)
		return m_eventThread;
	m_threadCount++;
	m_sendThread = CreateThread("SceCompanionUtilSendThread", SendThread, priority, stackSize,
				    cpuAffinityMask);
	if (m_sendThread < 0)
		return m_sendThread;
	m_threadCount++;
	m_recvThread = CreateThread("SceCompanionUtilRecvThread", RecvThread, priority, stackSize,
				    cpuAffinityMask);
	if (m_recvThread < 0)
		return m_recvThread;
	m_threadCount++;

	g_started = true;
	m_state = STATE_READY;
	return 0;
}

void CCompanionUtil::Initialize(void)
{
	m_searchRecvJob.m_abort = false;
	m_wakeupSendJob.m_abort = false;
	m_launchSendJob.m_abort = false;
	m_searchSendJob.m_abort = false;
	m_wakeupRequested = false;
	m_launchRequested = false;
	m_state = STATE_NONE;
	m_textState = 0;
	m_terminate = 0;
	m_threadCount = 0;
	m_eventThread = -1;
	m_sendThread = -1;
	m_recvThread = -1;
	m_connectMode = -1;
	m_wakeStart = 0;
	m_searchTimer.m_period = 0;
	m_searchTimer.m_deadline = 0;
	m_resendTimer.m_period = 0;
	m_resendTimer.m_deadline = 0;
	m_stateTimer.m_period = 0;
	m_stateTimer.m_deadline = 0;
	m_keepAliveTimer.m_period = 0;
	m_keepAliveTimer.m_deadline = 0;
	m_aliveTimer.m_period = 0;
	m_aliveTimer.m_deadline = 0;
	m_wakeTimer.m_period = 0;
	m_wakeTimer.m_deadline = 0;
	m_retryTimer.m_period = 0;
	m_retryTimer.m_deadline = 0;
	m_searchSendJob.m_state = JOB_IDLE;
	m_searchSendJob.m_port = 0;
	m_launchSendJob.m_state = JOB_IDLE;
	m_wakeupSendJob.m_state = JOB_IDLE;
	m_searchRecvJob.m_state = JOB_IDLE;
	m_searchRecvJob.m_searchList = NULL;
	m_searchRecvJob.m_hosts.m_hosts = NULL;
	m_searchRecvJob.m_hosts.m_count = 0;
	m_searchRecvJob.m_hosts.m_capacity = 0;
	m_sendQueue.Init();
	m_recvJob.m_state = JOB_IDLE;
	m_recvJob.m_abort = false;
	m_recvJob.m_result = -1;
	m_recvJob.m_decrypt = false;
	m_recvJob.m_body = NULL;
	m_recvJob.m_header = NULL;
	m_recvJob.m_bodyBuffer = NULL;
	m_searchList.Init(256);
	m_hostIds = NULL;
	m_hostIdCount = 0;
	m_hostIdCapacity = 0;
	m_callbackLock.Init();
	sceClibMemset(&m_target, 0, sizeof(m_target));
	m_targetSet = false;
	sceClibMemset(&m_packet, 0, sizeof(m_packet));
	sceClibMemset(&m_systemCallback, 0, sizeof(m_systemCallback) + sizeof(m_handlers));
	sceClibMemset(m_body, 0, sizeof(m_body));
	m_textRequestTimer.m_period = 0;
	m_textRequested = false;
	m_textRequestTimer.m_deadline = 0;
	g_udpSocket = -1;
	g_tcpSocket = -1;
}

int CCompanionUtil::Init(const CompanionUtilInitParam *param)
{
	SceUInt32 i;
	int ret;

	if (g_pCompanionUtil != NULL)
		return COMPANION_UTIL_ERROR_ALREADY_INIT;
	if (param == NULL)
		return COMPANION_UTIL_ERROR_INVALID_ARGUMENT;
	if (param->workMemory == NULL)
		return COMPANION_UTIL_ERROR_INVALID_ARGUMENT;
	if (param->workMemorySize < 0x80000)
		return COMPANION_UTIL_ERROR_INVALID_ARGUMENT;
	if (param->threadPriority != 0 &&
	    (param->threadPriority < 64 || param->threadPriority > 191) &&
	    (param->threadPriority < SCE_KERNEL_DEFAULT_PRIORITY_USER - 31 ||
	     param->threadPriority > SCE_KERNEL_DEFAULT_PRIORITY_USER + 32))
		return COMPANION_UTIL_ERROR_INVALID_ARGUMENT;
	if (param->threadStackSize != 0 && param->threadStackSize < 0x4000)
		return COMPANION_UTIL_ERROR_INVALID_ARGUMENT;
	if (param->threadCpuAffinityMask != 0 && (param->threadCpuAffinityMask & ~0x70000) != 0)
		return COMPANION_UTIL_ERROR_INVALID_ARGUMENT;
	for (i = 0; i < sizeof(param->reserved); i++) {
		if (param->reserved[i] != 0)
			return COMPANION_UTIL_ERROR_INVALID_ARGUMENT;
	}

	CreateHeap(param->workMemory, param->workMemorySize);
	g_pCompanionUtil = (CCompanionUtil *)Allocate(sizeof(CCompanionUtil));
	if (g_pCompanionUtil == NULL)
		return COMPANION_UTIL_ERROR_NO_MEMORY;
	ret = CSearchList::CreateMutex();
	if (ret >= 0) {
		ret = CTcpSendJobQueue::CreateMutex();
		if (ret >= 0) {
			g_pCompanionUtil->Initialize();
			ret = g_pCompanionUtil->Start(param);
			if (ret < 0) {
				g_pCompanionUtil->Shutdown();
				g_pCompanionUtil->Finalize();
				Free(g_pCompanionUtil);
				g_pCompanionUtil = NULL;
				DestroyHeap();
			}
		}
	}
	return ret;
}

int CCompanionUtil::Term(void)
{
	int ret;

	if (g_pCompanionUtil == NULL)
		return COMPANION_UTIL_ERROR_NOT_INIT;
	ret = g_pCompanionUtil->Shutdown();
	if (ret < 0)
		return ret;
	g_pCompanionUtil->Finalize();
	ret = CTcpSendJobQueue::DeleteMutex();
	if (ret < 0)
		return ret;
	ret = CSearchList::DeleteMutex();
	if (ret < 0)
		return ret;
	Free(g_pCompanionUtil);
	g_pCompanionUtil = NULL;
	DestroyHeap();
	return ret;
}

void CCompanionUtil::Finalize(void)
{
	if (m_searchRecvJob.m_hosts.m_hosts != NULL) {
		Free(m_searchRecvJob.m_hosts.m_hosts);
		m_searchRecvJob.m_hosts.m_hosts = NULL;
	}
	m_searchRecvJob.m_hosts.m_count = 0;
	m_searchRecvJob.m_hosts.m_capacity = 0;
	m_sendQueue.Term();
	if (m_hostIds != NULL) {
		Free(m_hostIds);
		m_hostIds = NULL;
	}
	m_hostIdCount = 0;
	m_hostIdCapacity = 0;
	m_searchList.Term();
	m_callbackLock.Term();
}

int CCompanionUtil::Stop(void)
{
	int lock;

	if (!g_started)
		return COMPANION_UTIL_ERROR_NOT_INIT;
	if (m_callbackLock.IsInCallback())
		return COMPANION_UTIL_ERROR_IN_CALLBACK;
	if (m_state == STATE_STOPPING)
		return 0;
	if (m_state == STATE_STOPPED)
		return 0;

	lock = LockMutex();
	if (lock < 0)
		return 0;
	m_searchRecvJob.m_abort = true;
	m_wakeupSendJob.m_abort = true;
	m_launchSendJob.m_abort = true;
	m_searchSendJob.m_abort = true;
	m_sendQueue.Abort();
	m_recvJob.m_abort = true;
	if (g_tcpSocket >= 0)
		CTcpSocket::Abort();
	if (g_udpSocket >= 0)
		CUdpSocket::Abort();
	m_textState = 0;
	m_terminate = 1;
	m_state = STATE_STOPPING;
	UnlockMutex(lock);
	return 0;
}

int CCompanionUtil::Abort(void)
{
	if (g_pCompanionUtil != NULL)
		return g_pCompanionUtil->Stop();
	return COMPANION_UTIL_ERROR_NOT_INIT;
}

int CCompanionUtil::SetHostIds(const CompanionUtilHostIdList *list)
{
	SceUInt32 count, i;
	char *hostId;
	int length;

	if (list == NULL)
		return -1;
	if (list->hostIds == NULL || list->count == 0)
		return -1;
	count = list->count;
	if (count >= 10)
		return -2;
	if (m_hostIds != NULL)
		return -1;

	m_hostIds = (char **)Allocate(count * sizeof(char *));
	if (m_hostIds != NULL) {
		sceClibMemset(m_hostIds, 0, count * sizeof(char *));
		m_hostIdCapacity = count;
		m_hostIdCount = 0;
	}
	for (i = 0; i < list->count; i++) {
		length = sceClibStrnlen(list->hostIds[i], 17);
		if (length <= 0 || length > 16)
			return -3;
		hostId = (char *)Allocate(length + 1);
		sceClibMemcpy_safe(hostId, list->hostIds[i], length + 1);
		if (m_hostIdCount < m_hostIdCapacity) {
			sceClibMemcpy_safe(&m_hostIds[m_hostIdCount], &hostId, sizeof(hostId));
			m_hostIdCount++;
		}
	}
	return 0;
}

int CCompanionUtil::GetStatus(SceUInt16 *status)
{
	if (!g_started) {
		*status = STATUS_NONE;
		return COMPANION_UTIL_ERROR_NOT_INIT;
	}
	if (m_callbackLock.IsInCallback()) {
		*status = STATUS_NONE;
		return COMPANION_UTIL_ERROR_IN_CALLBACK;
	}

	switch (m_state) {
	case STATE_NONE:
		*status = STATUS_NONE;
		break;
	case STATE_READY:
	case STATE_SEARCHING:
	case STATE_CONNECT:
	case STATE_WAKING:
	case STATE_LAUNCHING:
	case STATE_HELLO:
	case STATE_LOGIN:
	case STATE_WAIT_LOGIN:
	case STATE_LOGIN_RESULT:
	case STATE_WAIT_TARGET:
	case STATE_RELOGIN:
	case STATE_WAIT_HOST_ID:
	case STATE_CONNECTED:
		*status = STATUS_RUNNING;
		break;
	case STATE_STOPPING:
		if (m_threadCount != 0) {
			*status = STATUS_RUNNING;
		} else {
			m_state = STATE_STOPPED;
			*status = STATUS_STOPPED;
		}
		break;
	case STATE_STOPPED:
		*status = STATUS_STOPPED;
		break;
	default:
		*status = STATUS_INVALID;
		break;
	}
	return 0;
}

int CCompanionUtil::QueryStatus(SceUInt16 *status)
{
	if (g_pCompanionUtil == NULL)
		return COMPANION_UTIL_ERROR_NOT_INIT;
	if (status != NULL)
		return g_pCompanionUtil->GetStatus(status);
	return COMPANION_UTIL_ERROR_INVALID_ARGUMENT;
}

int CCompanionUtil::SetCallbackHandler(SceCompanionUtilEventHandler handler, SceUInt16 index)
{
	CCompanionUtil *companionUtil = g_pCompanionUtil;
	int lock;

	if (companionUtil == NULL)
		return COMPANION_UTIL_ERROR_NOT_INIT;
	if (!g_started)
		return COMPANION_UTIL_ERROR_NOT_INIT;
	if (index >= 2)
		return -1;

	lock = LockMutex();
	if (lock < 0)
		return lock;
	companionUtil->m_handlers[index] = handler;
	UnlockMutex(lock);
	return 0;
}

/* the search request again, every second, once the last one is out */
static inline void ResendSearch(CCompanionUtil *companionUtil)
{
	if (companionUtil->m_searchTimer.IsExpired() &&
	    companionUtil->m_searchSendJob.m_state == JOB_DONE && g_udpSocket >= 0) {
		companionUtil->m_searchSendJob.Request(COMPANION_UTIL_DISCOVERY_PORT);
		companionUtil->m_searchTimer.Start(1);
	}
}

/* the hosts the last search found, to the handlers, and the next search */
static inline void ReportSearch(CCompanionUtil *companionUtil)
{
	if (companionUtil->m_searchRecvJob.m_state == JOB_DONE) {
		sceClibMemset(&s_searchResult, 0, sizeof(s_searchResult));
		companionUtil->m_searchList.GetResult(&s_searchResult);
		companionUtil->Notify(2, &s_searchResult);
		companionUtil->m_searchRecvJob.Request(&companionUtil->m_searchList,
						       companionUtil->GetSearchTimeout());
	}
}

int CCompanionUtil::SearchToOrbis(void)
{
	int ret;

	if (!g_started)
		return COMPANION_UTIL_ERROR_NOT_INIT;

	switch (m_state) {
	case STATE_READY:
		if (m_terminate != 0) {
			ret = COMPANION_UTIL_ERROR_STOPPING;
		} else if (g_tcpSocket >= 0 || g_udpSocket >= 0) {
			ret = COMPANION_UTIL_ERROR_INVALID_STATE;
		} else {
			m_searchList.Clear();
			if (CUdpSocket::Open() < 0) {
				ret = -1;
			} else if (g_udpSocket < 0) {
				ret = -3;
			} else {
				m_searchSendJob.Request(COMPANION_UTIL_DISCOVERY_PORT);
				m_searchTimer.Start(1);
				m_searchRecvJob.Request(&m_searchList, GetSearchTimeout());
				m_state = STATE_SEARCHING;
				ret = 0;
			}
		}
		break;
	case STATE_SEARCHING:
		ret = COMPANION_UTIL_ERROR_SEARCHING;
		break;
	case STATE_CONNECT:
	case STATE_WAKING:
	case STATE_LAUNCHING:
	case STATE_HELLO:
	case STATE_LOGIN:
	case STATE_WAIT_LOGIN:
	case STATE_LOGIN_RESULT:
	case STATE_WAIT_TARGET:
	case STATE_RELOGIN:
	case STATE_WAIT_HOST_ID:
		ret = COMPANION_UTIL_ERROR_CONNECTING;
		break;
	case STATE_CONNECTED:
		ret = COMPANION_UTIL_ERROR_CONNECTED;
		break;
	case STATE_STOPPING:
	case STATE_STOPPED:
		ret = COMPANION_UTIL_ERROR_STOPPING;
		break;
	default:
		ret = COMPANION_UTIL_ERROR_INVALID_STATE;
		break;
	}
	return ret;
}

int CCompanionUtil::Search(void)
{
	if (g_pCompanionUtil != NULL)
		return g_pCompanionUtil->SearchToOrbis();
	return COMPANION_UTIL_ERROR_NOT_INIT;
}

int CCompanionUtil::UpdateTarget(CompanionUtilTarget *target)
{
	SceUInt32 address;

	if (g_pCompanionUtil == NULL)
		return COMPANION_UTIL_ERROR_NOT_INIT;
	address = g_pCompanionUtil->m_searchList.GetAddress(target->hostId);
	if (address != 0 && address != target->address)
		target->address = address;
	return 0;
}

int CCompanionUtil::Connect(int mode, const CompanionUtilTarget *target,
			    const CompanionUtilHostIdList *list)
{
	if (!g_started)
		return COMPANION_UTIL_ERROR_NOT_INIT;

	if (m_state == STATE_WAIT_TARGET) {
		if (g_udpSocket < 0)
			return -1;
		if (m_targetSet == true)
			return -1;
		if (m_connectMode != 1 && m_connectMode != 0)
			return -1;
		sceClibMemcpy_safe(&m_target, target, sizeof(m_target));
		m_state = STATE_RELOGIN;
		return 0;
	}

	if (m_state != STATE_READY && m_state != STATE_SEARCHING)
		return -1;
	if (list != NULL)
		SetHostIds(list);
	if (g_udpSocket < 0) {
		if (CUdpSocket::Open() < 0)
			return -1;
		if (g_udpSocket >= 0) {
			m_searchSendJob.Request(COMPANION_UTIL_DISCOVERY_PORT);
			m_searchTimer.Start(1);
		}
	}
	m_connectMode = mode;
	m_targetSet = true;
	sceClibMemcpy_safe(&m_target, target, sizeof(m_target));
	m_resendTimer.Start(2);
	if (m_connectMode == 2) {
		if (m_state == STATE_READY) {
			m_searchRecvJob.Request(&m_searchList, GetSearchTimeout());
			m_state = STATE_SEARCHING;
		}
	} else {
		m_state = STATE_CONNECT;
	}
	return 0;
}

int CCompanionUtil::ConnectToOrbis(const CompanionUtilTarget *target,
				   const CompanionUtilHostIdList *list)
{
	if (g_pCompanionUtil != NULL)
		return g_pCompanionUtil->Connect(1, target, list);
	return COMPANION_UTIL_ERROR_NOT_INIT;
}

int CCompanionUtil::LaunchOrbis(const CompanionUtilTarget *target)
{
	if (g_pCompanionUtil != NULL)
		return g_pCompanionUtil->Connect(0, target, NULL);
	return COMPANION_UTIL_ERROR_NOT_INIT;
}

int CCompanionUtil::PowerOnOrbisForRemotePlay(const CompanionUtilTarget *target)
{
	if (g_pCompanionUtil != NULL)
		return g_pCompanionUtil->Connect(2, target, NULL);
	return COMPANION_UTIL_ERROR_NOT_INIT;
}

int CCompanionUtil::RequestText(void)
{
	CCompanionUtil *companionUtil = g_pCompanionUtil;
	int lock, ret;

	if (companionUtil == NULL)
		return COMPANION_UTIL_ERROR_NOT_INIT;
	if (!g_started)
		return COMPANION_UTIL_ERROR_NOT_INIT;
	if (companionUtil->m_state != STATE_CONNECTED)
		return -1;
	if (companionUtil->m_textState == 2)
		return -4;
	if (g_tcpSocket < 0)
		return -2;
	if (!g_sessionReady)
		return -3;

	lock = LockMutex();
	if (lock < 0)
		return lock;
	if (companionUtil->m_textRequested) {
		ret = -4;
	} else {
		ret = companionUtil->m_sendQueue.Push(15, NULL, NULL);
		if (ret >= 0) {
			companionUtil->m_textRequested = true;
			companionUtil->m_textRequestTimer.Start(10);
		}
	}
	UnlockMutex(lock);
	return ret;
}

int CCompanionUtil::SendText(int composeStart, int composeLength, SceUInt32 caret,
			     int replaceStart, int replaceLength, const SceWChar16 *text)
{
	CompanionUtilRequestParam *param;
	const SceWChar16 *p;
	SceUInt32 length;
	SceSize size;
	int lock, ret;

	if (!g_started)
		return COMPANION_UTIL_ERROR_NOT_INIT;
	if (g_tcpSocket < 0)
		return -2;
	if (m_textState != 2)
		return -1;
	if (text == NULL)
		return -3;

	lock = LockMutex();
	if (lock < 0)
		return lock;
	length = 0;
	for (p = text; *p != 0; p++)
		length++;
	size = length * sizeof(SceWChar16) + sizeof(SceWChar16);
	param = (CompanionUtilRequestParam *)Allocate(sizeof(*param));
	sceClibMemset(param, 0, sizeof(*param));
	param->text.composeStart = composeStart;
	param->text.composeLength = composeLength;
	param->text.caret = caret;
	param->text.replaceStart = replaceStart;
	param->text.replaceLength = replaceLength;
	param->text.text = (SceWChar16 *)Allocate(size);
	sceClibMemcpy_safe(param->text.text, text, size);
	ret = m_sendQueue.Push(17, param, ReleaseTextParam);
	if (ret < 0) {
		Free(param->text.text);
		Free(param);
	}
	UnlockMutex(lock);
	return ret;
}

int CCompanionUtil::InputText(int composeStart, int composeLength, SceUInt32 caret,
			       int replaceStart, int replaceLength, const SceWChar16 *text)
{
	if (g_pCompanionUtil != NULL)
		return g_pCompanionUtil->SendText(composeStart, composeLength, caret, replaceStart,
						  replaceLength, text);
	return COMPANION_UTIL_ERROR_NOT_INIT;
}

int CCompanionUtil::CloseText(int entered)
{
	CCompanionUtil *companionUtil = g_pCompanionUtil;
	CompanionUtilRequestParam *param;
	int lock, ret;

	if (companionUtil == NULL)
		return COMPANION_UTIL_ERROR_NOT_INIT;
	if (!g_started)
		return COMPANION_UTIL_ERROR_NOT_INIT;
	if (g_tcpSocket < 0)
		return -2;
	if (companionUtil->m_textState != 2)
		return -1;

	lock = LockMutex();
	if (lock < 0)
		return lock;
	if (companionUtil->m_textRequested) {
		ret = -4;
	} else {
		param = (CompanionUtilRequestParam *)Allocate(sizeof(*param));
		sceClibMemset(param, 0, sizeof(*param));
		param->value.arg[0] = entered;
		ret = companionUtil->m_sendQueue.Push(18, param, ReleaseCloseParam);
		if (ret >= 0)
			companionUtil->m_textState = 1;
		else
			Free(param);
	}
	UnlockMutex(lock);
	return ret;
}

int CCompanionUtil::SendValue(int arg0, int arg1)
{
	CCompanionUtil *companionUtil = g_pCompanionUtil;
	CompanionUtilRequestParam *param;
	int lock, ret;

	if (companionUtil == NULL)
		return COMPANION_UTIL_ERROR_NOT_INIT;
	if (!g_started)
		return COMPANION_UTIL_ERROR_NOT_INIT;
	if (g_tcpSocket < 0)
		return -2;

	lock = LockMutex();
	if (lock < 0)
		return lock;
	param = (CompanionUtilRequestParam *)Allocate(sizeof(*param));
	sceClibMemset(param, 0, sizeof(*param));
	param->value.arg[0] = arg0;
	param->value.arg[1] = arg1;
	ret = companionUtil->m_sendQueue.Push(24, param, ReleaseValueParam);
	if (ret < 0)
		Free(param);
	UnlockMutex(lock);
	return ret;
}

/* a keep-alive, every ten seconds */
static inline void KeepAlive(CCompanionUtil *companionUtil)
{
	companionUtil->m_sendQueue.Push(14, NULL, CCompanionUtil::ReleaseParam);
	companionUtil->m_keepAliveTimer.Start(10);
}

int CCompanionUtil::UpdateConnected(void)
{
	CompanionUtilTextEvent textEvent;
	int values[3];
	int value;
	int length;
	bool requested, expired;
	int lock;

	if (g_tcpSocket < 0)
		return -1;
	if (m_keepAliveTimer.IsExpired())
		KeepAlive(this);

	if (m_recvJob.m_state == JOB_DONE) {
		if (m_recvJob.m_result == 0 ||
		    m_recvJob.m_result == COMPANION_UTIL_NET_ERROR_UNEXPECTED) {
			switch (m_packet.type) {
			case 18:
				m_aliveTimer.Start(60);
				break;
			case 24:
				sceClibMemset(&value, 0, sizeof(value));
				value = m_packet.arg[0];
				Notify(19, &value);
				break;
			case 13:
				lock = LockMutex();
				if (lock < 0)
					break;
				requested = false;
				if (m_textRequested) {
					requested = true;
					m_textRequested = false;
				}
				UnlockMutex(lock);
				if (!requested)
					break;
				if (m_packet.arg[0] == 0) {
					length = GetTextSize(&m_packet);
					sceClibMemset(&textEvent, 0, sizeof(textEvent));
					textEvent.text = m_body;
					textEvent.length = length;
					textEvent.arg[0] = m_packet.arg[1];
					textEvent.arg[1] = m_packet.arg[2];
					Notify(9, &textEvent);
					sceClibMemset(m_body, 0, sizeof(m_body));
					m_textState = 2;
				} else {
					Notify(16, NULL);
				}
				break;
			case 14:
				length = GetBodySize(&m_packet);
				sceClibMemset(&textEvent, 0, sizeof(textEvent));
				textEvent.text = m_body;
				textEvent.length = length;
				Notify(17, &textEvent);
				sceClibMemset(m_body, 0, sizeof(m_body));
				break;
			case 16:
				Notify(10, NULL);
				m_textState = 1;
				break;
			case 22:
				sceClibMemset(values, 0, sizeof(values));
				values[0] = m_packet.arg[0];
				values[1] = m_packet.arg[1];
				values[2] = m_packet.arg[2];
				Notify(18, values);
				break;
			case 27:
			case 28:
			case 32:
			case 35:
				break;
			default:
				break;
			}
			m_recvJob.Request(&m_packet, m_body, true);
		} else {
			return Lose();
		}
	}

	expired = false;
	lock = LockMutex();
	if (lock >= 0) {
		if (m_textRequested && m_textRequestTimer.IsExpired()) {
			expired = true;
			m_textRequested = false;
		}
		UnlockMutex(lock);
		if (expired)
			Notify(16, NULL);
	}
	if (m_aliveTimer.IsExpired())
		return Lose();
	return 0;
}

bool CCompanionUtil::IsRecvFailed(void)
{
	switch (m_recvJob.m_result) {
	case 0:
	case COMPANION_UTIL_NET_ERROR_IO:
	case COMPANION_UTIL_NET_ERROR_UNEXPECTED:
	case COMPANION_UTIL_NET_ERROR_BAD_HELLO:
	case COMPANION_UTIL_NET_ERROR_REFUSED:
	case COMPANION_UTIL_NET_ERROR_LOGIN:
		return false;
	case COMPANION_UTIL_NET_ERROR_BAD_SIZE:
	case COMPANION_UTIL_NET_ERROR_DENIED:
	case COMPANION_UTIL_NET_ERROR_UNKNOWN_TYPE:
		return true;
	default:
		return true;
	}
}

bool CCompanionUtil::PostRequest(int type, const CompanionUtilRequestParam *param)
{
	CompanionUtilRequestParam *copy;

	m_recvJob.Request(&m_packet, m_body, true);
	copy = NULL;
	if (param != NULL) {
		copy = (CompanionUtilRequestParam *)Allocate(sizeof(*copy));
		sceClibMemcpy_safe(copy, param, sizeof(*copy));
	}
	if (m_sendQueue.Push(type, copy, ReleaseParam) >= 0)
		return true;
	if (copy != NULL)
		Free(copy);
	return false;
}

int CCompanionUtil::HandleLoginResult(void)
{
	int result[2];
	int ret = 0;

	sceClibMemset(result, 0, sizeof(result));
	result[0] = m_packet.arg[0];
	result[1] = m_packet.arg[1];
	Notify(5, result);

	switch (m_packet.arg[0]) {
	case 0:
		CUdpSocket::Close();
		m_sendQueue.Push(21, NULL, ReleaseParam);
		sceClibMemset(&m_target, 0, sizeof(m_target));
		if (m_connectMode == 1 && m_hostIds != NULL && m_hostIdCount != 0) {
			m_state = STATE_SEND_HOST_ID;
		} else {
			Notify(7, NULL);
			m_recvJob.Request(&m_packet, m_body, true);
			m_state = STATE_CONNECTED;
			m_textState = 1;
		}
		break;
	case 6:
	case 20:
	case 21:
	case 22:
	case 23:
	case 24:
		m_state = STATE_WAIT_TARGET;
		m_recvJob.Request(&m_packet, m_body, true);
		break;
	case 7:
	case 8:
	case 17:
	case 30:
		ret = -1;
		break;
	default:
		ret = -1;
		break;
	}
	return ret;
}

/* where the target is now, from the search, as it sends to it */
static inline SceUInt32 GetTargetAddress(CCompanionUtil *companionUtil)
{
	SceUInt32 address, current;

	address = companionUtil->m_searchList.GetAddress(companionUtil->m_target.hostId);
	current = companionUtil->m_target.address;
	if (address != 0 && current != address) {
		companionUtil->m_target.address = address;
		current = address;
	}
	return current;
}

int CCompanionUtil::SendLaunch(void)
{
	SceNetSockaddrIn sin;
	SceUInt32 address;

	if (g_udpSocket < 0)
		return -1;
	if (!m_targetSet)
		return -2;
	if (m_launchSendJob.m_state == JOB_RUNNING)
		return -3;

	address = GetTargetAddress(this);
	sceClibMemset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = SCE_NET_AF_INET;
	sin.sin_port = sceNetHtons(COMPANION_UTIL_DISCOVERY_PORT);
	sin.sin_vport = 0;
	sin.sin_addr.s_addr = address;
	m_launchRequested = true;
	m_launchSendJob.Request(&sin, 0, 0);
	m_resendTimer.Start(2);
	return 0;
}

int CCompanionUtil::SendWakeup(bool remotePlay)
{
	SceNetSockaddrIn sin;
	SceUInt32 address;

	if (g_udpSocket < 0)
		return -1;
	if (!m_targetSet)
		return -2;
	if (m_wakeupSendJob.m_state == JOB_RUNNING)
		return -3;
	if (m_searchList.GetStatus(m_target.hostId) == 1)
		return -4;

	address = GetTargetAddress(this);
	sceClibMemset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = SCE_NET_AF_INET;
	sin.sin_port = sceNetHtons(COMPANION_UTIL_DISCOVERY_PORT);
	sin.sin_vport = 0;
	sin.sin_addr.s_addr = address;
	m_wakeupRequested = true;
	m_wakeupSendJob.Request(&sin, remotePlay ? 2 : 1, m_target.userCredential);
	m_resendTimer.Start(2);
	return 0;
}

int CCompanionUtil::Update(void)
{
	CompanionUtilRequestParam param;
	SceUInt32 address;
	SceUInt64 wakeStart;
	int hello[2];
	int hostId;
	int status;

	switch (m_state) {
	case STATE_SEARCHING:
		if (g_udpSocket >= 0) {
			ResendSearch(this);
			if (m_connectMode == 2 && m_resendTimer.IsExpired())
				SendWakeup(true);
		}
		ReportSearch(this);
		break;

	case STATE_CONNECT:
		if (m_searchList.GetStatus(m_target.hostId) == 2) {
			m_searchRecvJob.m_abort = true;
			m_wakeTimer.Start(30);
			m_wakeStart = 0;
			m_retryTimer.Start(1);
			m_wakeupRequested = true;
			m_state = STATE_WAKING;
		} else {
			m_stateTimer.Start(25);
			m_state = STATE_LAUNCHING;
		}
		break;

	case STATE_WAKING:
		if (g_udpSocket >= 0) {
			ResendSearch(this);
			if (m_resendTimer.IsExpired() && !m_wakeTimer.IsExpired())
				SendWakeup(false);
		}
		ReportSearch(this);
		status = m_searchList.GetStatus(m_target.hostId);
		if (status == 0)
			return Fail(2);
		if (status == 1) {
			m_searchRecvJob.m_abort = true;
			m_wakeupRequested = false;
			m_stateTimer.Start(25);
			m_state = STATE_LAUNCHING;
			break;
		}
		if (!m_wakeTimer.IsExpired())
			break;
		wakeStart = m_wakeStart;
		if (wakeStart == 0) {
			wakeStart = CTimer::GetTime();
			m_wakeStart = wakeStart;
		}
		if (m_searchList.IsSeenAfter(m_target.hostId, wakeStart))
			return Fail(2);
		if (m_retryTimer.IsExpired()) {
			m_searchRecvJob.m_abort = true;
			m_retryTimer.Start(1);
		}
		break;

	case STATE_LAUNCHING:
		if (g_udpSocket >= 0) {
			ResendSearch(this);
			if (m_resendTimer.IsExpired())
				SendLaunch();
		}
		ReportSearch(this);
		status = m_searchList.GetStatus(m_target.hostId);
		if (status == 0 || status == 2)
			return Fail(2);
		if (m_stateTimer.IsExpired())
			return Fail(2);
		if (m_launchRequested && m_launchSendJob.m_state == JOB_DONE) {
			m_launchRequested = false;
			address = GetTargetAddress(this);
			if (CTcpSocket::Connect(&address, &m_target.port) >= 0) {
				m_searchRecvJob.m_abort = true;
				m_targetSet = false;
				m_recvJob.Request(&m_packet, m_body, false);
				m_sendQueue.Push(0, NULL, ReleaseParam);
				m_stateTimer.Start(10);
				m_state = STATE_HELLO;
			}
		}
		break;

	case STATE_HELLO:
		if (m_stateTimer.IsExpired())
			return Fail(3);
		if (m_recvJob.m_state != JOB_DONE)
			break;
		if (IsRecvFailed() || m_recvJob.m_result == COMPANION_UTIL_NET_ERROR_UNEXPECTED)
			return Fail(1);
		if (m_packet.type != COMPANION_UTIL_PACKET_HELLO) {
			m_recvJob.Request(&m_packet, m_body, false);
			break;
		}
		sceClibMemset(hello, 0, sizeof(hello));
		hello[0] = m_packet.arg[0];
		hello[1] = m_packet.arg[1];
		Notify(3, hello);
		switch (m_packet.arg[1]) {
		case 0:
			m_state = STATE_SEND_KEY;
			break;
		case 2:
			return Fail(0);
		case 3:
			return Fail(0);
		case 1:
		default:
			return Fail(0);
		}
		break;

	case STATE_LOGIN:
		sceClibMemset(&param, 0, sizeof(param));
		switch (m_connectMode) {
		case 0:
			param.login.mode = 0;
			break;
		case 1:
			param.login.mode = 1;
			break;
		}
		sceClibMemcpy_safe(param.login.registKey, m_target.registKey,
				   sizeof(param.login.registKey));
		param.login.userCredential = m_target.userCredential;
		PostRequest(25, &param);
		m_stateTimer.Start(60);
		m_keepAliveTimer.Start(1);
		m_aliveTimer.Start(60);
		m_state = STATE_WAIT_LOGIN;
		break;

	case STATE_SEND_KEY:
		m_sendQueue.Push(26, NULL, ReleaseParam);
		m_state = STATE_LOGIN;
		break;

	case STATE_WAIT_LOGIN:
		if (m_stateTimer.IsExpired())
			return Fail(4);
		if (m_aliveTimer.IsExpired())
			return Lose();
		if (m_recvJob.m_state != JOB_DONE) {
			if (m_keepAliveTimer.IsExpired())
				KeepAlive(this);
			break;
		}
		if (IsRecvFailed())
			return Fail(1);
		if (m_packet.type == 7) {
			m_state = STATE_LOGIN_RESULT;
			break;
		}
		if (m_packet.type == 18)
			m_aliveTimer.Start(60);
		m_recvJob.Request(&m_packet, m_body, true);
		break;

	case STATE_LOGIN_RESULT:
		if (HandleLoginResult() < 0)
			return Fail(0);
		break;

	case STATE_WAIT_TARGET:
		if (m_aliveTimer.IsExpired())
			return Lose();
		if (m_recvJob.m_state != JOB_DONE) {
			if (m_keepAliveTimer.IsExpired())
				KeepAlive(this);
			break;
		}
		if (IsRecvFailed())
			return Fail(1);
		if (m_packet.type == 18)
			m_aliveTimer.Start(60);
		m_recvJob.Request(&m_packet, m_body, true);
		break;

	case STATE_RELOGIN:
		if (m_aliveTimer.IsExpired())
			return Lose();
		if (m_recvJob.m_state != JOB_DONE) {
			if (m_keepAliveTimer.IsExpired())
				KeepAlive(this);
			break;
		}
		if (IsRecvFailed())
			return Fail(1);
		m_state = STATE_LOGIN;
		break;

	case STATE_SEND_HOST_ID:
		sceClibMemset(&param, 0, sizeof(param));
		param.host.hostId = m_hostIds[0];
		PostRequest(11, &param);
		m_stateTimer.Start(60);
		m_state = STATE_WAIT_HOST_ID;
		break;

	case STATE_WAIT_HOST_ID:
		if (m_stateTimer.IsExpired())
			return Lose();
		if (m_aliveTimer.IsExpired())
			return Lose();
		if (m_recvJob.m_state != JOB_DONE) {
			if (m_keepAliveTimer.IsExpired())
				KeepAlive(this);
			break;
		}
		if (IsRecvFailed())
			return Fail(1);
		if (m_packet.type == 11) {
			sceClibMemset(&hostId, 0, sizeof(hostId));
			hostId = m_packet.arg[0];
			Notify(6, &hostId);
			Notify(7, NULL);
			m_recvJob.Request(&m_packet, m_body, true);
			m_state = STATE_CONNECTED;
			m_textState = 1;
		} else {
			if (m_packet.type == 18)
				m_aliveTimer.Start(60);
			m_recvJob.Request(&m_packet, m_body, true);
		}
		break;

	case STATE_CONNECTED:
		UpdateConnected();
		break;

	case STATE_STOPPING:
		return -1;

	default:
		break;
	}
	return 0;
}

int CCompanionUtil::EventThread(SceSize argSize, void *argBlock)
{
	CCompanionUtil *companionUtil = g_pCompanionUtil;
	int lock;

	companionUtil->m_callbackLock.SetOwner();
	while (companionUtil->m_terminate == 0) {
		companionUtil->Update();
		sceKernelDelayThread(10000);
	}
	companionUtil->m_callbackLock.ClearOwner();
	lock = LockMutex();
	companionUtil->m_threadCount--;
	UnlockMutex(lock);
	return 0;
}

int CCompanionUtil::RunSendThread(void)
{
	int lock;

	while (m_terminate == 0 || m_searchSendJob.m_state == JOB_RUNNING ||
	       m_launchSendJob.m_state == JOB_RUNNING || m_wakeupSendJob.m_state == JOB_RUNNING ||
	       m_sendQueue.GetCount() != 0) {
		if (m_searchSendJob.m_state == JOB_RUNNING) {
			m_searchSendJob.Run();
			m_searchSendJob.Finish();
		}
		if (m_launchSendJob.m_state == JOB_RUNNING) {
			m_launchSendJob.Run();
			m_launchSendJob.Finish();
		}
		if (m_wakeupSendJob.m_state == JOB_RUNNING) {
			m_wakeupSendJob.Run();
			m_wakeupSendJob.Finish();
		}
		m_sendQueue.Run(false);
		sceKernelDelayThread(1000);
	}
	if (g_tcpSocket >= 0) {
		m_sendQueue.Push(4, NULL, NULL);
		m_sendQueue.Run(true);
	}
	lock = LockMutex();
	m_threadCount--;
	UnlockMutex(lock);
	return 0;
}

int CCompanionUtil::SendThread(SceSize argSize, void *argBlock)
{
	return g_pCompanionUtil->RunSendThread();
}

int CCompanionUtil::RecvThread(SceSize argSize, void *argBlock)
{
	return g_pCompanionUtil->RunRecvThread();
}

int CCompanionUtil::RunRecvThread(void)
{
	int lock;

	while (m_terminate == 0 || m_searchRecvJob.m_state == JOB_RUNNING ||
	       m_recvJob.m_state == JOB_RUNNING) {
		if (m_searchRecvJob.m_state == JOB_RUNNING)
			m_searchRecvJob.Finish(m_searchRecvJob.Run());
		if (m_recvJob.m_state == JOB_RUNNING)
			m_recvJob.Finish(m_recvJob.Run());
		sceKernelDelayThread(1000);
	}
	lock = LockMutex();
	m_threadCount--;
	UnlockMutex(lock);
	return 0;
}

void CCompanionUtil::ReleaseTextParam(void *param)
{
	Free(((CompanionUtilRequestParam *)param)->text.text);
	Free(param);
}

void CCompanionUtil::ReleaseCloseParam(void *param)
{
	Free(param);
}

void CCompanionUtil::ReleaseValueParam(void *param)
{
	Free(param);
}

void CCompanionUtil::ReleaseParam(void *param)
{
	if (param != NULL)
		Free(param);
}

} /* namespace SceCompanionUtil */
