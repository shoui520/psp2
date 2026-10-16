#pragma once

#include <stddef.h>
#include <psp2kern/types.h>

typedef struct AppmgrSfoFile {
	void *buffer;
	SceSize size;
	void *psf;
} AppmgrSfoFile;

typedef struct AppmgrSfoEntry {
	signed char key[32];
	const void *value;
	SceUInt16 format;
	SceUInt16 reserved;
	SceSize length;
	SceSize capacity;
} AppmgrSfoEntry;

typedef struct AppmgrSavedataSfoParam {
	const char *parentDirectory;
	const char *title;
	const char *detail;
	const SceUInt32 *attribute;
	const SceUInt64 *accountId;
	const void *params;
} AppmgrSavedataSfoParam;

_Static_assert(sizeof(AppmgrSavedataSfoParam) == 24 &&
	       offsetof(AppmgrSavedataSfoParam, parentDirectory) == 0 &&
	       offsetof(AppmgrSavedataSfoParam, title) == 4 &&
	       offsetof(AppmgrSavedataSfoParam, detail) == 8 &&
	       offsetof(AppmgrSavedataSfoParam, attribute) == 12 &&
	       offsetof(AppmgrSavedataSfoParam, accountId) == 16 &&
	       offsetof(AppmgrSavedataSfoParam, params) == 20,
	       "savedata SFO inputs");

_Static_assert(sizeof(AppmgrSfoFile) == 12, "SFO file ownership");
_Static_assert(offsetof(AppmgrSfoFile, size) == 4 &&
	       offsetof(AppmgrSfoFile, psf) == 8, "SFO file fields");
_Static_assert(sizeof(AppmgrSfoEntry) == 48 &&
	       offsetof(AppmgrSfoEntry, value) == 32 &&
	       offsetof(AppmgrSfoEntry, format) == 36 &&
	       offsetof(AppmgrSfoEntry, length) == 40 &&
	       offsetof(AppmgrSfoEntry, capacity) == 44, "SFO entry fields");

int appmgrBuildSfoFile(AppmgrSfoFile *file, const AppmgrSfoEntry *entries,
		      int serialize);
int appmgrLoadSfoFile(const char *path, AppmgrSfoFile *file);
int appmgrReleaseSfoFile(AppmgrSfoFile *file);
int appmgrGetSfoEntry(AppmgrSfoFile *file, const char *key, void **data,
		      SceSize *length, SceSize *capacity);
int appmgrParseSfoVersion(const signed char *version);
int appmgrSetSfoAccountId(AppmgrSfoFile *file, SceUInt64 accountId);
int appmgrSetSfoParams(AppmgrSfoFile *file, const void *params);
int appmgrBuildSavedataSfo(AppmgrSfoFile *file,
			 const AppmgrSavedataSfoParam *param);
