/*
 * SceNet (vs0:sys/external/libnet.suprx): initialisation, the library lock, and the exports.
 * Every export but the byte-order and socket calls checks that the library is initialised,
 * and most hold the library lock around the call; the error each returns also sets
 * sceNetErrnoLoc()'s errno.
 */
#include <stdarg.h>
#include <libkernel/libkernel.h>
#include "libnet_internal.h"

/* the stack's ioctl that sets a P2P socket's key (udpp2p's SIOCP2PSETKEY) */
#define LIBNET_SIOCP2PSETKEY	0x802450C9

/* the argument of SIOCP2PSETKEY (udpp2p's struct udpp2p_peerreq) */
typedef struct LibnetP2pKeyRequest {
	SceNetSockaddrIn	address;
	SceUChar8		key[16];
	SceUShort16		port;
	SceUChar8		flags;
	SceUChar8		group;
} LibnetP2pKeyRequest;

LibnetGlobals libnet;

static int libnet_init(SceNetInitParam *param, int flags)
{
	LibnetP2pKeyRequest request;
	SceUChar8 *key;
	int ret, s;

	if (libnet.initialized)
		return SCE_NET_ERROR_EBUSY;
	if (param == NULL || param->memory == NULL || param->size < 0x4000 || param->flags != 0)
		return SCE_NET_ERROR_EINVAL;
	ret = libnet_sync_create(&libnet.lockSync, 1, "SceNetLibnetSync");
	if (ret < 0)
		return ret;
	ret = libnet_sync_create(&libnet.dhcpSync, 1, "SceNetDhcpSync");
	if (ret >= 0) {
		ret = libnet_memory_init(param->memory, param->size, flags);
		if (ret >= 0) {
			libnet.initialized = 1;
			ret = libnet_random_init(libnet_get_random());
			if (ret >= 0) {
				ret = libnet_resolver_init();
				if (ret >= 0) {
					ret = libnet_dns_init();
					if (ret >= 0) {
						libnet.statisticsMemory = libnet_malloc(LIBNET_IF_COUNT *
											sizeof(*libnet.statisticsMemory), 6);
						if (libnet.statisticsMemory != NULL) {
							libnet.netstatMemory = libnet_malloc(4096, 6);
							if (libnet.netstatMemory != NULL) {
								s = libnet_socket("", SCE_NET_AF_INET,
										  SCE_NET_SOCK_DGRAM_P2P, 0);
								if (s >= 0) {
									sceClibMemset(&request, 0, sizeof(request));
									key = request.key - 1;
									do
										*++key = libnet_random();
									while (key != &request.key[15]);
									ret = libnet_ioctl(s, LIBNET_SIOCP2PSETKEY,
											   &request);
									if (ret >= 0) {
										libnet_socket_close(s);
										libnet.lockOwner = -1;
										libnet.lockCount = 0;
										return ret;
									}
									libnet_socket_close(s);
								}
								libnet_free(libnet.netstatMemory);
							}
							libnet_free(libnet.statisticsMemory);
						}
						libnet_dns_term();
					}
					libnet_resolver_term();
				}
				libnet.initialized = 0;
				libnet_random_term();
			}
			libnet_memory_term();
		}
		libnet_sync_destroy(&libnet.dhcpSync);
	}
	libnet_sync_destroy(&libnet.lockSync);
	return ret;
}

/* an SceNet error that carries a BSD errno sets it; any other error sets SCE_NET_ERETURN */
void libnet_set_errno(int error)
{
	if (error < 0) {
		if (((error >> 8) & 0xff) == 1)
			*libnet_errno_loc() = error & 0xff;
		else
			*libnet_errno_loc() = SCE_NET_ERETURN;
	}
}

/* the library lock: recursive for its owner thread */
inline int libnet_lock(void)
{
	int self = libnet_get_thread_id();

	if (self != libnet.lockOwner)
		libnet_sync_wait(&libnet.lockSync, NULL);
	libnet.lockOwner = self;
	libnet.lockCount++;
	return 0;
}

inline int libnet_unlock(void)
{
	if (--libnet.lockCount == 0) {
		libnet.lockOwner = -1;
		libnet_sync_signal(&libnet.lockSync, 1);
	}
	return 0;
}

int sceNetInit(SceNetInitParam *param)
{
	return libnet_init(param, 0);
}

int SceNetInternal_65AFF9BE(SceNetInitParam *param)
{
	return libnet_init(param, 0x2000);
}

int sceNetTerm(void)
{
	if (!libnet.initialized)
		return SCE_NET_ERROR_ENOTINIT;
	libnet_free(libnet.netstatMemory);
	libnet_free(libnet.statisticsMemory);
	libnet_dns_term();
	libnet_resolver_term();
	libnet_random_term();
	libnet_memory_term();
	libnet_sync_destroy(&libnet.dhcpSync);
	libnet_sync_destroy(&libnet.lockSync);
	libnet.initialized = 0;
	return 0;
}

int SceNetInternal_EE1985D9(int policy)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_set_socket_suspend_policy(policy);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_A6B9F5D2(void *mss, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_set_tcp_mss(mss, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

const char *SceNetInternal_F3917021(unsigned int ifIndex)
{
	return libnet_if_name(ifIndex);
}

int SceNetInternal_7FD92AE7(int ifIndex)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	ret = libnet_if_kernel_index(ifIndex);
	libnet_set_errno(ret);
	return ret;
}

int SceNetInternal_235DE96C(unsigned int ifIndex, SceNetPsInterfaceInfo *info, int count)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	ret = libnet_get_if_info(ifIndex, info, count);
	libnet_set_errno(ret);
	return ret;
}

int SceNetInternal_EE94BA15(int ifIndex, LibnetIfStatistics *statistics, int count)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	ret = libnet_get_if_statistics(ifIndex, statistics, count, libnet.statisticsMemory);
	libnet_set_errno(ret);
	return ret;
}

int SceNetInternal_8157DE3E(int *indices, int count, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	if ((flags & ~1) != 0)
		ret = SCE_NET_ERROR_EINVAL;
	else
		ret = libnet_get_if_indices(indices, count, flags);
	libnet_set_errno(ret);
	return ret;
}

/* a control command to interface IFINDEX, or with IFINDEX -1 to the kernel's KERNELINDEX */
int SceNetInternal_AA414742(int ifIndex, int kernelIndex, int command, void *data, int length)
{
	int ret = kernelIndex;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	if (ifIndex == -1 && kernelIndex < 0) {
		ret = SCE_NET_ERROR_EINVAL;
	} else {
		if (ifIndex != -1) {
			if (kernelIndex != 0) {
				ret = SCE_NET_ERROR_EINVAL;
				goto out;
			}
			ret = libnet_if_kernel_index(ifIndex);
			if (ret < 0)
				goto out;
		}
		ret = libnet_control(ret, command, data, length);
	}
out:
	libnet_set_errno(ret);
	return ret;
}

int SceNetInternal_7F63379E(SceNetDnsInfo *info, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_get_dns_info(info, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetSetDnsInfo(SceNetDnsInfo *info, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_set_dns_info(info, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_16222ED6(SceNetDnsInfo *info, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_set_kernel_dns_info(info, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetClearDnsCache(int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = SCE_NET_ERROR_EINVAL;
	if (flags == 0)
		ret = libnet_clear_dns_cache(0);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_BDA92B76(int family, void *entries, int count, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_get_route_table(-1, family, entries, count, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

LibnetRouteEntry *SceNetInternal_5B0E370C(int ifIndex, int *count)
{
	LibnetRouteEntry *ret;

	if (!libnet.initialized)
		return NULL;
	libnet_lock();
	ret = libnet_get_route_entries(ifIndex, count);
	libnet_unlock();
	return ret;
}

int SceNetInternal_CF49E9C4(LibnetRouteEntry *entries)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_free_route_entries(entries);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_AAA34850(LibnetPingParam *param, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_ping(param, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetGetStatisticsInfo(SceNetStatisticsInfo *info, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_get_statistics_info(info, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_A9F2277C(SceNetPsSenderList *list, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_take_senders(list, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_01B090F1(int ifIndex, void *param, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_get_connection_param(ifIndex, param, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_E03F6A77(int ifIndex, const void *param, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_set_connection_param(ifIndex, param, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_26EFF46D(int ifIndex, int argument, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_update_connections(ifIndex, argument, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_4F5E0E8A(int ifIndex, int argument, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_reset_idle_time(ifIndex, argument, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_EE94BF52(int processId, int *policy, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_get_process_policy(processId, policy, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_0DFC23D6(int processId, int policy, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_set_process_policy(processId, policy, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_EF421A65(int threadId, int *mode, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_get_thread_mode(threadId, mode, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_9DBD2F60(int threadId, int mode, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_set_thread_mode(threadId, mode, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_DEF2357F(void *counts, int count, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_get_sleep_counts(counts, count, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_46E28336(void *list, int count, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_get_thread_list(list, count, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_F9E3A8B7(int ifIndex, void *param, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_get_promiscuous(ifIndex, param, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_853EEABE(int ifIndex, const void *param, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_set_promiscuous(ifIndex, param, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_DE8DD55E(int *error, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_get_error(error, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_7493FB0C(int error, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_set_error(error, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_EAC33599(int ifIndex, LibnetEventFunction function, void *argument, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_event_handler_add(ifIndex, function, argument, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_EDAA3453(int ifIndex, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_event_handler_remove(ifIndex, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_04E6136D(int timeout, int argument, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_event_poll(timeout, argument, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_B01ABD0A(int ifIndex, int *event, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_event_get_last(ifIndex, event, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

void *SceNetInternal_A2C2BC5F(int size, int flags)
{
	return libnet_malloc(size, flags);
}

void SceNetInternal_E62A2FAA(void *ptr)
{
	libnet_free(ptr);
}

int SceNetInternal_2FEC8B74(SceUInt64 *time)
{
	SceUInt64 now;
	int ret = 0;

	if (time != NULL) {
		ret = (int)libnet_get_time(&now);
		*time = now;
	}
	return ret;
}

int SceNetInternal_689B9D7D(unsigned int *random)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	if (random != NULL) {
		ret = 0;
		*random = libnet_random();
	} else {
		ret = SCE_NET_ERROR_EINVAL;
	}
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

/* where SceNetInternal_694F8996's messages go; zero drops them */
int SceNetInternal_A64CFBF5(LibnetOutput destination)
{
	int ret;

	if (!libnet.initialized) {
		ret = SCE_NET_ERROR_ENOTINIT;
		libnet_set_errno(ret);
		return ret;
	}
	libnet_lock();
	libnet.debugDestination = destination;
	libnet_unlock();
	return 0;
}

int SceNetInternal_694F8996(const char *format, ...)
{
	va_list ap;
	int ret;

	if (libnet.debugDestination == NULL)
		return 0;
	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	va_start(ap, format);
	ret = libnet_vprintf(libnet.debugDestination, format, ap);
	va_end(ap);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_225C4801(SceUID *thread, LibnetThreadParam *param, const char *name)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	if (param != NULL)
		ret = libnet_thread_create(thread, param, name);
	else
		ret = SCE_NET_ERROR_EINVAL;
	libnet_set_errno(ret);
	return ret;
}

int SceNetInternal_3B5F94B7(SceUID thread)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	ret = libnet_thread_join(thread);
	libnet_set_errno(ret);
	return ret;
}

int SceNetInternal_4EF96CD8(void)
{
	return libnet_thread_exit();
}

int SceNetInternal_104E4820(int usec)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	ret = libnet_usleep(usec);
	libnet_set_errno(ret);
	return ret;
}

int SceNetInternal_4054D2D8(LibnetSync *sync, int bits, const char *name)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_sync_create(sync, bits, name);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNetInternal_97527BDB(LibnetSync *sync)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_sync_destroy(sync);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

LibnetSync *SceNetInternal_F6291476(int which)
{
	if (which != 0)
		return NULL;
	return &libnet.dhcpSync;
}

int SceNetInternal_C2FF615E(LibnetSync *sync, SceUInt32 *timeout)
{
	return libnet_sync_wait(sync, timeout);
}

int SceNetInternal_9D12D8A4(LibnetSync *sync, int bits)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	ret = libnet_sync_signal(sync, bits);
	libnet_set_errno(ret);
	return ret;
}

int sceNetShowIfconfig(void)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_show_ifconfig();
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetShowRoute(void)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_show_route();
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetShowNetstat(void)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_show_netstat();
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetEmulationSet(SceNetEmulationParam *param, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_emulation_set(param, -1, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetEmulationGet(SceNetEmulationParam *param, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_emulation_get(param, -1, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetEtherStrton(const char *str, SceNetEtherAddr *n)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	ret = libnet_ether_strton(str, n);
	libnet_set_errno(ret);
	return ret;
}

int sceNetEtherNtostr(const SceNetEtherAddr *n, char *str, SceSize len)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	ret = libnet_ether_ntostr(n, str, len);
	libnet_set_errno(ret);
	return ret;
}

int sceNetGetMacAddress(SceNetEtherAddr *addr, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_get_mac_address(addr, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int SceNet_D886F498(SceNetEtherAddr *addr, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_get_if_mac_address(addr, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

const char *sceNetInetNtop(int af, const void *src, char *dst, SceSize size)
{
	return libnet_inet_ntop(af, src, dst, size);
}

int sceNetInetPton(int af, const char *src, void *dst)
{
	return libnet_inet_pton(af, src, dst, 1);
}

int SceNetInternal_916EA798(int af, const char *src, void *dst)
{
	return libnet_inet_pton(af, src, dst, 0);
}

SceUInt64 sceNetHtonll(SceUInt64 host64)
{
	return __builtin_bswap64(host64);
}

SceUInt32 sceNetHtonl(SceUInt32 host32)
{
	return __builtin_bswap32(host32);
}

SceUShort16 sceNetHtons(SceUShort16 host16)
{
	return ((unsigned int)host16 >> 8) | (((unsigned int)host16 << 8) & 0xFFFF);
}

SceUInt64 sceNetNtohll(SceUInt64 net64)
{
	return __builtin_bswap64(net64);
}

SceUInt32 sceNetNtohl(SceUInt32 net32)
{
	return __builtin_bswap32(net32);
}

SceUShort16 sceNetNtohs(SceUShort16 net16)
{
	return ((unsigned int)net16 >> 8) | (((unsigned int)net16 << 8) & 0xFFFF);
}

SceNetId sceNetResolverCreate(const char *name, SceNetResolverParam *param, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_resolver_create(name, param, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetResolverStartNtoa(SceNetId rid, const char *hostname, SceNetInAddr *addr, int timeout,
			    int retry, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_resolver_start_ntoa(rid, hostname, addr, timeout, retry, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetResolverStartAton(SceNetId rid, const SceNetInAddr *addr, char *hostname, int len,
			    int timeout, int retry, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_resolver_start_aton(rid, addr, hostname, len, timeout, retry, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetResolverGetError(SceNetId rid, int *result)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_resolver_get_error(rid, result);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetResolverDestroy(SceNetId rid)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_resolver_destroy(rid);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetResolverAbort(SceNetId rid, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_resolver_abort(rid, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetGetSockInfo(SceNetId s, SceNetSockInfo *info, int n, int flags)
{
	return libnet_get_sock_info(s, info, n, flags);
}

int sceNetGetSockIdInfo(SceNetFdSet *fds, int sockinfoflags, int flags)
{
	int ret;

	if (!libnet.initialized) {
		libnet_set_errno(SCE_NET_ERROR_ENOTINIT);
		return SCE_NET_ERROR_ENOTINIT;
	}
	libnet_lock();
	ret = libnet_get_sock_id_info(fds, sockinfoflags, flags);
	libnet_set_errno(ret);
	libnet_unlock();
	return ret;
}

int sceNetSocketAbort(SceNetId s, int flags)
{
	return libnet_socket_abort(s, flags);
}

SceNetId sceNetSocket(const char *name, int domain, int type, int protocol)
{
	return libnet_socket(name, domain, type, protocol);
}

int sceNetBind(SceNetId s, SceNetSockaddrConstArg addr, SceNetSocklen_t addrlen)
{
	return libnet_bind(s, addr.sockaddr, addrlen);
}

int sceNetListen(SceNetId s, int backlog)
{
	return libnet_listen(s, backlog);
}

SceNetId sceNetAccept(SceNetId s, SceNetSockaddrArg addr, SceNetSocklen_t *addrlen)
{
	return libnet_accept(s, addr.sockaddr, addrlen);
}

int sceNetConnect(SceNetId s, SceNetSockaddrConstArg name, SceNetSocklen_t namelen)
{
	return libnet_connect(s, name.sockaddr, namelen);
}

int SceNetInternal_93F2FF08(SceNetId s, int flags)
{
	return libnet_icm_connect(s, flags);
}

int sceNetSendmsg(SceNetId s, const SceNetMsghdr *msg, int flags)
{
	return libnet_sendmsg(s, msg, flags);
}

int sceNetSendto(SceNetId s, const void *msg, SceSize len, int flags, SceNetSockaddrConstArg to,
		 SceNetSocklen_t tolen)
{
	return libnet_sendto(s, msg, len, flags, to.sockaddr, tolen);
}

int sceNetSend(SceNetId s, const void *msg, SceSize len, int flags)
{
	return libnet_sendto(s, msg, len, flags, NULL, 0);
}

int sceNetRecvmsg(SceNetId s, SceNetMsghdr *msg, int flags)
{
	return libnet_recvmsg(s, msg, flags);
}

int sceNetRecvfrom(SceNetId s, void *buf, SceSize len, int flags, SceNetSockaddrArg from,
		   SceNetSocklen_t *fromlen)
{
	return libnet_recvfrom(s, buf, len, flags, from.sockaddr, fromlen);
}

int sceNetRecv(SceNetId s, void *buf, SceSize len, int flags)
{
	return libnet_recvfrom(s, buf, len, flags, NULL, NULL);
}

int sceNetShutdown(SceNetId s, int how)
{
	return libnet_shutdown(s, how);
}

int sceNetSetsockopt(SceNetId s, int level, int optname, const void *optval,
		     SceNetSocklen_t optlen)
{
	return libnet_setsockopt(s, level, optname, optval, optlen);
}

int sceNetGetsockopt(SceNetId s, int level, int optname, void *optval, SceNetSocklen_t *optlen)
{
	return libnet_getsockopt(s, level, optname, optval, optlen);
}

int sceNetGetsockname(SceNetId s, SceNetSockaddrArg name, SceNetSocklen_t *namelen)
{
	return libnet_getsockname(s, name.sockaddr, namelen);
}

int sceNetGetpeername(SceNetId s, SceNetSockaddrArg name, SceNetSocklen_t *namelen)
{
	return libnet_getpeername(s, name.sockaddr, namelen);
}

int SceNetInternal_333F9CB3(SceNetId s, unsigned int command, void *data)
{
	return libnet_ioctl(s, command, data);
}

int sceNetSocketClose(SceNetId s)
{
	return libnet_socket_close(s);
}

SceNetId sceNetEpollCreate(const char *name, int flags)
{
	return libnet_epoll_create(name, flags);
}

int sceNetEpollControl(SceNetId eid, int op, SceNetId id, SceNetEpollEvent *event)
{
	return libnet_epoll_control(eid, op, id, event);
}

int sceNetEpollWait(SceNetId eid, SceNetEpollEvent *events, int maxevents, int timeout)
{
	return libnet_epoll_wait(eid, events, maxevents, timeout, 0);
}

int sceNetEpollWaitCB(SceNetId eid, SceNetEpollEvent *events, int maxevents, int timeout)
{
	return libnet_epoll_wait(eid, events, maxevents, timeout, 1);
}

int sceNetEpollDestroy(SceNetId eid)
{
	return libnet_epoll_destroy(eid);
}

int sceNetEpollAbort(SceNetId eid, int flags)
{
	return libnet_epoll_abort(eid, flags);
}

SceNetId sceNetDumpCreate(const char *name, int len, int flags)
{
	return libnet_dump_create(name, len, flags);
}

int sceNetDumpRead(SceNetId id, void *buf, int len, int *pflags)
{
	return libnet_dump_read(id, buf, len, pflags);
}

int sceNetDumpDestroy(SceNetId id)
{
	return libnet_dump_destroy(id);
}

int sceNetDumpAbort(SceNetId id, int flags)
{
	return libnet_dump_abort(id, flags);
}

int SceNetInternal_E2F6CAEA(const char *name, int flags)
{
	return libnet_descriptor_create(name, flags);
}

int SceNetInternal_0F83B7A9(int id, int op, int s, void *info)
{
	return libnet_descriptor_control(id, op, s, info);
}

int SceNetInternal_873DE778(int id)
{
	return libnet_descriptor_destroy(id);
}

int SceNetInternal_FE8C89AE(const int *name, unsigned int namelen, void *oldp,
			    unsigned int *oldlenp, const void *newp, unsigned int newlen)
{
	return libnet_sysctl(name, namelen, oldp, oldlenp, newp, newlen);
}

int *sceNetErrnoLoc(void)
{
	return libnet_errno_loc();
}
