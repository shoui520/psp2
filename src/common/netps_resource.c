/* Shared SceNetPs resource accounting and identifier allocation. */

#include <sce/netps_process.h>
#include <sce/netps_resource.h>
#include <stddef.h>

int sceNetPsIsApplicationProgram(int *error);
int sceNetPsIsGameBudget(void);
SceUID sceNetPsGetProcessId(void);

int gNetPsOtherDumpCursor = 3;
int gNetPsOtherSocketCursor = 895;
int gNetPsGameEpollCursor = 127;
int gNetPsOtherEpollCursor = 127;
int gNetPsGameDescriptorCursor = 127;
int gNetPsOtherDescriptorCursor = 127;
int gNetPsGameDumpCursor = 11;
int gNetPsGameSocketCursor = 127;

SceNetPsResourceTable gNetPsResources;

/* Name unknown; account for a socket and enforce the application limit. */
int sceNetPsAcquireSocket(int checkLimit)
{
	SceNetPsResourceCounts *resources = sceNetPsGetProcessResourceCounts();
	int count;

	if (!resources)
		return 0;
	if (checkLimit) {
		resources->flags |= 1;
		if (sceNetPsIsApplicationProgram(NULL) && resources->counts[0] > 31)
			return -1;
	}
	count = ++resources->counts[0];
	if (count > resources->peakCounts[0])
		resources->peakCounts[0] = count;
	return 0;
}

/* Name unknown; account for an epoll and enforce the application limit. */
int sceNetPsAcquireEpoll(int checkLimit)
{
	SceNetPsResourceCounts *resources = sceNetPsGetProcessResourceCounts();
	int count;

	if (!resources)
		return 0;
	if (checkLimit) {
		resources->flags |= 1;
		if (sceNetPsIsApplicationProgram(NULL) && resources->counts[1] > 15)
			return -1;
	}
	count = ++resources->counts[1];
	if (count > resources->peakCounts[1])
		resources->peakCounts[1] = count;
	return 0;
}

/* Name unknown; account for a descriptor and enforce the application limit. */
int sceNetPsAcquireDescriptor(int checkLimit)
{
	SceNetPsResourceCounts *resources = sceNetPsGetProcessResourceCounts();
	int count;

	if (!resources)
		return 0;
	if (checkLimit) {
		resources->flags |= 1;
		if (sceNetPsIsApplicationProgram(NULL) && resources->counts[2] > 15)
			return -1;
	}
	count = ++resources->counts[2];
	if (count > resources->peakCounts[2])
		resources->peakCounts[2] = count;
	return 0;
}

/* Name unknown; report the inclusive socket identifier range. */
void sceNetPsGetSocketIdRange(int *first, int *last)
{
	*first = 0;
	*last = 1023;
}

/* Name unknown; report the inclusive epoll identifier range. */
void sceNetPsGetEpollIdRange(int *first, int *last)
{
	*first = 2000;
	*last = 2255;
}

/* Name unknown; report the inclusive descriptor identifier range. */
void sceNetPsGetDescriptorIdRange(int *first, int *last)
{
	*first = 4000;
	*last = 4255;
}

/* Name unknown; report the inclusive packet-dump identifier range. */
void sceNetPsGetDumpIdRange(int *first, int *last)
{
	*first = 6000;
	*last = 6015;
}

/* Name unknown; find a free socket identifier in the caller's budget pool. */
int sceNetPsAllocateSocketId(void)
{
	int candidate;
	int remaining;
	unsigned int word;
	unsigned int shift;

	if (!sceNetPsIsGameBudget()) {
		candidate = gNetPsOtherSocketCursor;
		remaining = 896;
		do {
			candidate = (candidate + 1) % 896;
			word = (unsigned int)candidate >> 5;
			shift = candidate & 31;
			if (!((gNetPsResources.resumeRequired[word] >> shift) & 1) &&
			    !((gNetPsResources.addressChanged[word] >> shift) & 1) &&
			    !gNetPsResources.sockets[candidate + 128]) {
				gNetPsOtherSocketCursor = candidate;
				if (sceNetPsAcquireSocket(1) < 0)
					return -1;
				return gNetPsOtherSocketCursor + 128;
			}
		} while (--remaining);
		gNetPsOtherSocketCursor = candidate;
		return -1;
	}
	candidate = gNetPsGameSocketCursor;
	remaining = 128;
	do {
		candidate = (candidate + 1) % 128;
		word = (unsigned int)candidate >> 5;
		shift = candidate & 31;
		if (!((gNetPsResources.resumeRequired[word] >> shift) & 1) &&
		    !((gNetPsResources.addressChanged[word] >> shift) & 1) &&
		    !gNetPsResources.sockets[candidate]) {
			gNetPsGameSocketCursor = candidate;
			sceNetPsAcquireSocket(0);
			return gNetPsGameSocketCursor;
		}
	} while (--remaining);
	gNetPsGameSocketCursor = candidate;
	return -1;
}

/* Name unknown; find a free epoll identifier in the caller's budget pool. */
int sceNetPsAllocateEpollId(void)
{
	int candidate;
	int remaining;

	if (!sceNetPsIsGameBudget()) {
		candidate = gNetPsOtherEpollCursor;
		remaining = 128;
		do {
			candidate = (candidate + 1) % 128;
			if (!gNetPsResources.epolls[candidate + 128]) {
				gNetPsOtherEpollCursor = candidate;
				if (sceNetPsAcquireEpoll(1) < 0)
					return -1;
				return gNetPsOtherEpollCursor + 2128;
			}
		} while (--remaining);
		gNetPsOtherEpollCursor = candidate;
		return -1;
	}
	candidate = gNetPsGameEpollCursor;
	remaining = 128;
	do {
		candidate = (candidate + 1) % 128;
		if (!gNetPsResources.epolls[candidate]) {
			gNetPsGameEpollCursor = candidate;
			sceNetPsAcquireEpoll(0);
			return gNetPsGameEpollCursor + 2000;
		}
	} while (--remaining);
	gNetPsGameEpollCursor = candidate;
	return -1;
}

/* Name unknown; find a free descriptor in the caller's budget pool. */
int sceNetPsAllocateDescriptorId(void)
{
	int candidate;
	int remaining;

	if (!sceNetPsIsGameBudget()) {
		candidate = gNetPsOtherDescriptorCursor;
		remaining = 128;
		do {
			candidate = (candidate + 1) % 128;
			if (!gNetPsResources.descriptors[candidate + 128]) {
				gNetPsOtherDescriptorCursor = candidate;
				if (sceNetPsAcquireDescriptor(1) < 0)
					return -1;
				return gNetPsOtherDescriptorCursor + 4128;
			}
		} while (--remaining);
		gNetPsOtherDescriptorCursor = candidate;
		return -1;
	}
	candidate = gNetPsGameDescriptorCursor;
	remaining = 128;
	do {
		candidate = (candidate + 1) % 128;
		if (!gNetPsResources.descriptors[candidate]) {
			gNetPsGameDescriptorCursor = candidate;
			sceNetPsAcquireDescriptor(0);
			return gNetPsGameDescriptorCursor + 4000;
		}
	} while (--remaining);
	gNetPsGameDescriptorCursor = candidate;
	return -1;
}

/* Name unknown; find a free packet dump in the caller's budget pool. */
int sceNetPsAllocateDumpId(void)
{
	int candidate;
	int remaining;

	if (sceNetPsIsGameBudget()) {
		candidate = gNetPsGameDumpCursor;
		remaining = 12;
		do {
			candidate = (candidate + 1) % 12;
			if (!gNetPsResources.dumps[candidate]) {
				gNetPsGameDumpCursor = candidate;
				return candidate + 6000;
			}
		} while (--remaining);
		gNetPsGameDumpCursor = candidate;
		return -1;
	}
	candidate = gNetPsOtherDumpCursor;
	remaining = 4;
	do {
		candidate = (candidate + 1) % 4;
		if (!gNetPsResources.dumps[candidate + 12]) {
			gNetPsOtherDumpCursor = candidate;
			return candidate + 6012;
		}
	} while (--remaining);
	gNetPsOtherDumpCursor = candidate;
	return -1;
}

/* Name unknown; set or clear the socket's resume-required state. */
void sceNetPsSetSocketResume(unsigned int socketId, int enabled)
{
	unsigned int word;

	if (socketId >= 1024)
		return;
	word = socketId >> 5;
	socketId &= 31;
	if (!enabled)
		gNetPsResources.resumeRequired[word] &= ~(1U << socketId);
	else
		gNetPsResources.resumeRequired[word] |= 1U << socketId;
}

/* Name unknown; set or clear the socket's changed-address state. */
void sceNetPsSetSocketAddressChanged(unsigned int socketId, int enabled)
{
	unsigned int word;

	if (socketId >= 1024)
		return;
	word = socketId >> 5;
	socketId &= 31;
	if (!enabled)
		gNetPsResources.addressChanged[word] &= ~(1U << socketId);
	else
		gNetPsResources.addressChanged[word] |= 1U << socketId;
}

/* Name unknown; install an epoll or release its process resource count. */
void sceNetPsSetEpoll(unsigned int epollId, SceNetPsEpoll *epoll)
{
	SceNetPsResourceCounts *resources;

	if (epollId - 2000 > 255)
		return;
	gNetPsResources.epolls[epollId - 2000] = epoll;
	if (epoll) {
		epoll->processId = sceNetPsGetProcessId();
	} else {
		resources = sceNetPsGetProcessResourceCounts();
		if (resources)
			--resources->counts[1];
	}
}

/* Name unknown; install a descriptor or release its process resource count. */
void sceNetPsSetDescriptor(unsigned int descriptorId,
			   SceNetPsDescriptor *descriptor)
{
	SceNetPsResourceCounts *resources;

	if (descriptorId - 4000 > 255)
		return;
	gNetPsResources.descriptors[descriptorId - 4000] = descriptor;
	if (descriptor) {
		descriptor->processId = sceNetPsGetProcessId();
	} else {
		resources = sceNetPsGetProcessResourceCounts();
		if (resources)
			--resources->counts[2];
	}
}

/* Name unknown; find an epoll, optionally bypassing process ownership. */
SceNetPsEpoll *sceNetPsLookupEpoll(unsigned int epollId, unsigned int flags)
{
	SceNetPsEpoll *epoll;

	if (epollId - 2000 > 255)
		return NULL;
	epoll = gNetPsResources.epolls[epollId - 2000];
	if (!epoll)
		return NULL;
	if ((flags & 1) || epoll->processId == sceNetPsGetProcessId())
		return epoll;
	return NULL;
}

/* Name unknown; find a descriptor, optionally bypassing process ownership. */
SceNetPsDescriptor *sceNetPsLookupDescriptor(unsigned int descriptorId,
					   unsigned int flags)
{
	SceNetPsDescriptor *descriptor;

	if (descriptorId - 4000 > 255)
		return NULL;
	descriptor = gNetPsResources.descriptors[descriptorId - 4000];
	if (!descriptor)
		return NULL;
	if ((flags & 1) || descriptor->processId == sceNetPsGetProcessId())
		return descriptor;
	return NULL;
}

/* Name unknown; locate a socket pointer without inspecting its object. */
int sceNetPsFindSocketId(const struct socket *socket)
{
	int socketId;

	for (socketId = 0; socketId < 1024; ++socketId) {
		if (gNetPsResources.sockets[socketId] == socket)
			return socketId;
	}
	return -1;
}
