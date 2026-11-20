/*
 * The shell's application event server (0x810E138C-0x810E181C): a second event server like the
 * shell's (event_server.cpp), with classes of its own (vtables 0x8152FD08-0x8152FD38; the
 * listener's Notify folds with the shell's) and its own mutex, Sony's
 * "SceShellAppEventServerMutex". Applications' listeners are added by event (game_plugin's alarm
 * and gc promote, mtp) and called when the shell sends one. The file's name, the classes' and the
 * internal functions' are ours (PLACEHOLDER): no record names them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/list>
#include <std/map>
#include <thread/thread.h>
#include "shell.h"

using namespace paf;

/* a listener: its callback and the argument it was added with */
class ShellAppEventListener {
public:
	ShellAppEventListener(ShellEventCallback callback, void *arg)
		: callback(callback), arg(arg), removed(false)
	{
	}

	virtual ~ShellAppEventListener()
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
class ShellAppEventListenerList {
public:
	typedef list<ShellAppEventListener *> Listeners;

	ShellAppEventListenerList() : sending(false)
	{
	}

	virtual ~ShellAppEventListenerList()
	{
		while (listeners.size() != 0) {
			Listeners::iterator first = listeners.begin();

			if (*first != NULL)
				delete *first;
			listeners.erase(first);
		}
	}

	ShellAppEventListener *Find(ShellAppEventListener *listener)
	{
		for (Listeners::iterator it = listeners.begin(); it != listeners.end(); ++it) {
			if (*it == listener)
				return *it;
		}
		return NULL;
	}

	ShellAppEventListener *Find(ShellEventCallback callback, void *arg)
	{
		for (Listeners::iterator it = listeners.begin(); it != listeners.end(); ++it) {
			ShellAppEventListener *listener = *it;

			if (listener->callback == callback && listener->arg == arg)
				return listener;
		}
		return NULL;
	}

	void Add(ShellAppEventListener *listener)
	{
		if (Find(listener) == NULL)
			listeners.push_back(listener);
	}

	Listeners listeners;
	bool sending;
};

/* the listener lists by event code */
class ShellAppEventServerBase {
public:
	typedef map<int, ShellAppEventListenerList *> ListenerMap;

	ShellAppEventServerBase()
	{
	}

	virtual ~ShellAppEventServerBase()
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

class ShellAppEventServer : public ShellAppEventServerBase {
public:
	ShellAppEventServer();
	virtual ~ShellAppEventServer();

	thread::RMutex mutex;
};

static ShellAppEventServer *s_appEventServer;

/* (0x810E138C) */
ShellAppEventServer::ShellAppEventServer() : mutex("SceShellAppEventServerMutex", 0)
{
}

/* (0x810E13E6, 0x810E1470) */
ShellAppEventServer::~ShellAppEventServer()
{
}

/* (0x810E1480) */
static __attribute__((noinline)) int
shellAppEventServerAdd(int code, ShellEventCallback callback, void *arg)
{
	thread::RMutexLock lock(&s_appEventServer->mutex);

	ShellAppEventServer::ListenerMap &lists = s_appEventServer->lists;

	ShellAppEventListener *listener = new ShellAppEventListener(callback, arg);
	ShellAppEventServer::ListenerMap::iterator it = lists.find(code);

	if (it != lists.end()) {
		if (it->second != NULL)
			it->second->Add(listener);
	} else {
		ShellAppEventListenerList *list = new ShellAppEventListenerList();

		list->Add(listener);
		lists.insert(ShellAppEventServer::ListenerMap::value_type(code, list));
	}
	return 0;
}

/* (0x810E15D6) */
static __attribute__((noinline)) int
shellAppEventServerRemove(int code, ShellEventCallback callback, void *arg)
{
	int ret = -1;

	thread::RMutexLock lock(&s_appEventServer->mutex);

	ShellAppEventServer::ListenerMap &lists = s_appEventServer->lists;

	ShellAppEventServer::ListenerMap::iterator it = lists.find(code);

	if (it != lists.end() && it->second != NULL) {
		ShellAppEventListenerList *list = it->second;
		ShellAppEventListener *listener = list->Find(callback, arg);
		ShellAppEventListenerList::Listeners::iterator where;

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

/* (0x810E16E0) */
static __attribute__((noinline)) int
shellAppEventServerSendEvent(int code, const void *data, SceSize size)
{
	thread::RMutexLock lock(&s_appEventServer->mutex);

	ShellAppEventServer::ListenerMap &lists = s_appEventServer->lists;

	ShellAppEventServer::ListenerMap::iterator it = lists.find(code);

	if (it != lists.end()) {
		int key = it->first;
		ShellAppEventListenerList *list = it->second;
		ShellAppEventListenerList::Listeners::iterator where;

		list->sending = true;
		for (where = list->listeners.begin(); where != list->listeners.end(); ++where) {
			if (*where != NULL)
				(*where)->Notify(key, data, size);
		}
		list->sending = false;
		for (where = list->listeners.begin(); where != list->listeners.end();) {
			ShellAppEventListener *listener = *where;

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

/* (0x810E17DE) */
int shellAppEventServerAddListener(int event, ShellEventCallback callback, void *arg)
{
	return shellAppEventServerAdd(event, callback, arg);
}

/* (0x810E17E6) */
int shellAppEventServerRemoveListener(int event, ShellEventCallback callback, void *arg)
{
	return shellAppEventServerRemove(event, callback, arg);
}

/* (0x810E17EE) */
int shellAppEventServerSend(int event, const void *data, SceSize size)
{
	return shellAppEventServerSendEvent(event, data, size);
}

/* (0x810E17F6) */
int shellAppEventServerCreate(void)
{
	s_appEventServer = new ShellAppEventServer();
	return 0;
}
