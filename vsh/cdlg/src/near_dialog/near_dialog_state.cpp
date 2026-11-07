/*
 * SceNearDialogPlugin's states: for each mode, the message dialog a state shows and what the
 * plugin is to do in it (Setup), the state after the dialog's button or the step the state waited
 * for (NextState), and the result the dialog ends with in a state that ends it (Result). The
 * update modes run states 1 to 42, the download mode 43 to 59; state 1 is the start. No Sony
 * record names the file, its functions or the states (the names are ours).
 */
#include <stddef.h>
#include <scetypes.h>
#include <near_dialog_util.h>
#include "near_dialog_plugin.h"

/* the dialog's buttons (CommonGuiDialogButton::value) */
#define NEAR_DIALOG_BUTTON_YES		2
#define NEAR_DIALOG_BUTTON_NO		3
#define NEAR_DIALOG_BUTTON_RETRY	6

/* what a state the dialog ends in gives when the user goes on to the near application */
#define NEAR_DIALOG_RESULT_LAUNCH_NEAR	1

/* the result a check-in or download start passed its callback: the near background service's
   states the dialog can go on from */
#define NEAR_DIALOG_START_READY			3
#define NEAR_DIALOG_START_NEED_UPDATE		6
#define NEAR_DIALOG_START_NEED_INITIALIZATION	7

/* the dialog's states (CommonDialogNearParam::state) and the system messages they show */
#define NEAR_DIALOG_STATE_START		1

/* the message dialog of a state (NearDialogMsgParam::mode) */
#define NEAR_DIALOG_MSG_MODE_SYSTEM	2
#define NEAR_DIALOG_MSG_MODE_ERROR_CODE	3
#define NEAR_DIALOG_MSG_MODE_PROGRESS	4

/* the result of the step the state waited for */
SceInt32 NearDialogGetStepResult(const CommonDialogNearParam *work, SceInt32 state)
{
	SceInt32 result = NEAR_DIALOG_STEP_PENDING;

	switch (state) {
	case 32:
		result = work->checkinStatus.stepResult[0];
		break;
	case 34:
		result = work->checkinStatus.stepResult[1];
		break;
	case 36:
	case 37:
		result = work->checkinStatus.stepResult[2];
		break;
	case 39:
		result = work->checkinStatus.stepResult[3];
		break;
	}
	return result;
}

static inline void ShowSysMsg(CommonDialogNearParam *work, SceInt32 type)
{
	work->msgParam.mode = NEAR_DIALOG_MSG_MODE_SYSTEM;
	work->msgParam.sysMsgType = type;
	work->flags |= NEAR_DIALOG_FLAG_OPEN;
}

static inline void ShowSysMsgStop(CommonDialogNearParam *work, SceInt32 type)
{
	work->msgParam.mode = NEAR_DIALOG_MSG_MODE_SYSTEM;
	work->msgParam.sysMsgType = type;
	work->flags = (work->flags | NEAR_DIALOG_FLAG_OPEN) & ~NEAR_DIALOG_FLAG_CHECKIN_STATUS;
}

static inline void ShowSysMsgCancel(CommonDialogNearParam *work, SceInt32 type)
{
	work->msgParam.mode = NEAR_DIALOG_MSG_MODE_SYSTEM;
	work->msgParam.sysMsgType = type;
	work->flags = (work->flags & ~NEAR_DIALOG_FLAG_CHECKIN_STATUS) |
		      NEAR_DIALOG_FLAG_CANCEL_CHECKIN | NEAR_DIALOG_FLAG_OPEN;
}

/* what an update state shows and does */
static int UpdateStateSetup(CommonDialogNearParam *work)
{
	int ret = SCE_OK;

	switch (work->state) {
	case NEAR_DIALOG_STATE_START:
		break;
	case 8:
		ShowSysMsg(work, 6);
		break;
	case 2:
		work->flags |= NEAR_DIALOG_FLAG_START_CHECKIN;
		break;
	case 31:
		ShowSysMsg(work, 29);
		break;
	case 41:
		work->msgParam.mode = NEAR_DIALOG_MSG_MODE_SYSTEM;
		work->msgParam.sysMsgType = 12;
		work->flags |= NEAR_DIALOG_FLAG_CANCEL_CHECKIN | NEAR_DIALOG_FLAG_OPEN;
		break;
	case 6:
		ShowSysMsgStop(work, 4);
		break;
	case 4:
		ShowSysMsg(work, 2);
		break;
	case 5:
		ShowSysMsg(work, 3);
		break;
	case 32:
		work->flags |= NEAR_DIALOG_FLAG_CHECKIN_STATUS;
		break;
	case 33:
		work->closeRequest = NEAR_DIALOG_CLOSE_REQUESTED;
		break;
	case 7:
		ShowSysMsgStop(work, 5);
		break;
	case 9:
		ShowSysMsgStop(work, 7);
		break;
	case 10:
		ShowSysMsgStop(work, 8);
		break;
	case 11:
		ShowSysMsgStop(work, 9);
		break;
	case 12:
		ShowSysMsgStop(work, 10);
		break;
	case 42:
		ShowSysMsgCancel(work, 12);
		break;
	case 34:
		ShowSysMsg(work, 11);
		break;
	case 35:
		work->closeRequest = NEAR_DIALOG_CLOSE_REQUESTED;
		break;
	case 14:
		ShowSysMsgStop(work, 13);
		break;
	case 15:
		ShowSysMsgStop(work, 14);
		break;
	case 16:
		ShowSysMsgStop(work, 15);
		break;
	case 17:
		ShowSysMsgStop(work, 16);
		break;
	case 19:
		ShowSysMsgStop(work, 19);
		break;
	case 13:
		ShowSysMsgCancel(work, 12);
		break;
	case 36:
		ShowSysMsg(work, 17);
		break;
	case 37:
		ShowSysMsg(work, 18);
		break;
	case 38:
		work->closeRequest = NEAR_DIALOG_CLOSE_REQUESTED;
		break;
	case 28:
		ShowSysMsgStop(work, 26);
		break;
	case 29:
		ShowSysMsgStop(work, 27);
		break;
	case 21:
		ShowSysMsgCancel(work, 12);
		break;
	case 23:
		work->userContentPosted = true;
		ShowSysMsg(work, 22);
		break;
	case 24:
		ShowSysMsg(work, 23);
		break;
	case 25:
		ShowSysMsg(work, 24);
		break;
	case 39:
		ShowSysMsg(work, 25);
		break;
	case 40:
		work->closeRequest = NEAR_DIALOG_CLOSE_REQUESTED;
		work->flags &= ~NEAR_DIALOG_FLAG_CHECKIN_STATUS;
		break;
	case 27:
		ShowSysMsg(work, 28);
		break;
	case 30:
		work->msgParam.mode = NEAR_DIALOG_MSG_MODE_ERROR_CODE;
		work->msgParam.sysMsgType = 0;
		work->msgParam.errorCode = work->errorCode;
		work->flags = (work->flags | NEAR_DIALOG_FLAG_OPEN) & ~NEAR_DIALOG_FLAG_CHECKIN_STATUS;
		break;
	default:
		ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
		break;
	}
	return ret;
}

/* the state after a check-in step that failed with the error */
static inline void UpdateStateError(CommonDialogNearParam *work, SceInt32 error)
{
	if (error == (SceInt32)SCE_NP_MANAGER_ERROR_NEED_CALL_NETCHECK_DIALOG) {
		work->state = 8;
	} else {
		work->errorCode = error;
		work->state = 30;
	}
}

/* the update state after the dialog's button or the step the state waited for */
static int UpdateNextState(CommonDialogNearParam *work, SceInt32 button)
{
	switch (work->state) {
	case NEAR_DIALOG_STATE_START:
		if (work->param.mode == SCE_NEAR_DIALOG_MODE_UPDATE) {
			work->retry = false;
			work->state = 2;
		} else if (work->param.mode == SCE_NEAR_DIALOG_MODE_UPDATE_RETRY) {
			work->retry = true;
			work->state = 2;
		} else {
			return NEAR_DIALOG_PLUGIN_ERROR_STATE;
		}
		break;
	case 2:
		work->state = 31;
		break;
	case 31:
		if (button == NEAR_DIALOG_BUTTON_NO) {
			work->state = 41;
			break;
		}
		if (work->jobState == NEAR_DIALOG_JOB_RUNNING) {
			work->state = 32;
			break;
		}
		switch (work->startResult) {
		case NEAR_DIALOG_START_NEED_UPDATE:
			work->state = 4;
			break;
		case NEAR_DIALOG_START_NEED_INITIALIZATION:
			work->state = 5;
			break;
		case NEAR_DIALOG_START_READY:
			work->state = 6;
			break;
		default:
			return SCE_NEAR_DIALOG_RESULT_BGPROC_DISCONNECTED;
		}
		break;
	case 32:
		if (button == NEAR_DIALOG_BUTTON_NO)
			work->state = 42;
		else
			work->state = 33;
		break;
	case 33:
		switch (work->stepResult) {
		case 0:
			work->state = 34;
			break;
		case 3:
		case 8:
			work->state = 7;
			break;
		case 6:
		case 7:
			work->state = 9;
			break;
		case 9:
			work->state = 10;
			break;
		case 10:
			work->state = 11;
			break;
		case 11:
			work->state = 12;
			break;
		default:
			UpdateStateError(work, work->checkinStatus.errorCode);
			break;
		}
		break;
	case 34:
		if (button == NEAR_DIALOG_BUTTON_NO)
			work->state = 13;
		else
			work->state = 35;
		break;
	case 35:
		switch (work->stepResult) {
		case 0:
			if (work->checkinStatus.postedBefore)
				work->state = 36;
			else
				work->state = 37;
			break;
		case 12:
			work->state = 14;
			break;
		case 14:
			work->state = 15;
			break;
		case 13:
			if (work->checkinStatus.postType == 1)
				work->state = 17;
			else
				work->state = 16;
			break;
		case 15:
		case 16:
			work->state = 19;
			break;
		default:
			UpdateStateError(work, work->checkinStatus.errorCode);
			break;
		}
		break;
	case 13:
		work->state = 19;
		break;
	case 16:
	case 17:
		if (button != NEAR_DIALOG_BUTTON_NO)
			return NEAR_DIALOG_PLUGIN_ERROR_STATE;
		work->state = 19;
		break;
	case 36:
	case 37:
		if (button == NEAR_DIALOG_BUTTON_NO)
			work->state = 21;
		else
			work->state = 38;
		break;
	case 38: {
		SceInt32 step = work->stepResult;
		SceInt32 error;

		if (step == 0) {
			work->state = 24;
			sce_paf_memcpy(&work->msgParam.finishInfo, &work->checkinStatus.finishInfo,
				       sizeof(work->msgParam.finishInfo));
			break;
		}
		if (step == 18) {
			work->state = 23;
			sce_paf_memcpy(&work->msgParam.finishInfo, &work->checkinStatus.finishInfo,
				       sizeof(work->msgParam.finishInfo));
			break;
		}
		if (step == 17)
			return SCE_NEAR_DIALOG_RESULT_SEND_CANCELED;
		error = work->checkinStatus.errorCode;
		switch (error) {
		case (SceInt32)0x8010450F:
		case (SceInt32)0x80104510:
		case (SceInt32)0x80104513:
		case (SceInt32)0x80106864:
		case (SceInt32)0x80106964:
			work->state = 28;
			break;
		case (SceInt32)0x80104511:
			work->state = 29;
			break;
		case (SceInt32)SCE_NP_MANAGER_ERROR_NEED_CALL_NETCHECK_DIALOG:
			work->state = 8;
			break;
		default:
			work->errorCode = error;
			work->state = 30;
			break;
		}
		break;
	}
	case 23:
		if (work->jobState == NEAR_DIALOG_JOB_RUNNING) {
			work->state = 24;
			break;
		}
		if (work->checkinStatus.stepResult[3] == 0)
			work->state = 27;
		else
			return SCE_NEAR_DIALOG_RESULT_SEND_OK;
		break;
	case 24:
		if (work->jobState == NEAR_DIALOG_JOB_RUNNING) {
			work->state = 25;
			break;
		}
		if (work->checkinStatus.stepResult[3] == 0)
			work->state = 27;
		else
			return SCE_NEAR_DIALOG_RESULT_SEND_OK;
		break;
	case 25:
		if (work->jobState == NEAR_DIALOG_JOB_RUNNING) {
			work->state = 39;
			break;
		}
		if (work->checkinStatus.stepResult[3] == 0)
			work->state = 27;
		else
			return SCE_NEAR_DIALOG_RESULT_SEND_OK;
		break;
	case 39:
		work->state = 40;
		break;
	case 40:
		if (work->stepResult == 0)
			work->state = 27;
		else
			return SCE_NEAR_DIALOG_RESULT_SEND_OK;
		break;
	default:
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	}
	return UpdateStateSetup(work);
}

/* what a download state shows and does */
static int DownloadStateSetup(CommonDialogNearParam *work)
{
	int ret = SCE_OK;

	switch (work->state) {
	case NEAR_DIALOG_STATE_START:
		break;
	case 43:
		work->flags |= NEAR_DIALOG_FLAG_START_DOWNLOAD;
		break;
	case 57:
		ShowSysMsg(work, 42);
		break;
	case 58:
		work->msgParam.mode = NEAR_DIALOG_MSG_MODE_SYSTEM;
		work->msgParam.sysMsgType = 43;
		work->flags |= NEAR_DIALOG_FLAG_CANCEL_DOWNLOAD | NEAR_DIALOG_FLAG_OPEN;
		break;
	case 46:
		ShowSysMsg(work, 33);
		break;
	case 44:
		ShowSysMsg(work, 30);
		break;
	case 45:
		ShowSysMsg(work, 3);
		break;
	case 47:
		ShowSysMsg(work, 32);
		break;
	case 48:
		work->msgParam.mode = NEAR_DIALOG_MSG_MODE_PROGRESS;
		work->msgParam.sysMsgType = 34;
		work->progressTarget = 0;
		work->progressRate = 0;
		work->flags |= NEAR_DIALOG_FLAG_DOWNLOAD_STATUS | NEAR_DIALOG_FLAG_OPEN;
		break;
	case 53:
		ShowSysMsg(work, 39);
		break;
	case 54:
		ShowSysMsg(work, 40);
		break;
	case 55:
		ShowSysMsg(work, 41);
		break;
	case 49:
		ShowSysMsg(work, 35);
		break;
	case 51:
		ShowSysMsg(work, 37);
		break;
	case 52:
		ShowSysMsg(work, 38);
		break;
	case 56:
		work->msgParam.mode = NEAR_DIALOG_MSG_MODE_ERROR_CODE;
		work->msgParam.sysMsgType = 0;
		work->msgParam.errorCode = work->errorCode;
		work->flags |= NEAR_DIALOG_FLAG_OPEN;
		break;
	case 59:
		work->flags = (work->flags & ~NEAR_DIALOG_FLAG_DOWNLOAD_STATUS) |
			      NEAR_DIALOG_FLAG_CANCEL_DOWNLOAD;
		break;
	default:
		ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
		break;
	}
	return ret;
}

/* the download state after the dialog's button or the step the state waited for */
static int DownloadNextState(CommonDialogNearParam *work, SceInt32 button)
{
	switch (work->state) {
	case 58:
		return SCE_OK;
	case 57:
		if (button == NEAR_DIALOG_BUTTON_NO) {
			work->state = 58;
			break;
		}
		if (work->jobState == NEAR_DIALOG_JOB_RUNNING) {
			work->state = 48;
			break;
		}
		switch (work->startResult) {
		case NEAR_DIALOG_START_NEED_UPDATE:
			work->state = 44;
			break;
		case NEAR_DIALOG_START_NEED_INITIALIZATION:
			work->state = 45;
			break;
		case NEAR_DIALOG_START_READY:
			work->state = 46;
			break;
		default:
			return SCE_NEAR_DIALOG_RESULT_BGPROC_DISCONNECTED;
		}
		break;
	case 48: {
		SceInt32 result;

		if (button == NEAR_DIALOG_BUTTON_NO) {
			work->state = 59;
			break;
		}
		result = work->downloadResult;
		switch (result) {
		case SCE_OK:
			work->state = 53;
			break;
		case (SceInt32)0x8010450D:
			work->state = 51;
			break;
		case (SceInt32)0x8010450F:
		case (SceInt32)0x80104510:
		case (SceInt32)0x80104513:
		case (SceInt32)0x80106864:
		case (SceInt32)0x80106964:
			work->state = 54;
			break;
		case (SceInt32)0x80104511:
			work->state = 55;
			break;
		case NEAR_SERVICE_ERROR_GIFT_DOWNLOADING:
			work->state = 46;
			break;
		case (SceInt32)0x8010451C:
			work->state = 52;
			break;
		case (SceInt32)0x80106974:
		case (SceInt32)0x80106975:
		case (SceInt32)0x80106976:
		case (SceInt32)0x80106977:
			work->state = 49;
			break;
		case (SceInt32)SCE_NP_MANAGER_ERROR_NEED_CALL_NETCHECK_DIALOG:
			work->state = 47;
			break;
		default:
			work->errorCode = result;
			work->state = 56;
			break;
		}
		break;
	}
	case 43:
		work->state = 57;
		break;
	case NEAR_DIALOG_STATE_START:
		if (work->param.mode != SCE_NEAR_DIALOG_MODE_DOWNLOAD)
			return NEAR_DIALOG_PLUGIN_ERROR_STATE;
		work->state = 43;
		break;
	default:
		return NEAR_DIALOG_PLUGIN_ERROR_STATE;
	}
	return DownloadStateSetup(work);
}

/* the state after the dialog's button or the step the state waited for, by the mode */
int NearDialogNextState(CommonDialogNearParam *work, SceInt32 button)
{
	switch (work->param.mode) {
	case SCE_NEAR_DIALOG_MODE_UPDATE:
	case SCE_NEAR_DIALOG_MODE_UPDATE_RETRY:
		return UpdateNextState(work, button);
	case SCE_NEAR_DIALOG_MODE_DOWNLOAD:
		return DownloadNextState(work, button);
	}
	return NEAR_DIALOG_PLUGIN_ERROR_INVALID;
}

/* the update state's result after the button: whether the dialog ends, and with which result */
static bool UpdateResult(CommonDialogNearParam *work, SceInt32 result, SceInt32 button,
			 SceInt32 *ret)
{
	switch (work->state) {
	case 8:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? SCE_NEAR_DIALOG_RESULT_NOT_SIGNIN :
							  NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 31:
		return false;
	case 41:
		*ret = NEAR_DIALOG_RESULT_LAUNCH_NEAR;
		return true;
	case 6:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? SCE_NEAR_DIALOG_RESULT_ALREADY_UPDATING :
							  NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 4:
		switch (button) {
		case NEAR_DIALOG_BUTTON_YES:
			*ret = SCE_NEAR_DIALOG_RESULT_LAUNCH_NEAR_NEED_INITIALIZATION;
			break;
		case NEAR_DIALOG_BUTTON_NO:
			*ret = NEAR_DIALOG_RESULT_LAUNCH_NEAR;
			break;
		default:
			*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
			break;
		}
		return true;
	case 5:
		*ret = button == NEAR_DIALOG_BUTTON_YES ?
			       SCE_NEAR_DIALOG_RESULT_MEMORY_CARD_NOT_INSERTED :
			       NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 33:
		return false;
	case 7:
		switch (button) {
		case NEAR_DIALOG_BUTTON_YES:
			*ret = SCE_NEAR_DIALOG_RESULT_LAUNCH_NEAR_NEED_USER_AGREEMENT;
			break;
		case NEAR_DIALOG_BUTTON_NO:
			*ret = NEAR_DIALOG_RESULT_LAUNCH_NEAR;
			break;
		default:
			*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
			break;
		}
		return true;
	case 9:
		switch (button) {
		case NEAR_DIALOG_BUTTON_YES:
			*ret = SCE_NEAR_DIALOG_RESULT_LAUNCH_NEAR_NEED_USER_AGREEMENT;
			break;
		case NEAR_DIALOG_BUTTON_NO:
			*ret = NEAR_DIALOG_RESULT_LAUNCH_NEAR;
			break;
		default:
			*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
			break;
		}
		return true;
	case 10:
		switch (button) {
		case NEAR_DIALOG_BUTTON_YES:
			*ret = SCE_NEAR_DIALOG_RESULT_LAUNCH_NEAR_NEED_UPDATE_PERMISSION;
			break;
		case NEAR_DIALOG_BUTTON_NO:
			*ret = NEAR_DIALOG_RESULT_LAUNCH_NEAR;
			break;
		default:
			*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
			break;
		}
		return true;
	case 11:
		switch (button) {
		case NEAR_DIALOG_BUTTON_YES:
			*ret = SCE_NEAR_DIALOG_RESULT_LAUNCH_NEAR_NEED_SHARED_USERID;
			break;
		case NEAR_DIALOG_BUTTON_NO:
			*ret = NEAR_DIALOG_RESULT_LAUNCH_NEAR;
			break;
		default:
			*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
			break;
		}
		return true;
	case 12:
		switch (button) {
		case NEAR_DIALOG_BUTTON_YES:
			*ret = SCE_NEAR_DIALOG_RESULT_LAUNCH_NEAR_NEED_SHARED_CONTENT;
			break;
		case NEAR_DIALOG_BUTTON_NO:
			*ret = NEAR_DIALOG_RESULT_LAUNCH_NEAR;
			break;
		default:
			*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
			break;
		}
		return true;
	case 32:
		return false;
	case 42:
		*ret = NEAR_DIALOG_RESULT_LAUNCH_NEAR;
		return true;
	case 34:
	case 35:
		return false;
	case 14:
		*ret = button == NEAR_DIALOG_BUTTON_YES ?
			       SCE_NEAR_DIALOG_RESULT_LOCATION_NOT_PERMITTED :
			       NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 15:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? SCE_NEAR_DIALOG_RESULT_WIFI_OFF :
							  NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 16:
		switch (button) {
		case NEAR_DIALOG_BUTTON_RETRY:
			*ret = SCE_NEAR_DIALOG_RESULT_UPDATE_RETRY;
			return true;
		case NEAR_DIALOG_BUTTON_NO:
			return false;
		}
		*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 17:
		switch (button) {
		case NEAR_DIALOG_BUTTON_RETRY:
			*ret = SCE_NEAR_DIALOG_RESULT_UPDATE_RETRY;
			return true;
		case NEAR_DIALOG_BUTTON_NO:
			return false;
		}
		*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 19:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? SCE_NEAR_DIALOG_RESULT_LOCATION_CANCELED :
							  NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 13:
	case 36:
	case 37:
	case 38:
		return false;
	case 21:
		*ret = SCE_NEAR_DIALOG_RESULT_SEND_CANCELED;
		return true;
	case 23:
		if (button == NEAR_DIALOG_BUTTON_YES)
			return false;
		*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 24:
		if (button == NEAR_DIALOG_BUTTON_YES)
			return false;
		*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 25:
		switch (button) {
		case NEAR_DIALOG_BUTTON_YES:
			return false;
		case NEAR_DIALOG_BUTTON_NO:
			*ret = SCE_NEAR_DIALOG_RESULT_SEND_OK;
			return true;
		}
		*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 39:
		*ret = button == NEAR_DIALOG_BUTTON_NO ? SCE_NEAR_DIALOG_RESULT_SEND_OK :
							 NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 40:
		return false;
	case 27:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? SCE_OK : NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 28:
		*ret = button == NEAR_DIALOG_BUTTON_YES ?
			       SCE_NEAR_DIALOG_RESULT_SERVER_MAINTENANCE :
			       NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 29:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? SCE_NEAR_DIALOG_RESULT_SERVER_END :
							  NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 30:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? work->msgParam.errorCode :
							  NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	}
	*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
	return true;
}

/* the download state's result after the button */
static __attribute__((noinline)) bool DownloadResult(CommonDialogNearParam *work,
						     SceInt32 result, SceInt32 button,
						     SceInt32 *ret)
{
	switch (work->state) {
	case 57:
		return false;
	case 58:
		*ret = NEAR_DIALOG_RESULT_LAUNCH_NEAR;
		return true;
	case 46:
		switch (button) {
		case NEAR_DIALOG_BUTTON_YES:
			*ret = SCE_NEAR_DIALOG_RESULT_LAUNCH_NEAR_NEED_GIFT_DOWNLOAD;
			break;
		case NEAR_DIALOG_BUTTON_NO:
			*ret = NEAR_DIALOG_RESULT_LAUNCH_NEAR;
			break;
		default:
			*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
			break;
		}
		return true;
	case 44:
		switch (button) {
		case NEAR_DIALOG_BUTTON_YES:
			*ret = SCE_NEAR_DIALOG_RESULT_LAUNCH_NEAR_NEED_INITIALIZATION;
			break;
		case NEAR_DIALOG_BUTTON_NO:
			*ret = NEAR_DIALOG_RESULT_LAUNCH_NEAR;
			break;
		default:
			*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
			break;
		}
		return true;
	case 45:
		*ret = button == NEAR_DIALOG_BUTTON_YES ?
			       SCE_NEAR_DIALOG_RESULT_MEMORY_CARD_NOT_INSERTED :
			       NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 47:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? SCE_NEAR_DIALOG_RESULT_NOT_SIGNIN :
							  NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 48:
		return false;
	case 53:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? SCE_OK : NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 54:
		*ret = button == NEAR_DIALOG_BUTTON_YES ?
			       SCE_NEAR_DIALOG_RESULT_SERVER_MAINTENANCE :
			       NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 55:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? SCE_NEAR_DIALOG_RESULT_SERVER_END :
							  NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 49:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? SCE_NEAR_DIALOG_RESULT_GIFT_NOT_FOUND :
							  NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 51:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? SCE_NEAR_DIALOG_RESULT_GIFT_CORRUPTED :
							  NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 52:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? SCE_NEAR_DIALOG_RESULT_GIFT_EXPIRED :
							  NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	case 56:
		*ret = button == NEAR_DIALOG_BUTTON_YES ? work->msgParam.errorCode :
							  NEAR_DIALOG_PLUGIN_ERROR_STATE;
		return true;
	}
	*ret = NEAR_DIALOG_PLUGIN_ERROR_STATE;
	return true;
}

/* whether the dialog ends after the button in the state, and with which result, by the mode */
bool NearDialogStateResult(CommonDialogNearParam *work, SceInt32 result, SceInt32 button,
			   SceInt32 *ret)
{
	switch (work->param.mode) {
	case SCE_NEAR_DIALOG_MODE_UPDATE:
	case SCE_NEAR_DIALOG_MODE_UPDATE_RETRY:
		return UpdateResult(work, result, button, ret);
	case SCE_NEAR_DIALOG_MODE_DOWNLOAD:
		return DownloadResult(work, result, button, ret);
	}
	*ret = NEAR_DIALOG_PLUGIN_ERROR_INVALID;
	return true;
}
