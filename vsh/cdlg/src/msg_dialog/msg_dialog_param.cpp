/*
 * SceMsgDialogPlugin's checks of the caller's parameter: each check that fails keeps its number
 * for MsgDialogPrintParamError, and the caller gets SCE_MSG_DIALOG_ERROR_PARAM. The message
 * printed is Sony's; the file's, the functions' and the numbers' names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <message_dialog.h>
#include "msg_dialog_plugin.h"

/* what failed, as MsgDialogPrintParamError prints it */
enum MsgDialogParamError {
	MSG_DIALOG_PARAM_OK			= 0,
	MSG_DIALOG_PARAM_SDK_VERSION		= 1,
	MSG_DIALOG_PARAM_MODE			= 2,
	MSG_DIALOG_PARAM_FLAG			= 3,
	MSG_DIALOG_PARAM_RESERVED		= 4,
	MSG_DIALOG_PARAM_USER_MSG_NULL		= 5,
	MSG_DIALOG_PARAM_USER_MSG_SET		= 6,
	MSG_DIALOG_PARAM_BUTTON_TYPE		= 7,
	MSG_DIALOG_PARAM_USER_MSG_TEXT_NULL	= 8,
	MSG_DIALOG_PARAM_USER_MSG_TEXT		= 9,
	MSG_DIALOG_PARAM_USER_MSG_RESERVED	= 10,
	MSG_DIALOG_PARAM_SYS_MSG_NULL		= 11,
	MSG_DIALOG_PARAM_SYS_MSG_SET		= 12,
	MSG_DIALOG_PARAM_SYS_MSG_TYPE		= 13,
	MSG_DIALOG_PARAM_SYS_MSG_VALUE		= 14,
	MSG_DIALOG_PARAM_SYS_MSG_RESERVED	= 15,
	MSG_DIALOG_PARAM_ERROR_CODE_NULL	= 16,
	MSG_DIALOG_PARAM_ERROR_CODE_SET		= 17,
	MSG_DIALOG_PARAM_ERROR_CODE_ZERO	= 18,
	MSG_DIALOG_PARAM_ERROR_CODE_RESERVED	= 19,
	MSG_DIALOG_PARAM_PROG_BAR_NULL		= 20,
	MSG_DIALOG_PARAM_PROG_BAR_SET		= 21,
	MSG_DIALOG_PARAM_PROG_BAR_TYPE		= 22,
	MSG_DIALOG_PARAM_PROG_BAR_SYS_MSG_TYPE	= 23,
	MSG_DIALOG_PARAM_PROG_BAR_TEXT_NULL	= 24,
	MSG_DIALOG_PARAM_PROG_BAR_TEXT_SET	= 25,
	MSG_DIALOG_PARAM_PROG_BAR_TEXT		= 26,
	MSG_DIALOG_PARAM_PROG_BAR_RESERVED	= 27,
	MSG_DIALOG_PARAM_BUTTONS_NULL		= 28,
	MSG_DIALOG_PARAM_BUTTONS_SET		= 29,
	MSG_DIALOG_PARAM_BUTTON1_TEXT		= 30,
	MSG_DIALOG_PARAM_BUTTON1_FONT_SIZE	= 31,
	MSG_DIALOG_PARAM_BUTTON2_TEXT		= 32,
	MSG_DIALOG_PARAM_BUTTON2_FONT_SIZE	= 33,
	MSG_DIALOG_PARAM_BUTTON3_TEXT		= 34,
	MSG_DIALOG_PARAM_BUTTON3_FONT_SIZE	= 35,
	MSG_DIALOG_PARAM_BUTTONS_RESERVED	= 36,
	MSG_DIALOG_PARAM_BUTTON1_TEXT_NULL	= 37,
	MSG_DIALOG_PARAM_BUTTON2_TEXT_NULL	= 38,
	MSG_DIALOG_PARAM_BUTTON3_TEXT_NULL	= 39,
	MSG_DIALOG_PARAM_SET_MSG_TARGET		= 101,
	MSG_DIALOG_PARAM_SET_MSG_TEXT		= 103,
};

/* a message's limits: its characters and its line breaks; a button's characters */
#define MSG_DIALOG_MSG_MAX_LENGTH	512
#define MSG_DIALOG_MSG_MAX_BREAKS	6
#define MSG_DIALOG_BUTTON_MAX_LENGTH	64

static int s_paramError;

/* a message under 512 characters with at most 6 line breaks */
static inline bool IsValidMsg(const SceChar8 *msg)
{
	SceUInt32 length = 0;
	int breaks = 0;

	if (msg[0] == '\0')
		return true;
	for (; msg[length] != '\0'; length++) {
		if (length >= MSG_DIALOG_MSG_MAX_LENGTH)
			break;
		if (msg[length] == '\n' || msg[length] == '\r')
			breaks++;
	}
	return length != MSG_DIALOG_MSG_MAX_LENGTH && breaks <= MSG_DIALOG_MSG_MAX_BREAKS;
}

/* a button's text: one line of 1 to 63 characters */
static inline bool IsValidButtonMsg(const SceChar8 *msg)
{
	SceUInt32 length = 0;

	while (*msg != '\0') {
		if (*msg == '\r' || *msg == '\n')
			return false;
		length++;
		msg++;
		if (length >= MSG_DIALOG_BUTTON_MAX_LENGTH)
			return false;
	}
	return length != 0;
}

static inline bool IsValidFontSize(SceMsgDialogFontSize fontSize)
{
	return fontSize == SCE_MSG_DIALOG_FONT_SIZE_DEFAULT || fontSize == SCE_MSG_DIALOG_FONT_SIZE_SMALL;
}

static inline bool IsZero(const SceChar8 *reserved, SceUInt32 size)
{
	SceUInt32 i;

	for (i = 0; i < size; i++) {
		if (reserved[i] != 0)
			return false;
	}
	return true;
}

void MsgDialogPrintParamError(void)
{
	sceClibPrintf("\n\n****** SceMsgDialog Parameter Error : %d ******\n\n", s_paramError);
}

static __attribute__((noinline)) int CheckSystemMessageParam(const SceMsgDialogSystemMessageParam *param)
{
	SceMsgDialogSystemMessageType type;

	if (param == NULL) {
		s_paramError = MSG_DIALOG_PARAM_SYS_MSG_NULL;
		return -1;
	}
	type = param->sysMsgType;
	if (type != SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_MAGNETIC_CALIBRATION &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_MIC_DISABLED &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_SMALL &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_CANCEL &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_NEED_MC_CONTINUE &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_NEED_MC_OPERATION &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_LOCATION_DATA_OBTAINING &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_LOCATION_DATA_FAILURE &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_LOCATION_DATA_FAILURE_RETRY &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_PATCH_FOUND &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_MIC_DISABLED_CONTINUABLE &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_MIC_DISABLED &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_OPERATION &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_WIFI_REQUIRED_APPLICATION &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_EMPTY_STORE &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_PSN_AGE_RESTRICTION &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_TRC_PSN_CHAT_RESTRICTION) {
		s_paramError = MSG_DIALOG_PARAM_SYS_MSG_TYPE;
		return -1;
	}
	/* only the space messages take a value */
	if (param->value != 0 && type != SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE &&
	    type != SCE_MSG_DIALOG_SYSMSG_TYPE_NOSPACE_CONTINUABLE) {
		s_paramError = MSG_DIALOG_PARAM_SYS_MSG_VALUE;
		return -1;
	}
	if (!IsZero(param->reserved, sizeof(param->reserved))) {
		s_paramError = MSG_DIALOG_PARAM_SYS_MSG_RESERVED;
		return -1;
	}
	return s_paramError != MSG_DIALOG_PARAM_OK ? -1 : 0;
}

static __attribute__((noinline)) int CheckProgressBarParam(const SceMsgDialogProgressBarParam *param)
{
	SceMsgDialogSystemMessageType type;

	if (param == NULL) {
		s_paramError = MSG_DIALOG_PARAM_PROG_BAR_NULL;
		goto out;
	}
	if (param->barType != SCE_MSG_DIALOG_PROGRESSBAR_TYPE_PERCENTAGE) {
		s_paramError = MSG_DIALOG_PARAM_PROG_BAR_TYPE;
		goto out;
	}
	type = param->sysMsgParam.sysMsgType;
	if (type == SCE_MSG_DIALOG_SYSMSG_TYPE_INVALID) {
		/* the caller's own text */
		if (param->msg == NULL) {
			s_paramError = MSG_DIALOG_PARAM_PROG_BAR_TEXT_NULL;
			goto out;
		}
		if (!IsValidMsg(param->msg)) {
			s_paramError = MSG_DIALOG_PARAM_PROG_BAR_TEXT;
			goto out;
		}
	} else {
		/* a waiting message */
		if (type != SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT &&
		    type != SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_SMALL &&
		    type != SCE_MSG_DIALOG_SYSMSG_TYPE_WAIT_CANCEL) {
			s_paramError = MSG_DIALOG_PARAM_PROG_BAR_SYS_MSG_TYPE;
			goto out;
		}
		if (param->msg != NULL) {
			s_paramError = MSG_DIALOG_PARAM_PROG_BAR_TEXT_SET;
			goto out;
		}
		if (CheckSystemMessageParam(&param->sysMsgParam) != 0)
			goto out;
	}
	if (!IsZero(param->reserved, sizeof(param->reserved)))
		s_paramError = MSG_DIALOG_PARAM_PROG_BAR_RESERVED;
out:
	return s_paramError != MSG_DIALOG_PARAM_OK ? -1 : 0;
}

static __attribute__((noinline)) int CheckButtonsParam(const SceMsgDialogButtonsParam *param)
{
	if (param == NULL) {
		s_paramError = MSG_DIALOG_PARAM_BUTTONS_NULL;
		goto out;
	}
	if (param->msg1 == NULL) {
		s_paramError = MSG_DIALOG_PARAM_BUTTON1_TEXT_NULL;
		goto out;
	}
	if (!IsValidButtonMsg(param->msg1)) {
		s_paramError = MSG_DIALOG_PARAM_BUTTON1_TEXT;
		goto out;
	}
	if (!IsValidFontSize(param->fontSize1)) {
		s_paramError = MSG_DIALOG_PARAM_BUTTON1_FONT_SIZE;
		goto out;
	}
	if (param->msg2 == NULL) {
		s_paramError = MSG_DIALOG_PARAM_BUTTON2_TEXT_NULL;
		goto out;
	}
	if (!IsValidButtonMsg(param->msg2)) {
		s_paramError = MSG_DIALOG_PARAM_BUTTON2_TEXT;
		goto out;
	}
	if (!IsValidFontSize(param->fontSize2)) {
		s_paramError = MSG_DIALOG_PARAM_BUTTON2_FONT_SIZE;
		goto out;
	}
	if (param->msg3 == NULL) {
		s_paramError = MSG_DIALOG_PARAM_BUTTON3_TEXT_NULL;
		goto out;
	}
	if (!IsValidButtonMsg(param->msg3)) {
		s_paramError = MSG_DIALOG_PARAM_BUTTON3_TEXT;
		goto out;
	}
	if (!IsValidFontSize(param->fontSize3)) {
		s_paramError = MSG_DIALOG_PARAM_BUTTON3_FONT_SIZE;
		goto out;
	}
	if (!IsZero(param->reserved, sizeof(param->reserved)))
		s_paramError = MSG_DIALOG_PARAM_BUTTONS_RESERVED;
out:
	return s_paramError != MSG_DIALOG_PARAM_OK ? -1 : 0;
}

static __attribute__((noinline)) int CheckUserMessageParam(const SceMsgDialogUserMessageParam *param)
{
	SceMsgDialogButtonType buttonType;

	if (param == NULL) {
		s_paramError = MSG_DIALOG_PARAM_USER_MSG_NULL;
		goto out;
	}
	buttonType = param->buttonType;
	if (buttonType != SCE_MSG_DIALOG_BUTTON_TYPE_OK &&
	    buttonType != SCE_MSG_DIALOG_BUTTON_TYPE_YESNO &&
	    buttonType != SCE_MSG_DIALOG_BUTTON_TYPE_NONE &&
	    buttonType != SCE_MSG_DIALOG_BUTTON_TYPE_OK_CANCEL &&
	    buttonType != SCE_MSG_DIALOG_BUTTON_TYPE_CANCEL &&
	    buttonType != SCE_MSG_DIALOG_BUTTON_TYPE_3BUTTONS) {
		s_paramError = MSG_DIALOG_PARAM_BUTTON_TYPE;
		goto out;
	}
	if (param->msg == NULL) {
		s_paramError = MSG_DIALOG_PARAM_USER_MSG_TEXT_NULL;
		goto out;
	}
	if (!IsValidMsg(param->msg)) {
		s_paramError = MSG_DIALOG_PARAM_USER_MSG_TEXT;
		goto out;
	}
	/* only the three buttons take their texts */
	if (buttonType == SCE_MSG_DIALOG_BUTTON_TYPE_3BUTTONS) {
		if (CheckButtonsParam(param->buttonParam) != 0)
			goto out;
	} else if (param->buttonParam != NULL) {
		s_paramError = MSG_DIALOG_PARAM_BUTTONS_SET;
		goto out;
	}
	if (!IsZero(param->reserved, sizeof(param->reserved)))
		s_paramError = MSG_DIALOG_PARAM_USER_MSG_RESERVED;
out:
	return s_paramError != MSG_DIALOG_PARAM_OK ? -1 : 0;
}

static inline int CheckErrorCodeParam(const SceMsgDialogErrorCodeParam *param)
{
	if (param == NULL) {
		s_paramError = MSG_DIALOG_PARAM_ERROR_CODE_NULL;
		goto out;
	}
	if (param->errorCode == 0) {
		s_paramError = MSG_DIALOG_PARAM_ERROR_CODE_ZERO;
		goto out;
	}
	if (!IsZero(param->reserved, sizeof(param->reserved)))
		s_paramError = MSG_DIALOG_PARAM_ERROR_CODE_RESERVED;
out:
	return s_paramError != MSG_DIALOG_PARAM_OK ? -1 : 0;
}

int MsgDialogParamCheck(const SceMsgDialogParam *param)
{
	int ret;

	s_paramError = MSG_DIALOG_PARAM_OK;
	ret = MsgDialogCheckSdkVersion(param->sdkVersion);
	if (ret < 0) {
		if (ret == MSG_DIALOG_PLUGIN_ERROR_SDK_VERSION_TOO_NEW)
			s_paramError = MSG_DIALOG_PARAM_SDK_VERSION;
		goto out;
	}

	/* the mode's parameter, and none of the others */
	switch (param->mode) {
	case SCE_MSG_DIALOG_MODE_USER_MSG:
		if (param->sysMsgParam != NULL) {
			s_paramError = MSG_DIALOG_PARAM_SYS_MSG_SET;
			goto out;
		}
		if (param->errorCodeParam != NULL) {
			s_paramError = MSG_DIALOG_PARAM_ERROR_CODE_SET;
			goto out;
		}
		if (param->progBarParam != NULL) {
			s_paramError = MSG_DIALOG_PARAM_PROG_BAR_SET;
			goto out;
		}
		if (CheckUserMessageParam(param->userMsgParam) != 0)
			goto out;
		break;
	case SCE_MSG_DIALOG_MODE_SYSTEM_MSG:
		if (param->userMsgParam != NULL) {
			s_paramError = MSG_DIALOG_PARAM_USER_MSG_SET;
			goto out;
		}
		if (param->errorCodeParam != NULL) {
			s_paramError = MSG_DIALOG_PARAM_ERROR_CODE_SET;
			goto out;
		}
		if (param->progBarParam != NULL) {
			s_paramError = MSG_DIALOG_PARAM_PROG_BAR_SET;
			goto out;
		}
		if (CheckSystemMessageParam(param->sysMsgParam) != 0)
			goto out;
		break;
	case SCE_MSG_DIALOG_MODE_ERROR_CODE:
		if (param->userMsgParam != NULL) {
			s_paramError = MSG_DIALOG_PARAM_USER_MSG_SET;
			goto out;
		}
		if (param->sysMsgParam != NULL) {
			s_paramError = MSG_DIALOG_PARAM_SYS_MSG_SET;
			goto out;
		}
		if (param->progBarParam != NULL) {
			s_paramError = MSG_DIALOG_PARAM_PROG_BAR_SET;
			goto out;
		}
		if (CheckErrorCodeParam(param->errorCodeParam) != 0)
			goto out;
		break;
	case SCE_MSG_DIALOG_MODE_PROGRESS_BAR:
		if (param->userMsgParam != NULL) {
			s_paramError = MSG_DIALOG_PARAM_USER_MSG_SET;
			goto out;
		}
		if (param->sysMsgParam != NULL) {
			s_paramError = MSG_DIALOG_PARAM_SYS_MSG_SET;
			goto out;
		}
		if (param->errorCodeParam != NULL) {
			s_paramError = MSG_DIALOG_PARAM_ERROR_CODE_SET;
			goto out;
		}
		if (CheckProgressBarParam(param->progBarParam) != 0)
			goto out;
		break;
	default:
		s_paramError = MSG_DIALOG_PARAM_MODE;
		goto out;
	}

	if (param->flag != 0)
		s_paramError = MSG_DIALOG_PARAM_FLAG;
	else if (!IsZero(param->reserved, sizeof(param->reserved)))
		s_paramError = MSG_DIALOG_PARAM_RESERVED;
out:
	if (s_paramError != MSG_DIALOG_PARAM_OK)
		return SCE_MSG_DIALOG_ERROR_PARAM;
	return ret;
}

/* the progress bar's message (sceMsgDialogProgressBarSetMsg): the default bar only */
int MsgDialogMsgCheck(SceMsgDialogProgressBarTarget target, const SceChar8 *msg)
{
	if (target != SCE_MSG_DIALOG_PROGRESSBAR_TARGET_BAR_DEFAULT)
		s_paramError = MSG_DIALOG_PARAM_SET_MSG_TARGET;
	else if (msg != NULL && !IsValidMsg(msg))
		s_paramError = MSG_DIALOG_PARAM_SET_MSG_TEXT;
	return s_paramError != MSG_DIALOG_PARAM_OK ? -1 : 0;
}
