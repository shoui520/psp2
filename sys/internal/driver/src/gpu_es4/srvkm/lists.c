/* IMG lists.c expansion under SCE's complete device-node owner layout.
 * Pristine IMPLEMENT_LIST_ANY_2 lives in the vendored include/lists.h.
 */
#include <gpu/device_sce.h>
#include <gpu/power_sce.h>
#include <stdarg.h>

PVRSRV_ERROR List_PVRSRV_DEVICE_NODE_PVRSRV_ERROR_Any(PVRSRV_DEVICE_NODE *psHead,
		PVRSRV_ERROR (*pfnCallBack)(PVRSRV_DEVICE_NODE *psNode))
{
	PVRSRV_ERROR result = PVRSRV_OK;
	PVRSRV_DEVICE_NODE *psNextNode = psHead;

	for (;;) {
		if (!psHead)
			break;
		if (result != PVRSRV_OK)
			break;
		psNextNode = psNextNode->psNext;
		result = pfnCallBack(psHead);
		psHead = psNextNode;
	}
	return result;
}

/* Pristine IMPLEMENT_LIST_ANY_VA(PVRSRV_DEVICE_NODE). */
IMG_VOID *List_PVRSRV_DEVICE_NODE_Any_va(PVRSRV_DEVICE_NODE *psHead,
		IMG_VOID *(*pfnCallBack)(PVRSRV_DEVICE_NODE *psNode, va_list va), ...)
{
	va_list ap;
	PVRSRV_DEVICE_NODE *psNextNode;
	IMG_VOID *pResult = 0;

	for (;;) {
		if (!psHead)
			break;
		if (pResult)
			break;
		psNextNode = psHead->psNext;
		va_start(ap, pfnCallBack);
		pResult = pfnCallBack(psHead, ap);
		va_end(ap);
		psHead = psNextNode;
	}
	return pResult;
}

/* Pristine IMPLEMENT_LIST_FOR_EACH(PVRSRV_DEVICE_NODE). */
IMG_VOID List_PVRSRV_DEVICE_NODE_ForEach(PVRSRV_DEVICE_NODE *psHead,
		IMG_VOID (*pfnCallBack)(PVRSRV_DEVICE_NODE *psNode))
{
	while (psHead) {
		pfnCallBack(psHead);
		psHead = psHead->psNext;
	}
}

/* Pristine IMPLEMENT_LIST_FOR_EACH_VA(PVRSRV_DEVICE_NODE). */
IMG_VOID List_PVRSRV_DEVICE_NODE_ForEach_va(PVRSRV_DEVICE_NODE *psHead,
		IMG_VOID (*pfnCallBack)(PVRSRV_DEVICE_NODE *psNode, va_list va), ...)
{
	va_list ap;

	while (psHead) {
		va_start(ap, pfnCallBack);
		pfnCallBack(psHead, ap);
		psHead = psHead->psNext;
		va_end(ap);
	}
}

/* Pristine IMPLEMENT_LIST_INSERT(PVRSRV_DEVICE_NODE). */
IMG_VOID List_PVRSRV_DEVICE_NODE_Insert(PVRSRV_DEVICE_NODE **ppsHead,
		PVRSRV_DEVICE_NODE *psNewNode)
{
	psNewNode->ppsThis = ppsHead;
	psNewNode->psNext = *ppsHead;
	*ppsHead = psNewNode;
	if (psNewNode->psNext)
		psNewNode->psNext->ppsThis = &psNewNode->psNext;
}

/* Pristine IMPLEMENT_LIST_REMOVE(PVRSRV_DEVICE_NODE). */
IMG_VOID List_PVRSRV_DEVICE_NODE_Remove(PVRSRV_DEVICE_NODE *psNode)
{
	*psNode->ppsThis = psNode->psNext;
	if (psNode->psNext)
		psNode->psNext->ppsThis = psNode->ppsThis;
}

/* Pristine IMPLEMENT_LIST_ANY_VA(PVRSRV_POWER_DEV). */
IMG_VOID *List_PVRSRV_POWER_DEV_Any_va(PVRSRV_POWER_DEV *psHead,
		IMG_VOID *(*pfnCallBack)(PVRSRV_POWER_DEV *psNode, va_list va), ...)
{
	va_list ap;
	PVRSRV_POWER_DEV *psNextNode;
	IMG_VOID *pResult = 0;

	for (;;) {
		if (!psHead)
			break;
		if (pResult)
			break;
		psNextNode = psHead->psNext;
		va_start(ap, pfnCallBack);
		pResult = pfnCallBack(psHead, ap);
		va_end(ap);
		psHead = psNextNode;
	}
	return pResult;
}

/* Pristine IMPLEMENT_LIST_ANY_VA_2(PVRSRV_POWER_DEV, PVRSRV_ERROR, PVRSRV_OK). */
PVRSRV_ERROR List_PVRSRV_POWER_DEV_PVRSRV_ERROR_Any_va(PVRSRV_POWER_DEV *psHead,
		PVRSRV_ERROR (*pfnCallBack)(PVRSRV_POWER_DEV *psNode, va_list va), ...)
{
	va_list ap;
	PVRSRV_POWER_DEV *psNextNode;
	PVRSRV_ERROR result = PVRSRV_OK;

	for (;;) {
		if (!psHead)
			break;
		if (result != PVRSRV_OK)
			break;
		psNextNode = psHead->psNext;
		va_start(ap, pfnCallBack);
		result = pfnCallBack(psHead, ap);
		va_end(ap);
		psHead = psNextNode;
	}
	return result;
}

/* Pristine IMPLEMENT_LIST_INSERT(PVRSRV_POWER_DEV). */
IMG_VOID List_PVRSRV_POWER_DEV_Insert(PVRSRV_POWER_DEV **ppsHead,
		PVRSRV_POWER_DEV *psNewNode)
{
	psNewNode->ppsThis = ppsHead;
	psNewNode->psNext = *ppsHead;
	*ppsHead = psNewNode;
	if (psNewNode->psNext)
		psNewNode->psNext->ppsThis = &psNewNode->psNext;
}

/* Pristine IMPLEMENT_LIST_REMOVE(PVRSRV_POWER_DEV). */
IMG_VOID List_PVRSRV_POWER_DEV_Remove(PVRSRV_POWER_DEV *psNode)
{
	*psNode->ppsThis = psNode->psNext;
	if (psNode->psNext)
		psNode->psNext->ppsThis = psNode->ppsThis;
}

/* IMG device-match callback; unchanged algorithm under the SCE ABI. */
IMG_VOID *MatchDeviceKM_AnyVaCb(PVRSRV_DEVICE_NODE *psDeviceNode, va_list va)
{
	IMG_UINT32 ui32DevIndex;
	IMG_BOOL bIgnoreClass;
	PVRSRV_DEVICE_CLASS eDevClass;

	ui32DevIndex = va_arg(va, IMG_UINT32);
	bIgnoreClass = va_arg(va, IMG_BOOL);
	if (!bIgnoreClass)
		eDevClass = va_arg(va, PVRSRV_DEVICE_CLASS);
	else
		eDevClass = PVRSRV_DEVICE_CLASS_FORCE_I32;
	if ((bIgnoreClass || psDeviceNode->sDevId.eDeviceClass == eDevClass) &&
			psDeviceNode->sDevId.ui32DeviceIndex == ui32DevIndex)
		return psDeviceNode;
	return 0;
}

/* Pristine IMG power-device index callback. */
IMG_VOID *MatchPowerDeviceIndex_AnyVaCb(PVRSRV_POWER_DEV *psPowerDev, va_list va)
{
	if (psPowerDev->ui32DeviceIndex == va_arg(va, IMG_UINT32))
		return psPowerDev;
	return 0;
}
