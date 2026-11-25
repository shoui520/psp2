/*
 * topmenu_gesture_listener.cpp: motion-sensor orientation listener and screen
 * orientation recommender for foreground applications (0x8129B5E0-0x8129BA97).
 */
#include "topmenu_internal.h"
#include <appmgr/appmgr.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/inputlistener.h>
#include <inputdevice/motionsensor/motionsensor.h>
#include <widget/core/uiinputlistener.h>

namespace {

static paf::common::SharedPtr<paf::inputdevice::InputListener> s_motionListener;
static int s_lastFrontAppId = -1;
static SceUInt32 s_lastOrientationActive = 0;
static int s_lastRecommendedOrientation = 0;
static SceUInt32 s_orientationStableFrames = 0;

/* 0x8129B924 */
static void topmenuMotionSensorHandler(paf::inputdevice::motionsensor::Data *motionData)
{
	if (paf::Framework::Instance()->GetEnvironmentInstance() == NULL)
		return;

	float accX = motionData->basic_orientation.extract_x();
	float accY = motionData->basic_orientation.extract_y();
	float accZ = motionData->basic_orientation.extract_z();
	int magnitudeSq = (int)(accX * accX + accY * accY + accZ * accZ);
	if (magnitudeSq != 1)
		return;

	int orientation = s_lastRecommendedOrientation;
	if (accX >= 0.0f) {
		if (accX <= 0.0f) {
			if (accY > 0.0f)
				orientation = 1;
			else if (accY < 0.0f)
				orientation = 2;
		} else {
			orientation = 4;
		}
	} else {
		orientation = 3;
	}

	if (s_lastRecommendedOrientation == orientation) {
		if (s_orientationStableFrames == 24) {
			sceAppMgrSetRecommendedScreenOrientationForShell(orientation);
			++s_orientationStableFrames;
		} else if (s_orientationStableFrames < 24) {
			++s_orientationStableFrames;
		}
	} else {
		s_orientationStableFrames = 0;
	}
	s_lastRecommendedOrientation = orientation;
}

} /* namespace */

/* 0x8129B5E0 */
void topmenuMotionInit(void)
{
	paf::ui::Environment *env = paf::Framework::Instance()->GetEnvironmentInstance();

	SceFVector3 threshold;
	threshold.x = 20.0f;
	threshold.y = 10.0f;
	threshold.z = 25.0f;
	ScePafMisc_DC13D542(&threshold);

	s_motionListener = paf::common::SharedPtr<paf::inputdevice::InputListener>(
		new paf::ui::UIMotionListener(env));
	paf::inputdevice::motionsensor::SetDeviceHandler(topmenuMotionSensorHandler);
}

/* 0x8129B89C */
void topmenuMotionUpdateFrontApp(void)
{
	SceAppMgrAppStatus status;
	if (sceAppMgrGetStatusByAppId(SHELL_APP_ID_FRONT, &status) != 0)
		return;

	if (status.appId != s_lastFrontAppId ||
	    status.recommendedOrientationActivated != s_lastOrientationActive) {
		if (status.recommendedOrientationActivated == 1)
			paf::inputdevice::AddInputListener(s_motionListener);
		else
			paf::inputdevice::DelInputListener(s_motionListener);
		s_lastFrontAppId = status.appId;
		s_lastOrientationActive = status.recommendedOrientationActivated;
	}
}
