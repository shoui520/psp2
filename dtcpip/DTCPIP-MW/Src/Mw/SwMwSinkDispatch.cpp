/*
 * swagner's request dispatcher for the sink (DTCPIP-MW/Src/Mw/SwMwSinkDispatch.cpp,
 * from its assert and error strings): the stream engine's commands (0x2201..0x2213,
 * the library's SW_STRM protocol) and the periodic 0x1201 stream connect, each handler
 * calling the sink or its stream (SwMwSink.cpp, SwMwSinkStream.cpp) and answering
 * through the server's reply.
 */

#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include "../Comm/vita/SwMwCommServer.h"
#include "SwMwSync.h"

#define SWMW_SINK_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwSinkDispatch.cpp"

extern "C" {
int swMwTrace(const char *format, ...);
int SwsSetToolMode(int value);

/* the sink object (SwMwSink.cpp) */
int SwMwSinkCreate(int *sink, int device);
int SwMwSinkDestroy(void *sink);
int SwMwSinkSetUri(void *sink, void *uri, int size);
int SwMwSinkGetUriResult(void *sink, int timeout);

/* the sink's stream object (SwMwSinkStream.cpp) */
struct SwMwGate *SwMwSinkStreamGate(void *stream);
int SwMwSinkStreamCreate(int *stream, void *sink, const int *window);
int SwMwSinkStreamDestroy(void *stream);
int SwMwSinkStreamConnect(void *stream);
int SwMwSinkStreamGetInfo(void *stream, void *info);
int SwMwSinkStreamSetParameter(void *stream, int parameter);
int SwMwSinkStreamPoll(void *stream);
int SwMwSinkStreamReceive(void *stream, int *offset, int *size, unsigned char *type);
int SwMwSinkStreamRelease(void *stream, int size);
int SwMwSinkStreamSend(void *stream, int *offset, int *size);
int SwMwSinkStreamSendMessage(void *stream, int message);
int SwMwSinkStreamAbort(void *stream);
int SwMwSinkStreamIsOpen(void *stream);
void SwMwSinkStreamGetWindow(void *stream, int *window);
}

/* the information block's first 16 bytes, copied whole */
struct SwMwSinkStreamInfoHeader {
	unsigned int word[4];
};

/* the stream's information block (0x38 bytes the library and the middleware share) */
struct SwMwSinkStreamInfo {
	struct SwMwSinkStreamInfoHeader header;	/* 0x00 */
	unsigned char second[16];	/* 0x10 */
	unsigned char third[16];	/* 0x20 */
	int value0;			/* 0x30 */
	int value1;			/* 0x34 */
};

/* the stream's shared-memory windows, as the middleware holds them */
struct SwMwSinkStreamWindow {
	unsigned char *receiveBase;	/* 0x00: the receive window's base */
	int sendSize;			/* 0x04 */
	unsigned char *sendBase;	/* 0x08: the shared memory's base */
	int receiveSize;		/* 0x0C */
	int reserved;			/* 0x10: the library's config[3] */
	int blockSize;			/* 0x14 */
};

/* answer a sink request with a status and no payload (0x8100cf4c) */
extern "C" int SwMwSinkReplyStatus(struct SwMwCommChannel *channel,
				   struct SwMwIpcMessage *request, int status)
{
	request->result = status;
	request->outSize = 0;
	request->outPayload = NULL;
	return SwMwCommServerReplyError(channel, request);
}

/* turn a sink handle back into its object (0x8100d30c) */
extern "C" int SwMwSinkDecodeHandle(int handle, int *sink)
{
	*sink = (int)((unsigned int)handle ^ 0x10AAAA55);
	return 0;
}

/* turn a stream handle back into its object (0x8100da00) */
extern "C" int SwMwSinkStreamDecodeHandle(int handle, int *stream)
{
	*stream = (int)((unsigned int)handle ^ 0x10AAAA55);
	return 0;
}

/* the stream gate's shared side, taken for a stream command (0x8107b964/0x8107b984) */
extern "C" struct SwMwGate **SwMwSinkStreamLock(struct SwMwGate **held, void *stream)
{
	held[0] = SwMwSinkStreamGate(stream);
	SwMwGateSharedLock(held[0]);
	return held;
}

extern "C" struct SwMwGate **SwMwSinkStreamUnlock(struct SwMwGate **held)
{
	SwMwGateSharedUnlock(held[0]);
	return held;
}

/* the stream gate's exclusive side, taken for the periodic connect (0x8107b998/0x8107b9b8) */
extern "C" struct SwMwGate **SwMwSinkConnectLock(struct SwMwGate **held, void *stream)
{
	held[0] = SwMwSinkStreamGate(stream);
	SwMwGateExclusiveLock(held[0]);
	return held;
}

extern "C" struct SwMwGate **SwMwSinkConnectUnlock(struct SwMwGate **held)
{
	SwMwGateExclusiveUnlock(held[0]);
	return held;
}

/* 0x2201: create the channel's sink from its device */
extern "C" int SwMwSinkConnectCommand(struct SwMwCommChannel *channel,
				      struct SwMwIpcMessage *request, void *argument)
{
	int status;
	struct SwMwManagerContextBlock context;
	int *values;
	int sink;

	SwMwManagerBuildContext(&context, channel);
	values = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 198);
	} else if (*values != 0) {
		SwsSetToolMode(0);
		status = SwMwSinkCreate(&sink, *values);
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_SINK_SOURCE, 211);
		}
		request->result = status;
		request->outSize = 0;
		request->outPayload = NULL;
		request->out0 = sink ^ 0x10AAAA55;
		status = SwMwCommServerReplyError(channel, request);
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_SINK_SOURCE, 219);
		}
	} else {
		SwMwSinkReplyStatus(channel, request, 12004);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_SINK_SOURCE, 204);
	}
	return 0;
}

/* 0x2202: destroy the request's sink */
extern "C" int SwMwSinkCloseCommand(struct SwMwCommChannel *channel,
				    struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int sink;
	int failed;

	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 233);
	} else {
		status = SwMwSinkDecodeHandle(request->value0, &sink);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 238);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 240);
		} else {
			status = SwMwSinkDestroy((void *)sink);
			if (status != 0) {
				swMwTrace("[SWMW] Error %d in %s:%d\n", status,
					  SWMW_SINK_SOURCE, 245);
				failed = 1;
			}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
			if (status != 0) {
				swMwTrace("[SWMW] Error %d in %s:%d\n", status,
					  SWMW_SINK_SOURCE, 252);
				failed = 1;
			}
		}
	}
	return 0;
}

/* 0x2203: give the sink its URI */
extern "C" int SwMwSinkSetUriCommand(struct SwMwCommChannel *channel,
				     struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int *payload;
	int sink;
	int failed;

	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 266);
	} else {
		status = SwMwSinkDecodeHandle(request->value0, &sink);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 271);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 273);
		} else if (request->size != 0) {
			unsigned short port = request->value1;

			payload = (int *)request->payload;
			status = SwMwSinkSetUri((void *)sink, payload, port);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 287);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 295);
					failed = 1;
				}
		} else {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 279);
		}
	}
	return 0;
}

/* 0x2204: the URI's result for the sink */
extern "C" int SwMwSinkGetUriResultCommand(struct SwMwCommChannel *channel,
					   struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int sink;
	int failed;

	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 307);
	} else {
		status = SwMwSinkDecodeHandle(request->value0, &sink);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 312);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 314);
		} else {
			status = SwMwSinkGetUriResult((void *)sink, request->value1);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 319);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 326);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2205: sync the sink's state */
extern "C" int SwMwSinkSyncCommand(struct SwMwCommChannel *channel,
				   struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int sink;
	int failed;

	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 338);
	} else {
		status = SwMwSinkDecodeHandle(request->value0, &sink);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 343);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 345);
		} else {
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 358);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2206: open the sink's stream on the request's shared-memory windows */
extern "C" int SwMwSinkOpenStreamCommand(struct SwMwCommChannel *channel,
					 struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int *payload;
	int *values;
	int sink;
	int stream;
	int window[6];
	struct SwMwCommRequestView view;
	struct SwMwManagerContextBlock context;
	int failed;

	SwMwManagerBuildContext(&context, channel);
	values = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 377);
	} else {
		SwMwCommServerGetRequest((const struct SwMwCommRequest *)channel, view.value);
		status = SwMwSinkDecodeHandle(request->value0, &sink);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 384);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 386);
		} else if (request->size != 0) {
			payload = (int *)request->payload;
			window[2] = view.value[1];
			window[3] = payload[1];
			window[0] = view.value[1] + payload[1];
			window[1] = payload[2];
			window[4] = payload[3];
			window[5] = payload[4];
			status = SwMwSinkStreamCreate(&stream, (void *)sink, window);
			if (status != 0) {
				swMwTrace("[SWMW] Error %d in %s:%d\n", status,
					  SWMW_SINK_SOURCE, 406);
				failed = 1;
			} else {
				values[1] = (int)sink;
				values[2] = stream;
				SwMwCommServerSetRequestValue((struct SwMwCommRequest *)channel,
							      context.head);
			}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			request->out0 = stream ^ 0x10AAAA55;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 419);
					failed = 1;
				}
		} else {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 392);
		}
	}
	return 0;
}

/* 0x2207: close the request's stream */
extern "C" int SwMwSinkCloseStreamCommand(struct SwMwCommChannel *channel,
					  struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int *values;
	int stream;
	struct SwMwManagerContextBlock context;
	int failed;

	SwMwManagerBuildContext(&context, channel);
	values = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 434);
	} else {
		status = SwMwSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 439);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 441);
		} else {
			status = SwMwSinkStreamDestroy((void *)stream);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 446);
					failed = 1;
				}
			values[1] = 0;
			values[2] = 0;
			SwMwCommServerSetRequestValue((struct SwMwCommRequest *)channel,
						      context.head);
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 457);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x220B: the stream's information */
extern "C" int SwMwSinkGetInfoCommand(struct SwMwCommChannel *channel,
				      struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int *payload;
	int stream;
	struct SwMwSinkStreamInfo info;
	int failed;

	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 471);
	} else {
		status = SwMwSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 476);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 478);
		} else if (request->size != 0) {
			payload = (int *)request->payload;
			memset(&info, 0, sizeof(info));
			info.header = *(const struct SwMwSinkStreamInfoHeader *)payload;
			info.value0 = payload[12];
			memcpy(info.third, payload + 8, 16);
			memcpy(info.second, payload + 4, 16);
			info.value1 = payload[13];
			status = SwMwSinkStreamGetInfo((void *)stream, &info);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 500);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 507);
					failed = 1;
				}
		} else {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 484);
		}
	}
	return 0;
}

/* 0x220C: set one of the stream's parameters */
extern "C" int SwMwSinkSetParameterCommand(struct SwMwCommChannel *channel,
					   struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 519);
	} else {
		status = SwMwSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 524);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 526);
		} else {
			status = SwMwSinkStreamSetParameter((void *)stream, request->value1);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 531);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 538);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x220D: poll the stream */
extern "C" int SwMwSinkPollCommand(struct SwMwCommChannel *channel,
				   struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 550);
	} else {
		status = SwMwSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 555);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 557);
		} else {
			status = SwMwSinkStreamPoll((void *)stream);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 562);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 569);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x220E: receive a chunk from the stream */
extern "C" int SwMwSinkReceiveCommand(struct SwMwCommChannel *channel,
				      struct SwMwIpcMessage *request, void *argument)
{
	int status;
	unsigned char type;
	int stream;
	int offset;
	int size;
	int window[6];
	int failed;

	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 585);
	} else {
		status = SwMwSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 590);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 592);
		} else {
			status = SwMwSinkStreamReceive((void *)stream, &offset, &size, &type);
				if (status != 12007 && status != 12401 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 597);
					failed = 1;
				}
			SwMwSinkStreamGetWindow((void *)stream, window);
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			request->out0 = offset - window[0];
			request->out1 = size;
			request->out2 = type;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 610);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x220F: release the bytes the caller has read */
extern "C" int SwMwSinkReleaseCommand(struct SwMwCommChannel *channel,
				      struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 622);
	} else {
		status = SwMwSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 627);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 629);
		} else {
			status = SwMwSinkStreamRelease((void *)stream, request->value1);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 634);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 641);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2210: take the peer's chunk from the stream */
extern "C" int SwMwSinkSendCommand(struct SwMwCommChannel *channel,
				   struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int offset;
	int size;
	int window[6];
	int failed;

	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 657);
	} else {
		status = SwMwSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 662);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 664);
		} else {
			status = SwMwSinkStreamSend((void *)stream, &offset, &size);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 669);
					failed = 1;
				}
			SwMwSinkStreamGetWindow((void *)stream, window);
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			request->out0 = offset - window[2];
			request->out1 = size;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 681);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2211: send a message to the stream's peer */
extern "C" int SwMwSinkSendMessageCommand(struct SwMwCommChannel *channel,
					  struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 693);
	} else {
		status = SwMwSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 698);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 700);
		} else {
			status = SwMwSinkStreamSendMessage((void *)stream, request->value1);
				if (status != 12015 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 705);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 712);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2212: abort the stream's transfer */
extern "C" int SwMwSinkAbortCommand(struct SwMwCommChannel *channel,
				    struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 724);
	} else {
		status = SwMwSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 729);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 731);
		} else {
			status = SwMwSinkStreamAbort((void *)stream);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 736);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 743);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2213: the stream's open state */
extern "C" int SwMwSinkIsOpenCommand(struct SwMwCommChannel *channel,
				     struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 755);
	} else {
		status = SwMwSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status,
				  SWMW_SINK_SOURCE, 760);
			failed = 1;
		}
		if (failed != 0) {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
				  SWMW_SINK_SOURCE, 762);
		} else {
			status = SwMwSinkStreamIsOpen((void *)stream);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 767);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_SINK_SOURCE, 774);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x1201: the stream's periodic connect */
extern "C" int SwMwSinkStreamConnectCommand(struct SwMwCommChannel *channel,
					    struct SwMwIpcMessage *request, void *argument)
{
	int stream;
	int status;

	stream = request->value0;
	if (stream != 0) {
		status = SwMwSinkStreamConnect((void *)stream);
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_SINK_SOURCE, 790);
			return 1;
		}
	} else {
		SwMwSinkReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_SINK_SOURCE, 785);
	}
	return status;
}

/* the sink's command dispatcher (0x8100cf7a) */
extern "C" int SwMwSinkDispatch(struct SwMwCommChannel *channel,
				struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *values;
	int command = request->command;

	if (channel == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_SINK_SOURCE, 91);
	if (request == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_SINK_SOURCE, 92);
	SwMwManagerBuildContext(&context, channel);
	values = SwMwManagerContextValues(&context);
	if (command <= 0x2200) {
		if (command == 0x1201) {
			struct SwMwGate *held;

			SwMwSinkConnectLock(&held, (void *)values[2]);
			SwMwSinkStreamConnectCommand(channel, request, argument);
			SwMwSinkConnectUnlock(&held);
		} else {
			SwMwSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 181);
		}
		return 0;
	}
	if (command > 0x2213) {
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 181);
		return 0;
	}
	switch (command) {
	case 0x2201:
		SwMwSinkConnectCommand(channel, request, argument);
		break;
	case 0x2202:
		SwMwSinkCloseCommand(channel, request, argument);
		break;
	case 0x2203:
		SwMwSinkSetUriCommand(channel, request, argument);
		break;
	case 0x2204:
		SwMwSinkGetUriResultCommand(channel, request, argument);
		break;
	case 0x2205:
		SwMwSinkSyncCommand(channel, request, argument);
		break;
	case 0x2206:
		SwMwSinkOpenStreamCommand(channel, request, argument);
		break;
	case 0x2207:
		SwMwSinkCloseStreamCommand(channel, request, argument);
		break;
	case 0x2208:
	case 0x2209:
	case 0x220A:
		SwMwSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_SINK_SOURCE, 181);
		break;
	case 0x220B: {
		struct SwMwGate *held;

		SwMwSinkStreamLock(&held, (void *)values[2]);
		SwMwSinkGetInfoCommand(channel, request, argument);
		SwMwSinkStreamUnlock(&held);
		break;
	}
	case 0x220C: {
		struct SwMwGate *held;

		SwMwSinkStreamLock(&held, (void *)values[2]);
		SwMwSinkSetParameterCommand(channel, request, argument);
		SwMwSinkStreamUnlock(&held);
		break;
	}
	case 0x220D: {
		struct SwMwGate *held;

		SwMwSinkStreamLock(&held, (void *)values[2]);
		SwMwSinkPollCommand(channel, request, argument);
		SwMwSinkStreamUnlock(&held);
		break;
	}
	case 0x220E: {
		struct SwMwGate *held;

		SwMwSinkStreamLock(&held, (void *)values[2]);
		SwMwSinkReceiveCommand(channel, request, argument);
		SwMwSinkStreamUnlock(&held);
		break;
	}
	case 0x220F: {
		struct SwMwGate *held;

		SwMwSinkStreamLock(&held, (void *)values[2]);
		SwMwSinkReleaseCommand(channel, request, argument);
		SwMwSinkStreamUnlock(&held);
		break;
	}
	case 0x2210: {
		struct SwMwGate *held;

		SwMwSinkStreamLock(&held, (void *)values[2]);
		SwMwSinkSendCommand(channel, request, argument);
		SwMwSinkStreamUnlock(&held);
		break;
	}
	case 0x2211: {
		struct SwMwGate *held;

		SwMwSinkStreamLock(&held, (void *)values[2]);
		SwMwSinkSendMessageCommand(channel, request, argument);
		SwMwSinkStreamUnlock(&held);
		break;
	}
	case 0x2212: {
		struct SwMwGate *held;

		SwMwSinkStreamLock(&held, (void *)values[2]);
		SwMwSinkAbortCommand(channel, request, argument);
		SwMwSinkStreamUnlock(&held);
		break;
	}
	case 0x2213: {
		struct SwMwGate *held;

		SwMwSinkStreamLock(&held, (void *)values[2]);
		SwMwSinkIsOpenCommand(channel, request, argument);
		SwMwSinkStreamUnlock(&held);
		break;
	}
	}
	return 0;
}
