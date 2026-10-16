#ifndef SCE_SELF_H
#define SCE_SELF_H

#include <psp2/types.h>
#include <sce/certified_file.h>

/* The certified-file envelope and complete SELF extended header. */
typedef struct SceSelfHeader {
	cf_header cfHeader;
	SceUInt64 selfType;
	SceUInt64 appinfoOffset;
	SceUInt64 elfOffset;
	SceUInt64 phdrOffset;
	SceUInt64 shdrOffset;
	SceUInt64 segmentInfoOffset;
	SceUInt64 sceversionOffset;
	SceUInt64 controlInfoOffset;
	SceUInt64 controlInfoSize;
	SceUInt64 reserved;
} SceSelfHeader;

typedef struct SceSelfAppInfo {
	SceUInt64 authId;
	SceUInt32 vendorId;
	SceUInt32 selfType;
	SceUInt64 version;
	SceUInt64 reserved;
} SceSelfAppInfo;

/* ModuleMgr's initial image view includes the following application info. */
typedef struct SceSelfImageHeader {
	SceSelfHeader header;
	SceSelfAppInfo appInfo;
} SceSelfImageHeader;

/* Control records have a 64-byte or 256-byte payload, selected by type. */
typedef struct SceSelfControlInfo {
	SceUInt32 type;
	SceUInt32 size;
	SceUInt64 next;
	unsigned char payload[];
} SceSelfControlInfo;

_Static_assert(sizeof(SceSelfControlInfo) == 16 &&
	       offsetof(SceSelfControlInfo, payload) == 16,
	       "SELF control record header");

_Static_assert(sizeof(SceSelfHeader) == 128, "SELF header");
_Static_assert(offsetof(SceSelfHeader, selfType) == 48 &&
	       offsetof(SceSelfHeader, appinfoOffset) == 56 &&
	       offsetof(SceSelfHeader, controlInfoOffset) == 104 &&
	       offsetof(SceSelfHeader, controlInfoSize) == 112,
	       "SELF extended header fields");
_Static_assert(sizeof(SceSelfAppInfo) == 32, "SELF application info");
_Static_assert(sizeof(SceSelfImageHeader) == 160 &&
	       offsetof(SceSelfImageHeader, appInfo.authId) == 128 &&
	       offsetof(SceSelfImageHeader, appInfo.version) == 144,
	       "SELF initial image fields");

#endif
