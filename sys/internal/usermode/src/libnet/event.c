/*
 * event.c: network event handlers. A thread adds a handler for an interface (or -1, all of
 * them); it is registered with the interface's driver through a callback object, and runs when
 * the thread polls, or when its callbacks run while it waits in libnet_event_poll. An interface
 * that comes or goes is registered or unregistered on the next poll.
 */
#include <libkernel/libkernel.h>
#include "libnet_internal.h"

/* LibnetEventHandler.kernelIfIndex while no driver has the handler */
#define LIBNET_EVENT_UNREGISTERED	-2

/* bits of LibnetEventHandler.flags */
#define LIBNET_EVENT_POLLING		0x1	/* its thread polls: events may be taken */

/* bits of LibnetEventList.flags */
#define LIBNET_EVENT_INTERFACES_CHANGED	0x1

/* the driver events that add or remove an interface */
#define LIBNET_EVENT_INTERFACE_MASK	0x3

/* the callback object a driver calls, and what its notifications deliver */
typedef struct LibnetEventRegistration {
	SceUID	callbackId;
	int	clearPending;
} LibnetEventRegistration;

typedef struct LibnetEventHandler {
	LibnetCallback			callback;
	char				name[32];
	struct LibnetEventHandler	*next;
	struct LibnetEventHandler	*prev;
	LibnetEventRegistration		registration;
	int				threadId;
	int				kernelIfIndex;
	int				ifIndex;
	LibnetEventFunction		function;
	void				*argument;
	int				reserved;
	int				pollArgument;
	int				lastEvent;
	int				flags;
} LibnetEventHandler;

typedef struct LibnetEventList {
	LibnetEventHandler	*head;
	LibnetEventHandler	*tail;
	int			flags;
} LibnetEventList;

static LibnetEventList libnet_events;

/* the handler's callback: take its driver's events, run it, and post what it returns */
static int libnet_event_callback(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
				 void *common)
{
	LibnetEventHandler *handler = common;
	SceNetPsDriverEventControl control;
	SceUInt64 events;
	int ifIndex;

	if (!(handler->flags & LIBNET_EVENT_POLLING))
		return SCE_NET_ERROR_ENODATA;
	sceClibMemset(&control, 0, sizeof(control));
	control.callbackId = handler->registration.callbackId;
	control.clearPending = handler->registration.clearPending;
	libnet_control(handler->kernelIfIndex, SCE_NETPS_CONTROL_TAKE_DRIVER_EVENTS, &control,
		       sizeof(control));
	events = control.events;
	if (events == 0)
		return SCE_NET_ERROR_ENODATA;
	ifIndex = handler->ifIndex;
	if (ifIndex == -1 && (events & LIBNET_EVENT_INTERFACE_MASK))
		libnet_events.flags |= LIBNET_EVENT_INTERFACES_CHANGED;
	handler->lastEvent = control.error;
	events = handler->function(ifIndex, events, handler->argument, handler->pollArgument);
	sceClibMemset(&control, 0, sizeof(control));
	control.callbackId = handler->registration.callbackId;
	control.clearPending = handler->registration.clearPending;
	control.events = events;
	control.error = 0;
	return libnet_control(handler->kernelIfIndex, SCE_NETPS_CONTROL_POST_EVENTS, &control,
			      sizeof(control));
}

static int libnet_event_register(LibnetEventHandler *handler, int kernelIfIndex)
{
	int clearPending, ret;

	if (handler->kernelIfIndex == LIBNET_EVENT_UNREGISTERED) {
		sceClibStrncpy(handler->name, "SceNetEventCallback********", 31);
		libnet_name_hex(handler->threadId, handler->name, 31);
		ret = libnet_callback_create(&handler->registration.callbackId, handler,
					     handler->name);
		if (ret < 0)
			return ret;
		clearPending = 0;
	} else {
		clearPending = 1;
	}
	handler->registration.clearPending = clearPending;
	ret = libnet_control(kernelIfIndex, SCE_NETPS_CONTROL_REGISTER_DRIVER_CALLBACK,
			     &handler->registration, sizeof(handler->registration));
	if (ret < 0)
		libnet_callback_destroy(handler->registration.callbackId);
	else
		handler->kernelIfIndex = kernelIfIndex;
	return ret;
}

static int libnet_event_unregister(LibnetEventHandler *handler)
{
	int ret;

	ret = libnet_control(handler->kernelIfIndex, SCE_NETPS_CONTROL_UNREGISTER_DRIVER_CALLBACK,
			     &handler->registration, sizeof(handler->registration));
	if (ret < 0)
		libnet_callback_destroy(handler->registration.callbackId);
	else
		ret = libnet_callback_destroy(handler->registration.callbackId);
	handler->kernelIfIndex = LIBNET_EVENT_UNREGISTERED;
	return ret;
}

static inline void libnet_event_unlink(LibnetEventHandler *handler)
{
	if (handler->next != NULL)
		handler->next->prev = handler->prev;
	else
		libnet_events.tail = handler->prev;
	if (handler->prev != NULL)
		handler->prev->next = handler->next;
	else
		libnet_events.head = handler->next;
}

int libnet_event_handler_add(int ifIndex, LibnetEventFunction function, void *argument,
			     int flags)
{
	LibnetEventHandler *handler, *tail;
	int thread, count, kernelIfIndex, ret;

	if (flags != 0 || function == NULL)
		return SCE_NET_ERROR_EINVAL;
	thread = libnet_get_thread_id();
	count = 0;
	for (handler = libnet_events.head; handler != NULL; handler = handler->next) {
		if (handler->ifIndex != ifIndex)
			continue;
		count++;
		if (handler->threadId == thread || count > 3) {
			ret = SCE_NET_ERROR_EEXIST;
			goto unlink;
		}
	}
	handler = libnet_malloc(sizeof(*handler), 6);
	if (handler == NULL)
		return SCE_NET_ERROR_ENOLIBMEM;
	tail = libnet_events.tail;
	handler->callback.object = handler;
	handler->threadId = thread;
	handler->ifIndex = ifIndex;
	handler->function = function;
	handler->argument = argument;
	handler->callback.function = libnet_event_callback;
	handler->prev = tail;
	handler->callback.active = 1;
	handler->kernelIfIndex = LIBNET_EVENT_UNREGISTERED;
	handler->reserved = 0;
	if (tail != NULL)
		tail->next = handler;
	else
		libnet_events.head = handler;
	libnet_events.tail = handler;
	handler->next = NULL;
	if (ifIndex == -1) {
		kernelIfIndex = ifIndex;
	} else {
		kernelIfIndex = libnet_if_kernel_index(ifIndex);
		if (kernelIfIndex < 0)
			return 0;
	}
	ret = libnet_event_register(handler, kernelIfIndex);
	if (ret >= 0)
		return 0;
unlink:
	libnet_event_unlink(handler);
	libnet_free(handler);
	return ret;
}

int libnet_event_handler_remove(int ifIndex, int flags)
{
	LibnetEventHandler *handler;
	int thread, ret;

	if (flags == 0) {
		thread = libnet_get_thread_id();
		for (handler = libnet_events.head; handler != NULL; handler = handler->next) {
			if (handler->ifIndex != ifIndex || handler->threadId != thread)
				continue;
			if (handler->kernelIfIndex == LIBNET_EVENT_UNREGISTERED)
				ret = 0;
			else
				ret = libnet_event_unregister(handler);
			libnet_event_unlink(handler);
			libnet_free(handler);
			return ret;
		}
	}
	libnet_free(NULL);
	return SCE_NET_ERROR_EINVAL;
}

/* run the thread's handlers; with none delivered, wait up to TIMEOUT for their callbacks */
int libnet_event_poll(int timeout, int argument, int flags)
{
	LibnetEventHandler *handler;
	int thread, ret, delivered, found, kernelIfIndex;

	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	thread = libnet_get_thread_id();
	if (libnet_events.head == NULL)
		return SCE_NET_ERROR_ENOENT;
	ret = 0;
	delivered = 0;
	found = 0;
	for (handler = libnet_events.head; handler != NULL; handler = handler->next) {
		if (handler->threadId != thread ||
		    handler->kernelIfIndex == LIBNET_EVENT_UNREGISTERED)
			continue;
		handler->pollArgument = argument;
		handler->flags |= LIBNET_EVENT_POLLING;
		ret = libnet_event_callback(0, 0, 0, handler);
		if (ret >= 0)
			delivered = 1;
		found = 1;
		handler->flags &= ~LIBNET_EVENT_POLLING;
	}
	if (!found)
		return SCE_NET_ERROR_ENOENT;
	if (timeout != 0 && !delivered) {
		for (handler = libnet_events.head; handler != NULL; handler = handler->next)
			if (handler->threadId == thread)
				handler->flags |= LIBNET_EVENT_POLLING;
		libnet_unlock();
		ret = libnet_wait_cb(0, 0, 0, timeout);
		libnet_lock();
		for (handler = libnet_events.head; handler != NULL; handler = handler->next)
			if (handler->threadId == thread)
				handler->flags &= ~LIBNET_EVENT_POLLING;
	}
	if (!(libnet_events.flags & LIBNET_EVENT_INTERFACES_CHANGED))
		return ret;
	for (handler = libnet_events.head; handler != NULL; handler = handler->next) {
		if (thread != handler->threadId || handler->kernelIfIndex == -1)
			continue;
		kernelIfIndex = libnet_if_kernel_index(handler->ifIndex);
		if (kernelIfIndex < 0) {
			if (handler->kernelIfIndex != LIBNET_EVENT_UNREGISTERED)
				libnet_event_unregister(handler);
		} else {
			libnet_event_register(handler, kernelIfIndex);
		}
	}
	libnet_events.flags &= ~LIBNET_EVENT_INTERFACES_CHANGED;
	return ret;
}

int libnet_event_get_last(int ifIndex, int *event, int flags)
{
	LibnetEventHandler *handler;
	int thread;

	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	if (event == NULL)
		return SCE_NET_ERROR_EINVAL;
	thread = libnet_get_thread_id();
	for (handler = libnet_events.head; handler != NULL; handler = handler->next) {
		if (handler->ifIndex == ifIndex && handler->threadId == thread) {
			*event = handler->lastEvent;
			return 0;
		}
	}
	return SCE_NET_ERROR_EINVAL;
}
