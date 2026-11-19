/*
 * SceSystemGesture (vs0:sys/external/libsystemgesture.suprx): the system's touch gesture library.
 * The primitive touch events of the front and back panels are tracked in a pool of nodes, and each
 * recognizer (tap, drag, tap and hold, pinch out/in, rotation) turns them into its own events.
 */

#include <math.h>
#include <string.h>
#include <kernel/libkernel.h>
#include <libkernel/libkernel.h>
#include <systemgesture.h>
#include <touch.h>
#include "libsystemgesture.h"

extern int sceTouchGetPixelDensity2(float *pDensityX0, float *pDensityY0, float *pDensityX1, float *pDensityY1);

static void CreateMutex(void) __attribute__((noinline));
static void DeleteMutex(void) __attribute__((noinline));
static int UpdateTapRecognizer(GestureRecognizer *pRecognizer) __attribute__((noinline));
static int UpdateDragRecognizer(GestureRecognizer *pRecognizer) __attribute__((noinline));
static int UpdateTapAndHoldRecognizer(GestureRecognizer *pRecognizer) __attribute__((noinline));
static int UpdatePinchOutInRecognizer(GestureRecognizer *pRecognizer) __attribute__((noinline));
static int UpdateRotationRecognizer(GestureRecognizer *pRecognizer) __attribute__((noinline));

static SceKernelLwMutexWork g_mutex;
static SceUInt64 g_knownIds[4];		/* the touches that have been seen, by panel and half of the IDs */
static SceUInt64 g_previousIds[4];	/* the IDs the last update reported */
static SceUInt64 g_currentIds[4];	/* the IDs this update reports */
static PrimitiveTouchState g_state;

/* the time between two samples, counted in 32 bits around a wrap of the sample counter */
static inline SceUInt32 ElapsedTime(SceUInt64 now, SceUInt64 before)
{
	SceUInt32 elapsed;

	elapsed = now - before;
	if (now <= before)
		elapsed = -1 - before + now + 1;
	return elapsed;
}

/* whether a position lies inside a rectangle: 1 inside, -1 outside, an error for a missing one */
static inline int CheckPositionInRectangle(const SceSystemGestureVector2 *pPosition,
	const SceSystemGestureRectangle *pRectangle)
{
	int ret;

	if (pPosition != NULL && pRectangle != NULL) {
		ret = -1;
		if (pPosition->x >= pRectangle->x && pPosition->y >= pRectangle->y
		    && pPosition->x <= pRectangle->x + pRectangle->width
		    && pPosition->y <= pRectangle->y + pRectangle->height)
			ret = 1;
	} else {
		ret = SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	}
	return ret;
}

/* the node put at the end of a list */
static inline void ListAppend(PrimitiveTouchNode **ppList, PrimitiveTouchNode *pNode)
{
	PrimitiveTouchNode *pLast;
	PrimitiveTouchNode *pNext;

	pLast = *ppList;
	if (pLast != NULL) {
		for (pNext = pLast->pNext; pNext != NULL; pNext = pNext->pNext)
			pLast = pNext;
		pLast->pNext = pNode;
	} else {
		*ppList = pNode;
	}
}

int module_start(SceSize argc, const void *args)
{
	CreateMutex();
	return 0;
}

int module_stop(SceSize argc, const void *args)
{
	DeleteMutex();
	return 0;
}

int sceSystemGestureResetPrimitiveTouchRecognizer(void) __attribute__((noinline));

int sceSystemGestureResetPrimitiveTouchRecognizer(void)
{
	PrimitiveTouchNode *pNode;
	int i;

	if (g_state.initialized == 0)
		return SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
	g_state.count = 0;
	g_state.pPool = NULL;
	g_state.pBegin = NULL;
	g_state.pActive = NULL;
	g_state.pEnd = NULL;
	g_state.pCancelled = NULL;
	g_knownIds[0] = 0;
	g_knownIds[1] = 0;
	pNode = g_state.nodes;
	for (i = 0; i < SYSTEM_GESTURE_NUM_PRIMITIVES; i++) {
		sceClibMemset(pNode, 0, 0x30);
		pNode->pNext = NULL;
		pNode->eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_INACTIVE;
		ListAppend(&g_state.pPool, pNode);
		pNode++;
	}
	return 0;
}

int sceSystemGestureInitializePrimitiveTouchRecognizer(SceSystemGesturePrimitiveTouchRecognizerParameter *parameter)
{
	sceClibMemset(&g_state, 0, sizeof(g_state));
	g_state.initialized = 1;
	sceSystemGestureResetPrimitiveTouchRecognizer();
	sceTouchGetPixelDensity2(&g_state.density[0][0], &g_state.density[0][1], &g_state.density[1][0],
		&g_state.density[1][1]);
	return 0;
}

int sceSystemGestureFinalizePrimitiveTouchRecognizer(void)
{
	sceClibMemset(&g_state, 0, sizeof(g_state));
	return 0;
}

int sceSystemGestureUpdatePrimitiveTouchRecognizer(const SceTouchData *pFrontData, const SceTouchData *pBackData)
{
	const SceTouchData *pData;
	const SceTouchReport *pReport;
	PrimitiveTouchNode *pNode;
	PrimitiveTouchNode *pNext;
	PrimitiveTouchNode *pPrevious;
	PrimitiveTouchNode *pBegin;
	PrimitiveTouchNode *pActive;
	PrimitiveTouchNode *pCancelled;
	PrimitiveTouchNode *pFound;
	SceUInt64 bit;
	SceUInt64 previousTime;
	SceUInt32 elapsed;
	SceUInt32 id;
	SceUInt32 primitiveID;
	SceUInt32 index;
	SceUInt8 port;
	SceUInt8 i;
	SceUInt8 n;
	SceInt16 x;
	SceInt16 y;
	SceInt16 force;
	int bitIndex;
	int ret;
	int stale;
	float densityX;
	float densityY;
	float distanceX;
	float distanceY;

	if (g_state.initialized == 0)
		return SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
	if (pFrontData == NULL && pBackData == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	ret = sceKernelLockLwMutex(&g_mutex, 1, NULL);
	if (ret < 0)
		return ret;
	pBegin = NULL;
	pActive = NULL;
	pCancelled = NULL;
	if (g_state.pCancelled != NULL) {
		for (pNode = g_state.pCancelled; pNode != NULL; pNode = pNext) {
			pNode->elapsedTime = 0;
			pNext = pNode->pNext;
			pNode->eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_INACTIVE;
		}
		ListAppend(&g_state.pPool, g_state.pCancelled);
		g_state.pCancelled = NULL;
	}
	if (g_state.pEnd != NULL) {
		for (pNode = g_state.pEnd; pNode != NULL; pNode = pNext) {
			pNode->elapsedTime = 0;
			pNext = pNode->pNext;
			pNode->eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_INACTIVE;
		}
		ListAppend(&g_state.pPool, g_state.pEnd);
		g_state.pEnd = NULL;
	}
	if (g_state.pBegin != NULL) {
		for (pNode = g_state.pBegin; pNode != NULL; pNode = pNext) {
			pNext = pNode->pNext;
			pNode->eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE;
		}
		ListAppend(&g_state.pActive, g_state.pBegin);
		g_state.pBegin = NULL;
	}
	/* the nodes of the first list are kept up to date with the new sample, as Sony's does */
	for (pNode = g_state.pPool; pNode != NULL; pNode = pNode->pNext) {
		port = pNode->primitiveID >> 8;
		pData = (port == 0) ? pFrontData : pBackData;
		if (pData != NULL)
			pNode->elapsedTime += ElapsedTime(pData->timeStamp, g_state.current[port].timeStamp);
	}
	sceClibMemcpy_safe(g_state.previous, g_state.current, sizeof(g_state.current));
	g_state.count = 0;
	for (i = 0; i < 4; i++) {
		g_previousIds[i] = g_currentIds[i];
		g_currentIds[i] = 0;
	}
	for (port = 0; port < 2; port++) {
		pData = (port == 0) ? pFrontData : pBackData;
		if (pData == NULL)
			continue;
		sceClibMemcpy_safe(&g_state.current[port], pData, sizeof(SceTouchData));
		if (g_state.current[port].status == SCE_TOUCH_STATUS_INTERCEPTED) {
			stale = 1;
		} else {
			previousTime = g_state.previous[port].timeStamp;
			stale = 0;
			if (previousTime != 0)
				stale = ElapsedTime(g_state.current[port].timeStamp, previousTime) > 200000;
		}
		for (n = 0; n < pData->reportNum; n++) {
			pReport = &pData->report[n];
			id = pReport->id;
			index = (id >= 0x40) | (2 * port);
			bitIndex = id;
			if (id >= 0x40)
				bitIndex = id - 64;
			bit = 1ULL << bitIndex;
			g_currentIds[index] |= bit;
			if (stale || pReport->info == SCE_TOUCH_REPORT_INFO_HIDE_UPPER_LAYER)
				g_knownIds[index] |= bit;
			primitiveID = (id & 0xFFFF00FF) | (port << 8);
			pFound = NULL;
			pPrevious = NULL;
			for (pNode = g_state.pActive; pNode != NULL; pNode = pNext) {
				pNext = pNode->pNext;
				if (pNode->primitiveID == primitiveID) {
					pFound = pNode;
					break;
				}
				pPrevious = pNode;
			}
			if (pFound != NULL) {
				if (pPrevious != NULL)
					pPrevious->pNext = pFound->pNext;
				else
					g_state.pActive = pFound->pNext;
				x = pReport->x;
				pFound->pNext = NULL;
				pFound->deltaVector.x = x - pFound->currentPosition.x;
				y = pReport->y;
				pFound->deltaVector.y = y - pFound->currentPosition.y;
				force = pReport->force;
				pFound->deltaForce = force - pFound->currentForce;
				pFound->currentPosition.x = x;
				elapsed = ElapsedTime(pData->timeStamp, g_state.previous[port].timeStamp);
				pFound->currentPosition.y = y;
				pFound->currentForce = force;
				pFound->deltaTime = elapsed;
				pFound->elapsedTime += elapsed;
				pFound->pNext = NULL;
				densityX = g_state.density[port != 0][0];
				densityY = g_state.density[port != 0][1];
				distanceY = (float)(y - pFound->pressedPosition.y) / densityY;
				distanceX = (float)(x - pFound->pressedPosition.x) / densityX;
				if (distanceY * distanceY + distanceX * distanceX > 9.0f)
					pFound->moved = 1;
				if (stale || pReport->info == SCE_TOUCH_REPORT_INFO_HIDE_UPPER_LAYER) {
					pFound->eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED;
					pFound->pNext = NULL;
					ListAppend(&pCancelled, pFound);
				} else {
					pFound->eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE;
					pFound->pNext = NULL;
					ListAppend(&pActive, pFound);
				}
				g_state.count++;
			} else if ((bit & g_knownIds[index]) == 0) {
				pNode = g_state.pPool;
				if (pNode != NULL) {
					g_state.pPool = pNode->pNext;
					pNode->pNext = NULL;
					pNode->eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_BEGIN;
					pNode->primitiveID = pReport->id | (port << 8);
					pNode->deltaVector.x = 0;
					pNode->deltaVector.y = 0;
					pNode->currentPosition.x = pReport->x;
					pNode->currentPosition.y = pReport->y;
					pNode->currentForce = pReport->force;
					pNode->deltaTime = 0;
					pNode->elapsedTime = 0;
					pNode->pressedPosition.x = pReport->x;
					pNode->pressedPosition.y = pReport->y;
					pNode->pressedForce = pReport->force;
					pNode->moved = 0;
					pNode->pNext = NULL;
					ListAppend(&pBegin, pNode);
					g_state.count++;
				}
			}
		}
	}
	pNode = g_state.pActive;
	g_state.pEnd = g_state.pActive;
	if (pNode != NULL) {
		do {
			g_state.count++;
			pNext = pNode->pNext;
			pNode->eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_END;
			pNode = pNext;
		} while (pNext != NULL);
	}
	g_state.pBegin = pBegin;
	g_state.pActive = pActive;
	g_state.pCancelled = pCancelled;
	for (i = 0; i < 4; i++) {
		SceUInt64 released;

		released = g_previousIds[i] & ~g_currentIds[i];
		if (released != 0)
			g_knownIds[i] &= ~released;
	}
	ret = sceKernelUnlockLwMutex(&g_mutex, 1);
	if (ret >= 0)
		return 0;
	return ret;
}

static inline int CopyPrimitiveTouchEvent(SceSystemGesturePrimitiveTouchEvent *pEvent, const PrimitiveTouchNode *pNode)
{
	if (pEvent == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	pEvent->primitiveID = pNode->primitiveID;
	pEvent->eventState = pNode->eventState;
	pEvent->pressedPosition.x = pNode->pressedPosition.x;
	pEvent->pressedPosition.y = pNode->pressedPosition.y;
	pEvent->pressedForce = pNode->pressedForce;
	pEvent->currentPosition.x = pNode->currentPosition.x;
	pEvent->currentPosition.y = pNode->currentPosition.y;
	pEvent->currentForce = pNode->currentForce;
	pEvent->deltaVector.x = pNode->deltaVector.x;
	pEvent->deltaVector.y = pNode->deltaVector.y;
	pEvent->deltaForce = pNode->deltaForce;
	pEvent->deltaTime = pNode->deltaTime;
	pEvent->elapsedTime = pNode->elapsedTime;
	return 0;
}

int sceSystemGestureGetPrimitiveTouchEvents(SceSystemGesturePrimitiveTouchEvent *primitiveEventBuffer,
	const SceUInt32 capacityOfBuffer, SceUInt32 *numberOfEvent)
{
	PrimitiveTouchNode *lists[4];
	PrimitiveTouchNode **ppList;
	PrimitiveTouchNode *pNode;
	SceSystemGesturePrimitiveTouchEvent *pEvent;
	SceInt32 limit;
	SceInt32 count;
	int result;
	int done;
	int i;

	if (g_state.initialized != 0) {
		if (primitiveEventBuffer != NULL && capacityOfBuffer != 0 && numberOfEvent != NULL) {
			sceClibMemset(primitiveEventBuffer, 0, 96 * capacityOfBuffer);
			limit = capacityOfBuffer;
			if (capacityOfBuffer >= g_state.count)
				limit = g_state.count;
			done = 0;
			lists[1] = g_state.pActive;
			lists[2] = g_state.pEnd;
			count = 0;
			lists[0] = g_state.pBegin;
			ppList = lists;
			lists[3] = g_state.pCancelled;
			i = 0;
			do {
				if (i >= 4)
					break;
				pNode = *ppList;
				if (pNode != NULL) {
					pEvent = &primitiveEventBuffer[count];
					do {
						if (pNode == NULL)
							break;
						if (CopyPrimitiveTouchEvent(pEvent, pNode) < 0)
							return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
						count++;
						pEvent++;
						if (count >= limit)
							done = 1;
						pNode = pNode->pNext;
					} while (done == 0);
				}
				ppList++;
				i++;
			} while (done == 0);
			result = 0;
			*numberOfEvent = count;
		} else {
			result = SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
		}
	} else {
		result = SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
	}
	return result;
}

int sceSystemGestureGetPrimitiveTouchEventsCount(void)
{
	if (g_state.initialized != 0)
		return g_state.count;
	return SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
}

int sceSystemGestureGetPrimitiveTouchEventByIndex(const SceUInt32 index,
	SceSystemGesturePrimitiveTouchEvent *primitiveTouchEvent)
{
	PrimitiveTouchNode *lists[4];
	PrimitiveTouchNode **ppList;
	PrimitiveTouchNode *pNode;
	SceUInt32 position;
	int result;
	int found;
	int i;

	if (g_state.initialized != 0) {
		if (primitiveTouchEvent != NULL) {
			if (index < g_state.count) {
				found = 0;
				ppList = lists;
				position = 0;
				lists[0] = g_state.pBegin;
				lists[1] = g_state.pActive;
				i = 0;
				lists[2] = g_state.pEnd;
				lists[3] = g_state.pCancelled;
				do {
					if (i >= 4)
						break;
					pNode = *ppList;
					if (pNode != NULL) {
						do {
							if (pNode == NULL)
								break;
							if (position == index) {
								found = 1;
								if (CopyPrimitiveTouchEvent(primitiveTouchEvent, pNode) < 0)
									return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
							}
							pNode = pNode->pNext;
							position++;
						} while (found == 0);
					}
					ppList++;
					i++;
				} while (found == 0);
				result = 0;
			} else {
				result = SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY;
			}
		} else {
			result = SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
		}
	} else {
		result = SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
	}
	return result;
}

int sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(const SceUInt16 primitiveID,
	SceSystemGesturePrimitiveTouchEvent *primitiveTouchEvent)
{
	PrimitiveTouchNode *lists[4];
	PrimitiveTouchNode **ppList;
	PrimitiveTouchNode *pNode;
	int found;
	int i;

	if (g_state.initialized == 0)
		return SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
	if (primitiveTouchEvent == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	ppList = lists;
	found = 0;
	lists[0] = g_state.pBegin;
	lists[1] = g_state.pActive;
	lists[2] = g_state.pEnd;
	lists[3] = g_state.pCancelled;
	i = 0;
	do {
		if (i >= 4)
			break;
		pNode = *ppList;
		if (pNode != NULL) {
			do {
				if (pNode == NULL)
					break;
				if (primitiveID == pNode->primitiveID) {
					found = 1;
					CopyPrimitiveTouchEvent(primitiveTouchEvent, pNode);
				}
				pNode = pNode->pNext;
			} while (found == 0);
		}
		ppList++;
		i++;
	} while (found == 0);
	if (found != 0)
		return 0;
	return SCE_SYSTEM_GESTURE_ERROR_EVENT_DATA_NOT_FOUND;
}

static inline void ResetRecognizer(GestureRecognizer *pRecognizer)
{
	int i;

	pRecognizer->updatedTime = 0;
	pRecognizer->firstTime = 0;
	pRecognizer->numEvents = 0;
	for (i = 16; i != 0; i--) {
		pRecognizer->slots[16 - i].eventState = 0;
		pRecognizer->slots[16 - i].gestureType = pRecognizer->gestureType;
		pRecognizer->slots[16 - i].eventID = 0;
		sceClibMemset(pRecognizer->slots[16 - i].property.propertyBuf, 0, 0x40);
	}
}

int sceSystemGestureResetTouchRecognizer(SceSystemGestureTouchRecognizer *touchRecognizer)
{
	GestureRecognizer *pRecognizer;

	pRecognizer = (GestureRecognizer *)touchRecognizer;
	if (pRecognizer == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	if (pRecognizer->initialized == 0)
		return SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
	ResetRecognizer(pRecognizer);
	return 0;
}

int sceSystemGestureCreateTouchRecognizer(SceSystemGestureTouchRecognizer *touchRecognizer,
	const SceSystemGestureType gestureType, const SceUInt8 touchPanelPortID,
	const SceSystemGestureRectangle *rectangle,
	const SceSystemGestureTouchRecognizerParameter *touchRecognizerParameter)
{
	GestureRecognizer *pRecognizer;
	SceTouchPanelInfo panelInfo;
	SceUInt32 value;
	SceUInt64 timeToInvokeEvent;
	int ret;

	pRecognizer = (GestureRecognizer *)touchRecognizer;
	if (pRecognizer == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	if (gestureType == 0 || (gestureType != SCE_SYSTEM_GESTURE_TYPE_TAP && gestureType != SCE_SYSTEM_GESTURE_TYPE_DRAG
	    && gestureType != SCE_SYSTEM_GESTURE_TYPE_TAP_AND_HOLD && gestureType != SCE_SYSTEM_GESTURE_TYPE_PINCH_OUT_IN
	    && gestureType != SCE_SYSTEM_GESTURE_TYPE_ROTATION))
		return SCE_SYSTEM_GESTURE_ERROR_NOT_SUPPORTED_GESTURE;
	pRecognizer->touchPanelPortID = touchPanelPortID;
	pRecognizer->gestureType = gestureType;
	pRecognizer->updatedTime = 0;
	pRecognizer->firstTime = 0;
	if (rectangle != NULL) {
		pRecognizer->rectangle.x = rectangle->x;
		pRecognizer->rectangle.y = rectangle->y;
		pRecognizer->rectangle.width = rectangle->width;
		pRecognizer->rectangle.height = rectangle->height;
	} else {
		ret = sceTouchGetPanelInfo(touchPanelPortID, &panelInfo);
		if (ret < 0)
			return ret;
		pRecognizer->rectangle.x = panelInfo.minAaX;
		pRecognizer->rectangle.y = panelInfo.minAaY;
		pRecognizer->rectangle.width = panelInfo.maxAaX - panelInfo.minAaX;
		pRecognizer->rectangle.height = panelInfo.maxAaY - panelInfo.minAaY;
	}
	sceClibMemset(&pRecognizer->parameters, 0, sizeof(pRecognizer->parameters));
	if (gestureType == SCE_SYSTEM_GESTURE_TYPE_TAP) {
		value = 1;
		if (touchRecognizerParameter != NULL)
			value = touchRecognizerParameter->tap.maxTapCount;
		pRecognizer->parameters.tap.maxTapCount = value;
		if (value == 0)
			pRecognizer->parameters.tap.maxTapCount = 1;
		pRecognizer->parameters.tap.reserved = 3;
		pRecognizer->parameters.tap.maxPressedTime = 400;
		pRecognizer->parameters.tap.maxMovedDistance = 4;
		pRecognizer->parameters.tap.maxTapInterval = 200;
	} else if (gestureType == SCE_SYSTEM_GESTURE_TYPE_DRAG) {
	} else if (gestureType == SCE_SYSTEM_GESTURE_TYPE_TAP_AND_HOLD) {
		timeToInvokeEvent = 1000;
		if (touchRecognizerParameter != NULL)
			timeToInvokeEvent = touchRecognizerParameter->tapAndHold.timeToInvokeEvent;
		pRecognizer->parameters.tapAndHold.timeToInvokeEvent = timeToInvokeEvent;
	} else if (gestureType == SCE_SYSTEM_GESTURE_TYPE_PINCH_OUT_IN) {
		pRecognizer->parameters.pair.paired = 0;
		pRecognizer->parameters.pair.flags = 1;
	} else if (gestureType == SCE_SYSTEM_GESTURE_TYPE_ROTATION) {
		pRecognizer->parameters.pair.paired = 0;
		pRecognizer->parameters.pair.flags = 1;
	}
	pRecognizer->initialized = 1;
	ResetRecognizer(pRecognizer);
	return 0;
}

int sceSystemGestureGetTouchRecognizerInformation(const SceSystemGestureTouchRecognizer *touchRecognizer,
	SceSystemGestureTouchRecognizerInformation *information)
{
	const GestureRecognizer *pRecognizer;

	pRecognizer = (const GestureRecognizer *)touchRecognizer;
	if (pRecognizer == NULL || information == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	if (pRecognizer->initialized == 0)
		return SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
	information->rectangle.x = pRecognizer->rectangle.x;
	information->rectangle.y = pRecognizer->rectangle.y;
	information->gestureType = pRecognizer->gestureType;
	information->touchPanelPortID = pRecognizer->touchPanelPortID;
	information->updatedTime = pRecognizer->updatedTime;
	information->rectangle.width = pRecognizer->rectangle.width;
	information->rectangle.height = pRecognizer->rectangle.height;
	return 0;
}

/* the slot a primitive's touch is followed in, found by its event ID; the slot's eventID is 0 for a free one */
static inline int IsOnPort(const PrimitiveTouchNode *pNode, const GestureRecognizer *pRecognizer)
{
	return (SceUInt8)(pNode->primitiveID >> 8) == pRecognizer->touchPanelPortID;
}

static int UpdateTapRecognizer(GestureRecognizer *pRecognizer)
{
	PrimitiveTouchNode *pNode;
	PrimitiveTouchNode *pEndList;
	PrimitiveTouchNode *pCancelledList;
	PrimitiveTouchNode *pBeginList;
	SceUInt64 elapsed;
	float density0X;
	float density0Y;
	float density1X;
	float density1Y;
	float densityX;
	float densityY;
	float threshold;
	float distanceX;
	float distanceY;
	int found;
	int i;
	int count;

	if (pRecognizer == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	density0X = g_state.density[0][0];
	density0Y = g_state.density[0][1];
	density1X = g_state.density[1][0];
	pBeginList = g_state.pBegin;
	density1Y = g_state.density[1][1];
	pEndList = g_state.pEnd;
	pCancelledList = g_state.pCancelled;
	for (i = 0; i < 16; i++) {
		if (pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE) {
			pRecognizer->slots[i].eventID = 0;
			pRecognizer->slots[i].eventState = 0;
			pRecognizer->slots[i].property.tap.released = 0;
		} else if (pRecognizer->slots[i].eventState == 0 && pRecognizer->slots[i].eventID != 0 && pRecognizer->slots[i].property.tap.released != 0) {
			elapsed = pRecognizer->slots[i].property.tap.elapsedTime
				+ ElapsedTime(g_state.current[pRecognizer->touchPanelPortID].timeStamp, pRecognizer->slots[i].updatedTime);
			pRecognizer->slots[i].property.tap.elapsedTime = elapsed;
			pRecognizer->slots[i].updatedTime = g_state.current[pRecognizer->touchPanelPortID].timeStamp;
			if (elapsed > (SceUInt32)(1000 * pRecognizer->parameters.tap.maxTapInterval))
				pRecognizer->slots[i].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE;
		}
	}
	threshold = (float)pRecognizer->parameters.tap.maxMovedDistance * (float)pRecognizer->parameters.tap.maxMovedDistance;
	for (pNode = pBeginList; pNode != NULL; pNode = pNode->pNext) {
		if (IsOnPort(pNode, pRecognizer) && CheckPositionInRectangle(&pNode->currentPosition, &pRecognizer->rectangle) > 0) {
			found = 0;
			for (i = 0; i < 16; i++) {
				if (pRecognizer->slots[i].eventState == 0 && pRecognizer->slots[i].eventID != 0 && pRecognizer->slots[i].property.tap.released != 0) {
					densityX = density1X;
					densityY = density1Y;
					if ((SceUInt8)(pNode->primitiveID >> 8) == 0) {
						densityX = density0X;
						densityY = density0Y;
					}
					distanceY = (float)(pNode->currentPosition.y - pRecognizer->slots[i].property.tap.releasedPosition.y) / densityY;
					distanceX = (float)(pNode->currentPosition.x - pRecognizer->slots[i].property.tap.releasedPosition.x) / densityX;
					if (distanceY * distanceY + distanceX * distanceX < threshold) {
						found = 1;
						pRecognizer->slots[i].eventID = pNode->primitiveID + 1;
						pRecognizer->slots[i].property.tap.released = 0;
						pRecognizer->slots[i].property.tap.primitiveID = pNode->primitiveID;
					}
				}
				if (found)
					break;
			}
			if (!found) {
				count = 0;
				i = 0;
				do {
					if (i >= 16)
						break;
					if (pRecognizer->slots[i].eventID == 0) {
						pRecognizer->slots[i].eventID = pNode->primitiveID + 1;
						pRecognizer->slots[i].eventState = 0;
						pRecognizer->slots[i].property.tap.primitiveID = pNode->primitiveID;
						pRecognizer->slots[i].property.tap.tappedCount = 0;
						pRecognizer->slots[i].property.tap.released = 0;
						pRecognizer->slots[i].property.tap.elapsedTime = 0;
						pRecognizer->slots[i].property.tap.position.x = pNode->pressedPosition.x;
						count = 1;
						pRecognizer->slots[i].property.tap.position.y = pNode->pressedPosition.y;
						pRecognizer->slots[i].property.tap.releasedPosition.x = -1;
						pRecognizer->slots[i].property.tap.releasedPosition.y = -1;
					}
					i++;
				} while (count == 0);
			}
		}
	}
	for (pNode = pEndList; pNode != NULL; pNode = pNode->pNext) {
		if (!IsOnPort(pNode, pRecognizer))
			continue;
		if (pNode->moved != 0
		    || pNode->elapsedTime >= (SceUInt32)(1000 * pRecognizer->parameters.tap.maxPressedTime)
		    || CheckPositionInRectangle(&pNode->currentPosition, &pRecognizer->rectangle) <= 0) {
			found = 0;
			i = 0;
			do {
				if (i >= 16)
					break;
				if (pRecognizer->slots[i].eventID == pNode->primitiveID + 1) {
					found = 1;
					pRecognizer->slots[i].eventID = 0;
				}
				i++;
			} while (found == 0);
		} else {
			found = 0;
			i = 0;
			do {
				if (i >= 16)
					break;
				if (pRecognizer->slots[i].eventID == pNode->primitiveID + 1) {
					pRecognizer->slots[i].updatedTime = g_state.current[pRecognizer->touchPanelPortID].timeStamp;
					pRecognizer->slots[i].property.tap.tappedCount++;
					pRecognizer->slots[i].property.tap.released = 1;
					pRecognizer->slots[i].property.tap.releasedPosition.x = pNode->currentPosition.x;
					pRecognizer->slots[i].property.tap.releasedPosition.y = pNode->currentPosition.y;
					pRecognizer->slots[i].property.tap.elapsedTime = 0;
					found = 1;
					if (pRecognizer->slots[i].property.tap.tappedCount == pRecognizer->parameters.tap.maxTapCount)
						pRecognizer->slots[i].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE;
				}
				i++;
			} while (found == 0);
		}
	}
	for (pNode = pCancelledList; pNode != NULL; pNode = pNode->pNext) {
		if (IsOnPort(pNode, pRecognizer)) {
			found = 0;
			i = 0;
			do {
				if (i >= 16)
					break;
				if (pRecognizer->slots[i].eventID == pNode->primitiveID + 1) {
					found = 1;
					pRecognizer->slots[i].eventID = 0;
				}
				i++;
			} while (found == 0);
		}
	}
	pRecognizer->numEvents = 0;
	for (i = 0; i < 16; i++) {
		if (pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE) {
			pRecognizer->slots[i].updatedTime = g_state.current[pRecognizer->touchPanelPortID].timeStamp;
			pRecognizer->numEvents++;
		}
	}
	return 0;
}

static int UpdateDragRecognizer(GestureRecognizer *pRecognizer)
{
	PrimitiveTouchNode *pNode;
	PrimitiveTouchNode *pActiveList;
	PrimitiveTouchNode *pEndList;
	PrimitiveTouchNode *pCancelledList;
	SceUInt32 elapsed;
	int found;
	int i;
	int free;

	if (pRecognizer == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	pActiveList = g_state.pActive;
	pEndList = g_state.pEnd;
	pCancelledList = g_state.pCancelled;
	for (i = 0; i < 16; i++) {
		if (pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_END
		    || pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED)
			pRecognizer->slots[i].eventState = 0;
	}
	for (pNode = pActiveList; pNode != NULL; pNode = pNode->pNext) {
		if (!IsOnPort(pNode, pRecognizer))
			continue;
		elapsed = ElapsedTime(pRecognizer->updatedTime, pRecognizer->firstTime);
		if (CheckPositionInRectangle(&pNode->pressedPosition, &pRecognizer->rectangle) <= 0
		    || CheckPositionInRectangle(&pNode->currentPosition, &pRecognizer->rectangle) <= 0
		    || !((SceUInt64)elapsed < pNode->elapsedTime)) {
			found = 0;
			i = 0;
			do {
				if (i >= 16)
					break;
				if (pRecognizer->slots[i].eventID == pNode->primitiveID + 1U && pRecognizer->slots[i].eventState != 0) {
					found = 1;
					pRecognizer->slots[i].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_END;
				}
				i++;
			} while (found == 0);
		} else {
			found = 0;
			i = 0;
			while (found == 0) {
				if (i >= 16) {
					for (i = 0; i < 16; i++) {
						if (pRecognizer->slots[i].eventState == 0 && pRecognizer->slots[i].eventID == (SceUInt16)pNode->primitiveID + 1U)
							found = 1;
						if (found)
							break;
					}
					if (found)
						break;
					free = -1;
					for (i = 0; i < 16; i++) {
						if (pRecognizer->slots[i].eventState == 0)
							free = i;
					}
					if (free >= 0) {
						pRecognizer->slots[free].eventID = (SceUInt16)pNode->primitiveID + 1U;
						pRecognizer->slots[free].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_BEGIN;
						pRecognizer->slots[free].property.drag.primitiveID = pNode->primitiveID;
						pRecognizer->slots[free].property.drag.currentPosition.x = pNode->currentPosition.x;
						pRecognizer->slots[free].property.drag.currentPosition.y = pNode->currentPosition.y;
						pRecognizer->slots[free].property.drag.pressedPosition.x = pNode->pressedPosition.x;
						pRecognizer->slots[free].property.drag.pressedPosition.y = pNode->pressedPosition.y;
						pRecognizer->slots[free].property.drag.deltaVector.x = pNode->deltaVector.x;
						pRecognizer->slots[free].property.drag.deltaVector.y = pNode->deltaVector.y;
					}
					break;
				}
				if (pRecognizer->slots[i].eventID == pNode->primitiveID + 1U) {
					if (pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_BEGIN
					    || pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE) {
						pRecognizer->slots[i].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE;
						pRecognizer->slots[i].updatedTime = g_state.current[pRecognizer->touchPanelPortID].timeStamp;
						found = 1;
						pRecognizer->slots[i].property.drag.currentPosition.x = pNode->currentPosition.x;
						pRecognizer->slots[i].property.drag.currentPosition.y = pNode->currentPosition.y;
						pRecognizer->slots[i].property.drag.deltaVector.x = pNode->deltaVector.x;
						pRecognizer->slots[i].property.drag.deltaVector.y = pNode->deltaVector.y;
					}
				}
				i++;
			}
		}
	}
	for (pNode = pEndList; pNode != NULL; pNode = pNode->pNext) {
		if (IsOnPort(pNode, pRecognizer)) {
			found = 0;
			i = 0;
			do {
				if (i >= 16)
					break;
				if (pRecognizer->slots[i].eventID == pNode->primitiveID + 1U) {
					if (pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_BEGIN
					    || pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE) {
						found = 1;
						pRecognizer->slots[i].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_END;
					}
				}
				i++;
			} while (found == 0);
		}
	}
	for (pNode = pCancelledList; pNode != NULL; pNode = pNode->pNext) {
		if (IsOnPort(pNode, pRecognizer)) {
			found = 0;
			i = 0;
			do {
				if (i >= 16)
					break;
				if (pRecognizer->slots[i].eventID == pNode->primitiveID + 1U) {
					if (pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_BEGIN
					    || pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE) {
						found = 1;
						pRecognizer->slots[i].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED;
					}
				}
				i++;
			} while (found == 0);
		}
	}
	pRecognizer->numEvents = 0;
	for (i = 0; i < 16; i++) {
		if (pRecognizer->slots[i].eventState != 0) {
			pRecognizer->slots[i].updatedTime = g_state.current[pRecognizer->touchPanelPortID].timeStamp;
			pRecognizer->numEvents++;
		}
	}
	return 0;
}

static int UpdateTapAndHoldRecognizer(GestureRecognizer *pRecognizer)
{
	PrimitiveTouchNode *pNode;
	PrimitiveTouchNode *pActiveList;
	PrimitiveTouchNode *pEndList;
	PrimitiveTouchNode *pCancelledList;
	SceUInt32 elapsed;
	SceUInt32 id;
	int found;
	int free;
	int i;

	if (pRecognizer == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	pActiveList = g_state.pActive;
	pEndList = g_state.pEnd;
	pCancelledList = g_state.pCancelled;
	for (i = 0; i < 16; i++) {
		if (pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_END
		    || pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED) {
			pRecognizer->slots[i].eventID = 0;
			pRecognizer->slots[i].eventState = 0;
		}
	}
	for (pNode = pActiveList; pNode != NULL; pNode = pNode->pNext) {
		if (!IsOnPort(pNode, pRecognizer))
			continue;
		if (CheckPositionInRectangle(&pNode->currentPosition, &pRecognizer->rectangle) <= 0)
			continue;
		elapsed = ElapsedTime(pRecognizer->updatedTime, pRecognizer->firstTime);
		if (pNode->elapsedTime > (SceUInt64)elapsed)
			continue;
		id = pNode->primitiveID + 1;
		found = 0;
		i = 0;
		while (i < 16) {
			if (pRecognizer->slots[i].eventID == id) {
				if (pNode->moved != 0)
					pRecognizer->slots[i].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_END;
				else
					pRecognizer->slots[i].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE;
				found = 1;
			}
			i++;
			if (found != 0)
				break;
		}
		if (found != 0)
			continue;
		if (pNode->moved == 0
		    && pNode->elapsedTime > 1000ULL * pRecognizer->parameters.tapAndHold.timeToInvokeEvent) {
			free = -1;
			for (i = 0; i < 16; i++) {
				if (pRecognizer->slots[i].eventID == 0)
					free = i;
			}
			if (free >= 0) {
				pRecognizer->slots[free].eventID = id;
				pRecognizer->slots[free].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_BEGIN;
				pRecognizer->slots[free].property.tapAndHold.pressedPosition.x = pNode->pressedPosition.x;
				pRecognizer->slots[free].property.tapAndHold.pressedPosition.y = pNode->pressedPosition.y;
				pRecognizer->slots[free].property.tapAndHold.primitiveID = pNode->primitiveID;
			}
		}
	}
	for (pNode = pEndList; pNode != NULL; pNode = pNode->pNext) {
		if (IsOnPort(pNode, pRecognizer)) {
			found = 0;
			i = 0;
			do {
				if (i >= 16)
					break;
				if (pRecognizer->slots[i].eventID == pNode->primitiveID + 1U) {
					found = 1;
					pRecognizer->slots[i].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_END;
				}
				i++;
			} while (found == 0);
		}
	}
	for (pNode = pCancelledList; pNode != NULL; pNode = pNode->pNext) {
		if (IsOnPort(pNode, pRecognizer)) {
			found = 0;
			i = 0;
			do {
				if (i >= 16)
					break;
				if (pRecognizer->slots[i].eventID == pNode->primitiveID + 1U) {
					found = 1;
					pRecognizer->slots[i].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED;
				}
				i++;
			} while (found == 0);
		}
	}
	pRecognizer->numEvents = 0;
	for (i = 0; i < 16; i++) {
		if (pRecognizer->slots[i].eventState != 0) {
			pRecognizer->slots[i].updatedTime = g_state.current[pRecognizer->touchPanelPortID].timeStamp;
			pRecognizer->numEvents++;
		}
	}
	return 0;
}

/* the touches of an end or cancelled list stop being followed by a pair of fingers */
static inline void ReleasePairedPrimitives(GestureRecognizer *pRecognizer, const PrimitiveTouchNode *pList)
{
	const PrimitiveTouchNode *pNode;
	int i;
	int k;

	for (pNode = pList; pNode != NULL; pNode = pNode->pNext) {
		for (i = 0; i < 16; i++) {
			for (k = 0; k < 2; k++) {
				if (pRecognizer->slots[i].property.pinchOutIn.present[k] == 1
				    && pRecognizer->slots[i].property.pinchOutIn.primitive[k].primitiveID == pNode->primitiveID)
					pRecognizer->slots[i].property.pinchOutIn.present[k] = 0;
			}
		}
	}
}

static inline float PixelDensityX(SceUInt32 port)
{
	return (port != 0) ? g_state.density[1][0] : g_state.density[0][0];
}

static inline float PixelDensityY(SceUInt32 port)
{
	return (port != 0) ? g_state.density[1][1] : g_state.density[0][1];
}

static int UpdatePinchOutInRecognizer(GestureRecognizer *pRecognizer)
{
	PrimitiveTouchNode *pNode;
	PrimitiveTouchNode *pBeginList;
	SceSystemGesturePrimitiveTouchEvent event[2];
	int result[2];
	SceUInt32 port;
	float densityX;
	float densityY;
	float dx;
	float dy;
	float pairedDistance;
	float distance;
	int slotIndex;
	int found;
	int failed;
	int cancelled;
	int i;
	int k;

	if (pRecognizer == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	slotIndex = 0;
	found = 0;
	pBeginList = g_state.pBegin;
	for (i = 0; i < 16; i++) {
		if (pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_END
		    || pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED) {
			pRecognizer->slots[i].eventID = 0;
			pRecognizer->slots[i].eventState = 0;
			pRecognizer->parameters.pair.paired = 0;
		} else if (pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_BEGIN) {
			pRecognizer->slots[i].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE;
		}
	}
	if (pRecognizer->parameters.pair.paired == 0) {
		int searching = 0;

		for (i = 0; i < 16 && searching == 0; i++) {
			
			if (pRecognizer->slots[slotIndex].property.pinchOutIn.present[0] == 0 || pRecognizer->slots[slotIndex].property.pinchOutIn.present[1] == 0) {
				searching = 1;
				slotIndex = i;
			}
		}
		if (i >= 16 && searching == 0)
			goto release;
		for (pNode = pBeginList; pNode != NULL; pNode = pNode->pNext) {
			if (!IsOnPort(pNode, pRecognizer))
				goto release;
			if (CheckPositionInRectangle(&pNode->currentPosition, &pRecognizer->rectangle) <= 0)
				goto release;
			k = pRecognizer->slots[slotIndex].property.pinchOutIn.present[0];
			if (k != 0) {
				if (pRecognizer->slots[slotIndex].property.pinchOutIn.present[1] == 0) {
					pRecognizer->slots[slotIndex].property.pinchOutIn.present[1] = 1;
					pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[1].primitiveID = pNode->primitiveID;
				}
				if (k != 1)
					continue;
			} else {
				pRecognizer->slots[slotIndex].property.pinchOutIn.present[0] = 1;
				pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[0].primitiveID = pNode->primitiveID;
			}
			if (pRecognizer->slots[slotIndex].property.pinchOutIn.present[1] == 1) {
				pRecognizer->parameters.pair.paired = 1;
				pRecognizer->slots[slotIndex].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_BEGIN;
				pRecognizer->slots[slotIndex].eventID = (pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[1].primitiveID + 1U)
					| ((pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[0].primitiveID + 1U) << 16);
				failed = 0;
				for (k = 0; k < 2; k++) {
					if (sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[k].primitiveID,
					    &event[0]) >= 0) {
						pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[k].pairedPosition.x = event[0].currentPosition.x;
						pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[k].pairedPosition.y = event[0].currentPosition.y;
					} else {
						pRecognizer->slots[slotIndex].property.pinchOutIn.present[k] = 0;
						failed = 1;
					}
				}
				if (failed <= 0) {
					pRecognizer->slots[slotIndex].property.pinchOutIn.scale = 1.0f;
					pRecognizer->slots[slotIndex].updatedTime = g_state.current[pRecognizer->touchPanelPortID].timeStamp;
				} else {
					pRecognizer->slots[slotIndex].eventID = 0;
					pRecognizer->slots[slotIndex].eventState = 0;
					pRecognizer->parameters.pair.paired = 1;
				}
			}
			if (pRecognizer->parameters.pair.paired != 0)
				goto release;
		}
		goto release;
	}
	found = 0;
	for (slotIndex = 0; slotIndex < 16; slotIndex++) {
		if (pRecognizer->slots[slotIndex].eventID == 0)
			continue;
		found = 1;
		for (k = 0; k < 2; k++) {
			result[k] = sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[k].primitiveID,
				&event[k]);
			if (result[k] >= 0) {
				pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[k].currentPosition.x = event[k].currentPosition.x;
				pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[k].currentPosition.y = event[k].currentPosition.y;
				pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[k].deltaVector.x = event[k].deltaVector.x;
				pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[k].deltaVector.y = event[k].deltaVector.y;
			} else {
				pRecognizer->slots[slotIndex].property.pinchOutIn.present[k] = 0;
			}
		}
		if (result[0] >= 0 && result[1] >= 0) {
			if (event[0].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_END
			    || event[0].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED
			    || CheckPositionInRectangle(&event[0].currentPosition, &pRecognizer->rectangle) <= 0
			    || event[1].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_END
			    || event[1].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED
			    || CheckPositionInRectangle(&event[1].currentPosition, &pRecognizer->rectangle) <= 0) {
				cancelled = 0;
				for (k = 0; k < 2; k++) {
					cancelled = 0;
					if (event[k].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED)
						cancelled = found;
					if (event[k].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_END || cancelled != 0)
						pRecognizer->slots[slotIndex].property.pinchOutIn.present[k] = 0;
				}
				if (pRecognizer->slots[slotIndex].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE) {
					if (cancelled != 0)
						pRecognizer->slots[slotIndex].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED;
					else
						pRecognizer->slots[slotIndex].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_END;
				}
			} else {
				port = pRecognizer->touchPanelPortID & 0xFFFF;
				densityX = PixelDensityX(port);
				densityY = PixelDensityY(port);
				dy = (float)(pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[0].pairedPosition.y - pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[1].pairedPosition.y)
					/ densityY;
				dx = (float)(pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[0].pairedPosition.x - pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[1].pairedPosition.x)
					/ densityX;
				pairedDistance = dy * dy + dx * dx;
				if (pairedDistance > 0.0f) {
					dy = (float)(pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[0].currentPosition.y - pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[1].currentPosition.y)
						/ densityY;
					dx = (float)(pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[0].currentPosition.x - pRecognizer->slots[slotIndex].property.pinchOutIn.primitive[1].currentPosition.x)
						/ densityX;
					distance = (dy * dy + dx * dx) / pairedDistance;
					pRecognizer->slots[slotIndex].property.pinchOutIn.scale = sqrt((double)distance);
					pRecognizer->slots[slotIndex].updatedTime = g_state.current[pRecognizer->touchPanelPortID].timeStamp;
				}
			}
		} else {
			pRecognizer->slots[slotIndex].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_END;
			for (k = 0; k < 2; k++) {
				if (result[k] < 0)
					pRecognizer->slots[slotIndex].property.pinchOutIn.present[k] = 0;
			}
		}
		break;
	}
	if (found == 0 && pRecognizer != NULL && pRecognizer->initialized != 0)
		ResetRecognizer(pRecognizer);
release:
	ReleasePairedPrimitives(pRecognizer, g_state.pEnd);
	ReleasePairedPrimitives(pRecognizer, g_state.pCancelled);
	pRecognizer->numEvents = 0;
	for (i = 0; i < 16; i++) {
		if (pRecognizer->slots[i].eventState != 0)
			pRecognizer->numEvents++;
	}
	return 0;
}

static int UpdateRotationRecognizer(GestureRecognizer *pRecognizer)
{
	PrimitiveTouchNode *pNode;
	PrimitiveTouchNode *pBeginList;
	SceSystemGesturePrimitiveTouchEvent event[2];
	int result[2];
	SceUInt32 port;
	float densityX;
	float densityY;
	float pairedX;
	float pairedY;
	float currentX;
	float currentY;
	float pairedLength;
	float currentLength;
	float lengths;
	float dot;
	float cross;
	float angle;
	float sign;
	float lastAngle;
	float rotation;
	int turns;
	int slotIndex;
	int found;
	int failed;
	int cancelled;
	int i;
	int k;

	if (pRecognizer == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	slotIndex = 0;
	found = 0;
	pBeginList = g_state.pBegin;
	for (i = 0; i < 16; i++) {
		if (pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_END
		    || pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED) {
			pRecognizer->slots[i].eventID = 0;
			pRecognizer->slots[i].eventState = 0;
			pRecognizer->parameters.pair.paired = 0;
		} else if (pRecognizer->slots[i].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_BEGIN) {
			pRecognizer->slots[i].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE;
		}
	}
	if (pRecognizer->parameters.pair.paired == 0) {
		int searching = 0;

		for (i = 0; i < 16 && searching == 0; i++) {
			
			if (pRecognizer->slots[slotIndex].property.rotation.present[0] == 0 || pRecognizer->slots[slotIndex].property.rotation.present[1] == 0) {
				searching = 1;
				slotIndex = i;
			}
		}
		if (i >= 16 && searching == 0)
			goto release;
		for (pNode = pBeginList; pNode != NULL; pNode = pNode->pNext) {
			if (!IsOnPort(pNode, pRecognizer))
				goto release;
			if (CheckPositionInRectangle(&pNode->currentPosition, &pRecognizer->rectangle) <= 0)
				goto release;
			k = pRecognizer->slots[slotIndex].property.rotation.present[0];
			if (k != 0) {
				if (pRecognizer->slots[slotIndex].property.rotation.present[1] == 0) {
					pRecognizer->slots[slotIndex].property.rotation.present[1] = 1;
					pRecognizer->slots[slotIndex].property.rotation.primitive[1].primitiveID = pNode->primitiveID;
				}
				if (k != 1)
					continue;
			} else {
				pRecognizer->slots[slotIndex].property.rotation.present[0] = 1;
				pRecognizer->slots[slotIndex].property.rotation.primitive[0].primitiveID = pNode->primitiveID;
			}
			if (pRecognizer->slots[slotIndex].property.rotation.present[1] == 1) {
				pRecognizer->parameters.pair.paired = 1;
				pRecognizer->slots[slotIndex].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_BEGIN;
				pRecognizer->slots[slotIndex].property.rotation.turns = 0;
				pRecognizer->slots[slotIndex].property.rotation.lastAngle = 0.0f;
				pRecognizer->slots[slotIndex].eventID = (pRecognizer->slots[slotIndex].property.rotation.primitive[1].primitiveID + 1U)
					| ((pRecognizer->slots[slotIndex].property.rotation.primitive[0].primitiveID + 1U) << 16);
				failed = 0;
				for (k = 0; k < 2; k++) {
					if (sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(pRecognizer->slots[slotIndex].property.rotation.primitive[k].primitiveID,
					    &event[0]) >= 0) {
						pRecognizer->slots[slotIndex].property.rotation.primitive[k].pairedPosition.x = event[0].currentPosition.x;
						pRecognizer->slots[slotIndex].property.rotation.primitive[k].pairedPosition.y = event[0].currentPosition.y;
					} else {
						pRecognizer->slots[slotIndex].property.rotation.present[k] = 0;
						failed = 1;
					}
				}
				if (failed <= 0) {
					pRecognizer->slots[slotIndex].property.rotation.scale = 0.0f;
					pRecognizer->slots[slotIndex].updatedTime = g_state.current[pRecognizer->touchPanelPortID].timeStamp;
				} else {
					pRecognizer->slots[slotIndex].eventID = 0;
					pRecognizer->slots[slotIndex].eventState = 0;
					pRecognizer->parameters.pair.paired = 0;
				}
			}
			if (pRecognizer->parameters.pair.paired != 0)
				goto release;
		}
		goto release;
	}
	found = 0;
	for (slotIndex = 0; slotIndex < 16; slotIndex++) {
		if (pRecognizer->slots[slotIndex].eventID == 0)
			continue;
		found = 1;
		for (k = 0; k < 2; k++) {
			result[k] = sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(pRecognizer->slots[slotIndex].property.rotation.primitive[k].primitiveID,
				&event[k]);
			if (result[k] >= 0) {
				pRecognizer->slots[slotIndex].property.rotation.primitive[k].currentPosition.x = event[k].currentPosition.x;
				pRecognizer->slots[slotIndex].property.rotation.primitive[k].currentPosition.y = event[k].currentPosition.y;
				pRecognizer->slots[slotIndex].property.rotation.primitive[k].deltaVector.x = event[k].deltaVector.x;
				pRecognizer->slots[slotIndex].property.rotation.primitive[k].deltaVector.y = event[k].deltaVector.y;
			} else {
				pRecognizer->slots[slotIndex].property.rotation.present[k] = 0;
			}
		}
		if (result[0] >= 0 && result[1] >= 0) {
			if (event[0].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_END
			    || event[0].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED
			    || CheckPositionInRectangle(&event[0].currentPosition, &pRecognizer->rectangle) <= 0
			    || event[1].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_END
			    || event[1].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED
			    || CheckPositionInRectangle(&event[1].currentPosition, &pRecognizer->rectangle) <= 0) {
				cancelled = 0;
				for (k = 0; k < 2; k++) {
					cancelled = 0;
					if (event[k].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED)
						cancelled = found;
					if (event[k].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_END || cancelled != 0)
						pRecognizer->slots[slotIndex].property.rotation.present[k] = 0;
				}
				if (pRecognizer->slots[slotIndex].eventState == SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE) {
					if (cancelled != 0)
						pRecognizer->slots[slotIndex].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED;
					else
						pRecognizer->slots[slotIndex].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_END;
				}
			} else {
				port = pRecognizer->touchPanelPortID & 0xFFFF;
				densityX = PixelDensityX(port);
				densityY = PixelDensityY(port);
				pairedY = (float)(pRecognizer->slots[slotIndex].property.rotation.primitive[0].pairedPosition.y - pRecognizer->slots[slotIndex].property.rotation.primitive[1].pairedPosition.y)
					/ densityY;
				pairedX = (float)(pRecognizer->slots[slotIndex].property.rotation.primitive[0].pairedPosition.x - pRecognizer->slots[slotIndex].property.rotation.primitive[1].pairedPosition.x)
					/ densityX;
				if (pairedY * pairedY + pairedX * pairedX > 0.0f) {
					port = (SceUInt8)(pRecognizer->slots[slotIndex].property.rotation.primitive[0].primitiveID >> 8);
					densityX = PixelDensityX(port);
					densityY = PixelDensityY(port);
					pairedY = (float)(pRecognizer->slots[slotIndex].property.rotation.primitive[0].pairedPosition.y
						- pRecognizer->slots[slotIndex].property.rotation.primitive[1].pairedPosition.y) / densityY;
					pairedX = (float)(pRecognizer->slots[slotIndex].property.rotation.primitive[0].pairedPosition.x
						- pRecognizer->slots[slotIndex].property.rotation.primitive[1].pairedPosition.x) / densityX;
					pairedLength = sqrtf(pairedY * pairedY + pairedX * pairedX);
					densityX = PixelDensityX(port);
					densityY = PixelDensityY(port);
					currentY = (float)(pRecognizer->slots[slotIndex].property.rotation.primitive[0].currentPosition.y
						- pRecognizer->slots[slotIndex].property.rotation.primitive[1].currentPosition.y) / densityY;
					currentX = (float)(pRecognizer->slots[slotIndex].property.rotation.primitive[0].currentPosition.x
						- pRecognizer->slots[slotIndex].property.rotation.primitive[1].currentPosition.x) / densityX;
					currentLength = sqrtf(currentY * currentY + currentX * currentX);
					lengths = currentLength * pairedLength;
					currentY = (float)(pRecognizer->slots[slotIndex].property.rotation.primitive[0].currentPosition.y
						- pRecognizer->slots[slotIndex].property.rotation.primitive[1].currentPosition.y);
					currentX = (float)(pRecognizer->slots[slotIndex].property.rotation.primitive[0].currentPosition.x
						- pRecognizer->slots[slotIndex].property.rotation.primitive[1].currentPosition.x);
					densityX = PixelDensityX(port);
					densityY = PixelDensityY(port);
					pairedX = (float)(pRecognizer->slots[slotIndex].property.rotation.primitive[0].pairedPosition.x
						- pRecognizer->slots[slotIndex].property.rotation.primitive[1].pairedPosition.x) / densityX;
					pairedY = (float)(pRecognizer->slots[slotIndex].property.rotation.primitive[0].pairedPosition.y
						- pRecognizer->slots[slotIndex].property.rotation.primitive[1].pairedPosition.y) / densityY;
					dot = (pairedX * currentX) / densityX + (pairedY * currentY) / densityY;
					cross = (pairedX * currentY) / densityY - (pairedY * currentX) / densityX;
					if (lengths == 0.0f) {
						rotation = 0.0f;
					} else {
						angle = dot / lengths;
						if (angle > 1.0f)
							angle = 1.0f;
						if (angle < -1.0f)
							angle = -1.0f;
						angle = acosf(angle);
						sign = -1.0f;
						turns = pRecognizer->slots[slotIndex].property.rotation.turns;
						if (cross > 0.0f)
							sign = 1.0f;
						angle = angle * sign;
						if (angle > 1.57079632675) {
							lastAngle = pRecognizer->slots[slotIndex].property.rotation.lastAngle;
							if (lastAngle <= 0.0f || angle >= 0.0f) {
								if (lastAngle < 0.0f && angle > 0.0f) {
									pRecognizer->slots[slotIndex].property.rotation.turns = turns - 1;
									turns = pRecognizer->slots[slotIndex].property.rotation.turns;
								}
							} else {
								pRecognizer->slots[slotIndex].property.rotation.turns = turns + 1;
								turns = pRecognizer->slots[slotIndex].property.rotation.turns;
							}
						}
						pRecognizer->slots[slotIndex].property.rotation.lastAngle = angle;
						rotation = angle + (double)turns * 6.283185307;
					}
					pRecognizer->slots[slotIndex].property.rotation.scale = rotation;
					pRecognizer->slots[slotIndex].updatedTime = g_state.current[pRecognizer->touchPanelPortID].timeStamp;
				}
			}
		} else {
			pRecognizer->slots[slotIndex].eventState = SCE_SYSTEM_GESTURE_TOUCH_STATE_END;
			for (k = 0; k < 2; k++) {
				if (result[k] < 0)
					pRecognizer->slots[slotIndex].property.rotation.present[k] = 0;
			}
		}
		break;
	}
	if (found == 0 && pRecognizer != NULL && pRecognizer->initialized != 0)
		ResetRecognizer(pRecognizer);
release:
	ReleasePairedPrimitives(pRecognizer, g_state.pEnd);
	ReleasePairedPrimitives(pRecognizer, g_state.pCancelled);
	pRecognizer->numEvents = 0;
	for (i = 0; i < 16; i++) {
		if (pRecognizer->slots[i].eventState != 0)
			pRecognizer->numEvents++;
	}
	return 0;
}

int sceSystemGestureUpdateTouchRecognizer(SceSystemGestureTouchRecognizer *touchRecognizer)
{
	GestureRecognizer *pRecognizer;
	SceUInt64 timeStamp;
	SceSystemGestureType type;
	int ret;

	pRecognizer = (GestureRecognizer *)touchRecognizer;
	if (pRecognizer == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	if (pRecognizer->initialized == 0)
		return SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
	ret = sceKernelLockLwMutex(&g_mutex, 1, NULL);
	if (ret < 0)
		return ret;
	timeStamp = g_state.current[pRecognizer->touchPanelPortID].timeStamp;
	pRecognizer->updatedTime = timeStamp;
	if (pRecognizer->firstTime == 0)
		pRecognizer->firstTime = timeStamp;
	type = pRecognizer->gestureType;
	if (type == SCE_SYSTEM_GESTURE_TYPE_TAP)
		UpdateTapRecognizer(pRecognizer);
	else if (type == SCE_SYSTEM_GESTURE_TYPE_DRAG)
		UpdateDragRecognizer(pRecognizer);
	else if (type == SCE_SYSTEM_GESTURE_TYPE_TAP_AND_HOLD)
		UpdateTapAndHoldRecognizer(pRecognizer);
	else if (type == SCE_SYSTEM_GESTURE_TYPE_PINCH_OUT_IN)
		UpdatePinchOutInRecognizer(pRecognizer);
	else if (type == SCE_SYSTEM_GESTURE_TYPE_ROTATION)
		UpdateRotationRecognizer(pRecognizer);
	return sceKernelUnlockLwMutex(&g_mutex, 1);
}

int sceSystemGestureUpdateTouchRecognizerRectangle(SceSystemGestureTouchRecognizer *touchRecognizer,
	const SceSystemGestureRectangle *rectangle)
{
	GestureRecognizer *pRecognizer;

	pRecognizer = (GestureRecognizer *)touchRecognizer;
	if (pRecognizer == NULL || rectangle == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	if (pRecognizer->initialized == 0)
		return SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
	pRecognizer->rectangle.x = rectangle->x;
	pRecognizer->rectangle.y = rectangle->y;
	pRecognizer->rectangle.width = rectangle->width;
	pRecognizer->rectangle.height = rectangle->height;
	return 0;
}

int sceSystemGestureGetTouchEvents(const SceSystemGestureTouchRecognizer *touchRecognizer,
	SceSystemGestureTouchEvent *touchEventBuffer, SceSize numEventBuffer, SceSize *numEvents)
{
	const GestureRecognizer *pRecognizer;
	SceSize limit;
	SceSize count;
	int done;
	int i;

	pRecognizer = (const GestureRecognizer *)touchRecognizer;
	if (pRecognizer == NULL || touchEventBuffer == NULL || numEventBuffer == 0 || numEvents == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	if (pRecognizer->initialized == 0)
		return SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
	sceClibMemset(touchEventBuffer, 0, numEventBuffer);
	limit = numEventBuffer;
	if (limit >= pRecognizer->numEvents)
		limit = pRecognizer->numEvents;
	count = 0;
	done = 0;
	i = 0;
	do {
		if (i >= 16)
			break;
		if (pRecognizer->slots[i].eventState != 0) {
			sceClibMemcpy_safe(touchEventBuffer, &pRecognizer->slots[i], sizeof(*touchEventBuffer));
			count++;
			touchEventBuffer++;
			if (count == limit)
				done = 1;
		}
		i++;
	} while (done == 0);
	*numEvents = count;
	return 0;
}

int sceSystemGestureGetTouchEventsCount(const SceSystemGestureTouchRecognizer *touchRecognizer)
{
	const GestureRecognizer *pRecognizer;

	pRecognizer = (const GestureRecognizer *)touchRecognizer;
	if (pRecognizer == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	if (pRecognizer->initialized != 0)
		return pRecognizer->numEvents;
	return SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
}

int sceSystemGestureGetTouchEventByIndex(const SceSystemGestureTouchRecognizer *touchRecognizer, SceUInt32 index,
	SceSystemGestureTouchEvent *touchEvent)
{
	const GestureRecognizer *pRecognizer;
	SceUInt32 count;
	int found;
	int i;

	pRecognizer = (const GestureRecognizer *)touchRecognizer;
	if (pRecognizer == NULL || touchEvent == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	if (pRecognizer->initialized == 0)
		return SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
	if (index >= pRecognizer->numEvents)
		return SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY;
	found = 0;
	count = 0;
	i = 0;
	do {
		if (i >= 16)
			break;
		if (pRecognizer->slots[i].eventState != 0) {
			if (count == index) {
				found = 1;
				sceClibMemcpy_safe(touchEvent, &pRecognizer->slots[i], sizeof(*touchEvent));
			}
			count++;
		}
		i++;
	} while (found == 0);
	return 0;
}

int sceSystemGestureGetTouchEventByEventID(const SceSystemGestureTouchRecognizer *touchRecognizer, SceUInt32 eventID,
	SceSystemGestureTouchEvent *touchEvent)
{
	const GestureRecognizer *pRecognizer;
	int found;
	int i;

	pRecognizer = (const GestureRecognizer *)touchRecognizer;
	if (pRecognizer == NULL || touchEvent == NULL)
		return SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT;
	if (pRecognizer->initialized == 0)
		return SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED;
	found = 0;
	i = 0;
	do {
		if (i >= 16)
			break;
		if (pRecognizer->slots[i].eventState != 0 && eventID == pRecognizer->slots[i].eventID) {
			found = 1;
			sceClibMemcpy_safe(touchEvent, &pRecognizer->slots[i], sizeof(*touchEvent));
		}
		i++;
	} while (found == 0);
	if (found != 0)
		return 0;
	return SCE_SYSTEM_GESTURE_ERROR_EVENT_DATA_NOT_FOUND;
}

static void CreateMutex(void)
{
	if (sceKernelCreateLwMutex(&g_mutex, "SceSystemGesture", 0, 0, NULL) < 0)
		sceKernelDeleteLwMutex(&g_mutex);
}

static void DeleteMutex(void)
{
	sceKernelDeleteLwMutex(&g_mutex);
}
