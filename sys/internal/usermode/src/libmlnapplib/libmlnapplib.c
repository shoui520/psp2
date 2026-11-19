/*
 * MarlinAppLib (vs0:sys/external/libmlnapplib.suprx): two contexts for the Marlin DRM library,
 * each a small state machine (closed, open, content set, licensed, ready), and the calls that
 * move them: open a session, set the content, read the license's validity and rights, and
 * decrypt a block.
 *
 * Built by Sony with SNC. No debug record names its files; the names are ours.
 */

#include <stddef.h>
#include <kernel/threadmgr.h>
#include <rtc.h>
#include "libmlnapplib.h"

extern int SceNpManager_DDF30B0E(void *pAccountId, int a1, SceUInt64 *pAccount);
extern int sceDrmBridgeGetCurrentSecureTick(SceUInt64 *pTick);

SceUID g_mutexId = -1;
int g_slots[MLNAPPLIB_NUM_CONTEXTS];
MarlinAppContext g_contexts[MLNAPPLIB_NUM_CONTEXTS];

/* the library's own copies (SNC does not inline them across calls) */
__attribute__((noinline))
static void *CopyContentType(void *pDst, const void *pSrc, int size)
{
	char *d = pDst;
	const char *s = pSrc;

	while (size-- != 0)
		*d++ = *s++;
	return pDst;
}

__attribute__((noinline))
static void *CopyBytes(void *pDst, const void *pSrc, int size)
{
	char *d = pDst;
	const char *s = pSrc;

	while (size-- != 0)
		*d++ = *s++;
	return pDst;
}

__attribute__((noinline))
static void *FillBytes(void *pDst, char value, int size)
{
	char *d = pDst;

	while (size-- != 0)
		*d++ = value;
	return pDst;
}

/* SceMarlin's result as this library's */
__attribute__((noinline))
static int ConvertResult(int result)
{
	switch (result) {
	case 0:
		return MLNAPPLIB_OK;
	case 2:
		return MLNAPPLIB_ERROR_PARAMETER;
	case 4:
		return MLNAPPLIB_ERROR_INVALID_ARG;
	case 23:
		return MLNAPPLIB_ERROR_NO_LICENSE;
	case 34:
		return MLNAPPLIB_ERROR_BAD_PADDING;
	default:
		return MLNAPPLIB_ERROR_FAILED;
	}
}

/* whether the signed in account has the service provider's ticket */
__attribute__((noinline))
static int CheckServiceProvider(char *pResult)
{
	char accountId[8] = { 0 };
	SceUInt64 account;
	char serviceId[256];
	SceUInt8 acid[264];
	MarlinBuffer service;
	MarlinBuffer tag;
	int params[3];
	int hContext;
	int *pTicket;
	int err = 0;
	int result;
	int count;
	int i;
	int r;

	FillBytes(accountId, 0, 8);
	if (SceNpManager_DDF30B0E(NULL, 0, &account) >= 0) {
		if (account != 0)
			CopyBytes(accountId, &account, 8);
		else
			err = MLNAPPLIB_ERROR_FAILED;
	} else {
		err = MLNAPPLIB_ERROR_FAILED;
	}
	result = err;
	if (err != 0) {
		result = MLNAPPLIB_ERROR_FAILED;
	} else {
		count = 0;
		for (i = 0; i < 8; i++) {
			if (accountId[i] != 0)
				count++;
		}
		if (count == 8) {
			if (pResult != NULL) {
				*pResult = 0;
				hContext = 0;
				pTicket = NULL;
				FillBytes(acid, 0, 264);
				FillBytes(serviceId, 0, 256);
				params[0] = 0;
				CopyBytes(serviceId, "urn:marlin:organization:sne:service-provider:1", 46);
				acid[0] = 0;
				acid[1] = 0;
				acid[2] = 0;
				acid[3] = 16;
				acid[4] = 'A';
				acid[5] = 'C';
				acid[6] = 'I';
				acid[7] = 'D';
				CopyBytes(&acid[8], accountId, 8);
				service.size = 46;
				service.pData = serviceId;
				tag.size = 16;
				tag.pData = acid;
				r = SceMarlin_9CD4CE1E(params, &hContext, 0);
				err = ConvertResult(r);
				if (err == 0) {
					r = SceMarlin_BCED6CC4(hContext, &service, &tag, &pTicket);
					err = ConvertResult(r);
					if (err == 0 && pTicket == NULL)
						*pResult = 1;
				}
				SceMarlin_CCA99803(pTicket);
				SceMarlin_8FEB4F94(hContext);
				result = err;
			} else {
				result = MLNAPPLIB_ERROR_INVALID_ARG;
			}
		}
	}
	return result;
}

__attribute__((noinline))
static int InitContext(MarlinAppContext *pContext)
{
	pContext->reserved2 = 0;
	pContext->session = -1;
	pContext->pReserved = &pContext->reserved1;
	pContext->reserved1 = 0;
	pContext->state = 0;
	pContext->validity = 0;
	pContext->pSession = &pContext->session;
	pContext->node = -1;
	pContext->reserved3 = 0;
	pContext->pLicense = NULL;
	pContext->pRights = NULL;
	FillBytes(pContext->key, 0, 272);
	pContext->keySize = 0;
	pContext->notBefore = 0;
	pContext->notAfter = 0;
	FillBytes(pContext->iv, 0, 32);
	FillBytes(&pContext->validityInfo, 0, 72);
	pContext->ppSlot = NULL;
	pContext->isBkdt = 1;
	return 1;
}

/* whether the event is allowed in the context's state */
__attribute__((noinline))
static int CheckState(const MarlinAppContext *pContext, int event)
{
	int err = 0;

	switch (pContext->state) {
	case MLNAPPLIB_STATE_CLOSED:
		if (event == MLNAPPLIB_EVENT_INIT) {
			if (*pContext->pSession != -1 || pContext->node != -1)
				err = MLNAPPLIB_ERROR_INVALID_STATE;
		} else {
			err = MLNAPPLIB_ERROR_INVALID_STATE;
		}
		break;
	case MLNAPPLIB_STATE_OPEN:
		if (event == MLNAPPLIB_EVENT_OPEN || event == MLNAPPLIB_EVENT_TERM) {
			if (*pContext->pSession != -1 || pContext->node != -1)
				err = MLNAPPLIB_ERROR_INVALID_STATE;
		} else {
			err = MLNAPPLIB_ERROR_INVALID_STATE;
		}
		break;
	case MLNAPPLIB_STATE_CONTENT_SET:
		if (event == MLNAPPLIB_EVENT_SET_CONTENT || event == MLNAPPLIB_EVENT_CLOSE) {
			if (*pContext->pSession == -1 || pContext->node != -1)
				err = MLNAPPLIB_ERROR_INVALID_STATE;
		} else {
			err = MLNAPPLIB_ERROR_INVALID_STATE;
		}
		break;
	case MLNAPPLIB_STATE_LICENSED:
		if (event == MLNAPPLIB_EVENT_GET_LICENSE || event == MLNAPPLIB_EVENT_CHECK_LICENSE ||
		    event == MLNAPPLIB_EVENT_CLOSE) {
			if (*pContext->pSession == -1 || pContext->node == -1)
				err = MLNAPPLIB_ERROR_INVALID_STATE;
		} else {
			err = MLNAPPLIB_ERROR_INVALID_STATE;
		}
		break;
	case MLNAPPLIB_STATE_READY:
		if (event == MLNAPPLIB_EVENT_GET_LICENSE || event == MLNAPPLIB_EVENT_DECRYPT ||
		    event == MLNAPPLIB_EVENT_CLOSE) {
			if (*pContext->pSession == -1 || pContext->node == -1)
				err = MLNAPPLIB_ERROR_INVALID_STATE;
		} else {
			err = MLNAPPLIB_ERROR_INVALID_STATE;
		}
		break;
	default:
		err = MLNAPPLIB_ERROR_FAILED;
		break;
	}
	return err;
}

/* the state after the event, when it was done without an error */
__attribute__((noinline))
static int UpdateState(MarlinAppContext *pContext, int err, int event)
{
	int state;

	if (err == 0) {
		state = pContext->state;
		switch (state) {
		case MLNAPPLIB_STATE_CLOSED:
			state = MLNAPPLIB_STATE_OPEN;
			break;
		case MLNAPPLIB_STATE_OPEN:
			if (event == MLNAPPLIB_EVENT_OPEN) {
				state = MLNAPPLIB_STATE_CONTENT_SET;
			} else if (event == MLNAPPLIB_EVENT_TERM) {
				InitContext(pContext);
				state = MLNAPPLIB_STATE_CLOSED;
			}
			break;
		case MLNAPPLIB_STATE_CONTENT_SET:
			if (event == MLNAPPLIB_EVENT_CLOSE) {
				FillBytes(&pContext->validityInfo, 0, 72);
				state = MLNAPPLIB_STATE_OPEN;
			} else if (event == MLNAPPLIB_EVENT_SET_CONTENT) {
				state = MLNAPPLIB_STATE_LICENSED;
			}
			break;
		case MLNAPPLIB_STATE_LICENSED:
			if (event == MLNAPPLIB_EVENT_CHECK_LICENSE)
				state = MLNAPPLIB_STATE_READY;
			else if (event == MLNAPPLIB_EVENT_CLOSE)
				state = MLNAPPLIB_STATE_OPEN;
			break;
		case MLNAPPLIB_STATE_READY:
			if (event == MLNAPPLIB_EVENT_CLOSE)
				state = MLNAPPLIB_STATE_OPEN;
			break;
		default:
			break;
		}
		pContext->state = state;
	}
	return err;
}

/* the time as SceDrmBridge keeps it, in minutes since 0001-01-01 less the Unix epoch's */
__attribute__((noinline))
static int GetCurrentTime(SceUInt64 *pTime)
{
	SceUInt64 tick;

	if (sceDrmBridgeGetCurrentSecureTick(&tick) != 0)
		return MLNAPPLIB_ERROR_FAILED;
	*pTime = (tick - 62135596800000000ULL) / 60000000ULL;
	return MLNAPPLIB_OK;
}

/* a date to the same minutes */
__attribute__((noinline))
static int DateToTime(const MarlinDateTime *pDate, SceUInt64 *pTime)
{
	SceRtcTick tick;
	SceDateTime time;
	int err = 0;

	if (pDate->year > 0 && pDate->year <= 9999)
		time.year = pDate->year;
	if (pDate->month > 0 && pDate->month <= 12)
		time.month = pDate->month;
	if (pDate->day > 0 && pDate->day <= 31)
		time.day = pDate->day;
	if (pDate->hour >= 0 && pDate->hour <= 23)
		time.hour = pDate->hour;
	if (pDate->minute >= 0 && pDate->minute <= 59)
		time.minute = pDate->minute;
	if (pDate->second >= 0 && pDate->second <= 59)
		time.second = pDate->second;
	if (pDate->microsecond >= 0 && pDate->microsecond <= 999999)
		time.microsecond = pDate->microsecond;
	if (sceRtcGetTick(&time, &tick) != 0) {
		err = MLNAPPLIB_ERROR_FAILED;
	} else if (tick.tick < 62135596800000000ULL) {
		err = MLNAPPLIB_ERROR_FAILED;
	} else {
		*pTime = (tick.tick - 62135596800000000ULL) / 60000000ULL;
	}
	return err;
}

/* SceMarlin's date (milliseconds) as a SceDateTime */
static int ConvertDate(const MarlinDate *pSrc, MarlinDateTime *pDst)
{
	if (pSrc == NULL || pDst == NULL)
		return MLNAPPLIB_ERROR_FAILED;
	pDst->year = pSrc->year;
	pDst->month = pSrc->month;
	pDst->day = pSrc->day;
	pDst->hour = pSrc->hour;
	pDst->minute = pSrc->minute;
	pDst->second = pSrc->second;
	pDst->microsecond = 1000 * pSrc->millisecond;
	return MLNAPPLIB_OK;
}

/* the license's validity, as the context keeps it */
__attribute__((noinline))
static int SetValidity(MarlinAppContext *pContext, const MarlinLicenseInfo *pInfo)
{
	SceUInt64 notBefore = 0;
	SceUInt64 notAfter = 0;
	int err = 0;

	pContext->validityInfo.flags = 0;
	if ((pInfo->flags & MARLIN_VALIDITY_NOT_BEFORE) != 0) {
		err = ConvertDate(pInfo->pNotBefore, &pContext->validityInfo.dates[0]);
		if (err != 0)
			return err;
		err = DateToTime(&pContext->validityInfo.dates[0], &pContext->notBefore);
		if (err != 0)
			return err;
		pContext->validityInfo.flags |= MARLIN_VALIDITY_NOT_BEFORE;
		if (pInfo->numNotBefore > 1) {
			err = ConvertDate(pInfo->pLaterNotBefore, &pContext->validityInfo.dates[2]);
			if (err != 0)
				return err;
			err = DateToTime(&pContext->validityInfo.dates[2], &notBefore);
			if (err != 0)
				return err;
			if (notBefore > pContext->notBefore)
				pContext->notBefore = notBefore;
			pContext->validityInfo.flags |= MARLIN_VALIDITY_NOT_BEFORE_LATER;
		}
	}
	if ((pInfo->flags & MARLIN_VALIDITY_NOT_AFTER) != 0) {
		err = ConvertDate(pInfo->pNotAfter, &pContext->validityInfo.dates[1]);
		if (err != 0)
			return err;
		err = DateToTime(&pContext->validityInfo.dates[1], &pContext->notAfter);
		if (err != 0)
			return err;
		pContext->validityInfo.flags |= MARLIN_VALIDITY_NOT_AFTER;
		if (pInfo->numNotAfter > 1) {
			err = ConvertDate(pInfo->pEarlierNotAfter, &pContext->validityInfo.dates[3]);
			if (err != 0)
				return err;
			err = DateToTime(&pContext->validityInfo.dates[3], &notAfter);
			if (err != 0)
				return err;
			if (notAfter < pContext->notAfter)
				pContext->notAfter = notAfter;
			pContext->validityInfo.flags |= MARLIN_VALIDITY_NOT_AFTER_EARLIER;
		}
	}
	if ((pInfo->flags & 8) != 0) {
		pContext->validityInfo.limit = pInfo->limit;
		pContext->validityInfo.flags |= MARLIN_VALIDITY_LIMIT;
	}
	return err;
}

/* the state the time puts a license that is otherwise validState in: before (2) or after (3) its window */
static int ClassifyTime(const MarlinAppContext *pContext, SceUInt64 now, int validState)
{
	int cmp = -1;
	int state = validState;

	if (now >= pContext->notBefore) {
		cmp = 0;
		if (now > pContext->notAfter)
			cmp = 1;
	}
	if (cmp != 0) {
		state = 2;
		if (cmp > 0)
			state = 3;
	}
	return state;
}

/* the license's state, from the rights and the time (a zero time is read) */
static int GetValidityState(const MarlinAppContext *pContext, int validState, SceUInt64 now, int *pState)
{
	int err = 0;

	if (pContext->validityInfo.limit != 0) {
		if (now == 0) {
			err = GetCurrentTime(&now);
			if (err != 0)
				return err;
		}
		validState = ClassifyTime(pContext, now, validState);
	}
	*pState = validState;
	return err;
}

/* the state a content's rights put it in */
__attribute__((noinline))
static int CheckRights(MarlinAppContext *pContext, const MarlinRights *pRights, int *pState)
{
	SceUInt64 now;
	char valid;
	int state = 0;
	int err;

	if (pContext->validityInfo.limit != 0) {
		if (pContext->notBefore == 0 || pContext->notAfter == 0 || pContext->notBefore > pContext->notAfter)
			return 4;
		valid = 0;
		err = GetCurrentTime(&now);
		if (err != 0)
			return err;
		if (now == 0 || (pRights->flagsA & 0x20) != 0) {
			*pState = 5;
			return 0;
		}
		if ((pRights->flagsA & 1) == 0) {
			state = 6;
			if ((pRights->flagsA & 2) == 0) {
				if ((pRights->flagsB & 2) != 0 || (pRights->flagsB & 8) != 0) {
					state = 3;
				} else {
					state = 7;
					if ((pRights->flagsB & 1) != 0)
						state = 2;
				}
			}
			*pState = state;
			return 0;
		}
		err = CheckServiceProvider(&valid);
		if (err != 0)
			return err;
		if (valid != 0)
			err = GetValidityState(pContext, 4, now, pState);
		else
			err = GetValidityState(pContext, 8, now, pState);
		return err;
	} else {
		err = 0;
		if ((pRights->flagsA & 1) != 0) {
			valid = 0;
			err = CheckServiceProvider(&valid);
			if (err != 0)
				return err;
			state = 8;
			if (valid != 0)
				state = 4;
		} else {
			state = 7;
		}
		*pState = state;
		return err;
	}
}

/* the context at the pointer, if the caller's is one of the table's and open */
static int FindContext(const MarlinAppContext *pContext)
{
	unsigned int i;

	for (i = 0; i < MLNAPPLIB_NUM_CONTEXTS; i++) {
		if (pContext == &g_contexts[i])
			break;
	}
	return i;
}

/* mlnapplib_2FE40F79: initialise the library (the Marlin library with the caller's four arguments) */
int mlnapplib_2FE40F79(int a0, int a1, int a2, int a3)
{
	SceKernelMutexOptParam option;
	int index = 0;
	int err = 0;
	int i;

	if (a0 != 0 && a1 != 0 && a2 != 0 && a3 != 0) {
		option.size = 8;
		g_mutexId = sceKernelCreateMutex("SceMlnAppLib", 0, 0, &option);
		if (g_mutexId <= 0)
			return MLNAPPLIB_ERROR_FAILED;
		for (i = 0; i < MLNAPPLIB_NUM_CONTEXTS; i++) {
			g_slots[i] = 0;
			InitContext(&g_contexts[i]);
		}
		err = 0;
		if (sceKernelLockMutex(g_mutexId, 1, NULL) != 0)
			err = MLNAPPLIB_ERROR_FAILED;
		if (err == 0) {
			for (i = 0; i < MLNAPPLIB_NUM_CONTEXTS; i++) {
				err = CheckState(&g_contexts[i], MLNAPPLIB_EVENT_INIT);
				if (err != 0)
					break;
				index = i;
				if (i + 1 >= MLNAPPLIB_NUM_CONTEXTS) {
					err = ConvertResult(SceMarlin_83B44D33(a0, a1, a2, a3));
					break;
				}
			}
		}
	} else {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
	}
	if (index == 1) {
		for (i = 0; i < MLNAPPLIB_NUM_CONTEXTS; i++)
			err = UpdateState(&g_contexts[i], err, MLNAPPLIB_EVENT_INIT);
	}
	sceKernelUnlockMutex(g_mutexId, 1);
	return err;
}

/* mlnapplib_5D6656EC: terminate it */
int mlnapplib_5D6656EC(void)
{
	int index = 0;
	int err = 0;
	int i;

	if (sceKernelLockMutex(g_mutexId, 1, NULL) != 0)
		err = MLNAPPLIB_ERROR_FAILED;
	if (err == 0) {
		for (i = 0; i < MLNAPPLIB_NUM_CONTEXTS; i++) {
			err = CheckState(&g_contexts[i], MLNAPPLIB_EVENT_TERM);
			if (err != 0)
				break;
			index = i;
			if (i + 1 >= MLNAPPLIB_NUM_CONTEXTS) {
				SceMarlin_C76DA6C0();
				break;
			}
		}
	}
	if (index == 1) {
		for (i = 0; i < MLNAPPLIB_NUM_CONTEXTS; i++)
			err = UpdateState(&g_contexts[i], err, MLNAPPLIB_EVENT_TERM);
	}
	sceKernelUnlockMutex(g_mutexId, 1);
	return sceKernelDeleteMutex(g_mutexId);
}

/* mlnapplib_08F83AB1: open a context and a session in it (the caller's two values go to the Marlin library) */
int mlnapplib_08F83AB1(int a0, int a1, MarlinAppContext **ppContext)
{
	MarlinAppContext *pContext;
	int err = 0;
	int index = 0;

	while (g_slots[index] != 0) {
		index++;
		if (index >= MLNAPPLIB_NUM_CONTEXTS)
			return MLNAPPLIB_ERROR_INVALID_STATE;
	}
	pContext = &g_contexts[index];
	if (a0 != 0 && a1 != 0 && ppContext != NULL) {
		if (sceKernelLockMutex(g_mutexId, 1, NULL) != 0)
			err = MLNAPPLIB_ERROR_FAILED;
		if (err == 0) {
			g_slots[index] = (int)pContext;
			pContext->ppSlot = &g_slots[index];
			err = CheckState(pContext, MLNAPPLIB_EVENT_OPEN);
			if (err == 0) {
				err = ConvertResult(SceMarlin_A43BC777(pContext->pSession));
				if (err == 0) {
					err = ConvertResult(SceMarlin_FB95B259(pContext->pSession, a1, a0));
					if (err == 0)
						*ppContext = (MarlinAppContext *)g_slots[index];
				}
			}
		}
	} else {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
	}
	if (err != 0 && index < MLNAPPLIB_NUM_CONTEXTS) {
		if (*pContext->pSession != -1) {
			int r = SceMarlin_23838449(pContext->pSession);

			*pContext->pSession = -1;
			ConvertResult(r);
		}
		g_slots[index] = 0;
	}
	err = UpdateState(pContext, err, MLNAPPLIB_EVENT_OPEN);
	sceKernelUnlockMutex(g_mutexId, 1);
	return err;
}

/* mlnapplib_993137AF: close a context: its content, session and license */
int mlnapplib_993137AF(MarlinAppContext *pContext)
{
	int err;
	int *pSession;
	int r;

	if (FindContext(pContext) == MLNAPPLIB_NUM_CONTEXTS) {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
	} else {
		if (pContext != NULL && (int)pContext == *pContext->ppSlot) {
			err = 0;
			if (sceKernelLockMutex(g_mutexId, 1, NULL) != 0)
				err = MLNAPPLIB_ERROR_FAILED;
			if (err == 0) {
				err = CheckState(pContext, MLNAPPLIB_EVENT_CLOSE);
				if (err == 0) {
					pSession = pContext->pSession;
					if (pContext->node != -1) {
						r = SceMarlin_49752AFE(pSession);
						pContext->node = -1;
						ConvertResult(r);
						pSession = pContext->pSession;
					}
					if (*pSession != -1) {
						r = SceMarlin_23838449(pSession);
						*pContext->pSession = -1;
						ConvertResult(r);
					}
					if (pContext->pLicense != NULL) {
						SceMarlin_2FD092BD(pContext->pLicense);
						pContext->pLicense = NULL;
					}
					if (pContext->pRights != NULL) {
						SceMarlin_2FD092BD(pContext->pRights);
						pContext->pRights = NULL;
					}
					pContext->validity = 0;
					pContext->notBefore = 0;
					pContext->notAfter = 0;
					FillBytes(pContext->key, 0, 272);
					pContext->keySize = 0;
					FillBytes(pContext->iv, 0, 32);
					FillBytes(&pContext->validityInfo, 0, 72);
				}
			}
		} else {
			err = MLNAPPLIB_ERROR_INVALID_ARG;
		}
		err = UpdateState(pContext, err, MLNAPPLIB_EVENT_CLOSE);
		if (pContext != NULL)
			*pContext->ppSlot = 0;
	}
	sceKernelUnlockMutex(g_mutexId, 1);
	return err;
}

/* mlnapplib_627F6BE5: set the content: its ID, key and what it is (0: BKDT, 1: audio) */
int mlnapplib_627F6BE5(MarlinAppContext *pContext, unsigned int type, const void *pData, int size)
{
	int node = -1;
	MarlinContentInfo info;
	MarlinBindParam param;
	SceUInt8 bkdt[48];
	SceUInt8 audio[48];
	unsigned int i;
	int err;
	int r;

	CopyContentType(bkdt, "urn:marlin:organization:sne:content-type:BKDT", 46);
	CopyContentType(audio, "urn:marlin:organization:sne:content-type:audio", 47);
	if (FindContext(pContext) == MLNAPPLIB_NUM_CONTEXTS) {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto unlock;
	}
	if (pContext == NULL || (int)pContext != *pContext->ppSlot) {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto fail;
	}
	err = 0;
	if (sceKernelLockMutex(g_mutexId, 1, NULL) != 0)
		err = MLNAPPLIB_ERROR_FAILED;
	if (err == 0) {
		err = CheckState(pContext, MLNAPPLIB_EVENT_SET_CONTENT);
		if (err == 0) {
			r = SceMarlin_A11B25C7(pData, size, &info);
			err = ConvertResult(r);
			if (err == 0) {
				if (&pContext->key == NULL) {
					err = MLNAPPLIB_ERROR_INVALID_CONTENT;
					goto fail;
				}
				if (type != 0) {
					if (type > 1 || info.keySize != 63) {
						err = MLNAPPLIB_ERROR_INVALID_CONTENT;
						goto fail;
					}
					for (i = 0; i < 47; i++) {
						if (info.pKey[16 + i] != audio[i]) {
							err = MLNAPPLIB_ERROR_INVALID_CONTENT;
							goto fail;
						}
					}
					pContext->isBkdt = 0;
				} else {
					if (info.keySize != 62) {
						err = MLNAPPLIB_ERROR_INVALID_CONTENT;
						goto fail;
					}
					for (i = 0; i < 46; i++) {
						if (info.pKey[16 + i] != bkdt[i]) {
							err = MLNAPPLIB_ERROR_INVALID_CONTENT;
							goto fail;
						}
					}
					pContext->isBkdt = 1;
				}
				if (info.contentIdSize > 512) {
					err = MLNAPPLIB_ERROR_INVALID_CONTENT;
					goto fail;
				}
				FillBytes(pContext->contentIdBuf, 0, 512);
				CopyBytes(pContext->contentIdBuf, info.pContentId, info.contentIdSize - 1);
				pContext->contentId.pData = pContext->contentIdBuf;
				pContext->contentIdBufSize = info.contentIdSize - 1;
				pContext->contentId.size = info.contentIdSize - 1;
				param.pContentId = &pContext->contentId;
				param.flagA = 0;
				param.type = 2;
				param.flagB = 0;
				param.encrypted = type != 0;
				r = SceMarlin_102593F5(pContext->pSession, &param, &node);
				err = ConvertResult(r);
				if (err == 0) {
					pContext->node = node;
					node = -1;
					if (info.keySize > 272) {
						err = MLNAPPLIB_ERROR_INVALID_CONTENT;
						goto fail;
					}
					CopyBytes(pContext->key, info.pKey, info.keySize);
					pContext->keySize = info.keySize;
					CopyBytes(pContext->iv, info.pIv, 32);
				}
			}
		}
	}
	if (err != 0) {
fail:
		if (pContext->node != -1) {
			r = SceMarlin_49752AFE(pContext->pSession);
			pContext->node = -1;
			ConvertResult(r);
		}
	}
	err = UpdateState(pContext, err, MLNAPPLIB_EVENT_SET_CONTENT);
unlock:
	sceKernelUnlockMutex(g_mutexId, 1);
	return err;
}

/* mlnapplib_E271FA8B: the license's state and validity (a copy of the context's) */
int mlnapplib_E271FA8B(MarlinAppContext *pContext, int *pState, MarlinValidity *pValidity)
{
	MarlinLicenseInfo *pLicense = NULL;
	int state = 0;
	int skipUpdate = 0;
	int err;
	int r;

	if (FindContext(pContext) == MLNAPPLIB_NUM_CONTEXTS) {
		skipUpdate = 1;
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto done;
	}
	if (pContext == NULL || (int)pContext != *pContext->ppSlot) {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto done;
	}
	if (pState == NULL || pValidity == NULL) {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto done;
	}
	err = 0;
	if (sceKernelLockMutex(g_mutexId, 1, NULL) != 0)
		err = MLNAPPLIB_ERROR_FAILED;
	if (err != 0)
		goto done;
	err = CheckState(pContext, MLNAPPLIB_EVENT_GET_LICENSE);
	if (err != 0)
		goto done;
	r = SceMarlin_A2E65A7E(pContext->pSession, 0, (void **)&pLicense);
	err = ConvertResult(r);
	if (err != 0)
		goto done;
	if (pLicense == NULL) {
		err = MLNAPPLIB_ERROR_NO_LICENSE;
		goto done;
	}
	err = SetValidity(pContext, pLicense);
	if (err != 0)
		goto done;
	switch (pContext->validity) {
	case 0:
		r = SceMarlin_92A6A48F(pContext->pSession, 0, &pContext->pRights);
		if (r == 0) {
			state = 1;
			goto commit;
		}
		if (r == 29) {
			err = CheckRights(pContext, pContext->pRights, &state);
			if (err == 0)
				goto commit;
			goto done;
		}
		if (r == 69) {
			err = GetValidityState(pContext, 6, 0, &state);
			if (err == 0)
				goto commit;
			goto done;
		}
		err = ConvertResult(r);
		if (err == 0)
			goto commit;
		break;
	case 1:
	case 4:
	case 5:
	case 6:
	case 8:
		err = GetValidityState(pContext, pContext->validity, 0, &state);
		if (err == 0)
			goto commit;
		break;
	case 2:
		err = GetValidityState(pContext, 1, 0, &state);
		if (err == 0)
			goto commit;
		break;
	case 3:
	case 7:
		state = pContext->validity;
commit:
		if (pContext->pLicense != NULL)
			SceMarlin_2FD092BD(pContext->pLicense);
		pContext->pLicense = pLicense;
		pLicense = NULL;
		*pValidity = pContext->validityInfo;
		pContext->validity = state;
		*pState = state;
		break;
	default:
		err = MLNAPPLIB_ERROR_FAILED;
		break;
	}
done:
	if (pLicense != NULL)
		SceMarlin_2FD092BD(pLicense);
	if (skipUpdate == 0)
		err = UpdateState(pContext, err, MLNAPPLIB_EVENT_GET_LICENSE);
	sceKernelUnlockMutex(g_mutexId, 1);
	return err;
}

/* mlnapplib_09482691: check the license for a video (0: not allowed, 1: ...) */
int mlnapplib_09482691(MarlinAppContext *pContext, unsigned int *pState)
{
	MarlinLicenseInfo *pLicense = NULL;
	MarlinRights *pRights = NULL;
	int state = 0;
	SceUInt8 video[48];
	int skipUpdate = 0;
	unsigned int i;
	int err;
	int r;

	CopyContentType(video, "urn:marlin:organization:sne:content-type:video", 47);
	if (FindContext(pContext) == MLNAPPLIB_NUM_CONTEXTS) {
		skipUpdate = 1;
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto done;
	}
	if (pContext == NULL || (int)pContext != *pContext->ppSlot) {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto done;
	}
	if (pState == NULL) {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto done;
	}
	err = 0;
	if (sceKernelLockMutex(g_mutexId, 1, NULL) != 0)
		err = MLNAPPLIB_ERROR_FAILED;
	if (err != 0)
		goto done;
	err = CheckState(pContext, MLNAPPLIB_EVENT_CHECK_LICENSE);
	if (err != 0)
		goto done;
	r = SceMarlin_A2E65A7E(pContext->pSession, 0, (void **)&pLicense);
	err = ConvertResult(r);
	if (err != 0)
		goto done;
	err = SetValidity(pContext, pLicense);
	if (err != 0)
		goto done;
	r = SceMarlin_F450D554(pContext->pSession, 0, (void **)&pRights);
	if (r == 0) {
		state = 1;
		goto bind;
	}
	if (r == 29) {
		err = CheckRights(pContext, pRights, &state);
		if (err != 0)
			goto done;
	} else if (r != 69) {
		err = ConvertResult(r);
		if (err != 0)
			goto done;
	} else {
		err = GetValidityState(pContext, 6, 0, &state);
		if (err != 0)
			goto done;
	}
	if (state != 1) {
set:
		pContext->validity = state;
		*pState = state;
		if (state != 1)
			err = 6;
		goto done;
	}
bind:
	if (&pContext->key == NULL) {
		err = MLNAPPLIB_ERROR_INVALID_CONTENT;
		goto done;
	}
	if (pContext->keySize == 63) {
		for (i = 0; i < 47; i++) {
			if (pContext->key[16 + i] != video[i])
				break;
		}
		if (i == 47) {
			err = MLNAPPLIB_ERROR_NO_LICENSE;
			goto done;
		}
	}
	r = SceMarlin_EB4A1345(pContext->pSession, pContext->node, 1, pContext->keySize, pContext->key, 32,
	    pContext->iv);
	if (r == 34) {
		err = MLNAPPLIB_ERROR_INVALID_CONTENT;
		goto done;
	}
	if (r != 0) {
		err = MLNAPPLIB_ERROR_FAILED;
		goto done;
	}
	goto set;
done:
	if (pLicense != NULL)
		SceMarlin_2FD092BD(pLicense);
	if (pRights != NULL)
		SceMarlin_2FD092BD(pRights);
	if (skipUpdate == 0)
		err = UpdateState(pContext, err, MLNAPPLIB_EVENT_CHECK_LICENSE);
	sceKernelUnlockMutex(g_mutexId, 1);
	return err;
}

/* mlnapplib_873FA8C6: decrypt a block with the content's key */
int mlnapplib_873FA8C6(MarlinAppContext *pContext, SceUInt32 inputSize, const void *pInput, void *pOut,
    SceUInt32 *pOutSize)
{
	MarlinValidity *pValidity = NULL;
	SceUInt64 now;
	int err;
	int r;

	if (pContext != NULL)
		pValidity = &pContext->validityInfo;
	if (FindContext(pContext) == MLNAPPLIB_NUM_CONTEXTS) {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto unlock;
	}
	if (pContext == NULL || (int)pContext != *pContext->ppSlot || inputSize == 0 || pInput == NULL ||
	    pOut == NULL || pOutSize == NULL) {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto update;
	}
	err = 0;
	if (sceKernelLockMutex(g_mutexId, 1, NULL) != 0)
		err = MLNAPPLIB_ERROR_FAILED;
	if (err != 0)
		goto update;
	err = CheckState(pContext, MLNAPPLIB_EVENT_DECRYPT);
	if (err != 0)
		goto update;
	if (pValidity->limit != 0) {
		err = GetCurrentTime(&now);
		if (err != 0)
			goto update;
		if (pContext->notAfter != 0 && now > pContext->notAfter) {
			err = MLNAPPLIB_ERROR_EXPIRED;
			goto update;
		}
	}
	r = SceMarlin_2F1E4047(pContext->pSession, pContext->node, pInput, inputSize, pOutSize, pOut);
	err = ConvertResult(r);
update:
	err = UpdateState(pContext, err, MLNAPPLIB_EVENT_DECRYPT);
unlock:
	sceKernelUnlockMutex(g_mutexId, 1);
	return err;
}

/* mlnapplib_6FECE360: decrypt the next block of a stream of totalSize bytes, and take its padding off the last */
int mlnapplib_6FECE360(MarlinAppContext *pContext, SceUInt32 totalSize, SceUInt32 inputSize,
    const void *pInput, SceUInt8 *pOut, SceUInt32 *pOutSize, SceUInt32 *pDone)
{
	MarlinValidity *pValidity = NULL;
	SceUInt32 last = totalSize - 16;
	SceUInt64 now;
	SceUInt32 done;
	SceUInt32 padding;
	int err;
	int r;

	if (pContext != NULL)
		pValidity = &pContext->validityInfo;
	if (FindContext(pContext) == MLNAPPLIB_NUM_CONTEXTS) {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto unlock;
	}
	if (pContext == NULL || (int)pContext != *pContext->ppSlot || inputSize == 0 || pInput == NULL ||
	    pOut == NULL || pOutSize == NULL || pDone == NULL) {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto update;
	}
	err = 0;
	if (sceKernelLockMutex(g_mutexId, 1, NULL) != 0)
		err = MLNAPPLIB_ERROR_FAILED;
	if (err != 0)
		goto update;
	err = CheckState(pContext, MLNAPPLIB_EVENT_DECRYPT);
	if (err != 0)
		goto update;
	if (pValidity->limit != 0) {
		err = GetCurrentTime(&now);
		if (err != 0)
			goto update;
		if (pContext->notAfter != 0 && now > pContext->notAfter) {
			err = MLNAPPLIB_ERROR_EXPIRED;
			goto update;
		}
	}
	r = SceMarlin_2F1E4047(pContext->pSession, pContext->node, pInput, inputSize, pOutSize, pOut);
	err = ConvertResult(r);
	if (err == 0) {
		done = *pDone + *pOutSize;
		*pDone = done;
		if (done >= last) {
			padding = pOut[*pOutSize - 1];
			if (padding > 16)
				err = MLNAPPLIB_ERROR_BAD_PADDING;
			else
				*pOutSize -= padding;
		}
	}
update:
	err = UpdateState(pContext, err, MLNAPPLIB_EVENT_DECRYPT);
unlock:
	sceKernelUnlockMutex(g_mutexId, 1);
	return err;
}

/* what a control sets of the result */
__attribute__((noinline))
static void ParseControl(const MarlinControlValue *pValue, MarlinControlResult *pResult)
{
	SceUInt32 mask;
	int copyControl, outputLevel, imageConstraint, analogProtection, hdcpRequired;

	if (pValue != NULL && pResult != NULL && pValue->version == 1) {
		mask = pValue->mask;
		copyControl = pValue->copyControl;
		outputLevel = pValue->outputLevel;
		imageConstraint = pValue->imageConstraint;
		analogProtection = pValue->analogProtection;
		hdcpRequired = pValue->hdcpRequired;
		if ((mask & 2) != 0) {
			if (copyControl == 0)
				pResult->copyControl = 0;
			if (copyControl == 1)
				pResult->copyControl = 1;
		}
		if ((mask & 4) != 0) {
			if (outputLevel == 0)
				pResult->outputLevel = 1;
			if (outputLevel == 1)
				pResult->outputLevel = 2;
			if (outputLevel == 2)
				pResult->outputLevel = 2;
			if (outputLevel == 3)
				pResult->outputLevel = 2;
		}
		if ((mask & 8) != 0) {
			if (imageConstraint == 0)
				pResult->imageConstraint = 1;
			if (imageConstraint == 1)
				pResult->imageConstraint = 0;
		}
		if ((mask & 0x20) != 0) {
			if (analogProtection == 0)
				pResult->analogProtection = 0;
			if (analogProtection == 1)
				pResult->analogProtection = 1;
			if (analogProtection == 2)
				pResult->analogProtection = 2;
			if (analogProtection == 3)
				pResult->analogProtection = 3;
		}
		if ((mask & 0x10) != 0) {
			if (hdcpRequired == 0)
				pResult->hdcpRequired = 0;
			if (hdcpRequired == 1)
				pResult->hdcpRequired = 1;
		}
	}
}

/* mlnapplib_B074B00E: what the license's controls say of the output */
int mlnapplib_B074B00E(MarlinAppContext *pContext, MarlinControlResult *pResult)
{
	MarlinRights *pLocal = NULL;
	const MarlinRights *pRights;
	int skipUpdate = 0;
	unsigned int i;
	int err;
	int r;

	if (FindContext(pContext) == MLNAPPLIB_NUM_CONTEXTS) {
		skipUpdate = 1;
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto done;
	}
	if (pContext == NULL || (int)pContext != *pContext->ppSlot) {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto done;
	}
	if (pResult == NULL) {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
		goto done;
	}
	err = 0;
	if (sceKernelLockMutex(g_mutexId, 1, NULL) != 0)
		err = MLNAPPLIB_ERROR_FAILED;
	if (err != 0)
		goto done;
	err = CheckState(pContext, MLNAPPLIB_EVENT_GET_LICENSE);
	if (err != 0)
		goto done;
	if (pContext->validity == 0) {
		r = SceMarlin_92A6A48F(pContext->pSession, 0, (void **)&pLocal);
		err = ConvertResult(r);
		if (err != 0)
			goto done;
	}
	pRights = pContext->pRights;
	if (((SceUInt32)pRights | (SceUInt32)pLocal) != 0) {
		if (pContext->validity == 0)
			pRights = pLocal;
		if (pRights != NULL) {
			pResult->copyControl = 1;
			pResult->outputLevel = 1;
			pResult->analogProtection = 0;
			pResult->imageConstraint = 0;
			pResult->hdcpRequired = 0;
			pResult->reserved1 = 0;
			pResult->reserved2 = 0;
			err = 0;
			for (i = 0; i < pRights->numControls; i++) {
				const MarlinControlEntry *pEntry = &pRights->pControls[i];

				if (pEntry->type == 3) {
					ParseControl(pEntry->pValue, pResult);
				} else if ((pEntry->flags & 1) != 0) {
					err = MLNAPPLIB_ERROR_FAILED;
					goto check;
				}
			}
			for (i = 0; i < pRights->numActions; i++) {
				const MarlinActionEntry *pEntry = &pRights->pActions[i];

				if (pEntry->type == 1) {
					ParseControl(pEntry->pValue, pResult);
				} else if ((pEntry->flags & 1) != 0) {
					err = MLNAPPLIB_ERROR_FAILED;
					goto check;
				}
			}
			if (pRights->numOptions != 0) {
				for (i = 0; i < pRights->numOptions; i++) {
					if ((pRights->pOptions[i].flags & 1) != 0) {
						err = MLNAPPLIB_ERROR_FAILED;
						break;
					}
				}
			}
check:
			if (err != 0)
				err = MLNAPPLIB_ERROR_NO_LICENSE;
		} else {
			err = MLNAPPLIB_ERROR_NO_LICENSE;
		}
	} else {
		err = MLNAPPLIB_ERROR_FAILED;
	}
done:
	if (pLocal != NULL)
		SceMarlin_2FD092BD(pLocal);
	if (skipUpdate == 0)
		err = UpdateState(pContext, err, MLNAPPLIB_EVENT_GET_LICENSE);
	sceKernelUnlockMutex(g_mutexId, 1);
	return err;
}

/*
 * mlnapplib_E775B916: the validity of the ticket an ID (of size bytes, at most 256) is licensed by,
 * for the signed in account: its state, and its not-before and not-after dates.
 */
int mlnapplib_E775B916(SceUInt32 size, const void *pId, int *pState, MarlinDateTime *pNotBefore,
    MarlinDateTime *pNotAfter)
{
	SceUInt64 now = 0;
	SceUInt64 start;
	SceUInt64 end;
	SceUInt64 account;
	char accountId[8] = { 0 };
	char serviceId[256];
	SceUInt8 acid[264];
	SceUInt8 etid[264];
	SceUInt8 data[528];
	MarlinBuffer service;
	MarlinBuffer tag;
	MarlinDate notBefore;
	MarlinDate notAfter;
	int params[3];
	int hContext = 0;
	int *pTicket = NULL;
	int err;
	int count;
	int cmp;
	int i;
	int r;

	if (size != 0 && size <= 256 && pId != NULL && pState != NULL && pNotBefore != NULL && pNotAfter != NULL) {
		err = 0;
		if (sceKernelLockMutex(g_mutexId, 1, NULL) != 0)
			err = MLNAPPLIB_ERROR_FAILED;
		if (err == 0) {
			err = 0;
			for (i = 0; i < MLNAPPLIB_NUM_CONTEXTS; i++) {
				if (g_contexts[i].state == 0)
					err = MLNAPPLIB_ERROR_INVALID_STATE;
			}
			if (err == 0) {
				err = GetCurrentTime(&now);
				if (err == 0) {
					FillBytes(acid, 0, 264);
					FillBytes(etid, 0, 264);
					FillBytes(serviceId, 0, 256);
					FillBytes(data, 0, 528);
					FillBytes(accountId, 0, 8);
					FillBytes(&notBefore, 0, 28);
					FillBytes(&notAfter, 0, 28);
					params[0] = 0;
					if (SceNpManager_DDF30B0E(NULL, 0, &account) >= 0 && account != 0) {
						CopyBytes(accountId, &account, 8);
						count = 0;
						for (i = 0; i < 8; i++) {
							if (accountId[i] != 0)
								count++;
						}
						if (count == 8) {
							acid[0] = 0;
							acid[1] = 0;
							acid[2] = 0;
							acid[3] = 16;
							acid[4] = 'A';
							acid[5] = 'C';
							acid[6] = 'I';
							acid[7] = 'D';
							CopyBytes(&acid[8], accountId, 8);
							etid[3] = size + 8;
							etid[0] = (size + 8) >> 24;
							etid[1] = (size + 8) >> 16;
							etid[2] = (SceUInt16)(size + 8) >> 8;
							etid[4] = 'E';
							etid[5] = 'T';
							etid[6] = 'I';
							etid[7] = 'D';
							CopyBytes(&etid[8], pId, size);
							CopyBytes(serviceId, "urn:marlin:organization:sne:service-provider:1", 46);
							service.size = 46;
							service.pData = serviceId;
							CopyBytes(data, acid, 16);
							CopyBytes(&data[16], etid, size + 8);
							tag.size = size + 24;
							tag.pData = data;
							r = SceMarlin_9CD4CE1E(params, &hContext, 0);
							err = ConvertResult(r);
							if (err == 0) {
								r = SceMarlin_BCED6CC4(hContext, &service, &tag, &pTicket);
								err = ConvertResult(r);
								if (err == 0) {
									if (pTicket != NULL) {
										r = SceMarlin_3AA04BBF(hContext, pTicket[1], pTicket[2],
										    &notBefore, &notAfter);
										err = ConvertResult(r);
										if (err == 0) {
											ConvertDate(&notBefore, pNotBefore);
											ConvertDate(&notAfter, pNotAfter);
											err = DateToTime(pNotBefore, &start);
											if (err == 0)
												err = DateToTime(pNotAfter, &end);
											if (err == 0) {
												cmp = -1;
												if (now >= start)
													cmp = end < now;
												i = 0;
												if (cmp != 0) {
													i = 2;
													if (cmp > 0)
														i = 3;
												}
												*pState = i;
											}
										}
									} else {
										*pState = 1;
									}
								}
							}
						} else {
							err = MLNAPPLIB_ERROR_FAILED;
						}
					} else {
						err = MLNAPPLIB_ERROR_FAILED;
					}
				}
			}
		}
	} else {
		err = MLNAPPLIB_ERROR_INVALID_ARG;
	}
	SceMarlin_CCA99803(pTicket);
	SceMarlin_8FEB4F94(hContext);
	if (now == 0 && err == MLNAPPLIB_ERROR_FAILED) {
		err = 0;
		*pState = 4;
	}
	sceKernelUnlockMutex(g_mutexId, 1);
	return err;
}

int module_start(void)
{
	return 0;
}

int module_stop(void)
{
	return 0;
}
