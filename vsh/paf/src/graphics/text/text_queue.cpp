/*
 * paf::graph::TextQueue (the file's name ours): the layouts waiting for the text threads, under
 * ScePafTextQueueMutex, with ScePafTextQueueCond to wake a thread when one is queued.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <graphics/text/text_layout.h>
#include <std/list>
#include <thread/thread.h>
#include "../../paf_types.h"

namespace paf {
namespace graph {

TextQueue::TextQueue()
	: m_mutex("ScePafTextQueueMutex", 0), m_cond(m_mutex, "ScePafTextQueueCond", 0)
{
}

TextQueue::~TextQueue()
{
}

int32_t TextQueue::Request(common::SharedPtr<TextLayout> &layout)
{
	int32_t state;

	layout->GetState(state);
	if ((state & TextLayoutState_Invalid) != 0 &&
	    (state & (TextLayoutState_Updating | TextLayoutState_Waiting)) == 0) {
		m_mutex.Lock();
		layout->SetState(TextLayoutState_Queuing | TextLayoutState_Waiting);
		m_queue.push_back(layout);
		m_cond.Notify();
		m_mutex.Unlock();
	}
	return SCE_PAF_OK;
}

int32_t TextQueue::Cancel(common::SharedPtr<TextLayout> &layout)
{
	int32_t state;

	m_mutex.Lock();
	layout->GetState(state);
	if ((state & TextLayoutState_Queuing) != 0) {
		for (list<common::SharedPtr<TextLayout> >::iterator it = m_queue.begin();
		     it != m_queue.end(); ++it) {
			if (it->get() == layout.get()) {
				m_queue.erase(it);
				layout->ClearState(TextLayoutState_Queuing | TextLayoutState_Waiting);
				break;
			}
		}
	}
	m_mutex.Unlock();
	return SCE_PAF_OK;
}

int32_t TextQueue::Pop(common::SharedPtr<TextLayout> &layout)
{
	int32_t ret = SCE_PAF_ERROR_TEXT_ABORTED;

	m_mutex.Lock();
	if (m_queue.size() == 0)
		m_cond.Wait(0);
	if (m_queue.size() != 0) {
		layout = m_queue.front();
		m_queue.pop_front();
		if (layout.get() != NULL) {
			layout->ClearState(TextLayoutState_Queuing);
			ret = SCE_PAF_OK;
		}
	}
	m_mutex.Unlock();
	return ret;
}

int32_t TextQueue::Abort()
{
	m_mutex.Lock();
	m_queue.push_front(common::SharedPtr<TextLayout>());
	m_cond.NotifyAll();
	m_mutex.Unlock();
	return SCE_PAF_OK;
}

}
}
