/*
 * SceCamImportDialogPlugin: Motion/orientation listener for camera UI rotation.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/shared_ptr.h>
#include <inputdevice/inputdevice.h>
#include "motion_listener.h"

extern "C" {
void ScePafMisc_DC13D542(const float thresholds[3]);
}

using namespace paf;
using namespace paf::common;
using namespace paf::inputdevice;

static MotionInputListener *s_motionListener = NULL;
static SharedPtr<InputListener> s_motionListenerRef;

MotionHandler::~MotionHandler()
{
}

MotionInputListener::MotionInputListener()
	: m_orientation(CAMERA_ORIENTATION_NORMAL), m_pendingOrientation(0), m_stableCount(0),
	  m_lastAxis(0), m_disabled(0), m_sensorError(0)
{
	m_type = 5;
}

void MotionInputListener::OnUpdate(const Data *data)
{
	if (data == NULL || m_disabled != 0)
		return;
	/* Dispatch orientation updates to enabled handlers */
	for (list<Entry>::iterator it = m_handlers.begin(); it != m_handlers.end(); ++it) {
		if (it->enabled && it->handler != NULL)
			it->handler->OnOrientationChanged(m_orientation);
	}
}

void AddMotionHandler(MotionHandler *handler, int flags)
{
	if (s_motionListener == NULL)
		return;
	MotionInputListener::Entry entry;

	entry.handler = handler;
	entry.reserved = flags;
	entry.enabled = false;
	s_motionListener->m_handlers.push_back(entry);
}

void RemoveMotionHandler(MotionHandler *handler)
{
	if (s_motionListener == NULL)
		return;
	for (list<MotionInputListener::Entry>::iterator it = s_motionListener->m_handlers.begin();
	     it != s_motionListener->m_handlers.end(); ++it) {
		if (it->handler == handler) {
			s_motionListener->m_handlers.erase(it);
			return;
		}
	}
}

void EnableMotionHandler(MotionHandler *handler)
{
	if (s_motionListener == NULL)
		return;
	for (list<MotionInputListener::Entry>::iterator it = s_motionListener->m_handlers.begin();
	     it != s_motionListener->m_handlers.end(); ++it) {
		if (it->handler == handler) {
			it->enabled = true;
			return;
		}
	}
}

void DisableMotionHandler(MotionHandler *handler)
{
	if (s_motionListener == NULL)
		return;
	for (list<MotionInputListener::Entry>::iterator it = s_motionListener->m_handlers.begin();
	     it != s_motionListener->m_handlers.end(); ++it) {
		if (it->handler == handler) {
			it->enabled = false;
			return;
		}
	}
}

int GetMotionOrientation(void)
{
	return (s_motionListener != NULL) ? s_motionListener->m_orientation
					  : CAMERA_ORIENTATION_NORMAL;
}

int GetMotionSensorError(void)
{
	return (s_motionListener != NULL) ? s_motionListener->m_sensorError : 0;
}

void CreateMotionListener(void)
{
	float thresholds[3] = { 20.0f, 10.0f, 25.0f };

	ScePafMisc_DC13D542(thresholds);
	s_motionListener = new MotionInputListener();
	s_motionListenerRef = SharedPtr<InputListener>(s_motionListener);
}

void DestroyMotionListener(void)
{
	s_motionListener = NULL;
	s_motionListenerRef.reset();
}

void StartMotionListener(void)
{
	AddInputListener(s_motionListenerRef);
}

void StopMotionListener(void)
{
	if (s_motionListenerRef.get() != NULL)
		DelInputListener(s_motionListenerRef);
}
