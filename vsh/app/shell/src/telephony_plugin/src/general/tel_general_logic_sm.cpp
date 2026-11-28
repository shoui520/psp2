/*
 * tel_general_logic_sm.cpp: SMC-generated state machine for
 * TelGeneralLogicMediator (15 states: BbmcEventGroup, EventGroup, Shutdown,
 * DefaultFunctions, DefaultSetForBooting, Error, Fatal, Initial, Booting,
 * DefaultSetForBooted, DefaultSetForInitializing, Initializing, Booted,
 * DefaultSetForIdle, Idle) (3.65-CEX: 0x8125F4B0..0x81260037).
 */
#include "tel_general_internal.h"

namespace telephony {

/* ------------------------------------------------------------------
 * Base state default forwarding -> Default(context)
 * ------------------------------------------------------------------ */

void TelGeneralLogicStateMachineState::AbortNetworkScan(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::ChangePinCode(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::CheckImsiForOperatorCode(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::CheckSimCondition(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::DeactivateSimLock(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetFlightMode(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetApnInfoListCount(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetApnInfoListItem(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetConditionError(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetGeneralLogicState(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetIccid(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetMccMncOfImsi(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetMccMncOfNetwork(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetNetRegistrationInfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetNetworkInfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetNetworkScanInfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetNetworkScanInfoCount(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetOperatorProfile(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetOperatorProfileCount(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetPhoneNumber(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetPinProtection(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetPinRemain(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetPinStatus(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetRoamingInfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetSignalLevel(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetSimLockDeactivateRemain(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::GetSimLockUnblockRemain(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::IsOpenSimModel(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventAttributeError(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcChangePinCodeFailed(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcChangePinCodeFin(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcComBootNg(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcComBootOk(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcComDownAtSuspend(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcComFailure(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcEnterLowBatt(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcExceedHiTemp(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcExitLowBatt(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcFallDownLowTemp(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcLpmFromNetworkReady(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcNeedUpdatingDeviceInfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcNetProfileSettingFailed(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcNetProfileSettingFin(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcNetRegisterSettingFailed(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcNetRegisterSettingFin(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcNetScanFailed(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcNetScanFin(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcNetinfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcNetworkReady(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcPacketCallEndReasonUpdate(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcPinNotRequired(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcPinPermanentlyBlocked(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcPinProtectionFailed(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcPinProtectionFin(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcPinRequired(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcPinStatusReady(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcPlmnModeBit(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcPukRequired(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcRegistrationReject(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcSetupReady(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcSimBlockedNormalOperation(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcSimLockInfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcSimPermanentlyBlocked(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventBbmcStatusUpdate(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorComFailure(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalBbmcError(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalCanNotUseSim(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalChangePinCode(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalDeactivateSimLock(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalDeactivateTelReg(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalEnableWwan(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalExitFromErrorState(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalLivespaceReady(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalReTurnOn(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalRemoveCurrentApnInfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalRestoreApnInfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalResume(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalSetApnInfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalSetApnInfoWithBillingPlan(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalSetDataRoamingPermission(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalSetPinProtection(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalShutdown(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalSimLockError(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalStartNetworkScan(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalUnblockPin(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalUnblockSimLock(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventMediatorInternalVerifyPin(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventNotifyCallback(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventSimDisabled(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::OnEventSimEnabled(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::RemoveCurrentApnInfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::RestoreApnInfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::SetApnInfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::SetDataRoamingDefinition(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::SetDataRoamingPermission(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::SetFlightMode(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::SetNetRegisterAuto(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::SetNetRegisterInfo(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::SetPinProtection(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::StartNetworkScan(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::TurnOn(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::UnblockPin(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::UnblockSimLock(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::VerifyPin(TelGeneralLogicStateMachineContext &context) { Default(context); }
void TelGeneralLogicStateMachineState::Default(TelGeneralLogicStateMachineContext &context) { (void)context; }

/* Forward declarations of the 15 SMC state singletons */
namespace GeneralLogicSm {
class BbmcEventGroupState;
class EventGroupState;
class ShutdownState;
class DefaultFunctionsState;
class DefaultSetForBootingState;
class ErrorState;
class FatalState;
class InitialState;
class BootingState;
class DefaultSetForBootedState;
class DefaultSetForInitializingState;
class InitializingState;
class BootedState;
class DefaultSetForIdleState;
class IdleState;

extern BbmcEventGroupState BbmcEventGroup;
extern EventGroupState EventGroup;
extern ShutdownState Shutdown;
extern DefaultFunctionsState DefaultFunctions;
extern DefaultSetForBootingState DefaultSetForBooting;
extern ErrorState Error;
extern FatalState Fatal;
extern InitialState Initial;
extern BootingState Booting;
extern DefaultSetForBootedState DefaultSetForBooted;
extern DefaultSetForInitializingState DefaultSetForInitializing;
extern InitializingState Initializing;
extern BootedState Booted;
extern DefaultSetForIdleState DefaultSetForIdle;
extern IdleState Idle;

/* ------------------------------------------------------------------
 * State 0: BbmcEventGroup (0x81537CB8)
 * ------------------------------------------------------------------ */
class BbmcEventGroupState : public TelGeneralLogicStateMachineState {
public:
	BbmcEventGroupState(const char *name, int stateId)
		: TelGeneralLogicStateMachineState(name, stateId)
	{
	}

	virtual void OnEventBbmcChangePinCodeFailed(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcForwardEvent();
	}
	virtual void OnEventBbmcChangePinCodeFin(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcForwardEvent();
	}
	virtual void OnEventBbmcComDownAtSuspend(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventBbmcNeedUpdatingDeviceInfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcNeedUpdatingDeviceInfo();
	}
	virtual void OnEventBbmcNetProfileSettingFailed(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcForwardEvent();
	}
	virtual void OnEventBbmcNetProfileSettingFin(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcForwardEvent();
	}
	virtual void OnEventBbmcNetRegisterSettingFailed(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcForwardEvent();
	}
	virtual void OnEventBbmcNetRegisterSettingFin(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcForwardEvent();
	}
	virtual void OnEventBbmcNetScanFailed(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcNetScanFailed();
	}
	virtual void OnEventBbmcNetScanFin(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcNetScanFin();
	}
	virtual void OnEventBbmcNetinfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcNetinfo();
	}
	virtual void OnEventBbmcNetworkReady(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcNetworkReady();
	}
	virtual void OnEventBbmcPinNotRequired(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcPinNotRequired();
	}
	virtual void OnEventBbmcPinPermanentlyBlocked(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcPinPermanentlyBlocked();
	}
	virtual void OnEventBbmcPinProtectionFailed(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcForwardEvent();
	}
	virtual void OnEventBbmcPinProtectionFin(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcForwardEvent();
	}
	virtual void OnEventBbmcPinRequired(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcPinRequired();
	}
	virtual void OnEventBbmcPinStatusReady(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcPinStatusReady();
	}
	virtual void OnEventBbmcPlmnModeBit(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcPlmnModeBit();
	}
	virtual void OnEventBbmcPukRequired(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcPukRequired();
	}
	virtual void OnEventBbmcRegistrationReject(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcRegistrationReject();
	}
	virtual void OnEventBbmcSetupReady(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcSetupReady();
	}
	virtual void OnEventBbmcSimLockInfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcSimLockInfo();
	}
	virtual void OnEventBbmcStatusUpdate(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcStatusUpdate();
	}
	virtual void OnEventNotifyCallback(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcForwardEvent();
	}
	virtual void OnEventSimEnabled(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcSimEnabled();
	}
};

/* ------------------------------------------------------------------
 * State 9: EventGroup (0x81537E74)
 * ------------------------------------------------------------------ */
class EventGroupState : public BbmcEventGroupState {
public:
	EventGroupState(const char *name, int stateId)
		: BbmcEventGroupState(name, stateId)
	{
	}

	virtual void OnEventMediatorInternalChangePinCode(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalChangePinCode();
	}
	virtual void OnEventMediatorInternalDeactivateSimLock(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalDeactivateSimLock();
	}
	virtual void OnEventMediatorInternalDeactivateTelReg(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalDeactivateTelReg();
	}
	virtual void OnEventMediatorInternalEnableWwan(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalEnableWwan();
	}
	virtual void OnEventMediatorInternalExitFromErrorState(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalExitFromErrorState();
	}
	virtual void OnEventMediatorInternalLivespaceReady(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalLivespaceReady();
	}
	virtual void OnEventMediatorInternalReTurnOn(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventMediatorInternalRemoveCurrentApnInfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalRemoveCurrentApnInfo();
	}
	virtual void OnEventMediatorInternalRestoreApnInfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalRestoreApnInfo();
	}
	virtual void OnEventMediatorInternalResume(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalResume();
	}
	virtual void OnEventMediatorInternalSetApnInfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalSetApnInfo();
	}
	virtual void OnEventMediatorInternalSetApnInfoWithBillingPlan(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalSetApnInfoWithBillingPlan();
	}
	virtual void OnEventMediatorInternalSetDataRoamingPermission(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalSetDataRoamingPermission();
	}
	virtual void OnEventMediatorInternalSetPinProtection(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalSetPinProtection();
	}
	virtual void OnEventMediatorInternalStartNetworkScan(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalStartNetworkScan();
	}
	virtual void OnEventMediatorInternalUnblockPin(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalUnblockPin();
	}
	virtual void OnEventMediatorInternalUnblockSimLock(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalUnblockSimLock();
	}
	virtual void OnEventMediatorInternalVerifyPin(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleMediatorInternalVerifyPin();
	}
};

/* ------------------------------------------------------------------
 * State 14: Shutdown (0x81538030)
 * ------------------------------------------------------------------ */
class ShutdownState : public TelGeneralLogicStateMachineState {
public:
	ShutdownState(const char *name, int stateId)
		: TelGeneralLogicStateMachineState(name, stateId)
	{
	}

	virtual void Entry(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().EnterStateCode(TEL_SM_STATE_SHUTDOWN);
	}
	virtual void CheckSimCondition(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionCheckSimCondition();
	}
	virtual void GetConditionError(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetConditionError();
	}
	virtual void GetGeneralLogicState(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetGeneralLogicState();
	}
};

/* ------------------------------------------------------------------
 * State 3: DefaultFunctions (0x815381EC)
 * ------------------------------------------------------------------ */
class DefaultFunctionsState : public EventGroupState {
public:
	DefaultFunctionsState(const char *name, int stateId)
		: EventGroupState(name, stateId)
	{
	}

	virtual void DeactivateSimLock(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionDeactivateSimLock();
	}
	virtual void GetFlightMode(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetFlightMode();
	}
	virtual void GetConditionError(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetConditionError();
	}
	virtual void GetGeneralLogicState(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetGeneralLogicState();
	}
	virtual void GetOperatorProfile(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetOperatorProfile();
	}
	virtual void GetOperatorProfileCount(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetOperatorProfileCount();
	}
	virtual void GetSimLockDeactivateRemain(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetSimLockDeactivateRemain();
	}
	virtual void GetSimLockUnblockRemain(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetSimLockUnblockRemain();
	}
	virtual void IsOpenSimModel(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionIsOpenSimModel();
	}

	/* Error-state push and fatal/shutdown transitions (defined after all state classes) */
	virtual void OnEventAttributeError(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventBbmcComBootNg(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventBbmcComFailure(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventBbmcEnterLowBatt(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventBbmcExceedHiTemp(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventBbmcSimBlockedNormalOperation(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventBbmcSimPermanentlyBlocked(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventMediatorComFailure(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventMediatorInternalBbmcError(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventMediatorInternalCanNotUseSim(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventMediatorInternalShutdown(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventMediatorInternalSimLockError(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventSimDisabled(TelGeneralLogicStateMachineContext &ctx);
	virtual void SetDataRoamingDefinition(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionSetDataRoamingDefinition();
	}
	virtual void SetDataRoamingPermission(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionSetDataRoamingPermission();
	}
	virtual void SetFlightMode(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionSetFlightMode();
	}
	virtual void UnblockSimLock(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionUnblockSimLock();
	}
};

/* ------------------------------------------------------------------
 * State 5: DefaultSetForBooting (0x815383A8)
 * ------------------------------------------------------------------ */
class DefaultSetForBootingState : public DefaultFunctionsState {
public:
	DefaultSetForBootingState(const char *name, int stateId)
		: DefaultFunctionsState(name, stateId)
	{
	}
};

/* ------------------------------------------------------------------
 * State 8: Error (0x81538564)
 * ------------------------------------------------------------------ */
class ErrorState : public DefaultFunctionsState {
public:
	ErrorState(const char *name, int stateId)
		: DefaultFunctionsState(name, stateId)
	{
	}

	virtual void Entry(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().EnterErrorState();
	}
	virtual void CheckSimCondition(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionCheckSimCondition();
	}
	virtual void OnEventBbmcExitLowBatt(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcExitLowBatt();
	}
	virtual void OnEventBbmcFallDownLowTemp(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcFallDownLowTemp();
	}
	virtual void OnEventMediatorInternalExitFromErrorState(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getState().Exit(ctx);
		ctx.popState();
		if (ctx.getOwner().GetStateCode() != TEL_SM_STATE_ERROR)
			ctx.getState().Entry(ctx);
	}
	virtual void Default(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleUnexpectedTransition();
	}
};

/* ------------------------------------------------------------------
 * State 10: Fatal (0x81538720)
 * ------------------------------------------------------------------ */
class FatalState : public DefaultFunctionsState {
public:
	FatalState(const char *name, int stateId)
		: DefaultFunctionsState(name, stateId)
	{
	}

	virtual void Entry(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().EnterFatalState();
	}
};

/* ------------------------------------------------------------------
 * State 12: Initial (0x815388DC)
 * ------------------------------------------------------------------ */
class InitialState : public DefaultFunctionsState {
public:
	InitialState(const char *name, int stateId)
		: DefaultFunctionsState(name, stateId)
	{
	}

	virtual void Entry(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().EnterStateCode(TEL_SM_STATE_INITIAL);
	}
	virtual void TurnOn(TelGeneralLogicStateMachineContext &ctx);
};

/* ------------------------------------------------------------------
 * State 2: Booting (0x81538A98)
 * ------------------------------------------------------------------ */
class BootingState : public DefaultSetForBootingState {
public:
	BootingState(const char *name, int stateId)
		: DefaultSetForBootingState(name, stateId)
	{
	}

	virtual void Entry(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().EnterStateCode(TEL_SM_STATE_BOOTING);
	}
	virtual void OnEventBbmcComBootOk(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcComBootOk();
	}
	virtual void OnEventBbmcPinStatusReady(TelGeneralLogicStateMachineContext &ctx);
	virtual void Default(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleUnexpectedTransition();
	}
};

/* ------------------------------------------------------------------
 * State 4: DefaultSetForBooted (0x81538C54)
 * ------------------------------------------------------------------ */
class DefaultSetForBootedState : public DefaultSetForBootingState {
public:
	DefaultSetForBootedState(const char *name, int stateId)
		: DefaultSetForBootingState(name, stateId)
	{
	}

	virtual void CheckSimCondition(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionCheckSimCondition();
	}
	virtual void GetPinProtection(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetPinProtection();
	}
	virtual void GetPinRemain(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetPinRemain();
	}
	virtual void GetPinStatus(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetPinStatus();
	}
	virtual void UnblockPin(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionUnblockPin();
	}
	virtual void VerifyPin(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionVerifyPin();
	}
};

/* ------------------------------------------------------------------
 * State 7: DefaultSetForInitializing (0x81538E10)
 * ------------------------------------------------------------------ */
class DefaultSetForInitializingState : public DefaultSetForBootedState {
public:
	DefaultSetForInitializingState(const char *name, int stateId)
		: DefaultSetForBootedState(name, stateId)
	{
	}

	virtual void ChangePinCode(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionChangePinCode();
	}
	virtual void CheckImsiForOperatorCode(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionCheckImsiForOperatorCode();
	}
	virtual void GetApnInfoListCount(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetApnInfoListCount();
	}
	virtual void GetApnInfoListItem(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetApnInfoListItem();
	}
	virtual void GetIccid(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetIccid();
	}
	virtual void GetMccMncOfImsi(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetMccMncOfImsi();
	}
	virtual void GetMccMncOfNetwork(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetMccMncOfNetwork();
	}
	virtual void GetNetRegistrationInfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetNetRegistrationInfo();
	}
	virtual void GetNetworkInfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetNetworkInfo();
	}
	virtual void GetNetworkScanInfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetNetworkScanInfo();
	}
	virtual void GetNetworkScanInfoCount(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetNetworkScanInfoCount();
	}
	virtual void GetPhoneNumber(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetPhoneNumber();
	}
	virtual void GetRoamingInfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetRoamingInfo();
	}
	virtual void GetSignalLevel(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionGetSignalLevel();
	}
	virtual void OnEventBbmcPacketCallEndReasonUpdate(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleBbmcPacketCallEndReasonUpdate();
	}
	virtual void RemoveCurrentApnInfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionRemoveCurrentApnInfo();
	}
	virtual void RestoreApnInfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionRestoreApnInfo();
	}
	virtual void SetApnInfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionSetApnInfo();
	}
	virtual void SetNetRegisterAuto(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionSetNetRegisterAuto();
	}
	virtual void SetNetRegisterInfo(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionSetNetRegisterInfo();
	}
	virtual void SetPinProtection(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionSetPinProtection();
	}
};

/* ------------------------------------------------------------------
 * State 13: Initializing (0x81538FCC)
 * ------------------------------------------------------------------ */
class InitializingState : public DefaultSetForInitializingState {
public:
	InitializingState(const char *name, int stateId)
		: DefaultSetForInitializingState(name, stateId)
	{
	}

	virtual void Entry(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().EnterStateCode(TEL_SM_STATE_INITIALIZING);
	}
	virtual void OnEventBbmcNetworkReady(TelGeneralLogicStateMachineContext &ctx);
	virtual void OnEventBbmcPukRequired(TelGeneralLogicStateMachineContext &ctx);
	virtual void Default(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleUnexpectedTransition();
	}
};

/* ------------------------------------------------------------------
 * State 1: Booted (0x81539188)
 * ------------------------------------------------------------------ */
class BootedState : public DefaultSetForBootedState {
public:
	BootedState(const char *name, int stateId)
		: DefaultSetForBootedState(name, stateId)
	{
	}

	virtual void Entry(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().EnterStateCode(TEL_SM_STATE_BOOTED);
	}
	virtual void OnEventBbmcSetupReady(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getState().Exit(ctx);
		ctx.clearState();
		ctx.getOwner().HandleBbmcSetupReady();
		ctx.setState(Initializing);
		ctx.getState().Entry(ctx);
	}
	virtual void Default(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleUnexpectedTransition();
	}
};

/* ------------------------------------------------------------------
 * State 6: DefaultSetForIdle (0x81539344)
 * ------------------------------------------------------------------ */
class DefaultSetForIdleState : public DefaultSetForInitializingState {
public:
	DefaultSetForIdleState(const char *name, int stateId)
		: DefaultSetForInitializingState(name, stateId)
	{
	}

	virtual void AbortNetworkScan(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionAbortNetworkScan();
	}
	virtual void StartNetworkScan(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().ActionStartNetworkScan();
	}
};

/* ------------------------------------------------------------------
 * State 11: Idle (0x81539500)
 * ------------------------------------------------------------------ */
class IdleState : public DefaultSetForIdleState {
public:
	IdleState(const char *name, int stateId)
		: DefaultSetForIdleState(name, stateId)
	{
	}

	virtual void Entry(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().EnterIdleState();
	}
	virtual void OnEventBbmcLpmFromNetworkReady(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getState().Exit(ctx);
		ctx.clearState();
		ctx.getOwner().HandleBbmcLpmFromNetworkReady();
		ctx.setState(Initializing);
		ctx.getState().Entry(ctx);
	}
	virtual void OnEventBbmcPukRequired(TelGeneralLogicStateMachineContext &ctx);
	virtual void Default(TelGeneralLogicStateMachineContext &ctx)
	{
		ctx.getOwner().HandleUnexpectedTransition();
	}
};

/* Static SMC state singletons (matching Sony's exact names and IDs 0..14) */
BbmcEventGroupState BbmcEventGroup("GeneralLogicSm::BbmcEventGroup", 0);
BootedState Booted("GeneralLogicSm::Booted", 1);
BootingState Booting("GeneralLogicSm::Booting", 2);
DefaultFunctionsState DefaultFunctions("GeneralLogicSm::DefaultFunctions", 3);
DefaultSetForBootedState DefaultSetForBooted("GeneralLogicSm::DefaultSetForBooted", 4);
DefaultSetForBootingState DefaultSetForBooting("GeneralLogicSm::DefaultSetForBooting", 5);
DefaultSetForIdleState DefaultSetForIdle("GeneralLogicSm::DefaultSetForIdle", 6);
DefaultSetForInitializingState DefaultSetForInitializing("GeneralLogicSm::DefaultSetForInitializing", 7);
ErrorState Error("GeneralLogicSm::Error", 8);
EventGroupState EventGroup("GeneralLogicSm::EventGroup", 9);
FatalState Fatal("GeneralLogicSm::Fatal", 10);
IdleState Idle("GeneralLogicSm::Idle", 11);
InitialState Initial("GeneralLogicSm::Initial", 12);
InitializingState Initializing("GeneralLogicSm::Initializing", 13);
ShutdownState Shutdown("GeneralLogicSm::Shutdown", 14);

/* Out-of-line transitions referencing complete state definitions */
void BbmcEventGroupState::OnEventBbmcComDownAtSuspend(TelGeneralLogicStateMachineContext &ctx)
{
	ctx.getState().Exit(ctx);
	ctx.clearState();
	ctx.getOwner().HandleBbmcComDownAtSuspend();
	ctx.setState(Booting);
	ctx.getState().Entry(ctx);
}

void EventGroupState::OnEventMediatorInternalReTurnOn(TelGeneralLogicStateMachineContext &ctx)
{
	ctx.getState().Exit(ctx);
	ctx.clearState();
	ctx.getOwner().HandleMediatorInternalReTurnOn();
	ctx.setState(Booting);
	ctx.getState().Entry(ctx);
}

void DefaultFunctionsState::OnEventAttributeError(TelGeneralLogicStateMachineContext &ctx)
{
	TelGeneralLogicStateMachineState &endState = ctx.getState();
	ctx.clearState();
	ctx.getOwner().HandleAttributeError();
	ctx.setState(endState);
	ctx.pushState(Error);
	ctx.getState().Entry(ctx);
}

void DefaultFunctionsState::OnEventBbmcComBootNg(TelGeneralLogicStateMachineContext &ctx)
{
	TelGeneralLogicStateMachineState &endState = ctx.getState();
	ctx.clearState();
	ctx.getOwner().HandleBbmcComBootNg();
	ctx.setState(endState);
	ctx.pushState(Error);
	ctx.getState().Entry(ctx);
}

void DefaultFunctionsState::OnEventBbmcComFailure(TelGeneralLogicStateMachineContext &ctx)
{
	TelGeneralLogicStateMachineState &endState = ctx.getState();
	ctx.clearState();
	ctx.getOwner().HandleBbmcComFailure();
	ctx.setState(endState);
	ctx.pushState(Error);
	ctx.getState().Entry(ctx);
}

void DefaultFunctionsState::OnEventBbmcEnterLowBatt(TelGeneralLogicStateMachineContext &ctx)
{
	TelGeneralLogicStateMachineState &endState = ctx.getState();
	ctx.clearState();
	ctx.getOwner().HandleBbmcEnterLowBatt();
	ctx.setState(endState);
	ctx.pushState(Error);
	ctx.getState().Entry(ctx);
}

void DefaultFunctionsState::OnEventBbmcExceedHiTemp(TelGeneralLogicStateMachineContext &ctx)
{
	TelGeneralLogicStateMachineState &endState = ctx.getState();
	ctx.clearState();
	ctx.getOwner().HandleBbmcExceedHiTemp();
	ctx.setState(endState);
	ctx.pushState(Error);
	ctx.getState().Entry(ctx);
}

void DefaultFunctionsState::OnEventBbmcSimBlockedNormalOperation(TelGeneralLogicStateMachineContext &ctx)
{
	TelGeneralLogicStateMachineState &endState = ctx.getState();
	ctx.clearState();
	ctx.getOwner().HandleBbmcSimBlockedNormalOperation();
	ctx.setState(endState);
	ctx.pushState(Error);
	ctx.getState().Entry(ctx);
}

void DefaultFunctionsState::OnEventBbmcSimPermanentlyBlocked(TelGeneralLogicStateMachineContext &ctx)
{
	TelGeneralLogicStateMachineState &endState = ctx.getState();
	ctx.clearState();
	ctx.getOwner().HandleBbmcSimPermanentlyBlocked();
	ctx.setState(endState);
	ctx.pushState(Error);
	ctx.getState().Entry(ctx);
}

void DefaultFunctionsState::OnEventMediatorComFailure(TelGeneralLogicStateMachineContext &ctx)
{
	ctx.getState().Exit(ctx);
	ctx.clearState();
	ctx.getOwner().HandleMediatorComFailure();
	ctx.setState(Fatal);
	ctx.getState().Entry(ctx);
}

void DefaultFunctionsState::OnEventMediatorInternalBbmcError(TelGeneralLogicStateMachineContext &ctx)
{
	TelGeneralLogicStateMachineState &endState = ctx.getState();
	ctx.clearState();
	ctx.getOwner().HandleMediatorInternalBbmcError();
	ctx.setState(endState);
	ctx.pushState(Error);
	ctx.getState().Entry(ctx);
}

void DefaultFunctionsState::OnEventMediatorInternalCanNotUseSim(TelGeneralLogicStateMachineContext &ctx)
{
	TelGeneralLogicStateMachineState &endState = ctx.getState();
	ctx.clearState();
	ctx.getOwner().HandleMediatorInternalCanNotUseSim();
	ctx.setState(endState);
	ctx.pushState(Error);
	ctx.getState().Entry(ctx);
}

void DefaultFunctionsState::OnEventMediatorInternalShutdown(TelGeneralLogicStateMachineContext &ctx)
{
	ctx.getState().Exit(ctx);
	ctx.clearState();
	ctx.getOwner().HandleMediatorInternalShutdown();
	ctx.setState(Shutdown);
	ctx.getState().Entry(ctx);
}

void DefaultFunctionsState::OnEventMediatorInternalSimLockError(TelGeneralLogicStateMachineContext &ctx)
{
	TelGeneralLogicStateMachineState &endState = ctx.getState();
	ctx.clearState();
	ctx.getOwner().HandleMediatorInternalSimLockError();
	ctx.setState(endState);
	ctx.pushState(Error);
	ctx.getState().Entry(ctx);
}

void DefaultFunctionsState::OnEventSimDisabled(TelGeneralLogicStateMachineContext &ctx)
{
	TelGeneralLogicStateMachineState &endState = ctx.getState();
	ctx.clearState();
	ctx.getOwner().HandleBbmcSimDisabled();
	ctx.setState(endState);
	ctx.pushState(Error);
	ctx.getState().Entry(ctx);
}

void InitialState::TurnOn(TelGeneralLogicStateMachineContext &ctx)
{
	ctx.getState().Exit(ctx);
	ctx.clearState();
	ctx.getOwner().ActionTurnOn();
	ctx.setState(Booting);
	ctx.getState().Entry(ctx);
}

void BootingState::OnEventBbmcPinStatusReady(TelGeneralLogicStateMachineContext &ctx)
{
	ctx.getState().Exit(ctx);
	ctx.clearState();
	ctx.getOwner().HandleBbmcPinStatusReady();
	ctx.setState(Booted);
	ctx.getState().Entry(ctx);
}

void InitializingState::OnEventBbmcNetworkReady(TelGeneralLogicStateMachineContext &ctx)
{
	ctx.getState().Exit(ctx);
	ctx.clearState();
	ctx.getOwner().HandleBbmcNetworkReady();
	ctx.setState(Idle);
	ctx.getState().Entry(ctx);
}

void IdleState::OnEventBbmcPukRequired(TelGeneralLogicStateMachineContext &ctx)
{
	ctx.getState().Exit(ctx);
	ctx.clearState();
	ctx.getOwner().HandleBbmcPukRequired();
	ctx.setState(Booted);
	ctx.getState().Entry(ctx);
}

void InitializingState::OnEventBbmcPukRequired(TelGeneralLogicStateMachineContext &ctx)
{
	ctx.getState().Exit(ctx);
	ctx.clearState();
	ctx.getOwner().HandleBbmcPukRequired();
	ctx.setState(Booted);
	ctx.getState().Entry(ctx);
}

} /* namespace GeneralLogicSm */

/* ------------------------------------------------------------------
 * TelGeneralLogicStateMachineContext
 * ------------------------------------------------------------------ */

TelGeneralLogicStateMachineContext::TelGeneralLogicStateMachineContext(TelGeneralLogicAction &owner)
	: statemap::FSMContext(GeneralLogicSm::Initial),
	  m_owner(owner)
{
}

void TelGeneralLogicStateMachineContext::enterStartState()
{
	getState().Entry(*this);
}

} /* namespace telephony */
