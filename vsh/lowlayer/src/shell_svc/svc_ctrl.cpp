#include <sce_atomic.h>
#include "svc_ctrl.h"

/*
 * SceSvcCtrl: one state word, moved between idle and one of two active states by
 * compare-and-swap. Each export fails with the state it found when that state is not the
 * one it moves from. No Sony name for the library's states or errors is known; the names
 * are ours.
 */

static int s_state = SVC_CTRL_STATE_IDLE;

/* active (1) -> idle */
int SceSvcCtrl_077F8AF1(void)
{
	if (sceAtomicCompareAndSwap32AcqRel(&s_state, SVC_CTRL_STATE_ACTIVE_1,
					    SVC_CTRL_STATE_IDLE) != SVC_CTRL_STATE_ACTIVE_1)
		return SVC_CTRL_ERROR_INVALID_STATE;
	return 0;
}

/* idle -> active (1); already active (1) is not an error */
int SceSvcCtrl_8BEB8B7C(void)
{
	int state;

	state = sceAtomicCompareAndSwap32AcqRel(&s_state, SVC_CTRL_STATE_IDLE,
						SVC_CTRL_STATE_ACTIVE_1);
	if (state == SVC_CTRL_STATE_IDLE)
		return 0;
	switch (state) {
	case SVC_CTRL_STATE_ACTIVE_1:
		return 0;
	case SVC_CTRL_STATE_ACTIVE_2:
		return SVC_CTRL_ERROR_ACTIVE_2;
	default:
		return SVC_CTRL_ERROR_UNKNOWN_STATE;
	}
}

/* idle -> active (2) */
int SceSvcCtrl_683998F2(void)
{
	int state;

	state = sceAtomicCompareAndSwap32AcqRel(&s_state, SVC_CTRL_STATE_IDLE,
						SVC_CTRL_STATE_ACTIVE_2);
	if (state == SVC_CTRL_STATE_IDLE)
		return 0;
	switch (state) {
	case SVC_CTRL_STATE_ACTIVE_1:
		return SVC_CTRL_ERROR_ACTIVE_1;
	case SVC_CTRL_STATE_ACTIVE_2:
		return SVC_CTRL_ERROR_INVALID_STATE;
	default:
		return SVC_CTRL_ERROR_UNKNOWN_STATE;
	}
}

/* active (2) -> idle */
int SceSvcCtrl_B21BE499(void)
{
	int state;

	state = sceAtomicCompareAndSwap32AcqRel(&s_state, SVC_CTRL_STATE_ACTIVE_2,
						SVC_CTRL_STATE_IDLE);
	if (state == SVC_CTRL_STATE_ACTIVE_2)
		return 0;
	switch (state) {
	case SVC_CTRL_STATE_ACTIVE_1:
	case SVC_CTRL_STATE_ACTIVE_2:
		return SVC_CTRL_ERROR_INVALID_STATE;
	default:
		return SVC_CTRL_ERROR_UNKNOWN_STATE;
	}
}
