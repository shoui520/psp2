/*
 * interface.c: the network interfaces by libnet's own index (its order of the stack's names),
 * their records, counters and kernel indices.
 */
#include <libkernel/libkernel.h>
#include "libnet_internal.h"

static const char *const libnet_if_names[LIBNET_IF_COUNT] = {
	"lo0", "eth0", "eth1", "eth2", "wlan0", "wlan1", "wlan2", "bt0", "phone0", "pppoe0",
};

const char *libnet_if_name(unsigned int ifIndex)
{
	if (ifIndex < LIBNET_IF_COUNT)
		return libnet_if_names[ifIndex];
	*libnet_errno_loc() = SCE_NET_EINVAL;
	return "";
}

/* the record of interface IFINDEX (1 when it exists, else 0), or with -1 every record */
int libnet_get_if_info(unsigned int ifIndex, SceNetPsInterfaceInfo *info, int count)
{
	SceNetPsInterfaceInfo *list, *entry;
	int copy, ret, i;

	if (ifIndex == (unsigned int)-1) {
		ret = libnet_get_if_list(info, count);
		libnet_free(NULL);
		return ret;
	}
	copy = info != NULL;
	if (!(info == NULL && count == 0) && (count != 1 || info == NULL)) {
		ret = SCE_NET_ERROR_EINVAL;
		list = NULL;
		goto out;
	}
	list = libnet_malloc(LIBNET_IF_COUNT * sizeof(*list), 6);
	if (list == NULL) {
		ret = SCE_NET_ERROR_ENOLIBMEM;
		goto out;
	}
	ret = libnet_get_if_list(list, LIBNET_IF_COUNT);
	if (ret <= 0)
		goto out;
	entry = list;
	i = 0;
	for (;;) {
		if (ifIndex <= LIBNET_IF_COUNT - 1) {
			i++;
			if (sceClibStrncmp(libnet_if_names[ifIndex], entry->name,
					   sizeof(entry->name)) == 0)
				break;
		} else {
			*libnet_errno_loc() = SCE_NET_EINVAL;
			i++;
			if (sceClibStrncmp("", entry->name, sizeof(entry->name)) == 0)
				break;
		}
		entry++;
		if (i == ret) {
			libnet_free(list);
			return 0;
		}
	}
	ret = 1;
	if (copy)
		sceClibMemcpy_safe(info, entry, sizeof(*info));
out:
	libnet_free(list);
	return ret;
}

int libnet_if_kernel_index(int ifIndex)
{
	SceNetPsInterfaceInfo info;
	int ret;

	if (ifIndex == -1)
		return SCE_NET_ERROR_EINVAL;
	ret = libnet_get_if_info(ifIndex, &info, 1);
	if (ret < 0)
		return ret;
	if (ret != 1)
		return SCE_NET_ERROR_ENODATA;
	return info.interfaceIndex;
}

/* the counters since the last call; a new generation of the interface starts again from 0 */
int libnet_get_if_statistics(int ifIndex, LibnetIfStatistics *statistics, int count,
			     LibnetIfStatisticsBaseline *baseline)
{
	SceNetPsInterfaceInfo *info;
	LibnetIfStatisticsBaseline *last;
	int ret, i;

	if (statistics == NULL || count != 1 || ifIndex == -1) {
		ret = SCE_NET_ERROR_EINVAL;
		info = NULL;
		goto out;
	}
	info = libnet_malloc(sizeof(*info), 6);
	if (info == NULL) {
		ret = SCE_NET_ERROR_ENOLIBMEM;
		goto out;
	}
	ret = libnet_get_if_info(ifIndex, info, 1);
	if (ret < 0)
		goto out;
	last = &baseline[ifIndex];
	if (info->generation != last->generation) {
		sceClibMemset(last, 0, sizeof(*last));
		last->generation = info->generation;
	}
	ret = 0;
	for (i = 0; i < 8; i++)
		statistics->txCounters[i] = info->txCounters[i] - last->last.txCounters[i];
	for (i = 0; i < 8; i++)
		statistics->rxCounters[i] = info->rxCounters[i] - last->last.rxCounters[i];
	statistics->uptimeSeconds = info->uptimeSeconds - last->last.uptimeSeconds;
	for (i = 0; i < 8; i++)
		last->last.txCounters[i] = info->txCounters[i];
	for (i = 0; i < 8; i++)
		last->last.rxCounters[i] = info->rxCounters[i];
	last->last.uptimeSeconds = info->uptimeSeconds;
out:
	libnet_free(info);
	return ret;
}

/* libnet's indices of the interfaces the stack has, lo0 and pppoe0 only with FLAGS bit 0 */
int libnet_get_if_indices(int *indices, int count, int flags)
{
	SceNetPsInterfaceInfo *list, *entry;
	int ret, n, i, ifIndex, first, end;

	if (indices == NULL || count <= 0) {
		libnet_free(NULL);
		return SCE_NET_ERROR_EINVAL;
	}
	list = libnet_malloc(LIBNET_IF_COUNT * sizeof(*list), 6);
	if (list == NULL) {
		ret = SCE_NET_ERROR_ENOLIBMEM;
		goto out;
	}
	n = libnet_get_if_list(list, LIBNET_IF_COUNT);
	if (n <= 0) {
		ret = n;
		goto out;
	}
	if (flags & 1) {
		end = LIBNET_IF_COUNT;
		first = 0;
	} else {
		end = LIBNET_IF_COUNT - 1;
		first = 1;
	}
	ret = 0;
	for (entry = list, i = 0; i != n; i++, entry++) {
		ifIndex = first;
		do {
			if (sceClibStrncmp(libnet_if_names[ifIndex], entry->name,
					   sizeof(entry->name)) == 0) {
				indices[ret++] = ifIndex;
				if (ret >= count) {
					ret = count;
					goto out;
				}
				break;
			}
		} while (++ifIndex < end);
	}
out:
	libnet_free(list);
	return ret;
}

/* Sony's strnlen: a string longer than MAXLEN counts MAXLEN + 1 */
SceSize libnet_strnlen(const char *str, SceSize maxlen)
{
	SceSize length;

	if (str == NULL || maxlen == (SceSize)-1)
		return 0;
	if (*str == '\0')
		return 0;
	length = 0;
	do {
		if (++length == maxlen + 1)
			break;
	} while (*++str != '\0');
	return length;
}

/* Sony's callers call it, not the thread-local lookup it wraps */
__attribute__((noinline)) int *libnet_errno_loc(void)
{
	return libnet_tls_errno();
}
