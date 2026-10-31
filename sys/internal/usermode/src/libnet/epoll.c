/*
 * epoll.c: wait on an epoll object. An event of a descriptor (SCE_NET_EPOLLDESCID) may carry a
 * handler in its system words: it returns 0 to pass the event on, 1 when it consumed it.
 * While every event was consumed, the wait goes on for the time left.
 */
#include "libnet_internal.h"

typedef int (*LibnetEpollHandler)(SceNetEpollEvent *event);

int libnet_epoll_wait(int eid, SceNetEpollEvent *events, int maxevents, int timeout,
		      int callback)
{
	LibnetEpollHandler handler;
	SceNetEpollEvent *event;
	int ret, handled, i, result;

	do {
		if (callback)
			ret = libnet_epoll_wait_events_cb(eid, events, maxevents, &timeout);
		else
			ret = libnet_epoll_wait_events(eid, events, maxevents, &timeout);
		if (ret < 0)
			break;
		if (ret == 0)
			continue;
		handled = 0;
		event = events;
		for (i = 0; i != ret; i++, event++) {
			if (!(event->events & SCE_NET_EPOLLDESCID))
				continue;
			handler = (LibnetEpollHandler)event->system.system[0];
			if (handler == NULL)
				continue;
			result = handler(event);
			if (result == 0)
				continue;
			handled++;
			if (result != 1)
				return SCE_NET_ERROR_ECALLBACK;
			event->events &= ~SCE_NET_EPOLLDESCID;
		}
		if (ret != handled)
			break;
	} while (timeout != 0);
	return ret;
}
