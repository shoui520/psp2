/*
 * The middleware server's request message and the request view of the SW core: the
 * library's 0x38-byte message (SwMwCommServer.cpp receives and answers it) and the
 * 269-byte block the core's dispatch reads from a channel.
 */

#pragma once

#include <scetypes.h>

/* one request as the server sees it (0x38 bytes, the library's message layout) */
struct SwMwIpcMessage {
	int command;		/* 0x00 */
	unsigned char channel;	/* 0x04 */
	unsigned char padAfterChannel;
	unsigned short size;	/* 0x06 */
	void *payload;		/* 0x08 */
	int value0;		/* 0x0C */
	int value1;		/* 0x10 */
	int value2;		/* 0x14 */
	int value3;		/* 0x18 */
	int result;		/* 0x1C */
	unsigned short outSize;	/* 0x20 */
	unsigned short padAfterOutSize;
	void *outPayload;	/* 0x24 */
	int out0;		/* 0x28 */
	int out1;		/* 0x2C */
	int out2;		/* 0x30 */
	int out3;		/* 0x34 */
};

/* the middleware's identity, from the launch parameter (main.cpp) */
extern char g_swagnerIdentity[16];

/* the SW core's request view of a channel: three words of its context and the 256-byte
 * block that follows them (the channel itself, seen from its head) */
struct SwMwCommRequest {
	int reserved;			/* 0x00 */
	int value[3];			/* 0x04: the context's first three words */
	unsigned char payload[256];	/* 0x10 */
};

/* what the core's copy fills: just the three context words and the 256-byte body */
struct SwMwCommRequestView {
	int value[3];			/* 0x00 */
	unsigned char payload[256];	/* 0x0C */
};

/* the dispatch context (SwMwMain.cpp's dispatcher): the channel word, its request view's
 * three words and the 256-byte body the request handlers read and write back */
struct SwMwManagerContextBlock {
	void *channel;				/* 0x00 */
	int head[3];				/* 0x04 */
	unsigned char body[256];		/* 0x10 */
};

struct SwMwCommChannel;
struct SwMwIpcMessage;

/* the context helpers and the server's replies keep C linkage */
extern "C" {
/* fill the context from the channel's request view and return its device words */
void SwMwManagerBuildContext(void *context, void *channel);
int *SwMwManagerContextValues(void *context);
int SwMwCommServerGetRequest(const struct SwMwCommRequest *request, int *out);
int SwMwCommServerReplyError(struct SwMwCommChannel *channel, struct SwMwIpcMessage *request);
int SwMwCommServerReplyStatus(struct SwMwCommChannel *channel, struct SwMwIpcMessage *request,
			      int status);
int SwMwCommServerSetRequestValue(struct SwMwCommRequest *request, const void *value);
}
