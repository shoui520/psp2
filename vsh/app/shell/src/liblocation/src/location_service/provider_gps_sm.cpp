/*
 * provider_gps_sm.cpp: ProviderGpsStateMachine (8-state XPS/GPS provider state machine;
 * 0x81196BB8-0x811976D0, sti 0x811975D4). Sony's class/state names ("ProviderGpsStateMachine",
 * "PROVIDER_GPS_STATE_*") come from its log strings.
 */
#include "location_internal.h"
#include <std/string.h>

extern "C" {
int _sceGpsGetState(int *outStatus);
int _sceGpsGetData(GpsLocation *data, GpsSatellites *satellites);
}

enum ProviderGpsStateId {
	PROVIDER_GPS_STATE_NO_DEVICE = 0,
	PROVIDER_GPS_STATE_WAIT_OPEN = 1,
	PROVIDER_GPS_STATE_WAIT_START = 2,
	PROVIDER_GPS_STATE_REQ_START = 3,
	PROVIDER_GPS_STATE_STATE_BUSY = 4,
	PROVIDER_GPS_STATE_DATA_RECEIVED = 5,
	PROVIDER_GPS_STATE_ALREADY_RECEIVED = 6,
	PROVIDER_GPS_STATE_REQ_STOP = 7
};

class ProviderGpsStateNoDevice : public ProviderGpsState {
public:
	virtual int HandleEvent(ProviderGpsEvent *ev, ProviderGpsStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return PROVIDER_GPS_STATE_NO_DEVICE; }
};

class ProviderGpsStateWaitOpen : public ProviderGpsState {
public:
	virtual int HandleEvent(ProviderGpsEvent *ev, ProviderGpsStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return PROVIDER_GPS_STATE_WAIT_OPEN; }
};

class ProviderGpsStateWaitStart : public ProviderGpsState {
public:
	virtual int HandleEvent(ProviderGpsEvent *ev, ProviderGpsStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return PROVIDER_GPS_STATE_WAIT_START; }
};

class ProviderGpsStateReqStart : public ProviderGpsState {
public:
	virtual int HandleEvent(ProviderGpsEvent *ev, ProviderGpsStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return PROVIDER_GPS_STATE_REQ_START; }
};

class ProviderGpsStateBusy : public ProviderGpsState {
public:
	virtual int HandleEvent(ProviderGpsEvent *ev, ProviderGpsStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return PROVIDER_GPS_STATE_STATE_BUSY; }
};

class ProviderGpsStateDataRecv : public ProviderGpsState {
public:
	virtual int HandleEvent(ProviderGpsEvent *ev, ProviderGpsStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return PROVIDER_GPS_STATE_DATA_RECEIVED; }
};

class ProviderGpsStateAlreadyRecv : public ProviderGpsState {
public:
	virtual int HandleEvent(ProviderGpsEvent *ev, ProviderGpsStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return PROVIDER_GPS_STATE_ALREADY_RECEIVED; }
};

class ProviderGpsStateReqStop : public ProviderGpsState {
public:
	virtual int HandleEvent(ProviderGpsEvent *ev, ProviderGpsStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return PROVIDER_GPS_STATE_REQ_STOP; }
};

static ProviderGpsStateNoDevice s_pgStateNoDevice;
static ProviderGpsStateWaitOpen s_pgStateWaitOpen;
static ProviderGpsStateWaitStart s_pgStateWaitStart;
static ProviderGpsStateReqStart s_pgStateReqStart;
static ProviderGpsStateBusy s_pgStateBusy;
static ProviderGpsStateDataRecv s_pgStateDataRecv;
static ProviderGpsStateAlreadyRecv s_pgStateAlreadyRecv;
static ProviderGpsStateReqStop s_pgStateReqStop;

static void notifyIfNotEmu(ProviderGpsController *ctrl, int arg)
{
	DriverManager *dev = locationDeviceGetInstance();
	if (ctrl->m_cbId >= 0 && dev->m_emuPower == 0)
		sceKernelNotifyCallback(ctrl->m_cbId, arg);
}

class ProviderGpsEventOpen : public ProviderGpsEvent {
public:
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x81196CF0 */
};

class ProviderGpsEventStart : public ProviderGpsEvent {
public:
	ProviderGpsEventStart(int mode) : m_mode(mode) {}
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x81196D60 */
	int m_mode;
};

class ProviderGpsEventStartResult : public ProviderGpsEvent {
public:
	ProviderGpsEventStartResult(int arg) : m_arg(arg) {}
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x81196E32 */
	int m_arg;
};

class ProviderGpsEventPassThrough : public ProviderGpsEvent {
public:
	ProviderGpsEventPassThrough(int arg) : m_arg(arg) {}
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x81196EA0 */
	int m_arg;
};

class ProviderGpsEventDataReady : public ProviderGpsEvent {
public:
	ProviderGpsEventDataReady(int arg) : m_arg(arg) {}
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x81196ECA */
	int m_arg;
};

class ProviderGpsEventStop : public ProviderGpsEvent {
public:
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x81196F00 */
};

class ProviderGpsEventStopSimple : public ProviderGpsEvent {
public:
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x81196F96 */
};

class ProviderGpsEventRestartOnStop : public ProviderGpsEvent {
public:
	ProviderGpsEventRestartOnStop(int arg) : m_arg(arg) {}
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x81196FFE */
	int m_arg;
};

class ProviderGpsEventStopResult : public ProviderGpsEvent {
public:
	ProviderGpsEventStopResult(int arg) : m_arg(arg) {}
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x81197046 */
	int m_arg;
};

class ProviderGpsEventCloseRaw : public ProviderGpsEvent {
public:
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x81197090 */
};

class ProviderGpsEventClose : public ProviderGpsEvent {
public:
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x8119709C */
};

class ProviderGpsEventStatusClosed : public ProviderGpsEvent {
public:
	ProviderGpsEventStatusClosed(int *out) : m_out(out) {}
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x811970BA */
	int *m_out;
};

class ProviderGpsEventStatusWaitStart : public ProviderGpsEvent {
public:
	ProviderGpsEventStatusWaitStart(int *out) : m_out(out) {}
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x811970C4 */
	int *m_out;
};

class ProviderGpsEventStatusReqStart : public ProviderGpsEvent {
public:
	ProviderGpsEventStatusReqStart(int *out) : m_out(out) {}
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x811970DA */
	int *m_out;
};

class ProviderGpsEventStatusStarted : public ProviderGpsEvent {
public:
	ProviderGpsEventStatusStarted(int *out) : m_out(out) {}
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x811970F0 */
	int *m_out;
};

class ProviderGpsEventStatusRaw : public ProviderGpsEvent {
public:
	ProviderGpsEventStatusRaw(int *out) : m_out(out) {}
	virtual int Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm); /* 0x81197108 */
	int *m_out;
};

/* 0x81196BB8 */
int ProviderGpsStateMachine::ChangeState(int nextState)
{
	switch (nextState) {
	case PROVIDER_GPS_STATE_NO_DEVICE:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(
				8, "Changing to PROVIDER_GPS_STATE_NO_DEVICE state\n");
		m_state = &s_pgStateNoDevice;
		break;
	case PROVIDER_GPS_STATE_WAIT_OPEN:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(
				8, "Changing to PROVIDER_GPS_STATE_WAIT_OPEN state\n");
		m_state = &s_pgStateWaitOpen;
		break;
	case PROVIDER_GPS_STATE_WAIT_START:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(
				8, "Changing to PROVIDER_GPS_STATE_WAIT_START state\n");
		m_state = &s_pgStateWaitStart;
		break;
	case PROVIDER_GPS_STATE_REQ_START:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(
				8, "Changing to PROVIDER_GPS_STATE_REQ_START state\n");
		m_state = &s_pgStateReqStart;
		break;
	case PROVIDER_GPS_STATE_STATE_BUSY:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(
				8, "Changing to PROVIDER_GPS_STATE_STATE_BUSY state\n");
		m_state = &s_pgStateBusy;
		break;
	case PROVIDER_GPS_STATE_DATA_RECEIVED:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(
				8, "Changing to PROVIDER_GPS_STATE_DATA_RECEIVED state\n");
		m_state = &s_pgStateDataRecv;
		break;
	case PROVIDER_GPS_STATE_ALREADY_RECEIVED:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(
				8, "Changing to PROVIDER_GPS_STATE_ALREADY_RECEIVED state\n");
		m_state = &s_pgStateAlreadyRecv;
		break;
	case PROVIDER_GPS_STATE_REQ_STOP:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(
				8, "Changing to PROVIDER_GPS_STATE_REQ_STOP state\n");
		m_state = &s_pgStateReqStop;
		break;
	default:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(8, "ProviderGpsStateMachine invalid NextState %d\n",
					      nextState);
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	}
	return 0;
}

/* 0x81196CE0 */
int ProviderGpsStateMachine::InitState()
{
	m_state = &s_pgStateWaitOpen;
	return 0;
}

/* 0x81196CF0 */
int ProviderGpsEventOpen::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	GpsCtrlManager *gpsCtrl = sm->m_ctrl->m_gpsCtrl;
	int ret = sceKernelLockLwMutex(&gpsCtrl->m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		switch (gpsCtrl->m_sm.m_state->GetStateId()) {
		case GPS_CTRL_STATE_CLOSED:
			ret = gpsCtrl->Open();
			if (ret == 0)
				sm->ChangeState(PROVIDER_GPS_STATE_WAIT_START);
			break;
		case GPS_CTRL_STATE_OPENED:
		case GPS_CTRL_STATE_START_REQ:
		case GPS_CTRL_STATE_STARTED:
			sm->ChangeState(PROVIDER_GPS_STATE_WAIT_START);
			break;
		case GPS_CTRL_STATE_STOP_REQ:
			ret = (int)0x8039000B;
			break;
		default:
			break;
		}
		int uret = sceKernelUnlockLwMutex(&gpsCtrl->m_sm.m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x81196D60 */
int ProviderGpsEventStart::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	ProviderGpsController *ctrl = sm->m_ctrl;
	GpsCtrlManager *gpsCtrl = ctrl->m_gpsCtrl;
	int ret = sceKernelLockLwMutex(&gpsCtrl->m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		switch (gpsCtrl->m_sm.m_state->GetStateId()) {
		case GPS_CTRL_STATE_CLOSED:
			ret = (int)0x80390004;
			sm->ChangeState(PROVIDER_GPS_STATE_WAIT_OPEN);
			break;
		case GPS_CTRL_STATE_OPENED:
			ret = gpsCtrl->Start(m_mode);
			if (ret == 0) {
				sm->ChangeState(PROVIDER_GPS_STATE_REQ_START);
				if (ctrl->m_resetListener != NULL)
					ctrl->m_resetListener->OnReset();
			}
			break;
		case GPS_CTRL_STATE_START_REQ:
		case GPS_CTRL_STATE_STOP_REQ:
			sm->ChangeState(PROVIDER_GPS_STATE_REQ_START);
			ret = 0;
			if (ctrl->m_resetListener != NULL)
				ctrl->m_resetListener->OnReset();
			break;
		case GPS_CTRL_STATE_STARTED:
			sm->ChangeState(PROVIDER_GPS_STATE_STATE_BUSY);
			ctrl->m_notifyTask.m_notifyArg = 0x10000;
			ret = ctrl->m_notifyTask.Schedule(1);
			if (ret == 0 && ctrl->m_resetListener != NULL)
				ctrl->m_resetListener->OnReset();
			break;
		default:
			break;
		}
		int uret = sceKernelUnlockLwMutex(&gpsCtrl->m_sm.m_mutex, 1);
		if (uret != 0)
			ret = uret;
	}
	return ret;
}

/* 0x81196E32 */
int ProviderGpsEventStartResult::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	ProviderGpsController *ctrl = sm->m_ctrl;
	int ret = sceKernelLockLwMutex(&ctrl->m_gpsCtrl->m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		if ((m_arg & 0xFF) != 0)
			sm->ChangeState(PROVIDER_GPS_STATE_WAIT_START);
		else
			sm->ChangeState(PROVIDER_GPS_STATE_STATE_BUSY);
		int uret = sceKernelUnlockLwMutex(&ctrl->m_gpsCtrl->m_sm.m_mutex, 1);
		if (uret != 0)
			ret = uret;
	}
	notifyIfNotEmu(ctrl, m_arg);
	return ret;
}

/* 0x81196EA0 */
int ProviderGpsEventPassThrough::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	notifyIfNotEmu(sm->m_ctrl, m_arg);
	return 0;
}

/* 0x81196ECA */
int ProviderGpsEventDataReady::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	sm->ChangeState(PROVIDER_GPS_STATE_DATA_RECEIVED);
	notifyIfNotEmu(sm->m_ctrl, m_arg);
	return 0;
}

/* 0x81196F00 */
int ProviderGpsEventStop::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	ProviderGpsController *ctrl = sm->m_ctrl;
	GpsCtrlManager *gpsCtrl = ctrl->m_gpsCtrl;
	int ret = sceKernelLockLwMutex(&gpsCtrl->m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		switch (gpsCtrl->m_sm.m_state->GetStateId()) {
		case GPS_CTRL_STATE_CLOSED:
		case GPS_CTRL_STATE_OPENED:
		case GPS_CTRL_STATE_START_REQ:
		case GPS_CTRL_STATE_STARTED:
			ret = gpsCtrl->Stop();
			if (ret == 0) {
				sm->ChangeState(PROVIDER_GPS_STATE_REQ_STOP);
			} else if (ret == (int)0x80390002) {
				sm->ChangeState(PROVIDER_GPS_STATE_WAIT_START);
				ctrl->m_notifyTask.m_notifyArg = 0x20000;
				ret = ctrl->m_notifyTask.Schedule(1);
			}
			break;
		case GPS_CTRL_STATE_STOP_REQ:
			sm->ChangeState(PROVIDER_GPS_STATE_REQ_STOP);
			ret = 0;
			break;
		default:
			break;
		}
		int uret = sceKernelUnlockLwMutex(&gpsCtrl->m_sm.m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x81196F96 */
int ProviderGpsEventStopSimple::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	GpsCtrlManager *gpsCtrl = sm->m_ctrl->m_gpsCtrl;
	int ret = sceKernelLockLwMutex(&gpsCtrl->m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		switch (gpsCtrl->m_sm.m_state->GetStateId()) {
		case GPS_CTRL_STATE_CLOSED:
		case GPS_CTRL_STATE_OPENED:
		case GPS_CTRL_STATE_START_REQ:
		case GPS_CTRL_STATE_STARTED:
			ret = gpsCtrl->Stop();
			if (ret == 0)
				sm->ChangeState(PROVIDER_GPS_STATE_REQ_STOP);
			break;
		case GPS_CTRL_STATE_STOP_REQ:
			sm->ChangeState(PROVIDER_GPS_STATE_REQ_STOP);
			ret = 0;
			break;
		default:
			break;
		}
		int uret = sceKernelUnlockLwMutex(&gpsCtrl->m_sm.m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x81196FFE */
int ProviderGpsEventRestartOnStop::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	ProviderGpsController *ctrl = sm->m_ctrl;
	if ((m_arg & 0xFF) == 0)
		return ctrl->m_gpsCtrl->Start(ctrl->m_startMode);
	sm->ChangeState(PROVIDER_GPS_STATE_WAIT_START);
	notifyIfNotEmu(ctrl, 0x10030);
	return 0;
}

/* 0x81197046 */
int ProviderGpsEventStopResult::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	sm->ChangeState(PROVIDER_GPS_STATE_WAIT_START);
	notifyIfNotEmu(sm->m_ctrl, m_arg);
	return 0;
}

/* 0x81197090 */
int ProviderGpsEventCloseRaw::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	return sm->m_ctrl->m_gpsCtrl->Close();
}

/* 0x8119709C */
int ProviderGpsEventClose::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	int ret = sm->m_ctrl->m_gpsCtrl->Close();
	if (ret == 0)
		sm->ChangeState(PROVIDER_GPS_STATE_WAIT_OPEN);
	return ret;
}

/* 0x811970BA */
int ProviderGpsEventStatusClosed::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	(void)sm;
	*m_out = 1;
	return 0;
}

/* 0x811970C4 */
int ProviderGpsEventStatusWaitStart::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	(void)sm;
	int ret = _sceGpsGetState(m_out);
	if (ret == 0)
		*m_out = 2;
	return ret;
}

/* 0x811970DA */
int ProviderGpsEventStatusReqStart::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	(void)sm;
	int ret = _sceGpsGetState(m_out);
	if (ret == 0)
		*m_out = 3;
	return ret;
}

/* 0x811970F0 */
int ProviderGpsEventStatusStarted::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	(void)sm;
	int ret = _sceGpsGetState(m_out);
	if (ret == 0)
		*m_out = 4;
	return ret;
}

/* 0x81197108 */
int ProviderGpsEventStatusRaw::Dispatch(ProviderGpsState *state, ProviderGpsStateMachine *sm)
{
	(void)state;
	(void)sm;
	return _sceGpsGetState(m_out);
}

/* 0x81197112 */
int ProviderGpsNotifyTask::OnNotify()
{
	notifyIfNotEmu(m_ctrl, m_notifyArg);
	return 0;
}

/* 0x81197138 */
int ProviderGpsController::Init()
{
	m_sm.m_mutexResult = sceKernelCreateLwMutex(&m_sm.m_mutex, "SceLocationCommon", 2, 0, NULL);
	int ret = m_sm.m_mutexResult;
	if (ret == 0)
		ret = m_sm.InitState();
	m_resetListener = NULL;
	if (ret == 0) {
		ret = m_gpsCtrl->AddListener(this);
		if (ret == 0)
			return g_locationManager->RegisterTask(&m_notifyTask);
	}
	return ret;
}

/* 0x8119719E */
int ProviderGpsController::Term()
{
	g_locationManager->UnregisterTask(&m_notifyTask);
	m_gpsCtrl->RemoveListener(this);

	int ret = 0;
	if (m_sm.m_mutexResult == 0) {
		ret = sceKernelDeleteLwMutex(&m_sm.m_mutex);
		if (ret == 0)
			m_sm.m_mutexResult = -1;
	}
	if (ret == 0)
		ret = m_sm.TermState();
	m_sm.m_state = NULL;
	return ret;
}

/* 0x811971EE */
int ProviderGpsController::Open(SceUID cbId)
{
	ProviderGpsEventOpen ev;
	int ret = sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		ret = m_sm.m_state->HandleEvent(&ev, &m_sm);
		int uret = sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		if (uret != 0)
			ret = uret;
		if (ret == 0)
			m_cbId = cbId;
	}
	return ret;
}

/* 0x8119725E */
int ProviderGpsController::Start(int mode)
{
	ProviderGpsEventStart ev(mode);
	int ret = sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		ret = m_sm.m_state->HandleEvent(&ev, &m_sm);
		int uret = sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		if (uret != 0)
			ret = uret;
	}
	m_startMode = mode;
	return ret;
}

/* 0x811972CE */
int ProviderGpsController::Stop()
{
	ProviderGpsEventStop ev;
	int ret = sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		ret = m_sm.m_state->HandleEvent(&ev, &m_sm);
		int uret = sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x81197330 */
int ProviderGpsController::Close()
{
	ProviderGpsEventClose ev;
	int ret = sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		ret = m_sm.m_state->HandleEvent(&ev, &m_sm);
		int uret = sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		if (uret != 0)
			ret = uret;
		if (ret == 0)
			m_cbId = -1;
	}
	return ret;
}

/* 0x8119739A */
int ProviderGpsController::GetData(GpsLocation *data, GpsSatellites *satellites)
{
	int ret = _sceGpsGetData(data, satellites);
	if (ret == 0) {
		ret = sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL);
		if (ret == 0) {
			m_sm.ChangeState(PROVIDER_GPS_STATE_DATA_RECEIVED);
			int uret = sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
			if (uret != 0)
				return uret;
		}
	}
	return ret;
}

/* 0x811973D8 */
int ProviderGpsController::GetStatus(int *outStatus)
{
	ProviderGpsEventStatusRaw ev(outStatus);
	int ret = sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		ret = m_sm.m_state->HandleEvent(&ev, &m_sm);
		int uret = sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x8119743E */
int ProviderGpsController::AddResetListener(ProviderGpsResetListener *listener)
{
	if (m_resetListener == NULL) {
		m_resetListener = listener;
		return 0;
	}
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* 0x8119746C */
void ProviderGpsController::OnStateChanged(int nextState)
{
	if (sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL) == 0) {
		if (nextState == 0)
			m_sm.ChangeState(PROVIDER_GPS_STATE_NO_DEVICE);
		sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
	}
}

/* 0x8119749A */
void ProviderGpsController::OnGpsEvent(int notifyId, int notifyCount, int notifyArg)
{
	(void)notifyId;
	(void)notifyCount;
	switch (notifyArg & 0xF0000) {
	case 0x40000: {
		ProviderGpsEventDataReady ev(notifyArg);
		if (sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL) == 0) {
			m_sm.m_state->HandleEvent(&ev, &m_sm);
			sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		}
		break;
	}
	case 0x30000: {
		ProviderGpsEventPassThrough ev(notifyArg);
		if (sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL) == 0) {
			m_sm.m_state->HandleEvent(&ev, &m_sm);
			sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		}
		break;
	}
	case 0x20000: {
		ProviderGpsEventStopResult ev(notifyArg);
		if (sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL) == 0) {
			m_sm.m_state->HandleEvent(&ev, &m_sm);
			sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		}
		break;
	}
	case 0x10000: {
		ProviderGpsEventStartResult ev(notifyArg);
		if (sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL) == 0) {
			m_sm.m_state->HandleEvent(&ev, &m_sm);
			sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		}
		break;
	}
	default:
		notifyIfNotEmu(this, notifyArg);
		break;
	}
}
