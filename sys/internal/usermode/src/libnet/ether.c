/*
 * ether.c: Ethernet addresses as text ("xx:xx:xx:xx:xx:xx"), and this console's own.
 */
#include <libkernel/libkernel.h>
#include "libnet_internal.h"

/* a hexadecimal digit's value, or 9999999 for any other character */
static inline int libnet_hex_digit(signed char c)
{
	if ((unsigned char)c - '0' <= 9u)
		return c - '0';
	if ((unsigned char)c - 'A' <= 25u)
		return c - 'A' + 10;
	if ((unsigned char)c - 'a' <= 25u)
		return c - 'a' + 10;
	return 9999999;
}

int libnet_ether_strton(const char *str, SceNetEtherAddr *n)
{
	SceUChar8 *to;
	const char *end;
	char digits[3];
	unsigned char first, second;
	char high, low;

	if (str == NULL || n == NULL)
		return SCE_NET_ERROR_EINVAL;
	to = &n->data[0] - 1;
	end = str + 18;
	do {
		sceClibMemcpy_safe(digits, str, 2);
		first = str[0];
		digits[2] = '\0';
		if ((unsigned int)(first - 48) <= 9)
			high = 16 * (first - 48);
		else if ((unsigned int)(first - 65) > 25) {
			if ((unsigned int)(first - 97) > 25)
				high = -16;
			else
				high = 16 * (first - 87);
		} else {
			high = 16 * (first - 55);
		}
		second = str[1];
		if ((unsigned int)(second - 48) > 9) {
			if ((unsigned int)(second - 65) > 25) {
				if ((unsigned int)(second - 97) > 25)
					low = 127;
				else
					low = second - 87;
			} else {
				low = second - 55;
			}
		} else {
			low = second - 48;
		}
		str += 3;
		*++to = low + high;
	} while (str != end);
	return 0;
}

int libnet_ether_ntostr(const SceNetEtherAddr *n, char *str, SceSize len)
{
	if (n == NULL || str == NULL || len < 18)
		return SCE_NET_ERROR_EINVAL;
	sceClibSnprintf(str, len, "%02x:%02x:%02x:%02x:%02x:%02x", n->data[0], n->data[1],
			n->data[2], n->data[3], n->data[4], n->data[5]);
	return 0;
}

int libnet_get_mac_address(SceNetEtherAddr *addr, int flags)
{
	SceNetPsInterfaceInfo info;
	int ret;

	if (addr == NULL)
		return SCE_NET_ERROR_EINVAL;
	if (flags != 0)
		return SCE_NET_ERROR_EINVAL;
	ret = libnet_get_if_info(4, &info, 1);
	if (ret < 0)
		return ret;
	sceClibMemcpy_safe(addr, &info.hardwareAddress, sizeof(*addr));
	return flags;
}

int libnet_get_if_mac_address(SceNetEtherAddr *addr, int flags)
{
	SceNetPsInterfaceInfo info;
	int ifIndex, ret;

	if (addr == NULL)
		return SCE_NET_ERROR_EINVAL;
	if ((flags & ~3) || flags == 0)
		return SCE_NET_ERROR_EINVAL;
	if (flags & SCE_NET_EMULATION_FLAG_ETH0) {
		if (flags & SCE_NET_EMULATION_FLAG_WLAN0)
			return SCE_NET_ERROR_EINVAL;
		ifIndex = 1;
	} else if (flags & SCE_NET_EMULATION_FLAG_WLAN0) {
		ifIndex = 4;
	} else {
		return SCE_NET_ERROR_EINVAL;
	}
	ret = libnet_get_if_info(ifIndex, &info, 1);
	if (ret < 0)
		return ret;
	sceClibMemcpy_safe(addr, &info.hardwareAddress, sizeof(*addr));
	return 0;
}
