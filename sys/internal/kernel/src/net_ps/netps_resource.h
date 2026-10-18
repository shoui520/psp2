#ifndef SCE_NETPS_RESOURCE_H
#define SCE_NETPS_RESOURCE_H

#include <scetypes.h>

struct socket;
struct SceNetPsEpoll;
struct SceNetPsDescriptor;
struct SceNetPsDump;

struct SceNetPsEpollEntry;

/* Sony's epoll allocation is 44 bytes; the list contains opaque entries. */
typedef struct SceNetPsEpoll {
	struct SceNetPsEpollEntry *entries;
	char name[32];
	SceUID processId;
	unsigned int flags;
} SceNetPsEpoll;

struct SceNetPsDescriptorEntry;

typedef struct SceNetPsDescriptor {
	struct SceNetPsDescriptorEntry *entries;
	char name[32];
	SceUID processId;
} SceNetPsDescriptor;

_Static_assert(sizeof(SceNetPsEpoll) == 44, "SceNetPsEpoll size");
_Static_assert(sizeof(SceNetPsDescriptor) == 40, "SceNetPsDescriptor size");

typedef struct SceNetPsResourceTable {
	struct socket *sockets[1024];
	int resumeRequired[32];
	int addressChanged[32];
	struct SceNetPsEpoll *epolls[256];
	struct SceNetPsDescriptor *descriptors[256];
	struct SceNetPsDump *dumps[16];
} SceNetPsResourceTable;

_Static_assert(sizeof(SceNetPsResourceTable) == 6464,
	       "SceNetPsResourceTable size");

extern SceNetPsResourceTable gNetPsResources;
extern int gNetPsOtherDumpCursor;
extern int gNetPsOtherSocketCursor;
extern int gNetPsGameEpollCursor;
extern int gNetPsOtherEpollCursor;
extern int gNetPsGameDescriptorCursor;
extern int gNetPsOtherDescriptorCursor;
extern int gNetPsGameDumpCursor;
extern int gNetPsGameSocketCursor;

int sceNetPsAcquireSocket(int checkLimit);
int sceNetPsAcquireEpoll(int checkLimit);
int sceNetPsAcquireDescriptor(int checkLimit);
void sceNetPsGetSocketIdRange(int *first, int *last);
void sceNetPsGetEpollIdRange(int *first, int *last);
void sceNetPsGetDescriptorIdRange(int *first, int *last);
void sceNetPsGetDumpIdRange(int *first, int *last);
int sceNetPsAllocateSocketId(void);
int sceNetPsAllocateEpollId(void);
int sceNetPsAllocateDescriptorId(void);
int sceNetPsAllocateDumpId(void);

void sceNetPsSetSocketResume(unsigned int socketId, int enabled);
void sceNetPsSetSocketAddressChanged(unsigned int socketId, int enabled);
void sceNetPsSetEpoll(unsigned int epollId, SceNetPsEpoll *epoll);
void sceNetPsSetDescriptor(unsigned int descriptorId,
			   SceNetPsDescriptor *descriptor);
SceNetPsEpoll *sceNetPsLookupEpoll(unsigned int epollId, unsigned int flags);
SceNetPsDescriptor *sceNetPsLookupDescriptor(unsigned int descriptorId,
					   unsigned int flags);
int sceNetPsFindSocketId(const struct socket *socket);

#endif
