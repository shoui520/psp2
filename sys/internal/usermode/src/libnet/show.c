/*
 * show.c: the debug listings of the interfaces (sceNetShowIfconfig), the routes
 * (sceNetShowRoute) and the sockets (sceNetShowNetstat), printed to the console and to
 * libnet's debug output.
 */
#include <libkernel/libkernel.h>
#include "libnet_internal.h"

#define LIBNET_INDENT		"   "

/* bits of SceNetPsInterfaceInfo.flags (BSD's IFF_*, and the stack's own) */
#define LIBNET_IFF_UP		0x1
#define LIBNET_IFF_BROADCAST	0x2
#define LIBNET_IFF_POINTOPOINT	0x10
#define LIBNET_IFF_RUNNING	0x40
#define LIBNET_IFF_MULTICAST	0x8000
#define LIBNET_IFF_DISABLED	0x10000

/* SceNetPsInterfaceInfo.driverType */
#define LIBNET_DRIVER_ETHERNET	1
#define LIBNET_DRIVER_WLAN	2
#define LIBNET_DRIVER_BT	3
#define LIBNET_DRIVER_PHONE	4

/* the sockets sceNetShowNetstat lists (sceNetGetSockInfo's flags) */
#define LIBNET_NETSTAT_FLAGS	0x23

/* the width of an address column of the route listing */
#define LIBNET_ROUTE_WIDTH	20

static void libnet_show_emulation(const SceNetEmulationData *data)
{
	LIBNET_PRINTF("drop:%d%%(%dms, %dms) ", data->drop_rate, data->drop_duration,
		      data->pass_duration);
	LIBNET_PRINTF("delay:%d+-%dms ", data->delay_time, data->delay_jitter);
	LIBNET_PRINTF("order:%d%%(%dms) ", data->order_rate, data->order_delay_time);
	LIBNET_PRINTF("dup:%d%%\n", data->duplication_rate);
	LIBNET_PRINTF("%s        ", LIBNET_INDENT);
	if (data->upper_size_limit != 0)
		LIBNET_PRINTF("bps:%d size:%d-%d ", data->bps_limit, data->lower_size_limit,
			      data->upper_size_limit);
	else
		LIBNET_PRINTF("bps:%d size:%d- ", data->bps_limit, data->lower_size_limit);
	LIBNET_PRINTF("pattern:%08x %08x\n", data->system_policy_pattern,
		      data->game_policy_pattern);
}

/* append TEXT to a line with LEFT bytes free, and return its length */
static int libnet_show_append(char *line, SceSize *left, const char *text)
{
	int length;

	sceClibStrncat(line, text, *left);
	length = libnet_strnlen(text, *left);
	*left -= length;
	return length;
}

static char *libnet_show_address(char *text, const SceNetInAddr *address)
{
	if (libnet_inet_ntop(SCE_NET_AF_INET, address, text, 16) == NULL) {
		sceClibStrncpy(text, "---", 16);
		text[15] = '\0';
	}
	return text;
}

/* an address of the socket listing: "*" for any */
static char *libnet_show_sock_address(char *text, const SceNetInAddr *address)
{
	if (libnet_inet_ntop(SCE_NET_AF_INET, address, text, 16) == NULL) {
		sceClibStrncpy(text, "---", 16);
		text[15] = '\0';
	}
	if (address->s_addr == 0) {
		sceClibStrncpy(text, "*", 16);
		text[15] = '\0';
	}
	return text;
}

/* a route's address, with the prefix length of MASK, as a column of the route listing */
static int libnet_show_route_address(char *line, SceSize *left, const SceNetSockaddr *sa,
				     const SceNetSockaddr *mask)
{
	const LibnetSockaddrDl *dl;
	const SceUChar8 *bytes;
	char text[32];
	unsigned int address;
	int bits, length, limit, width, i, result;

	if (sa->sa_len <= 1)
		return (int)line;
	if (mask != NULL) {
		if (sa->sa_family != SCE_NET_AF_INET)
			return (int)line;
		bytes = (const SceUChar8 *)mask + 4;
		limit = mask->sa_len - 4;
		bits = 0;
		if (limit > 0) {
			i = 0;
			while (bytes[i] == 0xff) {
				i++;
				bits += 8;
				if (i >= limit)
					goto address;
			}
			if (i >= limit)
				goto address;
			switch (bytes[i]) {
			case 0xfe:
				bits++;
				/* fall through */
			case 0xfc:
				bits++;
				/* fall through */
			case 0xf8:
				bits++;
				/* fall through */
			case 0xf0:
				bits++;
				/* fall through */
			case 0xe0:
				bits++;
				/* fall through */
			case 0xc0:
				bits++;
				/* fall through */
			case 0x80:
				bits++;
				i++;
				break;
			}
			/* a mask that is not contiguous has no column */
			for (; i < limit; i++)
				if (bytes[i] != 0)
					return (int)line;
		}
	} else if (sa->sa_family != SCE_NET_AF_INET) {
		if (sa->sa_family == LIBNET_AF_LINK) {
			dl = (const LibnetSockaddrDl *)sa;
			if (dl->sdl_alen != 0) {
				length = 0;
				for (i = 0; i < dl->sdl_alen; i++) {
					sceClibSnprintf(text, sizeof(text), "%02x%s",
							(SceUChar8)dl->sdl_data[dl->sdl_nlen + i],
							i < dl->sdl_alen - 1 ? ":" : "");
					length += libnet_show_append(line, left, text);
				}
				width = LIBNET_ROUTE_WIDTH - length;
			} else {
				sceClibSnprintf(text, sizeof(text), "link#%d", dl->sdl_index);
				width = LIBNET_ROUTE_WIDTH - libnet_show_append(line, left, text);
			}
		} else {
			width = LIBNET_ROUTE_WIDTH;
		}
		goto pad;
	} else {
		bits = 0;
	}
address:
	address = __builtin_bswap32(((const SceNetSockaddrIn *)sa)->sin_addr.s_addr);
	if (address == 0 && bits == 0) {
		result = libnet_show_append(line, left, "default");
	} else {
		sceClibSnprintf(text, sizeof(text), "%u", address >> 24);
		result = libnet_show_append(line, left, text);
		if ((unsigned int)(bits - 1) > 7) {
			sceClibSnprintf(text, sizeof(text), ".%u", (address >> 16) & 0xff);
			result += libnet_show_append(line, left, text);
			if ((unsigned int)(bits - 1) > 15) {
				sceClibSnprintf(text, sizeof(text), ".%u",
						(address >> 8) & 0xff);
				result += libnet_show_append(line, left, text);
				if ((unsigned int)(bits - 1) > 23) {
					sceClibSnprintf(text, sizeof(text), ".%u",
							address & 0xff);
					result += libnet_show_append(line, left, text);
				}
			}
		}
	}
	width = LIBNET_ROUTE_WIDTH - result;
	if (bits != 0) {
		sceClibSnprintf(text, 8, "/%d", bits);
		result = libnet_show_append(line, left, text);
		width -= result;
	}
pad:
	i = width - 1;
	if (width > 0) {
		do {
			i--;
			result = libnet_show_append(line, left, " ");
		} while (i != -1);
	}
	return result;
}

int libnet_show_ifconfig(void)
{
	SceNetPsInterfaceInfo *list, *info;
	SceNetPsInterfaceAddress *address;
	SceNetEmulationParam *emulation;
	SceNetStatisticsInfo statistics;
	SceNetDnsInfo dns;
	char ether[18];
	char text[16];
	int n, count, i, second, flags, link, time, minutes;

	n = libnet_get_if_list(NULL, 0);
	if (n < 0) {
		emulation = NULL;
		LIBNET_PRINTF("No interfaces found (%x).\n", n);
		list = NULL;
		goto done;
	}
	list = libnet_malloc(n * sizeof(*list), 2);
	if (list == NULL) {
		emulation = NULL;
		LIBNET_PRINTF("No memory.\n");
		goto done;
	}
	count = libnet_get_if_list(list, n);
	if (count < 0) {
		emulation = NULL;
		LIBNET_PRINTF("No interfaces found (%x).\n", count);
		goto done;
	}
	emulation = libnet_malloc(sizeof(*emulation), 2);
	for (i = 0, info = list; i != count; i++, info++) {
		libnet_ether_ntostr(&info->hardwareAddress, ether, sizeof(ether));
		LIBNET_PRINTF("%s (index=%d)\n", info->name, info->interfaceIndex);
		switch (info->driverType) {
		case LIBNET_DRIVER_ETHERNET:
			LIBNET_PRINTF("%s", LIBNET_INDENT);
			LIBNET_PRINTF("Type:Ethernet ");
			goto hardware;
		case LIBNET_DRIVER_WLAN:
			LIBNET_PRINTF("%s", LIBNET_INDENT);
			LIBNET_PRINTF("Type:Wlan ");
			goto hardware;
		case LIBNET_DRIVER_BT:
			LIBNET_PRINTF("%s", LIBNET_INDENT);
			LIBNET_PRINTF("Type:BT ");
			goto hardware;
		case LIBNET_DRIVER_PHONE:
			LIBNET_PRINTF("%s", LIBNET_INDENT);
			LIBNET_PRINTF("Type:Phone ");
hardware:
			LIBNET_PRINTF("HWaddr:%s", ether);
			LIBNET_PRINTF("\n");
			break;
		}
		LIBNET_PRINTF("%s", LIBNET_INDENT);
		for (second = 0, address = &info->addresses[0];; address++) {
			LIBNET_PRINTF("inet:%s", libnet_show_address(text,
				(const SceNetInAddr *)&address->address));
			LIBNET_PRINTF("/%s", libnet_show_address(text,
				(const SceNetInAddr *)&address->subnetMask));
			if (info->flags & LIBNET_IFF_POINTOPOINT)
				LIBNET_PRINTF("->%s(%s)", libnet_show_address(text,
					(const SceNetInAddr *)&address->broadcastAddress),
					info->parentName);
			if (info->flags & LIBNET_IFF_BROADCAST)
				LIBNET_PRINTF(" Bcast:%s", libnet_show_address(text,
					(const SceNetInAddr *)&address->broadcastAddressCopy));
			LIBNET_PRINTF("\n");
			if (second || info->addresses[1].address == 0)
				break;
			LIBNET_PRINTF("%s", LIBNET_INDENT);
			second = 1;
			LIBNET_PRINTF("[%s:%d] ", info->name, 1);
		}
		LIBNET_PRINTF("%s", LIBNET_INDENT);
		if (info->flags & LIBNET_IFF_UP)
			LIBNET_PRINTF("UP ");
		if (info->flags & LIBNET_IFF_RUNNING)
			LIBNET_PRINTF("RUNNING ");
		if (info->flags & LIBNET_IFF_BROADCAST)
			LIBNET_PRINTF("BROADCAST ");
		if (info->flags & LIBNET_IFF_MULTICAST)
			LIBNET_PRINTF("MULTICAST ");
		if (info->flags & LIBNET_IFF_POINTOPOINT)
			LIBNET_PRINTF("POINTOPOINT ");
		if (info->flags & LIBNET_IFF_DISABLED)
			LIBNET_PRINTF("DISABLED(%d) ", info->promiscuousReferences);
		if ((int)info->mtu > 0)
			LIBNET_PRINTF("MTU:%d(max:%d) ", info->mtu, info->driverMtu);
		LIBNET_PRINTF("\n");
		if (info->driverType == LIBNET_DRIVER_ETHERNET) {
			LIBNET_PRINTF("%s", LIBNET_INDENT);
			LIBNET_PRINTF("Link status: ");
			link = info->linkStatus;
			if (link & 1) {
				switch (link & ~4) {
				case 0x11:
					LIBNET_PRINTF("10BaseT Half Duplex");
					break;
				case 0x13:
					LIBNET_PRINTF("10BaseT Full Duplex");
					break;
				case 0x21:
					LIBNET_PRINTF("100BaseTX Half Duplex");
					break;
				case 0x23:
					LIBNET_PRINTF("100BaseTX Full Duplex");
					break;
				case 0x43:
					LIBNET_PRINTF("1000BaseT Full Duplex");
					break;
				default:
					LIBNET_PRINTF("(valid link) %08x", link);
					break;
				}
			} else {
				LIBNET_PRINTF("(no link)");
			}
			LIBNET_PRINTF("\n");
		}
		flags = info->flags;
		if (flags & (LIBNET_IFF_POINTOPOINT | 0x8))
			continue;
		LIBNET_PRINTF("%s", LIBNET_INDENT);
		LIBNET_PRINTF("RX packets:%u(%u bytes) errors:%u dropped:%u(emu %u)\n",
			      info->rxCounters[1], info->rxCounters[0], info->rxCounters[7],
			      info->rxCounters[6], info->softwareRxCounters[6]);
		LIBNET_PRINTF("%s", LIBNET_INDENT);
		LIBNET_PRINTF("   bcast:%u(%u bytes) mcast:%u(%u bytes)\n", info->rxCounters[3],
			      info->rxCounters[2], info->rxCounters[5], info->rxCounters[4]);
		LIBNET_PRINTF("%s", LIBNET_INDENT);
		LIBNET_PRINTF("TX packets:%u(%u bytes) errors:%u dropped:%u(emu %u)\n",
			      info->txCounters[1], info->txCounters[0], info->txCounters[7],
			      info->txCounters[6], info->softwareTxCounters[6]);
		LIBNET_PRINTF("%s", LIBNET_INDENT);
		LIBNET_PRINTF("   bcast:%u(%u bytes) mcast:%u(%u bytes)\n", info->txCounters[3],
			      info->txCounters[2], info->txCounters[5], info->txCounters[4]);
		LIBNET_PRINTF("%s", LIBNET_INDENT);
		time = info->uptimeSeconds;
		minutes = time / 60;
		LIBNET_PRINTF("time: %d:%02d:%02d\n", minutes / 60, minutes % 60, time % 60);
		if (info->autoConnectPolicy <= 0) {
			LIBNET_PRINTF("%s", LIBNET_INDENT);
			if (info->autoConnectPolicy != 0)
				LIBNET_PRINTF("ICM: disabled+");
			else
				LIBNET_PRINTF("ICM: disabled");
		} else {
			LIBNET_PRINTF("%s", LIBNET_INDENT);
			LIBNET_PRINTF("ICM: %d/%d prio=%2d ", info->idleSeconds,
				      info->idleTimeoutSeconds, info->autoConnectPolicy);
			switch (info->connectionState) {
			case 0:
				LIBNET_PRINTF("STOPPED");
				break;
			case 1:
				LIBNET_PRINTF("STARTING");
				break;
			case 2:
				LIBNET_PRINTF("READY");
				break;
			case 3:
				LIBNET_PRINTF("STARTED");
				break;
			case 4:
				LIBNET_PRINTF("STANDBY");
				break;
			default:
				LIBNET_PRINTF("---");
				break;
			}
		}
		LIBNET_PRINTF("\n");
		if (info->emulationSeedPresent != 1) {
			LIBNET_PRINTF("%semu: disabled\n", LIBNET_INDENT);
		} else if (emulation == NULL) {
			LIBNET_PRINTF("%semu: started (no libnet memory)\n", LIBNET_INDENT);
		} else if (libnet_emulation_get(emulation, info->interfaceIndex, 0) >= 0) {
			LIBNET_PRINTF("%semu: seed:%08x\n", LIBNET_INDENT, emulation->seed);
			LIBNET_PRINTF("%s   send ", LIBNET_INDENT);
			libnet_show_emulation(&emulation->send);
			LIBNET_PRINTF("%s   recv ", LIBNET_INDENT);
			libnet_show_emulation(&emulation->recv);
		}
	}
done:
	libnet_free(emulation);
	libnet_free(list);
	if (libnet_get_dns_info(&dns, 0) < 0) {
		LIBNET_PRINTF("No dns addresses found.\n");
	} else {
		LIBNET_PRINTF("dns addresses\n");
		LIBNET_PRINTF("%s", LIBNET_INDENT);
		LIBNET_PRINTF("%s, ", libnet_show_address(ether, &dns.dns_addr[0]));
		LIBNET_PRINTF("%s", libnet_show_address(ether, &dns.dns_addr[1]));
		LIBNET_PRINTF("\n");
	}
	if (libnet_get_statistics_info(&statistics, 0) < 0) {
		LIBNET_PRINTF("No statistics found.\n");
	} else {
		LIBNET_PRINTF("statistics\n");
		LIBNET_PRINTF("%s", LIBNET_INDENT);
		LIBNET_PRINTF("kernel: free_size=%7d/free_min=%7d\n", statistics.kernel_mem_free_size,
			      statistics.kernel_mem_free_min);
		LIBNET_PRINTF("%s", LIBNET_INDENT);
		LIBNET_PRINTF("libnet: free_size=%7d/free_min=%7d\n", statistics.libnet_mem_free_size,
			      statistics.libnet_mem_free_min);
		LIBNET_PRINTF("%s", LIBNET_INDENT);
		LIBNET_PRINTF("packet: %d, %d\n", statistics.packet_count,
			      statistics.packet_qos_count);
	}
	return 0;
}

int libnet_show_route(void)
{
	LibnetRouteEntry *entries, *entry;
	unsigned int flags;
	int count, width;
	SceSize left;
	char *line;

	count = 0;
	entries = libnet_get_route_entries(-1, &count);
	if (entries == NULL) {
		line = NULL;
		LIBNET_PRINTF("No routing table\n");
		goto out;
	}
	line = libnet_malloc(128, 0);
	if (line == NULL) {
		LIBNET_PRINTF("No routing table memory\n");
		goto out;
	}
	LIBNET_PRINTF("%-20s%-20s%-8s%s\n", "Destination", "Gateway", "Flags", "I/F");
	for (entry = entries; count > 0; count--, entry++) {
		sceClibStrncpy(line, "", 128);
		line[127] = '\0';
		left = 127;
		libnet_show_route_address(line, &left, &entry->destination.sa, &entry->netmask.sa);
		libnet_show_route_address(line, &left, &entry->gateway.sa, NULL);
		/* BSD's RTF_* letters, as netstat -r prints them */
		flags = entry->flags;
		width = 8;
		if (flags & 0x1)
			width -= libnet_show_append(line, &left, "U");
		if (flags & 0x2)
			width -= libnet_show_append(line, &left, "G");
		if (flags & 0x4)
			width -= libnet_show_append(line, &left, "H");
		if (flags & 0x8)
			width -= libnet_show_append(line, &left, "R");
		if (flags & 0x10)
			width -= libnet_show_append(line, &left, "D");
		if (flags & 0x20)
			width -= libnet_show_append(line, &left, "M");
		if (flags & 0x40)
			width -= libnet_show_append(line, &left, "D");
		if (flags & 0x80)
			width -= libnet_show_append(line, &left, "m");
		if (flags & 0x100)
			width -= libnet_show_append(line, &left, "C");
		if (flags & 0x200)
			width -= libnet_show_append(line, &left, "X");
		if (flags & 0x400)
			width -= libnet_show_append(line, &left, "L");
		if (flags & 0x800)
			width -= libnet_show_append(line, &left, "S");
		if (flags & 0x1000)
			width -= libnet_show_append(line, &left, "B");
		if (flags & 0x2000)
			width -= libnet_show_append(line, &left, "c");
		if (flags & 0x4000)
			width -= libnet_show_append(line, &left, "2");
		if (flags & 0x8000)
			width -= libnet_show_append(line, &left, "1");
		while (width-- > 0)
			libnet_show_append(line, &left, " ");
		libnet_show_append(line, &left, entry->ifName);
		LIBNET_PRINTF("%s\n", line);
	}
out:
	libnet_free(line);
	libnet_free_route_entries(entries);
	return 0;
}

/* a socket's local or remote address and port (and virtual port), in a 28-column field */
static int libnet_show_sock_port(const SceNetSockInfo *info, int local)
{
	const SceNetInAddr *address;
	unsigned int port, vport;
	char text[16];
	int length;

	if (local) {
		port = info->local_port;
		address = &info->local_adr;
		vport = info->local_vport;
	} else {
		port = info->remote_port;
		address = &info->remote_adr;
		vport = info->remote_vport;
	}
	port = (SceUShort16)(port << 8) | port >> 8;
	length = LIBNET_PRINTF(" %s", libnet_show_sock_address(text, address));
	if (port != 0)
		length += LIBNET_PRINTF(":%d", port);
	else
		length += LIBNET_PRINTF(":*");
	if (info->socket_type == SCE_NET_SOCK_DGRAM_P2P || info->socket_type == SCE_NET_SOCK_STREAM_P2P) {
		vport = (SceUShort16)(vport << 8) | vport >> 8;
		if (vport != 0)
			length += LIBNET_PRINTF(":v%d", vport);
		else
			length += LIBNET_PRINTF(":v*");
	}
	LIBNET_PRINTF("%*s", 28 - length, "");
	return 0;
}

int libnet_show_netstat(void)
{
	SceNetSockInfo *infos, *info;
	int ret, n;

	n = libnet_get_sock_info(-1, NULL, 0, LIBNET_NETSTAT_FLAGS);
	ret = n;
	if (n < 0) {
		infos = NULL;
		goto out;
	}
	if (n == 0) {
		infos = NULL;
		goto none;
	}
	infos = libnet_malloc(n * sizeof(*infos), 2);
	if (infos == NULL) {
		ret = SCE_NET_ERROR_ENOLIBMEM;
		LIBNET_PRINTF("No libnet memory\n");
		goto out;
	}
	ret = libnet_get_sock_info(-1, infos, n, LIBNET_NETSTAT_FLAGS);
	if (ret < 0)
		goto out;
	if (ret == 0) {
none:
		LIBNET_PRINTF("No socket info\n");
		goto out;
	}
	LIBNET_PRINTF("--- ID, type, [self/R/S/RE/SE/signal], ... ---\n");
	for (info = infos; ret != 0; info++) {
		ret--;
		LIBNET_PRINTF("ID:%4d ", info->s);
		switch (info->socket_type) {
		case SCE_NET_SOCK_STREAM:
			LIBNET_PRINTF("(tcp,%2d)   ", info->policy);
			break;
		case SCE_NET_SOCK_DGRAM:
			LIBNET_PRINTF("(udp,%2d)   ", info->policy);
			break;
		case SCE_NET_SOCK_RAW:
			LIBNET_PRINTF("(raw,%2d)   ", info->policy);
			break;
		case SCE_NET_SOCK_DGRAM_P2P:
			LIBNET_PRINTF("(udp2p,%2d) ", info->policy);
			break;
		case 7:
			LIBNET_PRINTF("(arp,%2d)   ", info->policy);
			break;
		case 9:
			LIBNET_PRINTF("(eth,%2d)   ", info->policy);
			break;
		case SCE_NET_SOCK_STREAM_P2P:
			LIBNET_PRINTF("(tcp2p,%2d) ", info->policy);
			break;
		case 11:
			LIBNET_PRINTF("(epoll)    ");
			break;
		default:
			LIBNET_PRINTF("(0x%02x,%2d)  ", info->socket_type, info->policy);
			break;
		}
		/* the socket's own, read, send, read-end, send-end and signal flags */
		if (info->flags & SCE_NET_SOCKINFO_F_SELF)
			LIBNET_PRINTF("*");
		else
			LIBNET_PRINTF(".");
		if (info->flags & SCE_NET_SOCKINFO_F_RECV_WAIT)
			LIBNET_PRINTF("*");
		else
			LIBNET_PRINTF(".");
		if (info->flags & SCE_NET_SOCKINFO_F_SEND_WAIT)
			LIBNET_PRINTF("*");
		else
			LIBNET_PRINTF(".");
		if (info->flags & SCE_NET_SOCKINFO_F_RECV_EWAIT)
			LIBNET_PRINTF("*");
		else
			LIBNET_PRINTF(".");
		if (info->flags & SCE_NET_SOCKINFO_F_SEND_EWAIT)
			LIBNET_PRINTF("*");
		else
			LIBNET_PRINTF(".");
		if (info->flags & SCE_NET_SOCKINFO_F_WAKEUP_SIGNAL)
			LIBNET_PRINTF("*");
		else
			LIBNET_PRINTF(".");
		LIBNET_PRINTF(" R-Q: %-6d", info->recv_queue_length);
		LIBNET_PRINTF(" S-Q: %-6d", info->send_queue_length);
		switch (info->state) {
		case SCE_NET_SOCKINFO_STATE_UNKNOWN:
			LIBNET_PRINTF(" UNKNOWN");
			break;
		case SCE_NET_SOCKINFO_STATE_CLOSED:
			LIBNET_PRINTF(" CLOSED");
			break;
		case SCE_NET_SOCKINFO_STATE_OPENED:
			LIBNET_PRINTF(" OPENED");
			break;
		case SCE_NET_SOCKINFO_STATE_LISTEN:
			LIBNET_PRINTF(" LISTEN");
			break;
		case SCE_NET_SOCKINFO_STATE_SYN_SENT:
			LIBNET_PRINTF(" SYN_SENT");
			break;
		case SCE_NET_SOCKINFO_STATE_SYN_RECEIVED:
			LIBNET_PRINTF(" SYN_RECEIVED");
			break;
		case SCE_NET_SOCKINFO_STATE_ESTABLISHED:
			LIBNET_PRINTF(" ESTABLISHED");
			break;
		case SCE_NET_SOCKINFO_STATE_FIN_WAIT_1:
			LIBNET_PRINTF(" FIN_WAIT_1");
			break;
		case SCE_NET_SOCKINFO_STATE_FIN_WAIT_2:
			LIBNET_PRINTF(" FIN_WAIT_2");
			break;
		case SCE_NET_SOCKINFO_STATE_CLOSE_WAIT:
			LIBNET_PRINTF(" CLOSE_WAIT");
			break;
		case SCE_NET_SOCKINFO_STATE_CLOSING:
			LIBNET_PRINTF(" CLOSING");
			break;
		case SCE_NET_SOCKINFO_STATE_LAST_ACK:
			LIBNET_PRINTF(" LAST_ACK");
			break;
		case SCE_NET_SOCKINFO_STATE_TIME_WAIT:
			LIBNET_PRINTF(" TIME_WAIT");
			break;
		default:
			LIBNET_PRINTF(" 0x%x", info->state);
			break;
		}
		LIBNET_PRINTF(" %s\n", info->name);
		LIBNET_PRINTF("  Local");
		libnet_show_sock_port(info, 1);
		LIBNET_PRINTF(" Remote");
		libnet_show_sock_port(info, 0);
		LIBNET_PRINTF("\n");
	}
	libnet_free(infos);
	return 0;
out:
	libnet_free(infos);
	return ret;
}
