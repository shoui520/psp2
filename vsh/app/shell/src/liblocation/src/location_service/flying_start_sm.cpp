/*
 * flying_start_sm.cpp: FlyingStartStateMachine (5-state GPS warm-start FSM triggered when a
 * location-enabled app launches/resumes; 0x81194684-0x81194E8C, sti 0x81194DDA).
 * Sony's class/state names ("FlyingStartStateMachine", "FLYING_START_STATE_*") come from its
 * log strings.
 */
#include "location_internal.h"

enum FlyingStartStateId {
	FLYING_START_STATE_IDLE = 0,
	FLYING_START_STATE_OPEN_REQUESTED = 1,
	FLYING_START_STATE_START_REQUESTED = 2,
	FLYING_START_STATE_STARTED = 3,
	FLYING_START_STATE_STOP_REQUESTED = 4
};

class FlyingStartStateIdle : public FlyingStartState {
public:
	virtual int HandleEvent(FlyingStartEvent *ev, FlyingStartStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return FLYING_START_STATE_IDLE; }
};

class FlyingStartStateOpenReq : public FlyingStartState {
public:
	virtual int HandleEvent(FlyingStartEvent *ev, FlyingStartStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return FLYING_START_STATE_OPEN_REQUESTED; }
};

class FlyingStartStateStartReq : public FlyingStartState {
public:
	virtual int HandleEvent(FlyingStartEvent *ev, FlyingStartStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return FLYING_START_STATE_START_REQUESTED; }
};

class FlyingStartStateStarted : public FlyingStartState {
public:
	virtual int HandleEvent(FlyingStartEvent *ev, FlyingStartStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return FLYING_START_STATE_STARTED; }
};

class FlyingStartStateStopReq : public FlyingStartState {
public:
	virtual int HandleEvent(FlyingStartEvent *ev, FlyingStartStateMachine *sm) { return ev->Dispatch(this, sm); }
	virtual int GetStateId() { return FLYING_START_STATE_STOP_REQUESTED; }
};

static FlyingStartStateIdle s_fsStateIdle;
static FlyingStartStateOpenReq s_fsStateOpenReq;
static FlyingStartStateStartReq s_fsStateStartReq;
static FlyingStartStateStarted s_fsStateStarted;
static FlyingStartStateStopReq s_fsStateStopReq;

class FlyingStartEventTrigger : public FlyingStartEvent {
public:
	virtual int Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm); /* 0x8119476A */
};

class FlyingStartEventGpsStartResult : public FlyingStartEvent {
public:
	FlyingStartEventGpsStartResult(int err) : m_err(err) {}
	virtual int Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm); /* 0x8119483C */
	int m_err;
};

class FlyingStartEventXpsReset : public FlyingStartEvent {
public:
	virtual int Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm); /* 0x811948E8 */
};

class FlyingStartEventStopDone : public FlyingStartEvent {
public:
	virtual int Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm); /* 0x81194906 */
};

class FlyingStartEventTimerOpen : public FlyingStartEvent {
public:
	FlyingStartEventTimerOpen(int *reschedule) : m_reschedule(reschedule) {}
	virtual int Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm); /* 0x81194914 */
	int *m_reschedule;
};

class FlyingStartEventTimerStop : public FlyingStartEvent {
public:
	FlyingStartEventTimerStop(int *reschedule) : m_reschedule(reschedule) {}
	virtual int Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm); /* 0x811949B4 */
	int *m_reschedule;
};

/* 0x81194684 */
int FlyingStartStateMachine::ChangeState(int nextState)
{
	switch (nextState) {
	case FLYING_START_STATE_IDLE:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(8, "Changing to FLYING_START_STATE_IDLE state\n");
		m_state = &s_fsStateIdle;
		g_locationManager->ReleaseActive();
		break;
	case FLYING_START_STATE_OPEN_REQUESTED:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(
				8, "Changing to FLYING_START_STATE_OPEN_REQUESTED state\n");
		m_state = &s_fsStateOpenReq;
		break;
	case FLYING_START_STATE_START_REQUESTED:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(
				8, "Changing to FLYING_START_STATE_START_REQUESTED state\n");
		m_state = &s_fsStateStartReq;
		break;
	case FLYING_START_STATE_STARTED:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(8, "Changing to FLYING_START_STATE_STARTED state\n");
		m_state = &s_fsStateStarted;
		break;
	case FLYING_START_STATE_STOP_REQUESTED:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(
				8, "Changing to FLYING_START_STATE_STOP_REQUESTED state\n");
		m_state = &s_fsStateStopReq;
		break;
	default:
		if (g_locationLogger != NULL)
			g_locationLogger->Log(8, "FlyingStartStateMachine invalid NextState %d\n",
					      nextState);
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	}
	return 0;
}

/* 0x8119475C */
int FlyingStartStateMachine::InitState()
{
	m_state = &s_fsStateIdle;
	return 0;
}

/* 0x8119476A */
int FlyingStartEventTrigger::Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm)
{
	(void)state;
	FlyingStartController *ctrl = sm->m_ctrl;
	GpsCtrlManager *gpsCtrl = ctrl->m_gpsCtrl;
	int ret = sceKernelLockLwMutex(&gpsCtrl->m_sm.m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	int gpsState = gpsCtrl->m_sm.m_state->GetStateId();
	if (gpsState == GPS_CTRL_STATE_CLOSED) {
		g_locationManager->AcquireActive();
		ctrl->m_timerTask.m_retryCount = 3;
		ret = ctrl->m_timerTask.Schedule(1);
		if (ret == 0)
			sm->ChangeState(FLYING_START_STATE_OPEN_REQUESTED);
	} else if (gpsState == GPS_CTRL_STATE_OPENED) {
		g_locationManager->AcquireActive();
		ret = gpsCtrl->Start(16);
		if (ret == 0) {
			sm->ChangeState(FLYING_START_STATE_START_REQUESTED);
		} else if (ret == (int)0x80390002 || ret == (int)0x8039000C) {
			ctrl->m_timerTask.m_retryCount = 3;
			ret = ctrl->m_timerTask.Schedule(1);
			if (ret == 0)
				sm->ChangeState(FLYING_START_STATE_OPEN_REQUESTED);
		}
	}

	int uret = sceKernelUnlockLwMutex(&gpsCtrl->m_sm.m_mutex, 1);
	if (uret != 0)
		return uret;
	return ret;
}

/* 0x8119483C */
int FlyingStartEventGpsStartResult::Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm)
{
	(void)state;
	FlyingStartController *ctrl = sm->m_ctrl;
	if (m_err != 0) {
		if (g_locationLogger != NULL)
			g_locationLogger->Log(
				8, "FlyingStartGpsStartResult failed to start. retrying\n");
		sm->ChangeState(FLYING_START_STATE_OPEN_REQUESTED);
		ctrl->m_timerTask.m_retryCount = 3;
		int ret = ctrl->m_timerTask.Schedule(1);
		if (ret != 0)
			sm->ChangeState(FLYING_START_STATE_IDLE);
		return ret;
	}

	sm->ChangeState(FLYING_START_STATE_STARTED);
	SceUInt64 now = sceKernelGetProcessTimeWide();
	unsigned int delayMs = 0;
	if (now < ctrl->m_deadline)
		delayMs = (unsigned int)((ctrl->m_deadline - now) / 1000U);
	ctrl->m_timerTask.m_retryCount = 0;
	return ctrl->m_timerTask.Schedule(delayMs);
}

/* 0x811948E8 */
int FlyingStartEventXpsReset::Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm)
{
	(void)state;
	int ret = sm->m_ctrl->m_timerTask.Cancel();
	if (ret == 0)
		sm->ChangeState(FLYING_START_STATE_IDLE);
	return ret;
}

/* 0x81194906 */
int FlyingStartEventStopDone::Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm)
{
	(void)state;
	return sm->ChangeState(FLYING_START_STATE_IDLE);
}

/* 0x81194914 */
int FlyingStartEventTimerOpen::Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm)
{
	(void)state;
	GpsCtrlManager *gpsCtrl = sm->m_ctrl->m_gpsCtrl;
	int ret = sceKernelLockLwMutex(&gpsCtrl->m_sm.m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	int gpsState = gpsCtrl->m_sm.m_state->GetStateId();
	if (gpsState == GPS_CTRL_STATE_CLOSED) {
		ret = gpsCtrl->Open();
		if (ret == 0) {
			ret = gpsCtrl->Start(16);
			if (ret == 0)
				sm->ChangeState(FLYING_START_STATE_START_REQUESTED);
		}
	} else if (gpsState == GPS_CTRL_STATE_OPENED) {
		ret = gpsCtrl->Start(16);
		if (ret == 0)
			sm->ChangeState(FLYING_START_STATE_START_REQUESTED);
	} else if (gpsState == GPS_CTRL_STATE_START_REQ) {
		sm->ChangeState(FLYING_START_STATE_START_REQUESTED);
	} else if (gpsState == GPS_CTRL_STATE_STARTED) {
		sm->ChangeState(FLYING_START_STATE_STARTED);
	}

	int uret = sceKernelUnlockLwMutex(&gpsCtrl->m_sm.m_mutex, 1);
	if (uret != 0)
		return uret;
	return ret;
}

/* 0x811949B4 */
int FlyingStartEventTimerStop::Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm)
{
	(void)state;
	FlyingStartController *ctrl = sm->m_ctrl;
	GpsCtrlManager *gpsCtrl = ctrl->m_gpsCtrl;
	int ret = sceKernelLockLwMutex(&gpsCtrl->m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		int gpsState = gpsCtrl->m_sm.m_state->GetStateId();
		SceUInt64 now = sceKernelGetProcessTimeWide();
		if (now <= ctrl->m_deadline)
			(void)((ctrl->m_deadline - now) / 1000U);
		if (gpsState == GPS_CTRL_STATE_STARTED) {
			ret = gpsCtrl->Stop();
			if (ret == 0)
				sm->ChangeState(FLYING_START_STATE_STOP_REQUESTED);
		} else if (gpsState == GPS_CTRL_STATE_STOP_REQ) {
			sm->ChangeState(FLYING_START_STATE_STOP_REQUESTED);
		}
		int uret = sceKernelUnlockLwMutex(&gpsCtrl->m_sm.m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x81194A74 */
int FlyingStartTimerTask::OnNotify()
{
	int reschedule = 0;
	if (sceKernelLockLwMutex(&m_sm->m_mutex, 1, NULL) == 0) {
		FlyingStartEventTimerOpen ev(&reschedule);
		int ret = sceKernelLockLwMutex(&m_sm->m_mutex, 1, NULL);
		if (ret == 0) {
			ret = m_sm->m_state->HandleEvent(&ev, m_sm);
			int uret = sceKernelUnlockLwMutex(&m_sm->m_mutex, 1);
			if (uret != 0)
				ret = uret;
		}
		if (ret != 0) {
			if (ret == (int)0x80390002 || ret == (int)0x8039000C) {
				Schedule(500);
				reschedule = 1;
			} else if (m_retryCount <= 0) {
				m_sm->ChangeState(FLYING_START_STATE_IDLE);
				reschedule = 0;
			} else {
				reschedule = 1;
				m_retryCount--;
			}
		}
		sceKernelUnlockLwMutex(&m_sm->m_mutex, 1);
	}
	return reschedule;
}

/* 0x81194B56 */
int FlyingStartController::Init()
{
	m_sm.m_mutexResult = sceKernelCreateLwMutex(&m_sm.m_mutex, "SceLocationCommon", 2, 0, NULL);
	int ret = m_sm.m_mutexResult;
	if (ret == 0)
		ret = m_sm.InitState();
	if (ret == 0) {
		ret = m_gpsCtrl->AddListener(this);
		if (ret == 0) {
			ret = m_gpsXps->AddResetListener(this);
			if (ret == 0)
				return g_locationManager->RegisterTask(&m_timerTask);
		}
	}
	return ret;
}

/* 0x81194BB4 */
int FlyingStartController::Term()
{
	g_locationManager->UnregisterTask(&m_timerTask);
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

/* 0x81194C04 */
int FlyingStartController::Trigger()
{
	m_deadline = sceKernelGetProcessTimeWide() + 120000000ULL;
	m_gpsCtrl->CheckStatus();

	FlyingStartEventTrigger ev;
	int ret = sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL);
	if (ret == 0) {
		ret = m_sm.m_state->HandleEvent(&ev, &m_sm);
		int uret = sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		if (uret != 0)
			return uret;
	}
	return ret;
}

/* 0x81194C86 */
void FlyingStartController::OnStateChanged(int nextState)
{
	if (sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL) == 0) {
		if (nextState == 0 && m_sm.m_state->GetStateId() != FLYING_START_STATE_IDLE) {
			m_timerTask.m_retryCount = 3;
			if (m_timerTask.Schedule(1) == 0)
				m_sm.ChangeState(FLYING_START_STATE_OPEN_REQUESTED);
		}
		sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
	}
}

/* 0x81194CD2 */
void FlyingStartController::OnGpsEvent(int notifyId, int notifyCount, int notifyArg)
{
	(void)notifyId;
	(void)notifyCount;
	if ((notifyArg & 0xF0000) == 0x10000) {
		FlyingStartEventGpsStartResult ev(notifyArg & 0xFF);
		if (sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL) == 0) {
			m_sm.m_state->HandleEvent(&ev, &m_sm);
			sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		}
	} else if ((notifyArg & 0xF0000) == 0x20000 && (notifyArg & 0xFF) == 0) {
		FlyingStartEventStopDone ev;
		if (sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL) == 0) {
			m_sm.m_state->HandleEvent(&ev, &m_sm);
			sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
		}
	}
}

/* 0x81194D82 */
void FlyingStartController::OnReset()
{
	FlyingStartEventXpsReset ev;
	if (sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL) == 0) {
		m_sm.m_state->HandleEvent(&ev, &m_sm);
		sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
	}
}
