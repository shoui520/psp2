/*
 * libnet_internal.h: SceNet's own interfaces between its files. No record names libnet's
 * files or internal functions; the port layer's names (libnet_sync_create, _wait, _signal)
 * are its own messages', and the rest follow them (agent namecheck lists the reviews).
 */
#ifndef LIBNET_INTERNAL_H
#define LIBNET_INTERNAL_H

#include <scetypes.h>
#include <stdarg.h>
#include <stddef.h>
#include <net.h>
#include <net_ps/netps.h>
#include <net_ps/netps_gateway.h>
#include <net_ps/netps_thread.h>

/* an error the stack returns as a negative BSD errno, as SceNet reports it */
#define LIBNET_ERROR(errno)		((int)(0x80410100U | (errno)))

/* an event flag and the bits signalled to it (the port layer's sync object) */
typedef struct LibnetSync {
	SceUID	eventFlag;
	int	bits;
	char	padding[24];
} LibnetSync;

/* a thread the port layer starts: ENTRY(ARGUMENT), then exit when FLAGS bit 0 */
typedef struct LibnetThreadParam {
	void	(*entry)(void *argument);
	void	*argument;
	int	priority;
	int	stackSize;
	int	flags;
} LibnetThreadParam;

/* a callback object: its notifications call FUNCTION with the object as COMMON (port.c) */
typedef struct LibnetCallback {
	int	(*function)(SceUID notifyId, SceInt32 notifyCount, SceInt32 notifyArg,
			    void *common);
	void	*object;			/* the object itself */
	int	active;
} LibnetCallback;

/* an interface's counters since the last call (interface.c) */
typedef struct LibnetIfStatistics {
	unsigned int	txCounters[8];
	unsigned int	rxCounters[8];
	int		uptimeSeconds;
} LibnetIfStatistics;

/* what the last call saw of an interface, while its generation stays the same */
typedef struct LibnetIfStatisticsBaseline {
	LibnetIfStatistics	last;
	unsigned int		generation;
} LibnetIfStatisticsBaseline;

/* the interfaces libnet knows, by its own index */
#define LIBNET_IF_COUNT		10

/* where libnet's debug messages go: one character at a time (printf.c) */
typedef void (*LibnetOutput)(void *argument, int c);

/* libnet.c: the library's state */
typedef struct LibnetGlobals {
	LibnetSync	lockSync;		/* the library lock */
	LibnetSync	dhcpSync;		/* SceNetInternal_F6291476(0) */
	int		lockOwner;		/* the thread holding the lock, or -1 */
	int		lockCount;		/* its recursion depth */
	LibnetIfStatisticsBaseline *statisticsMemory;	/* each interface's last counters */
	void		*netstatMemory;
	int		initialized;
	LibnetOutput	debugDestination;	/* SceNetInternal_A64CFBF5 */
} LibnetGlobals;

extern LibnetGlobals libnet;
void libnet_set_errno(int error);
int SceNetInternal_694F8996(const char *format, ...);

/* print to the console and to libnet's debug output; the console's count is the value */
#define LIBNET_PRINTF(...)						\
	({								\
		int __printed = sceClibPrintf(__VA_ARGS__);		\
		SceNetInternal_694F8996(__VA_ARGS__);			\
		__printed;						\
	})
int libnet_lock(void);
int libnet_unlock(void);

/* a link-level address family (BSD's AF_LINK) */
#define LIBNET_AF_LINK		18

/* a link-level address in a routing message (BSD's sockaddr_dl) */
typedef struct LibnetSockaddrDl {
	SceUChar8	sdl_len;
	SceUChar8	sdl_family;
	SceUShort16	sdl_index;
	SceUChar8	sdl_type;
	SceUChar8	sdl_nlen;		/* the interface name's length */
	SceUChar8	sdl_alen;		/* the link address's length */
	SceUChar8	sdl_slen;
	char		sdl_data[12];		/* the name, then the address */
} LibnetSockaddrDl;

typedef union LibnetRouteAddress {
	SceNetSockaddr		sa;
	SceNetSockaddrIn	in;
	LibnetSockaddrDl	dl;
} LibnetRouteAddress;

/* one route, as libnet_get_route_table returns it from the kernel's routing messages */
typedef struct LibnetRouteEntry {
	char			ifName[16];
	LibnetRouteAddress	destination;
	LibnetRouteAddress	netmask;
	LibnetRouteAddress	gateway;
	int			ifIndex;
	int			flags;
	int			mtu;
} LibnetRouteEntry;

/* ping.c: the address to reach, and its link address once a route has it */
typedef struct LibnetPingParam {
	SceNetInAddr	address;
	SceNetEtherAddr	ether;
} LibnetPingParam;

int libnet_ping(LibnetPingParam *param, int flags);

/* dns.c */
int libnet_dns_init(void);
int libnet_dns_term(void);
int libnet_get_dns_info(SceNetDnsInfo *info, int flags);
int libnet_set_dns_info(const SceNetDnsInfo *info, int flags);
int libnet_set_kernel_dns_info(const SceNetDnsInfo *info, int flags);

/* emulation.c */
int libnet_emulation_set(SceNetEmulationParam *param, int ifIndex, int flags);
int libnet_emulation_get(SceNetEmulationParam *param, int ifIndex, int flags);

/* epoll.c */
int libnet_epoll_wait(int eid, SceNetEpollEvent *events, int maxevents, int timeout,
		      int callback);

/* ether.c */
int libnet_ether_strton(const char *str, SceNetEtherAddr *n);
int libnet_ether_ntostr(const SceNetEtherAddr *n, char *str, SceSize len);
int libnet_get_mac_address(SceNetEtherAddr *addr, int flags);
int libnet_get_if_mac_address(SceNetEtherAddr *addr, int flags);

/* event.c: what a handler returns becomes the events posted back to the interface */
typedef SceUInt64 (*LibnetEventFunction)(int ifIndex, SceUInt64 events, void *argument,
					 int pollArgument);
int libnet_event_handler_add(int ifIndex, LibnetEventFunction function, void *argument,
			     int flags);
int libnet_event_handler_remove(int ifIndex, int flags);
int libnet_event_poll(int timeout, int argument, int flags);
int libnet_event_get_last(int ifIndex, int *event, int flags);

/* control.c: the stack's settings, through sceNetSyscallControl */
int libnet_get_connection_param(int ifIndex, SceNetPsConnectionParam *param, int flags);
int libnet_set_connection_param(int ifIndex, const SceNetPsConnectionParam *param, int flags);
int libnet_update_connections(int ifIndex, int argument, int flags);
int libnet_reset_idle_time(int ifIndex, int argument, int flags);
int libnet_get_process_policy(int processId, int *policy, int flags);
int libnet_set_process_policy(int processId, int policy, int flags);
int libnet_get_thread_mode(int threadId, int *mode, int flags);
int libnet_set_thread_mode(int threadId, int mode, int flags);
int libnet_get_sleep_counts(SceNetPsSleepCount *counts, int count, int flags);
int libnet_get_thread_list(SceNetPsThreadInfo *list, int count, int flags);
int libnet_get_promiscuous(int ifIndex, SceNetPsPromiscuousParam *param, int flags);
int libnet_set_promiscuous(int ifIndex, const SceNetPsPromiscuousParam *param, int flags);
int libnet_get_error(int *error, int flags);
int libnet_set_error(int error, int flags);

/* interface.c */
const char *libnet_if_name(unsigned int ifIndex);
int libnet_get_if_info(unsigned int ifIndex, SceNetPsInterfaceInfo *info, int count);
int libnet_if_kernel_index(int ifIndex);
int libnet_get_if_statistics(int ifIndex, LibnetIfStatistics *statistics, int count,
			     LibnetIfStatisticsBaseline *baseline);
int libnet_get_if_indices(int *indices, int count, int flags);
SceSize libnet_strnlen(const char *str, SceSize maxlen);
int *libnet_errno_loc(void);

/* socket.c */
int libnet_get_sock_info(int s, SceNetSockInfo *info, int n, int flags);
int libnet_socket_abort(int s, int flags);
int libnet_socket(const char *name, int domain, int type, int protocol);
int libnet_bind(int s, const SceNetSockaddr *addr, unsigned int addrlen);
int libnet_listen(int s, int backlog);
int libnet_accept(int s, SceNetSockaddr *addr, unsigned int *addrlen);
int libnet_connect(int s, const SceNetSockaddr *name, unsigned int namelen);
int libnet_icm_connect(int s, int flags);
int libnet_sendmsg(int s, const SceNetMsghdr *msg, int flags);
int libnet_sendto(int s, const void *msg, unsigned int len, int flags, const SceNetSockaddr *to,
		  unsigned int tolen);
int libnet_send(int s, const void *msg, unsigned int len, int flags);
int libnet_recvmsg(int s, SceNetMsghdr *msg, int flags);
int libnet_recvfrom(int s, void *buf, unsigned int len, int flags, SceNetSockaddr *from,
		    unsigned int *fromlen);
int libnet_recv(int s, void *buf, unsigned int len, int flags);
int libnet_shutdown(int s, int how);
int libnet_setsockopt(int s, int level, int optname, const void *optval, unsigned int optlen);
int libnet_getsockopt(int s, int level, int optname, void *optval, unsigned int *optlen);
int libnet_getsockname(int s, SceNetSockaddr *name, unsigned int *namelen);
int libnet_getpeername(int s, SceNetSockaddr *name, unsigned int *namelen);
int libnet_ioctl(int s, unsigned int command, void *data);
int libnet_socket_close(int s);
int libnet_epoll_create(const char *name, int flags);
int libnet_epoll_control(int eid, int op, int id, SceNetEpollEvent *event);
int libnet_epoll_wait_events(int eid, SceNetEpollEvent *events, int maxevents, int *timeout);
int libnet_epoll_wait_events_cb(int eid, SceNetEpollEvent *events, int maxevents, int *timeout);
int libnet_epoll_destroy(int eid);
int libnet_epoll_abort(int eid, int flags);
int libnet_dump_create(const char *name, int len, int flags);
int libnet_dump_read(int id, void *buf, int len, int *pflags);
int libnet_dump_destroy(int id);
int libnet_dump_abort(int id, int flags);
int libnet_descriptor_create(const char *name, int flags);
int libnet_descriptor_control(int id, int op, int s, void *info);
int libnet_descriptor_destroy(int id);
int libnet_get_if_list(SceNetPsInterfaceInfo *list, int count);
int libnet_control(int ifIndex, int command, void *data, int length);
int libnet_sysctl(const int *name, unsigned int namelen, void *oldp, unsigned int *oldlenp,
		  const void *newp, unsigned int newlen);

/* memory.c: the pool the library allocates from, sceNetInit's memory */
int libnet_memory_free_size(int minimum);
void *libnet_malloc(int size, int flags);
void libnet_free(void *ptr);
int libnet_memory_init(void *pool, int size, int reserve);
int libnet_memory_term(void);

/* stack.c: routes, random numbers, socket ids and statistics of the stack */
int libnet_set_socket_suspend_policy(int policy);
int libnet_set_tcp_mss(void *mss, int flags);
int libnet_get_route_table(int ifIndex, int family, LibnetRouteEntry *entries, int count,
			   int flags);
LibnetRouteEntry *libnet_get_route_entries(int ifIndex, int *count);
int libnet_free_route_entries(LibnetRouteEntry *entries);
int libnet_get_random(void);
int libnet_get_sock_id_info(SceNetFdSet *fds, int sockinfoflags, int flags);
int libnet_get_statistics_info(SceNetStatisticsInfo *info, int flags);
int libnet_take_senders(SceNetPsSenderList *list, int flags);

/* show.c */
int libnet_show_ifconfig(void);
int libnet_show_route(void);
int libnet_show_netstat(void);

/* resolver.c */
int libnet_resolver_init(void);
int libnet_resolver_term(void);
int libnet_resolver_create(const char *name, SceNetResolverParam *param, int flags);
int libnet_resolver_start_ntoa(int rid, const char *hostname, SceNetInAddr *addr, int timeout,
			       int retry, int flags);
int libnet_resolver_start_aton(int rid, const SceNetInAddr *addr, char *hostname, int len,
			       int timeout, int retry, int flags);
int libnet_resolver_get_error(int rid, int *result);
int libnet_resolver_destroy(int rid);
int libnet_resolver_abort(int rid, int flags);
int libnet_clear_dns_cache(int flags);

/* inet.c */
const char *libnet_inet_ntop(int af, const void *src, char *dst, unsigned int size);
int libnet_inet_pton(int af, const char *src, void *dst, int strict);

/* printf.c */
int libnet_vprintf(LibnetOutput output, const char *format, va_list ap);

/* port.c: the thread, sync and time services of SceLibKernel and SceThreadmgr */
int libnet_random_init(int seed);
int libnet_random_term(void);
unsigned int libnet_random(void);
SceUInt64 libnet_get_time(SceUInt64 *time);
int libnet_name_hex(int value, char *name, int length);
int libnet_thread_create(SceUID *thread, LibnetThreadParam *param, const char *name);
int libnet_get_thread_id(void);
int libnet_thread_join(SceUID thread);
int libnet_thread_exit(void);
int libnet_usleep(int usec);
int libnet_sync_create(LibnetSync *sync, int bits, const char *name);
int libnet_sync_destroy(LibnetSync *sync);
int libnet_sync_wait(LibnetSync *sync, SceUInt32 *timeout);
int libnet_sync_signal(LibnetSync *sync, int bits);
int libnet_callback_create(SceUID *callback, void *argument, const char *name);
int libnet_callback_destroy(SceUID callback);
int libnet_wait_cb(int unused0, int unused1, int unused2, int usec);
int *libnet_tls_errno(void);

#endif /* LIBNET_INTERNAL_H */
