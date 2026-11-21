/*
 * The geolocation monitor: one sceLocation handle for every web view, whose location and
 * heading callbacks pass the device's position to each view that asked for it. Sony's file
 * name is not known (the module has no debug records); the mutex's name says what it is
 * ("SceWebKitGeolocationMonitor"), and the names here are ours.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <liblocation.h>
#include <sysmodule/sysmodule.h>
#include "web_view.h"

namespace paf {
namespace ui {

/* a lightweight mutex for the life of the object */
class GeolocationMutex {
public:
	GeolocationMutex(const char *name)
	{
		sceKernelCreateLwMutex(&m_work, name,
			SCE_KERNEL_LW_MUTEX_ATTR_TH_PRIO | SCE_KERNEL_LW_MUTEX_ATTR_RECURSIVE, 0, NULL);
	}

	~GeolocationMutex()
	{
		sceKernelDeleteLwMutex(&m_work);
	}

	SceKernelLwMutexWork m_work;
};

/* the mutex held for the scope, when there is one */
class GeolocationLock {
public:
	GeolocationLock(GeolocationMutex *mutex) : m_mutex(mutex)
	{
		if (m_mutex != NULL)
			sceKernelLockLwMutex(&m_mutex->m_work, 1, NULL);
	}

	~GeolocationLock()
	{
		if (m_mutex != NULL)
			sceKernelUnlockLwMutex(&m_mutex->m_work, 1);
	}

	GeolocationMutex *m_mutex;
};

class WebViewGeolocationMonitor;

/* the monitor's state: the handle and the result of opening it, whether sceLocation is set up,
   the started clients and the last position */
static SceLocationHandle s_handle = SCE_LOCATION_HANDLE_INVALID;
static SceInt32 s_result;
static bool s_initialized;
static WebViewGeolocationMonitor *s_clients[16];
static GeolocationMutex s_mutex("SceWebKitGeolocationMonitor");
static WebIpcGeolocationPosition s_position;

/* a page's location: the monitor's client while it is started */
class WebViewGeolocationMonitor : public WebViewGeolocation {
public:
	WebViewGeolocationMonitor(WebViewGeolocationClient *client) : m_client(client), m_started(false)
	{
	}

	virtual void Start()
	{
		if (m_started)
			return;
		m_started = true;
		GeolocationMonitorStart(this);
	}

	virtual void Stop()
	{
		if (!m_started)
			return;
		GeolocationMonitorStop(this);
		m_started = false;
	}

	virtual int GetPermission()
	{
		int permission = GeolocationMonitorGetPermission();

		if (permission != 0)
			return permission;
		return GeolocationMonitorConfirm();
	}

	virtual void AbortPermission()
	{
		GeolocationMonitorAbortConfirm();
	}

	virtual ~WebViewGeolocationMonitor()
	{
		if (m_started) {
			GeolocationMonitorStop(this);
			m_started = false;
		}
	}

	/* the monitor's position passed to the client */
	virtual void Notify()
	{
		if (m_client != NULL)
			m_client->OnPosition(s_position);
	}

	static void GeolocationMonitorStart(WebViewGeolocationMonitor *client) __attribute__((noinline));
	static void GeolocationMonitorStop(WebViewGeolocationMonitor *client) __attribute__((noinline));
	static int GeolocationMonitorGetPermission() __attribute__((noinline));
	static int GeolocationMonitorConfirm() __attribute__((noinline));
	static void GeolocationMonitorAbortConfirm() __attribute__((noinline));

	WebViewGeolocationClient *m_client;
	bool m_started;
};

/* sceLocation loaded, set up and opened once: 0, or the error */
static inline SceInt32 OpenLocation()
{
	if (s_initialized && s_handle != SCE_LOCATION_HANDLE_INVALID)
		return s_result;
	if (!s_initialized) {
		if (sceSysmoduleIsLoaded(SCE_SYSMODULE_LOCATION) == SCE_SYSMODULE_ERROR_UNLOADED) {
			s_result = sceSysmoduleLoadModule(SCE_SYSMODULE_LOCATION);
			if (s_result != 0)
				return s_result;
		}
		s_result = sceLocationInit(1, SCE_LOCATION_MODE_V2 | SCE_LOCATION_MODE_CONFIRM_AUTO);
		if (s_result != 0)
			return s_result;
		s_initialized = true;
	}
	if (s_handle == SCE_LOCATION_HANDLE_INVALID) {
		s_result = sceLocationOpen(&s_handle, SCE_LOCATION_LMETHOD_NONE,
			SCE_LOCATION_HMETHOD_NONE);
		if (s_result != 0)
			return s_result;
	}
	return 0;
}

static SceInt32 OpenMonitor() __attribute__((noinline));

WebViewGeolocation *CreateGeolocation(WebViewGeolocationClient *client)
{
	if (OpenMonitor() != 0)
		return NULL;
	return new WebViewGeolocationMonitor(client);
}

void DestroyGeolocation(WebViewGeolocation *geolocation)
{
	if (geolocation != NULL)
		delete geolocation;
}

static void OnLocation(SceInt32 result, SceLocationHandle handle,
	const SceLocationLocationInfo *location, void *userdata);
static void OnHeading(SceInt32 result, SceLocationHandle handle,
	const SceLocationHeadingInfo *heading, void *userdata);

/* the client added; the first one starts the callbacks */
void AddClient(WebViewGeolocationMonitor *client)
{
	if (OpenLocation() != 0 || client == NULL)
		return;

	GeolocationLock lock(&s_mutex);
	WebViewGeolocationMonitor **slot = NULL;
	bool first = true;

	for (int i = 0; i < 16; i++) {
		if (s_clients[i] != NULL) {
			if (s_clients[i] == client)
				return;
			first = false;
		} else {
			slot = &s_clients[i];
		}
	}
	if (slot == NULL)
		return;
	*slot = client;
	if (first && sceLocationReopen(s_handle, SCE_LOCATION_LMETHOD_AGPS_AND_3G_AND_WIFI,
				       SCE_LOCATION_HMETHOD_HORIZONTAL |
				       SCE_LOCATION_HEADING_TIMING_1SEC |
				       SCE_LOCATION_HEADING_RESOLUTION_DEG5 |
				       SCE_LOCATION_HEADING_FILTER_AVERAGE_ENABLE |
				       SCE_LOCATION_HEADING_FILTER_HYSTERESIS_ENABLE) == 0) {
		sceLocationStartLocationCallback(s_handle, 5, OnLocation, NULL);
		sceLocationStartHeadingCallback(s_handle, 5.0f, OnHeading, NULL);
	}
}

/* the client taken out; the last one closes the handle */
void RemoveClient(WebViewGeolocationMonitor *client)
{
	if (client == NULL)
		return;

	bool removed = false;
	bool last = true;

	{
		GeolocationLock lock(&s_mutex);

		for (int i = 0; i < 16; i++) {
			if (s_clients[i] != NULL) {
				if (s_clients[i] == client) {
					s_clients[i] = NULL;
					removed = true;
				} else {
					last = false;
				}
			}
		}
	}
	if (removed && last) {
		sceLocationStopHeadingCallback(s_handle);
		sceLocationStopLocationCallback(s_handle);
		sceLocationClose(s_handle);
		s_handle = SCE_LOCATION_HANDLE_INVALID;
	}
}

/* 1 allowed, 0 not known yet, 2 denied or no location */
int CheckPermission()
{
	if (OpenLocation() != 0)
		return 2;

	SceLocationPermissionInfo info = { 0 };
	SceInt32 result = sceLocationGetPermission(s_handle, &info);

	if (result == 0)
		return 1;
	if (result == SCE_LOCATION_INFO_DENIED_BY_USER &&
	    (info.updated == SCE_LOCATION_PERMISSION_UPDATED ||
	     info.applicationstatus == SCE_LOCATION_PERMISSION_APPLICATION_INIT))
		return 0;
	return 2;
}

/* the user asked, and their answer: 0 while the dialog runs, 1 allowed, 2 denied */
int ConfirmPermission()
{
	if (OpenLocation() != 0)
		return 2;

	SceLocationDialogStatus status;

	if (sceLocationConfirmGetStatus(s_handle, &status) != 0)
		return 2;
	switch (status) {
	case SCE_LOCATION_DIALOG_STATUS_IDLE:
		sceLocationConfirm(s_handle);
		return 0;
	case SCE_LOCATION_DIALOG_STATUS_RUNNING:
		return 0;
	case SCE_LOCATION_DIALOG_STATUS_FINISHED: {
		SceLocationDialogResult dialogResult;
		int permission = 2;

		if (sceLocationConfirmGetResult(s_handle, &dialogResult) == 0 &&
		    dialogResult == SCE_LOCATION_DIALOG_STATUS_ENABLE)
			permission = 1;
		sceLocationConfirmAbort(s_handle);
		return permission;
	}
	}
	return 2;
}

void AbortConfirm()
{
	if (OpenLocation() != 0)
		return;
	sceLocationConfirmAbort(s_handle);
}

/* every client told of the position */
static inline void NotifyClients()
{
	GeolocationLock lock(&s_mutex);

	for (int i = 0; i < 16; i++) {
		if (s_clients[i] != NULL)
			s_clients[i]->Notify();
	}
}

/* the location kept (-9999: not known; the speed times 1000 / 60) */
static void OnLocation(SceInt32 result, SceLocationHandle handle,
	const SceLocationLocationInfo *location, void *userdata)
{
	s_position.m_timestamp = (double)(location->timestamp.tick / 1000);
	if (location->latitude == -9999.0) {
		s_position.m_hasLatitude = false;
	} else {
		s_position.m_latitude = location->latitude;
		s_position.m_hasLatitude = true;
	}
	if (location->longitude == -9999.0) {
		s_position.m_hasLongitude = false;
	} else {
		s_position.m_longitude = location->longitude;
		s_position.m_hasLongitude = true;
	}
	if (location->accuracy == -9999.0f) {
		s_position.m_hasAccuracy = false;
	} else {
		s_position.m_hasAccuracy = true;
		s_position.m_accuracy = location->accuracy;
	}
	if (location->altitude == -9999.0) {
		s_position.m_hasAltitude = false;
	} else {
		s_position.m_altitude = location->altitude;
		s_position.m_hasAltitude = true;
	}
	s_position.m_hasAltitudeAccuracy = false;
	if (location->speed == -9999.0f) {
		s_position.m_hasSpeed = false;
	} else {
		s_position.m_hasSpeed = true;
		s_position.m_speed = location->speed * 1000.0f / 60.0f;
	}
	NotifyClients();
}

/* the heading kept */
static void OnHeading(SceInt32 result, SceLocationHandle handle,
	const SceLocationHeadingInfo *heading, void *userdata)
{
	if (heading->heading == -9999.0f) {
		s_position.m_hasHeading = false;
	} else {
		s_position.m_hasHeading = true;
		s_position.m_heading = heading->heading;
	}
	NotifyClients();
}

void WebViewGeolocationMonitor::GeolocationMonitorStart(WebViewGeolocationMonitor *client)
{
	AddClient(client);
}

void WebViewGeolocationMonitor::GeolocationMonitorStop(WebViewGeolocationMonitor *client)
{
	RemoveClient(client);
}

int WebViewGeolocationMonitor::GeolocationMonitorGetPermission()
{
	return CheckPermission();
}

int WebViewGeolocationMonitor::GeolocationMonitorConfirm()
{
	return ConfirmPermission();
}

void WebViewGeolocationMonitor::GeolocationMonitorAbortConfirm()
{
	AbortConfirm();
}

/* OpenLocation, not inlined */
static SceInt32 OpenMonitor()
{
	return OpenLocation();
}

} /* namespace ui */
} /* namespace paf */
