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

/* the content object (14 dispatch slots, in table order) */
struct SwMwContent {
	virtual ~SwMwContent();					/* 0x00/0x04 */
	virtual int begin(void *uri, int value, int mode);	/* 0x08 */
	virtual int start(int *uriSet, struct SwMwUri *uri, int mode);	/* 0x0c */
	virtual int close(struct SwMwUri *uri);			/* 0x10 */
	virtual int list(void *out);				/* 0x14 */
	virtual int setUri(int *uriSet, struct SwMwUri *uri, int mode);	/* 0x18 */
	virtual int operation(int value, int mode);		/* 0x1c */
	virtual int request(int value, struct SwMwUri *uri, char mode);	/* 0x20 */
	virtual int response(int value, struct SwMwUri *uri);		/* 0x24 */
	virtual int commit(struct SwMwUri *uri, char mode);		/* 0x28 */
	virtual int abort(struct SwMwUri *uri, char mode);		/* 0x2c */
	virtual int getState();					/* 0x30 */
	virtual int getStateResult(struct SwMwUri *uri, int mode);	/* 0x34 */
};
