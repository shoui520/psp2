/*
 * SceCamImportDialogPlugin: Motion/orientation listener for camera UI rotation.
 */
#ifndef CAMERAIMPORT_DIALOG_MOTION_LISTENER_H
#define CAMERAIMPORT_DIALOG_MOTION_LISTENER_H

#include <stdint.h>
#include <std/list>
#include <inputdevice/inputdevice.h>
#include <inputdevice/inputlistener.h>

/* Device orientation states */
enum {
	CAMERA_ORIENTATION_NORMAL = 1,
	CAMERA_ORIENTATION_180 = 2,
	CAMERA_ORIENTATION_90 = 3,
	CAMERA_ORIENTATION_270 = 4
};

class MotionHandler {
public:
	virtual ~MotionHandler();
	virtual void OnOrientationChanged(int orientation) = 0;
	virtual void OnMotionSensorError(int error) {}
};

class MotionInputListener : public paf::inputdevice::InputGestureListener {
public:
	struct Entry {
		MotionHandler *handler;
		int reserved;
		bool enabled;
	};

	MotionInputListener();
	virtual ~MotionInputListener() {}
	virtual void OnUpdate(const paf::inputdevice::Data *data);

	paf::list<Entry> m_handlers;
	int m_orientation;
	int m_pendingOrientation;
	int m_stableCount;
	int m_lastAxis;
	int m_disabled;
	int m_sensorError;
};

void AddMotionHandler(MotionHandler *handler, int flags);
void RemoveMotionHandler(MotionHandler *handler);
void EnableMotionHandler(MotionHandler *handler);
void DisableMotionHandler(MotionHandler *handler);
int GetMotionOrientation(void);
int GetMotionSensorError(void);
void CreateMotionListener(void);
void DestroyMotionListener(void);
void StartMotionListener(void);
void StopMotionListener(void);

#endif /* CAMERAIMPORT_DIALOG_MOTION_LISTENER_H */
