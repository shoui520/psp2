/*
 * Vita middleware record/resume/label stores: the extra key-bin
 * read hook plus the stubbed-out persistent move/resume/label stores that return fixed
 * handles (0x12345678), empty lists, or 12005 (not supported).
 */

#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>

extern int g_swKeyBinHandle;
int SwMwCoreKeyBinReadExtra(void *keyBin, void *buffer, int size, int *outSize);

/* extra key-bin read helper */
int SwMwReadKeyBinExtra(int unusedFirst, int unusedSecond, void *buffer, int size)
{
	int outSize;

	(void)unusedFirst;
	(void)unusedSecond;
	if (g_swKeyBinHandle != 0)
		SwMwCoreKeyBinReadExtra((void *)(uintptr_t)g_swKeyBinHandle, buffer, size,
					&outSize);
	return 0;
}

/* label store (recordB): (not supported on Vita) */
int SwMwManagerOpenRecordB(int *record)
{
	(void)record;
	return 12005;
}

int SwMwManagerCloseRecordB(int record)
{
	(void)record;
	return 12005;
}

int SwMwLabelStoreGet(int store, int label, int *outStatus)
{
	(void)store;
	(void)label;
	(void)outStatus;
	return 12005;
}

int SwMwLabelStoreSet(int store, int label, int status)
{
	(void)store;
	(void)label;
	(void)status;
	return 12005;
}

int SwMwLabelStoreClear(int store)
{
	(void)store;
	return 12005;
}

/* sink move resume store (recordD): */
int SwMwManagerOpenRecordD(int *record)
{
	*record = 0x12345678;
	return 0;
}

int SwMwManagerCloseRecordD(int record)
{
	(void)record;
	return 0;
}

int SwMwMoveRecordAppend(int store, const void *record)
{
	(void)store;
	(void)record;
	return 0;
}

int SwMwMoveRecordBegin(int store, int id, int phase)
{
	(void)store;
	(void)id;
	(void)phase;
	return 0;
}

int SwMwMoveListRelease(int store, int id)
{
	(void)store;
	(void)id;
	return 0;
}

int SwMwMoveListCount(int store, unsigned int *outCount)
{
	(void)store;
	*outCount = 0;
	return 0;
}

int SwMwMoveListGet(int store, void **outRecords, unsigned int *outCount)
{
	(void)store;
	(void)outRecords;
	*outCount = 0;
	return 0;
}

int SwMwMoveListFree(int store, void *records)
{
	(void)store;
	(void)records;
	return 0;
}

int SwMwMoveListUpdate(int store, int id, int value)
{
	(void)store;
	(void)id;
	(void)value;
	return 0;
}

int SwMwMoveListClear(int store)
{
	(void)store;
	return 0;
}

/* source move resume store (recordC): */
int SwMwManagerOpenRecordC(int *record)
{
	*record = 0x12345678;
	return 0;
}

int SwMwManagerCloseRecordC(int record)
{
	(void)record;
	return 0;
}

int SwMwSourceResumeAppend(int store, const void *record)
{
	(void)store;
	(void)record;
	return 0;
}

int SwMwSourceResumeRemove(int store, int id)
{
	(void)store;
	(void)id;
	return 0;
}

int SwMwSourceResumeCount(int store, unsigned int *outCount)
{
	(void)store;
	*outCount = 0;
	return 0;
}

int SwMwSourceResumeListGet(int store, void **outRecords, unsigned int *outCount)
{
	(void)store;
	(void)outRecords;
	*outCount = 0;
	return 0;
}

int SwMwSourceResumeListFree(int store, void *records)
{
	(void)store;
	(void)records;
	return 0;
}

int SwMwSourceResumeClear(int store)
{
	(void)store;
	return 0;
}

/* recordA store: */
int SwMwManagerOpenRecordA(int *record)
{
	(void)record;
	return 12005;
}

int SwMwManagerCloseRecordA(int record)
{
	(void)record;
	return 0;
}
