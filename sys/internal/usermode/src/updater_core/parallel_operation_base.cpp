/*
 * The two-thread operations' base (Sony's parallel_operation_base.cpp): the elements, the two
 * channels between the threads, and the consumer thread, which runs each element the derived
 * class's mainOperation passes on until the end marker comes, and aborts both channels if an
 * element fails.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <updater_core/updater_core.h>
#include "updater_log.h"
#include "parallel_operation_base.h"

#define UPDATER_FILE_ID		0xeeedaf2a

/* what mainOperation's end sends the consumer */
static void *s_endMarker = NULL;

ParallelOperationBase::ParallelOperationBase()
	: m_fullChannel(PARALLEL_OPERATION_ELEMENT_NUM), m_freeChannel(PARALLEL_OPERATION_ELEMENT_NUM)
{
	m_consumerResult = 0;
	sceClibMemset(m_elements, 0, sizeof(m_elements));
}

ParallelOperationBase::~ParallelOperationBase()
{
	SceUInt32 i;

	for (i = 0; i < PARALLEL_OPERATION_ELEMENT_NUM; i++) {
		if (m_elements[i] != NULL) {
			delete m_elements[i];
			m_elements[i] = NULL;
		}
	}
}

/* makes the elements and puts them in the free channel */
int ParallelOperationBase::SetUp(void)
{
	ParallelOperationElement *element = NULL;
	SceUInt32 i;
	int ret;

	for (i = 0; i < PARALLEL_OPERATION_ELEMENT_NUM; i++) {
		ret = CreateElement(&element);
		if (ret < 0)
			return ret;
		m_elements[i] = element;
	}
	ret = m_freeChannel.Initialize((void **)m_elements, PARALLEL_OPERATION_ELEMENT_NUM);
	if (ret < 0)
		return ret;
	ret = m_fullChannel.Initialize(NULL, 0);
	if (ret < 0)
		return ret;
	return 0;
}

/* runs mainOperation with the consumer thread beside it */
int ParallelOperationBase::Execute(void *arg)
{
	SceUID threadId = 0;
	SceInt32 exitStatus = 0;
	ParallelOperationBase *self;
	int ret;

	m_consumerResult = 0;
	self = this;
	ret = StartThread(threadFunc, &self, &threadId);
	if (ret < 0)
		return ret;
	ret = mainOperation(arg);
	if (ret < 0) {
		m_fullChannel.Push(s_endMarker, true);
		if (ret == SCE_UPDATER_ERROR_ABORTED && m_consumerResult != 0) {
			UPDATER_ERROR_FMT(122, "%x", m_consumerResult);
			return m_consumerResult;
		}
		return ret;
	}
	m_fullChannel.Push(s_endMarker, true);
	JoinThread(threadId, &exitStatus);
	if (exitStatus < 0) {
		UPDATER_ERROR_FMT(131, "%x", exitStatus);
		return exitStatus;
	}
	return 0;
}

int ParallelOperationBase::consumerOperation(void)
{
	ParallelOperationElement *element = NULL;
	int ret;

	while (!m_fullChannel.IsAborted() && !m_freeChannel.IsAborted()) {
		ret = m_fullChannel.Pop((void **)&element, true);
		if (ret < 0) {
			UPDATER_ERROR_FMT(81, "%x", ret);
			return ret;
		}
		if (element == s_endMarker)
			break;
		ret = element->Operate();
		if (ret < 0) {
			UPDATER_ERROR_FMT(91, "%x", ret);
			m_consumerResult = ret;
			return ret;
		}
		/* Sony's tests the element's result again, not the push's */
		m_freeChannel.Push(element, true);
		if (ret < 0) {
			UPDATER_ERROR_FMT(98, "%x", ret);
			return ret;
		}
	}
	return 0;
}

SceInt32 ParallelOperationBase::threadFunc(SceSize args, void *argp)
{
	ParallelOperationBase *self = *(ParallelOperationBase **)argp;
	int ret;

	ret = self->consumerOperation();
	if (ret != 0) {
		UPDATER_ERROR_FMT(67, "%x", ret);
		self->m_fullChannel.Abort();
		self->m_freeChannel.Abort();
	}
	sceKernelExitThread(ret);
	return ret;
}
