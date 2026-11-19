/*
 * libsystemgesture.h: SceSystemGesture's private state.
 *
 * The library keeps the touch panels' primitive events in a pool of 48 nodes (the SDK's primitive
 * touch event, 0x30 bytes of it, with the list link where the event has its reserved bytes), and
 * each gesture recognizer in the application's SceSystemGestureTouchRecognizer buffer: 16 touch
 * event slots (the SDK's touch event, with the recognizer's working state in the property's
 * reserved bytes) and the target, the parameters and the time of the last update.
 */
#ifndef _LIBSYSTEMGESTURE_H
#define _LIBSYSTEMGESTURE_H

#include <scetypes.h>
#include <systemgesture.h>
#include <touch.h>

#define SYSTEM_GESTURE_NUM_PRIMITIVES	48
#define SYSTEM_GESTURE_NUM_SLOTS	16
#define SYSTEM_GESTURE_NUM_PORTS	2

/* a primitive touch event of the pool */
typedef struct PrimitiveTouchNode {
	SceSystemGestureTouchState eventState;
	SceUInt16 primitiveID;			/* the port in the high byte, the touch ID in the low byte */
	SceSystemGestureVector2 pressedPosition;
	SceInt16 pressedForce;
	SceSystemGestureVector2 currentPosition;
	SceInt16 currentForce;
	SceSystemGestureVector2 deltaVector;
	SceInt16 deltaForce;
	SceUInt64 deltaTime;
	SceUInt64 elapsedTime;
	SceUInt8 moved;				/* the finger has gone further than a tap allows */
	SceUInt8 padding[3];
	struct PrimitiveTouchNode *pNext;
} PrimitiveTouchNode;

/* the primitive recognizer: the pool, the last two touch data of each port and the lists */
typedef struct PrimitiveTouchState {
	PrimitiveTouchNode nodes[SYSTEM_GESTURE_NUM_PRIMITIVES];
	SceTouchData current[SYSTEM_GESTURE_NUM_PORTS];
	SceTouchData previous[SYSTEM_GESTURE_NUM_PORTS];
	PrimitiveTouchNode *pPool;
	PrimitiveTouchNode *pBegin;
	PrimitiveTouchNode *pActive;
	PrimitiveTouchNode *pEnd;
	PrimitiveTouchNode *pCancelled;
	SceUInt8 count;				/* the events of the begin, active, end and cancelled lists */
	SceUInt8 initialized;
	SceUInt8 padding[2];
	float density[SYSTEM_GESTURE_NUM_PORTS][2];	/* pixels per millimetre of each port, x then y */
	SceUInt8 reserved[8];
} PrimitiveTouchState;

/* the working state of each recognizer, in the reserved bytes of its slot's property */
typedef struct TapProperty {
	SceUInt16 primitiveID;
	SceSystemGestureVector2 position;
	SceUInt8 tappedCount;
	SceUInt8 released;			/* the finger of the last tap has gone up */
	SceUInt64 elapsedTime;			/* since it went up */
	SceSystemGestureVector2 releasedPosition;
	SceUInt8 reserved[44];
} TapProperty;

typedef struct PinchProperty {
	float scale;
	struct {
		SceUInt16 primitiveID;
		SceSystemGestureVector2 currentPosition;
		SceSystemGestureVector2 deltaVector;
		SceSystemGestureVector2 pairedPosition;
	} primitive[2];
	SceUInt8 present[2];			/* the primitive's finger is still down */
	SceInt8 turns;				/* the half turns the angle has gone through */
	SceUInt8 padding;
	float lastAngle;
	SceUInt8 reserved[24];
} PinchProperty;

typedef union SlotProperty {
	SceUInt8 propertyBuf[64];
	TapProperty tap;
	SceSystemGestureDragEventProperty drag;
	SceSystemGestureTapAndHoldEventProperty tapAndHold;
	PinchProperty pinchOutIn;
	PinchProperty rotation;
} SlotProperty;

typedef struct TouchSlot {
	SceUInt32 eventID;
	SceSystemGestureTouchState eventState;
	SceSystemGestureType gestureType;
	SceUInt8 padding[4];
	SceUInt64 updatedTime;
	SlotProperty property;
	SceUInt8 reserved[56];
} TouchSlot;

/* the parameters of each recognizer (SceSystemGestureTouchRecognizerParameter as it is kept) */
typedef struct TapParameters {
	SceUInt32 maxTapCount;
	SceUInt32 reserved;
	SceUInt32 maxPressedTime;		/* milliseconds a finger may stay down for a tap */
	SceUInt32 maxMovedDistance;		/* millimetres its position may move */
	SceUInt32 maxTapInterval;		/* milliseconds between two taps of a count */
	SceUInt8 padding[44];
} TapParameters;

typedef struct TapAndHoldParameters {
	SceUInt64 timeToInvokeEvent;		/* milliseconds, the elapsed time is in microseconds */
	SceUInt8 padding[56];
} TapAndHoldParameters;

typedef struct PairParameters {
	SceUInt32 flags;			/* 1 for the two */
	SceUInt32 paired;			/* the two fingers are paired (pinch and rotation) */
	SceUInt8 padding[56];
} PairParameters;

typedef union RecognizerParameters {
	SceUInt8 parameterBuf[64];
	TapParameters tap;
	TapAndHoldParameters tapAndHold;
	PairParameters pair;
} RecognizerParameters;

typedef struct GestureRecognizer {
	SceUInt32 touchPanelPortID;
	SceSystemGestureType gestureType;
	SceUInt64 updatedTime;
	SceUInt32 numEvents;
	SceUInt32 padding1;
	TouchSlot slots[SYSTEM_GESTURE_NUM_SLOTS];
	SceSystemGestureRectangle rectangle;
	RecognizerParameters parameters;
	SceUInt64 firstTime;			/* the time of the first update */
	SceUInt8 initialized;
	SceUInt8 padding2[7];
} GestureRecognizer;

#endif /* _LIBSYSTEMGESTURE_H */
