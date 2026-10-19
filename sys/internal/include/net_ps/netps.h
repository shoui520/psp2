/* Complete argument records copied by SceNetPsForSyscalls. */
#ifndef SCE_NETPS_H
#define SCE_NETPS_H

#include <net.h>
#include <scetypes.h>
#include <stddef.h>

/* Shared allocator adapters; implementations retain each module's heap. */
void *sceNetPsMalloc(SceSize size, char flags, SceSize align);
int sceNetPsFree(void *ptr);
void *sceNetPsCalloc(SceSize size);
void sceNetPsRelease(void *ptr);

/* Debug commands wrap the SDK's complete emulation parameter record. */
typedef struct SceNetPsDebugEmulationSelector {
	int interfaceType;
	unsigned int optionNumber;
} SceNetPsDebugEmulationSelector;

typedef struct SceNetPsDebugEmulationInfo {
	int interfaceType;
	unsigned int parameterSize;
	SceNetEmulationParam parameters;
} SceNetPsDebugEmulationInfo;

typedef struct SceNetPsDebugEmulationRequest {
	int interfaceType;
	unsigned int optionNumber;
	unsigned int parameterSize;
	SceNetEmulationParam parameters;
} SceNetPsDebugEmulationRequest;

_Static_assert(sizeof(SceNetPsDebugEmulationSelector) == 8,
	       "debug emulation selector size");
_Static_assert(offsetof(SceNetPsDebugEmulationSelector, interfaceType) == 0,
	       "debug selector interface type");
_Static_assert(offsetof(SceNetPsDebugEmulationSelector, optionNumber) == 4,
	       "debug selector option number");
_Static_assert(sizeof(SceNetPsDebugEmulationInfo) == 520,
	       "debug emulation info size");
_Static_assert(offsetof(SceNetPsDebugEmulationInfo, interfaceType) == 0,
	       "debug info interface type");
_Static_assert(offsetof(SceNetPsDebugEmulationInfo, parameterSize) == 4,
	       "debug info parameter size");
_Static_assert(offsetof(SceNetPsDebugEmulationInfo, parameters) == 8,
	       "debug info parameters");
_Static_assert(sizeof(SceNetPsDebugEmulationRequest) == 524,
	       "debug emulation request size");
_Static_assert(offsetof(SceNetPsDebugEmulationRequest, interfaceType) == 0,
	       "debug request interface type");
_Static_assert(offsetof(SceNetPsDebugEmulationRequest, optionNumber) == 4,
	       "debug request option number");
_Static_assert(offsetof(SceNetPsDebugEmulationRequest, parameterSize) == 8,
	       "debug request parameter size");
_Static_assert(offsetof(SceNetPsDebugEmulationRequest, parameters) == 12,
	       "debug request parameters");

/* The connection-control getter reports all five words of this record. */
typedef struct SceNetPsConnectionParam {
	int lastLinkStatus;
	int autoConnectEnabled;
	int idleTimeoutSeconds;
	int autoConnectPolicy;
	int loopbackRequested;
} SceNetPsConnectionParam;

_Static_assert(sizeof(SceNetPsConnectionParam) == 20,
	       "connection parameter size");
_Static_assert(offsetof(SceNetPsConnectionParam, lastLinkStatus) == 0,
	       "connection last link status");
_Static_assert(offsetof(SceNetPsConnectionParam, autoConnectEnabled) == 4,
	       "connection auto connect enabled");
_Static_assert(offsetof(SceNetPsConnectionParam, idleTimeoutSeconds) == 8,
	       "connection idle timeout");
_Static_assert(offsetof(SceNetPsConnectionParam, autoConnectPolicy) == 12,
	       "connection auto connect policy");
_Static_assert(offsetof(SceNetPsConnectionParam, loopbackRequested) == 16,
	       "connection loopback requested");

/* The setter requires references to be zero; the getter reports its count. */
typedef struct SceNetPsPromiscuousParam {
	int enabled;
	int references;
} SceNetPsPromiscuousParam;

_Static_assert(sizeof(SceNetPsPromiscuousParam) == 8,
	       "promiscuous parameter size");
_Static_assert(offsetof(SceNetPsPromiscuousParam, enabled) == 0,
	       "promiscuous enabled");
_Static_assert(offsetof(SceNetPsPromiscuousParam, references) == 4,
	       "promiscuous references");

/* Reviewed names: the SDK declares only opaque SceNetSyscallParameter. */
typedef struct SceNetSyscallSendtoParam {
	int socket;
	const void *buffer;
	unsigned int length;
	int flags;
	const SceNetSockaddr *address;
	unsigned int addressLength;
} SceNetSyscallSendtoParam;

typedef struct SceNetSyscallRecvfromParam {
	int socket;
	void *buffer;
	unsigned int length;
	int flags;
	SceNetSockaddr *address;
	unsigned int *addressLength;
} SceNetSyscallRecvfromParam;

typedef struct SceNetSyscallSysctlParam {
	const int *name;
	unsigned int nameLength;
	void *oldData;
	unsigned int *oldLength;
	const void *newData;
	unsigned int newLength;
} SceNetSyscallSysctlParam;

_Static_assert(sizeof(SceNetSyscallSendtoParam) == 24, "sendto argument size");
_Static_assert(offsetof(SceNetSyscallSendtoParam, socket) == 0, "sendto socket");
_Static_assert(offsetof(SceNetSyscallSendtoParam, buffer) == 4, "sendto buffer");
_Static_assert(offsetof(SceNetSyscallSendtoParam, length) == 8, "sendto length");
_Static_assert(offsetof(SceNetSyscallSendtoParam, flags) == 12, "sendto flags");
_Static_assert(offsetof(SceNetSyscallSendtoParam, address) == 16, "sendto address");
_Static_assert(offsetof(SceNetSyscallSendtoParam, addressLength) == 20,
	       "sendto address length");
_Static_assert(sizeof(SceNetSyscallRecvfromParam) == 24, "recvfrom argument size");
_Static_assert(offsetof(SceNetSyscallRecvfromParam, socket) == 0, "recvfrom socket");
_Static_assert(offsetof(SceNetSyscallRecvfromParam, buffer) == 4, "recvfrom buffer");
_Static_assert(offsetof(SceNetSyscallRecvfromParam, length) == 8, "recvfrom length");
_Static_assert(offsetof(SceNetSyscallRecvfromParam, flags) == 12, "recvfrom flags");
_Static_assert(offsetof(SceNetSyscallRecvfromParam, address) == 16,
	       "recvfrom address");
_Static_assert(offsetof(SceNetSyscallRecvfromParam, addressLength) == 20,
	       "recvfrom address length");
_Static_assert(sizeof(SceNetSyscallSysctlParam) == 24, "sysctl argument size");
_Static_assert(offsetof(SceNetSyscallSysctlParam, name) == 0, "sysctl name");
_Static_assert(offsetof(SceNetSyscallSysctlParam, nameLength) == 4,
	       "sysctl name length");
_Static_assert(offsetof(SceNetSyscallSysctlParam, oldData) == 8, "sysctl old data");
_Static_assert(offsetof(SceNetSyscallSysctlParam, oldLength) == 12,
	       "sysctl old length");
_Static_assert(offsetof(SceNetSyscallSysctlParam, newData) == 16,
	       "sysctl new data");
_Static_assert(offsetof(SceNetSyscallSysctlParam, newLength) == 20,
	       "sysctl new length");

#endif

/* net_ps_glue.c */
int SceNetPsForDriver_AB746734(int socket, const void *buffer, unsigned int length, int flags,
		  const SceNetSockaddr *to, unsigned int toLength);
int SceNetPsForDriver_4BF5FAB4(int socket, int level, int option, const void *value,
		     unsigned int length);
int SceNetPsForDriver_EB95B024(const char *name, int domain, int type, int protocol);
int SceNetPsForDriver_21F4428D(int socket);
