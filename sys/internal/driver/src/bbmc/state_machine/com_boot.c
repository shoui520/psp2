#include <sysmem/debug.h>
#include "../bbmc_control.h"
#include "../bbmc_message.h"
#include "../bbmc_memory.h"
#include "../bbcmd.h"

int bbmcSendSimpleMessage(BbmcControl *control, unsigned int command);
int bbmcFinalizeGps(void);

SceUInt32 comBootTransitionStep;
SceUInt32 comBootSuspendStep;

static const SceKernelDebugInfo dbg_suspendDuringFromComBootToStandby_L1664
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/com_boot.c",
	.line = 1664,
	.func = "suspendDuringFromComBootToStandby",
#endif
};

static const SceKernelDebugInfoSource
dbg_suspendDuringFromComBootToStandby_L1664Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_suspendDuringFromComBootToStandby_L1664,
	"state_machine/com_boot.c", 1664, "suspendDuringFromComBootToStandby",
};

static const SceKernelDebugInfo dbg_suspendDuringFromComBootToStandby_L1679
	__attribute__((section(".sce_dbginfo"), used)) = {
#if FW_DEBUG
	.file = "state_machine/com_boot.c",
	.line = 1679,
	.func = "suspendDuringFromComBootToStandby",
#endif
};

static const SceKernelDebugInfoSource
dbg_suspendDuringFromComBootToStandby_L1679Source
	__attribute__((section(SCE_DBGSRC_SECTION), used)) = {
	&dbg_suspendDuringFromComBootToStandby_L1679,
	"state_machine/com_boot.c", 1679, "suspendDuringFromComBootToStandby",
};

int suspendDuringFromComBootToStandby(BbmcControl *control,
				     BbmcMessage *message)
{
	BbcmdArgument input[7] = {0};
	SceUInt32 step = comBootSuspendStep;
	SceUInt32 suspendStep;
	int result;

	control->suspendError = (int)(step | 0x80010400);
	switch (step) {
	case 0:
	case 1:
		suspendStep = control->suspendStep;
		if (suspendStep > 2) {
			if (suspendStep == 3)
				goto requestZero;
			if (suspendStep == 5) {
				result = bbmcFinalizeGps();
				if (result < 0 && result != (int)0x80390005)
					SceDebugForDriver_391B5B74(
						&dbg_suspendDuringFromComBootToStandby_L1664,
						__builtin_return_address(0));
				input[0].value = 0;
				goto requestZero;
			}
			SceDebugForDriver_391B5B74(
				&dbg_suspendDuringFromComBootToStandby_L1679,
				__builtin_return_address(0));
		}
		switch (message->command) {
		case 43:
			comBootSuspendStep = 1;
			return 1;
		case 59:
			goto requestZero;
		case 58:
			goto requestSix;
		default:
			return 1;
		}
	case 2:
		if (message->command != 55)
			return 1;
		result = bbcmdExecute(0, 10, NULL, NULL,
				      bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
		comBootSuspendStep = 3;
		return 1;
	case 3:
		if (message->command != 56 || message->body.commandResult != 0)
			return 1;
requestSix:
		result = bbcmdExecute(0, 6, NULL, NULL,
				      bbcmdManagerCompletion, control);
		if (result < 0)
			goto finish;
		comBootSuspendStep = 4;
		return 1;
	case 4:
		if (message->command != 57)
			return 1;
		bbmcPrepareUsbShutdown(control);
		bbmcEnterCS(&control->mutex);
		control->targetState = 1;
		bbmcLeaveCS(&control->mutex);
		result = bbmcSendSimpleMessage(control, 42);
		goto finish;
	default:
		result = (int)0x80330001;
		goto finish;
	}

requestZero:
	result = bbcmdExecute(0, 0, input, NULL,
			      bbcmdManagerCompletion, control);
	if (result >= 0) {
		comBootSuspendStep = 2;
		return 1;
	}
	step = comBootSuspendStep;
	comBootTransitionStep = 5;
	if (step <= 4)
		return 1;
finish:
	control->suspendError = (int)0x80010405;
	if (result < 0) {
		SceUInt32 flags = control->flags;

		comBootSuspendStep = 0;
		control->flags = flags | 2;
		return 0;
	}
	comBootSuspendStep = 0;
	return 2;
}
