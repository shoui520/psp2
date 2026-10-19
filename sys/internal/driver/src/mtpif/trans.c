/* SceMtpIfDriver: privileged MTP transport and bounded user-buffer staging. */
#include <sysmem/sysclib.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <mtpif/mtpif.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/fast_mutex_object.h>
#include <intrmgr/syscall.h>
#include <mtpif/mtpif_driver.h>

#define MTP_INVALID	((int)0x80543603)
#define MTP_UNAVAILABLE	((int)0x8054360c)
#define MTP_OWNER	((int)0x80543602)
#define MTP_BUFFER_SIZE	0x10000

/* Opaque port identity; name unknown. */
static const struct {
	SceUInt32 id;
	const char *name;
} s_port = { 1, "USB" };

typedef struct MtpIfOperations {
	int (*reset)(void);
	int (*recvCommand)(void *, MtpIfTransfer *);
	int (*sendResponse)(const void *);
	int (*sendData)(MtpIfTransfer *);
	int (*recvData)(MtpIfTransfer *);
	int (*sendEvent)(void *);
	int (*cancel)(void);
	int (*waitConnect)(SceUInt32);
	int (*start)(void);
	int (*stop)(void);
	int (*isConnected)(void);
	int (*changePhase)(int);
} MtpIfOperations;

static struct {
	struct ThreadmgrFastMutex mutex;
	SceUID memblock;
	void *buffer;
	SceUID owner;
	SceUID handler;
	MtpIfOperations *ops;
	SceUInt32 reserved;
	MtpIfOperations usb;
	SceUInt32 configuredPort;
} s_mtp;

/* Process event callback names unknown. */
static int processNoop(void)
{
	return 0;
}

static int processEvent(SceUID pid, int event)
{
	if (s_mtp.owner == pid && event == 0x200)
		SceUsbMtpForDriver_08CD359F();
	return 0;
}

static int processExit(SceUID pid)
{
	if (s_mtp.owner == pid) {
		SceUsbMtpForDriver_08CD359F();
		s_mtp.owner = -1;
		SceUsbMtpForDriver_08CD359F();
	}
	return 0;
}

int _sceMtpIfModuleStart(SceSize arglen, const char *argp)
{
	SceUInt32 options[0x58 / 4];
	SceUInt32 handlers[7];
	int port = 2, offset = 0;
	const char *arg = argp;

	if ((int)arglen > 0) {
		while (1) {
			offset += strnlen(arg, 0x100) + 1;
			if ((int)arglen <= offset)
				break;
			arg = argp + offset;
			if (!strncmp(arg, "port=0", 6))
				port = 0;
			if (!strncmp(arg, "port=1", 6))
				port = 1;
			if (!strncmp(arg, "port=2", 6))
				port = 2;
			if (!strncmp(arg, "debug", 5))
				port = 2;
		}
	}
	sceKernelInitializeFastMutex(&s_mtp.mutex, "SceMtpIfData", 0x2002, 0);
	memset(options, 0, sizeof(options));
	options[0] = sizeof(options);
	options[2] = 0x200004;
	s_mtp.memblock = sceKernelAllocMemBlockForKernel("SceMtpIf", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_NC_RW,
					       MTP_BUFFER_SIZE, (void *)options);
	if (s_mtp.memblock < 0) {
		SCE_DBG_PRINTF(0, 246, "[%s%-29s] Err(%4d) sceKernelAllocMemBlock Fail\n",
			       "MTPIF:", __func__, 246);
		return 1;
	}
	if (sceKernelGetMemBlockBase(s_mtp.memblock, &s_mtp.buffer) < 0) {
		SCE_DBG_PRINTF(0, 252, "[%s%-29s] Err(%4d) cannot allocate memory\n",
			       "MTPIF:", __func__, 252);
		SceSysmemForDriver_009E1C61(s_mtp.memblock);
		return 1;
	}
	s_mtp.usb.reset = SceUsbMtpForDriver_B958E5DE;
	s_mtp.usb.recvCommand = SceUsbMtpForDriver_576A266F;
	s_mtp.usb.sendResponse = SceUsbMtpForDriver_4463009B;
	s_mtp.usb.sendData = sceUsbMtpSendData;
	s_mtp.usb.recvData = sceUsbMtpRecvData;
	s_mtp.usb.sendEvent = SceUsbMtpForDriver_5FA86BA4;
	s_mtp.usb.cancel = SceUsbMtpForDriver_151947D2;
	s_mtp.usb.waitConnect = SceUsbMtpForDriver_76D707B1;
	s_mtp.usb.start = sceUsbMtpStart;
	s_mtp.usb.stop = SceUsbMtpForDriver_08CD359F;
	s_mtp.usb.isConnected = SceUsbMtpForDriver_6F7DF4B8;
	s_mtp.usb.changePhase = SceUsbMtpForDriver_E64513A9;
	s_mtp.ops = &s_mtp.usb;
	handlers[0] = sizeof(handlers);
	handlers[1] = 0;
	handlers[2] = (SceUInt32)processExit;
	handlers[3] = (SceUInt32)processExit;
	handlers[4] = (SceUInt32)processEvent;
	handlers[5] = (SceUInt32)processNoop;
	handlers[6] = 0;
	s_mtp.owner = -1;
	s_mtp.configuredPort = port;
	s_mtp.handler = sceKernelRegisterProcEventHandler("SceMtpIf",
						       (void *)handlers, NULL);
	return s_mtp.handler < 0 ? 2 : 0;
}

int SceMtpIfForDriver_92CD107E(const void *port)
{
	if (port != &s_port)
		return MTP_INVALID;
	return s_mtp.ops && s_mtp.ops->reset ? s_mtp.ops->reset() : MTP_UNAVAILABLE;
}

int SceMtpIfForDriver_117F7519(const void *port, void *command)
{
	MtpIfTransfer transfer;

	transfer.buffer = s_mtp.buffer;
	if (port != &s_port || !command)
		return MTP_INVALID;
	return s_mtp.ops && s_mtp.ops->recvCommand ?
		s_mtp.ops->recvCommand(command, &transfer) : MTP_UNAVAILABLE;
}

int SceMtpIfForDriver_5766D257(const void *port, const void *response)
{
	if (port != &s_port || !response)
		return MTP_INVALID;
	return s_mtp.ops && s_mtp.ops->sendResponse ?
		s_mtp.ops->sendResponse(response) : MTP_UNAVAILABLE;
}

int SceMtpIfForDriver_EDC3AB26(const void *port, void *buffer,
			     SceUInt32 size, SceUInt32 totalSize)
{
	MtpIfTransfer transfer;
	int ret;

	if (port != &s_port || !buffer)
		return MTP_INVALID;
	if (!s_mtp.ops || !s_mtp.ops->sendData)
		return MTP_UNAVAILABLE;
	transfer.buffer = buffer;
	transfer.size = size;
	transfer.totalSize = totalSize;
	ret = s_mtp.ops->sendData(&transfer);
	return ret == 0 ? (int)transfer.transferred : ret;
}

int SceMtpIfForDriver_33F1826F(const void *port, MtpIfTransfer *transfer)
{
	if (port != &s_port || !transfer->buffer)
		return MTP_INVALID;
	return s_mtp.ops && s_mtp.ops->sendData ?
		s_mtp.ops->sendData(transfer) : MTP_UNAVAILABLE;
}

int SceMtpIfForDriver_F0FA2D86(const void *port, void *buffer, SceUInt32 size)
{
	MtpIfTransfer transfer;
	int ret;

	if (port != &s_port || !buffer)
		return MTP_INVALID;
	if (!s_mtp.ops || !s_mtp.ops->recvData)
		return MTP_UNAVAILABLE;
	transfer.buffer = buffer;
	transfer.size = size;
	ret = s_mtp.ops->recvData(&transfer);
	return ret == 0 ? (int)transfer.transferred : ret;
}

int SceMtpIfForDriver_27D40294(const void *port, MtpIfTransfer *transfer)
{
	if (port != &s_port || !transfer->buffer)
		return MTP_INVALID;
	return s_mtp.ops && s_mtp.ops->recvData ?
		s_mtp.ops->recvData(transfer) : MTP_UNAVAILABLE;
}

int SceMtpIfForDriver_B5E78858(const void *port, const SceUInt32 *command,
			     SceUInt32 *event)
{
	SceUInt32 count;

	if (port != &s_port || !command || !event || event[6] > 3)
		return MTP_INVALID;
	if (!s_mtp.ops || !s_mtp.ops->sendEvent)
		return MTP_UNAVAILABLE;
	count = event[6];
	event[1] = command[1];
	event[2] = command[2];
	if (count != 3)
		memset(event + count + 3, 0, (3 - count) * 4);
	return s_mtp.ops->sendEvent(event);
}

int SceMtpIfForDriver_F1EC5992(const void *port)
{
	if (port != &s_port)
		return MTP_INVALID;
	return s_mtp.ops && s_mtp.ops->cancel ? s_mtp.ops->cancel() : MTP_UNAVAILABLE;
}

int SceMtpIfForDriver_05408B5B(const void *port, SceUInt32 timeout)
{
	if (port != &s_port)
		return MTP_INVALID;
	return s_mtp.ops && s_mtp.ops->waitConnect ?
		s_mtp.ops->waitConnect(timeout) : MTP_UNAVAILABLE;
}

int SceMtpIfForDriver_2D9318E1(const void *port)
{
	if (port != &s_port)
		return MTP_INVALID;
	return s_mtp.ops && s_mtp.ops->isConnected ?
		s_mtp.ops->isConnected() & 1 : MTP_UNAVAILABLE;
}

int SceMtpIfForDriver_8D16B997(const void *port, int phase)
{
	if (port != &s_port)
		return MTP_INVALID;
	return s_mtp.ops && s_mtp.ops->changePhase ?
		s_mtp.ops->changePhase(phase) : MTP_UNAVAILABLE;
}

const void *SceMtpIfForDriver_16D7DBCC(int port)
{
	return port == 1 ? &s_port : NULL;
}

int SceMtpIfForDriver_5245D237(const void *port)
{
	SceUID pid = s_mtp.owner;

	if (sceKernelIsUserModeThread()) {
		pid = sceKernelGetProcessId();
		if (s_mtp.owner != -1 && pid != s_mtp.owner)
			return MTP_OWNER;
	}
	s_mtp.owner = pid;
	if (port != &s_port)
		return MTP_INVALID;
	return s_mtp.ops && s_mtp.ops->start ? s_mtp.ops->start() : sceUsbMtpStart();
}

int SceMtpIfForDriver_E6453F21(const void *port)
{
	if (sceKernelIsUserModeThread()) {
		if (s_mtp.owner == -1)
			return MTP_OWNER;
		s_mtp.owner = -1;
	}
	if (port != &s_port)
		return MTP_INVALID;
	return s_mtp.ops && s_mtp.ops->stop ?
		s_mtp.ops->stop() : SceUsbMtpForDriver_08CD359F();
}

int SceMtpIfForDriver_CB685AC0(int port)
{
	return port == 1 ? sceUsbMtpStart() : MTP_INVALID;
}

int SceMtpIfForDriver_FDF8C3C0(int port)
{
	return port == 1 ? SceUsbMtpForDriver_08CD359F() : MTP_INVALID;
}

/* Enter/leave the kernel syscall domain (TPIDRPRW). */
static inline int leaveSyscall(SceUInt32 saved, int ret)
{
	sceSyscallLeave(saved);
	return ret;
}

#define MTP_SYSCALL(name, driver) \
int name(const void *port) \
{ \
	SceUInt32 saved = sceSyscallEnter(); \
	int ret = SCE_ERROR_ERRNO_ENOSYS; \
	if (sceSblACMgrIsSystemProgram(0)) \
		ret = driver(port); \
	return leaveSyscall(saved, ret); \
}

MTP_SYSCALL(sceMtpIfReset, SceMtpIfForDriver_92CD107E)
MTP_SYSCALL(sceMtpIfCancelTransfer, SceMtpIfForDriver_F1EC5992)
MTP_SYSCALL(sceMtpIfIsConnected, SceMtpIfForDriver_2D9318E1)
MTP_SYSCALL(sceMtpIfStartPort, SceMtpIfForDriver_5245D237)
MTP_SYSCALL(sceMtpIfStopPort, SceMtpIfForDriver_E6453F21)

int sceMtpIfWaitConnect(const void *port, SceUInt32 timeout)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = (int)0x80543606;

	if (!SceThreadmgrForDriver_20C228E4()) {
		ret = SCE_ERROR_ERRNO_ENOSYS;
		if (sceSblACMgrIsSystemProgram(0))
			ret = SceMtpIfForDriver_05408B5B(port, timeout);
	}
	return leaveSyscall(saved, ret);
}

int sceMtpIfChangePhase(const void *port, int phase)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = SCE_ERROR_ERRNO_ENOSYS;

	if (sceSblACMgrIsSystemProgram(0))
		ret = SceMtpIfForDriver_8D16B997(port, phase);
	return leaveSyscall(saved, ret);
}

int sceMtpIfStartDriver(int port)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = SCE_ERROR_ERRNO_ENOSYS;

	if (sceSblACMgrIsSystemProgram(0) || SceSblACMgrForDriver_D0E11C89(0))
		ret = SceMtpIfForDriver_CB685AC0(port);
	return leaveSyscall(saved, ret);
}

int sceMtpIfStopDriver(int port)
{
	SceUInt32 saved = sceSyscallEnter();
	int ret = SCE_ERROR_ERRNO_ENOSYS;

	if (sceSblACMgrIsSystemProgram(0) || SceSblACMgrForDriver_D0E11C89(0))
		ret = SceMtpIfForDriver_FDF8C3C0(port);
	return leaveSyscall(saved, ret);
}

const void *sceMtpIfGetPort(int port)
{
	SceUInt32 saved = sceSyscallEnter();
	const void *ret = NULL;

	sceKernelApiDeclareTypeUInt32();
	if (sceSblACMgrIsSystemProgram(0))
		ret = SceMtpIfForDriver_16D7DBCC(port);
	leaveSyscall(saved, 0);
	return ret;
}

int sceMtpIfRecvCommand(const void *port, void *command)
{
	SceUInt32 saved = sceSyscallEnter();
	SceUInt32 local[9];
	int ret = SCE_ERROR_ERRNO_ENOSYS, copy;

	if (sceSblACMgrIsSystemProgram(0)) {
		ret = SceMtpIfForDriver_117F7519(port, local);
		if (ret >= 0) {
			copy = sceKernelCopyToUser(command, local, sizeof(local));
			if (copy < 0)
				ret = copy;
		}
	}
	return leaveSyscall(saved, ret);
}

int sceMtpIfSendResponse(const void *port, const void *response)
{
	SceUInt32 saved = sceSyscallEnter();
	SceUInt32 local[9];
	int ret = SCE_ERROR_ERRNO_ENOSYS;

	if (sceSblACMgrIsSystemProgram(0)) {
		ret = lockFastMutex(&s_mtp.mutex);
		if (ret >= 0) {
			ret = sceKernelCopyFromUser(local, response, sizeof(local));
			if (ret >= 0)
				ret = SceMtpIfForDriver_5766D257(port, local);
			sceKernelUnlockFastMutex(&s_mtp.mutex);
		}
	}
	return leaveSyscall(saved, ret);
}

int sceMtpIfSendDataWithParam(const void *port, MtpIfTransfer *params)
{
	SceUInt32 saved = sceSyscallEnter();
	MtpIfTransfer local;
	int ret = SCE_ERROR_ERRNO_ENOSYS, copy;

	if (!sceSblACMgrIsSystemProgram(0))
		goto out;
	ret = lockFastMutex(&s_mtp.mutex);
	if (ret < 0)
		goto out;
	ret = sceKernelCopyFromUser(&local, params, sizeof(local));
	if (ret < 0)
		goto unlock;
	if (local.size > MTP_BUFFER_SIZE) {
		ret = MTP_INVALID;
		goto unlock;
	}
	ret = sceKernelCopyFromUser(s_mtp.buffer, local.buffer,
				   (SceSize)local.size);
	if (ret < 0)
		goto unlock;
	local.buffer = s_mtp.buffer;
	ret = SceMtpIfForDriver_33F1826F(port, &local);
	sceKernelUnlockFastMutex(&s_mtp.mutex);
	if (ret >= 0) {
		copy = sceKernelCopyToUser(params, &local, sizeof(local));
		if (copy < 0)
			ret = copy;
	}
	goto out;
unlock:
	sceKernelUnlockFastMutex(&s_mtp.mutex);
out:
	return leaveSyscall(saved, ret);
}

int sceMtpIfRecvDataWithParam(const void *port, MtpIfTransfer *params)
{
	SceUInt32 saved = sceSyscallEnter();
	MtpIfTransfer local, transfer;
	int ret = SCE_ERROR_ERRNO_ENOSYS, copy;

	if (!sceSblACMgrIsSystemProgram(0))
		goto out;
	ret = lockFastMutex(&s_mtp.mutex);
	if (ret < 0)
		goto out;
	ret = sceKernelCopyFromUser(&local, params, sizeof(local));
	if (ret < 0)
		goto unlock;
	if (local.size > MTP_BUFFER_SIZE) {
		ret = MTP_INVALID;
		goto unlock;
	}
	transfer.buffer = s_mtp.buffer;
	transfer.size = local.size;
	ret = SceMtpIfForDriver_27D40294(port, &transfer);
	if (ret < 0)
		goto unlock;
	local.totalSize = transfer.totalSize;
	local.transferred = transfer.transferred;
	if (transfer.transferred > transfer.size) {
		ret = (int)0x805436ff;
		goto unlock;
	}
	copy = sceKernelCopyToUser(local.buffer, s_mtp.buffer,
				  (SceSize)transfer.transferred);
	if (copy < 0) {
		ret = copy;
		goto unlock;
	}
	sceKernelUnlockFastMutex(&s_mtp.mutex);
	copy = sceKernelCopyToUser(params, &local, sizeof(local));
	if (copy < 0)
		ret = copy;
	goto out;
unlock:
	sceKernelUnlockFastMutex(&s_mtp.mutex);
out:
	return leaveSyscall(saved, ret);
}

int sceMtpIfSendEvent(const void *port, const void *command, const void *event)
{
	SceUInt32 saved = sceSyscallEnter();
	SceUInt32 localCommand[9], localEvent[7];
	int ret = SCE_ERROR_ERRNO_ENOSYS;

	if (sceSblACMgrIsSystemProgram(0)) {
		ret = sceKernelCopyFromUser(localCommand, command,
					     sizeof(localCommand));
		if (ret >= 0) {
			ret = sceKernelCopyFromUser(localEvent, event,
						     sizeof(localEvent));
			if (ret >= 0)
				ret = SceMtpIfForDriver_B5E78858(port, localCommand, localEvent);
		}
	}
	return leaveSyscall(saved, ret);
}
