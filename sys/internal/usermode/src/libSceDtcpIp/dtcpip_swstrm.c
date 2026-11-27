/*
 * SceDtcpIp: the stream client of the SW IPC. It is the same protocol as the system
 * client, on its own object and command numbers: the session's connect and uri, then
 * the stream's chunks in both directions (receive/release, send/send-message), the
 * abort and the close.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <dtcpip/error.h>
#include <libkernel/libkernel.h>
#include "../../../../../internal/include/libkernel/libkernel.h"
#include "dtcpip_internal.h"

/* the stream IPC's commands */
#define SW_STRM_CONNECT			8705	/* 0x2201 */
#define SW_STRM_CLOSE			8706	/* 0x2202 */
#define SW_STRM_SET_URI			8707	/* 0x2203 */
#define SW_STRM_GET_URI_RESULT		8708	/* 0x2204 */
#define SW_STRM_OPEN_STREAM		8710	/* 0x2206 */
#define SW_STRM_CLOSE_STREAM		8711	/* 0x2207 */
#define SW_STRM_GET_INFO		8715	/* 0x220B */
#define SW_STRM_SET_PARAMETER		8716	/* 0x220C */
#define SW_STRM_RECEIVE			8718	/* 0x220E */
#define SW_STRM_RELEASE			8719	/* 0x220F */
#define SW_STRM_SEND			8720	/* 0x2210 */
#define SW_STRM_SEND_MESSAGE		8721	/* 0x2211 */
#define SW_STRM_ABORT			8722	/* 0x2212 */
#define SW_STRM_IS_OPEN			8723	/* 0x2213 */

/* the status the middleware answers while it is working on a request */
#define SW_STRM_STATUS_LATER		12007
#define SW_STRM_STATUS_EMPTY		12401

/* the stream engine's session client: the connection it wraps and its request state */
struct SwStrmClient {
	struct SceDtcpIpIpcObject *ipc;	/* 0x00: the connection */
	int pad;			/* 0x04 */
	SceKernelLwMutexWork mutex;	/* 0x08: "sce_sws_mutex" */
	int pending;			/* 0x28: the status of a failed request */
	int flushing;			/* 0x2C: a flush is pending */
	int state;			/* 0x30: the operations in flight */
	int session;			/* 0x34: the middleware's session handle */
};

/* the stream engine's stream: its channel and its shared buffers */
struct SwStrmStream {
	struct SwStrmClient *client;	/* 0x00: the session client */
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

/* the SW IPC's status, in the library's request statuses (dtcpip_swstrm.c's table) */
int dtcpIpMapIpcStatus3(int status);

/* create the stream client: a 56-byte object over the connection, its mutex, and the
 * connect request that opens the middleware's session */
int dtcpIpStrmInit(struct SwStrmClient **outClient, struct SceDtcpIpIpcObject *connection)
{
	struct SwStrmClient *client;
	struct SceDtcpIpIpcMessage message;
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
		dtcpIpFree(client);
		*outClient = NULL;
		return 5;
	}
	client->flushing = 0;
	message.command = SW_STRM_CONNECT;
	message.channel = 0;
	message.size = 0;
	ret = dtcpIpIpcRequest(connection->stream, &message);
	if (ret != 0) {
		ret = dtcpIpMapIpcStatus3(ret);
	} else if (message.result != 0) {
		ret = dtcpIpMapIpcStatus3(message.result);
	} else {
		client->session = message.out0;
		client->pending = 0;
		client->ipc = connection;
		*outClient = client;
		return 0;
	}
	sceKernelDeleteLwMutex(&client->mutex);
	dtcpIpFree(client);
	*outClient = NULL;
	return ret;
}

/* set the stream's uri (the middleware answers once it has it) */
int dtcpIpStrmSetUri(struct SwStrmClient *client, const char *uri, unsigned short port)
{
	struct SceDtcpIpIpcMessage message;
	int ret;
	int tries;

	if (client == NULL)
		return 2;
	client->flushing = 0;
	message.command = SW_STRM_SET_URI;
	message.channel = 0;
	message.size = dtcpIpStrnlen(uri) + 1;
	message.payload = (void *)uri;
	message.value0 = client->session;
	message.value1 = port;
	ret = dtcpIpIpcRequest(client->ipc->stream, &message);
	if (ret == 0) {
		ret = message.result;
		if (message.result == 0 || message.result == SW_STRM_STATUS_LATER) {
			for (;;) {
				message.command = SW_STRM_GET_URI_RESULT;
				message.channel = 0;
				message.size = 0;
				message.value0 = client->session;
				message.value1 = client->flushing;
				ret = dtcpIpIpcRequest(client->ipc->stream, &message);
				if (ret != 0)
					break;
				ret = message.result;
				if (message.result == 0) {
					client->pending = 0;
					return 0;
				}
				if (message.result != SW_STRM_STATUS_LATER)
					break;
				sceKernelDelayThread(1000);
			}
		}
	}
	client->pending = ret;
	return dtcpIpMapIpcStatus3(ret);
}

/* close the stream client */
int dtcpIpStrmCloseSession(struct SwStrmClient *client)
{
	struct SceDtcpIpIpcMessage message;
	int ret;
	int result = 0;

	if (client == NULL)
		return 2;
	message.command = SW_STRM_CLOSE;
	message.channel = 0;
	message.size = 0;
	message.value0 = client->session;
	ret = dtcpIpIpcRequest(client->ipc->stream, &message);
	if (ret != 0)
		result = dtcpIpMapIpcStatus3(ret);
	else if (message.result != 0)
		result = dtcpIpMapIpcStatus3(message.result);
	sceKernelDeleteLwMutex(&client->mutex);
	dtcpIpFree(client);
	return result;
}

/* the caller starts a flush: the stream's state says so */
int dtcpIpStrmFlushStart(struct SwStrmClient *client)
{
	if (client == NULL)
		return 2;
	client->flushing = 1;
	return 0;
}

/* create the stream: a channel over the session's IPC client, its shared buffers, and
 * the middleware's open request (the caller's buffer sizes and affinity) */
int dtcpIpStrmOpenStream(struct SwStrmStream **outStream, struct SwStrmClient *client, int *config)
{
	struct SwStrmStream *stream;
	struct SceDtcpIpIpcMessage message;
	void *shared[4];
	int ret;

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
				  config[1] + config[2], 1, config[5]);
	if (ret != 0) {
		ret = dtcpIpMapIpcStatus3(ret);
	} else if ((ret = dtcpIpIpcInitShared(stream->channel, shared)) != 0) {
		ret = dtcpIpMapIpcStatus3(ret);
	} else {
		stream->buffer = shared[1];
		stream->buffer2 = (char *)shared[1] + config[1];
		message.command = SW_STRM_OPEN_STREAM;
		message.channel = 0;
		message.size = 24;
		message.payload = config;
		message.value0 = client->session;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret != 0) {
			ret = dtcpIpMapIpcStatus3(ret);
		} else if (message.result != 0) {
			ret = dtcpIpMapIpcStatus3(message.result);
		} else {
			stream->id = message.out0;
			stream->status = 0;
			*outStream = stream;
			return 0;
		}
	}
	sceKernelDeleteLwMutex(&stream->mutex);
	dtcpIpFree(stream);
	*outStream = NULL;
	return ret;
}

/* close the stream */
int dtcpIpStrmCloseStream(void *object)
{
	struct SwStrmStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	message.command = SW_STRM_CLOSE_STREAM;
	message.channel = 0;
	message.size = 0;
	message.value0 = stream->id;
	ret = dtcpIpIpcRequest(stream->channel, &message);
	if (ret != 0)
		dtcpIpMapIpcStatus3(ret);
	else if (message.result != 0)
		dtcpIpMapIpcStatus3(message.result);
	ret = dtcpIpIpcFiniStream(stream->channel, 0);
	sceKernelDeleteLwMutex(&stream->mutex);
	dtcpIpFree(stream);
	return ret;
}

/* get the stream's info from the middleware (the caller's 56-byte buffer) */
int dtcpIpStrmGetInfo(void *object, void *info)
{
	struct SwStrmStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	message.command = SW_STRM_GET_INFO;
	message.channel = 0;
	message.size = 56;
	message.payload = info;
	message.value0 = stream->id;
	ret = dtcpIpIpcRequest(stream->channel, &message);
	if (ret != 0 || (ret = message.result) != 0) {
		stream->status = ret;
		return dtcpIpMapIpcStatus3(ret);
	}
	return 0;
}

/* set the stream's parameter (the middleware waits for it while it is busy) */
int dtcpIpStrmSetParameter(void *object, int parameter)
{
	struct SwStrmStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	for (;;) {
		message.command = SW_STRM_SET_PARAMETER;
		message.channel = 0;
		message.size = 0;
		message.value0 = stream->id;
		message.value1 = parameter;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret != 0)
			break;
		ret = message.result;
		if (message.result != SW_STRM_STATUS_LATER)
			break;
		sceKernelDelayThread(1000);
	}
	if (ret == 0) {
		stream->status = 0;
		return 0;
	}
	stream->status = ret;
	return dtcpIpMapIpcStatus3(ret);
}

/*
 * receive the next chunk: the middleware fills the shared buffer; the caller gets the
 * chunk's address and size and its type byte. timeout counts the empty answers
 * (-1 waits forever, 0 fails at once).
 */
int dtcpIpStrmReceive(void *object, void **buffer, SceSize *size, void *type, int timeout)
{
	struct SwStrmStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int ret;
	int dataSize;
	int empty;
	int chunk;

	if (stream == NULL)
		return 2;
	for (;;) {
		message.command = SW_STRM_RECEIVE;
		message.channel = 1;
		message.size = 0;
		message.value0 = stream->id;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret != 0)
			break;
		ret = message.result;
		if (message.result != SW_STRM_STATUS_EMPTY && message.result != 0)
			break;
		chunk = message.out0;
		dataSize = message.out1;
		empty = message.out2;
		*buffer = (char *)stream->buffer2 + chunk;
		*size = dataSize;
		if (type != NULL) {
			*(unsigned char *)type = (unsigned char)empty;
			empty = *size;
		}
		if (ret != 0) {
			stream->status = ret;
			return dtcpIpMapIpcStatus3(ret);
		}
		if (empty != 0)
			return 0;
		if (timeout == -1) {
			sceKernelDelayThread(1000);
		} else {
			if (timeout == 0) {
				ret = 12006;
				stream->status = ret;
				return dtcpIpMapIpcStatus3(ret);
			}
			sceKernelDelayThread(1000);
			--timeout;
		}
	}
	stream->status = ret;
	return dtcpIpMapIpcStatus3(ret);
}

/* release the chunk the caller has read */
int dtcpIpStrmRelease(void *object, SceSize size)
{
	struct SwStrmStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	message.command = SW_STRM_RELEASE;
	message.channel = 1;
	message.size = 0;
	message.value0 = stream->id;
	message.value1 = size;
	ret = dtcpIpIpcRequest(stream->channel, &message);
	if (ret != 0 || (ret = message.result) != 0) {
		stream->status = ret;
		return dtcpIpMapIpcStatus3(ret);
	}
	return 0;
}

/* take the peer's data: the caller gets its address and size */
int dtcpIpStrmSend(void *object, void **buffer, SceSize *size, int timeout)
{
	struct SwStrmStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int ret;
	int chunk;
	int length;

	if (stream == NULL)
		return 2;
	for (;;) {
		message.command = SW_STRM_SEND;
		message.channel = 2;
		message.size = 0;
		message.value0 = stream->id;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret != 0)
			break;
		ret = message.result;
		if (message.result != SW_STRM_STATUS_EMPTY && message.result != 0)
			break;
		chunk = message.out0;
		length = message.out1;
		*size = length;
		*buffer = (char *)stream->buffer + chunk;
		if (ret != 0) {
			stream->status = ret;
			return dtcpIpMapIpcStatus3(ret);
		}
		if (length != 0)
			return 0;
		if (timeout == -1) {
			sceKernelDelayThread(1000);
		} else {
			if (timeout == 0) {
				ret = 12006;
				stream->status = ret;
				return dtcpIpMapIpcStatus3(ret);
			}
			sceKernelDelayThread(1000);
			--timeout;
		}
	}
	stream->status = ret;
	return dtcpIpMapIpcStatus3(ret);
}

/* send a message to the peer */
int dtcpIpStrmSendMessage(void *object, int messageValue)
{
	struct SwStrmStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	message.command = SW_STRM_SEND_MESSAGE;
	message.channel = 2;
	message.size = 0;
	message.value0 = stream->id;
	message.value1 = messageValue;
	ret = dtcpIpIpcRequest(stream->channel, &message);
	if (ret != 0 || (ret = message.result) != 0) {
		stream->status = ret;
		return dtcpIpMapIpcStatus3(ret);
	}
	return 0;
}

/* abort the stream's transfer */
int dtcpIpStrmAbort(void *object)
{
	struct SwStrmStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	for (;;) {
		message.command = SW_STRM_ABORT;
		message.channel = 2;
		message.size = 0;
		message.value0 = stream->id;
		ret = dtcpIpIpcRequest(stream->channel, &message);
		if (ret != 0)
			break;
		ret = message.result;
		if (message.result != SW_STRM_STATUS_LATER)
			break;
		sceKernelDelayThread(1000);
	}
	if (ret == 0) {
		stream->status = 0;
		return 0;
	}
	stream->status = ret;
	return dtcpIpMapIpcStatus3(ret);
}

/* whether the stream is open: the middleware says so */
int dtcpIpStrmIsStreamOpen(void *object)
{
	struct SwStrmStream *stream = object;
	struct SceDtcpIpIpcMessage message;
	int ret;

	if (stream == NULL)
		return 2;
	message.command = SW_STRM_IS_OPEN;
	message.channel = 3;
	message.size = 0;
	message.value0 = stream->id;
	ret = dtcpIpIpcRequest(stream->channel, &message);
	if (ret != 0 || (ret = message.result) != 0) {
		stream->status = ret;
		return dtcpIpMapIpcStatus3(ret);
	}
	return 0;
}

/* the middleware's status word, in the stream client's request statuses */
int dtcpIpMapIpcStatus3(int status)
{
	int result;

	if (status < 1000 || status > 1652) {
		if (status > 12000) {
			if (status > 12205) {
				if (status > 12402) {
#if FW_VERSION == 0x03630011
					if (status > 13000 && status <= 13008) {
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
						}
						return result;
					}
#else
					if (status > 13000 && status <= 13017) {
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
						return result;
					}
#endif
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
					return result;
				}
				if (status > 12101) {
					if (status > 12200) {
						switch (status) {
						case 12201:
							result = 8;
							break;
						case 12202:
							result = 9;
							break;
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
