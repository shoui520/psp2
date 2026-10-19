/* IMG common/resman.c base; native SCE deltas are marked locally. */
#include <gpu/resman_sce.h>
#include <stddef.h>
#include <stdarg.h>

RESMAN_LIST *gpsResList = NULL;

PVRSRV_ERROR OSFreeMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID *pvLinAddr, IMG_HANDLE hBlockAlloc);
PVRSRV_ERROR OSAllocMem_Impl(IMG_UINT32 ui32Flags, IMG_SIZE_T ui32Size,
		IMG_VOID **ppvLinAddr, IMG_HANDLE *phBlockAlloc);

/* Pristine IMPLEMENT_LIST_REVERSE(RESMAN_ITEM) expansion. */
IMG_VOID List_RESMAN_ITEM_Reverse(RESMAN_ITEM **ppsHead)
{
	RESMAN_ITEM *psTmpNode1;
	RESMAN_ITEM *psTmpNode2;
	RESMAN_ITEM *psCurNode;

	psTmpNode1 = NULL;
	psCurNode = *ppsHead;
	while (psCurNode) {
		psTmpNode2 = psCurNode->psNext;
		psCurNode->psNext = psTmpNode1;
		psTmpNode1 = psCurNode;
		psCurNode = psTmpNode2;
		if (psCurNode)
			psTmpNode1->ppsThis = &psCurNode->psNext;
		else
			psTmpNode1->ppsThis = ppsHead;
	}
	*ppsHead = psTmpNode1;
}

/* Pristine IMG resource selection callback. */
IMG_VOID *FreeResourceByCriteria_AnyVaCb(RESMAN_ITEM *psCurItem, va_list va)
{
	IMG_UINT32 ui32SearchCriteria = va_arg(va, IMG_UINT32);
	IMG_UINT32 ui32ResType = va_arg(va, IMG_UINT32);
	IMG_VOID *pvParam = va_arg(va, IMG_VOID *);
	IMG_UINT32 ui32Param = va_arg(va, IMG_UINT32);

	if ((ui32SearchCriteria & RESMAN_CRITERIA_RESTYPE) &&
			psCurItem->ui32ResType != ui32ResType)
		return NULL;
	if ((ui32SearchCriteria & RESMAN_CRITERIA_PVOID_PARAM) &&
			psCurItem->pvParam != pvParam)
		return NULL;
	if ((ui32SearchCriteria & RESMAN_CRITERIA_UI32_PARAM) &&
			psCurItem->ui32Param != ui32Param)
		return NULL;
	return psCurItem;
}

/* SCE non-destructive search adds a distinct copy of the pristine predicate. */
IMG_VOID *ResManFindResourceByCriteria_AnyVaCb(RESMAN_ITEM *psCurItem, va_list va)
{
	IMG_UINT32 ui32SearchCriteria = va_arg(va, IMG_UINT32);
	IMG_UINT32 ui32ResType = va_arg(va, IMG_UINT32);
	IMG_VOID *pvParam = va_arg(va, IMG_VOID *);
	IMG_UINT32 ui32Param = va_arg(va, IMG_UINT32);

	if ((ui32SearchCriteria & RESMAN_CRITERIA_RESTYPE) &&
			psCurItem->ui32ResType != ui32ResType)
		return NULL;
	if ((ui32SearchCriteria & RESMAN_CRITERIA_PVOID_PARAM) &&
			psCurItem->pvParam != pvParam)
		return NULL;
	if ((ui32SearchCriteria & RESMAN_CRITERIA_UI32_PARAM) &&
			psCurItem->ui32Param != ui32Param)
		return NULL;
	return psCurItem;
}

/* Pristine IMPLEMENT_LIST_ANY_VA(RESMAN_ITEM) expansion. */
IMG_VOID *List_RESMAN_ITEM_Any_va(RESMAN_ITEM *psHead,
		IMG_VOID *(*pfnCallBack)(RESMAN_ITEM *psNode, va_list va), ...)
{
	va_list ap;
	RESMAN_ITEM *psNextNode;
	IMG_VOID *pResult = NULL;

	while (psHead) {
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

/* IMG free callback path; SCE omits diagnostics and Linux sync primitives. */
PVRSRV_ERROR FreeResourceByPtr(RESMAN_ITEM *psItem, IMG_BOOL bExecuteCallback,
		IMG_BOOL bForceCleanup)
{
	PVRSRV_ERROR eError = PVRSRV_OK;

	*psItem->ppsThis = psItem->psNext;
	if (psItem->psNext)
		psItem->psNext->ppsThis = psItem->ppsThis;
	if (bExecuteCallback)
		eError = psItem->pfnFreeResource(psItem->pvParam, psItem->ui32Param,
				bForceCleanup);
	OSFreeMem_Impl(1, sizeof(*psItem), psItem, NULL);
	return eError;
}

/* SCE passes the list head directly and always executes the cleanup callback. */
PVRSRV_ERROR FreeResourceByCriteria(RESMAN_ITEM **ppsResItemList,
		IMG_UINT32 ui32SearchCriteria, IMG_UINT32 ui32ResType,
		IMG_VOID *pvParam, IMG_UINT32 ui32Param)
{
	RESMAN_ITEM *psCurItem;
	PVRSRV_ERROR eError = PVRSRV_OK;

	while ((psCurItem = List_RESMAN_ITEM_Any_va(*ppsResItemList,
			FreeResourceByCriteria_AnyVaCb, ui32SearchCriteria, ui32ResType,
			pvParam, ui32Param)) != NULL && eError == PVRSRV_OK)
		eError = FreeResourceByPtr(psCurItem, IMG_TRUE, IMG_FALSE);
	return eError;
}

/* Pristine resource-manager initialization, with SCE diagnostics omitted. */
PVRSRV_ERROR ResManInit(IMG_VOID)
{
	if (gpsResList == NULL) {
		if (OSAllocMem_Impl(1, sizeof(*gpsResList),
				(IMG_VOID **)&gpsResList, NULL) != PVRSRV_OK)
			return PVRSRV_ERROR_OUT_OF_MEMORY;
		gpsResList->psContextList = NULL;
	}
	return PVRSRV_OK;
}

IMG_VOID ResManDeInit(IMG_VOID)
{
	if (gpsResList != NULL) {
		OSFreeMem_Impl(1, sizeof(*gpsResList), gpsResList, NULL);
		gpsResList = NULL;
	}
}

/* IMG context setup; SCE omits diagnostics and expands list insertion inline. */
PVRSRV_ERROR PVRSRVResManConnect(PVRSRV_PER_PROCESS_DATA *psPerProc,
		RESMAN_CONTEXT **phResManContext)
{
	PVRSRV_ERROR eError;
	RESMAN_CONTEXT *psResManContext;

	eError = OSAllocMem_Impl(1, sizeof(*psResManContext),
			(IMG_VOID **)&psResManContext, NULL);
	if (eError != PVRSRV_OK)
		return eError;
	psResManContext->psPerProc = psPerProc;
	psResManContext->psResItemList = NULL;
	psResManContext->ppsThis = &gpsResList->psContextList;
	psResManContext->psNext = gpsResList->psContextList;
	gpsResList->psContextList = psResManContext;
	if (psResManContext->psNext)
		psResManContext->psNext->ppsThis = &psResManContext->psNext;
	*phResManContext = psResManContext;
	return PVRSRV_OK;
}

/* IMG teardown order with SCE render/shared-sync/mutex resource extensions. */
IMG_VOID PVRSRVResManDisconnect(RESMAN_CONTEXT *psResManContext,
		IMG_BOOL bKernelContext)
{
	if (!bKernelContext) {
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_OS_USERMODE_MAPPING, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_DMA_CLIENT_FIFO_DATA, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_EVENT_OBJECT, NULL, 0);
		List_RESMAN_ITEM_Reverse(&psResManContext->psResItemList);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_MODIFY_SYNC_OPS, NULL, 0);
		List_RESMAN_ITEM_Reverse(&psResManContext->psResItemList);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_RENDER_TARGET, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_RENDER_CONTEXT, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_TRANSFER_CONTEXT, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_HW_RENDER_CONTEXT, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_HW_TRANSFER_CONTEXT, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_HW_2D_CONTEXT, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_SHARED_PB_DESC_CREATE_LOCK, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_SHARED_PB_DESC, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_MUTEX, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_SHARED_SYNC_INFO, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_SYNC_INFO, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_DEVICECLASSMEM_MAPPING, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_DEVICEMEM_WRAP, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_DEVICEMEM_MAPPING, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_KERNEL_DEVICEMEM_ALLOCATION, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_DEVICEMEM_ALLOCATION, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_DEVICEMEM_CONTEXT, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_SHARED_MEM_INFO, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_DISPLAYCLASS_SWAPCHAIN_REF, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_DISPLAYCLASS_DEVICE, NULL, 0);
		FreeResourceByCriteria(&psResManContext->psResItemList,
				RESMAN_CRITERIA_RESTYPE, RESMAN_TYPE_BUFFERCLASS_DEVICE, NULL, 0);
	}
	*psResManContext->ppsThis = psResManContext->psNext;
	if (psResManContext->psNext)
		psResManContext->psNext->ppsThis = psResManContext->ppsThis;
	OSFreeMem_Impl(1, sizeof(*psResManContext), psResManContext, NULL);
}

/* Pristine IMG registration with SCE diagnostics omitted. */
RESMAN_ITEM *ResManRegisterRes(RESMAN_CONTEXT *psResManContext,
		IMG_UINT32 ui32ResType, IMG_VOID *pvParam, IMG_UINT32 ui32Param,
		RESMAN_FREE_FN pfnFreeResource)
{
	RESMAN_ITEM *psNewResItem;

	if (psResManContext == NULL)
		return NULL;
	if (OSAllocMem_Impl(1, sizeof(*psNewResItem),
			(IMG_VOID **)&psNewResItem, NULL) != PVRSRV_OK)
		return NULL;
	psNewResItem->ui32ResType = ui32ResType;
	psNewResItem->pvParam = pvParam;
	psNewResItem->ui32Param = ui32Param;
	psNewResItem->pfnFreeResource = pfnFreeResource;
	psNewResItem->ui32Flags = 0;
	psNewResItem->ppsThis = &psResManContext->psResItemList;
	psNewResItem->psNext = psResManContext->psResItemList;
	psResManContext->psResItemList = psNewResItem;
	if (psNewResItem->psNext)
		psNewResItem->psNext->ppsThis = &psNewResItem->psNext;
	return psNewResItem;
}

PVRSRV_ERROR ResManFreeResByPtr(RESMAN_ITEM *psResItem, IMG_BOOL bForceCleanup)
{
	if (psResItem == NULL)
		return PVRSRV_OK;
	return FreeResourceByPtr(psResItem, IMG_TRUE, bForceCleanup);
}

PVRSRV_ERROR ResManFreeResByCriteria(RESMAN_CONTEXT *psResManContext,
		IMG_UINT32 ui32SearchCriteria, IMG_UINT32 ui32ResType,
		IMG_VOID *pvParam, IMG_UINT32 ui32Param)
{
	return FreeResourceByCriteria(&psResManContext->psResItemList,
			ui32SearchCriteria, ui32ResType, pvParam, ui32Param);
}

/* Pristine reassociation; SCE expands list methods without diagnostics. */
PVRSRV_ERROR ResManDissociateRes(RESMAN_ITEM *psResItem,
		RESMAN_CONTEXT *psNewResManContext)
{
	if (psResItem == NULL)
		return PVRSRV_ERROR_INVALID_PARAMS;
	if (psNewResManContext == NULL)
		return FreeResourceByPtr(psResItem, IMG_FALSE, IMG_FALSE);
	*psResItem->ppsThis = psResItem->psNext;
	if (psResItem->psNext)
		psResItem->psNext->ppsThis = psResItem->ppsThis;
	psResItem->ppsThis = &psNewResManContext->psResItemList;
	psResItem->psNext = psNewResManContext->psResItemList;
	psNewResManContext->psResItemList = psResItem;
	if (psResItem->psNext)
		psResItem->psNext->ppsThis = &psResItem->psNext;
	return PVRSRV_OK;
}

/* SCE non-destructive criteria lookup returns the matching resource item. */
RESMAN_ITEM *ResManFindResourceByCriteria(RESMAN_CONTEXT *psResManContext,
		IMG_UINT32 ui32SearchCriteria, IMG_UINT32 ui32ResType,
		IMG_VOID *pvParam, IMG_UINT32 ui32Param)
{
	return List_RESMAN_ITEM_Any_va(psResManContext->psResItemList,
			ResManFindResourceByCriteria_AnyVaCb, ui32SearchCriteria,
			ui32ResType, pvParam, ui32Param);
}
