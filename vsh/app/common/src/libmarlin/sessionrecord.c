/*
 * The session record in MlnShared's buffers[2]: the platform's session id
 * and mode, when the session started and who started it (0x81052d14-).
 * The file's name is ours.
 */

#include "mln_internal.h"

typedef struct MlnSessionRecord {
	int id;
	int mode;		/* 0: no session */
	unsigned int start;
	int owner;
} MlnSessionRecord;

static inline __attribute__((always_inline)) MlnSessionRecord *MlnSessionRecordGet(MlnShared *shared)
{
	MlnSessionRecord *record = shared->buffers[2];

	if (record == NULL) {
		record = MlnAllocate(MLN_HEAP_GENERAL, sizeof(*record));
		if (record == NULL)
			return NULL;
		shared->buffers[2] = record;
		MlnMemset(record, 0, sizeof(*record));
	}
	return record;
}

/* records a session */
unsigned int MlnImpl_81052D14(int id, int mode, unsigned int start, int owner)
{
	MlnSessionRecord *record = MlnSessionRecordGet(MlnImpl_81006EAC());

	if (record == NULL)
		return 2;
	record->id = id;
	record->mode = mode;
	record->start = start;
	record->owner = owner;
	return 0;
}

/* the session's id and mode; 0x220B0001 when there is none */
unsigned int MlnImpl_81052D84(int *id, int *mode)
{
	MlnSessionRecord *record = MlnSessionRecordGet(MlnImpl_81006EAC());

	if (record == NULL)
		return 2;
	if (record->mode == 0)
		return 0x220B0001;
	*id = record->id;
	*mode = record->mode;
	return 0;
}

/* when the session started */
unsigned int MlnImpl_81052E04(unsigned int *start)
{
	MlnSessionRecord *record = MlnSessionRecordGet(MlnImpl_81006EAC());

	if (record == NULL)
		return 2;
	*start = record->start;
	return 0;
}

/* who started the session */
unsigned int MlnImpl_81052E60(int *owner)
{
	MlnSessionRecord *record = MlnSessionRecordGet(MlnImpl_81006EAC());

	if (record == NULL)
		return 2;
	*owner = record->owner;
	return 0;
}

/* the session's id while one runs, else when the last one started */
unsigned int MlnImpl_81052EBC(unsigned int *seconds)
{
	MlnSessionRecord *record = MlnSessionRecordGet(MlnImpl_81006EAC());

	if (record != NULL && record->mode != 0) {
		*seconds = record->id;
		return 0;
	}
	return MlnImpl_81052E04(seconds);
}
