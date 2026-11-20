/*
 * exclusive_func_mgr.cpp: the exclusive function manager (0x810E1944-0x810E206C). It keeps the
 * shell's 4 MiB block ("SceShellExclusiveFuncMemBlock") and runs one function in it at a time, on
 * the main thread: the function requested with the highest priority starts, one requested above
 * the running one stops it first, and a function stopped that may wait goes back into the queue.
 * The file's name and the names not quoted are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/list>
#include <std/string.h>
#include <common/shared_ptr.h>
#include <common/main_thread_call_list.h>
#include <thread/thread.h>
#include <graphics/graphmem/graphmem.h>
#include <plugin.h>
#include "shell.h"
#include "plugins.h"
#include "exclusive_func.h"

using namespace paf;

#define SCE_PAF_ERROR_INVALID_ARGUMENT	-2136014705	/* 0x80AF008F */

/* the shell's block the functions run in */
#define EXCLUSIVE_FUNC_MEM_BLOCK_SIZE	0x400000

/* power_manage_plugin's callback on the power events (PLACEHOLDER) */
typedef void (*ExclusiveFuncPowerCallback)(int event, int arg1, int arg2, void *arg);

/* Sony's build keeps the queue's clear and pop_back out of line, pop_back with the count checked */
template<>
__attribute__((noinline)) void list<ExclusiveFuncPtr>::clear()
{
	if (head != NULL) {
		_Node *node = head->next;

		while (node != head) {
			_Node *next = node->next;

			destroy(&node->value);
			operator delete(node);
			node = next;
		}
		head->prev = head;
		head->next = head;
		count = 0;
	}
}

template<>
__attribute__((noinline)) void list<ExclusiveFuncPtr>::pop_back()
{
	if (count != 0 && head != NULL) {
		_Node *node = head->prev;

		node->next->prev = node->prev;
		node->prev->next = node->next;
		destroy(&node->value);
		operator delete(node);
		--count;
	}
}

static ExclusiveFuncMgr *s_exclusiveFuncMgr = NULL;

/* (0x810E2046) */
static void exclusiveFuncMgrOnPower(int event, int arg1, int arg2, void *arg)
{
	ExclusiveFuncMgr *mgr = (ExclusiveFuncMgr *)arg;

	mgr->m_mutex.Lock();
	mgr->m_events |= EXCLUSIVE_FUNC_EVENT_POWER;
	mgr->m_mutex.Unlock();
}

/* (0x810E2064) */
static void exclusiveFuncMgrOnMainThread(void *arg)
{
	((ExclusiveFuncMgr *)arg)->Update();
}

/* (0x810E1944) */
ExclusiveFuncMgr::ExclusiveFuncMgr()
	: m_powerCallbackAdded(false), m_events(0), m_mutex("SceShellExclusiveFuncMgr", 0),
	  m_reserved(-1), m_memBlock(NULL)
{
	sce_paf_memset(m_priority, 0, sizeof(m_priority));
	m_memBlock = GraphMem::AllocOSMemory(GraphMem::DeviceType_SystemMemory,
					     EXCLUSIVE_FUNC_MEM_BLOCK_SIZE,
					     "SceShellExclusiveFuncMemBlock", NULL);
	common::MainThreadCallList::Register(exclusiveFuncMgrOnMainThread, this);
}

/* (0x810E19C8) */
int shellExclusiveFuncMgrCreate(void)
{
	int ret = SHELL_ERROR_ALREADY_CREATED;

	if (s_exclusiveFuncMgr == NULL) {
		ret = SHELL_ERROR_NO_MEMORY;
		s_exclusiveFuncMgr = new ExclusiveFuncMgr();
		if (s_exclusiveFuncMgr != NULL)
			ret = 0;
	}
	return ret;
}

/* (0x810E1A0A) */
__attribute__((noinline)) ExclusiveFuncMgr *ExclusiveFuncMgr::GetInstance()
{
	return s_exclusiveFuncMgr;
}

/* (0x810E1A16) */
int ExclusiveFuncMgr::Update()
{
	int ret = 0;

	if (!m_powerCallbackAdded) {
		Plugin *power = Plugin::Find("power_manage_plugin");

		if (power != NULL) {
			ShellPluginInterface *iface = (ShellPluginInterface *)power->GetInterface(1);

			if (iface != NULL)
				((void (*)(ExclusiveFuncPowerCallback, void *))
				 iface->func[POWER_MANAGE_PLUGIN_FUNC_ADD_EXCLUSIVE_CALLBACK])(
					exclusiveFuncMgrOnPower, this);
			m_powerCallbackAdded = true;
		}
	}

	m_mutex.Lock();
	if (m_current.get() != NULL) {
		if (m_current->GetState() == EXCLUSIVE_FUNC_STATE_CANCELED) {
			ret = m_current->Stop();
			if (ret == 0)
				m_current->SetState(EXCLUSIVE_FUNC_STATE_STOPPING);
		}
		if ((m_current->GetState() & EXCLUSIVE_FUNC_STATE_STOPPED) != 0) {
			m_current->ClearState(EXCLUSIVE_FUNC_STATE_STOPPED |
					      EXCLUSIVE_FUNC_STATE_STOPPING |
					      EXCLUSIVE_FUNC_STATE_CANCELED);
			m_current.reset();
		}
	}
	if (m_current.get() == NULL && m_queue.size() != 0) {
		m_current = m_queue.back();
		m_current->SetState(EXCLUSIVE_FUNC_STATE_REQUESTED);
		m_queue.pop_back();
	}
	if (m_current.get() != NULL) {
		if (m_current->GetState() == EXCLUSIVE_FUNC_STATE_REQUESTED) {
			ExclusiveFuncMemBlock mem;

			mem.base = m_memBlock;
			mem.size = EXCLUSIVE_FUNC_MEM_BLOCK_SIZE;
			ret = m_current->Start(&mem);
			if (ret == 0)
				m_current->SetState(EXCLUSIVE_FUNC_STATE_STARTING);
		}
		if ((m_current->GetState() & EXCLUSIVE_FUNC_STATE_STARTED) != 0)
			m_current->ClearState(EXCLUSIVE_FUNC_STATE_STARTED |
					      EXCLUSIVE_FUNC_STATE_STARTING |
					      EXCLUSIVE_FUNC_STATE_REQUESTED);
		if (m_events != 0 && m_current->GetState() == 0) {
			ret = m_current->Notify(m_events);
			if (ret == 0)
				m_events = 0;
		}
	}
	m_mutex.Unlock();
	return ret;
}

/* (0x810E1C58) */
__attribute__((noinline)) int ExclusiveFuncMgr::Remove(ExclusiveFuncPtr *func)
{
	for (list<ExclusiveFuncPtr>::iterator it = m_queue.begin(); it != m_queue.end(); ++it) {
		if (it->get() == func->get()) {
			m_queue.erase(it);
			break;
		}
	}
	return 0;
}

/* (0x810E1CAC) */
int ExclusiveFuncMgr::Cancel(ExclusiveFuncPtr *func)
{
	if (func->get() == NULL)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;

	m_mutex.Lock();
	if (func->get() == m_current.get())
		m_current->SetState(EXCLUSIVE_FUNC_STATE_CANCELED);
	Remove(func);
	m_mutex.Unlock();
	return 0;
}

/* (0x810E1CE8, 0x810E1E00) */
ExclusiveFuncMgr::~ExclusiveFuncMgr()
{
	common::MainThreadCallList::Unregister(exclusiveFuncMgrOnMainThread, this);
	Cancel(&m_current);
	for (list<ExclusiveFuncPtr>::iterator it = m_queue.begin(); it != m_queue.end(); ++it)
		Cancel(&*it);
	while (m_current.get() != NULL) {
		Update();
		thread::Sleep(100);
	}
	if (m_memBlock != NULL)
		GraphMem::FreeOSMemory(GraphMem::DeviceType_SystemMemory, m_memBlock);
}

/* (0x810E1E12) */
int ExclusiveFuncMgr::Insert(ExclusiveFuncPtr *func)
{
	list<ExclusiveFuncPtr>::iterator it = m_queue.begin();

	while (it != m_queue.end()) {
		if (it->get() == func->get())
			break;
		if (GetPriority(it->get()) < GetPriority(func->get())) {
			m_queue.insert(it, *func);
			break;
		}
		if ((*it)->IsQueueable())
			++it;
		else
			it = m_queue.erase(it);
	}
	if (it == m_queue.end())
		m_queue.push_back(*func);
	return 0;
}

/* (0x810E1EDA) */
int ExclusiveFuncMgr::Request(ExclusiveFuncPtr *func)
{
	if (func->get() == NULL)
		return SCE_PAF_ERROR_INVALID_ARGUMENT;

	m_mutex.Lock();
	if (func->get() != m_current.get() ||
	    (m_current->GetState() & EXCLUSIVE_FUNC_STATE_STOPPING) != 0 ||
	    (m_queue.size() != 0 &&
	     GetPriority(m_queue.back().get()) < GetPriority(func->get()))) {
		bool preempt = false;

		if (m_current.get() != NULL &&
		    (m_current->GetState() & EXCLUSIVE_FUNC_STATE_CANCELED) == 0)
			preempt = GetPriority(func->get()) <= GetPriority(m_current.get());
		if (m_current.get() == NULL || preempt) {
			m_events = 0;
			if (preempt) {
				m_current->SetState(EXCLUSIVE_FUNC_STATE_CANCELED);
				if (m_current->IsQueueable())
					Insert(&m_current);
			}
		}
		Insert(func);
	} else {
		m_current->ClearState(EXCLUSIVE_FUNC_STATE_CANCELED);
		Remove(func);
	}
	m_mutex.Unlock();
	return 0;
}

/* (0x810E1FBA) */
int ExclusiveFuncMgr::SetPriority(int type, int priority)
{
	if (priority != m_priority[type]) {
		m_mutex.Lock();

		list<ExclusiveFuncPtr> queue(m_queue);

		m_queue.clear();
		m_priority[type] = priority;
		for (list<ExclusiveFuncPtr>::iterator it = queue.begin(); it != queue.end(); ++it)
			Request(&*it);
		m_mutex.Unlock();
	}
	return 0;
}

