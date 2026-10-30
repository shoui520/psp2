/*
 * The DBGP network group: the stack's statistics, a process's sockets, an
 * interface, the resolver and the network emulation settings, each asked of
 * SceNetPs through its debugger client (Sony's group_net.c).
 */

#include <kernel/error.h>
#include <net/dns.h>
#include <net/errno.h>
#include <net_ps/netps.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include "dbgp_internal.h"

/* SceNetPs's own errors the debugger reports */
#define NET_PS_ERROR_NOT_ATTACHED	(int)0x8041810d
#define NET_PS_ERROR_BUSY		(int)0x8041810e
#define NET_PS_ERROR_TIMEOUT		(int)0x8041810f

/* the commands of SceNetPsDebugClient.control */
#define NET_PS_DEBUG_GET_EMULATION	1
#define NET_PS_DEBUG_SET_EMULATION	2
#define NET_PS_DEBUG_GET_STATISTICS	3
#define NET_PS_DEBUG_GET_INTERFACE	4
#define NET_PS_DEBUG_GET_SOCKETS	5
#define NET_PS_DEBUG_GET_DNS		6

/* the protocol's interface record: the first 260 bytes of SceNetPs's */
#define DBGP_NET_INTERFACE_INFO_SIZE	260

typedef struct DbgpNetSockRequest {
	DbgpPrefix prefix;
	SceUID pid;
} DbgpNetSockRequest;

typedef struct DbgpNetInterfaceRequest {
	DbgpPrefix prefix;
	SceUInt32 ifnum;
} DbgpNetInterfaceRequest;

typedef struct DbgpNetEmulationQuery {
	DbgpPrefix prefix;
	SceNetPsDebugEmulationSelector query;
} DbgpNetEmulationQuery;

typedef struct DbgpNetEmulationSetRequest {
	DbgpPrefix prefix;
	SceNetPsDebugEmulationRequest request;
} DbgpNetEmulationSetRequest;

typedef struct DbgpNetSystemInfoResponse {
	DbgpResponseHeader header;
	SceNetPsKernelStatistics statistics;
} DbgpNetSystemInfoResponse;

typedef struct DbgpNetSockInfoResponse {
	DbgpResponseHeader header;
	SceNetPsDebugSockInfo info;
} DbgpNetSockInfoResponse;

typedef struct DbgpNetInterfaceInfoResponse {
	DbgpResponseHeader header;
	int interfaceType;
	SceSize infoSize;		/* 0x20 */
	SceUInt8 info[DBGP_NET_INTERFACE_INFO_SIZE];
} DbgpNetInterfaceInfoResponse;

typedef struct DbgpNetResolverInfoResponse {
	DbgpResponseHeader header;
	SceNetDnsInfo dns;
} DbgpNetResolverInfoResponse;

typedef struct DbgpNetEmulationResponse {
	DbgpResponseHeader header;
	SceNetPsDebugEmulationInfo info;
} DbgpNetEmulationResponse;

_Static_assert(sizeof(DbgpNetSockRequest) == 24, "DBGP socket request");
_Static_assert(sizeof(DbgpNetEmulationQuery) == 28, "DBGP emulation query");
_Static_assert(sizeof(DbgpNetEmulationSetRequest) == 0x220,
	       "DBGP emulation setting");
_Static_assert(sizeof(DbgpNetSystemInfoResponse) == 0x2c, "DBGP net statistics");
_Static_assert(sizeof(DbgpNetSockInfoResponse) == 40, "DBGP socket list");
_Static_assert(sizeof(DbgpNetInterfaceInfoResponse) == 0x128, "DBGP interface");
_Static_assert(sizeof(DbgpNetResolverInfoResponse) == 0x24, "DBGP resolver");

static inline int _changeNetErrorToDbgpError(int error)
{
	switch (error) {
	case SCE_NET_ERROR_ENOENT:
	case SCE_NET_ERROR_EINVAL:
	case SCE_NET_ERROR_ENOSPC:
	case NET_PS_ERROR_NOT_ATTACHED:
		return 7;
	case NET_PS_ERROR_BUSY:
		return 0x200d1;
	case NET_PS_ERROR_TIMEOUT:
		return 4;
	default:
		SCE_PRINTF_WITH_INFO(15, 990, "0x%08x\n", error);
		return 0x200d0;
	}
}

int netGetNetSystemInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	DbgpNetSystemInfoResponse *response;
	SceNetPsKernelStatistics *work;
	SceNetPsDebugClient client;
	int result, ret, error;

	if (_dbgpGetRecvSize(proc) < sizeof(*packet)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	client.control = NULL;
	client.controlUnlocked = NULL;
	client.flags = 0;
	if (dbgpIsApplicationActive() == 0)
		client.flags = 2;
	result = SceSysrootForKernel_6C036B53(&client);
	if (result >= 0) {
		ret = client.control(&client, NET_PS_DEBUG_GET_STATISTICS, NULL, 0,
				     work, DBGP_WORK_BUF_SIZE);
		result = SceSysrootForKernel_A9510EFE(&client);
	}
	if (result < 0) {
		error = _changeNetErrorToDbgpError(result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      error);
		return (int)0x80080001;
	}
	if (ret < 0) {
		error = _changeNetErrorToDbgpError(ret);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      error);
		return (int)0x80080001;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	response->statistics = *work;
	dbgpFillResponseHeader(proc, packet->group, 1, 0, 0);
	return 0;
}

int netGetNetSockInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpNetSockRequest *request = (const DbgpNetSockRequest *)packet;
	DbgpNetSockInfoResponse *response;
	SceNetPsDebugClient client;
	SceUID pid, input;
	SceSize size;
	SceUInt32 n;
	void *work;
	int result, ret, error;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	if (checkInvalidProcess(pid) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      0x20011);
		return (int)0x80080001;
	}
	client.control = NULL;
	client.controlUnlocked = NULL;
	client.flags = 0;
	if (dbgpIsApplicationActive() == 0)
		client.flags = 2;
	result = SceSysrootForKernel_6C036B53(&client);
	if (result >= 0) {
		input = pid;
		ret = client.control(&client, NET_PS_DEBUG_GET_SOCKETS, &input,
				     sizeof(input), work, DBGP_WORK_BUF_SIZE);
		result = SceSysrootForKernel_A9510EFE(&client);
	}
	if (result < 0) {
		error = _changeNetErrorToDbgpError(result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      error);
		return (int)0x80080001;
	}
	if (ret < 0) {
		error = _changeNetErrorToDbgpError(ret);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      error);
		return (int)0x80080001;
	}
	n = (SceUInt32)(ret - sizeof(SceNetPsDebugSockInfo)) / sizeof(SceNetSockInfo);
	size = sizeof(*response) + n * sizeof(SceNetSockInfo);
	response = _dbgpAllocSendBuf(proc, size);
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, size);
	_dbgpSetSendSize(proc, size);
	memcpy(&response->info, work,
	       sizeof(SceNetPsDebugSockInfo) + n * sizeof(SceNetSockInfo));
	if (n == 0)
		response->info.recordSize = 0;
	dbgpFillResponseHeader(proc, packet->group, 3, 0, 0);
	return 0;
}

int netGetNetInterfaceInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpNetInterfaceRequest *request =
		(const DbgpNetInterfaceRequest *)packet;
	DbgpNetInterfaceInfoResponse *response;
	SceNetPsDebugClient client;
	SceUInt32 input;
	void *work;
	int result, ret, error;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	if (checkInvalidNetInterface(request->ifnum) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 7);
		return (int)0x80080001;
	}
	client.control = NULL;
	client.controlUnlocked = NULL;
	client.flags = 0;
	if (dbgpIsApplicationActive() == 0)
		client.flags = 2;
	result = SceSysrootForKernel_6C036B53(&client);
	if (result >= 0) {
		input = request->ifnum;
		ret = client.control(&client, NET_PS_DEBUG_GET_INTERFACE, &input,
				     sizeof(input), work, DBGP_WORK_BUF_SIZE);
		result = SceSysrootForKernel_A9510EFE(&client);
	}
	if (result < 0) {
		error = _changeNetErrorToDbgpError(result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      error);
		return (int)0x80080001;
	}
	if (ret < 0) {
		error = _changeNetErrorToDbgpError(ret);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      error);
		return (int)0x80080001;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	memcpy(&response->interfaceType, work,
	       sizeof(*response) - offsetof(DbgpNetInterfaceInfoResponse,
					    interfaceType));
	response->infoSize = DBGP_NET_INTERFACE_INFO_SIZE;
	dbgpFillResponseHeader(proc, packet->group, 5, 0, 0);
	return 0;
}

int netGetNetResolverInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	DbgpNetResolverInfoResponse *response;
	SceNetPsDebugClient client;
	SceNetDnsInfo *work;
	int result, ret, error;

	if (_dbgpGetRecvSize(proc) < sizeof(*packet)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	client.control = NULL;
	client.controlUnlocked = NULL;
	client.flags = 0;
	if (dbgpIsApplicationActive() == 0)
		client.flags = 2;
	result = SceSysrootForKernel_6C036B53(&client);
	if (result >= 0) {
		ret = client.control(&client, NET_PS_DEBUG_GET_DNS, NULL, 0, work,
				     DBGP_WORK_BUF_SIZE);
		result = SceSysrootForKernel_A9510EFE(&client);
	}
	if (result < 0) {
		error = _changeNetErrorToDbgpError(result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      error);
		return (int)0x80080001;
	}
	if (ret < 0) {
		error = _changeNetErrorToDbgpError(ret);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      error);
		return (int)0x80080001;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	response->dns = *work;
	dbgpFillResponseHeader(proc, packet->group, 7, 0, 0);
	return 0;
}

int netGetNetEmulationSettingCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpNetEmulationQuery *request = (const DbgpNetEmulationQuery *)packet;
	DbgpNetEmulationResponse *response;
	SceNetPsDebugClient client;
	SceUInt32 input[3];
	void *work;
	int result, ret, error;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	work = _dbgpAllocWorkBuf(proc, DBGP_WORK_BUF_SIZE);
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	if (checkInvalidNetEmulationQuery(request->query) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 7);
		return (int)0x80080001;
	}
	client.control = NULL;
	client.controlUnlocked = NULL;
	client.flags = 0;
	if (dbgpIsApplicationActive() == 0)
		client.flags = 2;
	result = SceSysrootForKernel_6C036B53(&client);
	if (result >= 0) {
		input[0] = request->query.interfaceType;
		input[1] = request->query.optionNumber;
		input[2] = 0;
		ret = client.control(&client, NET_PS_DEBUG_GET_EMULATION, input,
				     sizeof(input), work, DBGP_WORK_BUF_SIZE);
		result = SceSysrootForKernel_A9510EFE(&client);
	}
	if (result < 0) {
		error = _changeNetErrorToDbgpError(result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      error);
		return (int)0x80080001;
	}
	if (ret < 0) {
		error = _changeNetErrorToDbgpError(ret);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      error);
		return (int)0x80080001;
	}
	response = _dbgpAllocSendBuf(proc, ret + sizeof(DbgpResponseHeader));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, ret + sizeof(DbgpResponseHeader));
	_dbgpSetSendSize(proc, ret + sizeof(DbgpResponseHeader));
	memcpy(&response->info, work, ret);
	dbgpFillResponseHeader(proc, packet->group, 9, 0, 0);
	return 0;
}

int netSetNetEmulationSettingCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpNetEmulationSetRequest *request =
		(const DbgpNetEmulationSetRequest *)packet;
	SceNetPsDebugEmulationRequest *work;
	DbgpResponseHeader *response;
	SceNetPsDebugClient client;
	char *clear;
	int result, ret, error, i;

	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	work = _dbgpAllocWorkBuf(proc, sizeof(*work));
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	if (checkInvalidNetEmulationRequest(request->request) != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 7);
		return (int)0x80080001;
	}
	client.control = NULL;
	client.controlUnlocked = NULL;
	client.flags = 0;
	if (dbgpIsApplicationActive() == 0)
		client.flags = 2;
	result = SceSysrootForKernel_6C036B53(&client);
	if (result >= 0) {
		memset(work, 0, sizeof(*work));
		memcpy(work, &request->request, sizeof(*work));
		/* the debugger's reserved fields never reach the stack */
		clear = (char *)&work->parameters.reserved1;
		for (i = 0; i < sizeof(work->parameters.reserved1); i++)
			clear[i] = 0;
		memset(work->parameters.send.reserved, 0,
		       sizeof(work->parameters.send.reserved));
		memset(work->parameters.recv.reserved, 0,
		       sizeof(work->parameters.recv.reserved));
		memset(work->parameters.reserved, 0,
		       sizeof(work->parameters.reserved));
		ret = client.control(&client, NET_PS_DEBUG_SET_EMULATION, work,
				     sizeof(*work), NULL, 0);
		result = SceSysrootForKernel_A9510EFE(&client);
	}
	if (result < 0) {
		error = _changeNetErrorToDbgpError(result);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      error);
		return (int)0x80080001;
	}
	if (ret < 0) {
		error = _changeNetErrorToDbgpError(ret);
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code,
			      error);
		return (int)0x80080001;
	}
	response = _dbgpAllocSendBuf(proc, sizeof(*response));
	if (response == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(response, 0, sizeof(*response));
	_dbgpSetSendSize(proc, sizeof(*response));
	dbgpFillResponseHeader(proc, packet->group, 0xb, 0, 0);
	return 0;
}
