/*
 * flying_start_sm.cpp: FlyingStartStateMachine (5-state GPS warm-start FSM triggered when a
 * location-enabled app launches/resumes; 0x81194684-0x81194E8C, sti 0x81194DDA).
 * Sony's class/state names ("FlyingStartStateMachine", "FLYING_START_STATE_*") come from its
 * log strings.
 */
#include "location_service.h"

extern void *g_locationManager;				/* 0x815496B4 */
extern void *g_locationLogger;				/* 0x815496B8 */
extern void locationLoggerPrintf(void *logger, int level, const char *fmt, ...);
extern int locationTaskSchedule(void *task, unsigned int delayMs); /* 0x8118A780 */
extern int locationTaskCancel(void *task);		/* 0x8118A800 */
extern int locationMgrRegisterTask(void *mgr, void *task); /* 0x8118AACE */
extern int locationMgrUnregisterTask(void *mgr, void *task); /* 0x8118AB62 */
extern int locationMgrReleaseActive(void *mgr);		/* 0x8118ABDC */
extern int locationMgrAcquireActive(void *mgr);		/* 0x8118AC32 */

extern int gpsCtrlCheckStatus(void *gpsCtrl);		/* 0x8119402A */
extern int gpsCtrlAddListener(void *gpsCtrl, void *listener); /* 0x81194294 */
extern int gpsCtrlRemoveListener(void *gpsCtrl, void *listener); /* 0x811942C4 */
extern int gpsCtrlOpen(void *gpsCtrl);			/* 0x811942F6 */
extern int gpsCtrlStart(void *gpsCtrl, int mode);	/* 0x8119435C */
extern int gpsCtrlStop(void *gpsCtrl);			/* 0x811943CE */
extern int gpsXpsAddListener(void *gpsXps, void *listener); /* 0x8119743E */

enum FlyingStartStateId {
	FLYING_START_STATE_IDLE = 0,
	FLYING_START_STATE_OPEN_REQUESTED = 1,
	FLYING_START_STATE_START_REQUESTED = 2,
	FLYING_START_STATE_STARTED = 3,
	FLYING_START_STATE_STOP_REQUESTED = 4
};

class FlyingStartStateMachine;
class FlyingStartEvent;

class FlyingStartState {
public:
	virtual int HandleEvent(FlyingStartEvent *ev, FlyingStartStateMachine *sm) = 0; /* +0 */
	virtual int GetStateId() = 0;				/* +4 */
	virtual ~FlyingStartState() {}
};

class FlyingStartEvent {
public:
	virtual int Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm) = 0;
	virtual ~FlyingStartEvent() {}
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

class FlyingStartController;

class FlyingStartStateMachine {
public:
	virtual ~FlyingStartStateMachine() {}
	virtual int ChangeState(int nextState);			/* 0x81194684 (+8) */
	virtual int InitState();				/* 0x8119475C (+12) */
	virtual int TermState() { return 0; }			/* (+16) */

	SceKernelLwMutexWork m_mutex;				/* +8 (+24 in ctrl) */
	int m_mutexResult;					/* +40 (+56 in ctrl) */
	FlyingStartState *m_state;				/* +44 (+60 in ctrl) */
	FlyingStartController *m_ctrl;				/* +48 (+64 in ctrl) */
};

class FlyingStartTimerTask {
public:
	virtual ~FlyingStartTimerTask() {}
	virtual int OnNotify();					/* 0x81194A74 */

	int m_queued;						/* +4 */
	int m_active;						/* +8 */
	FlyingStartStateMachine *m_sm;				/* +12 */
	int m_retryCount;					/* +16 (+88 in ctrl) */
};

class FlyingStartController {
public:
	virtual void OnGpsStateChanged(int nextState);		/* 0x81194C86 */
	virtual void OnGpsEvent(int notifyId, int notifyCount, int notifyArg); /* 0x81194CD2 */
	virtual void OnXpsReset();				/* 0x81194D82 */

	int Init();						/* 0x81194B56 */
	int Term();						/* 0x81194BB4 */
	int Trigger();						/* 0x81194C04 */

	void *m_xpsVtbl;					/* +4 */
	void *m_gpsCtrl;					/* +8 */
	void *m_gpsXps;						/* +12 */
	FlyingStartStateMachine m_sm;				/* +16..+67 */
	int m_pad68;						/* +68 */
	FlyingStartTimerTask m_timerTask;			/* +72..+91 */
	int m_pad92;						/* +92 */
	SceUInt64 m_deadline;					/* +96 */
};

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

static int gpsCtrlGetStateId(void *gpsCtrl)
{
	struct StateBase {
		virtual int HandleEvent(void *, void *);
		virtual int GetStateId();
	};
	StateBase *st = *(StateBase **)((char *)gpsCtrl + 60);
	return st->GetStateId();
}

/* 0x81194684 */
int FlyingStartStateMachine::ChangeState(int nextState)
{
	switch (nextState) {
	case FLYING_START_STATE_IDLE:
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 8,
					     "Changing to FLYING_START_STATE_IDLE state\n");
		m_state = &s_fsStateIdle;
		locationMgrReleaseActive(g_locationManager);
		break;
	case FLYING_START_STATE_OPEN_REQUESTED:
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 8,
					     "Changing to FLYING_START_STATE_OPEN_REQUESTED state\n");
		m_state = &s_fsStateOpenReq;
		break;
	case FLYING_START_STATE_START_REQUESTED:
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 8,
					     "Changing to FLYING_START_STATE_START_REQUESTED state\n");
		m_state = &s_fsStateStartReq;
		break;
	case FLYING_START_STATE_STARTED:
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 8,
					     "Changing to FLYING_START_STATE_STARTED state\n");
		m_state = &s_fsStateStarted;
		break;
	case FLYING_START_STATE_STOP_REQUESTED:
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 8,
					     "Changing to FLYING_START_STATE_STOP_REQUESTED state\n");
		m_state = &s_fsStateStopReq;
		break;
	default:
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 8,
					     "FlyingStartStateMachine invalid NextState %d\n",
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
	void *gpsCtrl = ctrl->m_gpsCtrl;
	SceKernelLwMutexWork *gpsMutex = (SceKernelLwMutexWork *)((char *)gpsCtrl + 24);
	int ret = sceKernelLockLwMutex(gpsMutex, 1, NULL);
	if (ret != 0)
		return ret;

	int gpsState = gpsCtrlGetStateId(gpsCtrl);
	if (gpsState == 0) {
		locationMgrAcquireActive(g_locationManager);
		ctrl->m_timerTask.m_retryCount = 3;
		ret = locationTaskSchedule(&ctrl->m_timerTask, 1);
		if (ret == 0)
			sm->ChangeState(FLYING_START_STATE_OPEN_REQUESTED);
	} else if (gpsState == 1) {
		locationMgrAcquireActive(g_locationManager);
		ret = gpsCtrlStart(gpsCtrl, 16);
		if (ret == 0) {
			sm->ChangeState(FLYING_START_STATE_START_REQUESTED);
		} else if (ret == (int)0x80390002 || ret == (int)0x8039000C) {
			ctrl->m_timerTask.m_retryCount = 3;
			ret = locationTaskSchedule(&ctrl->m_timerTask, 1);
			if (ret == 0)
				sm->ChangeState(FLYING_START_STATE_OPEN_REQUESTED);
		}
	}

	int uret = sceKernelUnlockLwMutex(gpsMutex, 1);
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
			locationLoggerPrintf(g_locationLogger, 8,
					     "FlyingStartGpsStartResult failed to start. retrying\n");
		sm->ChangeState(FLYING_START_STATE_OPEN_REQUESTED);
		ctrl->m_timerTask.m_retryCount = 3;
		int ret = locationTaskSchedule(&ctrl->m_timerTask, 1);
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
	return locationTaskSchedule(&ctrl->m_timerTask, delayMs);
}

/* 0x811948E8 */
int FlyingStartEventXpsReset::Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm)
{
	(void)state;
	int ret = locationTaskCancel(&sm->m_ctrl->m_timerTask);
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
	void *gpsCtrl = sm->m_ctrl->m_gpsCtrl;
	SceKernelLwMutexWork *gpsMutex = (SceKernelLwMutexWork *)((char *)gpsCtrl + 24);
	int ret = sceKernelLockLwMutex(gpsMutex, 1, NULL);
	if (ret != 0)
		return ret;

	int gpsState = gpsCtrlGetStateId(gpsCtrl);
	if (gpsState == 0) {
		ret = gpsCtrlOpen(gpsCtrl);
		if (ret == 0) {
			ret = gpsCtrlStart(gpsCtrl, 16);
			if (ret == 0)
				sm->ChangeState(FLYING_START_STATE_START_REQUESTED);
		}
	} else if (gpsState == 1) {
		ret = gpsCtrlStart(gpsCtrl, 16);
		if (ret == 0)
			sm->ChangeState(FLYING_START_STATE_START_REQUESTED);
	} else if (gpsState == 2) {
		sm->ChangeState(FLYING_START_STATE_START_REQUESTED);
	} else if (gpsState == 3) {
		sm->ChangeState(FLYING_START_STATE_STARTED);
	}

	int uret = sceKernelUnlockLwMutex(gpsMutex, 1);
	if (uret != 0)
		return uret;
	return ret;
}

/* 0x811949B4 */
int FlyingStartEventTimerStop::Dispatch(FlyingStartState *state, FlyingStartStateMachine *sm)
{
	(void)state;
	FlyingStartController *ctrl = sm->m_ctrl;
	void *gpsCtrl = ctrl->m_gpsCtrl;
	SceKernelLwMutexWork *gpsMutex = (SceKernelLwMutexWork *)((char *)gpsCtrl + 24);
	int ret = sceKernelLockLwMutex(gpsMutex, 1, NULL);
	if (ret == 0) {
		int gpsState = gpsCtrlGetStateId(gpsCtrl);
		SceUInt64 now = sceKernelGetProcessTimeWide();
		if (now <= ctrl->m_deadline)
			(void)((ctrl->m_deadline - now) / 1000U);
		if (gpsState == 3) {
			ret = gpsCtrlStop(gpsCtrl);
			if (ret == 0)
				sm->ChangeState(FLYING_START_STATE_STOP_REQUESTED);
		} else if (gpsState == 4) {
			sm->ChangeState(FLYING_START_STATE_STOP_REQUESTED);
		}
		int uret = sceKernelUnlockLwMutex(gpsMutex, 1);
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
				locationTaskSchedule(this, 500);
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
		ret = gpsCtrlAddListener(m_gpsCtrl, this);
		if (ret == 0) {
			ret = gpsXpsAddListener(m_gpsXps, &m_xpsVtbl);
			if (ret == 0)
				return locationMgrRegisterTask(g_locationManager, &m_timerTask);
		}
	}
	return ret;
}

/* 0x81194BB4 */
int FlyingStartController::Term()
{
	locationMgrUnregisterTask(g_locationManager, &m_timerTask);
	gpsCtrlRemoveListener(m_gpsCtrl, this);

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
	gpsCtrlCheckStatus(m_gpsCtrl);

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
void FlyingStartController::OnGpsStateChanged(int nextState)
{
	if (sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL) == 0) {
		if (nextState == 0 && m_sm.m_state->GetStateId() != FLYING_START_STATE_IDLE) {
			m_timerTask.m_retryCount = 3;
			if (locationTaskSchedule(&m_timerTask, 1) == 0)
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
void FlyingStartController::OnXpsReset()
{
	FlyingStartEventXpsReset ev;
	if (sceKernelLockLwMutex(&m_sm.m_mutex, 1, NULL) == 0) {
		m_sm.m_state->HandleEvent(&ev, &m_sm);
		sceKernelUnlockLwMutex(&m_sm.m_mutex, 1);
	}
}
