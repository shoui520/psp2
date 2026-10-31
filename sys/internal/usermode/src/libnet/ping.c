/*
 * ping.c: find an IPv4 neighbour's link address. While no route has it, one ICMP echo request
 * to the address makes the stack resolve it, and the routes are read again.
 */
#include <libkernel/libkernel.h>
#include <net/header/icmp.h>
#include "libnet_internal.h"

/* the raw socket option the echo request needs (net_ps's SO_SCE_OPTION_20000) */
#define LIBNET_SO_OPTION_20000		0x20000

/* the echo request: its header and one byte of data */
#define LIBNET_PING_SIZE		9

/* bits of libnet_ping's flags; bit 0 is set once the link address is found */
#define LIBNET_PING_FOUND		0x1
#define LIBNET_PING_NO_REPLY		0x2	/* send only, wait for no reply */
#define LIBNET_PING_NO_OPTION		0x4	/* leave the socket option alone */

int libnet_ping(LibnetPingParam *param, int flags)
{
	LibnetRouteEntry *entries, *entry;
	SceNetIcmpHeader *icmp;
	SceNetSockaddrIn to;
	SceUShort16 *word;
	unsigned int sum, id, sequence;
	int retried, state, ret, count, n, s, error, on, timeout;

	retried = flags & ~7;
	if (retried != 0)
		return SCE_NET_ERROR_EINVAL;
	for (;;) {
		count = libnet_get_route_table(-1, SCE_NET_AF_INET, NULL, 0, 0);
		ret = count;
		if (count <= 0) {
			ret = SCE_NET_ERROR_ENOENT;
			state = flags;
			entries = NULL;
		} else {
			entries = libnet_malloc(count * sizeof(*entries), 4);
			if (entries != NULL) {
				n = libnet_get_route_table(-1, SCE_NET_AF_INET, entries, count, 0);
				if (n <= 0)
					goto not_found;
				entry = entries;
				while (entry->destination.in.sin_addr.s_addr != param->address.s_addr) {
					n--;
					entry++;
					if (n == 0)
						goto not_found;
				}
				if (entry->gateway.dl.sdl_alen != 0) {
					state = flags | LIBNET_PING_FOUND;
					sceClibMemcpy_safe(&param->ether, &entry->gateway.dl.sdl_data[0],
							   sizeof(param->ether));
				} else {
not_found:
					ret = SCE_NET_ERROR_ENOENT;
					state = flags;
				}
			} else {
				ret = SCE_NET_ERROR_ENOLIBMEM;
				state = flags;
			}
		}
		if ((state & LIBNET_PING_FOUND) || retried)
			break;

		libnet_unlock();
		on = 1;
		s = libnet_socket("SceNetPing", SCE_NET_AF_INET, SCE_NET_SOCK_RAW, SCE_NET_IPPROTO_ICMP);
		if (s < 0) {
			libnet_free(NULL);
			libnet_lock();
			ret = SCE_NET_ERROR_ENOENT;
			break;
		}
		icmp = NULL;
		if ((state & LIBNET_PING_NO_OPTION) ||
		    (error = libnet_setsockopt(s, SCE_NET_SOL_SOCKET, LIBNET_SO_OPTION_20000, &on,
					       sizeof(on))) >= 0) {
			icmp = libnet_malloc(LIBNET_PING_SIZE, 6);
			if (icmp != NULL) {
				sceClibMemset(&to, 0, sizeof(to));
				to.sin_family = SCE_NET_AF_INET;
				to.sin_addr = param->address;
				icmp->type = SCE_NET_ICMP_TYPE_ECHO_REQUEST;
				icmp->code = 0;
				id = libnet_random();
				icmp->un.echo.id = (SceUShort16)(id << 8) | (SceUChar8)(id >> 8);
				sequence = libnet_random();
				icmp->un.echo.sequence = (sequence << 8) | (SceUChar8)(sequence >> 8);
				sum = 0;
				word = (SceUShort16 *)icmp;
				do
					sum += *word++;
				while (word != (SceUShort16 *)icmp + 4);
				sum += ((SceUChar8 *)icmp)[8];
				sum = (sum & 0xffff) + (sum >> 16);
				icmp->checksum = ~(sum + (sum >> 16));
				error = libnet_sendto(s, icmp, LIBNET_PING_SIZE, 0,
						      (SceNetSockaddr *)&to, sizeof(to));
				if (error >= 0) {
					if (state & LIBNET_PING_NO_REPLY) {
						error = 0;
					} else {
						timeout = 3000000;
						error = libnet_setsockopt(s, SCE_NET_SOL_SOCKET,
									  SCE_NET_SO_RCVTIMEO, &timeout,
									  sizeof(timeout));
						if (error >= 0) {
							error = libnet_recv(s, icmp, LIBNET_PING_SIZE,
									    state & LIBNET_PING_NO_REPLY);
							if (error > 0)
								error = 0;
						}
					}
				}
			} else {
				error = SCE_NET_ERROR_ENOLIBMEM;
			}
		}
		libnet_free(icmp);
		libnet_socket_close(s);
		libnet_lock();
		if (error != 0 || (state & LIBNET_PING_NO_REPLY)) {
			ret = SCE_NET_ERROR_ENOENT;
			break;
		}
		retried = 1;
		libnet_free(entries);
	}
	libnet_free(entries);
	return ret;
}
