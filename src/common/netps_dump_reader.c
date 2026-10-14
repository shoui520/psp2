/* Shared Sony capture-ring reader; original source filename unavailable. */

#include "../os0/kd/bootimage/net_ps/source/net_ps_internal.h"
#include <string.h>

int sceNetPsDumpRead(int dumpId, void *buffer, int length, int *flags)
{
	SceNetPsDump *dump;
	char *destination = buffer;
	int userFlags;
	int inputFlags;
	int first;
	int position;
	int result;
	int error;

	sce_psnet_bnet_mutex_lock(&gNetPsJumboMutex, NULL);
	if (sceNetPsConsumeWakeup((const void *)(uintptr_t)dumpId, 1)) {
		error = 4;
		goto failed;
	}
	if (length < 0) {
		error = 22;
		goto failed;
	}
	dump = sceNetPsLookupDump(dumpId, 0);
	if (!dump) {
		error = 9;
		goto failed;
	}
	if (flags) {
		if (sceNetPsCheckedCopyUserIn(flags, &userFlags, sizeof(userFlags)))
			goto badAddress;
	} else {
		memset(&userFlags, 0, sizeof(userFlags));
	}
	inputFlags = userFlags;
	if (inputFlags & ~0x70) {
		error = 22;
		goto failed;
	}
	if (!(inputFlags & SCE_NET_DUMP_DONTWAIT) && dump->bufferedLength <= 0) {
		++dump->waiters;
		error = sceNetPsSleep(dump, 0, NULL, -1);
		if (error) {
			if (error < 0)
				error = -error;
			goto failed;
		}
		--dump->waiters;
	}
	userFlags = dump->flags & SCE_NET_DUMP_OVERFLOW;
	if (flags && sceNetPsCheckedCopyUserOut(&userFlags, flags, sizeof(userFlags)))
		goto badAddress;
	if (length >= dump->bufferedLength)
		length = dump->bufferedLength;
	first = dump->length - dump->readPosition;
	if (length < first)
		first = length;
	if (first > 0) {
		if (destination && sceNetPsCheckedCopyUserOut(
			    dump->buffer + dump->readPosition, destination, first))
			goto badAddress;
		if (!(inputFlags & SCE_NET_DUMP_PEEK)) {
			dump->readPosition += first;
			dump->bufferedLength -= first;
		}
		length -= first;
		if (length > 0) {
			if (destination && destination + first &&
			    sceNetPsCheckedCopyUserOut(dump->buffer,
						      destination + first, length))
				goto badAddress;
			if (!(inputFlags & SCE_NET_DUMP_PEEK)) {
				dump->readPosition = length;
				dump->bufferedLength -= length;
			}
		}
		position = dump->readPosition;
		if (position >= dump->length)
			dump->readPosition = position - dump->length;
	}
	result = length + first;
	if (result > 0 && !(inputFlags & SCE_NET_DUMP_PEEK))
		dump->flags &= ~SCE_NET_DUMP_OVERFLOW;
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return result;

badAddress:
	error = 14;
failed:
	sce_psnet_bnet_mutex_unlock(&gNetPsJumboMutex);
	return -error;
}
