/*
 * The contexts ("CTXT:"): one for each request of the API's callers, from its Begin to its
 * End. The task queue holds the contexts of queued requests; the context manager holds the
 * one running and the cancel, terminate and pause requests beside it. A request with a data
 * phase passes the host's data between the device thread and its caller through an event
 * flag of its own.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <kernel/error.h>
#include <rtc.h>
#include <common/shared_ptr.h>
#include <thread/thread.h>
#include "mtp_internal.h"

using paf::common::SharedPtr;

#define MTP_TAG_CTXT			"CTXT:"

/* a context's state */
#define MTP_CONTEXT_STATE_IDLE		0
#define MTP_CONTEXT_STATE_DONE		1
#define MTP_CONTEXT_STATE_IN_USE	2
#define MTP_CONTEXT_STATE_INVALIDATED	3
#define MTP_CONTEXT_STATE_DELETED	4

/* a context's event flag */
#define MTP_CONTEXT_EVF_DONE		0x10
#define MTP_CONTEXT_EVF_CANCELED	0x20
#define MTP_CONTEXT_EVF_INVALIDATED	0x40

/* a data phase's event flag */
#define MTP_CONTEXT_SR_TERMINATE	0x01
#define MTP_CONTEXT_SR_REQUEST		0x02	/* the caller's buffer is ready */
#define MTP_CONTEXT_SR_RECV		0x04	/* data was received into it */
#define MTP_CONTEXT_SR_SENT		0x08	/* data was sent from it */
#define MTP_CONTEXT_SR_RESPONSE		0x10	/* the host's response arrived */

#define MTP_CONTEXT_ID_MAX		0x80000000
#define MTP_CONTEXT_TIMEOUT		10000000	/* a context's, unused */
#define MTP_CONTEXT_WAIT_TIMEOUT	10000000
#define MTP_CONTEXT_SR_TIMEOUT		30000000
#define MTP_CONTEXT_SR_TIMEOUT_NET	15000000
#define MTP_CONTEXT_RESPONSE_TIMEOUT	500000
#define MTP_CONTEXT_PASS_MAX		0x10000

/* the operation that receives into the caller's words only */
#define MTP_OPERATION_VENDOR_C123	0xC123
/* the event that tells the host a request was canceled */
#define MTP_EVENT_VENDOR_CANCEL		0xC108

static SceUInt32 s_nextContextId = 1;
static SceUInt32 s_pauseContextId = 1;
static paf::thread::Mutex *s_dataMutex = NULL;	/* a data phase's caller buffer */
static MtpContextManager *s_contextManager;

int mtpContextInitialize(void)
{
	int ret;

	if (s_contextManager != NULL) {
		MTP_ERR(MTP_TAG_CTXT, 158, "already initialized\n");
		return SCE_MTP_ERROR_ALREADY;
	}
	s_dataMutex = new paf::thread::Mutex("SceMtpContextMutex", 0);
	if (s_dataMutex == NULL)
		return SCE_MTP_ERROR_NO_MEMORY;
	s_contextManager = new (g_mtpUtilHeap) MtpContextManager;
	if (s_contextManager == NULL) {
		MTP_ERR(MTP_TAG_CTXT, 170, "no memory\n");
		delete s_dataMutex;
		s_dataMutex = NULL;
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	ret = s_contextManager->initialize();
	if (ret < 0) {
		delete s_dataMutex;
		s_dataMutex = NULL;
		delete s_contextManager;
		s_contextManager = NULL;
		return ret;
	}
	return 0;
}

int mtpContextFinalize(void)
{
	int ret;

	if (s_contextManager == NULL) {
		MTP_ERR(MTP_TAG_CTXT, 191, "not initialized\n");
		return SCE_MTP_ERROR_DEVICE;
	}
	ret = s_contextManager->finalize();
	delete s_dataMutex;
	s_dataMutex = NULL;
	delete s_contextManager;
	s_contextManager = NULL;
	return ret;
}

MtpContext::MtpContext()
{
	id = 0;
	eventFlag = -1;
	data = NULL;
	dataSize = 0;
	result = 0;
	state = MTP_CONTEXT_STATE_IDLE;
	resultParam = 0;
	useCount = 0;
	sendRecv = NULL;
	freeData = NULL;
	onResult = NULL;
	opcode = 0;
	startTick = 0;
	timeout = 0;
	progress = NULL;
	cancelHandler = NULL;
	cancelHandlerArg = NULL;
	param = 0;
	objectHandle = 0;
	pauseData[0] = NULL;
	pauseData[1] = NULL;
	checkVersion = 1;
	sce_paf_memset(work, 0, sizeof(work));
}

static inline int createContextSendRecvData(MtpContext *context)
{
	MtpContextSendRecvData *data;
	SceUID evf;

	data = (MtpContextSendRecvData *)mtpUtilMalloc(sizeof(*data));
	if (data == NULL) {
		MTP_ERR(MTP_TAG_CTXT, 1043, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	mtpUtilMemset(data, 0, sizeof(*data));
	evf = sceKernelCreateEventFlag("SceMtpContextSendRecvData", SCE_KERNEL_EVF_ATTR_MULTI, 0,
				       NULL);
	if (evf <= 0) {
		MTP_ERR(MTP_TAG_CTXT, 1050, "sceKernelCreateEventFlag failed: 0x%x\n", evf);
		mtpUtilFree(data);
		return evf != 0 ? evf : SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	data->eventFlag = evf;
	context->sendRecv = data;
	return 0;
}

static inline void deleteContextSendRecvData(MtpContext *context)
{
	MtpContextSendRecvData *data = context->sendRecv;
	int ret;

	if (data == NULL)
		return;
	if (data->eventFlag != -1) {
		ret = sceKernelDeleteEventFlag(data->eventFlag);
		if (ret < 0)
			MTP_ERR(MTP_TAG_CTXT, 1073, "sceKernelDeleteEventFlag failed: 0x%x, 0x%x\n",
				ret, data->eventFlag);
		data->eventFlag = -1;
	}
	mtpUtilFree(data);
	context->sendRecv = NULL;
}

int createContext(int kind, MtpContext **context)
{
	MtpContext *created;
	SceRtcTick tick;
	SceUID evf;
	int ret;

	created = new (g_mtpUtilHeap) MtpContext;
	if (created == NULL) {
		MTP_ERR(MTP_TAG_CTXT, 1095, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	if (kind != 0) {
		ret = createContextSendRecvData(created);
		if (ret < 0) {
			delete created;
			return ret;
		}
	}
	evf = sceKernelCreateEventFlag("SceMtpContext", SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (evf <= 0) {
		MTP_ERR(MTP_TAG_CTXT, 1110, "sceKernelCreateEventFlag failed: 0x%x\n", evf);
		deleteContextSendRecvData(created);
		delete created;
		return evf != 0 ? evf : SCE_MTP_ERROR_NOT_INITIALIZED;
	}
	sceRtcGetCurrentTick(&tick);
	created->eventFlag = evf;
	created->id = s_nextContextId++;
	created->data = NULL;
	created->dataSize = 0;
	created->result = MTP_RESPONSE_UNDEFINED;
	created->state = MTP_CONTEXT_STATE_IDLE;
	created->startTick = tick.tick;
	created->timeout = MTP_CONTEXT_TIMEOUT;
	if (s_nextContextId == MTP_CONTEXT_ID_MAX)
		s_nextContextId = 1;
	*context = created;
	return 0;
}

int mtpContextCreate(int kind, MtpContext **context, int opcode)
{
	int ret;

	if (s_contextManager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	s_contextManager->lock();
	ret = createContext(kind, context);
	s_contextManager->unlock();
	return ret;
}

int mtpContextIsIdle(int *idle)
{
	MtpContextManager *manager = s_contextManager;

	if (manager == NULL || idle == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	manager->lock();
	*idle = manager->m_current.get() == NULL;
	s_contextManager->unlock();
	return 0;
}

int mtpContextCreateCancel(MtpContext **context, int opcode)
{
	MtpContextManager *manager = s_contextManager;
	int ret;

	if (manager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	manager->lock();
	if (manager->m_cancel.get() != NULL) {
		MTP_ERR(MTP_TAG_CTXT, 263,
			"CURRENT LIMITATION: there is one cancel request currently running\n");
		s_contextManager->unlock();
		return SCE_MTP_ERROR_NOT_STOPPED;
	}
	ret = createContext(0, context);
	s_contextManager->unlock();
	return ret;
}

int mtpContextCreateTerminate(MtpContext **context, int opcode)
{
	int ret;

	if (s_contextManager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	s_contextManager->lock();
	ret = createContext(0, context);
	s_contextManager->unlock();
	return ret;
}

int mtpContextCreatePause(MtpContext **context, int opcode)
{
	MtpContextManager *manager = s_contextManager;
	int ret;

	if (manager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	manager->lock();
	if (manager->m_pause.get() != NULL) {
		MTP_ERR(MTP_TAG_CTXT, 309,
			"CURRENT LIMITATION: there is one pause request currently running\n");
		s_contextManager->unlock();
		return SCE_MTP_ERROR_NOT_STOPPED;
	}
	ret = createContext(0, context);
	s_contextManager->unlock();
	return ret;
}

void mtpContextRegister(SharedPtr<MtpContext> context)
{
	s_contextManager->registerContext(context);
}

void mtpContextDelete(MtpContext *context)
{
	MtpContextManager *manager = s_contextManager;

	if (manager == NULL) {
		MTP_ERR(MTP_TAG_CTXT, 332, "invalid status\n");
		return;
	}
	if (context == NULL)
		return;
	manager->lock();
	if (context->useCount != 0) {
		MTP_ERR(MTP_TAG_CTXT, 341,
			"context id: %u is still being used count: %u, status: %u \n", context->id,
			context->useCount, context->state);
		context->state = MTP_CONTEXT_STATE_DELETED;
		s_contextManager->unlock();
		return;
	}
	if (manager->m_current.get() == context)
		manager->m_current.reset();
	else if (manager->m_cancel.get() == context)
		manager->m_cancel.reset();
	else if (manager->m_terminate.get() == context)
		manager->m_terminate.reset();
	else if (manager->m_pause.get() == context)
		manager->m_pause.reset();
	else
		MTP_ERR(MTP_TAG_CTXT, 358, "CURRENT LIMITATION: wrong context %u ??\n", context->id);
	s_contextManager->unlock();
}

/* take a context for its caller: one not running, or one already taken by another */
static inline int useContext(MtpContext *context)
{
	int state = context->state;

	if (state == MTP_CONTEXT_STATE_IDLE ||
	    (state == MTP_CONTEXT_STATE_IN_USE && context->useCount != 0)) {
		context->state = MTP_CONTEXT_STATE_IN_USE;
		context->useCount++;
		return 0;
	}
	return state == MTP_CONTEXT_STATE_DONE ? SCE_MTP_ERROR_TERMINATED
					       : SCE_MTP_ERROR_NOT_INITIALIZED;
}

int mtpContextGet(SceUInt32 contextId, int use, MtpContext **context)
{
	MtpContextManager *manager = s_contextManager;
	MtpContext *found = NULL;
	int ret;

	if (manager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	manager->lock();
	ret = mtpTaskQueueGetContext(contextId, &found);
	if (ret < 0) {
		if (manager->m_current.get() != NULL && manager->m_current->id == contextId)
			found = manager->m_current.get();
		else if (manager->m_cancel.get() != NULL && manager->m_cancel->id == contextId)
			found = manager->m_cancel.get();
		else if (manager->m_terminate.get() != NULL &&
			 manager->m_terminate->id == contextId)
			found = manager->m_terminate.get();
		else if (manager->m_pause.get() != NULL && manager->m_pause->id == contextId)
			found = manager->m_pause.get();
		if (found == NULL) {
			s_contextManager->unlock();
			return ret;
		}
	}
	if (use != 0) {
		ret = useContext(found);
		if (ret < 0) {
			s_contextManager->unlock();
			return ret;
		}
	}
	*context = found;
	s_contextManager->unlock();
	return 0;
}

int mtpContextGetContextByEvent(int opcode, MtpContext **context)
{
	MtpContextManager *manager = s_contextManager;
	MtpContext *current;
	int ret;

	if (manager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	manager->lock();
	current = manager->m_current.get();
	if (current == NULL || current->opcode != opcode) {
		s_contextManager->unlock();
		return SCE_MTP_ERROR_NOT_FOUND;
	}
	ret = useContext(current);
	if (ret < 0) {
		s_contextManager->unlock();
		return ret;
	}
	*context = current;
	s_contextManager->unlock();
	return 0;
}

int mtpContextPutContext(MtpContext *context, int restart)
{
	SceRtcTick tick;
	int deleted = 0;

	if (s_contextManager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	s_contextManager->lock();
	if (context->state != MTP_CONTEXT_STATE_IN_USE) {
		MTP_ERR(MTP_TAG_CTXT, 460, "wrong context status: %d, id: %u, count: %u \n",
			context->state, context->id, context->useCount);
		deleted = context->state == MTP_CONTEXT_STATE_DELETED;
	}
	if (--context->useCount == 0)
		context->state = MTP_CONTEXT_STATE_IDLE;
	else
		deleted = 0;
	if (restart != 0 && sceRtcGetCurrentTick(&tick) == 0)
		context->startTick = tick.tick;
	s_contextManager->unlock();
	if (deleted)
		mtpContextDelete(context);
	return 0;
}

int mtpContextSetDone(MtpContext *context)
{
	MtpContextSendRecvData *data;
	int ret = 0;
	int deleted = 0;

	if (s_contextManager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	s_contextManager->lock();
	if (s_contextManager->m_current.get() != context &&
	    s_contextManager->m_cancel.get() != context &&
	    s_contextManager->m_terminate.get() != context &&
	    s_contextManager->m_pause.get() != context) {
		MTP_ERR(MTP_TAG_CTXT, 501, "not found context id: %d\n", context->id);
		s_contextManager->unlock();
		return 0;
	}
	if (context->state != MTP_CONTEXT_STATE_IN_USE) {
		MTP_ERR(MTP_TAG_CTXT, 508, "wrong context status: %d\n", context->state);
		if (context->state == MTP_CONTEXT_STATE_DELETED)
			deleted = 1;
	}
	if (--context->useCount != 0) {
		data = context->sendRecv;
		if (data != NULL && data->eventFlag != -1) {
			ret = sceKernelSetEventFlag(data->eventFlag, MTP_CONTEXT_SR_TERMINATE);
			if (ret < 0)
				MTP_ERR(MTP_TAG_CTXT, 518, "sceKernelSetEventFlag failed: 0x%x\n", ret);
		}
		deleted = 0;
	} else {
		context->state = MTP_CONTEXT_STATE_DONE;
	}
	if (context->eventFlag != -1) {
		ret = sceKernelSetEventFlag(context->eventFlag, MTP_CONTEXT_EVF_DONE);
		if (ret < 0)
			MTP_ERR(MTP_TAG_CTXT, 528, "SetEventFlag failed: 0x%x\n", ret);
	}
	if (context->cancelHandler != NULL) {
		context->cancelHandler(context->cancelHandlerArg);
		context->cancelHandler = NULL;
	}
	s_contextManager->unlock();
	if (deleted != 0)
		mtpContextDelete(context);
	return ret;
}

int setCancel(MtpContext *context)
{
	MtpContextSendRecvData *data;
	int state = context->state;
	int ret;

	if (state != MTP_CONTEXT_STATE_IDLE && state != MTP_CONTEXT_STATE_IN_USE)
		return SCE_MTP_ERROR_DEVICE;
	data = context->sendRecv;
	if (data != NULL && data->eventFlag != -1) {
		ret = sceKernelSetEventFlag(data->eventFlag, MTP_CONTEXT_SR_TERMINATE);
		if (ret < 0)
			MTP_ERR(MTP_TAG_CTXT, 1149, "sceKenrelSetEventFlag failed: 0x%x\n", ret);
	}
	if (context->eventFlag != -1) {
		ret = sceKernelSetEventFlag(context->eventFlag, MTP_CONTEXT_EVF_CANCELED);
		if (ret < 0)
			MTP_ERR(MTP_TAG_CTXT, 1155, "sceKenrelSetEventFlag failed: 0x%x\n", ret);
	}
	if (context->cancelHandler != NULL) {
		context->cancelHandler(context->cancelHandlerArg);
		context->cancelHandler = NULL;
	}
	return 0;
}

int mtpContextCancelById(SceUInt32 contextId)
{
	MtpContext *context = NULL;
	int ret;

	if (s_contextManager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	s_contextManager->lock();
	if (mtpTaskQueueGetContext(contextId, &context) < 0) {
		s_contextManager->unlock();
		return SCE_MTP_ERROR_NOT_FOUND;
	}
	ret = setCancel(context);
	s_contextManager->unlock();
	return ret;
}

int mtpContextCancelAll(void)
{
	MtpContextManager *manager = s_contextManager;
	int ret;

	if (manager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	manager->lock();
	if (manager->m_current.get() != NULL)
		setCancel(manager->m_current.get());
	ret = mtpTaskQueueCancelAll();
	if (manager->m_current.get() == NULL && mtpMarlinIsWaiting())
		ret = mtpMarlinCancelWaitResult();
	s_contextManager->unlock();
	return ret;
}

int mtpContextCreateProgress(MtpContext *context)
{
	int ret = 0;

	if (s_contextManager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	s_contextManager->lock();
	context->progress = (MtpContextProgress *)mtpUtilMalloc(sizeof(MtpContextProgress));
	if (context->progress != NULL)
		mtpUtilMemset(context->progress, 0, sizeof(MtpContextProgress));
	else
		ret = SCE_MTP_ERROR_NO_MEMORY;
	s_contextManager->unlock();
	return ret;
}

int mtpContextSetProgress(MtpContext *context, SceUInt64 done, SceUInt64 total, int newPhase)
{
	MtpContextProgress *progress;

	if (s_contextManager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	s_contextManager->lock();
	progress = context->progress;
	if (progress == NULL) {
		s_contextManager->unlock();
		return SCE_MTP_ERROR_NOT_SUPPORTED;
	}
	progress->done = done;
	progress->total = total;
	if (newPhase != 0)
		progress->count++;
	s_contextManager->unlock();
	return 0;
}

int mtpContextGetProgress(SceUInt32 contextId, MtpContextProgress *progress)
{
	MtpContextManager *manager = s_contextManager;
	MtpContext *current;
	MtpContextProgress *own;

	if (manager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	manager->lock();
	current = manager->m_current.get();
	if (current == NULL || current->id != contextId) {
		s_contextManager->unlock();
		return SCE_MTP_ERROR_NOT_FOUND;
	}
	own = current->progress;
	if (own == NULL) {
		s_contextManager->unlock();
		return SCE_MTP_ERROR_NOT_SUPPORTED;
	}
	progress->done = own->done;
	progress->total = own->total;
	progress->count = own->count;
	s_contextManager->unlock();
	return 0;
}

/* a context nobody used for its timeout is invalidated; notify wakes its waiter */
int mtpContextInvalidateContext(MtpContext *context, int notify)
{
	SceRtcTick tick;
	SceUInt32 timeout;
	int ret = 0;

	s_contextManager->lock();
	if (context->state == MTP_CONTEXT_STATE_IN_USE) {
		ret = SCE_MTP_ERROR_NOT_STOPPED;
	} else {
		timeout = context->timeout;
		tick.tick = 0;
		sceRtcGetCurrentTick(&tick);
		if (tick.tick - context->startTick >= timeout) {
			context->state = MTP_CONTEXT_STATE_INVALIDATED;
			if (notify != 0) {
				ret = sceKernelSetEventFlag(context->eventFlag,
							    MTP_CONTEXT_EVF_INVALIDATED);
				if (ret < 0)
					MTP_ERR(MTP_TAG_CTXT, 691, "SetEventFlag failed: 0x%x\n", ret);
				if (context->cancelHandler != NULL) {
					context->cancelHandler(context->cancelHandlerArg);
					context->cancelHandler = NULL;
				}
			}
		} else {
			ret = SCE_MTP_ERROR_NOT_STOPPED;
		}
	}
	s_contextManager->unlock();
	return ret;
}

int mtpContextWaitBuffer(MtpContextSendRecvData *data, void **buffer, SceSize *size,
			 SceUInt32 *response)
{
	SceUInt32 outBits = 0;
	int ret;

	ret = sceKernelWaitEventFlag(data->eventFlag, MTP_CONTEXT_SR_TERMINATE |
				     MTP_CONTEXT_SR_REQUEST | MTP_CONTEXT_SR_RESPONSE,
				     SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				     &outBits, NULL);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CTXT, 1194, "sceKernelWaitEventFlag failed: 0x%x\n", ret);
		sceKernelSetEventFlag(data->eventFlag, MTP_CONTEXT_SR_TERMINATE);
		return ret;
	}
	if (outBits & MTP_CONTEXT_SR_TERMINATE) {
		MTP_ERR(MTP_TAG_CTXT, 1200, "terminated\n");
		sceKernelSetEventFlag(data->eventFlag, MTP_CONTEXT_SR_TERMINATE |
				      MTP_CONTEXT_SR_RECV | MTP_CONTEXT_SR_SENT);
		return SCE_MTP_ERROR_TERMINATED;
	}
	if (outBits & MTP_CONTEXT_SR_RESPONSE) {
		MTP_ERR(MTP_TAG_CTXT, 1207, "Response: %x\n", data->response);
		*response = data->response;
		return SCE_MTP_ERROR_TERMINATED;
	}
	*buffer = data->buffer;
	*size = data->size;
	return 0;
}

static inline int mtpContextNotifySendRecv(MtpContextSendRecvData *data, SceSize transferred,
					   SceUInt64 remaining, int last, SceUInt32 bits)
{
	int ret;

	data->transferred = transferred;
	data->remaining = remaining;
	if (last != 0 && remaining == 0)
		data->done = 1;
	ret = sceKernelSetEventFlag(data->eventFlag, bits);
	if (ret < 0)
		MTP_ERR(MTP_TAG_CTXT, 1237, "sceKernelSetEventFlag failed: 0x%x\n", ret);
	return ret;
}

static inline void mtpContextNotifySendRecvError(MtpContextSendRecvData *data, int error,
						 SceUInt32 bits)
{
	int ret;

	data->transferred = 0;
	data->remaining = 0;
	data->done = 1;
	data->error = error;
	ret = sceKernelSetEventFlag(data->eventFlag, bits);
	if (ret < 0)
		MTP_ERR(MTP_TAG_CTXT, 1260, "sceKernelSetEventFlag failed: 0x%x\n", ret);
}

static inline void mtpContextWaitResultResponse(MtpContextSendRecvData *data,
						SceUInt32 *response)
{
	SceUInt32 outBits = 0;
	SceUInt timeout = MTP_CONTEXT_RESPONSE_TIMEOUT;
	int ret;

	ret = sceKernelWaitEventFlag(data->eventFlag,
				     MTP_CONTEXT_SR_TERMINATE | MTP_CONTEXT_SR_RESPONSE,
				     SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				     &outBits, &timeout);
	if (ret < 0) {
		if (ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			MTP_ERR(MTP_TAG_CTXT, 1279, "sceKernelWaitEventFlag failed: 0x%x\n", ret);
	} else if (outBits & MTP_CONTEXT_SR_TERMINATE) {
		MTP_ERR(MTP_TAG_CTXT, 1285, "terminated\n");
		sceKernelSetEventFlag(data->eventFlag, MTP_CONTEXT_SR_TERMINATE);
	} else if (data->response != 0) {
		*response = data->response;
	}
}

/* the device thread receives a data phase into the caller's buffers; size 0: the first word */
int mtpContextPassRecvData(MtpContext *context, SceUInt64 size, SceUInt32 *response,
			   int last)
{
	MtpContextSendRecvData *data = context->sendRecv;
	void *buffer = NULL;
	SceSize bufferSize = 0;
	SceUInt32 length = 0;
	SceUInt64 remaining = size;
	SceUInt64 total;
	SceSize n;
	int progress;
	int ret;

	if (data == NULL) {
		MTP_ERR(MTP_TAG_CTXT, 718, "RecvData is not set\n");
		return -2;
	}
	progress = 0;
	total = size;
	if (context->progress != NULL && size != 0) {
		progress = 1;
		mtpContextSetProgress(context, 0, total, 1);
	}
	if (total == 0)
		remaining = (SceUInt64)-1;
	ret = 0;
	while (remaining != 0) {
		if (mtpContextWaitBuffer(data, &buffer, &bufferSize, response) < 0) {
			if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
				mtpDevIfReset();
			return -2;
		}
		if (bufferSize > MTP_CONTEXT_PASS_MAX)
			bufferSize = MTP_CONTEXT_PASS_MAX;
		if (remaining == (SceUInt64)-1) {
			ret = mtpCoreRecvData(&length, sizeof(length), NULL);
			if (ret < 0) {
				MTP_ERR(MTP_TAG_CTXT, 749, "sceMtpIfRecvData failed: 0x%x\n", ret);
				return -2;
			}
			remaining = length;
			if (context->progress != NULL) {
				mtpContextSetProgress(context, 0, remaining, 1);
				progress = 1;
			}
		}
		n = remaining > bufferSize ? bufferSize : (SceSize)remaining;
		s_dataMutex->Lock();
		if (context->sendRecv->buffer == NULL) {
			s_dataMutex->Unlock();
			MTP_ERR(MTP_TAG_CTXT, 768, "RecvData canceled.\n");
			mtpContextNotifySendRecvError(data, SCE_MTP_ERROR_CANCELED,
						      MTP_CONTEXT_SR_RECV);
			*response = MTP_RESPONSE_VENDOR_CANCELED;
			return -2;
		}
		ret = mtpCoreRecvData(data->buffer, n, NULL);
		if (ret < 0) {
			s_dataMutex->Unlock();
			MTP_ERR(MTP_TAG_CTXT, 776, "MtpIfRecvData failed: 0x%x\n", ret);
			mtpContextNotifySendRecvError(data, ret, MTP_CONTEXT_SR_RECV);
			return ret;
		}
		s_dataMutex->Unlock();
		if (ret == 0 && n != 0) {
			MTP_ERR(MTP_TAG_CTXT, 784, "recv 0 byte\n");
			mtpContextNotifySendRecvError(data, SCE_MTP_ERROR_DATA_PHASE_OVER,
						      MTP_CONTEXT_SR_RECV);
			*response = MTP_RESPONSE_INCOMPLETE_TRANSFER;
			return -2;
		}
		remaining -= ret;
		ret = mtpContextNotifySendRecv(data, ret, remaining, last, MTP_CONTEXT_SR_RECV);
		if (ret < 0)
			return -2;
		if (progress != 0)
			mtpContextSetProgress(context, total - remaining, total, 0);
	}
	if (last != 0)
		mtpContextWaitResultResponse(data, response);
	return ret;
}

/* the device thread sends a data phase from the caller's buffers */
int mtpContextPassSendData(MtpContextSendRecvData *data, SceUInt64 size, SceUInt64 totalSize,
			   SceUInt32 *response, int last)
{
	void *buffer = NULL;
	SceSize bufferSize = 0;
	int total;
	SceSize n;
	int ret;

	total = totalSize < 0xFFFFFFFF ? (int)totalSize : -1;
	ret = 0;
	while (size != 0) {
		if (mtpContextWaitBuffer(data, &buffer, &bufferSize, response) < 0)
			return -2;
		n = size > bufferSize ? bufferSize : (SceSize)size;
		ret = mtpCoreDriverSendData(buffer, n, total);
		if (ret < 0) {
			MTP_ERR(MTP_TAG_CTXT, 839, "mtpDriverSendData failed: 0x%x\n", ret);
			mtpContextNotifySendRecvError(data, ret, MTP_CONTEXT_SR_SENT);
			break;
		}
		size -= n;
		ret = mtpContextNotifySendRecv(data, n, size, last, MTP_CONTEXT_SR_SENT);
		if (ret < 0)
			return -2;
	}
	return ret;
}

/* the host went away: the error for the kind of host it was */
static inline int hostLostError(void)
{
	switch (mtpStatusGetPlatformClass()) {
	case MTP_PLATFORM_CLASS_PC:
		return SCE_MTP_ERROR_PC_TIMEOUT;
	case MTP_PLATFORM_CLASS_PS:
		return SCE_MTP_ERROR_PS_TIMEOUT;
	default:
		return SCE_MTP_ERROR_HOST_TIMEOUT;
	}
}

static inline void resetOnHostLost(int error)
{
	if (mtpDevGetIfType() == MTP_IF_TYPE_NET &&
	    (error == SCE_MTP_ERROR_PS_TIMEOUT || error == SCE_MTP_ERROR_PC_TIMEOUT ||
	     error == SCE_MTP_ERROR_HOST_TIMEOUT))
		mtpDevIfReset();
}

int mtpContextWaitResult(MtpContext *context, void *result)
{
	SceUInt32 outBits = 0;
	SceUInt timeout;
	MtpStatus status;
	int ret;

	for (;;) {
		timeout = MTP_CONTEXT_WAIT_TIMEOUT;
		ret = sceKernelWaitEventFlag(context->eventFlag, MTP_CONTEXT_EVF_DONE |
					     MTP_CONTEXT_EVF_CANCELED | MTP_CONTEXT_EVF_INVALIDATED,
					     SCE_KERNEL_EVF_WAITMODE_OR |
					     SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &outBits, &timeout);
		if (ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			break;
		if (mtpContextInvalidateContext(context, 0) == 0)
			break;
	}
	if (ret == 0 && (outBits & MTP_CONTEXT_EVF_CANCELED)) {
		mtpStatusGet(&status);
		if (status.reason == 8 || status.reason == 9 || (ret = mtpStatusCheck(0)) >= 0)
			ret = SCE_MTP_ERROR_CANCELED;
	} else if ((ret == 0 && (outBits & MTP_CONTEXT_EVF_INVALIDATED)) ||
		   ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT) {
		if (mtpDevGetIfType() == MTP_IF_TYPE_USB)
			mtpDevIfReset();
		ret = hostLostError();
	}
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CTXT, 861, "mtpContextWait %u failed: 0x%x\n", context->id, ret);
		resetOnHostLost(ret);
		return ret;
	}
	if (context->onResult != NULL)
		context->onResult(context, result);
	if (context->result != MTP_RESPONSE_OK)
		ret = mtpUtilResponseCodeToError(context->result);
	return ret;
}

/* the caller hands its buffer to the device thread and waits for the pass over it */
int mtpContextWaitSendRecv(MtpContext *context, MtpContextSendRecvData *data, int send)
{
	SceUInt32 outBits;
	SceUInt timeout;
	SceUInt32 pattern;
	SceRtcTick tick;
	SceUInt32 elapsed;
	int busy;
	int ret;

	ret = sceKernelSetEventFlag(data->eventFlag, MTP_CONTEXT_SR_REQUEST);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CTXT, 1383, "sceKernelSetEventFlag failed: 0x%x\n", ret);
		return ret;
	}
	outBits = 0;
	timeout = MTP_CONTEXT_SR_TIMEOUT;
	if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
		timeout = MTP_CONTEXT_SR_TIMEOUT_NET;
	pattern = MTP_CONTEXT_SR_TERMINATE | MTP_CONTEXT_SR_RECV;
	if (send != 0)
		pattern = MTP_CONTEXT_SR_TERMINATE | MTP_CONTEXT_SR_SENT;
	for (;;) {
		ret = sceKernelWaitEventFlag(data->eventFlag, pattern, SCE_KERNEL_EVF_WAITMODE_OR |
					     SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &outBits, &timeout);
		if (ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			break;
		/* the wait goes on while the request is young or another caller uses it */
		tick.tick = 0;
		s_contextManager->lock();
		sceRtcGetCurrentTick(&tick);
		elapsed = tick.tick - context->startTick;
		if (elapsed >= MTP_CONTEXT_WAIT_TIMEOUT) {
			busy = 0;
			if (context->useCount > 1)
				busy = SCE_MTP_ERROR_NOT_STOPPED;
		} else {
			busy = SCE_MTP_ERROR_NOT_STOPPED;
		}
		s_contextManager->unlock();
		if (busy == 0)
			break;
		timeout = MTP_CONTEXT_SR_TIMEOUT;
		if (mtpDevGetIfType() == MTP_IF_TYPE_NET)
			timeout = MTP_CONTEXT_SR_TIMEOUT_NET;
	}
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CTXT, 1417, "sceKernelWaitEventFlag failed: 0x%x, pattern=0x%x\n",
			ret, pattern);
		if (ret == SCE_KERNEL_ERROR_WAIT_DELETE)
			return ret;
		sceKernelSetEventFlag(data->eventFlag, MTP_CONTEXT_SR_TERMINATE);
		if (sceKernelPollEventFlag(data->eventFlag, MTP_CONTEXT_SR_REQUEST,
					   SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL) ==
		    SCE_KERNEL_ERROR_EVF_COND)
			sceKernelWaitEventFlag(data->eventFlag, pattern ^ MTP_CONTEXT_SR_TERMINATE,
					       SCE_KERNEL_EVF_WAITMODE_OR |
					       SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
		if (ret != SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			return ret;
		return hostLostError();
	}
	if (outBits & MTP_CONTEXT_SR_TERMINATE) {
		s_dataMutex->Lock();
		MTP_ERR(MTP_TAG_CTXT, 1445, "terminated id: %u, count: %u \n", context->id,
			context->useCount);
		sceKernelSetEventFlag(data->eventFlag, MTP_CONTEXT_SR_TERMINATE);
		context->sendRecv->buffer = NULL;
		s_dataMutex->Unlock();
		return SCE_MTP_ERROR_TERMINATED;
	}
	return 0;
}

int mtpContextRecvData(MtpContext *context, void *buffer, SceSize size, SceSize *received,
		       SceUInt64 *remaining)
{
	MtpContextSendRecvData *data = context->sendRecv;
	int ret;

	if (data == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	if (context->opcode == MTP_OPERATION_VENDOR_C123 && (size & 3) != 0) {
		MTP_ERR(MTP_TAG_CTXT, 890, "Wrong buffer size: %u\n", size);
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	}
	if (data->done != 0)
		return SCE_MTP_ERROR_DATA_PHASE_OVER;
	data->buffer = buffer;
	data->size = size;
	data->transferred = 0;
	data->remaining = 0;
	ret = mtpContextWaitSendRecv(context, data, 0);
	if (ret < 0) {
		resetOnHostLost(ret);
		return ret;
	}
	*received = data->transferred;
	ret = data->error;
	*remaining = data->remaining;
	if (ret != 0)
		return ret;
	return 0;
}

int mtpContextSendData(MtpContext *context, const void *buffer, SceSize size, SceSize *sent,
		       SceUInt64 *remaining)
{
	MtpContextSendRecvData *data = context->sendRecv;
	int ret;

	if (data == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	if (data->done != 0)
		return SCE_MTP_ERROR_ALREADY;
	data->buffer = (void *)buffer;
	data->size = size;
	data->transferred = 0;
	data->remaining = 0;
	ret = mtpContextWaitSendRecv(context, data, 1);
	if (ret < 0) {
		resetOnHostLost(ret);
		return ret;
	}
	*sent = data->transferred;
	ret = data->error;
	*remaining = data->remaining;
	if (ret != 0)
		return ret;
	return 0;
}

int mtpContextNotifyResultResponse(MtpContext *context, SceUInt32 response)
{
	MtpContextSendRecvData *data = context->sendRecv;
	int ret;

	if (data == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	data->response = response;
	ret = sceKernelSetEventFlag(data->eventFlag, MTP_CONTEXT_SR_RESPONSE);
	if (ret < 0)
		MTP_ERR(MTP_TAG_CTXT, 1304, "sceKernelSetEventFlag failed: 0x%x\n", ret);
	return ret;
}

int mtpContextSetCancelHandler(MtpContext *context, void (*handler)(void *arg), void *arg)
{
	if (s_contextManager == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	s_contextManager->lock();
	if (context->cancelHandler != NULL) {
		s_contextManager->unlock();
		return SCE_MTP_ERROR_ALREADY;
	}
	context->cancelHandler = handler;
	context->cancelHandlerArg = arg;
	s_contextManager->unlock();
	return 0;
}

/* a cancel request: tell the host, then cancel the queued requests */
int mtpContextCancel(MtpContext *context, SceUInt32 contextId)
{
	MtpEvent event;
	int ret;

	mtpUtilMemset(&event, 0, sizeof(event));
	event.code = MTP_EVENT_VENDOR_CANCEL;
	event.param[0] = context->id;
	event.param[1] = contextId;
	event.numParams = 2;
	context->data = NULL;
	context->dataSize = 0;
	ret = mtpCoreSendEvent(&event);
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CTXT, 1008, "mtpCoreSendEvent failed: 0x%x\n", ret);
		return ret;
	}
	ret = mtpTaskQueueCancelAllForDoCancelAndPause();
	if (ret < 0) {
		MTP_ERR(MTP_TAG_CTXT, 1014,
			"mtpTaskQueueCancelAllForDoCancelAndPause failed: 0x%x\n", ret);
		return ret;
	}
	return 0;
}

int mtpContextSetPauseContextData(SceUInt32 contextId)
{
	MtpContext *context = NULL;
	int ret;

	ret = mtpContextGet(contextId, 0, &context);
	if (ret < 0)
		MTP_ERR(MTP_TAG_CTXT, 1460, "GetContext %u failed: 0x%x\n", contextId, ret);
	else
		s_pauseContextId = contextId;
	return ret;
}

SceUInt32 mtpContextGetPauseContextId(void)
{
	return s_pauseContextId;
}

int mtpContextIsDeleted(MtpContext *context)
{
	return context == NULL || context->state == MTP_CONTEXT_STATE_DELETED;
}

int mtpContextPauseSource(void *data)
{
	mtpExportPauseSource(data);
	return 0;
}

int mtpContextPauseTarget(void *data)
{
	mtpExportPauseTarget(data);
	return 0;
}

void mtpContextSetCanceling(int canceling)
{
	s_contextManager->setCanceling(canceling);
}

int mtpContextIsCanceling(void)
{
	return s_contextManager->isCanceling();
}

MtpContext::~MtpContext()
{
	if (freeData != NULL)
		freeData(data, dataSize);
	else
		mtpUtilFree(data);
	data = NULL;
	deleteContextSendRecvData(this);
	if (eventFlag != -1) {
		sceKernelDeleteEventFlag(eventFlag);
		eventFlag = -1;
	}
	if (progress != NULL) {
		mtpUtilFree(progress);
		progress = NULL;
	}
}
