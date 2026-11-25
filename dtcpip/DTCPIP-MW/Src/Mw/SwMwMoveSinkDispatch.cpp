/*
 * swagner's request dispatcher for the move sink (DTCPIP-MW/Src/Mw/
 * SwMwMoveSinkDispatch.cpp, from its assert and error strings): the system engine's
 * commands (0x2801..0x2819, the library's SW_SYS protocol), the DTCP move completion
 * commands (MV_FINALIZE/MV_COMPLETE) and the periodic 0x1801 stream connect.
 */

#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include "../Comm/vita/SwMwCommServer.h"
#include "SwMwSync.h"
#include "SwMwMoveSink.h"

#define SWMW_MOVESINK_SOURCE	"C:\\work\\svn\\psp2\\dtcpip\\trunk.365\\DTCPIP-MW\\Src\\Mw\\SwMwMoveSinkDispatch.cpp"

extern "C" {
int swMwTrace(const char *format, ...);
int SwsSetToolMode(int value);

/* the move sink's stream object (SwMwMoveSinkStream.cpp) */
struct SwMwGate *SwMwMoveSinkStreamGate(void *stream);
int SwMwMoveSinkStreamCreate(int *stream, int sink, const int *window);
int SwMwMoveSinkStreamDestroy(int stream);
int SwMwMoveSinkStreamConnect(int stream);
int SwMwMoveSinkStreamGetInfo(int stream, void *info);
int SwMwMoveSinkStreamSetParameter(int stream, int parameter);
int SwMwMoveSinkStreamPoll(int stream);
int SwMwMoveSinkStreamReceive(int stream, int *offset, int *size, int *type);
int SwMwMoveSinkStreamRelease(int stream, int size);
int SwMwMoveSinkStreamSend(int stream, int *offset, int *size);
int SwMwMoveSinkStreamSendMessage(int stream, int message);
int SwMwMoveSinkStreamAbort(int stream);
int SwMwMoveSinkStreamIsOpen(int stream);
int SwMwMoveSinkStreamWaitClosed(int stream);
int SwMwMoveSinkStreamComplete(int stream);
int SwMwMoveSinkStreamFinalize(int stream, int label);
int SwMwMoveSinkStreamCompleteLabel(int stream, int label);
void SwMwMoveSinkStreamGetWindow(int stream, int *window);
}

/* the information block's first 16 bytes, copied whole */
struct SwMwMoveSinkStreamInfoHeader {
	unsigned int word[4];
};

/* the move stream's information block: its head, the two 16-byte ids, a byte, the
 * 0x200-byte payload and the payload size */
struct SwMwMoveSinkStreamInfo {
	struct SwMwMoveSinkStreamInfoHeader header;	/* 0x00 */
	unsigned char second[16];	/* 0x10 */
	unsigned char third[16];	/* 0x20 */
	int value0;			/* 0x30 */
	int value1;			/* 0x34 */
	unsigned char byte;		/* 0x38 */
	unsigned char reserved[3];	/* 0x39 */
	int size;			/* 0x3C */
	unsigned char payload[0x200];	/* 0x40 */
};

/* the stream's shared-memory windows, in the middleware's order */
struct SwMwMoveSinkStreamWindow {
	unsigned char *receiveBase;	/* 0x00 */
	int sendSize;			/* 0x04 */
	unsigned char *sendBase;	/* 0x08 */
	int receiveSize;		/* 0x0C */
	int reserved;			/* 0x10 */
	int blockSize;			/* 0x14 */
};

/* answer a move-sink request with a status and no payload (0x81007f68) */
extern "C" int SwMwMoveSinkReplyStatus(struct SwMwCommChannel *channel,
				       struct SwMwIpcMessage *request, int status)
{
	request->result = status;
	request->outSize = 0;
	request->outPayload = NULL;
	return SwMwCommServerReplyError(channel, request);
}

/* turn a move-sink handle back into its object (0x8100837c) */
extern "C" int SwMwMoveSinkDecodeHandle(int handle, int *sink)
{
	*sink = (int)((unsigned int)handle ^ 0x10AAAA55);
	return 0;
}

/* turn a move-sink stream handle back into its object (0x81008a82) */
extern "C" int SwMwMoveSinkStreamDecodeHandle(int handle, int *stream)
{
	*stream = (int)((unsigned int)handle ^ 0x10AAAA55);
	return 0;
}

/* the stream gate's shared side (0x8107b604/0x8107b624) */
extern "C" struct SwMwGate **SwMwMoveSinkStreamLock(struct SwMwGate **held, void *stream)
{
	held[0] = SwMwMoveSinkStreamGate(stream);
	SwMwGateSharedLock(held[0]);
	return held;
}

extern "C" struct SwMwGate **SwMwMoveSinkStreamUnlock(struct SwMwGate **held)
{
	SwMwGateSharedUnlock(held[0]);
	return held;
}

/* the stream gate's exclusive side (0x8107b638/0x8107b658) */
extern "C" struct SwMwGate **SwMwMoveSinkConnectLock(struct SwMwGate **held, void *stream)
{
	held[0] = SwMwMoveSinkStreamGate(stream);
	SwMwGateExclusiveLock(held[0]);
	return held;
}

extern "C" struct SwMwGate **SwMwMoveSinkConnectUnlock(struct SwMwGate **held)
{
	SwMwGateExclusiveUnlock(held[0]);
	return held;
}

/* 0x2801: create the channel's move sink from its device */
extern "C" int SwMwMoveSinkConnectCommand(struct SwMwCommChannel *channel,
					  struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int status;
	int *values;
	int sink = 0;
	int failed;

	SwMwManagerBuildContext(&context, channel);
	values = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 213);
	} else if (*values != 0) {
		SwsSetToolMode(1);
		status = SwMwMoveSinkCreate(&sink, *values);
			if (status != 0) {
							swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 226);
				failed = 1;
			}
		request->result = status;
		request->outSize = 0;
		request->outPayload = NULL;
		request->out0 = sink ^ 0x10AAAA55;
		status = SwMwCommServerReplyError(channel, request);
			if (status != 0) {
							swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 234);
				failed = 1;
			}
	} else {
		SwMwMoveSinkReplyStatus(channel, request, 12004);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MOVESINK_SOURCE, 219);
	}
	return 0;
}

/* 0x2802: destroy the request's move sink */
extern "C" int SwMwMoveSinkCloseCommand(struct SwMwCommChannel *channel,
					struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int sink;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 254);
	} else {
		status = SwMwMoveSinkDecodeHandle(request->value0, &sink);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 259);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 261);
		} else {
			status = SwMwMoveSinkDestroy(sink);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 266);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 273);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2803: give the move sink its URI */
extern "C" int SwMwMoveSinkSetUriCommand(struct SwMwCommChannel *channel,
					 struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int sink;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 287);
	} else {
		status = SwMwMoveSinkDecodeHandle(request->value0, &sink);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 292);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 294);
		} else if (request->size != 0) {
			unsigned short port = request->value1;

			status = SwMwMoveSinkSetUri(sink, request->payload, port);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 308);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 316);
					failed = 1;
				}
		} else {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 300);
		}
	}
	return 0;
}

/* 0x2804: the move sink's URI result */
extern "C" int SwMwMoveSinkGetUriResultCommand(struct SwMwCommChannel *channel,
					       struct SwMwIpcMessage *request, void *argument)
{
	unsigned char result;
	int status;
	int sink;
	int failed;

	result = 0;
	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 329);
	} else {
		status = SwMwMoveSinkDecodeHandle(request->value0, &sink);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 334);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 336);
		} else {
			status = SwMwMoveSinkGetUriResult(sink, &result, request->value1);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 341);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			request->out0 = result;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 349);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2805: sync the move sink's state */
extern "C" int SwMwMoveSinkSyncCommand(struct SwMwCommChannel *channel,
				       struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int sink;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 361);
	} else {
		status = SwMwMoveSinkDecodeHandle(request->value0, &sink);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 366);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 368);
		} else {
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 380);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2806: open the move sink's stream on the request's shared-memory windows */
extern "C" int SwMwMoveSinkOpenStreamCommand(struct SwMwCommChannel *channel,
					     struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int *payload;
	int *values;
	int sink;
	int stream = 0;
	struct SwMwMoveSinkStreamWindow window;
	struct SwMwCommRequestView view;
	struct SwMwManagerContextBlock context;
	int failed;

	SwMwManagerBuildContext(&context, channel);
	values = SwMwManagerContextValues(&context);
	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 399);
	} else {
		SwMwCommServerGetRequest((const struct SwMwCommRequest *)channel, view.value);
		status = SwMwMoveSinkDecodeHandle(request->value0, &sink);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 406);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 408);
		} else if (request->size != 0) {
			payload = (int *)request->payload;
			window.sendBase = (unsigned char *)view.value[1];
			window.receiveSize = payload[1];
			window.receiveBase = (unsigned char *)(view.value[1] + payload[1]);
			window.sendSize = payload[2];
			window.reserved = payload[3];
			window.blockSize = payload[4];
			status = SwMwMoveSinkStreamCreate(&stream, sink, (const int *)&window);
			if (status != 0) {
				swMwTrace("[SWMW] Error %d in %s:%d\n", status,
					  SWMW_MOVESINK_SOURCE, 428);
				failed = 1;
			} else {
				values[3] = sink;
				values[4] = stream;
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
						  SWMW_MOVESINK_SOURCE, 441);
					failed = 1;
				}
		} else {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 414);
		}
	}
	return 0;
}

/* 0x2807: close the request's move stream */
extern "C" int SwMwMoveSinkCloseStreamCommand(struct SwMwCommChannel *channel,
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
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 462);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 467);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 469);
		} else {
			status = SwMwMoveSinkStreamDestroy(stream);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 474);
					failed = 1;
				}
			values[3] = 0;
			values[4] = 0;
			SwMwCommServerSetRequestValue((struct SwMwCommRequest *)channel,
						      context.head);
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 485);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x280b: the move stream's information (0x240 bytes, 0x200 of payload) */
extern "C" int SwMwMoveSinkGetInfoCommand(struct SwMwCommChannel *channel,
					  struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwMoveSinkStreamInfo info;
	int *payload;
	int stream;
	int status;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 499);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 504);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 506);
		} else if (request->size >= 0x240) {
			payload = (int *)request->payload;
			memset(&info, 0, 0x240);
			info.header = *(const struct SwMwMoveSinkStreamInfoHeader *)payload;
			info.value0 = payload[12];
			memcpy(info.third, payload + 8, 16);
			memcpy(info.second, payload + 4, 16);
			info.value1 = payload[13];
			info.byte = *((unsigned char *)payload + 56);
			if (payload[15] > 0x200) {
				SwMwMoveSinkReplyStatus(channel, request, 12003);
				swMwTrace("[SWMW] Error %d in %s:%d\n", 12003,
					  SWMW_MOVESINK_SOURCE, 533);
			} else {
				info.size = payload[15];
				memcpy(info.payload, payload + 16, payload[15]);
				status = SwMwMoveSinkStreamGetInfo(stream, &info);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 538);
					failed = 1;
				}
				request->result = status;
				request->outSize = 0;
				request->outPayload = NULL;
				status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 545);
					failed = 1;
				}
			}
		} else {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 512);
		}
	}
	return 0;
}

/* 0x280c: set one of the move stream's parameters */
extern "C" int SwMwMoveSinkSetParameterCommand(struct SwMwCommChannel *channel,
					       struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 557);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 562);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 564);
		} else {
			status = SwMwMoveSinkStreamSetParameter(stream, request->value1);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 569);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 576);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x280d: poll the move stream */
extern "C" int SwMwMoveSinkPollCommand(struct SwMwCommChannel *channel,
				       struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 588);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 593);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 595);
		} else {
			status = SwMwMoveSinkStreamPoll(stream);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 600);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 607);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x280e: receive a chunk from the move stream */
extern "C" int SwMwMoveSinkReceiveCommand(struct SwMwCommChannel *channel,
					  struct SwMwIpcMessage *request, void *argument)
{
	int window[6];
	int type;
	int stream;
	int offset;
	int size;
	int status;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 623);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 628);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 630);
		} else {
			status = SwMwMoveSinkStreamReceive(stream, &offset, &size, &type);
				if (status != 12007 && status != 12401 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 635);
					failed = 1;
				}
			SwMwMoveSinkStreamGetWindow(stream, window);
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			request->out0 = offset - window[0];
			request->out1 = size;
			request->out2 = type;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 648);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x280f: release the bytes the caller has read */
extern "C" int SwMwMoveSinkReleaseCommand(struct SwMwCommChannel *channel,
					  struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 660);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 665);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 667);
		} else {
			status = SwMwMoveSinkStreamRelease(stream, request->value1);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 672);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 679);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2810: take the peer's chunk from the move stream */
extern "C" int SwMwMoveSinkSendCommand(struct SwMwCommChannel *channel,
				       struct SwMwIpcMessage *request, void *argument)
{
	int window[6];
	int stream;
	int offset;
	int size;
	int status;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 695);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 700);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 702);
		} else {
			status = SwMwMoveSinkStreamSend(stream, &offset, &size);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 707);
					failed = 1;
				}
			SwMwMoveSinkStreamGetWindow(stream, window);
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			request->out0 = offset - window[2];
			request->out1 = size;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 719);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2811: send a message to the move stream's peer */
extern "C" int SwMwMoveSinkSendMessageCommand(struct SwMwCommChannel *channel,
					      struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 731);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 736);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 738);
		} else {
			status = SwMwMoveSinkStreamSendMessage(stream, request->value1);
				if (status != 12015 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 743);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 750);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2812: abort the move stream's transfer */
extern "C" int SwMwMoveSinkAbortCommand(struct SwMwCommChannel *channel,
					struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 762);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 767);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 769);
		} else {
			status = SwMwMoveSinkStreamAbort(stream);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 774);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 781);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2813: the move stream's open state */
extern "C" int SwMwMoveSinkIsOpenCommand(struct SwMwCommChannel *channel,
					 struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 793);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 798);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 800);
		} else {
			status = SwMwMoveSinkStreamIsOpen(stream);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 805);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 812);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x1801: the move stream's periodic connect */
extern "C" int SwMwMoveSinkStreamConnectCommand(struct SwMwCommChannel *channel,
						struct SwMwIpcMessage *request, void *argument)
{
	int stream;
	int status;
	int failed;

	stream = request->value0;
	if (stream != 0) {
		status = SwMwMoveSinkStreamConnect(stream);
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 828);
			return 1;
		}
	} else {
		SwMwMoveSinkReplyStatus(channel, request, 12004);
		return swMwTrace("[SWMW] Error %d in %s:%d\n", 12004, SWMW_MOVESINK_SOURCE, 823);
	}
	return status;
}

/* 0x2814: wait for the move stream to close */
extern "C" int SwMwMoveSinkWaitClosedCommand(struct SwMwCommChannel *channel,
					     struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 839);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 844);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 846);
		} else {
			status = SwMwMoveSinkStreamWaitClosed(stream);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 851);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 858);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2815: poll the move stream's close (the MV_COMPLETE message) */
extern "C" int SwMwMoveSinkPollClosedCommand(struct SwMwCommChannel *channel,
					     struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 870);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 875);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 877);
		} else {
			status = SwMwMoveSinkStreamComplete(stream);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 882);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 889);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2816: send the move's MV_FINALIZE message with its payload */
extern "C" int SwMwMoveSinkFinalizeCommand(struct SwMwCommChannel *channel,
					   struct SwMwIpcMessage *request, void *argument)
{
	unsigned short value2;
	int status;
	int sink;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 904);
	} else {
		status = SwMwMoveSinkDecodeHandle(request->value0, &sink);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 909);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 911);
		} else if (request->size != 0) {
			value2 = request->value2;
			status = SwMwMoveSinkFinalize(sink, request->payload, value2,
						      request->value1, value2);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 925);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 932);
					failed = 1;
				}
		} else {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 917);
		}
	}
	return 0;
}

/* 0x2817: send the move's MV_COMPLETE message */
extern "C" int SwMwMoveSinkCompleteCommand(struct SwMwCommChannel *channel,
					   struct SwMwIpcMessage *request, void *argument)
{
	int status;
	int sink;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 944);
	} else {
		status = SwMwMoveSinkDecodeHandle(request->value0, &sink);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 949);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 951);
		} else {
			status = SwMwMoveSinkComplete(sink);
				if (status != 12007 && status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 956);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 963);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2818: the move stream sends MV_FINALIZE with its label */
extern "C" int SwMwMoveSinkStreamFinalizeCommand(struct SwMwCommChannel *channel,
						 struct SwMwIpcMessage *request,
						 void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 975);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 980);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 982);
		} else {
			status = SwMwMoveSinkStreamFinalize(stream, request->value1);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 987);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 994);
					failed = 1;
				}
		}
	}
	return 0;
}

/* 0x2819: the move stream sends MV_COMPLETE with its label */
extern "C" int SwMwMoveSinkStreamCompleteCommand(struct SwMwCommChannel *channel,
						 struct SwMwIpcMessage *request,
						 void *argument)
{
	int status;
	int stream;
	int failed;

	if (request->channel == 255) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 1006);
	} else {
		status = SwMwMoveSinkStreamDecodeHandle(request->value0, &stream);
		failed = 0;
		if (status != 0) {
			swMwTrace("[SWMW] Error %d in %s:%d\n", status, SWMW_MOVESINK_SOURCE, 1011);
			failed = 1;
		}
		if (failed != 0) {
			SwMwMoveSinkReplyStatus(channel, request, 12003);
			swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 1013);
		} else {
			status = SwMwMoveSinkStreamCompleteLabel(stream, request->value1);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 1018);
					failed = 1;
				}
			request->result = status;
			request->outSize = 0;
			request->outPayload = NULL;
			status = SwMwCommServerReplyError(channel, request);
				if (status != 0) {
					swMwTrace("[SWMW] Error %d in %s:%d\n", status,
						  SWMW_MOVESINK_SOURCE, 1025);
					failed = 1;
				}
		}
	}
	return 0;
}

/* the move sink's command dispatcher (0x81007f96) */
extern "C" int SwMwMoveSinkDispatch(struct SwMwCommChannel *channel,
				    struct SwMwIpcMessage *request, void *argument)
{
	struct SwMwManagerContextBlock context;
	int *values;
	int command = request->command;
	int failed = 0;

	if (channel == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MOVESINK_SOURCE, 88);
	if (request == NULL)
		swMwTrace("[SWMW] Assertion Failed in %s:%d\n", SWMW_MOVESINK_SOURCE, 89);
	SwMwManagerBuildContext(&context, channel);
	values = SwMwManagerContextValues(&context);
	if (command <= 0x2800) {
		if (command == 0x1801) {
			struct SwMwGate *held;

			SwMwMoveSinkConnectLock(&held, (void *)values[4]);
			SwMwMoveSinkStreamConnectCommand(channel, request, argument);
			SwMwMoveSinkConnectUnlock(&held);
			return 0;
		}
		failed = 1;
	} else if (command > 0x2819) {
		failed = 1;
	} else {
		switch (command) {
		case 0x2801:
			SwMwMoveSinkConnectCommand(channel, request, argument);
			break;
		case 0x2802:
			SwMwMoveSinkCloseCommand(channel, request, argument);
			break;
		case 0x2803:
			SwMwMoveSinkSetUriCommand(channel, request, argument);
			break;
		case 0x2804:
			SwMwMoveSinkGetUriResultCommand(channel, request, argument);
			break;
		case 0x2805:
			SwMwMoveSinkSyncCommand(channel, request, argument);
			break;
		case 0x2806:
			SwMwMoveSinkOpenStreamCommand(channel, request, argument);
			break;
		case 0x2807:
			SwMwMoveSinkCloseStreamCommand(channel, request, argument);
			break;
		case 0x2808:
		case 0x2809:
		case 0x280A:
			failed = 1;
			break;
		case 0x280B: {
			struct SwMwGate *held;

			SwMwMoveSinkStreamLock(&held, (void *)values[4]);
			SwMwMoveSinkGetInfoCommand(channel, request, argument);
			SwMwMoveSinkStreamUnlock(&held);
			break;
		}
		case 0x280C: {
			struct SwMwGate *held;

			SwMwMoveSinkStreamLock(&held, (void *)values[4]);
			SwMwMoveSinkSetParameterCommand(channel, request, argument);
			SwMwMoveSinkStreamUnlock(&held);
			break;
		}
		case 0x280D: {
			struct SwMwGate *held;

			SwMwMoveSinkStreamLock(&held, (void *)values[4]);
			SwMwMoveSinkPollCommand(channel, request, argument);
			SwMwMoveSinkStreamUnlock(&held);
			break;
		}
		case 0x280E: {
			struct SwMwGate *held;

			SwMwMoveSinkStreamLock(&held, (void *)values[4]);
			SwMwMoveSinkReceiveCommand(channel, request, argument);
			SwMwMoveSinkStreamUnlock(&held);
			break;
		}
		case 0x280F: {
			struct SwMwGate *held;

			SwMwMoveSinkStreamLock(&held, (void *)values[4]);
			SwMwMoveSinkReleaseCommand(channel, request, argument);
			SwMwMoveSinkStreamUnlock(&held);
			break;
		}
		case 0x2810: {
			struct SwMwGate *held;

			SwMwMoveSinkStreamLock(&held, (void *)values[4]);
			SwMwMoveSinkSendCommand(channel, request, argument);
			SwMwMoveSinkStreamUnlock(&held);
			break;
		}
		case 0x2811: {
			struct SwMwGate *held;

			SwMwMoveSinkStreamLock(&held, (void *)values[4]);
			SwMwMoveSinkSendMessageCommand(channel, request, argument);
			SwMwMoveSinkStreamUnlock(&held);
			break;
		}
		case 0x2812: {
			struct SwMwGate *held;

			SwMwMoveSinkStreamLock(&held, (void *)values[4]);
			SwMwMoveSinkAbortCommand(channel, request, argument);
			SwMwMoveSinkStreamUnlock(&held);
			break;
		}
		case 0x2813: {
			struct SwMwGate *held;

			SwMwMoveSinkStreamLock(&held, (void *)values[4]);
			SwMwMoveSinkIsOpenCommand(channel, request, argument);
			SwMwMoveSinkStreamUnlock(&held);
			break;
		}
		case 0x2814:
			SwMwMoveSinkWaitClosedCommand(channel, request, argument);
			break;
		case 0x2815:
			SwMwMoveSinkPollClosedCommand(channel, request, argument);
			break;
		case 0x2816:
			SwMwMoveSinkFinalizeCommand(channel, request, argument);
			break;
		case 0x2817:
			SwMwMoveSinkCompleteCommand(channel, request, argument);
			break;
		case 0x2818:
			SwMwMoveSinkStreamFinalizeCommand(channel, request, argument);
			break;
		case 0x2819:
			SwMwMoveSinkStreamCompleteCommand(channel, request, argument);
			break;
		}
	}
	if (failed != 0) {
		SwMwMoveSinkReplyStatus(channel, request, 12003);
		swMwTrace("[SWMW] Error %d in %s:%d\n", 12003, SWMW_MOVESINK_SOURCE, 196);
	}
	return 0;
}
