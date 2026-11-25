/*
 * SceDtcpIp: the system client of the SW IPC. It carries the session's requests: connect,
 * set the uri, open the stream, read its chunks, send data, flush and close. The
 * request commands are the middleware's (ours, from the decompiled numbers).
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <dtcpip/error.h>
#include <libkernel/libkernel.h>
#include "../../../../../internal/include/libkernel/libkernel.h"
#include "dtcpip_internal.h"

/* the system IPC's commands */
#define SW_SYS_CONNECT			10241	/* 0x2801 */
#define SW_SYS_CLOSE			10242	/* 0x2802 */
#define SW_SYS_SET_URI			10243	/* 0x2803 */
#define SW_SYS_GET_URI_RESULT		10244	/* 0x2804 */
#define SW_SYS_OPEN_STREAM		10246	/* 0x2806 */
#define SW_SYS_CLOSE_STREAM		10247	/* 0x2807 */
#define SW_SYS_GET_INFO			10251	/* 0x280B */
#define SW_SYS_SET_PARAMETER		10252	/* 0x280C */
#define SW_SYS_RECEIVE			10254	/* 0x280E */
#define SW_SYS_RELEASE			10255	/* 0x280F */
#define SW_SYS_SEND			10256	/* 0x2810 */
#define SW_SYS_SEND_MESSAGE		10257	/* 0x2811 */
#define SW_SYS_ABORT			10258	/* 0x2812 */
#define SW_SYS_IS_STREAM_OPEN		10259	/* 0x2813 */
#define SW_SYS_WAIT_CLOSED		10260	/* 0x2814 */
#define SW_SYS_POLL_CLOSED		10261	/* 0x2815 */

/* the statuses the middleware answers while it is working on a request */
#define SW_SYS_STATUS_LATER		12007
#define SW_SYS_STATUS_EMPTY		12401
#define SW_SYS_STATUS_NO_WAIT		12006

/* the session client: the connection it wraps and the request state */
struct SwSysClient {
	struct SceDtcpIpIpcObject *ipc;	/* 0x00: the connection */
	int pad;			/* 0x04 */
	SceKernelLwMutexWork mutex;	/* 0x08: "sce_sws_mutex" */
	int pending;			/* 0x28: the status of a failed request */
	int flushing;			/* 0x2C: a flush is pending */
	int state;			/* 0x30: the operations in flight */
	int session;			/* 0x34: the middleware's session handle */
};

/* the stream: the stream channel on the session's client and its shared buffer */
struct SwSysStream {
	struct SwSysClient *client;	/* 0x00: the session client */
	void *channel;			/* 0x04: the stream channel */
	int padding[3];			/* 0x08 */
	int padAfterChannel;		/* 0x14 */
	SceKernelLwMutexWork mutex;	/* 0x18: "sce_sws_mutex" */
	int status;			/* 0x38: the status of a failed request */
	int state;			/* 0x3C: the operations in flight */
	int id;				/* 0x40: the middleware's stream id */
	void *buffer;			/* 0x44: the shared buffer */
	void *buffer2;			/* 0x48: the stream's data area */
	int trailingPad;		/* 0x4C */
};

/* the SW IPC's status, in the library's request statuses (dtcpip_swipc.c) */
int dtcpIpMapIpcStatus2(int status);

/* the client's state word, taken while an operation is in flight (dtcpip_swipc.c) */
int dtcpIpIpcSetState(int *hold, void *connection, unsigned int state);

/* take the stream's state bits: a conflicting operation in flight is refused */
int dtcpIpSwAcquireStream(int *hold, void *object, unsigned int state)
{
	struct SwSysStream *stream = object;
	int conflict;

	hold[0] = (int)object;
	hold[1] = (int)state;
	if (stream == NULL)
		return 0;
	stream = (struct SwSysStream *)hold[0];
	sceKernelLockLwMutex(&stream->mutex, 1, NULL);
	if (state > 0x3FF) {
		if (state == 0x80000000)
			conflict = (stream->state & 2) != 0;
		else if (state == 0x2000 || state == 4096 || state == 2048 || state == 1024)
			conflict = (stream->state & 2) != 0;
		else
			conflict = 0;
	} else if (state > 0x10) {
		if (state > 0x40)
			conflict = state == 512 && (stream->state & 2) != 0;
		else if (state <= 0x3F)
			conflict = 0;
		else
			conflict = (stream->state & 2) != 0;
	} else if (state > 2) {
		conflict = state > 0xF && stream->state != 0;
	} else if (state != 0) {
		conflict = stream->state != 0;
	} else {
		conflict = 0;
	}
	if (conflict) {
		sceKernelUnlockLwMutex(&stream->mutex, 1);
		return 12004;
	}
	stream->state |= hold[1];
	sceKernelUnlockLwMutex(&stream->mutex, 1);
	return 0;
}

/*
 * create the session client: a 56-byte object over the connection, its mutex, and the
 * connect request that carries the middleware's session parameters.
 */
int dtcpIpSysInit(struct SwSysClient **outClient, struct SceDtcpIpIpcObject *connection)
{
	struct SwSysClient *client;
	struct SceDtcpIpIpcMessage message;
	int hold[2] = { 0, 0 };
	int ret;

	if (outClient == NULL)
		return 2;
	if (connection == NULL)
		return 2;
	client = dtcpIpAlloc(sizeof(*client));
	if (client == NULL)
		return 5;
	client->ipc = NULL;
	client->pending = 0;
	client->flushing = 0;
	client->state = 0;
	client->session = 0;
	if (sceKernelCreateLwMutex(&client->mutex, "sce_sws_mutex", 0x2000, 0, NULL) < 0) {
		ret = 5;
		dtcpIpFree(client);
		*outClient = NULL;
		return ret;
	}
	if (dtcpIpIpcSetState(hold, client, 1) != 0) {
		ret = 3;
	} else {
		client->flushing = 0;
		message.command = SW_SYS_CONNECT;
		message.channel = 0;
		message.size = 0;
		if (dtcpIpIpcRequest(connection->stream, &message) != 0 || message.result != 0) {
			ret = dtcpIpMapIpcStatus2(message.result);
		} else {
			client->session = message.out0;
			client->pending = 0;
			client->ipc = connection;
			*outClient = client;
			ret = 0;
		}
	}
	if (ret != 0) {
		dtcpIpFree(client);
		*outClient = NULL;
	}
	if (hold[0] != 0) {
		struct SwSysClient *held = (struct SwSysClient *)hold[0];

		sceKernelLockLwMutex(&held->mutex, 1, NULL);
		held->state &= ~hold[1];
		sceKernelUnlockLwMutex(&held->mutex, 1);
	}
	return ret;
}

/*
 * set the stream's uri: the middleware gets the uri, its port and the session handle,
 * and answers the uri's id; the caller gets the id's low byte.
 */
int dtcpIpSysSetUri(struct SwSysClient *client, const char *uri, unsigned short port,
		    unsigned char *outId)
{
	struct SceDtcpIpIpcMessage message;
	int hold[2] = { 0, 0 };
	int ret;
	int result;

	if (client == NULL)
		return 2;
	if (dtcpIpIpcSetState(hold, client, 4) != 0) {
		result = 3;
	} else {
		client->flushing = 0;
		message.command = SW_SYS_SET_URI;
		message.channel = 0;
		message.size = dtcpIpStrnlen(uri) + 1;
		message.payload = (void *)uri;
		message.value0 = client->session;
		message.value1 = port;
		ret = dtcpIpIpcRequest(client->ipc->stream, &message);
		if (ret == 0) {
			ret = message.result;
			if (message.result == 0 || message.result == SW_SYS_STATUS_LATER) {
				for (;;) {
					message.command = SW_SYS_GET_URI_RESULT;
					message.channel = 0;
					message.size = 0;
					message.value0 = client->session;
					message.value1 = client->flushing;
					ret = dtcpIpIpcRequest(client->ipc->stream, &message);
					if (ret != 0)
						break;
					ret = message.result;
					if (message.result == 0) {
						*outId = (unsigned char)message.out0;
						client->pending = 0;
						break;
					}
					if (message.result != SW_SYS_STATUS_LATER)
						break;
					sceKernelDelayThread(1000);
				}
			}
		}
		if (ret == 0) {
			result = 0;
		} else {
			client->pending = ret;
			result = dtcpIpMapIpcStatus2(ret);
		}
	}
	if (hold[0] != 0) {
		struct SwSysClient *held = (struct SwSysClient *)hold[0];

		sceKernelLockLwMutex(&held->mutex, 1, NULL);
		held->state &= ~hold[1];
		sceKernelUnlockLwMutex(&held->mutex, 1);
	}
	return result;
}

/* close the session client: the middleware's session, the mutex and the object */
int dtcpIpSysCloseSession(struct SwSysClient *client)
{
	struct SceDtcpIpIpcMessage message;
	int hold[2] = { 0, 0 };
	int ret;
	int result;

	if (client == NULL)
		return 2;
	if (dtcpIpIpcSetState(hold, client, 2) != 0) {
		result = 3;
	} else {
		message.command = SW_SYS_CLOSE;
		message.channel = 0;
		message.size = 0;
		message.value0 = client->session;
		ret = dtcpIpIpcRequest(client->ipc->stream, &message);
		if (ret != 0)
			result = dtcpIpMapIpcStatus2(ret);
		else if (message.result != 0)
			result = dtcpIpMapIpcStatus2(message.result);
		else
			result = 0;
	}
	if (hold[0] != 0) {
		struct SwSysClient *held = (struct SwSysClient *)hold[0];

		sceKernelLockLwMutex(&held->mutex, 1, NULL);
		held->state &= ~hold[1];
		sceKernelUnlockLwMutex(&held->mutex, 1);
	}
	sceKernelDeleteLwMutex(&client->mutex);
	dtcpIpFree(client);
	return result;
}

/* the caller starts a flush: the stream's state says so */
int dtcpIpSysFlushStart(struct SwSysClient *client)
{
	if (client == NULL)
		return 2;
	client->flushing = 1;
	return 0;
}

/* create the stream: a channel over the session's client and the middleware's open */
int dtcpIpSysOpenStream(struct SwSysStream **outStream, struct SwSysClient *client, int *config)
{
	struct SwSysStream *stream;
	struct SceDtcpIpIpcMessage message;
	void *shared[4];
	int hold[2] = { 0, 0 };
	int ret;
	int result;

	if (outStream == NULL)
		return 2;
	if (client == NULL)
		return 2;
	stream = dtcpIpAlloc(sizeof(*stream));
	if (stream == NULL)
		return 5;
	stream->client = NULL;
	stream->channel = NULL;
	stream->padAfterChannel = 0;
	stream->status = 0;
	stream->state = 0;
	stream->id = 0;
	if (sceKernelCreateLwMutex(&stream->mutex, "sce_sws_mutex", 0x2000, 0, NULL) < 0) {
		dtcpIpFree(stream);
		*outStream = NULL;
		return 5;
	}
	stream->client = client;
	ret = dtcpIpIpcInitStream(&stream->channel, client->ipc->session, config[0],
				  config[1] + config[2], 1, 0);
	if (ret != 0) {
		result = dtcpIpMapIpcStatus2(ret);
	} else if ((ret = dtcpIpIpcInitShared(stream->channel, shared)) != 0) {
		result = dtcpIpMapIpcStatus2(ret);
	} else {
		stream->buffer = shared[1];
		stream->buffer2 = (char *)shared[1] + config[1];
		if (dtcpIpSwAcquireStream(hold, stream, 1) != 0) {
			result = 3;
		} else {
			message.command = SW_SYS_OPEN_STREAM;
			message.channel = 0;
			message.size = 20;
			message.payload = config;
			message.value0 = client->session;
			ret = dtcpIpIpcRequest(stream->channel, &message);
			if (ret == 0)
				ret = message.result;
			if (ret == 0) {
				stream->id = message.out0;
				stream->status = 0;
				*outStream = stream;
				sceKernelLockLwMutex(&stream->mutex, 1, NULL);
				stream->state &= ~hold[1];
				sceKernelUnlockLwMutex(&stream->mutex, 1);
				return 0;
			}
			result = dtcpIpMapIpcStatus2(ret);
		}
	}
	dtcpIpFree(stream);
	*outStream = NULL;
	if (hold[0] != 0) {
		struct SwSysStream *held = (struct SwSysStream *)hold[0];

		sceKernelLockLwMutex(&held->mutex, 1, NULL);
		held->state &= ~hold[1];
		sceKernelUnlockLwMutex(&held->mutex, 1);
	}
	return result;
}

/* close the stream: the middleware's stream request, then the channel and object */
int dtcpIpSysCloseStream(void *object)
{
	struct SwSysStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int hold[2] = { 0, 0 };
	int ret;
	int result;
	int closeResult;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(hold, stream, 2) == 0) {
		message.command = SW_SYS_CLOSE_STREAM;
		message.channel = 0;
		message.size = 0;
		message.value0 = stream->id;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret == 0)
			ret = message.result;
		if (ret != 0)
			result = dtcpIpMapIpcStatus2(ret);
		else
			result = 0;
		if (hold[0] != 0) {
			struct SwSysStream *held = (struct SwSysStream *)hold[0];

			sceKernelLockLwMutex(&held->mutex, 1, NULL);
			held->state &= ~hold[1];
			sceKernelUnlockLwMutex(&held->mutex, 1);
		}
		closeResult = dtcpIpIpcFiniStream(stream->channel, hold[0]);
		sceKernelDeleteLwMutex(&stream->mutex);
		dtcpIpFree(stream);
		return closeResult;
	}
	if (hold[0] != 0) {
		struct SwSysStream *held = (struct SwSysStream *)hold[0];

		sceKernelLockLwMutex(&held->mutex, 1, NULL);
		held->state &= ~hold[1];
		sceKernelUnlockLwMutex(&held->mutex, 1);
	}
	return 3;
}

/* get the stream's info from the middleware (the caller's 56-byte buffer) */
int dtcpIpSysGetInfo(void *object, void *info)
{
	struct SwSysStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int hold[2] = { 0, 0 };
	int ret;
	int result;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(hold, stream, 16) != 0) {
		result = 3;
	} else {
		message.command = SW_SYS_GET_INFO;
		message.channel = 0;
		message.size = 576;
		message.payload = info;
		message.value0 = stream->id;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret == 0)
			ret = message.result;
		if (ret != 0) {
			stream->status = ret;
			result = dtcpIpMapIpcStatus2(ret);
		} else {
			result = 0;
		}
	}
	if (hold[0] != 0) {
		struct SwSysStream *held = (struct SwSysStream *)hold[0];

		sceKernelLockLwMutex(&held->mutex, 1, NULL);
		held->state &= ~hold[1];
		sceKernelUnlockLwMutex(&held->mutex, 1);
	}
	return result;
}

/* set the stream's parameter (the middleware waits for it while it is busy) */
int dtcpIpSysSetParameter(void *object, int parameter)
{
	struct SwSysStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int hold[2] = { 0, 0 };
	int ret;
	int result;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(hold, stream, 0x40) != 0) {
		result = 3;
	} else {
		message.command = SW_SYS_SET_PARAMETER;
		message.channel = 0;
		message.size = 0;
		message.value0 = stream->id;
		message.value1 = parameter;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret == 0) {
			ret = message.result;
			while (ret == SW_SYS_STATUS_LATER) {
				sceKernelDelayThread(1000);
				message.command = SW_SYS_SET_PARAMETER;
				message.channel = 0;
				message.size = 0;
				message.value0 = stream->id;
				message.value1 = parameter;
				ret = dtcpIpIpcRequest(stream->channel, &message);
				if (ret != 0)
					break;
				ret = message.result;
			}
		}
		if (ret == 0) {
			stream->status = 0;
			result = 0;
		} else {
			stream->status = ret;
			result = dtcpIpMapIpcStatus2(ret);
		}
	}
	if (hold[0] != 0) {
		struct SwSysStream *held = (struct SwSysStream *)hold[0];

		sceKernelLockLwMutex(&held->mutex, 1, NULL);
		held->state &= ~hold[1];
		sceKernelUnlockLwMutex(&held->mutex, 1);
	}
	return result;
}

/* hand out the middleware's next chunk: its address, its size and its type byte */
int dtcpIpSysReceive(void *object, void **buffer, SceSize *size, void *type, int timeout)
{
	struct SwSysStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int hold[2] = { 0, 0 };
	int ret;
	int result;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(hold, stream, 512) != 0) {
		result = 3;
	} else {
		for (;;) {
			message.command = SW_SYS_RECEIVE;
			message.channel = 1;
			message.size = 0;
			message.value0 = stream->id;
			ret = dtcpIpIpcRequest(stream->channel, &message);
			if (ret != 0)
				break;
			ret = message.result;
			if (ret != SW_SYS_STATUS_EMPTY && ret != 0)
				break;
			*buffer = (char *)stream->buffer2 + message.out0;
			*size = message.out1;
			if (type != NULL)
				*(unsigned char *)type = (unsigned char)message.out2;
			if (ret != 0)
				break;
			if (*size != 0)
				break;
			if (timeout == -1) {
				sceKernelDelayThread(1000);
			} else {
				if (timeout == 0) {
					ret = SW_SYS_STATUS_NO_WAIT;
					break;
				}
				sceKernelDelayThread(1000);
				--timeout;
			}
		}
		if (ret != 0) {
			stream->status = ret;
			result = dtcpIpMapIpcStatus2(ret);
		} else {
			result = 0;
		}
	}
	if (hold[0] != 0) {
		struct SwSysStream *held = (struct SwSysStream *)hold[0];

		sceKernelLockLwMutex(&held->mutex, 1, NULL);
		held->state &= ~hold[1];
		sceKernelUnlockLwMutex(&held->mutex, 1);
	}
	return result;
}

/* release the chunk the middleware handed out */
int dtcpIpSysRelease(void *object, SceSize size)
{
	struct SwSysStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int hold[2] = { 0, 0 };
	int ret;
	int result;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(hold, stream, 0x400) != 0) {
		result = 3;
	} else {
		message.command = SW_SYS_RELEASE;
		message.channel = 1;
		message.size = 0;
		message.value0 = stream->id;
		message.value1 = size;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret == 0)
			ret = message.result;
		if (ret != 0) {
			stream->status = ret;
			result = dtcpIpMapIpcStatus2(ret);
		} else {
			result = 0;
		}
	}
	if (hold[0] != 0) {
		struct SwSysStream *held = (struct SwSysStream *)hold[0];

		sceKernelLockLwMutex(&held->mutex, 1, NULL);
		held->state &= ~hold[1];
		sceKernelUnlockLwMutex(&held->mutex, 1);
	}
	return result;
}

/* take the middleware's next chunk to send: its address and its capacity */
int dtcpIpSysSend(void *object, void **buffer, SceSize *size, int timeout)
{
	struct SwSysStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int hold[2] = { 0, 0 };
	int ret;
	int result;
	SceSize chunkSize;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(hold, stream, 2048) != 0) {
		result = 3;
	} else {
		for (;;) {
			message.command = SW_SYS_SEND;
			message.channel = 2;
			message.size = 0;
			message.value0 = stream->id;
			ret = dtcpIpIpcRequest(stream->channel, &message);
			chunkSize = message.out1;
			if (ret != 0)
				break;
			ret = message.result;
			if (ret != SW_SYS_STATUS_EMPTY && ret != 0)
				break;
			*buffer = (char *)stream->buffer + message.out0;
			*size = message.out1;
			if (ret != 0)
				break;
			if (chunkSize != 0)
				break;
			if (timeout == -1) {
				sceKernelDelayThread(1000);
			} else {
				if (timeout == 0) {
					ret = SW_SYS_STATUS_NO_WAIT;
					break;
				}
				sceKernelDelayThread(1000);
				--timeout;
			}
		}
		if (ret != 0) {
			stream->status = ret;
			result = dtcpIpMapIpcStatus2(ret);
		} else {
			result = 0;
		}
	}
	if (hold[0] != 0) {
		struct SwSysStream *held = (struct SwSysStream *)hold[0];

		sceKernelLockLwMutex(&held->mutex, 1, NULL);
		held->state &= ~hold[1];
		sceKernelUnlockLwMutex(&held->mutex, 1);
	}
	return result;
}

/* tell the middleware how much of its chunk the caller filled */
int dtcpIpSysSendMessage(void *object, int messageValue)
{
	struct SwSysStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int hold[2] = { 0, 0 };
	int ret;
	int result;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(hold, stream, 4096) != 0) {
		result = 3;
	} else {
		message.command = SW_SYS_SEND_MESSAGE;
		message.channel = 2;
		message.size = 0;
		message.value0 = stream->id;
		message.value1 = messageValue;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret == 0)
			ret = message.result;
		if (ret != 0) {
			stream->status = ret;
			result = dtcpIpMapIpcStatus2(ret);
		} else {
			result = 0;
		}
	}
	if (hold[0] != 0) {
		struct SwSysStream *held = (struct SwSysStream *)hold[0];

		sceKernelLockLwMutex(&held->mutex, 1, NULL);
		held->state &= ~hold[1];
		sceKernelUnlockLwMutex(&held->mutex, 1);
	}
	return result;
}

/* abort the stream's transfer (the middleware may take a while to accept it) */
int dtcpIpSysAbort(void *object)
{
	struct SwSysStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int hold[2] = { 0, 0 };
	int ret;
	int result;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(hold, stream, 0x2000) != 0) {
		result = 3;
	} else {
		message.command = SW_SYS_ABORT;
		message.channel = 2;
		message.size = 0;
		message.value0 = stream->id;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret == 0) {
			ret = message.result;
			while (ret == SW_SYS_STATUS_LATER) {
				sceKernelDelayThread(1000);
				message.command = SW_SYS_ABORT;
				message.channel = 2;
				message.size = 0;
				message.value0 = stream->id;
				ret = dtcpIpIpcRequest(stream->channel, &message);
				if (ret != 0)
					break;
				ret = message.result;
			}
		}
		if (ret == 0) {
			stream->status = 0;
			result = 0;
		} else {
			stream->status = ret;
			result = dtcpIpMapIpcStatus2(ret);
		}
	}
	if (hold[0] != 0) {
		struct SwSysStream *held = (struct SwSysStream *)hold[0];

		sceKernelLockLwMutex(&held->mutex, 1, NULL);
		held->state &= ~hold[1];
		sceKernelUnlockLwMutex(&held->mutex, 1);
	}
	return result;
}

/* whether the middleware says the stream is open */
int dtcpIpSysIsStreamOpen(void *object)
{
	struct SwSysStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	message.command = SW_SYS_IS_STREAM_OPEN;
	message.channel = 3;
	message.size = 0;
	message.value0 = stream->id;
	ret = dtcpIpIpcRequest(stream->channel, &message);
	if (ret != 0 || (ret = message.result) != 0) {
		stream->status = ret;
		return dtcpIpMapIpcStatus2(ret);
	}
	return 0;
}

/* close the stream on the middleware: it may answer "later" while it is busy */
int dtcpIpSysClose(void *object)
{
	struct SwSysStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int hold[2] = { 0, 0 };
	int ret;
	int result;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(hold, stream, 8) != 0) {
		result = 3;
	} else {
		message.command = SW_SYS_WAIT_CLOSED;
		message.channel = 0;
		message.size = 0;
		message.value0 = stream->id;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret == 0) {
			ret = message.result;
			while (ret == SW_SYS_STATUS_LATER) {
				message.command = SW_SYS_POLL_CLOSED;
				message.channel = 0;
				message.size = 0;
				message.value0 = stream->id;
				ret = dtcpIpIpcRequest(stream->channel, &message);
				if (ret != 0)
					break;
				ret = message.result;
				if (ret != SW_SYS_STATUS_LATER)
					break;
				sceKernelDelayThread(1000);
			}
		}
		result = dtcpIpMapIpcStatus2(ret);
	}
	if (hold[0] != 0) {
		struct SwSysStream *held = (struct SwSysStream *)hold[0];

		sceKernelLockLwMutex(&held->mutex, 1, NULL);
		held->state &= ~hold[1];
		sceKernelUnlockLwMutex(&held->mutex, 1);
	}
	return result;
}
