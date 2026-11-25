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

