#pragma once

#include "qmi_backend.h"

/* Native custom selection records: the consumer ignores the final halfword. */
typedef struct BbmcRefreshFile {
	SceUInt16 path[2];
	SceUInt16 fileId;
	SceUInt16 reserved;
} BbmcRefreshFile;

typedef struct BbmcRefreshFileList {
	SceUInt16 fileCount;
	SceUInt16 reserved;
	BbmcRefreshFile files[];
} BbmcRefreshFileList;

/* The configured service serializes each path component most significant first. */
typedef struct QmiRefreshFileIdentity {
	SceUInt8 path[2][2];
	SceUInt8 fileId[2];
} QmiRefreshFileIdentity;

typedef struct QmiRefreshFileList {
	QmiTlvHeader header;
	SceUInt8 fileCountLow;
	SceUInt8 fileCountHigh;
	QmiRefreshFileIdentity files[];
} QmiRefreshFileList;

typedef struct QmiUimRefreshInfo {
	QmiTlvHeader header;
	SceUInt8 registerFlag;
	SceUInt8 voteForInit;
	SceUInt8 fileCountLow;
	SceUInt8 fileCountHigh;
	SceUInt8 files[];
} QmiUimRefreshInfo;

typedef struct QmiUimRefreshParameters {
	QmiUimSessionTlv session;
	QmiUimRefreshInfo info;
} QmiUimRefreshParameters;

/* File paths follow their byte count without alignment in the UIM packet. */
typedef struct QmiUimRefreshFile {
	SceUInt16 fileId;
	SceUInt8 pathLength;
	SceUInt16 path[];
} __attribute__((packed)) QmiUimRefreshFile;

_Static_assert(sizeof(BbmcRefreshFile) == 8, "custom refresh file size");
_Static_assert(offsetof(BbmcRefreshFile, fileId) == 4, "custom refresh file ID");
_Static_assert(sizeof(BbmcRefreshFileList) == 4, "custom refresh list header");
_Static_assert(offsetof(BbmcRefreshFileList, files) == 4, "custom refresh list records");
_Static_assert(sizeof(QmiRefreshFileIdentity) == 6, "configured refresh file identity");
_Static_assert(sizeof(QmiRefreshFileList) == 5, "configured refresh file list prefix");
_Static_assert(sizeof(QmiUimRefreshInfo) == 7, "UIM refresh registration info");
_Static_assert(sizeof(QmiUimRefreshParameters) == 12, "UIM refresh registration prefix");
_Static_assert(sizeof(QmiUimRefreshFile) == 3, "UIM refresh file prefix");
_Static_assert(offsetof(QmiUimRefreshFile, path) == 3, "UIM refresh unaligned path");
