/*
 * systemgesture.h: the system gesture library's error codes, gesture types, records and functions,
 * under Sony's names and with Sony's definitions (SDK Release 03.570.011, target/include/
 * systemgesture.h).
 */
#ifndef _SCE_SYSTEM_GESTURE_SYSTEMGESTURE_H
#define _SCE_SYSTEM_GESTURE_SYSTEMGESTURE_H

#include <scetypes.h>
#include <touch.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_SYSTEM_GESTURE_ERROR_INVALID_ARGUMENT -2138505215	/* 0x80890001 */
#define SCE_SYSTEM_GESTURE_ERROR_NOT_SUPPORTED_GESTURE -2138505214	/* 0x80890002 */
#define SCE_SYSTEM_GESTURE_ERROR_NOT_INITIALIZED -2138505213	/* 0x80890003 */
#define SCE_SYSTEM_GESTURE_ERROR_INDEX_OUT_OF_ARRAY -2138505212	/* 0x80890004 */
#define SCE_SYSTEM_GESTURE_ERROR_EVENT_DATA_NOT_FOUND -2138505211	/* 0x80890005 */
typedef enum SceSystemGestureTouchState
{
	SCE_SYSTEM_GESTURE_TOUCH_STATE_INACTIVE		= 0x00000000,
	SCE_SYSTEM_GESTURE_TOUCH_STATE_BEGIN		= 0x00000001,
	SCE_SYSTEM_GESTURE_TOUCH_STATE_ACTIVE		= 0x00000002,
	SCE_SYSTEM_GESTURE_TOUCH_STATE_END			= 0x00000003,
	SCE_SYSTEM_GESTURE_TOUCH_STATE_CANCELLED	= 0x00000004

} SceSystemGestureTouchState;
typedef enum SceSystemGestureType
{
	SCE_SYSTEM_GESTURE_TYPE_TAP				= 0x00000001,
	SCE_SYSTEM_GESTURE_TYPE_DRAG			= 0x00000002,
	SCE_SYSTEM_GESTURE_TYPE_TAP_AND_HOLD	= 0x00000004,
	SCE_SYSTEM_GESTURE_TYPE_PINCH_OUT_IN	= 0x00000008,
	SCE_SYSTEM_GESTURE_TYPE_ROTATION		= 0x00000010
} SceSystemGestureType;
#pragma pack(push,8)
typedef struct SceSystemGestureVector2
{
	SceInt16 x;
	SceInt16 y;

} SceSystemGestureVector2;
typedef struct SceSystemGestureRectangle
{
	SceInt16 x;
	SceInt16 y;
	SceInt16 width;
	SceInt16 height;

} SceSystemGestureRectangle;
typedef struct SceSystemGesturePrimitiveTouchEvent
{
	SceSystemGestureTouchState eventState;              /*E state of primitive touch event */
	SceUInt16 primitiveID;				/*E logical add of touch panel port(higher 8bit) and touch ID(lower 8bit) */

	SceSystemGestureVector2 pressedPosition;          /*E pressed time position */
	SceInt16 pressedForce;                            /*E force value of pressed time */

	SceSystemGestureVector2 currentPosition;          /*E current finger position */
	SceInt16 currentForce;                            /*E current force value */

	SceSystemGestureVector2 deltaVector;              /*E distance from previous updated position */
	SceInt16 deltaForce;                              /*E force value difference from previous updated time */

	SceUInt64 deltaTime;			/* elapsed time from previous update (microseconds) */

	SceUInt64 elapsedTime;			/* elapsed tiem from when finger pressed (microseconds) */

	SceUInt8 reserved[56];

} SceSystemGesturePrimitiveTouchEvent;
typedef struct SceSystemGesturePrimitiveTouchRecognizerParameter
{
	SceUInt8 reserved[ 64 ]; /*E reserved */

} SceSystemGesturePrimitiveTouchRecognizerParameter;
typedef struct SceSystemGestureTouchRecognizer
{
	SceUInt64 reserved[ 307 ]; /*E reserved */
} SceSystemGestureTouchRecognizer;
typedef struct SceSystemGestureTouchRecognizerInformation
{
	SceSystemGestureType gestureType;		/*E touch gesture type */
	SceUInt32 touchPanelPortID;				/*E target touch panel port */
	SceSystemGestureRectangle rectangle;	/*E target rectangle */
	SceUInt64 updatedTime;					/*E last updated time */

	SceUInt8 reserved[ 256 ];				/*E reserved */
} SceSystemGestureTouchRecognizerInformation;
typedef struct SceSystemGestureTapRecognizerParameter
{
	SceUInt8		maxTapCount; /*E maximum tap counts to be recognized (if 0 was set, 1 was used instead) */

	SceUInt8 reserved[ 63 ]; /*E reserved */

} SceSystemGestureTapRecognizerParameter;
typedef struct SceSystemGestureDragRecognizerParameter
{
	SceUInt8 reserved[ 64 ]; /*E reserved */
} SceSystemGestureDragRecognizerParameter;
typedef struct SceSystemGestureTapAndHoldRecognizerParameter
{
	SceUInt64		timeToInvokeEvent;	/*E time to invoke this touch event (msec) */

	SceUInt8 reserved[ 56 ];			/*E reserved */
} SceSystemGestureTapAndHoldRecognizerParameter;
typedef struct SceSystemGesturePinchOutInRecognizerParameter
{
	SceUInt8 reserved[ 64 ]; /*E reserved */
} SceSystemGesturePinchOutInRecognizerParameter;
typedef struct SceSystemGestureRotationRecognizerParameter
{
	SceUInt8 reserved[ 64 ]; /*E reserved */
} SceSystemGestureRotationRecognizerParameter;
typedef union SceSystemGestureTouchRecognizerParameter
{
	SceUInt8 parameterBuf[ 64 ];

	SceSystemGestureTapRecognizerParameter			tap;			/*E parameters for Tap gesture recgonizer */
	SceSystemGestureDragRecognizerParameter			drag;			/*E parameters for Drag gesture recgonizer */
	SceSystemGestureTapAndHoldRecognizerParameter	tapAndHold;		/*E parameters for Tap and Hold gesture recgonizer */
	SceSystemGesturePinchOutInRecognizerParameter	pinchOutIn;		/*E parameters for Pinch Out/In gesture recgonizer */
	SceSystemGestureRotationRecognizerParameter		rotation;		/*E parameters for Rotation gesture recognizer */

} SceSystemGestureTouchRecognizerParameter;
typedef struct SceSystemGestureTapEventProperty {
	SceUInt16 primitiveID;              /*E primitive touch event ID related with this event */
	SceSystemGestureVector2 position;   /*E tapped position */
	SceUInt8 tappedCount;               /*E tapped count */

	SceUInt8 reserved[ 57 ]; /*E reserved */
} SceSystemGestureTapEventProperty;
typedef struct SceSystemGestureDragEventProperty {
	SceUInt16 primitiveID;              /*E primitive touch event ID related with this event */
	SceSystemGestureVector2 deltaVector; /*E distance from previous updated position */
	SceSystemGestureVector2 currentPosition; /*E current finger position */
	SceSystemGestureVector2 pressedPosition; /*E pressed time position */

	SceUInt8 reserved[ 50 ]; /*E reserved */
} SceSystemGestureDragEventProperty;
typedef struct SceSystemGestureTapAndHoldEventProperty {
	SceUInt16 primitiveID;              /*E primitive touch event ID related with this event */
	SceSystemGestureVector2 pressedPosition; /*E position of pressed time */

	SceUInt8 reserved[ 58 ]; /*E reserved */
} SceSystemGestureTapAndHoldEventProperty;
typedef struct SceSystemGesturePinchOutInEventProperty {
	float scale;   /* scale factor relative to the positions at when two fingers were paired  */

	struct {
		SceUInt16 primitiveID;						/*E primitive touch event ID related with this event */
		SceSystemGestureVector2 currentPosition;	/*E current positions */
		SceSystemGestureVector2 deltaVector;		/*E distance from previous updated position */
		SceSystemGestureVector2 pairedPosition;		/*E position at when two fingers were paired */
	} primitive[2];              /*E touch datas related with this event */

	SceUInt8 reserved[ 32 ]; /*E reserved */
} SceSystemGesturePinchOutInEventProperty;
typedef struct SceSystemGestureRotationEventProperty {
	float angle;   /* rotation angle in radians from when two fingers were paired */

	struct {
		SceUInt16 primitiveID;						/*E primitive touch event ID related with this event */
		SceSystemGestureVector2 currentPosition;	/*E current positions */
		SceSystemGestureVector2 deltaVector;		/*E distance from previous updated position */
		SceSystemGestureVector2 pairedPosition;		/*E position at when two fingers were paired */
	} primitive[2];

	SceUInt8 reserved[ 32 ]; /*E reserved */
} SceSystemGestureRotationEventProperty;
typedef struct SceSystemGestureTouchEvent
{
	SceUInt32 eventID;						/*E touch gesture event ID */
	SceSystemGestureTouchState eventState;	/*E touch gesture state */
	SceSystemGestureType gestureType;		/*E touch gesture type */

	SceUInt8 padding[4];			/*E reserved */
	SceUInt64 updatedTime;          /*E last updated time */

	union {
		SceUInt8 propertyBuf[ 64 ]; /*E buffer */

		SceSystemGestureTapEventProperty tap;				/*E property for Tap event */
		SceSystemGestureDragEventProperty drag;				/*E property for Drag event */
		SceSystemGestureTapAndHoldEventProperty tapAndHold; /*E property for Tap and Hold event */
		SceSystemGesturePinchOutInEventProperty pinchOutIn; /*E property for Pinch Out/In event */
		SceSystemGestureRotationEventProperty rotation;		/*E property for Rotation event */
	} property; /*E union of touch event properties */

	SceUInt8 reserved[56]; /*E reserved */

} SceSystemGestureTouchEvent;

#pragma pack(pop)

int sceSystemGestureInitializePrimitiveTouchRecognizer(SceSystemGesturePrimitiveTouchRecognizerParameter* parameter);
int sceSystemGestureFinalizePrimitiveTouchRecognizer(void);
int sceSystemGestureResetPrimitiveTouchRecognizer(void);
int sceSystemGestureUpdatePrimitiveTouchRecognizer(const SceTouchData* pFrontData, const SceTouchData* pBackData);
int sceSystemGestureGetPrimitiveTouchEvents(SceSystemGesturePrimitiveTouchEvent* primitiveEventBuffer, const SceUInt32 capacityOfBuffer, SceUInt32* numberOfEvent);
int sceSystemGestureGetPrimitiveTouchEventsCount(void);
int sceSystemGestureGetPrimitiveTouchEventByIndex(const SceUInt32 index, SceSystemGesturePrimitiveTouchEvent* primitiveTouchEvent);
int sceSystemGestureGetPrimitiveTouchEventByPrimitiveID(const SceUInt16 primitiveID, SceSystemGesturePrimitiveTouchEvent* primitiveTouchEvent);
int sceSystemGestureCreateTouchRecognizer(SceSystemGestureTouchRecognizer* touchRecognizer,
										  const SceSystemGestureType gestureType,
										  const SceUInt8 touchPanelPortID,
										  const SceSystemGestureRectangle* rectangle,
										  const SceSystemGestureTouchRecognizerParameter* touchRecognizerParameter );
int sceSystemGestureGetTouchRecognizerInformation(const SceSystemGestureTouchRecognizer* touchRecognizer,
												  SceSystemGestureTouchRecognizerInformation* information);
int sceSystemGestureResetTouchRecognizer( SceSystemGestureTouchRecognizer* touchRecognizer );
int sceSystemGestureUpdateTouchRecognizer( SceSystemGestureTouchRecognizer* touchRecognizer );
int sceSystemGestureUpdateTouchRecognizerRectangle( SceSystemGestureTouchRecognizer* touchRecognizer, const SceSystemGestureRectangle* rectangle );
int sceSystemGestureGetTouchEvents( const SceSystemGestureTouchRecognizer* touchRecognizer,
								  SceSystemGestureTouchEvent* eventBuffer, const SceUInt32 capacityOfBuffer, SceUInt32* numberOfEvent);
int sceSystemGestureGetTouchEventsCount( const SceSystemGestureTouchRecognizer* touchRecognizer );
int sceSystemGestureGetTouchEventByIndex( const SceSystemGestureTouchRecognizer* touchRecognizer,
										 const SceUInt32 index,
										 SceSystemGestureTouchEvent* touchEvent);
int sceSystemGestureGetTouchEventByEventID( const SceSystemGestureTouchRecognizer* touchRecognizer,
										   const SceUInt32 eventID,
										   SceSystemGestureTouchEvent* touchEvent);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_SYSTEM_GESTURE_SYSTEMGESTURE_H */
