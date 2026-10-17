#include "netps_socket.h"
#include "netps_resource.h"
#include "netps_process.h"

SceUID sceNetPsGetProcessId(void);

void sceNetPsSetSocket(unsigned int socketId, struct socket *socket)
{
	SceNetPsResourceCounts *counts;

	if (socketId >= 1024)
		return;
	gNetPsResources.sockets[socketId] = socket;
	if (socket) {
		socket->processId = sceNetPsGetProcessId();
	} else {
		counts = sceNetPsGetProcessResourceCounts();
		if (counts)
			counts->counts[0]--;
	}
}

struct socket *sceNetPsLookupSocket(unsigned int socketId, int *error,
				  unsigned int flags)
{
	struct socket *socket;
	unsigned int word, bit;

	if (socketId >= 1024 || !(socket = gNetPsResources.sockets[socketId]))
		goto invalid;
	word = socketId >> 5;
	bit = socketId & 31;
	if ((gNetPsResources.addressChanged[word] >> bit) & 1) {
		*error = 163;
		return NULL;
	}
	if (!((gNetPsResources.resumeRequired[word] >> bit) & 1)) {
		SceUID processId;

		if (flags & 1)
			return socket;
		processId = socket->processId;
		if (processId == sceNetPsGetProcessId())
			return socket;
		goto invalid;
	}
	*error = 162;
	return NULL;
invalid:
	*error = 9;
	return NULL;
}

