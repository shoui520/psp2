#ifndef BBMC_CONTROL_H
#define BBMC_CONTROL_H

#include <stddef.h>
#include <bbmc/bbmc.h>
#include <threadmgr/fast_mutex_object.h>
#include <syscon/syscon.h>
#include "bbmc_memory.h"
#include "bbmc_message.h"
#include "bbmc_mailbox.h"
#include "bbmc_network_info.h"
#include "bbmc_pin.h"
#include "bbmc_proactive.h"
#include "bbmc_setup.h"
#include "bbmc_sim_content.h"
#include "bbmc_sim_refresh.h"
#include "bbmc_terminal_response.h"
#include "bbmc_update_work.h"
#include "qmi_modem_status.h"

struct BbmcMessage;
struct BbmcMailbox;

typedef struct BbmcSubscriber {
	SceInt32 active;
	SceUID processId;
	BbmcSubscribeFeatureIdParam parameters;
} BbmcSubscriber;

typedef struct BbmcCallEndReason {
	SceUInt16 type;
	SceInt16 reason;
} BbmcCallEndReason;

/* One complete manager, including the separately owned native response types. */
struct BbmcControl {
	struct ThreadmgrFastMutex mutex;
	SceUInt32 flags;
	SceInt32 currentState;
	SceInt32 targetState;
	SceInt32 savedTargetState;
	int (*stateHandler)(BbmcControl *, struct BbmcMessage *);
	SceUInt32 suspendState;
	SceUInt32 suspendStep;
	union {
		int (*suspendHandler)(BbmcControl *);
		int (*resumeHandler)(BbmcControl *, struct BbmcMessage *);
		int (*suspendMessageHandler)(BbmcControl *, struct BbmcMessage *);
	};
	SceUInt32 backendShutdownRequested;
	SceInt32 suspendError;
	BbmcNetworkInfo networkInfo;
	SceUInt32 nasCause;
	SceUInt32 nasReason;
	BbmcNetworkScanInfo networkScan;
	SceUInt16 secondBootReply;
	SceUInt16 firstBootReply;
	SceSysconPacket eventPacket;
	/* Only whole-manager initialization reaches this inactive interval. */
	SceUInt8 reservedPacketStorage[128];
	SceSysconPacket bytePacket;
	SceSysconPacket statusPacket;
	SceSysconPacket operationModePacket;
	SceSysconPacket proactivePacket;
	SceSysconPacket transitionPacket;
	SceUID threadId;
	SceUID eventFlag;
	SceUID processEventHandler;
	struct BbmcMailbox *mailbox;
	SceUInt32 references;
	SceUInt32 pendingEvents[9];
	SceUInt32 deliveredEvents[9];
	SceUInt32 acknowledgedEvents[9];
	SceUInt32 callbackPending[9];
	BbmcSubscriber subscribers[9];
	BbmcPinStatus pinStatus;
	QmiIndexedResponse indexedResponses[5];
	BbmcProactiveCmd proactiveCommand;
	BbmcFileRefreshDetail refreshDetail;
	BbmcSimContentInfo simContent;
	BbmcUnblockPinParam unblockPin;
	BbmcControlKeyParam controlKey;
	BbmcTerminalResponse terminalResponse;
	QmiCat2CommandOptions envelopeOptions;
	SceUInt32 simRefreshPending;
	SceUInt32 refreshCommandValue;
	SceUInt32 sysconRefreshPending;
	SceUInt32 simCommandBusy;
	SceUInt32 smsListBusy;
	SceUInt32 smsReadState;
	SceUInt32 smsSendBusy;
	SceUInt32 smsIndices[64];
	SceUInt32 smsCount;
	SceUInt32 smsReadLength;
	SceUInt8 smsReadData[256];
	SceUInt32 smsFormat;
	SceUInt8 smsSendData[256];
	BbmcDeviceInformation deviceInformation;
	BbmcSmscAddress smscAddress;
	BbmcSystemSelectionPreference systemSelection;
	BbmcProfileSettings profile;
	QmiCountedBytes countedBytes;
	BbmcSetupValue setupValue;
	SceUInt16 versionFamily;
	SceUInt16 versionBuild;
	SceUInt16 versionRevision;
	SceUInt8 versionPatch;
	SceUInt16 compatibleVersion;
	SceUInt8 replyInterface;
	SceUInt8 uartMode;
	SceUInt32 baudRate;
	SceUInt32 trackingState;
	SceUInt32 gpsProviderRouting;
	SceUInt32 gpsIoctlPending;
	SceUInt32 versionCapability;
	SceUInt32 trackingStartMode;
	SceUInt32 cellQueryState;
	BbmcCellLocationInfo cellLocation;
	SceUInt32 connectPending;
	SceUInt32 closeNetworkLinkInProgress;
	SceUInt32 clientInitialized;
	SceUInt32 clientOnline;
	SceUInt32 networkStarted;
	SceUInt32 networkStartBusy;
	SceUInt32 networkStopBusy;
	SceUInt32 deferredNetworkStart;
	SceUInt32 networkRetryPending;
	BbmcCallEndReason callEndReason;
	SceUInt32 controlFlagsBusy;
	SceUInt32 controlFlags;
	SceUInt32 normalizedControlState;
	SceUInt32 currentBearer;
	SceUInt32 bearerDetails;
	SceUInt32 operationBusy;
	SceUInt32 reservedOperation;
	SceUInt32 setupWriteState;
	SceUInt32 setupReadState;
	SceUInt32 powerTransitionBusy;
	SceUInt32 registrationPending;
	SceUInt32 defaultProfile;
	SceUInt32 networkScanState;
	SceUInt32 pendingPacketRequest;
	SceUInt32 attachChangeBusy;
	SceUInt32 networkScanAbortBusy;
	SceUInt32 forceOperationMode;
	BbmcUpdateWork updateWork;
	SceInt32 lastError;
	SceUInt32 downloadNotificationRegistered;
	SceUInt32 storedImagesState;
	SceInt32 storedImagesResult;
	SceUInt32 firmwarePreferenceState;
	SceInt32 firmwarePreferenceResult;
	SceUInt32 firmwareImageState;
	SceInt32 firmwareImageResult;
	SceUInt32 setFirmwarePreferenceState;
	SceInt32 setFirmwarePreferenceResult;
	SceUInt32 downloadImageCount;
	SceUInt32 firstDownloadImageType;
	SceUInt32 secondDownloadImageType;
	QmiStoredImages storedImages;
	QmiFirmwarePreference firmwarePreference;
	QmiFirmwareImage firmwareImage;
	QmiSetFirmwarePreference setFirmwarePreference;
	SceUInt32 replyVersionValid;
	SceUInt32 replyVersion;
	SceUInt32 updateHandler;
	SceUInt32 configurationMode;
	SceUInt32 configurationReplyValid;
	SceUInt32 firstConfigurationReply;
	SceUInt32 secondConfigurationReply;
	SceUInt32 decodedReply;
	QmiStoredImages *updateStoredImages;
	SceUInt32 updateTotalBytes;
	SceUInt32 updateCompletedBytes;
	SceUID updateFiles[5];
	SceUInt32 updateEntryId;
	SceUID updateFilePair[2];
	SceUID updateFile;
	SceUInt32 firstStatusValue;
	SceUInt32 secondStatusValue;
	SceUInt32 updateCancelled;
	SceUInt32 sysconPowerRequestPending;
	SceUInt32 transportReady;
	SceUInt32 transportReplyPending;
	SceUInt32 transportHandshakePending;
	SceUInt32 deferredSysconEvent;
	SceUInt32 resumePending;
	SceUInt32 pendingPowerUp;
	SceUInt32 pendingPowerDown;
	SceUInt32 pinCommandInterrupted;
	SceUInt32 pinCommandDeferred;
	SceUInt32 sysconPowerInverted;
	SceUInt32 powerAvailable;
	SceUInt32 powerPolicy;
	SceUInt32 powerEventActive;
	SceUInt32 recentPowerEvent;
	SceUID powerEventTimer;
	SceUID networkRetryTimer;
	SceUInt32 suspendEligible;
	SceUInt32 suspendArgument;
	SceUInt32 firmwareBuildSelection;
	SceUInt32 controlKeyReported;
	SceInt32 firmwareRevisionSelection;
	SceUInt32 configurationReplySeen;
	SceUInt32 runningStorageIndex;
	SceUInt32 replyOperationMode;
	SceUInt32 completionPending;
	SceInt32 previousCurrentState;
	SceInt32 previousTargetState;
	SceUInt32 transitionStep;
	SceInt32 transitionStatus;
};

_Static_assert(sizeof(BbmcSubscriber) == 24, "BBMC subscriber size");
_Static_assert(sizeof(BbmcCallEndReason) == 4, "BBMC verbose call end reason size");
_Static_assert(offsetof(BbmcCallEndReason, type) == 0, "BBMC call end reason type");
_Static_assert(offsetof(BbmcCallEndReason, reason) == 2, "BBMC call end reason value");
_Static_assert(offsetof(BbmcSubscriber, parameters) == 8,
	"BBMC subscriber parameters");
_Static_assert(sizeof(BbmcControl) == 25032, "BBMC manager size");

#define BBMC_CONTROL_OFFSET(member, value) \
	_Static_assert(offsetof(BbmcControl, member) == (value), \
		"BBMC manager " #member)
BBMC_CONTROL_OFFSET(flags, 64);
BBMC_CONTROL_OFFSET(stateHandler, 80);
BBMC_CONTROL_OFFSET(suspendHandler, 92);
BBMC_CONTROL_OFFSET(resumeHandler, 92);
BBMC_CONTROL_OFFSET(suspendMessageHandler, 92);
BBMC_CONTROL_OFFSET(networkInfo, 104);
BBMC_CONTROL_OFFSET(nasCause, 560);
BBMC_CONTROL_OFFSET(nasReason, 564);
BBMC_CONTROL_OFFSET(networkScan, 568);
BBMC_CONTROL_OFFSET(secondBootReply, 1532);
BBMC_CONTROL_OFFSET(eventPacket, 1536);
BBMC_CONTROL_OFFSET(reservedPacketStorage, 1664);
BBMC_CONTROL_OFFSET(bytePacket, 1792);
BBMC_CONTROL_OFFSET(statusPacket, 1920);
BBMC_CONTROL_OFFSET(operationModePacket, 2048);
BBMC_CONTROL_OFFSET(proactivePacket, 2176);
BBMC_CONTROL_OFFSET(transitionPacket, 2304);
BBMC_CONTROL_OFFSET(threadId, 2432);
BBMC_CONTROL_OFFSET(pendingEvents, 2452);
BBMC_CONTROL_OFFSET(subscribers, 2596);
BBMC_CONTROL_OFFSET(pinStatus, 2812);
BBMC_CONTROL_OFFSET(indexedResponses, 2824);
BBMC_CONTROL_OFFSET(proactiveCommand, 2904);
BBMC_CONTROL_OFFSET(refreshDetail, 19600);
BBMC_CONTROL_OFFSET(simContent, 19816);
BBMC_CONTROL_OFFSET(unblockPin, 19872);
BBMC_CONTROL_OFFSET(controlKey, 19924);
BBMC_CONTROL_OFFSET(terminalResponse, 19952);
BBMC_CONTROL_OFFSET(envelopeOptions, 20500);
BBMC_CONTROL_OFFSET(refreshCommandValue, 20520);
BBMC_CONTROL_OFFSET(smsReadState, 20536);
BBMC_CONTROL_OFFSET(smsIndices, 20544);
BBMC_CONTROL_OFFSET(smsReadData, 20808);
BBMC_CONTROL_OFFSET(smsSendData, 21068);
BBMC_CONTROL_OFFSET(deviceInformation, 21324);
BBMC_CONTROL_OFFSET(smscAddress, 21760);
BBMC_CONTROL_OFFSET(systemSelection, 21788);
BBMC_CONTROL_OFFSET(profile, 21812);
BBMC_CONTROL_OFFSET(countedBytes, 22364);
BBMC_CONTROL_OFFSET(setupValue, 22376);
BBMC_CONTROL_OFFSET(versionFamily, 22928);
BBMC_CONTROL_OFFSET(compatibleVersion, 22936);
BBMC_CONTROL_OFFSET(baudRate, 22940);
BBMC_CONTROL_OFFSET(cellLocation, 22968);
BBMC_CONTROL_OFFSET(connectPending, 23504);
BBMC_CONTROL_OFFSET(currentBearer, 23556);
BBMC_CONTROL_OFFSET(setupWriteState, 23572);
BBMC_CONTROL_OFFSET(updateWork, 23612);
BBMC_CONTROL_OFFSET(lastError, 23752);
BBMC_CONTROL_OFFSET(storedImages, 23804);
BBMC_CONTROL_OFFSET(firmwarePreference, 24500);
BBMC_CONTROL_OFFSET(firmwareImage, 24624);
BBMC_CONTROL_OFFSET(setFirmwarePreference, 24684);
BBMC_CONTROL_OFFSET(updateHandler, 24828);
BBMC_CONTROL_OFFSET(configurationMode, 24832);
BBMC_CONTROL_OFFSET(updateStoredImages, 24852);
BBMC_CONTROL_OFFSET(updateFiles, 24864);
BBMC_CONTROL_OFFSET(updateEntryId, 24884);
BBMC_CONTROL_OFFSET(updateFile, 24896);
BBMC_CONTROL_OFFSET(updateCancelled, 24908);
BBMC_CONTROL_OFFSET(sysconPowerRequestPending, 24912);
BBMC_CONTROL_OFFSET(powerPolicy, 24960);
BBMC_CONTROL_OFFSET(firmwareBuildSelection, 24988);
BBMC_CONTROL_OFFSET(firmwareRevisionSelection, 24996);
BBMC_CONTROL_OFFSET(runningStorageIndex, 25004);
BBMC_CONTROL_OFFSET(transitionStatus, 25028);
#undef BBMC_CONTROL_OFFSET

/* bbmc.c */
int bbmcSendSysconCommand(BbmcControl *control,
	int (*callback)(SceSysconPacket *, void *), SceSysconPacket *packet,
	SceUInt8 selector, SceUInt8 operation, unsigned int byteCount, ...);
int bbmcSendSizedSysconCommand(BbmcControl *control,
	int (*callback)(SceSysconPacket *, void *), SceSysconPacket *packet,
	SceUInt8 selector, SceUInt8 payloadLength, unsigned int byteCount, ...);
int bbmcCancelSysconCommand(SceSysconPacket *packet, int *pending);

/* bbmc_gpio.c */
int bbmcInitializeGpio(void);
int bbmcFinalizeGpio(void);
int bbmcResetGpio(void);
int bbmcAcquireGpio(int *previous);
int bbmcReleaseGpio(int *previous);
int bbmcGetGpioUsers(void);

/* bbmc_timer.c */
int bbmcInitializeTimer(void);
int bbmcFinalizeTimer(void);
int bbmcTimerResetHook(void);
int bbmcTimerSetHandler(SceUInt32 delay, void (*callback)(void *arg), void *arg);
int bbmcTimerCancelHandler(int id);

/* state_machine/com_boot.c */
int suspendDuringFromComBootToStandby(BbmcControl *control, BbmcMessage *message);

/* state_machine/connected.c */
int suspendSteadyConnected(BbmcControl *control, BbmcMessage *message);
int suspendGotoLPM(BbmcControl *control, BbmcMessage *message);

/* state_machine/low_power_mode.c */
int suspendGotoConnected(BbmcControl *control, BbmcMessage *message);

#endif
