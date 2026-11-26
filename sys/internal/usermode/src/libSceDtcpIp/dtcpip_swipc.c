/*
 * SceDtcpIp: the SW IPC the middleware application owns. The library connects to the
 * application's message pipes through SceLibKernel's msg pipes and a shared memory
 * block, sends a request message and waits for the reply. The commands are the
 * middleware's (ours, from the decompiled command numbers).
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <dtcpip/error.h>
#include <libkernel/libkernel.h>
#include "../../../../../internal/include/libkernel/libkernel.h"
#include "dtcpip_internal.h"

/* the SW IPC command numbers the library sends */
#define SCE_DTCP_IP_IPC_CONNECT			8449	/* 0x2101: create the session */
#define SCE_DTCP_IP_IPC_DISCONNECT		8450	/* 0x2102: close it */
#define SCE_DTCP_IP_IPC_GET_SESSION		8467	/* 0x2113: ask for the session's status */
#define SCE_DTCP_IP_IPC_GET_SESSION_RESULT	8468	/* 0x2114: poll the status reply */
#define SCE_DTCP_IP_IPC_CLOSE_SESSION		8469	/* 0x2115 */
#define SCE_DTCP_IP_IPC_OPEN_SESSION		8470	/* 0x2116 */
#define SCE_DTCP_IP_IPC_WAIT_READY		8471	/* 0x2117 */
#define SCE_DTCP_IP_IPC_DESTROY			8472	/* 0x2118 */

/* the status the middleware answers while it is not ready yet */
#define SCE_DTCP_IP_IPC_STATUS_BUSY		12007

/* the status the middleware answers with "not yet"; the pollers wait on it */
#define SCE_DTCP_IP_IPC_STATUS_LATER		30

/* the transport statuses a close treats as "the other side is gone already" */
#define SCE_DTCP_IP_IPC_STATUS_PIPE		12011
#define SCE_DTCP_IP_IPC_STATUS_CLOSED		12017

/*
 * the middleware's status word, in the library's request statuses. The library's
 * request layer has its own statuses; the session layer maps them again
 * (dtcpIpMapError).
 */
int dtcpIpMapIpcStatus(int status)
{
	int result;

	if (status < 1000 || status > 1652) {
		if (status > 12000) {
			if (status > 12205) {
				if (status > 12402) {
					if (status > 13000 && status <= 13019) {
						switch (status) {
						case 13001:
							result = 22;
							break;
						case 13002:
							result = 23;
							break;
						case 13003:
							result = 24;
							break;
						case 13004:
						case 13009:
						case 13010:
						case 13011:
						case 13012:
						case 13013:
						case 13016:
						case 13018:
							return 5;
						case 13005:
							result = 25;
							break;
						case 13006:
							result = 26;
							break;
						case 13007:
							result = 28;
							break;
						case 13008:
							result = 27;
							break;
						case 13014:
						case 13015:
						case 13017:
							result = 37;
							break;
						case 13019:
							result = 38;
							break;
						}
						return result;
					}
				} else if (status > 12400) {
					if (status <= 12401)
						return 20;
					else
						return 21;
				}
			} else {
				if (status <= 12017) {
					switch (status) {
					case 12001:
					case 12002:
						result = 1;
						break;
					case 12003:
						result = 2;
						break;
					case 12004:
						result = 3;
						break;
					case 12005:
						result = 4;
						break;
					case 12006:
						result = 29;
						break;
					case 12007:
					case 12008:
					case 12009:
					case 12010:
					case 12012:
					case 12013:
					case 12014:
						result = 5;
						break;
					case 12011:
						result = 35;
						break;
					case 12015:
						result = 6;
						break;
					case 12016:
						result = 34;
						break;
					case 12017:
						result = 36;
						break;
					}
					return result;
				}
				if (status > 12101) {
					if (status > 12200) {
						switch (status) {
						case 12201:
							result = 8;
							break;
						case 12202:
							return 5;
						case 12203:
							result = 10;
							break;
						case 12204:
							result = 11;
							break;
						case 12205:
							result = 12;
							break;
						}
						return result;
					}
				} else if (status > 12100) {
					return 7;
				}
			}
		} else if (status > 204) {
			if (status > 11008) {
				if (status <= 11009)
					return 21;
			} else if (status <= 215) {
				switch (status) {
				case 205:
				case 207:
				case 208:
				case 215:
					result = 10;
					break;
				case 206:
				case 209:
					result = 9;
					break;
				case 210:
				case 211:
				case 212:
				case 213:
				case 214:
					return 5;
				}
				return result;
			}
		} else if (status == 0) {
			return 0;
		}
		return 5;
	}
	switch (status) {
	case 1005:
	case 1508:
		return 7;
	case 1016:
		return 21;
	default:
		return 11;
	}
}

/*
 * create an IPC session object: the two clients (the session's and the stream's) over
 * one shared memory block, then the session request that carries the caller's config.
 */
int dtcpIpIpcCreate(int *outConnection, const int *sessionInfo, const unsigned int *config,
		    SceSize size)
{
	struct SceDtcpIpIpcMessage message;
	struct SceDtcpIpIpcObject *object;
	int ret;

	if (outConnection == NULL)
		return 2;
	if (sessionInfo == NULL)
		return 2;

	object = dtcpIpAlloc(sizeof(*object));
	if (object == NULL)
		return 5;
	object->session = NULL;
	object->stream = NULL;
	if (dtcpIpIpcInitSession(object, config, size) != 0 ||
	    dtcpIpIpcInitStream(&object->stream, object->session, 0, 0, 0, 0) != 0 ||
	    dtcpIpIpcInitShared(object->stream, &object->shared) != 0) {
		ret = dtcpIpMapIpcStatus(0);
	} else {
		message.command = SCE_DTCP_IP_IPC_CONNECT;
		message.channel = 0;
		message.size = size;
		message.payload = (void *)config;
		message.value0 = sessionInfo[0];
		message.value1 = sessionInfo[1];
		message.value2 = g_dtcpIpIpcFlag;
		message.value3 = 0;
		if (dtcpIpIpcRequest(object->stream, &message) == 0 && message.result == 0) {
			*outConnection = (int)object;
			return 0;
		}
		ret = dtcpIpMapIpcStatus(message.result);
	}
	if (object->stream != NULL)
		dtcpIpIpcFiniStream(object->stream, 0);
	if (object->session != NULL)
		dtcpIpIpcFiniSession(object->session);
	dtcpIpFree(object);
	*outConnection = 0;
	return ret;
}

/* close an IPC session object */
int dtcpIpIpcClose(int connection)
{
	struct SceDtcpIpIpcObject *object = (struct SceDtcpIpIpcObject *)connection;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (object == NULL)
		return 2;
	message.command = SCE_DTCP_IP_IPC_DISCONNECT;
	message.channel = 0;
	message.size = 0;
	ret = dtcpIpIpcRequest(object->stream, &message);
	if ((ret == 0 || ret == SCE_DTCP_IP_IPC_STATUS_PIPE ||
	     ret == SCE_DTCP_IP_IPC_STATUS_CLOSED) &&
	    (ret != 0 || (ret = message.result, ret == 0)) &&
	    ((ret = dtcpIpIpcFiniStream(object->stream, message.result)) == 0 ||
	     ret == SCE_DTCP_IP_IPC_STATUS_CLOSED || ret == SCE_DTCP_IP_IPC_STATUS_PIPE) &&
	    (dtcpIpIpcFiniSession(object->session), dtcpIpFree(object), ret == 0))
		return 0;
	return dtcpIpMapIpcStatus(ret);
}

/* tell the middleware the session is being destroyed (it closes it first) */
int dtcpIpIpcDestroy(int connection)
{
	struct SceDtcpIpIpcObject *object = (struct SceDtcpIpIpcObject *)connection;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (connection == 0)
		return 2;
	message.command = SCE_DTCP_IP_IPC_DESTROY;
	message.channel = 0;
	message.size = 0;
	ret = dtcpIpIpcRequest(object->stream, &message);
	if (ret != 0)
		return dtcpIpMapIpcStatus(ret);
	if (message.result != 0)
		return dtcpIpMapIpcStatus(message.result);
	return 0;
}

/*
 * wait for the middleware to finish opening its session: it answers 12007 while it is
 * still working on it; the caller polls again (30 tells it to).
 */
int dtcpIpIpcWaitReady(int connection, int *outStatus)
{
	struct SceDtcpIpIpcObject *object = (struct SceDtcpIpIpcObject *)connection;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (connection == 0)
		return 2;
	message.command = SCE_DTCP_IP_IPC_WAIT_READY;
	message.channel = 0;
	message.size = 0;
	ret = dtcpIpIpcRequest(object->stream, &message);
	if (ret != 0)
		return dtcpIpMapIpcStatus(ret);
	if (outStatus != NULL)
		*outStatus = message.out0;
	if (message.result != SCE_DTCP_IP_IPC_STATUS_BUSY)
		return dtcpIpMapIpcStatus(message.result);
	return SCE_DTCP_IP_IPC_STATUS_LATER;
}

/* connect to the middleware: its session and stream objects over one shared block */
int dtcpIpIpcConnect(void **outConnection)
{
	struct SceDtcpIpIpcObject *object;
	unsigned int config[35];
	int sharedSize;
	int ret;

	if (outConnection == NULL)
		return 2;

	config[34] = 0;
	config[0] = (unsigned int)&g_dtcpIpIpcConfig[35];
	object = dtcpIpAlloc(sizeof(*object));
	if (object == NULL)
		return 5;
	object->session = NULL;
	object->stream = NULL;
	ret = dtcpIpIpcInitSession(object, config, 140);
	if (ret == 0)
		ret = dtcpIpIpcInitStream(&object->stream, object->session, 0, 0, 0, 0x80000);
	if (ret == 0)
		ret = dtcpIpIpcInitShared(object->stream, &object->shared);
	if (ret != 0) {
		ret = dtcpIpMapIpcStatus(ret);
		if (object->stream != NULL)
			dtcpIpIpcFiniStream(object->stream, 0);
		if (object->session != NULL)
			dtcpIpIpcFiniSession(object->session);
		dtcpIpFree(object);
		*outConnection = NULL;
		return ret;
	}
	*outConnection = object;
	return 0;
}

/* disconnect from the middleware */
int dtcpIpIpcDisconnect(void *connection)
{
	struct SceDtcpIpIpcObject *object = connection;
	int ret;

	if (object == NULL)
		return 2;
	ret = dtcpIpIpcFiniStream(object->stream, 0);
	if (ret != 0)
		return dtcpIpMapIpcStatus(ret);
	dtcpIpIpcFiniSession(object->session);
	dtcpIpFree(object);
	return 0;
}

/* the middleware's session status: ask, then poll until it answers */
int dtcpIpIpcGetSessionStatus(void *connection, int *outStatus)
{
	struct SceDtcpIpIpcObject *object = connection;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (connection == NULL)
		return 2;
	message.command = SCE_DTCP_IP_IPC_GET_SESSION;
	message.channel = 0;
	message.size = 0;
	if (dtcpIpIpcRequest(object->stream, &message) == 0 && message.result == 0) {
		for (;;) {
			message.command = SCE_DTCP_IP_IPC_GET_SESSION_RESULT;
			message.channel = 0;
			message.size = 0;
			if (dtcpIpIpcRequest(object->stream, &message) != 0)
				break;
			if (message.result == 0) {
				if (outStatus != NULL)
					*outStatus = message.out0;
				return 0;
			}
			if (message.result != SCE_DTCP_IP_IPC_STATUS_BUSY)
				break;
			sceKernelDelayThread(10000);
		}
	}
	if (outStatus != NULL)
		*outStatus = message.out0;
	return dtcpIpMapIpcStatus(message.result);
}

/* ask the middleware to close its session */
int dtcpIpIpcCloseSession(void *connection)
{
	struct SceDtcpIpIpcObject *object = connection;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (connection == NULL)
		return 2;
	message.command = SCE_DTCP_IP_IPC_CLOSE_SESSION;
	message.channel = 0;
	message.size = 0;
	ret = dtcpIpIpcRequest(object->stream, &message);
	if (ret != 0)
		return dtcpIpMapIpcStatus(ret);
	if (message.result != 0)
		return dtcpIpMapIpcStatus(message.result);
	return 0;
}

/* ask the middleware to open its session */
int dtcpIpIpcOpenSession(void *connection)
{
	struct SceDtcpIpIpcObject *object = connection;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (connection == NULL)
		return 2;
	message.command = SCE_DTCP_IP_IPC_OPEN_SESSION;
	message.channel = 0;
	message.size = 0;
	ret = dtcpIpIpcRequest(object->stream, &message);
	if (ret != 0)
		return dtcpIpMapIpcStatus(ret);
	if (message.result != 0)
		return dtcpIpMapIpcStatus(message.result);
	return 0;
}

/* the head of an SW IPC client: the mutex and the operations-in-flight word */
struct SceDtcpIpIpcClientState {
	void *ipc;			/* 0x00: the connection */
	int pad;			/* 0x04 */
	SceKernelLwMutexWork mutex;	/* 0x08: "sce_sws_mutex" */
	int pending;			/* 0x28: the status of a failed request */
	int flushing;			/* 0x2C: a flush is pending */
	int state;			/* 0x30: the operations in flight */
	int session;			/* 0x34: the middleware's session handle */
};

/* take the client's state word and hold an operation bit on it (0 none, 1 opening, 2 open) */
int dtcpIpIpcSetState(int *hold, void *connection, unsigned int state)
{
	struct SceDtcpIpIpcClientState *client = connection;

	hold[0] = (int)connection;
	hold[1] = state;
	if (client == NULL)
		return 0;
	sceKernelLockLwMutex(&client->mutex, 1, NULL);
	if ((state > 2 && state == 4) || (state <= 2 && state != 0)) {
		if (((struct SceDtcpIpIpcClientState *)hold[0])->state != 0) {
			sceKernelUnlockLwMutex(&client->mutex, 1);
			return 12004;
		}
	}
	((struct SceDtcpIpIpcClientState *)hold[0])->state |= hold[1];
	sceKernelUnlockLwMutex(&client->mutex, 1);
	return 0;
}

/*
 * the system engine's status word, in the library's request statuses: the middleware's
 * session statuses first, then its request statuses.
 */
int dtcpIpMapIpcStatus2(int status)
{
	int result;

	if (status >= 1000 && status <= 1652) {
		if (status > 1019) {
			if (status == 1508)
				return 7;
		} else if (status > 1004) {
			switch (status) {
			case 1005:
				return 7;
			case 1006:
			case 1007:
			case 1008:
			case 1009:
			case 1010:
			case 1011:
			case 1012:
			case 1013:
			case 1014:
			case 1015:
			case 1017:
			case 1018:
				return 11;
			case 1016:
				result = 21;
				break;
			case 1019:
				result = 33;
				break;
			}
			return result;
		}
		return 11;
	}
	if (status > 12000) {
		if (status > 12206) {
			if (status <= 12402) {
				if (status > 12400) {
					if (status <= 12401)
						return 20;
					else
						return 21;
				}
				return 5;
			}
			if (status <= 13000 || status > 13017)
				return 5;
			switch (status) {
			case 13001:
				result = 22;
				break;
			case 13002:
				result = 23;
				break;
			case 13003:
				result = 24;
				break;
			case 13004:
			case 13009:
			case 13010:
			case 13011:
			case 13012:
			case 13013:
			case 13014:
			case 13015:
			case 13016:
				return 5;
			case 13005:
				result = 25;
				break;
			case 13006:
				result = 26;
				break;
			case 13007:
				result = 28;
				break;
			case 13008:
				result = 27;
				break;
			case 13017:
				result = 37;
				break;
			}
		} else if (status <= 12017) {
			switch (status) {
			case 12001:
			case 12002:
				result = 1;
				break;
			case 12003:
				result = 2;
				break;
			case 12004:
				result = 3;
				break;
			case 12005:
				result = 4;
				break;
			case 12006:
				result = 29;
				break;
			case 12007:
				result = 30;
				break;
			case 12008:
			case 12009:
			case 12010:
			case 12012:
			case 12013:
			case 12014:
				result = 5;
				break;
			case 12011:
				result = 35;
				break;
			case 12015:
				result = 6;
				break;
			case 12016:
				return 5;
			case 12017:
				result = 36;
				break;
			}
		} else {
			if (status <= 12101) {
				if (status > 12100)
					return 7;
				return 5;
			}
			if (status <= 12200)
				return 5;
			switch (status) {
			case 12201:
				result = 8;
				break;
			case 12202:
				return 5;
			case 12203:
				result = 10;
				break;
			case 12204:
				result = 11;
				break;
			case 12205:
				result = 12;
				break;
			case 12206:
				result = 33;
				break;
			}
		}
	} else {
		if (status <= 204) {
			if (status == 0)
				return 0;
			return 5;
		}
		if (status > 11008) {
			if (status <= 11009)
				return 21;
			return 5;
		}
		if (status > 215)
			return 5;
		switch (status) {
		case 205:
		case 207:
		case 208:
		case 215:
			result = 10;
			break;
		case 206:
		case 209:
			result = 9;
			break;
		case 210:
		case 211:
		case 212:
		case 213:
		case 214:
			return 5;
		}
	}
	return result;
}
