/*
 * dns.c: the DNS servers. Servers an application sets are kept here and override the
 * kernel's configuration; the kernel's are set with libnet_set_kernel_dns_info.
 */
#include <libkernel/libkernel.h>
#include "libnet_internal.h"

/* the kernel's DNS configuration flag: its servers changed, the resolver's cache is stale */
#define LIBNET_DNS_CONFIG_CHANGED	0x1

static SceNetDnsInfo libnet_dns_info;

int libnet_dns_init(void)
{
	return 0;
}

int libnet_dns_term(void)
{
	sceClibMemset(&libnet_dns_info, 0, sizeof(libnet_dns_info));
	return 0;
}

int libnet_get_dns_info(SceNetDnsInfo *info, int flags)
{
	SceNetPsDnsConfig config;
	int ret;

	if (info == NULL)
		return SCE_NET_ERROR_EINVAL;
	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	ret = libnet_control(-1, SCE_NETPS_CONTROL_GET_DNS_CONFIG, &config, sizeof(config));
	if (ret < 0)
		return ret;
	if (config.flags & LIBNET_DNS_CONFIG_CHANGED)
		libnet_clear_dns_cache(flags);
	if (libnet_dns_info.dns_addr[0].s_addr != 0)
		sceClibMemcpy_safe(info, &libnet_dns_info, sizeof(*info));
	else
		*info = config.dns;
	if (info->dns_addr[0].s_addr == 0)
		return 0;
	return info->dns_addr[1].s_addr != 0 ? 2 : 1;
}

int libnet_set_dns_info(const SceNetDnsInfo *info, int flags)
{
	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	if (info == NULL) {
		sceClibMemset(&libnet_dns_info, 0, sizeof(libnet_dns_info));
		return 0;
	}
	if (info->dns_addr[0].s_addr == 0)
		return SCE_NET_ERROR_EINVAL;
	sceClibMemcpy_safe(&libnet_dns_info, info, sizeof(libnet_dns_info));
	return 0;
}

int libnet_set_kernel_dns_info(const SceNetDnsInfo *info, int flags)
{
	SceNetPsDnsConfig config;

	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	if (info == NULL)
		return SCE_NET_ERROR_EINVAL;
	sceClibMemset(&config, 0, sizeof(config));
	config.dns = *info;
	return libnet_control(-1, SCE_NETPS_CONTROL_SET_DNS_CONFIG, &config, sizeof(config));
}
