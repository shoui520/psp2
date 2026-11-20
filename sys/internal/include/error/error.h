/*
 * SceError's error history: the record a module posts (user mode through SceErrorUser's
 * sceErrorHistoryPostError, in SceDriverUser). The names are ours.
 */
#ifndef _ERROR_ERROR_H
#define _ERROR_ERROR_H

#include <scetypes.h>

typedef struct ErrorRecord {
	char text[0x100];			/* the error's text, UTF-8 */
	SceUInt32 messageIds[15];		/* the resource IDs of its details */
	SceUInt8 reserved0[2];
	SceUInt8 messageIdCount;
	SceUInt8 reserved1;
	SceUInt32 code;
	SceUInt32 id;
	SceUInt32 swVersion;			/* SceKernelSystemSwVersion.version */
	SceUInt32 format;			/* the default format, when set */
	SceUInt8 key[10];
	SceUInt8 padding[2];
	SceUInt32 swVersionExtra;		/* the word after it */
	SceUInt8 reserved[0x20];
} ErrorRecord;

#ifdef __cplusplus
extern "C" {
#endif

int sceErrorHistoryPostError(const void *record);
/* the error's code as the user sees it ("C1-2345-6"), into a buffer of at least 18 bytes */
int sceErrorGetExternalString(char *buf, SceUInt32 code);

#ifdef __cplusplus
}
#endif

#endif
