/*
 * socket.c: the stack's calls (SceNetPsForSyscalls). Each returns the stack's result, or for a
 * negative errno sets the thread's libnet errno and returns it as SceNet's error code; an
 * error the stack returns already coded only sets SCE_NET_ERETURN.
 */
#include "libnet_internal.h"

static inline int libnet_syscall_result(int ret)
{
	int error;

	if (ret >= 0)
		return ret;
	error = -ret;
	if ((error >> 16) & 0xfff) {
		*libnet_tls_errno() = SCE_NET_ERETURN;
		return ret;
	}
	*libnet_tls_errno() = error;
	return LIBNET_ERROR(error);
}

int libnet_get_sock_info(int s, SceNetSockInfo *info, int n, int flags)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallGetSockinfo(s, info, n, flags));
}

int libnet_socket_abort(int s, int flags)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallSocketAbort(s, flags));
}

int libnet_socket(const char *name, int domain, int type, int protocol)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallSocket(name, domain, type, protocol));
}

int libnet_bind(int s, const SceNetSockaddr *addr, unsigned int addrlen)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallBind(s, addr, addrlen));
}

int libnet_listen(int s, int backlog)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallListen(s, backlog));
}

int libnet_accept(int s, SceNetSockaddr *addr, unsigned int *addrlen)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallAccept(s, addr, addrlen));
}

int libnet_connect(int s, const SceNetSockaddr *name, unsigned int namelen)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallConnect(s, name, namelen));
}

int libnet_icm_connect(int s, int flags)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallIcmConnect(s, flags));
}

int libnet_sendmsg(int s, const SceNetMsghdr *msg, int flags)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallSendmsg(s, msg, flags));
}

int libnet_sendto(int s, const void *msg, unsigned int len, int flags, const SceNetSockaddr *to,
		  unsigned int tolen)
{
	SceNetSyscallSendtoParam parameter;

	parameter.socket = s;
	parameter.buffer = msg;
	parameter.length = len;
	parameter.flags = flags;
	parameter.address = to;
	parameter.addressLength = tolen;
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallSendto(&parameter));
}

int libnet_send(int s, const void *msg, unsigned int len, int flags)
{
	SceNetSyscallSendtoParam parameter;

	parameter.socket = s;
	parameter.buffer = msg;
	parameter.length = len;
	parameter.flags = flags;
	parameter.address = NULL;
	parameter.addressLength = 0;
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallSendto(&parameter));
}

int libnet_recvmsg(int s, SceNetMsghdr *msg, int flags)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallRecvmsg(s, msg, flags));
}

int libnet_recvfrom(int s, void *buf, unsigned int len, int flags, SceNetSockaddr *from,
		    unsigned int *fromlen)
{
	SceNetSyscallRecvfromParam parameter;

	parameter.socket = s;
	parameter.buffer = buf;
	parameter.length = len;
	parameter.flags = flags;
	parameter.address = from;
	parameter.addressLength = fromlen;
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallRecvfrom(&parameter));
}

int libnet_recv(int s, void *buf, unsigned int len, int flags)
{
	SceNetSyscallRecvfromParam parameter;

	parameter.socket = s;
	parameter.buffer = buf;
	parameter.length = len;
	parameter.flags = flags;
	parameter.address = NULL;
	parameter.addressLength = NULL;
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallRecvfrom(&parameter));
}

int libnet_shutdown(int s, int how)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallShutdown(s, how));
}

int libnet_setsockopt(int s, int level, int optname, const void *optval, unsigned int optlen)
{
	SceNetSyscallSetsockoptParam parameter;

	parameter.socket = s;
	parameter.level = level;
	parameter.option = optname;
	parameter.value = optval;
	parameter.length = optlen;
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallSetsockopt(&parameter));
}

int libnet_getsockopt(int s, int level, int optname, void *optval, unsigned int *optlen)
{
	SceNetSyscallGetsockoptParam parameter;

	parameter.socket = s;
	parameter.level = level;
	parameter.option = optname;
	parameter.value = optval;
	parameter.length = optlen;
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallGetsockopt(&parameter));
}

int libnet_getsockname(int s, SceNetSockaddr *name, unsigned int *namelen)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallGetsockname(s, name, namelen));
}

int libnet_getpeername(int s, SceNetSockaddr *name, unsigned int *namelen)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallGetpeername(s, name, namelen));
}

int libnet_ioctl(int s, unsigned int command, void *data)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallIoctl(s, command, data));
}

int libnet_socket_close(int s)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallClose(s));
}

int libnet_epoll_create(const char *name, int flags)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallEpollCreate(name, flags));
}

int libnet_epoll_control(int eid, int op, int id, SceNetEpollEvent *event)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallEpollCtl(eid, op, id, event));
}

int libnet_epoll_wait_events(int eid, SceNetEpollEvent *events, int maxevents, int *timeout)
{
	SceNetSyscallEpollWaitParam parameter;

	parameter.epoll = eid;
	parameter.events = events;
	parameter.maxevents = maxevents;
	parameter.timeout = timeout;
	parameter.callback = 0;
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallEpollWait(&parameter));
}

int libnet_epoll_wait_events_cb(int eid, SceNetEpollEvent *events, int maxevents, int *timeout)
{
	SceNetSyscallEpollWaitParam parameter;

	parameter.epoll = eid;
	parameter.events = events;
	parameter.maxevents = maxevents;
	parameter.timeout = timeout;
	parameter.callback = 1;
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallEpollWait(&parameter));
}

int libnet_epoll_destroy(int eid)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallEpollClose(eid));
}

int libnet_epoll_abort(int eid, int flags)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallEpollAbort(eid, flags));
}

int libnet_dump_create(const char *name, int len, int flags)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallDumpCreate(name, len, flags));
}

int libnet_dump_read(int id, void *buf, int len, int *pflags)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallDumpRead(id, buf, len, pflags));
}

int libnet_dump_destroy(int id)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallDumpClose(id));
}

int libnet_dump_abort(int id, int flags)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallDumpAbort(id, flags));
}

int libnet_descriptor_create(const char *name, int flags)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallDescriptorCreate(name, flags));
}

int libnet_descriptor_control(int id, int op, int s, void *info)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallDescriptorCtl(id, op, s, info));
}

int libnet_descriptor_destroy(int id)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallDescriptorClose(id));
}

int libnet_get_if_list(SceNetPsInterfaceInfo *list, int count)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallGetIfList(list, count));
}

int libnet_control(int ifIndex, int command, void *data, int length)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallControl(ifIndex, command, data, length));
}

int libnet_sysctl(const int *name, unsigned int namelen, void *oldp, unsigned int *oldlenp,
		  const void *newp, unsigned int newlen)
{
	SceNetSyscallSysctlParam parameter;

	parameter.name = name;
	parameter.nameLength = namelen;
	parameter.oldData = oldp;
	parameter.oldLength = oldlenp;
	parameter.newData = newp;
	parameter.newLength = newlen;
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	return libnet_syscall_result(sceNetSyscallSysctl(&parameter));
}
