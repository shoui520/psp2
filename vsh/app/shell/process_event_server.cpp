/*
 * process_event_server.cpp: SceShell's process/application event server (0x81241220-0x812416D4).
 *
 * Listeners are registered per event code via SceShell_83D16CA8, removed via SceShell_E3D3A162,
 * and notified via shellAppEventSend. A listener removed while its event is being dispatched is
 * marked and deleted once dispatch completes. Synchronized by "SceShellProcessEventServerMutex".
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/list>
#include <std/map>
#include <thread/thread.h>
#include "shell.h"

using namespace paf;

class ShellProcessEventListener {
public:
	ShellProcessEventListener(ShellEventCallback callback, void *arg)
		: callback(callback), arg(arg), removed(false)
	{
	}

	virtual ~ShellProcessEventListener()
	{
	}

	virtual int Notify(int code, const void *data, SceSize size)
	{
		callback(code, data, size, arg);
		return 0;
	}

	ShellEventCallback callback;
	void *arg;
	bool removed;
};

class ShellProcessEventListenerList {
public:
	typedef list<ShellProcessEventListener *> Listeners;

	ShellProcessEventListenerList() : sending(false)
	{
	}

	virtual ~ShellProcessEventListenerList()
	{
		while (listeners.size() != 0) {
			Listeners::iterator first = listeners.begin();

			if (*first != NULL)
				delete *first;
			listeners.erase(first);
		}
	}

	ShellProcessEventListener *Find(ShellProcessEventListener *listener)
	{
		for (Listeners::iterator it = listeners.begin(); it != listeners.end(); ++it) {
			if (*it == listener)
				return *it;
		}
		return NULL;
	}

	ShellProcessEventListener *Find(ShellEventCallback callback, void *arg)
	{
		for (Listeners::iterator it = listeners.begin(); it != listeners.end(); ++it) {
			ShellProcessEventListener *listener = *it;

			if (listener->callback == callback && listener->arg == arg)
				return listener;
		}
		return NULL;
	}

	void Add(ShellProcessEventListener *listener)
	{
		if (Find(listener) == NULL)
			listeners.push_back(listener);
	}

	Listeners listeners;
	bool sending;
};

class ShellProcessEventServerBase {
public:
	typedef map<int, ShellProcessEventListenerList *> ListenerMap;

	ShellProcessEventServerBase()
	{
	}

	virtual ~ShellProcessEventServerBase()
	{
		while (lists.size() != 0) {
			ListenerMap::iterator it = lists.begin();

			if (it->second != NULL)
				delete it->second;
			lists.erase(it);
		}
	}

	ListenerMap lists;
};

class ShellProcessEventServer : public ShellProcessEventServerBase {
public:
	ShellProcessEventServer();
	virtual ~ShellProcessEventServer();

	thread::RMutex mutex;
};

static int s_processEventInitCount;
static ShellProcessEventServer *s_processEventServer;
static ShellProcessEventServer *s_processEventServerInstance;

/* (0x8124126C) */
ShellProcessEventServer::ShellProcessEventServer()
	: mutex("SceShellProcessEventServerMutex", 0)
{
	s_processEventServerInstance = this;
}

/* (0x812412D0, 0x81241366) */
ShellProcessEventServer::~ShellProcessEventServer()
{
	s_processEventServerInstance = NULL;
}

/* (0x81241376) */
static __attribute__((noinline)) int
shellProcessEventServerAdd(int event, ShellEventCallback callback, void *arg)
{
	thread::RMutexLock lock(&s_processEventServerInstance->mutex);

	ShellProcessEventServer::ListenerMap &lists = s_processEventServerInstance->lists;
	ShellProcessEventListener *listener = new ShellProcessEventListener(callback, arg);
	ShellProcessEventServer::ListenerMap::iterator it = lists.find(event);

	if (it != lists.end()) {
		if (it->second != NULL)
			it->second->Add(listener);
	} else {
		ShellProcessEventListenerList *list = new ShellProcessEventListenerList();

		list->Add(listener);
		lists.insert(ShellProcessEventServer::ListenerMap::value_type(event, list));
	}
	return 0;
}

/* (0x812414CC) */
static __attribute__((noinline)) int
shellProcessEventServerRemove(int event, ShellEventCallback callback, void *arg)
{
	int ret = -1;

	thread::RMutexLock lock(&s_processEventServerInstance->mutex);

	ShellProcessEventServer::ListenerMap &lists = s_processEventServerInstance->lists;
	ShellProcessEventServer::ListenerMap::iterator it = lists.find(event);

	if (it != lists.end() && it->second != NULL) {
		ShellProcessEventListenerList *list = it->second;
		ShellProcessEventListener *listener = list->Find(callback, arg);
		ShellProcessEventListenerList::Listeners::iterator where;

		for (where = list->listeners.begin(); where != list->listeners.end(); ++where) {
			if (*where != listener)
				continue;
			if (list->sending) {
				listener->removed = true;
			} else {
				list->listeners.erase(where);
				if (listener != NULL)
					delete listener;
			}
			ret = 0;
			break;
		}
	}
	return ret;
}

/* (0x812415D6) */
static __attribute__((noinline)) int
shellProcessEventServerSend(int event, const void *data, int flags)
{
	thread::RMutexLock lock(&s_processEventServerInstance->mutex);

	ShellProcessEventServer::ListenerMap &lists = s_processEventServerInstance->lists;
	ShellProcessEventServer::ListenerMap::iterator it = lists.find(event);

	if (it != lists.end()) {
		int key = it->first;
		ShellProcessEventListenerList *list = it->second;
		ShellProcessEventListenerList::Listeners::iterator where;

		list->sending = true;
		for (where = list->listeners.begin(); where != list->listeners.end(); ++where) {
			if (*where != NULL)
				(*where)->Notify(key, data, (SceSize)flags);
		}
		list->sending = false;
		for (where = list->listeners.begin(); where != list->listeners.end();) {
			ShellProcessEventListener *listener = *where;

			if (listener != NULL) {
				if (!listener->removed) {
					++where;
					continue;
				}
				delete listener;
			}
			where = list->listeners.erase(where);
		}
	}
	return 0;
}

/* (0x81241220) */
int SceShell_83D16CA8(int event, ShellEventCallback callback, void *arg)
{
	return shellProcessEventServerAdd(event, callback, arg);
}

/* (0x81241228) */
int SceShell_E3D3A162(int event, ShellEventCallback callback, void *arg)
{
	return shellProcessEventServerRemove(event, callback, arg);
}

/* (0x81241230) */
int shellAppEventSend(int event, const void *data, int flags)
{
	return shellProcessEventServerSend(event, data, flags);
}

/* (0x81241238) */
int shellAppEventInit(void)
{
	if (++s_processEventInitCount == 1)
		s_processEventServer = new ShellProcessEventServer();
	return 0;
}
