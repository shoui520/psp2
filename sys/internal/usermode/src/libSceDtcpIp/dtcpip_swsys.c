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
#define SW_SYS_GET_INFO		10251	/* 0x280B */
#define SW_SYS_SET_PARAMETER		10252	/* 0x280C */
#define SW_SYS_RECEIVE			10254	/* 0x280E */
#define SW_SYS_RELEASE			10255	/* 0x280F */
#define SW_SYS_SEND			10256	/* 0x2810 */
#define SW_SYS_SEND_MESSAGE		10257	/* 0x2811 */
#define SW_SYS_ABORT			10258	/* 0x2812 */
#define SW_SYS_IS_STREAM_OPEN		10259	/* 0x2813 */
#define SW_SYS_CLOSE_STREAM		10260	/* 0x2814 */

/* the status the middleware answers while it is working on a request */
#define SW_SYS_STATUS_LATER		12007

/* the session client: the connection it wraps and the request state */
struct SwSysClient {
	struct SceDtcpIpIpcObject *ipc;	/* 0x00: the connection */
	int reserved0;			/* 0x04 */
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
	int reserved0[3];		/* 0x08 */
	int reserved1;			/* 0x14 */
	SceKernelLwMutexWork mutex;	/* 0x18: "sce_sws_mutex" */
	int status;			/* 0x38: the status of a failed request */
	int state;			/* 0x3C: the operations in flight */
	int id;				/* 0x40: the middleware's stream id */
	void *buffer;			/* 0x44: the shared buffer */
	void *buffer2;			/* 0x48: the stream's data area */
	int reserved2;			/* 0x4C */
};


/* the SW IPC's status, in the library's request statuses (dtcpip_swipc.c) */
int dtcpIpMapIpcStatus2(int status);

/* the stream object's state bits, at its own mutex (0x81004bc8) */
int dtcpIpSwAcquireStream(int *outAcquired, void *object, unsigned int state);

/*
 * take the stream object's mutex and its state bits: a conflicting operation already
 * in flight is refused (12004), and the caller releases the bits when it is done.
 */
int dtcpIpSwAcquireStream(int *outAcquired, void *object, unsigned int state)
{
	int acquired = (int)object;
	int bits = (int)state;

	outAcquired[0] = acquired;
	outAcquired[1] = bits;
	if (acquired == 0)
		return 0;
	sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired + 24), 1, NULL);
	{
		int object2 = outAcquired[0];

		if (bits > 0x3FF) {
			if (bits == 0x80000000) {
				if ((*(int *)(object2 + 60) & 2) != 0)
					goto busy;
				goto store;
			}
			if (bits != 0x2000 && bits != 4096 && bits != 2048 && bits != 1024)
				goto store;
			if ((*(int *)(object2 + 60) & 2) == 0)
				goto store;
			goto busy;
		}
		if (bits > 0x10) {
			if (bits > 0x40) {
				if (bits != 512)
					goto store;
			} else if (bits <= 0x3F) {
				goto store;
			}
			if ((*(int *)(object2 + 60) & 2) == 0)
				goto store;
			goto busy;
		}
		if (bits > 2) {
			if (bits > 0xF)
				goto checkZero;
		} else if (bits != 0) {
			goto checkZero;
		}
		goto store;
checkZero:
		if (*(int *)(object2 + 60) == 0)
			goto store;
busy:
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired + 24), 1);
		return 12004;
store:
		*(int *)(object2 + 60) |= outAcquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired + 24), 1);
	}
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
	int ret;
	int acquired[2] = { 0, 0 };
	int flags = 0;

	if (outClient == NULL)
		return 2;
	if (connection == NULL)
		return 2;

	client = dtcpIpAlloc(56);
	if (client == NULL)
		return 5;
	client->ipc = NULL;
	client->pending = 0;
	client->flushing = 0;
	client->state = 0;
	client->session = 0;
	if (sceKernelCreateLwMutex(&client->mutex, "sce_sws_mutex", 0x2000, 0, NULL) < 0) {
		ret = 5;
		goto fail;
	}
	if (dtcpIpIpcSetState(&acquired, (int)client, 1) != 0) {
		ret = 3;
		goto fail;
	}
	client->session = 0;
	message.command = SW_SYS_CONNECT;
	message.channel = 0;
	message.size = 0;
	if (dtcpIpIpcRequest(connection->stream, &message) != 0 || message.result != 0) {
		ret = dtcpIpMapIpcStatus2(message.result);
		goto fail;
	}
	client->session = message.value0;
	client->pending = 0;
	client->ipc = connection;
	*outClient = client;
	if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1, NULL);
		*(int *)(acquired[0] + 48) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1);
	}
	return 0;
fail:
	dtcpIpFree(client);
	*outClient = NULL;
	if (acquired[0] != 0)
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1, NULL);
		*(int *)(acquired[0] + 48) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1);
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
	int acquired[2] = { 0, 0 };
	struct SceDtcpIpIpcMessage message;
	int ret;
	int tries;
	int id;

	if (client == NULL)
		return 2;
	if (dtcpIpIpcSetState(&acquired, (int)client, 4) != 0) {
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1, NULL);
		*(int *)(acquired[0] + 48) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1);
	}
		return 3;
	}
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
				message.value0 = client->session;
				message.value1 = client->flushing;
				ret = dtcpIpIpcRequest(client->ipc->stream, &message);
				if (ret != 0)
					break;
				ret = message.result;
				if (message.result == 0) {
					*outId = (unsigned char)message.out0;
					client->pending = 0;
					if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
					return 0;
				}
				if (message.result != SW_SYS_STATUS_LATER)
					break;
				sceKernelDelayThread(1000);
			}
		}
	}
	client->pending = ret;
	id = dtcpIpMapIpcStatus2(ret);
	if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1, NULL);
		*(int *)(acquired[0] + 48) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1);
	}
	return id;
}

/* close the session client's session */
int dtcpIpSysCloseSession(struct SwSysClient *client)
{
	int acquired[2] = { 0, 0 };
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (client == NULL)
		return 2;
	if (dtcpIpIpcSetState(&acquired, (int)client, 2) != 0) {
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1, NULL);
		*(int *)(acquired[0] + 48) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1);
	}
		return 3;
	}
	message.command = SW_SYS_CLOSE;
	message.channel = 0;
	message.size = 0;
	message.value0 = client->session;
	if (dtcpIpIpcRequest(client->ipc->stream, &message) != 0) {
		ret = dtcpIpMapIpcStatus2(message.result);
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1, NULL);
		*(int *)(acquired[0] + 48) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1);
	}
	} else {
		if (message.result == 0) {
			if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1, NULL);
		*(int *)(acquired[0] + 48) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1);
	}
			ret = 0;
			goto done;
		}
		ret = dtcpIpMapIpcStatus2(message.result);
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1, NULL);
		*(int *)(acquired[0] + 48) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1);
	}
	}
done:
	sceKernelDeleteLwMutex(&client->mutex);
	dtcpIpFree(client);
	return ret;
}

/* the caller starts a flush: the stream's state says so */
int dtcpIpSysFlushStart(struct SwSysClient *client)
{
	if (client == NULL)
		return 2;
	client->flushing = 1;
	return 0;
}

/*
 * create the stream: a channel over the session's IPC client, its shared buffers, and
 * the middleware's open request (the caller's buffer sizes).
 */
int dtcpIpSysOpenStream(struct SwSysStream **outStream, struct SwSysClient *client, int *config)
{
	struct SwSysStream *stream;
	struct SceDtcpIpIpcMessage message;
	void *shared[4];
	int acquired[2] = { 0, 0 };
	int ret;
	int result;

	if (outStream == NULL)
		return 2;
	if (client == NULL)
		return 2;

	stream = dtcpIpAlloc(80);
	if (stream == NULL)
		return 5;
	stream->client = client;
	stream->channel = NULL;
	stream->reserved1 = 0;
	stream->status = 0;
	stream->state = 0;
	stream->id = 0;
	if (sceKernelCreateLwMutex(&stream->mutex, "sce_sws_mutex", 0x2000, 0, NULL) < 0) {
		ret = 5;
		goto fail;
	}
	stream->client = client;
	ret = dtcpIpIpcInitStream(&stream->channel, client->ipc->session, config[0],
				  config[1] + config[2], 1, 0);
	if (ret != 0) {
		ret = dtcpIpMapIpcStatus2(ret);
		goto fail;
	}
	ret = dtcpIpIpcInitShared(stream->channel, shared);
	if (ret != 0) {
		ret = dtcpIpMapIpcStatus2(ret);
		goto fail;
	}
	stream->buffer = shared[0];
	stream->buffer2 = (char *)shared[0] + config[1];
	if (dtcpIpSwAcquireStream(&acquired, stream, 1) != 0) {
		ret = 3;
		goto fail;
	}
	message.command = SW_SYS_OPEN_STREAM;
	message.channel = 0;
	message.size = 20;
	message.payload = config;
	message.value0 = client->session;
	if (dtcpIpIpcRequest(stream->channel, &message) != 0 || message.result != 0) {
		ret = dtcpIpMapIpcStatus2(message.result);
		goto fail;
	}
	stream->id = message.out0;
	stream->status = 0;
	*outStream = stream;
	if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1, NULL);
		*(int *)(acquired[0] + 48) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1);
	}
	return 0;
fail:
	dtcpIpFree(stream);
	*outStream = NULL;
	if (acquired[0] != 0)
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1, NULL);
		*(int *)(acquired[0] + 48) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 8), 1);
	}
	return ret;
}

/* close the stream */
int dtcpIpSysCloseStream(void *object)
{
	struct SwSysStream *stream = object;
	int acquired[2] = { 0, 0 };
	struct SceDtcpIpIpcMessage message;
	int ret;
	int result = 0;
	int channelResult;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(&acquired, stream, 2) != 0) {
		if (acquired[0] != 0) {
			sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
			*(int *)(acquired[0] + 60) &= ~acquired[1];
			sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
		}
		return 3;
	}
	message.command = 10247;
	message.channel = 0;
	message.size = 0;
	message.value0 = stream->id;
	if (dtcpIpIpcRequest(stream->channel, &message) != 0) {
		dtcpIpMapIpcStatus2(0);
		ret = acquired[0];
		if (acquired[0] != 0)
			goto release;
	} else if (message.result != 0) {
		dtcpIpMapIpcStatus2(message.result);
		ret = acquired[0];
		if (acquired[0] != 0)
			goto release;
	} else {
		ret = acquired[0];
		if (acquired[0] != 0)
			goto release;
	}
	goto freed;
release:
	sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
	*(int *)(acquired[0] + 60) &= ~acquired[1];
	sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
freed:
	channelResult = dtcpIpIpcFiniStream(stream->channel, ret);
	sceKernelDeleteLwMutex(&stream->mutex);
	dtcpIpFree(stream);
	return channelResult;
}

int dtcpIpSysGetInfo(void *object, void *info)
{
	struct SwSysStream *stream = object;
	int acquired[2] = { 0, 0 };
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(&acquired, stream, 16) != 0) {
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return 3;
	}
	message.command = SW_SYS_GET_INFO;
	message.channel = 0;
	message.size = 576;
	message.payload = info;
	message.value0 = stream->id;
	ret = dtcpIpIpcRequest(stream->channel, &message);
	if (ret != 0 || (ret = message.result) != 0) {
		stream->status = ret;
		ret = dtcpIpMapIpcStatus2(ret);
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return ret;
	}
	if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
	return 0;
}

/* set the stream's parameter (the middleware waits for it while it is busy) */
int dtcpIpSysSetParameter(void *object, int parameter)
{
	struct SwSysStream *stream = object;
	int acquired[2] = { 0, 0 };
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(&acquired, stream, 0x40) != 0) {
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return 3;
	}
	for (;;) {
		message.command = SW_SYS_SET_PARAMETER;
		message.value0 = stream->id;
		message.value1 = parameter;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret != 0)
			break;
		ret = message.result;
		if (message.result != SW_SYS_STATUS_LATER)
			break;
		sceKernelDelayThread(1000);
	}
	if (ret == 0) {
		stream->status = 0;
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return 0;
	}
	stream->status = ret;
	ret = dtcpIpMapIpcStatus2(ret);
	if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
	return ret;
}

/* receive the next chunk: the middleware fills the shared buffer and the caller gets it */
int dtcpIpSysReceive(void *object, void **buffer, SceSize *size, void *type, int timeout)
{
	struct SwSysStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	void *chunk;
	SceSize chunkSize;
	int acquired[2] = { 0, 0 };
	int ret;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(&acquired, stream, 512) != 0) {
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return 3;
	}
	for (;;) {
		message.command = SW_SYS_RECEIVE;
		message.channel = 1;
		message.value0 = stream->id;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret != 0 || (ret = message.result) != 0)
			break;
		ret = message.result;
		if (ret != SW_SYS_STATUS_LATER)
			break;
		sceKernelDelayThread(timeout);
	}
	if (ret != 0) {
		stream->status = ret;
		ret = dtcpIpMapIpcStatus2(ret);
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return ret;
	}
	chunk = (char *)stream->buffer2 + message.out0;
	chunkSize = message.out1;
	*(unsigned char *)type = (unsigned char)message.out2;
	*buffer = chunk;
	*size = chunkSize;
	if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
	return 0;
}

/* release the chunk the caller has read */
int dtcpIpSysRelease(void *object, SceSize size)
{
	struct SwSysStream *stream = object;
	int acquired[2] = { 0, 0 };
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(&acquired, stream, 0x400) != 0) {
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return 3;
	}
	message.command = SW_SYS_RELEASE;
	message.channel = 1;
	message.value0 = stream->id;
	message.value1 = size;
	ret = dtcpIpIpcRequest(stream->channel, &message);
	if (ret != 0 || (ret = message.result) != 0) {
		stream->status = ret;
		ret = dtcpIpMapIpcStatus2(ret);
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return ret;
	}
	if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
	return 0;
}

/* send data to the peer (the middleware copies it from the shared buffer) */
int dtcpIpSysSend(void *object, int a1, int a2, int a3)
{
	struct SwSysStream *stream = object;
	int acquired[2] = { 0, 0 };
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(&acquired, stream, 2048) != 0) {
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return 3;
	}
	for (;;) {
		message.command = SW_SYS_SEND;
		message.channel = 2;
		message.value0 = stream->id;
		message.value1 = a1;
		message.value2 = a2;
		message.value3 = a3;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret != 0)
			break;
		ret = message.result;
		if (message.result != SW_SYS_STATUS_LATER)
			break;
		sceKernelDelayThread(1000);
	}
	if (ret != 0) {
		stream->status = ret;
		ret = dtcpIpMapIpcStatus2(ret);
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return ret;
	}
	*(int *)a2 = message.out0;
	*(int *)a3 = message.out0 + message.out1;
	if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
	return 0;
}

/* send a message to the peer */
int dtcpIpSysSendMessage(void *object, int messageValue)
{
	struct SwSysStream *stream = object;
	int acquired[2] = { 0, 0 };
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(&acquired, stream, 0x1000) != 0) {
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return 3;
	}
	for (;;) {
		message.command = SW_SYS_SEND_MESSAGE;
		message.channel = 2;
		message.value0 = stream->id;
		message.value1 = messageValue;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret != 0)
			break;
		ret = message.result;
		if (message.result != SW_SYS_STATUS_LATER)
			break;
		sceKernelDelayThread(1000);
	}
	if (ret != 0) {
		stream->status = ret;
		ret = dtcpIpMapIpcStatus2(ret);
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return ret;
	}
	if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
	return 0;
}

/* abort the stream's transfer */
int dtcpIpSysAbort(void *object)
{
	struct SwSysStream *stream = object;
	int acquired[2] = { 0, 0 };
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(&acquired, stream, 0x2000) != 0) {
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return 3;
	}
	for (;;) {
		message.command = SW_SYS_ABORT;
		message.channel = 2;
		message.value0 = stream->id;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret != 0)
			break;
		ret = message.result;
		if (message.result != SW_SYS_STATUS_LATER)
			break;
		sceKernelDelayThread(1000);
	}
	if (ret == 0) {
		stream->status = 0;
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return 0;
	}
	stream->status = ret;
	ret = dtcpIpMapIpcStatus2(ret);
	if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
	return ret;
}

/* whether the stream is open: the middleware says so */
int dtcpIpSysIsStreamOpen(void *object)
{
	struct SwSysStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	message.command = SW_SYS_IS_STREAM_OPEN;
	message.channel = 3;
	message.value0 = stream->id;
	ret = dtcpIpIpcRequest(stream->channel, &message);
	if (ret != 0 || (ret = message.result) != 0) {
		stream->status = ret;
		return dtcpIpMapIpcStatus2(ret);
	}
	return 0;
}

/* close the session's stream (the middleware's request; 0x8100571e) */
int dtcpIpSysClose(void *object)
{
	struct SwSysStream *stream = object;
	int acquired[2] = { 0, 0 };
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	if (dtcpIpSwAcquireStream(&acquired, stream, 8) != 0) {
		if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
		return 3;
	}
	message.command = SW_SYS_CLOSE_STREAM;
	message.channel = 0;
	message.size = 0;
	message.value0 = stream->id;
	ret = dtcpIpIpcRequest(stream->channel, &message);
	if (ret == 0) {
		ret = message.result;
		if (message.result == 0 || message.result == SW_SYS_STATUS_LATER) {
			if (message.result != SW_SYS_STATUS_LATER) {
				if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
				return 0;
			}
			for (;;) {
				message.command = SW_SYS_CLOSE_STREAM;
				message.value0 = stream->id;
				ret = dtcpIpIpcRequest(stream->channel, &message);
				if (ret != 0)
					break;
				ret = message.result;
				if (message.result == 0) {
					if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
					return 0;
				}
				if (message.result != SW_SYS_STATUS_LATER)
					break;
				sceKernelDelayThread(1000);
			}
		}
	}
	stream->status = ret;
	ret = dtcpIpMapIpcStatus2(ret);
	if (acquired[0] != 0) {
		sceKernelLockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1, NULL);
		*(int *)(acquired[0] + 60) &= ~acquired[1];
		sceKernelUnlockLwMutex((SceKernelLwMutexWork *)(acquired[0] + 24), 1);
	}
	sceKernelDeleteLwMutex(&stream->mutex);
	dtcpIpFree(stream);
	return ret;
}
