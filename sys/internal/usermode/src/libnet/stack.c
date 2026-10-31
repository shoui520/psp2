/*
 * stack.c: what libnet asks the stack directly: the socket suspend policy and TCP MSS, the
 * IPv4 routes (from the routing socket's sysctl dump), a random number, the sockets' ids,
 * and its statistics.
 */
#include <libkernel/libkernel.h>
#include "libnet_internal.h"

/* the routing sysctl: CTL_NET, PF_ROUTE, 0, AF_INET, NET_RT_DUMP, 0 */
#define LIBNET_CTL_NET			4
#define LIBNET_PF_ROUTE			17
#define LIBNET_NET_RT_DUMP		1

/* a routing message's addresses (BSD's RTA_*), in this order after its header */
#define LIBNET_RTA_DST			0x01
#define LIBNET_RTA_GATEWAY		0x02
#define LIBNET_RTA_NETMASK		0x04
#define LIBNET_RTA_GENMASK		0x08
#define LIBNET_RTA_IFP			0x10
#define LIBNET_RTA_IFA			0x20
#define LIBNET_RTA_AUTHOR		0x40
#define LIBNET_RTA_BRD			0x80

#define LIBNET_RTF_GATEWAY		0x02

/* bits of libnet_get_route_table's flags */
#define LIBNET_ROUTE_DEFAULT		0x1	/* only the default route, into one entry */

/* the flags sceNetGetSockIdInfo accepts */
#define LIBNET_SOCK_ID_INFO_FLAGS	0x001f0007

/* the header of a routing message (BSD's struct rt_msghdr) */
typedef struct LibnetRtMsghdr {
	SceUShort16	rtm_msglen;
	SceUChar8	rtm_version;
	SceUChar8	rtm_type;
	SceUShort16	rtm_index;
	int		rtm_flags;
	int		rtm_addrs;
	int		rtm_pid;
	int		rtm_seq;
	int		rtm_errno;
	int		rtm_use;
	SceUInt32	rtm_inits;
	SceUInt32	rmx_locks;
	SceUInt32	rmx_mtu;
	SceUInt32	rmx_metrics[8];
} LibnetRtMsghdr;

/* a socket address's space in a routing message */
#define LIBNET_RT_ROUNDUP(sa)	((sa)->sa_len ? 1 + (((sa)->sa_len - 1) | 3) : 4)

int libnet_set_socket_suspend_policy(int policy)
{
	int value = policy;

	return libnet_control(-1, SCE_NETPS_CONTROL_SOCKET_SUSPEND_POLICY, &value, sizeof(value));
}

int libnet_set_tcp_mss(void *mss, int flags)
{
	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	return libnet_control(-1, SCE_NETPS_CONTROL_SET_TCP_MSS, mss, 8);
}

/* the IPv4 routes of IFINDEX (-1: all) into ENTRIES, or without ENTRIES their number */
int libnet_get_route_table(int ifIndex, int family, LibnetRouteEntry *entries, int count,
			   int flags)
{
	SceNetSockaddr *address[8];
	LibnetRtMsghdr *message, *buffer;
	LibnetSockaddrDl *ifp;
	SceNetSockaddr *sa;
	char *end;
	int mib[6];
	int kernelIfIndex, size, ret, addrs;

	if (family != SCE_NET_AF_INET)
		goto invalid;
	if ((flags & LIBNET_ROUTE_DEFAULT) && (count != 1 || entries == NULL))
		goto invalid;
	if (ifIndex == -1) {
		kernelIfIndex = -1;
	} else {
		kernelIfIndex = libnet_if_kernel_index(ifIndex);
		if (kernelIfIndex < 0) {
			libnet_free(NULL);
			return kernelIfIndex;
		}
	}
	mib[0] = LIBNET_CTL_NET;
	mib[3] = SCE_NET_AF_INET;
	mib[4] = LIBNET_NET_RT_DUMP;
	mib[2] = 0;
	mib[5] = 0;
	mib[1] = LIBNET_PF_ROUTE;
	ret = libnet_sysctl(mib, 6, NULL, (unsigned int *)&size, NULL, 0);
	if (ret < 0) {
		buffer = NULL;
		goto out;
	}
	buffer = NULL;
	ret = size;
	if (size == 0)
		goto out;
	size = (size + 3) & ~3;
	buffer = libnet_malloc(size, 6);
	if (buffer == NULL) {
		ret = SCE_NET_ERROR_ENOLIBMEM;
		goto out;
	}
	ret = libnet_sysctl(mib, 6, buffer, (unsigned int *)&size, NULL, 0);
	if (ret < 0)
		goto out;
	ret = 0;
	end = (char *)buffer + size / 4 * 4;
	for (message = buffer; (char *)message < end;
	     message = (LibnetRtMsghdr *)((char *)message + (message->rtm_msglen & ~3))) {
		sceClibMemset(address, 0, sizeof(address));
		addrs = message->rtm_addrs;
		sa = (SceNetSockaddr *)(message + 1);
		if (addrs & LIBNET_RTA_DST) {
			address[0] = sa;
			if (sa->sa_family != SCE_NET_AF_INET)
				continue;
			sa = (SceNetSockaddr *)((char *)sa + LIBNET_RT_ROUNDUP(sa));
		}
		if (addrs & LIBNET_RTA_GATEWAY) {
			address[1] = sa;
			sa = (SceNetSockaddr *)((char *)sa + LIBNET_RT_ROUNDUP(sa));
		}
		if (addrs & LIBNET_RTA_NETMASK) {
			address[2] = sa;
			sa = (SceNetSockaddr *)((char *)sa + LIBNET_RT_ROUNDUP(sa));
		}
		if (addrs & LIBNET_RTA_GENMASK) {
			address[3] = sa;
			sa = (SceNetSockaddr *)((char *)sa + LIBNET_RT_ROUNDUP(sa));
		}
		if (addrs & LIBNET_RTA_IFP) {
			address[4] = sa;
			sa = (SceNetSockaddr *)((char *)sa + LIBNET_RT_ROUNDUP(sa));
		}
		if (addrs & LIBNET_RTA_IFA) {
			address[5] = sa;
			sa = (SceNetSockaddr *)((char *)sa + LIBNET_RT_ROUNDUP(sa));
		}
		if (addrs & LIBNET_RTA_AUTHOR) {
			address[6] = sa;
			sa = (SceNetSockaddr *)((char *)sa + LIBNET_RT_ROUNDUP(sa));
		}
		if (addrs & LIBNET_RTA_BRD)
			address[7] = sa;
		if (entries == NULL) {
			ret++;
			continue;
		}
		if (ret >= count)
			break;
		if (flags & LIBNET_ROUTE_DEFAULT) {
			if (!(message->rtm_flags & LIBNET_RTF_GATEWAY) || address[0] == NULL ||
			    ((SceNetSockaddrIn *)address[0])->sin_addr.s_addr != 0)
				continue;
		}
		if (ifIndex != -1 && message->rtm_index != kernelIfIndex)
			continue;
		sceClibMemset(entries, 0, sizeof(*entries));
		ifp = (LibnetSockaddrDl *)address[4];
		if (ifp != NULL && ifp->sdl_len > 19 && ifp->sdl_family == LIBNET_AF_LINK) {
			sceClibMemcpy_safe(entries->ifName, ifp->sdl_data, ifp->sdl_nlen);
			entries->ifName[ifp->sdl_nlen] = '\0';
		}
		if (address[0] != NULL)
			sceClibMemcpy_safe(&entries->destination, address[0], address[0]->sa_len);
		if (address[2] != NULL)
			sceClibMemcpy_safe(&entries->netmask, address[2], address[2]->sa_len);
		if (address[1] != NULL)
			sceClibMemcpy_safe(&entries->gateway, address[1], address[1]->sa_len);
		ret++;
		entries->ifIndex = message->rtm_index;
		entries->flags = message->rtm_flags;
		entries->mtu = message->rmx_mtu;
		entries++;
	}
out:
	libnet_free(buffer);
	return ret;
invalid:
	libnet_free(NULL);
	return SCE_NET_ERROR_EINVAL;
}

LibnetRouteEntry *libnet_get_route_entries(int ifIndex, int *count)
{
	LibnetRouteEntry *entries;
	int n;

	n = libnet_get_route_table(ifIndex, SCE_NET_AF_INET, NULL, 0, 0);
	if (n <= 0)
		return NULL;
	entries = libnet_malloc(n * sizeof(*entries), 4);
	if (entries == NULL)
		return NULL;
	n = libnet_get_route_table(ifIndex, SCE_NET_AF_INET, entries, n, 0);
	*count = n;
	if (n <= 0) {
		libnet_free(entries);
		return NULL;
	}
	return entries;
}

int libnet_free_route_entries(LibnetRouteEntry *entries)
{
	libnet_free(entries);
	return 0;
}

int libnet_get_random(void)
{
	int value = 0;

	libnet_control(-1, SCE_NETPS_CONTROL_GET_RANDOM, &value, sizeof(value));
	return value;
}

/* the ids of the sockets whose flags have one of SOCKINFOFLAGS */
int libnet_get_sock_id_info(SceNetFdSet *fds, int sockinfoflags, int flags)
{
	SceNetSockInfo *infos, *info, *end;
	int ret, n, i;
	SceNetId s;

	if (fds == NULL || (sockinfoflags & ~LIBNET_SOCK_ID_INFO_FLAGS)) {
		ret = SCE_NET_ERROR_EINVAL;
		infos = NULL;
		goto out;
	}
	ret = libnet_get_sock_info(-1, NULL, 0, flags);
	if (ret < 0) {
		infos = NULL;
		goto out;
	}
	infos = libnet_malloc(ret * sizeof(*infos), 2);
	if (infos == NULL)
		goto out;
	n = libnet_get_sock_info(-1, infos, ret, flags);
	ret = n;
	if (n < 0)
		goto out;
	for (i = 0; i != SCE_NET_FD_SETSIZE >> SCE_NET_NBITS_SHIFT; i++)
		fds->bits[i] = 0;
	if (n == 0)
		goto out;
	ret = 0;
	info = infos;
	end = infos + n;
	do {
		if (sockinfoflags & info->flags) {
			s = info->s;
			ret++;
			if ((unsigned int)s < SCE_NET_FD_SETSIZE)
				fds->bits[(unsigned int)s >> SCE_NET_NBITS_SHIFT] |= 1 << (s & 31);
		}
		info++;
	} while (info != end);
out:
	libnet_free(infos);
	return ret;
}

int libnet_get_statistics_info(SceNetStatisticsInfo *info, int flags)
{
	SceNetPsKernelStatistics statistics;
	int ret;

	if (info == NULL)
		return SCE_NET_ERROR_EINVAL;
	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	ret = libnet_control(-1, SCE_NETPS_CONTROL_GET_STATISTICS, &statistics,
			     sizeof(statistics));
	if (ret < 0)
		return ret;
	info->packet_count = statistics.packet_count;
	info->packet_qos_count = statistics.packet_qos_count;
	info->kernel_mem_free_size = statistics.kernel_mem_free_size;
	info->kernel_mem_free_min = statistics.kernel_mem_free_min;
	info->libnet_mem_free_size = libnet_memory_free_size(flags);
	info->libnet_mem_free_min = libnet_memory_free_size(1);
	return ret;
}

int libnet_take_senders(SceNetPsSenderList *list, int flags)
{
	if (list == NULL)
		return SCE_NET_ERROR_EINVAL;
	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	return libnet_control(-1, SCE_NETPS_CONTROL_TAKE_SENDERS, list, sizeof(*list));
}
