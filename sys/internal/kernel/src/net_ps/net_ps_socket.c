/* SceNetPs socket adapters; original source filename is unavailable. */
/*
 * Copyright (c) 1982, 1986, 1989, 1990, 1993
 * The Regents of the University of California. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "net_ps_internal.h"
#include <net_ps/netps_gateway.h>
#include <net_ps/netps.h>
#include <string.h>

struct mbuf *m_get(int how, int type);
void sce_psnet_panic(const char *message, ...) __attribute__((noreturn));
extern struct malloc_type *mbtypes[8];
int sceNetPsDestroyDump(unsigned int dumpId, struct SceNetPsDump *dump);
int sceNetPsDestroyEpoll(unsigned int epollId, SceNetPsEpoll *epoll);
int sceNetPsDestroyDescriptor(unsigned int descriptorId,
			     SceNetPsDescriptor *descriptor);
int sceNetPsInterfaceControl(int interfaceIndex, int command, void *data,
			    int length);
int sceNetPsStackControl(int flags, int command, void *data, int length);
int sceNetPsRoutingSysctl(const int *name, unsigned int nameLength, void *oldData,
			  unsigned int *oldLength, const void *newData,
			  unsigned int newLength);

/* SCE: the public query exposes the DNS addresses and configuration flags. */
int sceNetPsQueryDnsConfig(int flags, void *buffer, unsigned int length)
{
	SceNetPsDnsConfig config;
	int result;

	if (flags)
		return 0x8041810c;
	if (!buffer)
		return 0x80418101;
	if (length <= 11)
		return 0x8041810c;
	result = sceNetPsGetDnsConfig(&config, 0, 1);
	if (result >= 0) {
		memcpy(buffer, &config, 12);
		return 12;
	}
	if ((result & 0xffff0000) == 0xffff0000)
		result = -result | 0x80410100;
	return result;
}

/* SCE: the gateway allocator callbacks ignore their interface argument. */
int sceNetPsGatewayFree(struct SceNetKernelGateway *interface, void *pointer)
{
	(void)interface;
	return sceNetPsFree(pointer);
}

void *sceNetPsGatewayAllocate(struct SceNetKernelGateway *interface,
			    SceSize size, SceSize alignment)
{
	(void)interface;
	return sceNetPsMalloc(size, 1, alignment);
}

extern SceNetPsGatewayState *gNetPsGatewayInterfaceSlot;

/* SCE: release the jumbo lock before the socket send may block. */
int sceNetPsGatewaySend(struct SceNetKernelGateway *interface, const void *data,
			unsigned int length, unsigned int destinationAddress)
{
	SceNetSockaddrIn address;
	int socket;
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (gNetPsGatewayInterfaceSlot->interface != interface) {
		result = 0x80418108;
		goto failed;
	}
	if (!gNetPsGatewayInterfaceSlot->interface) {
		result = 0x80418107;
		goto failed;
	}
	memset(&address, 0, sizeof(address));
	address.sin_addr.s_addr = destinationAddress;
	socket = gNetPsGatewayInterfaceSlot->socket;
	address.sin_family = 2;
	address.sin_len = sizeof(address);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return SceNetPsForDriver_AB746734(socket, data, length, 0,
			     (const SceNetSockaddr *)&address, sizeof(address));
failed:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

/* SCE: notifications carry the DNS addresses with all remaining words clear. */
int sceNetPsNotifyGatewayDns(const SceNetPsDnsConfig *config)
{
	SceNetPsDnsConfig notification;

	if (gNetPsGatewayInterfaceSlot->interface && config) {
		memset(&notification, 0, sizeof(notification));
		notification.dns = config->dns;
		gNetPsGatewayInterfaceSlot->interface->notify(
			gNetPsGatewayInterfaceSlot->interface, 1,
			&notification, sizeof(notification));
	}
	return 0;
}

extern SceNetPsMutex gNetPsDevSendMutex;
extern SceNetPsMutex gNetPsDevRecvMutex;

/* Reviewed export name: Sony's gateway socket string and prototype candidate. */
int SceNetKernelGateway(struct SceNetKernelGateway *interface)
{
	int socket;
	int value;
	int result;

	if (!interface || !interface->receive || !interface->notify)
		return 0x80418101;
	socket = SceNetPsForDriver_EB95B024("SceNetKernelGateway", 2, 3, 0);
	if (socket < 0)
		return 0x80418105;
	value = 1;
	if (SceNetPsForDriver_4BF5FAB4(socket, 0, 2, &value, sizeof(value)) < 0)
		goto socketFailed;
	value = 1;
	if (SceNetPsForDriver_4BF5FAB4(socket, 0xffff, 0x1002,
			      &value, sizeof(value)) < 0)
		goto socketFailed;
	value = 1;
	if (SceNetPsForDriver_4BF5FAB4(socket, 0, 25, &value, sizeof(value)) < 0)
		goto socketFailed;
	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	sce_psnet_bnet_mutex_lock(&gNetPsDevSendMutex, NULL);
	sce_psnet_bnet_mutex_lock(&gNetPsDevRecvMutex, NULL);
	if (!gNetPsGatewayInterfaceSlot) {
		result = 0x80418106;
	} else if (gNetPsGatewayInterfaceSlot->interface) {
		result = 0x80418101;
	} else {
		gNetPsGatewayInterfaceSlot->interface = interface;
		gNetPsGatewayInterfaceSlot->socket = socket;
		interface->allocate = sceNetPsGatewayAllocate;
		gNetPsGatewayInterfaceSlot->interface->free = sceNetPsGatewayFree;
		gNetPsGatewayInterfaceSlot->interface->send = sceNetPsGatewaySend;
		gNetPsGatewayInterfaceSlot->interface->portBase = 40000;
		gNetPsGatewayInterfaceSlot->interface->portCount = 9152;
		sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
		sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return 0;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	goto closeSocket;
socketFailed:
	result = 0x80418106;
closeSocket:
	SceNetPsForDriver_21F4428D(socket);
	return result;
}

/* SCE: detach under all three locks, then close the socket outside them. */
int sceNetPsUnregisterGateway(struct SceNetKernelGateway *interface)
{
	int socket;
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	sce_psnet_bnet_mutex_lock(&gNetPsDevSendMutex, NULL);
	sce_psnet_bnet_mutex_lock(&gNetPsDevRecvMutex, NULL);
	if (!gNetPsGatewayInterfaceSlot) {
		result = 0x80418106;
		goto failed;
	}
	if (gNetPsGatewayInterfaceSlot->interface != interface) {
		result = 0x80418101;
		goto failed;
	}
	gNetPsGatewayInterfaceSlot->interface = NULL;
	socket = gNetPsGatewayInterfaceSlot->socket;
	sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (socket >= 0)
		SceNetPsForDriver_21F4428D(socket);
	return 0;
failed:
	sce_psnet_bnet_mutex_unlock(&gNetPsDevRecvMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsDevSendMutex);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

/*
 * SCE adaptation of NetBSD uipc_syscalls.c 1.90.2.4 sockargs:
 * copy selector, platform external allocation, M_EXT without M_EXT_RW,
 * no upstream length rejection, and m_freem on copy failure.
 */
/* SCE: references in Sony's complete socket owner guard upstream sofree. */
void bnet_sorele(struct socket *socket)
{
	if (socket) {
		if (socket->refCount <= 0)
			sce_psnet_panic("", __func__);
		if (--socket->refCount == 0)
			sofree(socket);
	}
}

int sceNetPsSockargs(struct mbuf **result, const void *buffer,
		     unsigned int length, int type, int kernel)
{
	struct sockaddr *address;
	struct mbuf *mbuf;
	int error;

	mbuf = m_get(0, type);
	if (length > MLEN) {
		mbuf->m_ext.ext_buf = sceNetPsMalloc(length, 0, 8);
		if (mbuf->m_ext.ext_buf != NULL) {
			mbuf->m_data = mbuf->m_ext.ext_buf;
			mbuf->m_ext.ext_size = length;
			mbuf->m_ext.ext_free = NULL;
			mbuf->m_ext.ext_arg = NULL;
			mbuf->m_flags = (mbuf->m_flags & ~M_EXTCOPYFLAGS) | M_EXT;
			mbuf->m_ext.ext_type = mbtypes[mbuf->m_type];
			mbuf->m_ext.ext_nextref = mbuf;
			mbuf->m_ext.ext_prevref = mbuf;
		}
	}
	mbuf->m_len = length;
	error = sceNetPsCopyIn(buffer, mbuf->m_data, length, kernel);
	if (error) {
		m_freem(mbuf);
		return error;
	}
	*result = mbuf;
	if (type == MT_SONAME) {
		address = mtod(mbuf, struct sockaddr *);
		if (address->sa_family == 0 && address->sa_len < AF_MAX)
			address->sa_family = address->sa_len;
		address->sa_len = length;
	}
	return error;
}

int sceNetPsCollectSockInfo(int socket, SceNetSockInfo *records, int capacity,
			 int flags, int *count, SceUID processId,
			 int kernel);

unsigned int sceNetPsGetSockinfo(int socket, void *buffer, int capacity, int flags)
{
	int count = 0;
	int result, records;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (flags & ~0x23) {
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return -22;
	}
	result = sceNetPsCollectSockInfo(socket, buffer, capacity, flags,
					&count, 0, 0);
	records = count;
	if (result < 0) {
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return result;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (!result)
		return records;
	return -result;
}

int sceNetPsGetIfList(void *list, int count)
{
	unsigned int size;
	void *buffer = NULL;
	int error = 0;
	int result = 0;
	int empty = list != NULL && count == 0;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (empty)
		goto freeBuffer;
	if (list == NULL) {
		result = sceNetPsGetInterfaceList(NULL, count);
		goto freeBuffer;
	}
	size = count * 320U;
	buffer = sceNetPsMalloc(size, 3, 8);
	if (buffer == NULL) {
		error = 12;
		goto freeBuffer;
	}
	result = sceNetPsGetInterfaceList(buffer, count);
	if (result < 0)
		goto freeBuffer;
	error = copyout(buffer, list, size);
	sceNetPsFree(buffer);
	if (error < 0) {
		error = -error;
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return -error;
	}
	goto out;
freeBuffer:
	sceNetPsFree(buffer);
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (error)
		return -error;
	return result;
}

int sceNetPsControl(int interfaceIndex, int command, void *data, int length)
{
	void *buffer = NULL;
	int result = 0;
	int error = 0;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (data != NULL) {
		buffer = sceNetPsMalloc(length, 1, 8);
		if (buffer == NULL) {
			error = 12;
			goto freeBuffer;
		}
		if (command & 0x30000000) {
			error = copyin(data, buffer, length);
			if (error)
				goto copyError;
		}
	}
	if (interfaceIndex >= 0)
		result = sceNetPsInterfaceControl(interfaceIndex, command, buffer, length);
	else
		result = sceNetPsStackControl(0, command, buffer, length);
	if (result >= 0 && data != NULL &&
	    (command & 0x30000000) != 0x20000000) {
		error = copyout(buffer, data, length);
		goto copyError;
	}
freeBuffer:
	sceNetPsFree(buffer);
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (error)
		return -error;
	return result;
copyError:
	sceNetPsFree(buffer);
	if (error < 0) {
		error = -error;
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return -error;
	}
	goto out;
}

int sceNetPsSysctl(const int *name, unsigned int nameLength, void *oldData,
		   unsigned int *oldLength, const void *newData,
		   unsigned int newLength)
{
	unsigned int length = 0;
	unsigned int *lengthPointer;
	unsigned int capacity;
	int identifiers[12];
	int error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (oldLength != NULL) {
		error = copyin(oldLength, &length, sizeof(length));
		if (error)
			goto copyError;
		lengthPointer = &length;
		capacity = length;
	} else {
		lengthPointer = NULL;
		capacity = 0;
	}
	if (nameLength - 2 > 10) {
		error = 22;
		goto out;
	}
	error = copyin(name, identifiers,
					nameLength * sizeof(*identifiers));
	if (error)
		goto copyError;
	if (identifiers[0] != 4) {
		error = 45;
		goto out;
	}
	if (nameLength == 2 || (identifiers[1] != 0 && nameLength == 3)) {
		error = 21;
		goto positiveError;
	}
	if (identifiers[1] != 0) {
		if (identifiers[1] != 17) {
			error = 42;
			goto positiveError;
		}
		error = sceNetPsRoutingSysctl(identifiers + 3, nameLength - 3,
					      oldData, lengthPointer,
					      newData, newLength);
		if (error)
			goto copyError;
	}
	if (oldLength != NULL) {
		error = copyout(&length, oldLength, sizeof(length));
		if (error)
			goto copyError;
	}
	if (oldData != NULL)
		error = capacity < length ? 12 : 0;
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (error)
		return -error;
	return error;
copyError:
	if (error < 0) {
		error = -error;
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return -error;
	}
positiveError:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -error;
}

int sceNetPsDescriptorCreate(const char *name, int flags)
{
	SceNetPsDescriptor *descriptor;
	unsigned int copied;
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (flags != 0 || name == NULL) {
		result = -22;
		goto out;
	}
	descriptor = sceNetPsMalloc(sizeof(*descriptor), 3, 8);
	if (descriptor == NULL) {
		result = -12;
		goto out;
	}
	result = sceNetPsAllocateDescriptorId();
	if (result < 0) {
		result = -24;
		sceNetPsFree(descriptor);
		goto out;
	}
	sceNetPsCopyInString(name, descriptor->name, 31, &copied);
	sceNetPsSetDescriptor(result, descriptor);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

int sceNetPsDescriptorClose(int descriptorId)
{
	SceNetPsDescriptor *descriptor;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	descriptor = sceNetPsLookupDescriptor(descriptorId, 0);
	if (descriptor != NULL) {
		sceNetPsDestroyDescriptor(descriptorId, descriptor);
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return 0;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -9;
}

/* SCE: append a bounded record to the capture ring, or set overflow. */
void sceNetPsWriteDump(SceNetPsDump *dump, const void *buffer, int length)
{
	const char *source = buffer;
	int first;

	if (dump->flags & SCE_NET_DUMP_OVERFLOW)
		return;
	if (dump->length - dump->bufferedLength < length) {
		dump->flags |= SCE_NET_DUMP_OVERFLOW;
		return;
	}
	first = dump->length - dump->writePosition;
	if (length < first)
		first = length;
	if (source)
		memcpy(dump->buffer + dump->writePosition, source, first);
	else
		memset(dump->buffer + dump->writePosition, 0, first);
	dump->writePosition += first;
	dump->bufferedLength += first;
	length -= first;
	if (length > 0) {
		dump->writePosition = 0;
		if (source)
			memcpy(dump->buffer, source + first, length);
		else
			memset(dump->buffer, 0, length);
		dump->writePosition += length;
		dump->bufferedLength += length;
	}
	if (dump->writePosition >= dump->length)
		dump->writePosition -= dump->length;
}

int sceNetPsDestroyDump(unsigned int dumpId, SceNetPsDump *dump)
{
	extern int gNetPsPacketCaptureActive;

	--gNetPsPacketCaptureActive;
	if (dump->waiters)
		sceNetPsWakeupCanceled(dump);
	sceNetPsConsumeWakeup((const void *)(uintptr_t)dumpId, 0);
	sceNetPsFree(dump);
	sceNetPsSetDump(dumpId, NULL);
	return 0;
}

int sceNetPsDumpCreate(const char *name, int length, int flags)
{
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (length < 2048 || flags != 0 || name == NULL) {
		result = -22;
	} else {
		result = sceNetPsAllocateDumpId();
		if (result < 0)
			result = -24;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

int sceNetPsDumpClose(int dumpId)
{
	struct SceNetPsDump *dump;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	dump = sceNetPsLookupDump(dumpId, 0);
	if (dump != NULL) {
		sceNetPsDestroyDump(dumpId, dump);
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return 0;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -9;
}

int sceNetPsDumpAbort(int dumpId, int flags)
{
	struct SceNetPsDump *dump;
	int error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (flags & ~1) {
		error = 22;
	} else {
		dump = sceNetPsLookupDump(dumpId, 0);
		if (dump == NULL) {
			error = 9;
		} else {
			error = sceNetPsWakeupInterrupted(dump);
			if (error != 0) {
				if (flags)
					error = sceNetPsQueueWakeup((const void *)(uintptr_t)dumpId, 1);
				if (error < 0) {
					error = -error;
					sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
					return -error;
				}
			}
		}
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return error ? -error : error;
}

/* SCE: wake each epoll that watches the socket, directly or through a descriptor */
void sceNetPsWakeupEpolls(struct socket *socket)
{
	SceNetPsEpoll *epoll;
	SceNetPsEpollEntry *entry;
	SceNetPsDescriptorEntry *descriptorEntry;
	int first, last;
	int epollId;

	sceNetPsGetEpollIdRange(&first, &last);
	for (epollId = first; epollId <= last; epollId++) {
		epoll = sceNetPsLookupEpoll(epollId, 1);
		if (epoll == NULL)
			continue;
		for (entry = epoll->entries; entry != NULL; entry = entry->next) {
			if ((entry->kind & SCE_NETPS_EPOLL_ENTRY_SOCKET) &&
			    entry->target.socket == socket)
				goto wakeup;
			if ((entry->kind & SCE_NETPS_EPOLL_ENTRY_DESCRIPTOR) &&
			    entry->target.descriptor != NULL) {
				for (descriptorEntry = entry->target.descriptor->entries;
				     descriptorEntry != NULL;
				     descriptorEntry = descriptorEntry->next)
					if (descriptorEntry->socket == socket)
						goto wakeup;
			}
		}
		continue;
wakeup:
		sceNetPsWakeupNormal(epoll);
	}
}

int sceNetPsEpollCreate(const char *name, int flags)
{
	SceNetPsEpoll *epoll;
	unsigned int copied;
	int result;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (flags != 0 || name == NULL) {
		result = -22;
		goto out;
	}
	epoll = sceNetPsMalloc(sizeof(*epoll), 3, 8);
	if (epoll == NULL) {
		result = -12;
		goto out;
	}
	result = sceNetPsAllocateEpollId();
	if (result < 0) {
		result = -24;
		sceNetPsFree(epoll);
		goto out;
	}
	sceNetPsCopyInString(name, epoll->name, 31, &copied);
	sceNetPsSetEpoll(result, epoll);
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;
}

int sceNetPsEpollClose(int epollId)
{
	SceNetPsEpoll *epoll;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	epoll = sceNetPsLookupEpoll(epollId, 0);
	if (epoll != NULL) {
		sceNetPsDestroyEpoll(epollId, epoll);
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return 0;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -9;
}

int sceNetPsEpollAbort(int epollId, int flags)
{
	SceNetPsEpoll *epoll;
	unsigned int invalidFlags;
	int error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	invalidFlags = flags & ~1;
	if (invalidFlags) {
		error = 22;
		goto out;
	}
	epoll = sceNetPsLookupEpoll(epollId, invalidFlags);
	if (epoll == NULL) {
		error = 9;
		goto out;
	}
	error = sceNetPsWakeupInterrupted(epoll);
	if (error == 15) {
		if (flags == 0)
			goto out;
		error = sceNetPsQueueWakeup((const void *)(uintptr_t)epollId, 1);
	}
	if (error < 0) {
		error = -error;
		sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
		return -error;
	}
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	if (error)
		return -error;
	return invalidFlags;
out:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -error;
}
