/*
 * SceError's error history: the record a module posts (user mode through SceErrorUser's
 * sceErrorHistoryPostError, in SceDriverUser). The names are ours.
 */
#ifndef _ERROR_ERROR_H
#define _ERROR_ERROR_H

#include <scetypes.h>

typedef struct ErrorRecord {
	SceUInt8 data[0x140];
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

#ifdef __cplusplus
}
#endif

#endif
