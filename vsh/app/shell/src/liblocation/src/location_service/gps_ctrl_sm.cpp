/*
 * gps_ctrl_sm.cpp: GpsCtrlStateMachine (5-state GPS hardware open/start/stop/close FSM and
 * "SceLocationGpsCtrl" callback manager; 0x81193DF6-0x81194684, sti 0x811945E0).
 * Sony's class/state names ("GpsCtrlStateMachine", "GPS_CTRL_STATE_*") come from its log strings.
 */
#include "location_internal.h"
#include <std/string.h>

extern "C" {
SceUInt64 sceRtcGetAccumulativeTime(void);
int _sceGpsOpen(SceUID cbId);
int _sceGpsClose(void);
int _sceGpsStart(int mode);
int _sceGpsStop(void);
int _sceGpsResumeCallback(void);
int _sceGpsGetState(int *outStatus);
}

extern int gpsCtrlCallbackEntry(int notifyId, int notifyCount, int notifyArg, void *common); /* 0x8141F7D4 */

class GpsCtrlStateClosed : public GpsCtrlState {
public:
	virtual int HandleEvent(GpsCtrlEvent *ev, GpsCtrlStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return GPS_CTRL_STATE_CLOSED; }
};

class GpsCtrlStateOpened : public GpsCtrlState {
public:
	virtual int HandleEvent(GpsCtrlEvent *ev, GpsCtrlStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return GPS_CTRL_STATE_OPENED; }
};

class GpsCtrlStateStartReq : public GpsCtrlState {
public:
	virtual int HandleEvent(GpsCtrlEvent *ev, GpsCtrlStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return GPS_CTRL_STATE_START_REQ; }
};

class GpsCtrlStateStarted : public GpsCtrlState {
public:
	virtual int HandleEvent(GpsCtrlEvent *ev, GpsCtrlStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return GPS_CTRL_STATE_STARTED; }
};

class GpsCtrlStateStopReq : public GpsCtrlState {
public:
	virtual int HandleEvent(GpsCtrlEvent *ev, GpsCtrlStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return GPS_CTRL_STATE_STOP_REQ; }
};

static GpsCtrlStateClosed s_gpsStateClosed;
static GpsCtrlStateOpened s_gpsStateOpened;
static GpsCtrlStateStartReq s_gpsStateStartReq;
static GpsCtrlStateStarted s_gpsStateStarted;
static GpsCtrlStateStopReq s_gpsStateStopReq;

class GpsCtrlEventOpen : public GpsCtrlEvent {
public:
	GpsCtrlEventOpen(SceUID cbId) : m_cbId(cbId) {}
	virtual int Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm); /* 0x81193EEE */
	SceUID m_cbId;
};

class GpsCtrlEventStart : public GpsCtrlEvent {
public:
	GpsCtrlEventStart(int mode) : m_mode(mode) {}
	virtual int Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm); /* 0x81193F0A */
	int m_mode;
};

class GpsCtrlEventStartResult : public GpsCtrlEvent {
public:
	GpsCtrlEventStartResult(int err) : m_err(err) {}
	virtual int Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm); /* 0x81193F26 */
	int m_err;
};

class GpsCtrlEventStop : public GpsCtrlEvent {
public:
	virtual int Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm); /* 0x81193F48 */
};

class GpsCtrlEventStopResult : public GpsCtrlEvent {
public:
	GpsCtrlEventStopResult(int err) : m_err(err) {}
	virtual int Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm); /* 0x81193F82 */
	int m_err;
};

class GpsCtrlEventClose : public GpsCtrlEvent {
public:
	virtual int Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm); /* 0x81193FA4 */
};

class GpsCtrlEventCheckStatus : public GpsCtrlEvent {
public:
	virtual int Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm); /* 0x81193FDE */
};

/* 0x81193DF6 */
int GpsCtrlStateMachine::ChangeState(int nextState)
{
	switch (nextState) {
	case GPS_CTRL_STATE_CLOSED:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(8, "Changing to GPS_CTRL_STATE_CLOSED state\n");
		m_state = &s_gpsStateClosed;
		break;
	case GPS_CTRL_STATE_OPENED:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(8, "Changing to GPS_CTRL_STATE_OPENED state\n");
		m_state = &s_gpsStateOpened;
		break;
	case GPS_CTRL_STATE_START_REQ:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(8, "Changing to GPS_CTRL_STATE_START_REQ state\n");
		m_state = &s_gpsStateStartReq;
		break;
	case GPS_CTRL_STATE_STARTED:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(8, "Changing to GPS_CTRL_STATE_STARTED state\n");
		m_state = &s_gpsStateStarted;
		break;
	case GPS_CTRL_STATE_STOP_REQ:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(8, "Changing to GPS_CTRL_STATE_STOP_REQ state\n");
		m_state = &s_gpsStateStopReq;
		break;
	default:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(8, "GpsCtrlStateMachine invalid NextState %d\n",
					      nextState);
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	}

	for (int i = 0; i < 3; i++) {
		if (m_mgr->m_listeners[i] != NULL)
			m_mgr->m_listeners[i]->OnStateChanged(nextState);
	}
	return 0;
}

/* 0x81193EE0 */
int GpsCtrlStateMachine::InitState()
{
	m_state = &s_gpsStateClosed;
	return 0;
}

/* 0x81193EEE */
int GpsCtrlEventOpen::Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm)
{
	(void)state;
	int ret = _sceGpsOpen(m_cbId);
	if (ret == 0)
		sm->ChangeState(GPS_CTRL_STATE_OPENED);
	return ret;
}

/* 0x81193F0A */
int GpsCtrlEventStart::Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm)
{
	(void)state;
	int ret = _sceGpsStart(m_mode);
	if (ret == 0)
		sm->ChangeState(GPS_CTRL_STATE_START_REQ);
	return ret;
}

/* 0x81193F26 */
int GpsCtrlEventStartResult::Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm)
{
	(void)state;
	if (m_err != 0)
		sm->ChangeState(GPS_CTRL_STATE_OPENED);
	else
		sm->ChangeState(GPS_CTRL_STATE_STARTED);
	return 0;
}

/* 0x81193F48 */
int GpsCtrlEventStop::Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm)
{
	(void)state;
	int ret = _sceGpsStop();
	if (ret == 0)
		sm->ChangeState(GPS_CTRL_STATE_STOP_REQ);
	else if (ret == (int)0x80390002 || ret == (int)0x80390004)
		sm->ChangeState(GPS_CTRL_STATE_CLOSED);
	return ret;
}

/* 0x81193F82 */
int GpsCtrlEventStopResult::Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm)
{
	(void)state;
	if (m_err != 0)
		sm->ChangeState(GPS_CTRL_STATE_STARTED);
	else
		sm->ChangeState(GPS_CTRL_STATE_OPENED);
	return 0;
}

/* 0x81193FA4 */
int GpsCtrlEventClose::Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm)
{
	(void)state;
	int ret = _sceGpsClose();
	if (ret == 0 || ret == (int)0x80390002 || ret == (int)0x80390004)
		sm->ChangeState(GPS_CTRL_STATE_CLOSED);
	return ret;
}

/* 0x81193FDE */
int GpsCtrlEventCheckStatus::Dispatch(GpsCtrlState *state, GpsCtrlStateMachine *sm)
{
	(void)state;
	int status = 0;
	int ret = _sceGpsGetState(&status);
	if (ret == (int)0x80390002 || status == 1 || status == 2)
		sm->ChangeState(GPS_CTRL_STATE_CLOSED);
	return ret;
}

/* 0x8119402A */
int GpsCtrlManager::CheckStatus()
{
	GpsCtrlEventCheckStatus ev;
	int ret = sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		ret = m_sm.m_state->HandleEvent(&ev, &m_sm);
		int uret = sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x8119408C */
int GpsCtrlPollTask::OnNotify()
{
	if (m_gpsCtrl->m_sm.m_state->GetStateId() == GPS_CTRL_STATE_STARTED)
		m_gpsCtrl->m_lastStartedTime = sceRtcGetAccumulativeTime();

	GpsCtrlEventCheckStatus ev;
	if (sceKernelLockLwMutex(&m_gpsCtrl->m_sm.m_mutex, 1, NULL) == 0) {
		m_gpsCtrl->m_sm.m_state->HandleEvent(&ev, &m_gpsCtrl->m_sm);
		sceKernelUnlockLwMutex(&m_gpsCtrl->m_sm.m_mutex, 1);
	}
	return 1;
}

/* 0x81194106 */
int GpsCtrlCreateCbTask::OnNotify()
{
	if (m_gpsCtrl->m_needCreateCb == 1 && m_gpsCtrl->m_callbackId == -1) {
		SceUID cbId = sceKernelCreateCallback("SceLocationGpsCtrl", 0,
						      gpsCtrlCallbackEntry, m_gpsCtrl);
		m_gpsCtrl->m_callbackId = cbId;
		if (g_locationLogger != NULL)
			g_locationLogger->Log(8, "m_callbackId created 0x%x\n", cbId);
		m_gpsCtrl->m_needCreateCb = 0;
	}
	return 1;
}

/* 0x81194160 */
int GpsCtrlManager::Init()
{
	sce_paf_memset(m_listeners, 0, sizeof(m_listeners));
	m_callbackId = -1;
	m_lastStartedTime = 0;
	m_needCreateCb = 1;

	int ret = g_locationManager->RegisterObserver(this);
	if (ret == 0) {
		m_sm.m_mutexResult = sceKernelCreateLwMutex(&m_sm.m_mutex, "SceLocationCommon",
							    2, 0, NULL);
		ret = m_sm.m_mutexResult;
		if (ret == 0)
			ret = m_sm.InitState();
		if (ret == 0) {
			g_locationManager->RegisterTask(&m_pollTask);
			g_locationManager->RegisterTask(&m_createCbTask);
			m_createCbTask.Schedule(1);
		}
	}
	return ret;
}

/* 0x811941EE */
int GpsCtrlManager::Term()
{
	int ret = 0;
	if (m_createCbTask.m_mgr != NULL) {
		m_createCbTask.Cancel();
		g_locationManager->UnregisterTask(&m_createCbTask);
	}
	if (m_callbackId >= 0) {
		ret = sceKernelDeleteCallback(m_callbackId);
		m_callbackId = -1;
	}
	if (m_pollTask.m_mgr != NULL) {
		m_pollTask.Cancel();
		g_locationManager->UnregisterTask(&m_pollTask);
	}
	sce_paf_memset(m_listeners, 0, sizeof(m_listeners));

	int smRet = 0;
	if (m_sm.m_mutexResult == 0) {
		smRet = sceKernelDeleteLwMutex(&m_sm.m_mutex);
		if (smRet == 0)
			m_sm.m_mutexResult = -1;
	}
	if (smRet == 0)
		smRet = m_sm.TermState();
	m_sm.m_state = NULL;
	if (smRet != 0)
		ret = smRet;
	g_locationManager->UnregisterObserver(this);
	return ret;
}

/* 0x81194294 */
int GpsCtrlManager::AddListener(GpsCtrlListener *listener)
{
	for (int i = 0; i < 3; i++) {
		if (m_listeners[i] == NULL) {
			m_listeners[i] = listener;
			return 0;
		}
	}
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* 0x811942C4 */
int GpsCtrlManager::RemoveListener(GpsCtrlListener *listener)
{
	for (int i = 0; i < 3; i++) {
		if (m_listeners[i] == listener) {
			m_listeners[i] = NULL;
			return 0;
		}
	}
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* 0x811942F6 */
int GpsCtrlManager::Open()
{
	GpsCtrlEventOpen ev(m_callbackId);
	int ret = sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		ret = m_sm.m_state->HandleEvent(&ev, &m_sm);
		int uret = sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x8119435C */
int GpsCtrlManager::Start(int mode)
{
	if (m_callbackId < 0)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	GpsCtrlEventStart ev(mode);
	int ret = sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		ret = m_sm.m_state->HandleEvent(&ev, &m_sm);
		int uret = sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x811943CE */
int GpsCtrlManager::Stop()
{
	if (m_callbackId < 0)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	GpsCtrlEventStop ev;
	int ret = sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		ret = m_sm.m_state->HandleEvent(&ev, &m_sm);
		int uret = sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x8119443E */
int GpsCtrlManager::Close()
{
	if (m_callbackId < 0)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	GpsCtrlEventClose ev;
	int ret = sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		ret = m_sm.m_state->HandleEvent(&ev, &m_sm);
		int uret = sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x811944AE */
int GpsCtrlManager::HandleGpsEvent(int notifyId, int notifyCount, int notifyArg)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "_handleGpsEvent received 0x%x\n", notifyArg);

	int ret = sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		if ((notifyArg & 0xF0000) == 0x10000) {
			GpsCtrlEventStartResult ev(notifyArg & 0xFF);
			if (sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL) == 0) {
				m_sm.m_state->HandleEvent(&ev, &m_sm);
				sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
			}
		} else if ((notifyArg & 0xF0000) == 0x20000) {
			GpsCtrlEventStopResult ev(notifyArg & 0xFF);
			if (sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL) == 0) {
				m_sm.m_state->HandleEvent(&ev, &m_sm);
				sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
			}
		}

		for (int i = 0; i < 3; i++) {
			if (m_listeners[i] != NULL)
				m_listeners[i]->OnGpsEvent(notifyId, notifyCount, notifyArg);
		}

		int uret = sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		if (uret != 0)
			ret = uret;
	}
	_sceGpsResumeCallback();
	return ret;
}

/* 0x8141F7D4 */
int gpsCtrlCallbackEntry(int notifyId, int notifyCount, int notifyArg, void *common)
{
	return static_cast<GpsCtrlManager *>(common)->HandleGpsEvent(
		notifyId, notifyCount, notifyArg);
}
