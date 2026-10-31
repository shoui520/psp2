/*
 * SceDeci4pSDbgp (os0:kd/deci4p_sdbgp.skprx): SDBGP_GROUP_MISC (0x70)
 * network command handlers (group_net.c).
 */

#include <string.h>
#include <scetypes.h>
#include <kernel/error.h>
#include <net/dns.h>
#include <net_ps/netps.h>
#include <processmgr/processmgr.h>
#include <sysmem/sysroot.h>
#include "sdbgp_internal.h"

#define NET_PS_DEBUG_GET_STATISTICS	3
#define NET_PS_DEBUG_GET_INTERFACE	4
#define NET_PS_DEBUG_GET_SOCKETS	5
#define NET_PS_DEBUG_GET_DNS		6

#define SDBGP_NET_INTERFACE_INFO_SIZE	260

typedef struct SdbgpNetSockRequest {
	SdbgpRequest header;
	SceUID pid;				/* 0x14 */
} SdbgpNetSockRequest;

typedef struct SdbgpNetInterfaceRequest {
	SdbgpRequest header;
	uint32_t ifnum;				/* 0x14 */
} SdbgpNetInterfaceRequest;

typedef struct SdbgpNetSystemInfoReply {
	SdbgpHeader header;
	SceNetPsKernelStatistics statistics;	/* 0x1c */
} SdbgpNetSystemInfoReply;

typedef struct SdbgpNetInterfaceInfoReply {
	SdbgpHeader header;
	int interfaceType;			/* 0x1c */
	SceSize infoSize;			/* 0x20 */
	uint8_t info[SDBGP_NET_INTERFACE_INFO_SIZE];
} SdbgpNetInterfaceInfoReply;

typedef struct SdbgpNetResolverInfoReply {
	SdbgpHeader header;
	SceNetDnsInfo dns;			/* 0x1c */
} SdbgpNetResolverInfoReply;

typedef struct SdbgpNetSockInfoReply {
	SdbgpHeader header;
	SceNetPsDebugSockInfo info;		/* 0x1c */
} SdbgpNetSockInfoReply;

int sdbgpCmd70_00(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	SdbgpNetSystemInfoReply *reply;
	void *work;
	SceNetPsDebugClient client;
	int result;
	int ret;

	if (sdbgpGetRecvSize(cpu) <= 0x13) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	if (!work)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(&client, 0, sizeof(client));
	client.flags = 1;
	result = SceSysrootForKernel_CCB8BB5E(&client);
	if (result < 0 ||
	    (ret = client.controlUnlocked(&client, NET_PS_DEBUG_GET_STATISTICS, NULL, 0,
					  work, SDBGP_WORK_BUF_SIZE),
	     (result = SceSysrootForKernel_40F28DC6(&client)) < 0)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       result);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	reply = sdbgpAllocSendBuf(cpu, sizeof(*reply));
	if (!reply)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sizeof(*reply));
	sdbgpSetSendSize(cpu, sizeof(*reply));
	SceDeci4pSDfMgrForKernel_5996E2DB(&reply->statistics, work,
					  sizeof(reply->statistics));
	sdbgpSendErrorHeader(cpu, packet->group, 1, 0, 0);
	return 0;
}

int sdbgpCmd70_04(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpNetInterfaceRequest *req = (const SdbgpNetInterfaceRequest *)packet;
	SdbgpNetInterfaceInfoReply *reply;
	const void *work;
	SceNetPsDebugClient client;
	uint32_t input;
	int result;
	int ret;

	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	ret = isIllegalKernelApiId(req->ifnum);
	if (ret != 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_ILLEGAL_VALUE);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	if (!work)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	client.control = NULL;
	client.controlUnlocked = NULL;
	client.flags = 1;
	result = SceSysrootForKernel_CCB8BB5E(&client);
	if (result < 0 ||
	    (input = req->ifnum,
	     ret = client.controlUnlocked(&client, NET_PS_DEBUG_GET_INTERFACE, &input,
					  sizeof(input), (void *)work,
					  SDBGP_WORK_BUF_SIZE),
	     (result = SceSysrootForKernel_40F28DC6(&client)) < 0)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       result);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	reply = sdbgpAllocSendBuf(cpu, sizeof(*reply));
	if (!reply)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(reply, 0, sizeof(*reply));
	sdbgpSetSendSize(cpu, sizeof(*reply));
	memcpy(&reply->interfaceType, work,
	       sizeof(*reply) - offsetof(SdbgpNetInterfaceInfoReply, interfaceType));
	sdbgpSendErrorHeader(cpu, packet->group, 5, 0, 0);
	return 0;
}

int sdbgpCmd70_06(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	SdbgpNetResolverInfoReply *reply;
	void *work;
	SceNetPsDebugClient client;
	int result;
	int ret;

	if (sdbgpGetRecvSize(cpu) <= 0x13) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	if (!work)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	client.control = NULL;
	client.controlUnlocked = NULL;
	client.flags = 1;
	result = SceSysrootForKernel_CCB8BB5E(&client);
	if (result < 0 ||
	    (ret = client.controlUnlocked(&client, NET_PS_DEBUG_GET_DNS, NULL, 0, work,
					  SDBGP_WORK_BUF_SIZE),
	     (result = SceSysrootForKernel_40F28DC6(&client)) < 0)) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       result);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	if (ret < 0) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, ret);
		return SDBGP_ERROR_INVALID_ARGUMENT;
	}
	reply = sdbgpAllocSendBuf(cpu, sizeof(*reply));
	if (!reply)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, sizeof(*reply));
	sdbgpSetSendSize(cpu, sizeof(*reply));
	SceDeci4pSDfMgrForKernel_5996E2DB(&reply->dns, work, sizeof(reply->dns));
	sdbgpSendErrorHeader(cpu, packet->group, 7, 0, 0);
	return 0;
}

static void sdbgpCopyNetSockInfo(SdbgpNetSockInfoReply *reply, const void *src,
				 SceSize size)
{
	SceSize left = size - sizeof(SceNetPsDebugSockInfo);
	uint8_t *dst = (uint8_t *)reply->info.sockets;
	const SceNetPsDebugSockInfo *cur = src;

	while (left != 0) {
		SceSize chunk = cur->count * sizeof(SceNetSockInfo);
		SceDeci4pSDfMgrForKernel_5996E2DB(dst, cur->sockets, chunk);
		dst += chunk;
		left -= chunk;
		cur = (const SceNetPsDebugSockInfo *)((const uint8_t *)cur + chunk);
	}
}

int sdbgpCmd70_02(SdbgpCpuState *cpu, SdbgpHeader *packet)
{
	const SdbgpNetSockRequest *req = (const SdbgpNetSockRequest *)packet;
	SdbgpNetSockInfoReply *reply;
	SceUID *work;
	void *sockWork;
	SceNetPsDebugSockInfo *curWork;
	SceNetPsDebugClient client;
	SceUID pid;
	SceUID input;
	SceSize count;
	SceSize totalBytes;
	SceSize remSize;
	SceSize numSockets;
	SceSize payloadBytes;
	SceSize replySize;
	SceSize copySize;
	int allPids;
	int i;
	int ret;
	int result;

	count = 0;
	if (sdbgpGetRecvSize(cpu) <= 0x17) {
		sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag,
			       SDBGP_RESULT_PACKET_TOO_SHORT);
		return SDBGP_ERROR_PACKET_SIZE;
	}
	pid = req->pid;
	allPids = ((unsigned int)(pid + 1) <= 1);
	work = sdbgpAllocWorkBuf(cpu, SDBGP_WORK_BUF_SIZE);
	if (!work)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	if (!allPids) {
		if (isIllegalProcessId(pid) != 0) {
			result = SDBGP_RESULT_ILLEGAL_PROCESS_ID;
			goto error;
		}
		sockWork = (uint8_t *)work + 64;
		SceProcessmgrForKernel_EFFF2975(5, work, 64, &count);
		SceDeci4pSDfMgrForKernel_8A3B06F2(&client, 0, sizeof(client));
		client.flags = 1;
		if (SceSysrootForKernel_CCB8BB5E(&client) < 0) {
			result = 0x200f0;
			goto error;
		}
		SceDeci4pSDfMgrForKernel_8A3B06F2(&input, 0, sizeof(input));
		input = pid;
		ret = client.controlUnlocked(&client, NET_PS_DEBUG_GET_SOCKETS, &input,
					     sizeof(input), sockWork,
					     SDBGP_WORK_BUF_SIZE - 64);
		result = SceSysrootForKernel_40F28DC6(&client);
		totalBytes = ret - sizeof(SceNetPsDebugSockInfo);
		if (result < 0)
			goto error;
		if (ret < 0) {
			result = 0x200f0;
			goto error;
		}
	} else {
		sockWork = (uint8_t *)work + 64;
		i = allPids &
		    ((unsigned int)SceProcessmgrForKernel_EFFF2975(5, work, 64, &count) >>
		     31);
		if (i != 0) {
			result = SDBGP_RESULT_ILLEGAL_PROCESS_ID;
			goto error;
		}
		SceDeci4pSDfMgrForKernel_8A3B06F2(&client, 0, sizeof(client));
		client.flags = 1;
		if (SceSysrootForKernel_CCB8BB5E(&client) < 0) {
			result = 0x200f0;
			goto error;
		}
		if ((int)count > 0) {
			totalBytes = 0;
			remSize = SDBGP_WORK_BUF_SIZE - 64;
			curWork = sockWork;
			for (;;) {
				SceSize chunk;

				SceDeci4pSDfMgrForKernel_8A3B06F2(&input, 0,
								  sizeof(input));
				input = work[i];
				ret = client.controlUnlocked(
					&client, NET_PS_DEBUG_GET_SOCKETS, &input,
					sizeof(input), curWork, remSize);
				if (ret < 0) {
					result = SceSysrootForKernel_40F28DC6(&client);
					if (result >= 0)
						result = 0x200f0;
					goto error;
				}
				totalBytes += ret - sizeof(SceNetPsDebugSockInfo);
				i++;
				chunk = curWork->count * sizeof(SceNetSockInfo);
				curWork = (SceNetPsDebugSockInfo *)((uint8_t *)curWork +
								    chunk);
				remSize -= chunk;
				if ((int)count <= i)
					break;
				if (remSize <= sizeof(SceNetSockInfo) - 1) {
					sockWork = curWork;
					break;
				}
			}
		} else {
			totalBytes = 0;
		}
		result = SceSysrootForKernel_40F28DC6(&client);
		if (result < 0)
			goto error;
	}
	numSockets = totalBytes / sizeof(SceNetSockInfo);
	payloadBytes = numSockets * sizeof(SceNetSockInfo);
	replySize = payloadBytes + sizeof(SdbgpNetSockInfoReply);
	copySize = payloadBytes + sizeof(SceNetPsDebugSockInfo);
	reply = sdbgpAllocSendBuf(cpu, replySize);
	if (!reply)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	SceDeci4pSDfMgrForKernel_8A3B06F2(reply, 0, replySize);
	sdbgpSetSendSize(cpu, replySize);
	if (allPids) {
		reply->info.count = numSockets;
		sdbgpCopyNetSockInfo(reply, sockWork, copySize);
	} else {
		memcpy(&reply->info, sockWork, copySize);
	}
	if (numSockets == 0)
		reply->info.recordSize = 0;
	sdbgpSendErrorHeader(cpu, packet->group, 3, 0, 0);
	return 0;

error:
	sdbgpSendError(cpu, packet->group, packet->command + 1, packet->tag, result);
	return SDBGP_ERROR_INVALID_ARGUMENT;
}
