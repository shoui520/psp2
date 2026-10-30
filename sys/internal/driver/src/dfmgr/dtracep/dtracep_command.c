/*
 * dtracep_command.c: the host's commands. Each takes a request from a
 * channel's queue, runs it on the client its token names (most through
 * dtrace_ioctl_remote), and queues the reply, whose command is the request's
 * plus one. A reply too big for one packet is kept with the client, and the
 * host fetches the rest of it with GET_BIG_DATA.
 */
#include <sys/dtrace.h>
#include <sys/dtrace_impl.h>
#include <dtrace/heap.h>
#include <dtrace/logging.h>
#include <dtrace/sce/dtrace_security.h>
#include <acmgr/acmgr.h>
#include <modulemgr/modulemgr.h>
#include <modulemgr/modulemgr_debugger.h>
#include <processmgr/process_info.h>
#include <processmgr/processmgr.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/uid_guid.h>
#include <kernel/error.h>
#include <sceerror.h>
#include "dtrace_client.h"
#include "dtrace_client_list.h"
#include "dtracep.h"
#include "dtracep_command.h"

/* dtrace.c's, which no header declares */
extern dtrace_optval_t dtrace_dof_maxsize;

/* the most a reply carries of a big reply's data */
#define DTRACEP_BIG_DATA_MAX	0xffac
#define DTRACEP_EPROBE_DATA_MAX	0xff9c
#define DTRACEP_AGG_DESC_DATA_MAX	0xff8c
#define DTRACEP_FORMAT_DATA_MAX	0xffc4
#define DTRACEP_BIG_DATA_PART_MAX	0xffd4
#define DTRACEP_MODULE_INFO_DATA_MAX	0xffd8
#define DTRACEP_DOF_DATA_MAX	0xff94
#define DTRACEP_PROBE_LIST_DATA_MAX	0xfe8c

/* a reply with nothing but its status */
typedef struct DtracepStatusReply {
	DtracepHeader header;
	int status;
} DtracepStatusReply;

typedef struct DtracepProviderReply {
	DtracepHeader header;
	int status;
	dtrace_providerdesc_t desc;
} __attribute__((packed, aligned(4))) DtracepProviderReply;

typedef struct DtracepProbeReply {
	DtracepHeader header;
	int status;
	dtrace_probedesc_t desc;
} __attribute__((packed, aligned(4))) DtracepProbeReply;

typedef struct DtracepBufSnapReply {
	DtracepHeader header;
	int status;
	int more;			/* the rest is the client's big data */
	SceSize dataSize;
	dtrace_bufdesc_t desc;
	char data[];
} __attribute__((packed, aligned(4))) DtracepBufSnapReply;

typedef struct DtracepBufSnapRequest {
	DtracepHeader header;
	dtrace_bufdesc_t desc;
} __attribute__((packed, aligned(4))) DtracepBufSnapRequest;

typedef struct DtracepEProbeRequest {
	DtracepHeader header;
	dtrace_eprobedesc_t desc;
} __attribute__((packed, aligned(4))) DtracepEProbeRequest;

typedef struct DtracepEProbeReply {
	DtracepHeader header;
	int status;
	int more;
	SceSize dataSize;
	char data[sizeof(dtrace_eprobedesc_t)];	/* at least the description's */
} DtracepEProbeReply;

typedef struct DtracepProbeArgReply {
	DtracepHeader header;
	int status;
	dtrace_argdesc_t desc;
} __attribute__((packed, aligned(4))) DtracepProbeArgReply;

typedef struct DtracepConfReply {
	DtracepHeader header;
	int status;
	dtrace_conf_t conf;
} DtracepConfReply;

typedef struct DtracepStatusCmdReply {
	DtracepHeader header;
	int status;
	dtrace_status_t dtStatus;
} __attribute__((packed, aligned(4))) DtracepStatusCmdReply;

/* GO's and STOP's: the CPU DTRACEIOC_GO or DTRACEIOC_STOP names */
typedef struct DtracepCpuReply {
	DtracepHeader header;
	int status;
	processorid_t cpu;
} DtracepCpuReply;

typedef struct DtracepAggDescReply {
	DtracepHeader header;
	int status;
	int more;
	SceSize dataSize;
	char data[sizeof(dtrace_aggdesc_t)];	/* at least the description's */
} DtracepAggDescReply;

typedef struct DtracepFormatRequest {
	DtracepHeader header;
	dtrace_fmtdesc_t desc;
} __attribute__((packed, aligned(4))) DtracepFormatRequest;

typedef struct DtracepFormatReply {
	DtracepHeader header;
	int status;
	int more;
	SceSize dataSize;
	dtrace_fmtdesc_t desc;
	char data[];
} __attribute__((packed, aligned(4))) DtracepFormatReply;

typedef struct DtracepDofGetRequest {
	DtracepHeader header;
	dof_hdr_t dof;
} DtracepDofGetRequest;

typedef struct DtracepDofGetReply {
	DtracepHeader header;
	int status;
	int more;
	SceSize dataSize;
	dof_hdr_t dof;			/* the DOF from its header on */
} __attribute__((packed, aligned(4))) DtracepDofGetReply;

/* ALL_PROBES', ALL_PROBE_MATCH's and ALL_EPROBES' */
typedef struct DtracepProbeListReply {
	DtracepHeader header;
	int status;
	int more;
	SceSize dataSize;
	union {
		char data[sizeof(dtrace_probedesc_list_t)];
		dtrace_eprobedesc_list_t eprobes;
	} __attribute__((packed, aligned(4)));
} DtracepProbeListReply;

typedef struct DtracepEProbeListRequest {
	DtracepHeader header;
	dtrace_eprobedesc_list_t list;
} __attribute__((packed, aligned(4))) DtracepEProbeListRequest;

typedef struct DtracepOpenRequest {
	DtracepHeader header;
	int flags;			/* sceDTraceClientOpen's */
	SceSize pathSize;
	char path[];
} DtracepOpenRequest;

typedef struct DtracepBigDataReply {
	DtracepHeader header;
	int status;
	int more;
	SceSize dataSize;
	char data[];
} DtracepBigDataReply;

typedef struct DtracepPutBigDataRequest {
	DtracepHeader header;
	SceSize totalSize;		/* of all the parts */
	SceSize size;			/* of this part */
	char data[];
} DtracepPutBigDataRequest;

typedef struct DtracepModuleInfoRequest {
	DtracepHeader header;
	SceUID pid;			/* SCE_KERNEL_PROCESS_ID: every process */
} DtracepModuleInfoRequest;

/* the modules' records, then their paths */
typedef struct DtracepModuleInfoReply {
	DtracepHeader header;
	SceUID pid;
	int count;
	char data[];
} DtracepModuleInfoReply;

typedef struct DtracepProcessInfoRequest {
	DtracepHeader header;
	SceUID pid;
} DtracepProcessInfoRequest;

typedef struct DtracepProcessInfoReply {
	DtracepHeader header;
	SceUID pid;
	int status;			/* the process's, 1 to 5; 0: any other */
	SceUID parentPid;
	SceUInt32 fingerprint;		/* its main module's */
	SceUInt32 cpuAffinity;
	SceUInt32 param;
	void *entryPoint;
	char name[32];
	SceSize pathLength;		/* its main module's path */
	char path[];
} DtracepProcessInfoReply;

typedef struct DtracepTestRequest {
	DtracepHeader header;
	int value;
} DtracepTestRequest;

typedef struct DtracepTestReply {
	DtracepHeader header;
	int status;
	int value;			/* the request's plus one */
} DtracepTestReply;

typedef struct DtracepTestNotification {
	DtracepHeader header;
	int value;			/* the TEST request's */
	SceSize length;
	char message[];
} DtracepTestNotification;

typedef struct DtracepEnableRequest {
	DtracepHeader header;
	int bigData;			/* the DOF is the client's big data */
	dof_hdr_t dof;
} DtracepEnableRequest;

_Static_assert(sizeof(DtracepProviderReply) == 0x80, "DtracepProviderReply size");
_Static_assert(sizeof(DtracepProbeReply) == 0x168, "DtracepProbeReply size");
_Static_assert(sizeof(DtracepBufSnapReply) == 0x54, "DtracepBufSnapReply size");
_Static_assert(sizeof(DtracepEProbeReply) == 0x64, "DtracepEProbeReply size");
_Static_assert(sizeof(DtracepProbeArgReply) == 0x130, "DtracepProbeArgReply size");
_Static_assert(sizeof(DtracepConfReply) == 0x54, "DtracepConfReply size");
_Static_assert(sizeof(DtracepStatusCmdReply) == 0x7c, "DtracepStatusCmdReply size");
_Static_assert(sizeof(DtracepCpuReply) == 0x28, "DtracepCpuReply size");
_Static_assert(sizeof(DtracepAggDescReply) == 0x74, "DtracepAggDescReply size");
_Static_assert(sizeof(DtracepFormatReply) == 0x3c, "DtracepFormatReply size");
_Static_assert(sizeof(DtracepDofGetReply) == 0x6c, "DtracepDofGetReply size");
_Static_assert(sizeof(DtracepProbeListReply) == 0x174, "DtracepProbeListReply size");
_Static_assert(sizeof(DtracepProcessInfoReply) == 0x60, "DtracepProcessInfoReply size");
_Static_assert(sizeof(DtracepEnableRequest) == 0x64, "DtracepEnableRequest size");

/* the channel's dtracepReqSend, called whole */
static __attribute__((noinline)) int reqSend(DtracepChannel *ch)
{
	return dtracepReqSend(ch);
}

static inline int dtracepSendStatusReply(DtracepChannel *ch, SceUInt8 command, int token,
					 int status)
{
	DtracepPacket *reply;

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(DtracepStatusReply), command, token);
	((DtracepStatusReply *)reply->data)->status = status;
	dtracepPushPacketQueue(ch->sendQueue, reply);
	return reqSend(ch);
}

/* the processes GET_MODULE_INFO looks at, and the most of a module's record */
#define DTRACEP_PROCESS_MAX	16
#define DTRACEP_MODULE_INFO_MAX	156

/* a module's path, after the modules' records in GET_MODULE_INFO's reply */
typedef struct DtracepModulePath {
	SceSize size;			/* of the entry, rounded up to 4 */
	SceSize length;
	char path[];
} DtracepModulePath;

/*
 * The paths of the modules whose records sceKernelGetModuleInfoForDebugger
 * gave, counts[i] of them for process pids[i]. Without paths, it counts the
 * bytes they take.
 */
static int _getExtendedModuleInfo(const SceKernelModuleInfoForDebugger *info, int count,
				  const SceUID *pids, const int *counts, int nprocs,
				  DtracepModulePath *paths, SceSize *pathsSize)
{
	SceSize capacity, size, entrySize;
	SceModuleCB *module;
	SceUID *owners, modid;
	size_t length;
	int i, j, k, n, res;

	capacity = pathsSize != NULL ? *pathsSize : 0;
	owners = dtrace_heap_zalloc(count * sizeof(SceUID));
	if (owners == NULL) {
		DTRACE_LOG_ERROR_AT(1654, "%s - failed to allocate memory", __func__);
		return SCE_KERNEL_ERROR_NO_MEMORY;
	}

	for (n = 0, j = 0; j < nprocs; j++) {
		for (k = 0; k < counts[j]; k++)
			owners[n + k] = pids[j];
		n += counts[j];
	}

	size = 0;
	for (i = 0; i < count; i++) {
		module = NULL;
		length = 0;
		modid = info->modid;
		if (owners[i] != SCE_KERNEL_PROCESS_ID) {
			modid = scePUIDtoGUID(owners[i], info->modid);
			if (modid < 0) {
				DTRACE_LOG_ERROR_AT(1672, "failed to convert module puid 0x%08x to guid in process 0x%08x (0x%08x)",
						    info->modid, owners[i], modid);
			}
		}
		if (modid >= 0) {
			res = SceModulemgrForKernel_37512E29(modid, &module);
			if (res != 0) {
				DTRACE_LOG_ERROR_AT(1680, "failed to get module with id 0x%08x (0x%08x)",
						    modid, res);
			} else if (module != NULL && module->path != NULL) {
				length = strlen(module->path);
			}
		}

		entrySize = (sizeof(DtracepModulePath) + length + 5) & ~3;
		size += entrySize;
		if (paths != NULL) {
			if (capacity < size) {
				dtrace_heap_free(owners);
				return SCE_KERNEL_ERROR_NO_MEMORY;
			}
			paths->size = entrySize;
			paths->length = length;
			if (module != NULL && length != 0)
				strncpy(paths->path, module->path, length);
			paths = (DtracepModulePath *)((char *)paths + paths->size);
		}
		info = (const SceKernelModuleInfoForDebugger *)((const char *)info + info->size);
	}

	dtrace_heap_free(owners);
	if (pathsSize != NULL)
		*pathsSize = size;
	return 0;
}

/* whether every variable a DIF object uses is one the state has */
static int dtracepCheckDifoVariables(const dif_instr_t *text, uint_t len,
				     const dtrace_vstate_t *vstate)
{
	int nglobals, ntlocals, nlocals;
	uint_t pc, v;

	nglobals = vstate->dtvs_nglobals;
	if (nglobals < 0)
		return 0;
	nlocals = vstate->dtvs_nlocals;
	if (nlocals < 0)
		return 0;
	ntlocals = vstate->dtvs_ntlocals;
	if (ntlocals < 0)
		return 0;

	for (pc = 0; pc < len; pc++) {
		v = DIF_INSTR_VAR(text[pc]);

		switch (DIF_INSTR_OP(text[pc])) {
		case DIF_OP_LDGS:
			if (v < DIF_VAR_OTHER_UBASE)
				break;
			if (v - DIF_VAR_OTHER_UBASE >= nglobals)
				return 0;
			break;
		case DIF_OP_STGS:
		case DIF_OP_LDGAA:
		case DIF_OP_STGAA:
			if (v < DIF_VAR_OTHER_UBASE)
				return 0;
			if (v - DIF_VAR_OTHER_UBASE >= nglobals)
				return 0;
			break;
		case DIF_OP_LDTS:
		case DIF_OP_STTS:
		case DIF_OP_LDTAA:
		case DIF_OP_STTAA:
			if (v < DIF_VAR_OTHER_UBASE || v - DIF_VAR_OTHER_UBASE >= ntlocals)
				return 0;
			break;
		case DIF_OP_LDLS:
			if (v < DIF_VAR_OTHER_UBASE)
				break;
			if (v - DIF_VAR_OTHER_UBASE >= nlocals)
				return 0;
			break;
		case DIF_OP_STLS:
			if (v < DIF_VAR_OTHER_UBASE)
				return 0;
			if (v - DIF_VAR_OTHER_UBASE >= nlocals)
				return 0;
			break;
		}
	}

	return 1;
}

/* whether the predicates and actions of a client's enablings use only its variables */
int dtracepCheckStateVariables(DtraceClientCB *client)
{
	dtrace_state_t *state;
	dtrace_predicate_t *pred;
	dtrace_action_t *act;
	dtrace_ecb_t *ecb;
	dtrace_difo_t *difo;
	minor_t minor;
	void *softstate;
	int i;

	minor = getminor(client->dtraceDev);
	if (ddi_get_last_soft_state(&softstate) != 0)
		return 0;

	state = ddi_get_soft_state(softstate, minor);
	if (state == NULL)
		return 0;

	for (i = 0; i < state->dts_necbs; i++) {
		for (ecb = state->dts_ecbs[i]; ecb != NULL; ecb = ecb->dte_next) {
			pred = ecb->dte_predicate;
			act = ecb->dte_action;
			if (pred != NULL) {
				difo = pred->dtp_difo;
				if (difo != NULL && difo->dtdo_buf != NULL &&
				    !dtracepCheckDifoVariables(difo->dtdo_buf, difo->dtdo_len,
							       &state->dts_vstate))
					return 0;
			}
			if (act != NULL) {
				difo = act->dta_difo;
				if (difo != NULL && difo->dtdo_buf != NULL &&
				    !dtracepCheckDifoVariables(difo->dtdo_buf, difo->dtdo_len,
							       &state->dts_vstate))
					return 0;
			}
		}
	}

	return 1;
}

int dtracepProviderCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepProviderReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	int res, rv;

	if (packet->payloadSize < sizeof(dtrace_providerdesc_t)) {
		DTRACE_LOG_ERROR_AT(227, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_PROVIDER + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		DTRACE_LOG_ERROR_AT(238, "(%d) %s - invalid token", header->token, __func__);
		res = SCE_ERROR_ERRNO_EBADF;
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_PROVIDER, header + 1,
					  sizeof(dtrace_providerdesc_t), NULL, NULL, &rv);
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data), DTRACEP_CMD_PROVIDER + 1, header->token);
	if (res >= 0)
		res = rv;
	data = reply->data;
	data->status = res;
	memcpy(&data->desc, header + 1, sizeof(data->desc));
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepProbesCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepProbeReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	int res, rv;

	if (packet->payloadSize < sizeof(dtrace_probedesc_t)) {
		DTRACE_LOG_ERROR_AT(264, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_PROBES + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		DTRACE_LOG_ERROR_AT(276, "(%d) %s - invalid token", header->token, __func__);
		res = SCE_ERROR_ERRNO_EBADF;
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_PROBES, header + 1,
					  sizeof(dtrace_probedesc_t), NULL, NULL, &rv);
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data), DTRACEP_CMD_PROBES + 1, header->token);
	if (res >= 0)
		res = rv;
	data = reply->data;
	data->status = res;
	memcpy(&data->desc, header + 1, sizeof(data->desc));
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepBufSnapCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepBufSnapReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	dtrace_bufdesc_t desc;
	size_t outSize;
	void *out;
	int res, rv, more;

	if (packet->payloadSize < sizeof(dtrace_bufdesc_t)) {
		DTRACE_LOG_ERROR_AT(302, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_BUF_SNAP + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	desc = ((DtracepBufSnapRequest *)header)->desc;
	more = 0;
	outSize = 0;
	out = NULL;
	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		res = SCE_ERROR_ERRNO_EBADF;
		DTRACE_LOG_ERROR_AT(318, "(%d) %s - invalid token", header->token, __func__);
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_BUFSNAP, &desc,
					  sizeof(desc), &out, &outSize, &rv);
	}

	if (res != 0) {
		outSize = 0;
		desc.dtbd_size = 0;
	} else if (outSize > DTRACEP_BIG_DATA_MAX) {
		more = 1;
		client->bigDataSize = outSize;
		client->bigDataOffset = DTRACEP_BIG_DATA_MAX;
		outSize = DTRACEP_BIG_DATA_MAX;
		client->bigData = out;
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data) + outSize, DTRACEP_CMD_BUF_SNAP + 1, header->token);
	data = reply->data;
	data->status = res < 0 ? res : rv;
	data->desc = desc;
	if (out != NULL && outSize != 0) {
		data->dataSize = outSize;
		memcpy(data->data, out, outSize);
		data->more = more;
	} else {
		data->dataSize = 0;
	}
	if (out != NULL && !more)
		dtrace_heap_free(out);

	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepProbeMatchCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepProbeReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	int res, rv;

	if (packet->payloadSize < sizeof(dtrace_probedesc_t)) {
		DTRACE_LOG_ERROR_AT(389, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_PROBE_MATCH + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		DTRACE_LOG_ERROR_AT(401, "(%d) %s - invalid token", header->token, __func__);
		res = SCE_ERROR_ERRNO_EBADF;
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_PROBEMATCH, header + 1,
					  sizeof(dtrace_probedesc_t), NULL, NULL, &rv);
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data), DTRACEP_CMD_PROBE_MATCH + 1, header->token);
	if (res >= 0)
		res = rv;
	data = reply->data;
	data->status = res;
	memcpy(&data->desc, header + 1, sizeof(data->desc));
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepEnableCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepEnableRequest *request = (DtracepEnableRequest *)packet->header;
	DtracepStatusReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	SceSize dofSize;
	void *dof;
	int res, rv;

	dofSize = packet->payloadSize;
	if (dofSize < sizeof(*request) - sizeof(DtracepHeader)) {
		DTRACE_LOG_ERROR_AT(427, "%s - payload size error (%d bytes)", __func__, dofSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_ENABLE + 1, request->header.command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	dof = &request->dof;
	if (request->bigData) {
		dof = NULL;
		dofSize = 0;
	}

	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, request->header.token);
	if (client == NULL) {
		DTRACE_LOG_ERROR_AT(440, "(%d) %s - invalid token", request->header.token,
				    __func__);
		res = SCE_ERROR_ERRNO_EBADF;
	} else {
		if (client->bigData != NULL && client->bigDataSize != 0) {
			dof = client->bigData;
			dofSize = client->bigDataSize;
		}
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_ENABLE, dof, dofSize,
					  NULL, NULL, &rv);
		if (client->bigData != NULL && client->bigDataSize != 0) {
			dtrace_heap_free(client->bigData);
			client->bigData = NULL;
			client->bigDataSize = 0;
			client->bigDataOffset = 0;
		}
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(DtracepStatusReply), DTRACEP_CMD_ENABLE + 1,
			  request->header.token);
	data = reply->data;
	if (res >= 0)
		res = rv;
	data->status = res;
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepAggSnapCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepBufSnapReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	dtrace_bufdesc_t desc;
	size_t outSize;
	void *out;
	int res, rv, more;

	if (packet->payloadSize < sizeof(dtrace_bufdesc_t)) {
		DTRACE_LOG_ERROR_AT(477, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_AGG_SNAP + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	desc = ((DtracepBufSnapRequest *)header)->desc;
	more = 0;
	outSize = 0;
	out = NULL;
	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		res = SCE_ERROR_ERRNO_EBADF;
		DTRACE_LOG_ERROR_AT(493, "(%d) %s - invalid token", header->token, __func__);
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_AGGSNAP, &desc,
					  sizeof(desc), &out, &outSize, &rv);
	}

	if (res != 0) {
		outSize = 0;
		desc.dtbd_size = 0;
	} else if (outSize > DTRACEP_BIG_DATA_MAX) {
		more = 1;
		client->bigDataSize = outSize;
		client->bigDataOffset = DTRACEP_BIG_DATA_MAX;
		outSize = DTRACEP_BIG_DATA_MAX;
		client->bigData = out;
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data) + outSize, DTRACEP_CMD_AGG_SNAP + 1, header->token);
	data = reply->data;
	data->status = res < 0 ? res : rv;
	data->desc = desc;
	if (out != NULL && outSize != 0) {
		data->dataSize = outSize;
		memcpy(data->data, out, outSize);
		data->more = more;
	} else {
		data->dataSize = 0;
	}
	if (out != NULL && !more)
		dtrace_heap_free(out);

	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepEProbeCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepEProbeReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	dtrace_eprobedesc_t desc;
	size_t outSize;
	void *out;
	int res, rv, more;

	if (packet->payloadSize < sizeof(dtrace_eprobedesc_t)) {
		DTRACE_LOG_ERROR_AT(563, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_EPROBE + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	desc = ((DtracepEProbeRequest *)header)->desc;
	more = 0;
	outSize = 0;
	out = NULL;
	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		res = SCE_ERROR_ERRNO_EBADF;
		DTRACE_LOG_ERROR_AT(579, "(%d) %s - invalid token", header->token, __func__);
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_EPROBE, &desc,
					  sizeof(desc), &out, &outSize, &rv);
		if (res == 0 && outSize > DTRACEP_EPROBE_DATA_MAX) {
			more = 1;
			client->bigDataSize = outSize;
			client->bigDataOffset = DTRACEP_EPROBE_DATA_MAX;
			outSize = DTRACEP_EPROBE_DATA_MAX;
			client->bigData = out;
		}
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data) + outSize, DTRACEP_CMD_EPROBE + 1, header->token);
	data = reply->data;
	data->status = res < 0 ? res : rv;
	if (res >= 0 && out != NULL) {
		data->dataSize = outSize;
		memcpy(data->data, out, outSize);
		data->more = more;
	} else {
		data->dataSize = 0;
		memset(data->data, 0, sizeof(dtrace_eprobedesc_t));
	}
	if (out != NULL && !more && outSize != 0)
		dtrace_heap_free(out);

	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepProbeArgCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepProbeArgReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	int res, rv;

	if (packet->payloadSize < sizeof(dtrace_argdesc_t)) {
		DTRACE_LOG_ERROR_AT(650, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_PROBE_ARG + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		DTRACE_LOG_ERROR_AT(662, "(%d) %s - invalid token", header->token, __func__);
		res = SCE_ERROR_ERRNO_EBADF;
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_PROBEARG, header + 1,
					  sizeof(dtrace_argdesc_t), NULL, NULL, &rv);
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data), DTRACEP_CMD_PROBE_ARG + 1, header->token);
	if (res >= 0)
		res = rv;
	data = reply->data;
	data->status = res;
	memcpy(&data->desc, header + 1, sizeof(data->desc));
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepConfCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepConfReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	dtrace_conf_t conf;
	int res, rv;

	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		DTRACE_LOG_ERROR_AT(700, "(%d) %s - invalid token", header->token, __func__);
		res = SCE_ERROR_ERRNO_EBADF;
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_CONF, &conf, sizeof(conf),
					  NULL, NULL, &rv);
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data), DTRACEP_CMD_CONF + 1, header->token);
	if (res >= 0)
		res = rv;
	data = reply->data;
	data->status = res;
	data->conf = conf;
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepStatusCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepStatusCmdReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	dtrace_status_t status;
	int res, rv;

	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		DTRACE_LOG_ERROR_AT(739, "(%d) %s - invalid token", header->token, __func__);
		res = SCE_ERROR_ERRNO_EBADF;
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_STATUS, &status,
					  sizeof(status), NULL, NULL, &rv);
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data), DTRACEP_CMD_STATUS + 1, header->token);
	if (res >= 0)
		res = rv;
	data = reply->data;
	data->status = res;
	memcpy(&data->dtStatus, &status, sizeof(status));
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepGoCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepCpuReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	processorid_t cpu;
	int res, rv;

	cpu = 0;
	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		res = SCE_ERROR_ERRNO_EBADF;
		DTRACE_LOG_ERROR_AT(780, "(%d) %s - invalid token", header->token, __func__);
	} else if (!dtracepCheckStateVariables(client)) {
		res = SCE_ERROR_ERRNO_EINVAL;
		SCE_DBG_ASSERT_FAIL(785);
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_GO, &cpu, sizeof(cpu),
					  NULL, NULL, &rv);
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data), DTRACEP_CMD_GO + 1, header->token);
	data = reply->data;
	if (res >= 0)
		res = rv;
	data->status = res;
	data->cpu = cpu;
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepStopCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepCpuReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	processorid_t cpu;
	int res, rv;

	cpu = 0;
	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		DTRACE_LOG_ERROR_AT(823, "(%d) %s - invalid token", header->token, __func__);
		res = SCE_ERROR_ERRNO_EBADF;
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_STOP, &cpu, sizeof(cpu),
					  NULL, NULL, &rv);
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data), DTRACEP_CMD_STOP + 1, header->token);
	data = reply->data;
	if (res >= 0)
		res = rv;
	data->status = res;
	data->cpu = cpu;
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepAggDescCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepAggDescReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	dtrace_aggdesc_t desc;
	size_t outSize;
	void *out;
	int res, rv, more;

	if (packet->payloadSize < sizeof(dtrace_aggdesc_t)) {
		DTRACE_LOG_ERROR_AT(849, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_AGG_DESC + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	more = 0;
	memcpy(&desc, header + 1, sizeof(desc));
	outSize = 0;
	out = NULL;
	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		res = SCE_ERROR_ERRNO_EBADF;
		DTRACE_LOG_ERROR_AT(865, "(%d) %s - invalid token", header->token, __func__);
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_AGGDESC, &desc,
					  sizeof(desc), &out, &outSize, &rv);
		if (res == 0 && outSize > DTRACEP_AGG_DESC_DATA_MAX) {
			more = 1;
			client->bigDataSize = outSize;
			client->bigDataOffset = DTRACEP_AGG_DESC_DATA_MAX;
			outSize = DTRACEP_AGG_DESC_DATA_MAX;
			client->bigData = out;
		}
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data) + outSize, DTRACEP_CMD_AGG_DESC + 1,
			  header->token);
	data = reply->data;
	data->status = res < 0 ? res : rv;
	if (res >= 0 && out != NULL) {
		data->dataSize = outSize;
		memcpy(data->data, out, outSize);
		data->more = more;
	} else {
		data->dataSize = 0;
		memset(data->data, 0, sizeof(dtrace_aggdesc_t));
	}
	if (out != NULL && !more && outSize != 0)
		dtrace_heap_free(out);

	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepFormatCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepFormatReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	dtrace_fmtdesc_t desc;
	size_t outSize;
	void *out;
	int res, rv, more;

	if (packet->payloadSize < sizeof(dtrace_fmtdesc_t)) {
		DTRACE_LOG_ERROR_AT(936, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_FORMAT + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	more = 0;
	desc = ((DtracepFormatRequest *)header)->desc;
	outSize = 0;
	out = NULL;
	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		res = SCE_ERROR_ERRNO_EBADF;
		DTRACE_LOG_ERROR_AT(953, "(%d) %s - invalid token", header->token, __func__);
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_FORMAT, &desc,
					  sizeof(desc), &out, &outSize, &rv);
	}

	if (res != 0) {
		outSize = 0;
		desc.dtfd_length = 0;
	} else if (outSize > DTRACEP_FORMAT_DATA_MAX) {
		more = 1;
		client->bigDataSize = outSize;
		client->bigDataOffset = DTRACEP_FORMAT_DATA_MAX;
		outSize = DTRACEP_FORMAT_DATA_MAX;
		client->bigData = out;
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data) + outSize, DTRACEP_CMD_FORMAT + 1, header->token);
	data = reply->data;
	data->status = res < 0 ? res : rv;
	data->desc = desc;
	if (out != NULL && outSize != 0) {
		data->dataSize = outSize;
		memcpy(data->data, out, outSize);
		data->more = more;
	} else {
		data->dataSize = 0;
	}
	if (out != NULL && !more)
		dtrace_heap_free(out);

	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepDofGetCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepDofGetRequest *request = (DtracepDofGetRequest *)packet->header;
	DtracepDofGetReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	dof_hdr_t *dof;
	size_t maxSize, size;
	SceSize replySize;
	int res, rv, more;

	if (packet->payloadSize < sizeof(dof_hdr_t)) {
		DTRACE_LOG_ERROR_AT(1024, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_DOF_GET + 1, request->header.command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	maxSize = dtrace_dof_maxsize;
	rv = 0;
	size = request->dof.dofh_loadsz;
	client = dtraceFindClientByToken(dtraceClients, request->header.token);
	if (client == NULL) {
		res = SCE_ERROR_ERRNO_EBADF;
		DTRACE_LOG_ERROR_AT(1039, "(%d) %s - invalid token", request->header.token,
				    __func__);
		goto error;
	}
	if (size < sizeof(dof_hdr_t)) {
		res = SCE_ERROR_ERRNO_EINVAL;
		DTRACE_LOG_ERROR_AT(1044, "(%d) %s - invalid load size %d bytes < sizeof(dof_hdr_t) = %d bytes",
				    client->token, __func__, size, sizeof(dof_hdr_t));
		goto error;
	}
	if (size > maxSize) {
		res = SCE_ERROR_ERRNO_E2BIG;
		DTRACE_LOG_ERROR_AT(1050, "(%d) %s - unable to allocate %d byte DOF data - exceeds maximum size of %d bytes",
				    client->token, __func__, size, maxSize);
		goto error;
	}
	dof = dtrace_heap_zalloc(size);
	if (dof == NULL) {
		res = SCE_ERROR_ERRNO_ENOMEM;
		DTRACE_LOG_ERROR_AT(1058, "(%d) %s - failed to allocate %d byte DOF data",
				    client->token, __func__, size);
		goto error;
	}

	*dof = request->dof;
	res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_DOFGET, dof, size, NULL, NULL,
				  &rv);
	more = 0;
	if (res != 0) {
		size = sizeof(dof_hdr_t);
		replySize = sizeof(*data) + sizeof(dof_hdr_t);
	} else {
		if (size >= dof->dofh_loadsz)
			size = dof->dofh_loadsz;
		if (size > DTRACEP_DOF_DATA_MAX) {
			client->bigDataSize = size;
			size = DTRACEP_DOF_DATA_MAX;
			more = 1;
			client->bigData = dof;
			client->bigDataOffset = DTRACEP_DOF_DATA_MAX;
		}
		replySize = sizeof(*data) + size;
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, replySize, DTRACEP_CMD_DOF_GET + 1, request->header.token);
	data = reply->data;
	data->status = res < 0 ? res : rv;
	data->dataSize = size;
	memcpy(&data->dof, dof, size);
	data->more = more;
	if (!more)
		dtrace_heap_free(dof);
	goto send;

error:
	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data) + sizeof(dof_hdr_t), DTRACEP_CMD_DOF_GET + 1,
			  request->header.token);
	data = reply->data;
	data->status = res;
	data->dataSize = sizeof(dof_hdr_t);
	data->dof = request->dof;
send:
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepReplicateCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepStatusReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	int res, rv;

	if (packet->payloadSize < sizeof(dtrace_repldesc_t)) {
		DTRACE_LOG_ERROR_AT(1128, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_REPLICATE + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		DTRACE_LOG_ERROR_AT(1141, "(%d) %s - invalid token", header->token, __func__);
		res = SCE_ERROR_ERRNO_EBADF;
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_REPLICATE, header + 1,
					  sizeof(dtrace_repldesc_t), NULL, NULL, &rv);
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data), DTRACEP_CMD_REPLICATE + 1, header->token);
	data = reply->data;
	if (res >= 0)
		res = rv;
	data->status = res;
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepAllProbesCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepProbeListReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	dtrace_probedesc_list_t list;
	size_t outSize;
	void *out;
	int res, rv, more;

	if (packet->payloadSize < sizeof(dtrace_probedesc_list_t)) {
		DTRACE_LOG_ERROR_AT(1167, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_ALL_PROBES + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	more = 0;
	memcpy(&list, header + 1, sizeof(list));
	outSize = 0;
	out = NULL;
	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		res = SCE_ERROR_ERRNO_EBADF;
		DTRACE_LOG_ERROR_AT(1183, "(%d) %s - invalid token", header->token, __func__);
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_ALLPROBES, &list,
					  sizeof(list), &out, &outSize, &rv);
		if (res == 0 && outSize > DTRACEP_PROBE_LIST_DATA_MAX) {
			more = 1;
			client->bigDataSize = outSize;
			client->bigDataOffset = DTRACEP_PROBE_LIST_DATA_MAX;
			outSize = DTRACEP_PROBE_LIST_DATA_MAX;
			client->bigData = out;
		}
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data) + outSize, DTRACEP_CMD_ALL_PROBES + 1, header->token);
	data = reply->data;
	data->status = res < 0 ? res : rv;
	if (res >= 0 && out != NULL) {
		data->dataSize = outSize;
		memcpy(data->data, out, outSize);
		data->more = more;
	} else {
		data->dataSize = sizeof(list);
		memcpy(data->data, &list, sizeof(list));
	}
	if (out != NULL && !more && outSize != 0)
		dtrace_heap_free(out);

	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepAllProbeMatchCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepProbeListReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	dtrace_probedesc_list_t list;
	size_t outSize;
	void *out;
	int res, rv, more;

	if (packet->payloadSize < sizeof(dtrace_probedesc_list_t)) {
		DTRACE_LOG_ERROR_AT(1255, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_ALL_PROBE_MATCH + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	more = 0;
	memcpy(&list, header + 1, sizeof(list));
	outSize = 0;
	out = NULL;
	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		res = SCE_ERROR_ERRNO_EBADF;
		DTRACE_LOG_ERROR_AT(1271, "(%d) %s - invalid token", header->token, __func__);
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_ALLPROBEMATCH, &list,
					  sizeof(list), &out, &outSize, &rv);
		if (res == 0 && outSize > DTRACEP_PROBE_LIST_DATA_MAX) {
			more = 1;
			client->bigDataSize = outSize;
			client->bigDataOffset = DTRACEP_PROBE_LIST_DATA_MAX;
			outSize = DTRACEP_PROBE_LIST_DATA_MAX;
			client->bigData = out;
		}
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data) + outSize, DTRACEP_CMD_ALL_PROBE_MATCH + 1, header->token);
	data = reply->data;
	data->status = res < 0 ? res : rv;
	if (res >= 0 && out != NULL) {
		data->dataSize = outSize;
		memcpy(data->data, out, outSize);
		data->more = more;
	} else {
		data->dataSize = sizeof(list);
		memcpy(data->data, &list, sizeof(list));
	}
	if (out != NULL && !more && outSize != 0)
		dtrace_heap_free(out);

	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepAllEProbesCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepProbeListReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	dtrace_eprobedesc_list_t list;
	size_t outSize;
	void *out;
	int res, rv, more;

	if (packet->payloadSize < sizeof(dtrace_eprobedesc_list_t)) {
		DTRACE_LOG_ERROR_AT(1342, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_ALL_EPROBES + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	list = ((DtracepEProbeListRequest *)header)->list;
	more = 0;
	outSize = 0;
	out = NULL;
	rv = 0;
	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		res = SCE_ERROR_ERRNO_EBADF;
		DTRACE_LOG_ERROR_AT(1358, "(%d) %s - invalid token", header->token, __func__);
	} else {
		res = dtrace_ioctl_remote(client->dtraceDev, DTRACEIOC_ALLEPROBES, &list,
					  sizeof(list), &out, &outSize, &rv);
		if (res == 0 && outSize > DTRACEP_PROBE_LIST_DATA_MAX) {
			more = 1;
			client->bigDataSize = outSize;
			client->bigDataOffset = DTRACEP_PROBE_LIST_DATA_MAX;
			outSize = DTRACEP_PROBE_LIST_DATA_MAX;
			client->bigData = out;
		}
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data) + outSize, DTRACEP_CMD_ALL_EPROBES + 1,
			  header->token);
	data = reply->data;
	data->status = res < 0 ? res : rv;
	if (res >= 0 && out != NULL) {
		data->dataSize = outSize;
		memcpy(data->data, out, outSize);
		data->more = more;
	} else {
		data->dataSize = sizeof(list);
		data->eprobes = list;
	}
	if (out != NULL && !more && outSize != 0)
		dtrace_heap_free(out);

	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepOpenCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepOpenRequest *request = (DtracepOpenRequest *)packet->header;
	DtracepStatusReply *data;
	DtracepPacket *reply;
	int res;

	if (packet->payloadSize < offsetof(DtracepOpenRequest, path) - sizeof(DtracepHeader)) {
		DTRACE_LOG_ERROR_AT(1429, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_OPEN + 1, request->header.command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	request->path[request->pathSize - 1] = '\0';
	res = sceDTraceClientOpen(request->path, SCE_KERNEL_PROCESS_ID, -1, request->flags);

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data), DTRACEP_CMD_OPEN + 1, request->header.token);
	data = reply->data;
	data->status = res;
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepCloseCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepStatusReply *data;
	DtracepPacket *reply;
	int res;

	if (packet->payloadSize < sizeof(int)) {
		DTRACE_LOG_ERROR_AT(1460, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_CLOSE + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	res = sceDTraceClientClose(header->token);

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data), DTRACEP_CMD_CLOSE + 1, header->token);
	data = reply->data;
	data->status = res;
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepGetBigDataCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepBigDataReply *data;
	DtraceClientCB *client;
	DtracepPacket *reply;
	SceSize size, replySize;
	const char *src;
	int more;

	client = dtraceFindClientByToken(dtraceClients, header->token);
	if (client == NULL) {
		DTRACE_LOG_ERROR_AT(1493, "(%d) %s - invalid token", header->token, __func__);
		dtracepSendStatusReply(ch, DTRACEP_CMD_GET_BIG_DATA + 1, header->command,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	if (client->bigData == NULL || client->bigDataSize == 0 || client->bigDataOffset == 0) {
		reply = dtracepAllocPacket();
		dtracepInitPacket(reply, sizeof(*data), DTRACEP_CMD_GET_BIG_DATA + 1,
				  header->token);
		data = reply->data;
		data->status = 0;
		data->more = 0;
		data->dataSize = 0;
		dtracepPushPacketQueue(ch->sendQueue, reply);
		reqSend(ch);
		return 0;
	}

	size = client->bigDataSize - client->bigDataOffset;
	src = (char *)client->bigData + client->bigDataOffset;
	if (size > DTRACEP_BIG_DATA_PART_MAX) {
		size = DTRACEP_BIG_DATA_PART_MAX;
		more = 1;
		replySize = sizeof(*data) + DTRACEP_BIG_DATA_PART_MAX;
		client->bigDataOffset += DTRACEP_BIG_DATA_PART_MAX;
	} else {
		replySize = sizeof(*data) + size;
		more = 0;
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, replySize, DTRACEP_CMD_GET_BIG_DATA + 1, header->token);
	data = reply->data;
	data->status = 0;
	if (src != NULL && size != 0) {
		data->dataSize = size;
		memcpy(data->data, src, size);
		data->more = more;
	} else {
		data->dataSize = 0;
	}
	if (!more) {
		dtrace_heap_free(client->bigData);
		client->bigData = NULL;
		client->bigDataSize = 0;
		client->bigDataOffset = 0;
	}

	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

int dtracepPutBigDataCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepPutBigDataRequest *request = (DtracepPutBigDataRequest *)packet->header;
	DtraceClientCB *client;
	DtracepPacket *reply;
	int error;

	if (packet->payloadSize < offsetof(DtracepPutBigDataRequest, data) - sizeof(DtracepHeader)) {
		DTRACE_LOG_ERROR_AT(1562, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		error = DTRACEP_ERROR_INVALID;
		goto error;
	}

	client = dtraceFindClientByToken(dtraceClients, request->header.token);
	if (client == NULL) {
		error = DTRACEP_ERROR_INVALID;
		DTRACE_LOG_ERROR_AT(1569, "(%d) %s - invalid token", request->header.token,
				    __func__);
		goto error;
	}

	if (client->bigData == NULL) {
		client->bigData = dtrace_heap_alloc(request->totalSize);
		if (client->bigData == NULL) {
			error = DTRACEP_ERROR_NO_MEMORY;
			DTRACE_LOG_ERROR_AT(1577, "%s - failed to allocate %d byte buffer for large data input",
					    __func__, request->totalSize);
			goto error;
		}
	}

	if (client->bigDataSize == 0) {
		if (client->bigDataOffset != 0) {
			DTRACE_LOG_ERROR_AT(1585, "%s - dtrace data buffer offset non-zero when initializing buffer size",
					    __func__);
			error = DTRACEP_ERROR_STATE;
			goto abort;
		}
		client->bigDataSize = request->totalSize;
	} else if (client->bigDataSize != request->totalSize) {
		DTRACE_LOG_ERROR_AT(1598, "%s - total data size of big data input changed during transaction",
				    __func__);
		error = DTRACEP_ERROR_INVALID;
		goto abort;
	}

	memcpy((char *)client->bigData + client->bigDataOffset, request->data, request->size);
	client->bigDataOffset += request->size;

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(DtracepStatusReply), DTRACEP_CMD_PUT_BIG_DATA + 1,
			  request->header.token);
	((DtracepStatusReply *)reply->data)->status = 0;
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;

abort:
	dtracepSendStatusReply(ch, DTRACEP_CMD_PUT_BIG_DATA + 1, request->header.command, error);
	dtrace_heap_free(client->bigData);
	client->bigData = NULL;
	client->bigDataSize = 0;
	client->bigDataOffset = 0;
	return error;

error:
	dtracepSendStatusReply(ch, DTRACEP_CMD_PUT_BIG_DATA + 1, request->header.command, error);
	return error;
}

int dtracepGetModuleInfoCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepModuleInfoRequest *request;
	SceKernelModuleInfoForDebugger *info;
	DtracepModuleInfoReply *data;
	DtracepModulePath *paths;
	DtracepPacket *reply;
	SceUID pids[DTRACEP_PROCESS_MAX];
	int counts[DTRACEP_PROCESS_MAX];
	SceSize nprocs, infoSize, pathsSize;
	char *record;
	int total, i, j, res;

	i = 0;
	pathsSize = 0;
	memset(counts, 0, sizeof(counts));
	nprocs = 0;
	request = (DtracepModuleInfoRequest *)packet->header;
	if (packet->payloadSize < sizeof(SceUID)) {
		DTRACE_LOG_ERROR_AT(1753, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_GET_MODULE_INFO + 1,
				       DTRACEP_CMD_GET_MODULE_INFO, DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	memset(pids, 0, sizeof(pids));
	SceProcessmgrForKernel_EFFF2975(5, pids, sizeof(pids), &nprocs);

	total = 0;
	info = NULL;
	paths = NULL;
	infoSize = 0;
	for (; i < (int)nprocs; i++) {
		if (!((pids[i] == SCE_KERNEL_PROCESS_ID && dtrace_security_check_internal() == 0) ||
		      SceSblACMgrForDriver_4CBD6156(pids[i]) != 0))
			continue;

		res = sceKernelGetModuleInfoForDebugger(pids[i], NULL, NULL);
		if (res < 0) {
			total = 0;
			DTRACE_LOG_ERROR_AT(1772, "%s - failed to get module info count for process 0x%08x (0x%08x)",
					    __func__, pids[i], res);
			goto reply;
		}
		if (request->pid == SCE_KERNEL_PROCESS_ID || request->pid == pids[i]) {
			total += res;
			counts[i] = res;
		}
	}
	if (total == 0)
		goto reply;

	infoSize = total * DTRACEP_MODULE_INFO_MAX;
	info = dtrace_heap_zalloc(infoSize);
	if (info == NULL) {
		total = 0;
		DTRACE_LOG_ERROR_AT(1792, "%s - failed to allocate module info", __func__);
		infoSize = 0;
		goto reply;
	}

	record = (char *)info;
	for (i = 0; i < (int)nprocs; i++) {
		if (counts[i] == 0)
			continue;
		res = sceKernelGetModuleInfoForDebugger(pids[i],
							(SceKernelModuleInfoForDebugger *)record,
							&counts[i]);
		if (res < 0) {
			total = 0;
			DTRACE_LOG_ERROR_AT(1805, "%s - failed to get module info (0x%08x)", __func__,
					    res);
			goto reply;
		}
		for (j = 0; j < counts[i]; j++)
			record += ((SceKernelModuleInfoForDebugger *)record)->size;
	}

	record = (char *)info;
	j = 0;
	do {
		j++;
		record += ((SceKernelModuleInfoForDebugger *)record)->size;
	} while (j < total);
	infoSize = record - (char *)info;

	res = _getExtendedModuleInfo(info, total, pids, counts, nprocs, NULL, &pathsSize);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(1818, "%s - failed to get extended module info size (0x%08x)",
				    __func__, res);
		goto reply;
	}
	paths = dtrace_heap_zalloc(pathsSize);
	if (paths == NULL) {
		DTRACE_LOG_ERROR_AT(1825, "%s - failed to allocate extended module info for process",
				    __func__);
		pathsSize = 0;
		goto reply;
	}
	res = _getExtendedModuleInfo(info, total, pids, counts, nprocs, paths, &pathsSize);
	if (res < 0) {
		DTRACE_LOG_ERROR_AT(1833, "%s - failed to get extended module info (0x%08x)",
				    __func__, res);
		pathsSize = 0;
	}

reply:
	if (pathsSize + infoSize > DTRACEP_MODULE_INFO_DATA_MAX) {
		DTRACE_LOG_ERROR_AT(1841, "%s - %d bytes of module info is too large for single packet",
				    __func__, pathsSize + infoSize);
		pathsSize = 0;
		infoSize = 0;
		reply = dtracepAllocPacket();
		dtracepInitPacket(reply, sizeof(*data) + pathsSize, DTRACEP_CMD_GET_MODULE_INFO + 1,
				  request->header.token);
		data = reply->data;
		data->pid = request->pid;
		data->count = 0;
	} else {
		reply = dtracepAllocPacket();
		dtracepInitPacket(reply, sizeof(*data) + pathsSize + infoSize,
				  DTRACEP_CMD_GET_MODULE_INFO + 1, request->header.token);
		data = reply->data;
		data->pid = request->pid;
		data->count = total;
		if (infoSize != 0)
			memcpy(data->data, info, infoSize);
	}
	if (pathsSize != 0)
		memcpy(data->data + infoSize, paths, pathsSize);
	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);

	if (info != NULL)
		dtrace_heap_free(info);
	if (paths != NULL)
		dtrace_heap_free(paths);
	return 0;
}

int dtracepGetProcessInfoCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepProcessInfoRequest *request;
	DtracepProcessInfoReply *data;
	DtracepPacket *reply;
	SceKernelProcessInfo info;
	char path[256];
	SceUID pid, modid;
	SceSize size, length;
	int res;

	if (packet->payloadSize < sizeof(SceUID)) {
		DTRACE_LOG_ERROR_AT(1889, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_GET_PROCESS_INFO + 1,
				       DTRACEP_CMD_GET_PROCESS_INFO, DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	request = (DtracepProcessInfoRequest *)packet->header;
	pid = request->pid;
	memset(&info, 0, sizeof(info));
	info.size = sizeof(info);
	if ((pid == SCE_KERNEL_PROCESS_ID && dtrace_security_check_internal() != 0) ||
	    dtrace_security_check_access_process_state(pid) != 0) {
		length = 0;
		size = sizeof(*data) + 1;
		modid = 0;
	} else {
		res = sceKernelGetProcessInfo(pid, &info);
		if (res != 0) {
			DTRACE_LOG_ERROR_AT(1907, "%s - failed to get process 0x%08x info (0x%08x)",
					    __func__, pid, res);
			memset(&info, 0, sizeof(info));
			info.size = sizeof(info);
		}
		modid = sceKernelGetModuleIdByPid(pid);
		if (modid > 0) {
			SceModulemgrForKernel_79E761B5(modid, path, sizeof(path));
			length = strnlen(path, sizeof(path));
			size = sizeof(*data) + length + 1;
		} else {
			size = sizeof(*data) + 1;
			length = 0;
		}
	}

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, size, DTRACEP_CMD_GET_PROCESS_INFO + 1, request->header.token);
	data = reply->data;
	if (info.pid == pid) {
		data->pid = pid;
		switch ((SceUInt8)info.status) {
		case 1:
			data->status = 1;
			break;
		case 2:
			data->status = 2;
			break;
		case 3:
			data->status = 3;
			break;
		case 4:
			data->status = 4;
			break;
		case 5:
			data->status = 5;
			break;
		default:
			data->status = 0;
			break;
		}
		data->parentPid = info.parentPid;
		SceModulemgrForKernel_337A3908(info.mainModule, &data->fingerprint);
		data->cpuAffinity = info.cpuAffinity;
		data->param = info.param;
		data->entryPoint = info.entryPoint;
		strncpy(data->name, info.name, sizeof(data->name) - 1);
	}
	if (modid > 0) {
		data->pathLength = length;
		memcpy(data->path, path, length + 1);
	}

	dtracepPushPacketQueue(ch->sendQueue, reply);
	reqSend(ch);
	return 0;
}

/* a notification the host did not ask for, to test the link */
static inline void dtracepSendTestNotification(DtracepChannel *ch, int value,
					       const char *message)
{
	DtracepTestNotification *data;
	DtracepPacket *notification;
	SceSize length;

	length = strnlen(message, 256);
	length = MIN(length, 255);

	notification = dtracepAllocPacket();
	dtracepInitPacket(notification, sizeof(*data) + length + 1, DTRACEP_TEST_NOTIFICATION,
			  -1);
	data = notification->data;
	DTRACE_LOG_DEBUG_AT(2015, 0, "%s - value=0x%08x message=%s len=%d size=%d", __func__,
			    value, message, length, notification->size);
	data->value = value;
	data->length = length;
	memcpy(data->message, message, length);
	data->message[length] = '\0';
	dtracepPushPacketQueue(ch->sendQueue, notification);
	reqSend(ch);
}

int dtracepTestCmd(DtracepChannel *ch, DtracepPacket *packet)
{
	DtracepHeader *header = packet->header;
	DtracepTestReply *data;
	DtracepPacket *reply;
	int value;

	if (packet->payloadSize < sizeof(int)) {
		DTRACE_LOG_ERROR_AT(1976, "%s - payload size error (%d bytes)", __func__,
				    packet->payloadSize);
		dtracepSendStatusReply(ch, DTRACEP_CMD_TEST + 1, DTRACEP_CMD_TEST,
				       DTRACEP_ERROR_INVALID);
		return DTRACEP_ERROR_INVALID;
	}

	value = ((DtracepTestRequest *)header)->value;
	DTRACE_LOG_DEBUG_AT(1984, 0, "%s - value=%d", __func__, value);

	reply = dtracepAllocPacket();
	dtracepInitPacket(reply, sizeof(*data), DTRACEP_CMD_TEST + 1, header->token);
	data = reply->data;
	data->status = 0;
	data->value = value + 1;
	dtracepPushPacketQueue(ch->sendQueue, reply);

	dtracepSendTestNotification(ch, value, "ZOMG! Ponies!!");
	reqSend(ch);
	return 0;
}

int dtracepSendErrorReply(DtracepChannel *ch, SceUInt8 command, int error)
{
	return dtracepSendStatusReply(ch, command + 1, command, error);
}
