/*
 * The SWS content object the sink and the move sink drive (the 0xD0-byte class whose
 * dispatch table is ): the content session's start, close, URI and state
 * calls. The declaration order is the object's dispatch table; the method names are ours
 * where no Sony record names them (recorded in agent/namecheck).
 */

#pragma once

#include <scetypes.h>

/* the SWS support library's URI text base (0x0c bytes): its method table, the string
 * data and the ownership byte */
struct SwMwUriText {
	void *table;			/* 0x00 */
	void *data;			/* 0x04 */
	unsigned char owns;		/* 0x08 */
	unsigned char reserved[3];	/* 0x09 */
};

/* the full URI value: the text base and the 16-bit port/length at +0x10 */
struct SwMwUri : SwMwUriText {
	unsigned char gap[4];		/* 0x0C */
	unsigned short length;		/* 0x10 */
	unsigned char padding[2];	/* 0x12 */
};

extern "C" {
int SwMwUriInit(struct SwMwUri *uri, const char *text);		/* */
int SwMwUriDone(struct SwMwUriText *uri);				/* */
int SwMwUriFormatText(struct SwMwUriText *text, const struct SwMwUri *uri); /* */
const char *SwMwUriTextGet(const struct SwMwUriText *text);		/* */
int SwMwUriSet(struct SwMwUri *uri, const struct SwMwUri *text,
	       unsigned short size);					/* */
int SwMwUriIsSet(const struct SwMwUri *uri);				/* */
int SwMwNetParseAddress(int kind, const char *text, void *address);	/* */
int SwMwNetResolveName(const char *text, unsigned short size, int kind,
		       void *address);					/* */
int SwMwNetFormatAddress(int kind, const void *address, char *text,
			 int size);					/* */
int SwMwStatus(int status);						/* */
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
int SwMwMoveMessageInit(struct SwMwMoveMessage *message);	/* */
int SwMwMoveMessageDone(struct SwMwMoveMessage *message);	/* */
}

struct SwMwStreamEvent;

/* the content library's transfer object (the downloader/reader pair's wrapper): its
 * interface in dispatch-table order. Only the calls the middleware makes are named; the
 * rest carry the behaviour their bodies showed. Ours (no Sony record names the library
 * class; recorded in agent/namecheck). */
struct SwMwTransfer {
	virtual void destroy() = 0;				/* 0x00 */
	virtual void dispose() = 0;				/* 0x04 */
	virtual int setEvent(struct SwMwStreamEvent *event) = 0;	/* 0x08 */
	virtual int start() = 0;				/* 0x0C */
	virtual int reset() = 0;				/* 0x10 */
	virtual int isReady() = 0;				/* 0x14 */
	virtual int getState() = 0;				/* 0x18 */
	virtual int setParameter(int parameter) = 0;		/* 0x1C */
	virtual int attach(int value) = 0;			/* 0x20 */
	virtual int transfer(int value) = 0;			/* 0x24 */
	virtual int write(int offset, int size) = 0;		/* 0x28 */
	virtual int getChunkList(void **chunks, int *count) = 0;	/* 0x2C */
	virtual int release(int size) = 0;			/* 0x30 */
	virtual int setSessionData(const void *data) = 0;	/* 0x34 */
	virtual int getSessionData(int *out) = 0;		/* 0x38 */
	virtual int isKeyExpiring() = 0;			/* 0x3C */
	virtual int startMove(int value) = 0;			/* 0x40 */
};

/* the SWS transfer's other interface: its dispatch table's negative words name the
 * offsets of the object's interfaces (the downloader class keeps them zero). A use site
 * writes the adjustment out: transfer + transfer->table[-word]. */

/* the SWS content manager the core keeps behind a device (the 0x14-byte object at
 * device+0x12c, dispatch table ): its destructors, the plain transfer factory
 * , the release and the factory with the event/work areas
 * that getInfo calls */
/* the content library's buffer reader (its debug name, "SdiBuffer(" at ): the
 * ring the stream commands drain. The called slots are consume (+0x10), available
 * (+0x14), isEndOfStream (+0x1c) and getReadChunkList (+0x38); the others are named from
 * their bodies (reset, size, read, write and the write-side chunk list). Ours. */
struct SwSdiBuffer {
	virtual void destroy();				/* 0x00 */
	virtual void dispose();				/* 0x04 */
	virtual void setEndOfStream();			/* 0x08 */
	virtual int append(unsigned int size);		/* 0x0c */
	virtual int consume(unsigned int size);		/* 0x10 */
	virtual int available();			/* 0x14 */
	virtual int size();				/* 0x18 */
	virtual int isEndOfStream();			/* 0x1c */
	virtual int setName(const struct SwMwUri *uri);	/* 0x20 */
	virtual void reset();				/* 0x24 */
	virtual int setSize(unsigned int size);		/* 0x28 */
	virtual int read(void *buffer, unsigned int size);	/* 0x2c */
	virtual int write(const void *buffer, unsigned int size);/* 0x30 */
	virtual int getWriteChunkList(void **list);	/* 0x34 */
	virtual int getReadChunkList(void **list);	/* 0x38 */
	virtual int setError(int error);		/* 0x3c */
	virtual int getError();				/* 0x40 */
	virtual int noop(int value);			/* 0x44 */
};

struct SwMwContentManager {
	virtual void destroy() = 0;						/* 0x00 */
	virtual void dispose() = 0;						/* 0x04 */
	virtual int createTransfer(struct SwMwTransfer **transfer, struct SwMwUri *uri,
				   int mode, struct SwMwStreamEvent *event,
				   int receiveSize, struct SwMwSinkWork *work,
				   int sendBase) = 0;				/* 0x08 */
	virtual int releaseTransfer(struct SwMwTransfer *transfer) = 0;		/* 0x0c */
	virtual int createTransferOn(struct SwMwTransfer **transfer, struct SwMwUri *uri,
				      int mode, struct SwMwStreamEvent *event,
				      unsigned char label, int receiveSize,
				      struct SwMwSinkWork *work, int sendBase) = 0;	/* 0x10 */
};

/* the content object (14 dispatch slots, in table order) */
struct SwMwContent {
	virtual ~SwMwContent() {}				/* 0x00/0x04 */
	virtual int begin(void *uri, int value, int mode) = 0;	/* 0x08 */
	virtual int start(int *uriSet, struct SwMwUri *uri, int mode) = 0;	/* 0x0c */
	virtual int close(struct SwMwUri *uri) = 0;		/* 0x10 */
	virtual int list(void *out) = 0;			/* 0x14 */
	virtual int setUri(int *uriSet, struct SwMwUri *uri, int mode) = 0;	/* 0x18 */
	virtual int operation(struct SwMwUri *uri, int *result) = 0;	/* 0x1c */
	virtual int request(struct SwMwUri *uri, unsigned char label,
			    struct SwMwMoveMessage *message, int value) = 0;	/* 0x20 */
	virtual int finalize(struct SwMwUri *uri, struct SwMwMoveMessage *message) = 0;	/* 0x24 */
	virtual int complete(struct SwMwUri *uri, unsigned char state) = 0;		/* 0x28 */
	virtual int abort(struct SwMwUri *uri, unsigned char state) = 0;		/* 0x2c */
	virtual int getState() = 0;					/* 0x30 */
	virtual int getStateResult(struct SwMwUri *uri, int mode) = 0;	/* 0x34 */
};
