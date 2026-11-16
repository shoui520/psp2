/*
 * iofilter.cpp: the I/O filters. A filter is added to slot by index
 * (0 to FIOS_IOFILTER_INDEX_MAX), and sees every chunk tree and every command, in slot order.
 */
#include "fios2_internal.h"

FiosIOFilter s_ioFilters[FIOS_IOFILTER_INDEX_MAX + 1];
int s_ioFilterCount;

/* the filter's init runs first: a slot taken meanwhile is an error, and the filter is ended */
int fiosIOFilterAdd(int index, SceFiosIOFilterCallback pFilterCallback, void *pFilterContext)
{
	FiosIOFilterFunc pCallback = (FiosIOFilterFunc)pFilterCallback;
	int err;

	if (pFilterCallback == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	if (index < 0 || index > FIOS_IOFILTER_INDEX_MAX)
		return SCE_FIOS_ERROR_BAD_INDEX;
	if (s_ioFilters[index].pCallback != NULL)
		return SCE_FIOS_ERROR_BAD_INDEX;
	err = pCallback(pFilterContext, FIOS_FILTER_INIT, NULL);
	if (err == SCE_OK || err == SCE_FIOS_ERROR_EVENT_NOT_HANDLED) {
		s_ioFilterLock.WriteLock();
		if (s_ioFilters[index].pCallback == NULL) {
			s_ioFilters[index].pCallback = pFilterCallback;
			s_ioFilters[index].pContext = pFilterContext;
			s_ioFilterCount++;
			s_ioFilterLock.WriteUnlock();
			return SCE_OK;
		}
		s_ioFilterLock.WriteUnlock();
		err = SCE_FIOS_ERROR_BAD_INDEX;
		pCallback(pFilterContext, FIOS_FILTER_TERM, NULL);
	}
	return err;
}

int fiosIOFilterGetInfo(int index, SceFiosIOFilterCallback *pOutFilterCallback,
			void **pOutFilterContext)
{
	SceFiosIOFilterCallback pCallback;

	if (index < 0 || index > FIOS_IOFILTER_INDEX_MAX)
		return SCE_FIOS_ERROR_BAD_INDEX;
	s_ioFilterLock.ReadLock();
	pCallback = s_ioFilters[index].pCallback;
	if (pCallback == NULL) {
		s_ioFilterLock.ReadUnlock();
		return SCE_FIOS_ERROR_BAD_INDEX;
	}
	if (pOutFilterCallback != NULL)
		*pOutFilterCallback = pCallback;
	if (pOutFilterContext != NULL)
		*pOutFilterContext = s_ioFilters[index].pContext;
	s_ioFilterLock.ReadUnlock();
	return SCE_OK;
}

/* the slot is freed first, then the filter ended */
__attribute__((noinline)) int fiosIOFilterRemove(int index)
{
	FiosIOFilterFunc pCallback;
	void *pContext;
	int err;

	if (index < 0 || index > FIOS_IOFILTER_INDEX_MAX)
		return SCE_FIOS_ERROR_BAD_INDEX;
	s_ioFilterLock.WriteLock();
	pCallback = (FiosIOFilterFunc)s_ioFilters[index].pCallback;
	if (pCallback == NULL) {
		s_ioFilterLock.WriteUnlock();
		return SCE_FIOS_ERROR_BAD_INDEX;
	}
	pContext = s_ioFilters[index].pContext;
	s_ioFilterCount--;
	s_ioFilters[index].pContext = NULL;
	s_ioFilters[index].pCallback = NULL;
	s_ioFilterLock.WriteUnlock();
	err = pCallback(pContext, FIOS_FILTER_TERM, NULL);
	if (err != SCE_OK && err != SCE_FIOS_ERROR_EVENT_NOT_HANDLED)
		sceFiosPrintf("FIOS2 warning: %s returned error %d at termination\n",
			      fiosDebugDumpIOFilter((SceFiosIOFilterCallback)pCallback, pContext,
						    NULL, 0),
			      err);
	return SCE_OK;
}

/* each filter in turn may replace the chunk tree */
uint16_t fiosIOFiltersChunkTree(uint16_t rootID)
{
	FiosIOFilter *pFilter = s_ioFilters;
	uint16_t id;
	int n;
	int i;

	s_ioFilterLock.ReadLock();
	id = rootID;
	n = s_ioFilterCount;
	for (i = 0; i <= FIOS_IOFILTER_INDEX_MAX; i++, pFilter++) {
		FiosIOFilterFunc pCallback;

		if (n <= 0 || id == CHUNKID_INVALID)
			break;
		pCallback = (FiosIOFilterFunc)pFilter->pCallback;
		if (pCallback != NULL) {
			void *pContext = pFilter->pContext;
			int err = pCallback(pContext, FIOS_FILTER_CHUNK, &rootID);

			n--;
			if (err != SCE_OK && err != SCE_FIOS_ERROR_EVENT_NOT_HANDLED)
				sceFiosPrintf("FIOS2 warning: %s returned error %d while filtering "
					      "chunk tree %s\n",
					      fiosDebugDumpIOFilter((SceFiosIOFilterCallback)pCallback,
								    pContext, NULL, 0),
					      err, fiosDebugDumpChunk(rootID, NULL, 0));
		}
		id = rootID;
	}
	s_ioFilterLock.ReadUnlock();
	return id;
}

/* the index of the first filter that handles the command */
int fiosIOFiltersCall(int command, void *pArgs)
{
	FiosIOFilter *pFilter = s_ioFilters;
	int n;
	int i;

	s_ioFilterLock.ReadLock();
	n = s_ioFilterCount;
	for (i = 0; i <= FIOS_IOFILTER_INDEX_MAX; i++, pFilter++) {
		if (n <= 0)
			break;
		FiosIOFilterFunc pCallback = (FiosIOFilterFunc)pFilter->pCallback;
		void *pContext = pFilter->pContext;

		if (pCallback != NULL) {
			if (pCallback(pContext, command, pArgs) == SCE_OK) {
				s_ioFilterLock.ReadUnlock();
				return i;
			}
			n--;
		}
	}
	s_ioFilterLock.ReadUnlock();
	return SCE_FIOS_ERROR_BAD_INDEX;
}

void fiosIOFiltersTerm()
{
	int i;

	for (i = 0; i <= FIOS_IOFILTER_INDEX_MAX; i++)
		fiosIOFilterRemove(i);
}
