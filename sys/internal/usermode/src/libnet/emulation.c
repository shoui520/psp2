/*
 * emulation.c: the network emulation parameters of an interface, by its index or, for -1,
 * by the SDK's flags (SCE_NET_EMULATION_FLAG_ETH0 or _WLAN0).
 */
#include <libkernel/libkernel.h>
#include "libnet_internal.h"

/* the kernel index of the interface FLAGS names: -2 when they name none or both */
static int libnet_emulation_if_index(int flags)
{
	if (flags & SCE_NET_EMULATION_FLAG_ETH0) {
		if (!(flags & SCE_NET_EMULATION_FLAG_WLAN0))
			return libnet_if_kernel_index(1);
	} else if (flags & SCE_NET_EMULATION_FLAG_WLAN0) {
		return libnet_if_kernel_index(4);
	}
	return -2;
}

int libnet_emulation_set(SceNetEmulationParam *param, int ifIndex, int flags)
{
	if (param == NULL || (flags & ~3))
		return SCE_NET_ERROR_EINVAL;
	if (ifIndex == -1) {
		ifIndex = libnet_emulation_if_index(flags);
		if (ifIndex == -2)
			return SCE_NET_ERROR_EINVAL;
		if (ifIndex == -1)
			return SCE_NET_ERROR_ENODEV;
	}
	return libnet_control(ifIndex, SCE_NETPS_CONTROL_SET_EMULATION, param, sizeof(*param));
}

int libnet_emulation_get(SceNetEmulationParam *param, int ifIndex, int flags)
{
	if (param == NULL || (flags & ~3))
		return SCE_NET_ERROR_EINVAL;
	if (ifIndex == -1) {
		ifIndex = libnet_emulation_if_index(flags);
		if (ifIndex == -2)
			return SCE_NET_ERROR_EINVAL;
		if (ifIndex == -1)
			return SCE_NET_ERROR_ENODEV;
	}
	sceClibMemset(param, 0, sizeof(*param));
	return libnet_control(ifIndex, SCE_NETPS_CONTROL_GET_EMULATION, param, sizeof(*param));
}
