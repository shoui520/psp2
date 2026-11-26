/*
 * The SWS content object the sink and the move sink drive (the 0xD0-byte class whose
 * dispatch table is 0x8109a1dc): the content session's start, close, URI and state
 * calls. The declaration order is the object's dispatch table; the method names are ours
 * where no Sony record names them (recorded in agent/namecheck).
 */

#pragma once

#include <scetypes.h>

/* the SWS support library's URI value (0x14 bytes): its method table, the string data,
 * the ownership byte and the 16-bit length */
struct SwMwUri {
	void *table;			/* 0x00 */
	void *data;			/* 0x04 */
	unsigned char owns;		/* 0x08 */
	unsigned char reserved[7];	/* 0x09 */
	unsigned short length;		/* 0x10 */
	unsigned char padding[2];	/* 0x12 */
};

extern "C" {
int SwMwUriInit(struct SwMwUri *uri, const char *text);		/* 0x81052c04 */
int SwMwUriDone(struct SwMwUri *uri);					/* 0x81052d6a */
int SwMwUriSet(struct SwMwUri *uri, const struct SwMwUri *text,
	       unsigned short size);					/* 0x810507ba */
int SwMwUriIsSet(const struct SwMwUri *uri);				/* 0x8105091c */
int SwMwNetParseAddress(int kind, const char *text, void *address);	/* 0x81055eb8 */
int SwMwNetResolveName(const char *text, unsigned short size, int kind,
		       void *address);					/* 0x81055de8 */
int SwMwNetFormatAddress(int kind, const void *address, char *text,
			 int size);					/* 0x81055ee2 */
int SwMwStatus(int status);						/* 0x81012408 */
}

/* the DTCP move message (0x24 bytes): its type word, the kxm label byte and the three
 * payload blocks the content object sends with MV_FINALIZE */
struct SwMwMoveMessage {
	unsigned int type;		/* 0x00 */
	unsigned char label;		/* 0x04 */
	unsigned char key[8];		/* 0x05 */
	unsigned char first[10];	/* 0x0D */
	unsigned char second[10];	/* 0x17 */
	unsigned char padding[3];	/* 0x21 */
};

extern "C" {
int SwMwMoveMessageInit(struct SwMwMoveMessage *message);	/* 0x8105236c */
int SwMwMoveMessageDone(struct SwMwMoveMessage *message);	/* 0x810523d6 */
}

struct SwMwStreamEvent;

/* the content library's transfer object (the downloader/reader pair's wrapper): its
 * interface in dispatch-table order. Only the calls the middleware makes are named; the
 * rest carry the behaviour their bodies showed. Ours (no Sony record names the library
 * class; recorded in agent/namecheck). */
struct SwMwTransfer {
	virtual void destroy();				/* 0x00 */
	virtual void dispose();				/* 0x04 */
	virtual int setEvent(struct SwMwStreamEvent *event);	/* 0x08 */
	virtual int start(int *out);			/* 0x0C */
	virtual int reset();				/* 0x10 */
	virtual int isReady();				/* 0x14 */
	virtual int getState();				/* 0x18 */
	virtual int setParameter(int parameter);	/* 0x1C */
	virtual int attach(int value);			/* 0x20 */
	virtual int transfer(int value);		/* 0x24 */
	virtual int write(int offset, int size);	/* 0x28 */
	virtual int getChunkList(int *out);		/* 0x2C */
	virtual int release(int size);			/* 0x30 */
	virtual int setSessionData(const void *data);	/* 0x34 */
	virtual int getSessionData(int *out);		/* 0x38 */
	virtual int isKeyExpiring();			/* 0x3C */
	virtual int startMove(int value);		/* 0x40 */
};

/* the SWS transfer's other interface: its dispatch table's negative words name the
 * offsets of the object's interfaces (the downloader class keeps them zero). A use site
 * writes the adjustment out: transfer + transfer->table[-word]. */

/* the SWS content manager the core keeps behind a device (the 0x14-byte object at
 * device+0x12c, dispatch table 0x8109a300): its destructors, the plain transfer factory
 * 0x81016cb4, the release 0x81016e40 and the factory with the event/work areas
 * 0x81016e58 that getInfo calls */
struct SwMwContentManager {
	virtual void destroy();						/* 0x00 */
	virtual void dispose();						/* 0x04 */
	virtual int createTransfer(struct SwMwTransfer **transfer);	/* 0x08 */
	virtual int releaseTransfer(struct SwMwTransfer *transfer);	/* 0x0c */
	virtual int createTransferOn(struct SwMwTransfer **transfer, int *device,
				      int mode, void *channel, int label,
				      int value, void *work, int value2);	/* 0x10 */
};

/* the content object (14 dispatch slots, in table order) */
struct SwMwContent {
	virtual ~SwMwContent();					/* 0x00/0x04 */
	virtual int begin(void *uri, int value, int mode);	/* 0x08 */
	virtual int start(int *uriSet, struct SwMwUri *uri, int mode);	/* 0x0c */
	virtual int close(struct SwMwUri *uri);			/* 0x10 */
	virtual int list(void *out);				/* 0x14 */
	virtual int setUri(int *uriSet, struct SwMwUri *uri, int mode);	/* 0x18 */
	virtual int operation(struct SwMwUri *uri, int *result);	/* 0x1c */
	virtual int request(struct SwMwUri *uri, unsigned char label,
			    struct SwMwMoveMessage *message, int value);	/* 0x20 */
	virtual int finalize(struct SwMwUri *uri, struct SwMwMoveMessage *message);	/* 0x24 */
	virtual int complete(struct SwMwUri *uri, unsigned char state);		/* 0x28 */
	virtual int abort(struct SwMwUri *uri, unsigned char state);		/* 0x2c */
	virtual int getState();					/* 0x30 */
	virtual int getStateResult(struct SwMwUri *uri, int mode);	/* 0x34 */
};
