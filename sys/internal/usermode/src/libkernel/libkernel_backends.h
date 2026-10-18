#ifndef SCE_LIBKERNEL_BACKENDS_H
#define SCE_LIBKERNEL_BACKENDS_H

#include <scetypes.h>
#include <systimer/timer_regs.h>
#include <kernel/threadmgr.h>
#include <kernel/modulemgr.h>
#include <kernel/iofilemgr.h>
#include <kernel/openpsid.h>

/* Native syscall boundaries. Request layouts remain with their producers. */
struct AdhocAuthExchangeRequest;
struct AdhocAuthRequest;
struct AdhocBB160Context;
struct AdhocBB224Context;
struct AdhocBroadcastEncryptRequest;
struct AdhocDecryptRequest;
struct AdhocKeysRequest;
struct AdhocUnicastEncryptRequest;
struct BacktraceOptions;
struct EventCancelOptions;
struct EventFlagWaitOptions;
struct EventInfoRequest;
struct EventNotifyOptions;
struct EventWaitOptions;
struct IoControlAsyncOptions;
struct IoControlOptions;
struct IoDeviceControlOptions;
struct IoPathAsyncOptions;
struct IoPathOptions;
struct IoPositionAsyncOptions;
struct IoPositionOptions;
struct IoRenameOptions;
struct IoSeekAsyncOptions;
struct IoSeekOptions;
struct LibkernelAddresses;
struct LwCondCreateOptions;
struct LwInfoRequest;
struct LwMutexCreateOptions;
struct ModuleCallOptions;
struct MsSaveBBCipherContext;
struct MsSaveBBCipherRequest;
struct MsSaveBBMacContext;
struct MsSaveBBMacRequest;
struct MsgPipeCreateOptions;
struct MsgPipeInfoRequest;
struct MsgPipeTryOptions;
struct MsgPipeWaitOptions;
struct MultipleEventWaitOptions;
struct ObjectInfoRequest;
struct PackageVerifyRequest;
struct PcActivationChallengeRequest;
struct PsmActivationRequest;
struct RandomNumberRequest;
struct SecureClockRequest;
struct SemaCreateOptions;
struct ThreadContextRequest;
struct ThreadEventHandlerOptions;
struct ThreadRecordRequest;
struct ThreadStatusRequest;
struct TimerInfoRequest;

typedef struct SceKernelCallFrame SceKernelCallFrame;

extern int _sceSblGcAuthMgrAdhocBB160UniCastEncrypt(
		struct AdhocBB160Context *context, void *data, SceSize size,
		const struct AdhocUnicastEncryptRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160UniCastDecrypt(
		struct AdhocBB160Context *context, void *data, SceSize size,
		const struct AdhocDecryptRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160BroadCastEncrypt(void *data, SceSize size,
		void *authentication,
		const struct AdhocBroadcastEncryptRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160BroadCastDecrypt(
		struct AdhocBB160Context *context, void *data, SceSize size,
		const struct AdhocDecryptRequest *request);
extern int _sceSblGcAuthMgrPkgVry(const void *first, const void *second,
		const struct PackageVerifyRequest *request);
extern int _sceSblGcAuthMgrSclkGetData1(void *data,
		const struct SecureClockRequest *request);
extern int _sceSblGcAuthMgrSclkSetData2(const void *data,
		const struct SecureClockRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224Init(void);
extern int _sceSblGcAuthMgrAdhocBB160Init(void);
extern int _sceSblGcAuthMgrPcactGetChallenge(SceUInt mode, const void *input,
		void *output, const struct PcActivationChallengeRequest *request);
extern int _sceSblGcAuthMgrPcactActivation(const void *activation,
		SceUInt unknownArgument);
extern int _sceSblGcAuthMgrPsmactCreateC1(SceUInt mode, void *activation,
		void *response, const struct PsmActivationRequest *request);
extern int _sceSblGcAuthMgrPsmactVerifyR1(const void *activation,
		const void *response, void *output,
		const struct PsmActivationRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224Auth1(struct AdhocBB224Context *context,
		void *output, const struct AdhocAuthRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160Auth1(struct AdhocBB160Context *context,
		void *output, const struct AdhocAuthRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160Auth5(struct AdhocBB160Context *context,
		const void *input, const struct AdhocAuthRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224Auth2(struct AdhocBB224Context *context,
		const void *input, void *output,
		const struct AdhocAuthExchangeRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224Auth3(struct AdhocBB224Context *context,
		const void *input, void *output,
		const struct AdhocAuthExchangeRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224Auth4(struct AdhocBB224Context *context,
		const void *input, void *output,
		const struct AdhocAuthExchangeRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224Auth5(struct AdhocBB224Context *context,
		const void *input, const struct AdhocAuthRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160Auth2(struct AdhocBB160Context *context,
		const void *input, void *output,
		const struct AdhocAuthExchangeRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160Auth3(struct AdhocBB160Context *context,
		const void *input, void *output,
		const struct AdhocAuthExchangeRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160Auth4(struct AdhocBB160Context *context,
		const void *input, void *output,
		const struct AdhocAuthExchangeRequest *request);
extern int _sceSblGcAuthMgrAdhocBB224GetKeys(
		const struct AdhocBB224Context *context, void *firstKey,
		void *secondKey, const struct AdhocKeysRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160GetKeys(
		const struct AdhocBB160Context *context, void *firstKey,
		void *secondKey, const struct AdhocKeysRequest *request);
extern int _sceSblGcAuthMgrGetMediaIdType01(void *mediaId,
		const struct AdhocAuthRequest *request);
extern int _sceKernelBacktrace(SceUID threadId, SceKernelCallFrame *frames,
		SceSize size, const struct BacktraceOptions *options);
extern int _sceKernelWaitEvent(SceUID id, SceUInt32 pattern,
		SceUInt32 *result, const struct EventWaitOptions *options);
extern int _sceKernelWaitEventCB(SceUID id, SceUInt32 pattern,
		SceUInt32 *result, const struct EventWaitOptions *options);
extern int _sceKernelPollEvent(SceUID id, SceUInt32 pattern,
		SceUInt32 *result, SceUInt64 *userData);
extern int _sceKernelCancelEvent(SceUID id, SceUInt32 *waiting);
extern int _sceKernelWaitMultipleEvents(SceKernelWaitEvent *events,
		SceUInt32 count, SceUInt32 mode,
		const struct MultipleEventWaitOptions *options);
extern int _sceKernelWaitMultipleEventsCB(SceKernelWaitEvent *events,
		SceUInt32 count, SceUInt32 mode,
		const struct MultipleEventWaitOptions *options);
extern int _sceKernelSetEventWithNotifyCallback(SceUID id, SceUInt32 pattern,
		const struct EventNotifyOptions *options);
extern int _sceKernelPulseEventWithNotifyCallback(SceUID id, SceUInt32 pattern,
		const struct EventNotifyOptions *options);
extern int _sceKernelCancelEventWithSetPattern(SceUID id, SceUInt32 pattern,
		const struct EventCancelOptions *options);
extern int _sceKernelGetEventPattern(SceUID id, SceUInt32 *pattern);
extern int _sceKernelGetEventInfo(SceUID id, SceKernelEventInfo *info,
		const struct EventInfoRequest *request);
extern SceUID _sceIoOpen(const char *path, int flags, SceMode mode,
		const struct IoPathOptions *options);
extern int _sceIoRemove(const char *path, const struct IoPathOptions *options);
extern int _sceIoRename(const char *oldPath, const char *newPath,
		const struct IoRenameOptions *options);
extern int _sceIoMkdir(const char *path, SceMode mode,
		const struct IoPathOptions *options);
extern int _sceIoRmdir(const char *path, const struct IoPathOptions *options);
extern SceUID _sceIoDopen(const char *path, const struct IoPathOptions *options);
extern int _sceIoGetstat(const char *path, SceIoStat *status,
		const struct IoPathOptions *options);
extern int _sceIoChstat(const char *path, const SceIoStat *status, int bits,
		const struct IoPathOptions *options);
extern int _sceIoSync(const char *device, unsigned int flags,
		const struct IoPathOptions *options);
extern int _sceIoOpenAsync(const char *path, int flags, SceMode mode,
		const struct IoPathAsyncOptions *options);
extern int _sceIoRemoveAsync(const char *path, void *result,
		const struct IoPathOptions *options);
extern int _sceIoMkdirAsync(const char *path, SceMode mode, void *result,
		const struct IoPathOptions *options);
extern int _sceIoRmdirAsync(const char *path, void *result,
		const struct IoPathOptions *options);
extern int _sceIoRenameAsync(const char *oldPath, const char *newPath,
		void *result, const struct IoRenameOptions *options);
extern int _sceIoChstatAsync(const char *path, const SceIoStat *status, int bits,
		const struct IoPathAsyncOptions *options);
extern int _sceIoGetstatAsync(const char *path, SceIoStat *status, void *result,
		const struct IoPathOptions *options);
extern int _sceIoSyncAsync(const char *device, unsigned int flags, void *result,
		const struct IoPathOptions *options);
extern SceOff _sceIoLseek(SceUID id, const struct IoSeekOptions *options);
extern int _sceIoLseekAsync(SceUID id,
		const struct IoSeekAsyncOptions *options);
extern int _sceIoIoctl(SceUID id, unsigned int command, const void *input,
		const struct IoControlOptions *options);
extern int _sceIoDevctl(const char *device, unsigned int command,
		const void *input, const struct IoDeviceControlOptions *options);
extern int _sceIoIoctlAsync(SceUID id, unsigned int command, const void *input,
		const struct IoControlAsyncOptions *options);
extern int _sceIoDevctlAsync(const char *device, unsigned int command,
		const void *input, const struct IoControlAsyncOptions *options);
extern int _sceIoPread(SceUID id, void *buffer, SceSize size,
		const struct IoPositionOptions *options);
extern int _sceIoPwrite(SceUID id, const void *buffer, SceSize size,
		const struct IoPositionOptions *options);
extern int _sceIoPreadAsync(SceUID id, void *buffer, SceSize size,
		const struct IoPositionAsyncOptions *options);
extern int _sceIoPwriteAsync(SceUID id, const void *buffer, SceSize size,
		const struct IoPositionAsyncOptions *options);
extern int _sceIoDread(SceUID id, SceIoDirent *entry);
extern int _sceIoGetstatByFd(SceUID id, SceIoStat *status);
extern int _sceIoChstatByFd(SceUID id, const SceIoStat *status, int bits);
extern int _sceIoCompleteMultiple(const void *requests, int count,
		int *completed);
extern int _sceKernelExitCallback(int status);
extern int _sceKernelRegisterLibkernelAddresses(const struct LibkernelAddresses *addresses);
extern int _sceKernelGetTimer5Reg(SceLongTimerRegisters **timer);
extern void _sceKernelExitProcessForUser(int status);
extern int _sceKernelLockLwMutex(SceKernelLwMutexWork *work, int count,
		unsigned int *timeout, unsigned int callbacks);
extern int _sceKernelUnlockLwMutex(SceKernelLwMutexWork *work, int count);
extern int __sceKernelCreateLwMutex(SceKernelLwMutexWork *work,
		const char *name, unsigned int attributes,
		const struct LwMutexCreateOptions *options);
extern int _sceKernelDeleteLwMutex(SceKernelLwMutexWork *work);
extern int _sceKernelGetLwMutexInfoById(SceUID id, SceKernelLwMutexInfo *info,
		const struct LwInfoRequest *request);
extern int _sceKernelCreateLwCond(SceKernelLwCondWork *work,
		const char *name, unsigned int attributes,
		const struct LwCondCreateOptions *options);
extern int _sceKernelDeleteLwCond(SceKernelLwCondWork *work);
extern int _sceKernelWaitLwCond(SceKernelLwCondWork *work, unsigned int *timeout);
extern int _sceKernelWaitLwCondCB(SceKernelLwCondWork *work, unsigned int *timeout);
extern int _sceKernelSignalLwCond(SceKernelLwCondWork *work);
extern int _sceKernelSignalLwCondAll(SceKernelLwCondWork *work);
extern int _sceKernelSignalLwCondTo(SceKernelLwCondWork *work, SceUID threadId);
extern int _sceKernelGetLwCondInfo(SceKernelLwCondWork *work, SceKernelLwCondInfo *info);
extern int _sceKernelGetLwCondInfoById(SceUID id, SceKernelLwCondInfo *info,
		const struct LwInfoRequest *request);
extern SceUID _sceKernelLoadModule(const char *path, SceUInt flags,
		const SceKernelLoadModuleOpt *option);
extern int _sceKernelUnloadModule(SceUID id, SceUInt flags,
		const SceKernelUnloadModuleOpt *option);
extern int _sceKernelStartModule(SceUID id, SceSize size, const void *argument,
		const struct ModuleCallOptions *options);
extern int _sceKernelStopModule(SceUID id, SceSize size, const void *argument,
		const struct ModuleCallOptions *options);
extern SceUID _sceKernelLoadStartModule(const char *path, SceSize size,
		const void *argument, const struct ModuleCallOptions *options);
extern int _sceKernelStopUnloadModule(SceUID id, SceSize size, const void *argument,
		const struct ModuleCallOptions *options);
extern SceUID _sceKernelOpenModule(const char *path, SceSize size,
		void *argument, const struct ModuleCallOptions *options);
extern int _sceKernelCloseModule(SceUID id, SceSize size, void *argument,
		const struct ModuleCallOptions *options);
extern SceUID _sceKernelCreateMsgPipeWithLR(const char *name, SceUInt32 type,
		SceUInt32 attributes, const struct MsgPipeCreateOptions *options);
extern int _sceKernelSendMsgPipeVector(SceUID id,
		const SceKernelMsgPipeVector *vectors, SceUInt32 count,
		const struct MsgPipeWaitOptions *options);
extern int _sceKernelSendMsgPipeVectorCB(SceUID id,
		const SceKernelMsgPipeVector *vectors, SceUInt32 count,
		const struct MsgPipeWaitOptions *options);
extern int _sceKernelTrySendMsgPipeVector(SceUID id,
		const SceKernelMsgPipeVector *vectors, SceUInt32 count,
		const struct MsgPipeTryOptions *options);
extern int _sceKernelReceiveMsgPipeVector(SceUID id,
		const SceKernelMsgPipeVector *vectors, SceUInt32 count,
		const struct MsgPipeWaitOptions *options);
extern int _sceKernelReceiveMsgPipeVectorCB(SceUID id,
		const SceKernelMsgPipeVector *vectors, SceUInt32 count,
		const struct MsgPipeWaitOptions *options);
extern int _sceKernelTryReceiveMsgPipeVector(SceUID id,
		const SceKernelMsgPipeVector *vectors, SceUInt32 count,
		const struct MsgPipeTryOptions *options);
extern int _sceKernelCancelMsgPipe(SceUID id, SceUInt32 *sendWaiting,
				   SceUInt32 *receiveWaiting);
extern int _sceKernelGetMsgPipeInfo(SceUID id, SceKernelMsgPipeInfo *info,
				    const struct MsgPipeInfoRequest *request);
extern SceUInt32 sceKernelGetProcessTimeLowCore(void);
extern SceUInt64 sceKernelGetProcessTimeWideCore(void);
extern int sceKernelGetProcessTimeCore(SceKernelSysClock *clock);
extern int _sceSblGcAuthMgrMsSaveBBMacInit(struct MsSaveBBMacContext *context,
		SceUInt mode);
extern int _sceSblGcAuthMgrMsSaveBBMacUpdate(struct MsSaveBBMacContext *context,
		const void *data, SceSize size);
extern int _sceSblGcAuthMgrMsSaveBBCipherUpdate(
		struct MsSaveBBCipherContext *context, void *data, SceSize size);
extern int _sceSblGcAuthMgrMsSaveBBCipherFinal(
		struct MsSaveBBCipherContext *context);
extern int _sceSblGcAuthMgrMsSaveBBMacFinal(struct MsSaveBBMacContext *context,
		void *mac, const void *key, const struct MsSaveBBMacRequest *request);
extern int _sceSblGcAuthMgrMsSaveBBCipherInit(
		struct MsSaveBBCipherContext *context, SceUInt mode,
		SceUInt generationMode, const struct MsSaveBBCipherRequest *request);
extern int _sceSblGcAuthMgrAdhocBB160Shutdown(void);
extern int _sceSblGcAuthMgrAdhocBB224Shutdown(void);
extern int _sceKernelGetOpenPsId(SceKernelOpenPsId *id);
extern SceInt32 _sceSblACMgrIsGameProgram(SceBool *result);
extern int _sceKernelGetRandomNumber(void *output, SceSize size,
		const struct RandomNumberRequest *request);
extern int _sceKernelGetThreadCpuAffinityMask(SceUID threadId);
extern int _sceKernelGetThreadEventInfo(SceUID id, void *info,
		const struct ThreadRecordRequest *request);
extern int _sceKernelSetThreadContextForVM(SceUID id, const void *cpu,
		const void *vfp, const struct ThreadContextRequest *request);
extern int _sceKernelGetThreadContextForVM(SceUID id, void *cpu, void *vfp,
		const struct ThreadContextRequest *request);
extern int _sceKernelPMonThreadGetCounter(SceUID id, unsigned int counter,
		SceUInt32 *value);
extern int _sceKernelGetSystemTime(SceUInt64 *time);
extern int _sceKernelWaitException(SceUID id, void *info, SceUInt *timeout,
		const struct ThreadRecordRequest *request);
extern int _sceKernelWaitExceptionCB(SceUID id, void *info, SceUInt *timeout,
		const struct ThreadRecordRequest *request);
extern SceUID _sceKernelCreateSimpleEvent(const char *name, SceUInt attributes,
		SceUInt pattern, const SceKernelSimpleEventOptParam *option);
extern int _sceKernelWaitSignal(SceUInt64 value, SceUInt *timeout);
extern int _sceKernelWaitSignalCB(SceUInt64 value, SceUInt *timeout);
extern int _sceKernelGetThreadInfo(SceUID id, SceKernelThreadInfo *info,
				   const struct ObjectInfoRequest *request);
extern int _sceKernelGetCallbackInfo(SceUID id, SceKernelCallbackInfo *info,
				     const struct ObjectInfoRequest *request);
extern int _sceKernelGetSystemInfo(SceKernelSystemInfo *info,
				   const struct ObjectInfoRequest *request);
extern int _sceKernelGetThreadExitStatus(SceUID id, int *status);
extern SceUID _sceKernelCreateRWLock(const char *name, SceUInt32 attributes,
				   const SceKernelRWLockOptParam *option);
extern int _sceKernelLockReadRWLock(SceUID id, unsigned int *timeout);
extern int _sceKernelLockReadRWLockCB(SceUID id, unsigned int *timeout);
extern int _sceKernelLockWriteRWLock(SceUID id, unsigned int *timeout);
extern int _sceKernelLockWriteRWLockCB(SceUID id, unsigned int *timeout);
extern int _sceKernelCancelRWLock(SceUID id, SceUInt32 *readWaiting,
				  SceUInt32 *writeWaiting, SceInt32 flags);
extern int _sceKernelGetRWLockInfo(SceUID id, SceKernelRWLockInfo *info,
				   const struct ObjectInfoRequest *request);
extern SceUID _sceKernelCreateSema(const char *name, SceUInt attributes,
				int initialCount, const struct SemaCreateOptions *options);
extern SceUID _sceKernelCreateSema_16XX(const char *name, SceUInt attributes,
				     int initialCount, const struct SemaCreateOptions *options);
extern int _sceKernelWaitSema(SceUID id, int count, SceUInt *timeout);
extern int _sceKernelWaitSemaCB(SceUID id, int count, SceUInt *timeout);
extern int _sceKernelCancelSema(SceUID id, int count, SceUInt32 *waiting);
extern int _sceKernelGetSemaInfo(SceUID id, SceKernelSemaInfo *info,
			       const struct ObjectInfoRequest *request);
extern SceUID _sceKernelCreateMutex(const char *name, SceUInt attributes,
				 int initialCount, const SceKernelMutexOptParam *option);
extern int _sceKernelLockMutex(SceUID id, int count, unsigned int *timeout);
extern int _sceKernelLockMutexCB(SceUID id, int count, unsigned int *timeout);
extern int _sceKernelCancelMutex(SceUID id, int count, SceUInt32 *waiting);
extern int _sceKernelGetMutexInfo(SceUID id, SceKernelMutexInfo *info,
				const struct ObjectInfoRequest *request);
extern SceUID _sceKernelCreateEventFlag(const char *name, SceUInt32 attributes,
				     SceUInt32 pattern, const SceKernelEventFlagOptParam *option);
extern int _sceKernelWaitEventFlag(SceUID id, unsigned int bits, unsigned int mode,
				 const struct EventFlagWaitOptions *options);
extern int _sceKernelWaitEventFlagCB(SceUID id, unsigned int bits, unsigned int mode,
				   const struct EventFlagWaitOptions *options);
extern int _sceKernelPollEventFlag(SceUID id, unsigned int bits, unsigned int mode,
				 unsigned int *resultBits);
extern int _sceKernelCancelEventFlag(SceUID id, unsigned int bits, unsigned int *waiting);
extern int _sceKernelGetEventFlagInfo(SceUID id, SceKernelEventFlagInfo *info,
				    const struct ObjectInfoRequest *request);
extern SceUID _sceKernelCreateCond(const char *name, SceUInt attributes,
				SceUID mutexId, const SceKernelCondOptParam *option);
extern int _sceKernelWaitCond(SceUID id, unsigned int *timeout);
extern int _sceKernelWaitCondCB(SceUID id, unsigned int *timeout);
extern int _sceKernelGetCondInfo(SceUID id, SceKernelCondInfo *info,
			       const struct ObjectInfoRequest *request);
extern int _sceKernelGetThreadRunStatus(SceKernelThreadRunStatus *status,
				      const struct ThreadStatusRequest *request);
extern int _sceKernelStartThread(SceUID id, SceSize size, const void *argument);
extern int _sceKernelWaitThreadEnd(SceUID id, int *status, SceUInt *timeout);
extern int _sceKernelWaitThreadEndCB(SceUID id, int *status, SceUInt *timeout);
extern SceUID _sceKernelRegisterThreadEventHandler(const char *name, SceUID id,
		SceInt32 mask, const struct ThreadEventHandlerOptions *options);
extern SceUID _sceKernelCreateTimer(const char *name, SceUInt32 attributes,
				   const SceKernelTimerOptParam *option);
extern int _sceKernelGetTimerBase(SceUID id, SceKernelSysClock *clock);
extern int _sceKernelGetTimerTime(SceUID id, SceKernelSysClock *clock);
extern int _sceKernelSetTimerTime(SceUID id, SceKernelSysClock *clock);
extern int _sceKernelSetTimerEvent(SceUID id, SceInt32 type,
				   SceKernelSysClock *interval, SceInt32 repeat);
extern int _sceKernelCancelTimer(SceUID id, SceUInt32 *waiting);
extern int _sceKernelGetTimerInfo(SceUID id, SceKernelTimerInfo *info,
				  const struct TimerInfoRequest *request);
extern int _sceKernelGetTimerEventRemainingTime(SceUID id, SceKernelSysClock *clock);

#endif

