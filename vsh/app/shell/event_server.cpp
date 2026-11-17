/*
 * The shell's event server: listeners added per event code (SceShell_7FE4E732), removed
 * (SceShell_D4C7D2B7) and called when the shell sends an event. A listener removed while its
 * event is being sent is only marked, and dropped once the sending ends. The file's and every
 * name here are ours (no record names them); the mutex is Sony's "SceShellEventServerMutex".
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/list>
#include <std/map>
#include <thread/thread.h>
#include "shell.h"

using namespace paf;

/* a listener: its callback and the argument it was added with */
class ShellEventListener {
public:
	ShellEventListener(ShellEventCallback callback, void *arg)
		: callback(callback), arg(arg), removed(false)
	{
	}

	virtual ~ShellEventListener()
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

/* the listeners of one event code */
class ShellEventListenerList {
public:
	typedef list<ShellEventListener *> Listeners;

	ShellEventListenerList() : sending(false)
	{
	}

	virtual ~ShellEventListenerList()
	{
		while (listeners.size() != 0) {
			Listeners::iterator first = listeners.begin();

			if (*first != NULL)
				delete *first;
			listeners.erase(first);
		}
	}

	ShellEventListener *Find(ShellEventListener *listener)
	{
		for (Listeners::iterator it = listeners.begin(); it != listeners.end(); ++it) {
			if (*it == listener)
				return *it;
		}
		return NULL;
	}

	ShellEventListener *Find(ShellEventCallback callback, void *arg)
	{
		for (Listeners::iterator it = listeners.begin(); it != listeners.end(); ++it) {
			ShellEventListener *listener = *it;

			if (listener->callback == callback && listener->arg == arg)
				return listener;
		}
		return NULL;
	}

	void Add(ShellEventListener *listener)
	{
		if (Find(listener) == NULL)
			listeners.push_back(listener);
	}

	Listeners listeners;
	bool sending;
};

/* the listener lists by event code */
class ShellEventServerBase {
public:
	typedef map<int, ShellEventListenerList *> ListenerMap;

	ShellEventServerBase()
	{
	}

	virtual ~ShellEventServerBase()
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

class ShellEventServer : public ShellEventServerBase {
public:
	ShellEventServer();
	virtual ~ShellEventServer();

	thread::RMutex mutex;
};

static ShellEventServer *s_eventServer;

ShellEventServer::ShellEventServer() : mutex("SceShellEventServerMutex", 0)
{
}

ShellEventServer::~ShellEventServer()
{
}

__attribute__((noinline)) static int shellEventServerAddListener(int code, ShellEventCallback callback, void *arg)
{
	thread::RMutexLock lock(&s_eventServer->mutex);

	ShellEventServer::ListenerMap &lists = s_eventServer->lists;

	ShellEventListener *listener = new ShellEventListener(callback, arg);
	ShellEventServer::ListenerMap::iterator it = lists.find(code);

	if (it != lists.end()) {
		if (it->second != NULL)
			it->second->Add(listener);
	} else {
		ShellEventListenerList *list = new ShellEventListenerList();

		list->Add(listener);
		lists.insert(ShellEventServer::ListenerMap::value_type(code, list));
	}
	return 0;
}

__attribute__((noinline)) static int shellEventServerRemoveListener(int code, ShellEventCallback callback, void *arg)
{
	int ret = -1;

	thread::RMutexLock lock(&s_eventServer->mutex);

	ShellEventServer::ListenerMap &lists = s_eventServer->lists;

	ShellEventServer::ListenerMap::iterator it = lists.find(code);

	if (it != lists.end() && it->second != NULL) {
		ShellEventListenerList *list = it->second;
		ShellEventListener *listener = list->Find(callback, arg);
		ShellEventListenerList::Listeners::iterator where;

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

__attribute__((noinline)) static int shellEventServerSendEvent(int code, const void *data, SceSize size)
{
	thread::RMutexLock lock(&s_eventServer->mutex);

	ShellEventServer::ListenerMap &lists = s_eventServer->lists;

	ShellEventServer::ListenerMap::iterator it = lists.find(code);

	if (it != lists.end()) {
		int key = it->first;
		ShellEventListenerList *list = it->second;
		ShellEventListenerList::Listeners::iterator where;

		list->sending = true;
		for (where = list->listeners.begin(); where != list->listeners.end(); ++where) {
			if (*where != NULL)
				(*where)->Notify(key, data, size);
		}
		list->sending = false;
		for (where = list->listeners.begin(); where != list->listeners.end();) {
			ShellEventListener *listener = *where;

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

int SceShell_7FE4E732(int code, ShellEventCallback callback, void *arg)
{
	return shellEventServerAddListener(code, callback, arg);
}

int SceShell_D4C7D2B7(int code, ShellEventCallback callback, void *arg)
{
	return shellEventServerRemoveListener(code, callback, arg);
}

int shellEventServerSend(int code, const void *data, SceSize size)
{
	return shellEventServerSendEvent(code, data, size);
}

int shellEventServerCreate(void)
{
	s_eventServer = new ShellEventServer();
	return 0;
}
