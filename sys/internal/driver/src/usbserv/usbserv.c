/* SceUsbServ: USB port selection, accessory power and suspend/resume service. */
#include "usbserv.h"
#include <intrmgr/syscall.h>
#include <sysmem/sysevent_ids.h>
#include <fw.h>

#define SERV_ERROR(name) ((int)SCE_USBSERV_ERROR_##name)
#if FW_DEBUG
#define DEBUG(flags, line, func, fmt, ...) \
	SceDebugForDriver_D9703808(1, flags, SCE_DBGINFO_FUNC(line, func), fmt, ##__VA_ARGS__)
#else
#define DEBUG(flags, line, func, fmt, ...) do { } while (0)
#endif
#define ERROR(line, func) \
	SceDebugForDriver_02B04343(15, SCE_DBGINFO_FUNC(line, func), "### error ###\n")

#define OWNER_MAX 8

static int usb_probe(int id);
static int usb_attach(int id);
static int usb_detach(int id);
static inline __attribute__((always_inline)) void lock(void);
static inline __attribute__((always_inline)) void unlock(void);

/* Private state names unknown unless recovered from debug records. */
static SceUID s_timer = -1;
static SceUsbdDriver s_driver = { "usb_charge", usb_probe, usb_attach, usb_detach };
static int s_etherSuspend = 1;
static int s_hostActive;
static int s_accessory;
static int s_ether;
static SceUID s_mutex;
static struct { int count; SceUID process; } s_owner[OWNER_MAX];
static SceUID s_thread;
static SceUID s_systemHandler;
static SceUID s_event;
static int s_mode[3];
static int s_attached;
static SceUID s_processHandler;
static int s_resumed;

static inline __attribute__((always_inline)) int used(void)
{
	if (s_owner[0].count)
		return 1;
	for (int i = 1; i < OWNER_MAX; i++)
		if (s_owner[i].process != -1 && s_owner[i].count)
			return 1;
	return 0;
}
static inline __attribute__((always_inline)) int usbEnabled(unsigned port, unsigned mode)
{
	ScePervasiveForDriver_4AF7A01E(port, 11);
	ScePervasiveForDriver_AD1E81EB(port, 11);
	ScePervasiveForDriver_6B2BE37E(port, mode);
	ScePervasiveForDriver_A2EFD7AF(port, mode ? 10 : 9);
	ScePervasiveForDriver_13CC07C9(port, mode ? 10 : 9);
	for (unsigned i = 0; i < 2000; i++)
		if (!SceUdcdForDriver_1C684884(500, port))
			return 1;
	SceDebugForDriver_02B04343(6, SCE_DBGINFO_FUNC(207, "usbEnabled"),
		"timeout: USB PHY ready(ch=%d sel=%d)\n", port, mode);
	return 0;
}
static inline int usb0MacSelect(int mode, int action)
{
	int ret = 0;
	if (mode) {
		if (mode != 1)
			return SERV_ERROR(INVALID_PARAM);
		if (!s_mode[0]) {
			ret = sceUsbdHostStop(0);
			if (ret < 0)
				DEBUG(6, 333, "usb0MacSelect", "# ERROR # sceUsbdHostStop() ret=%08x\n", ret);
		}
		ScePervasiveForDriver_4F64DEF6(0);
		return ret;
	}
	ScePervasiveForDriver_BB67EF58(0);
	if (!usbEnabled(0, 0))
		return SERV_ERROR(FATAL);
	s_mode[0] = 0;
	if (action == 2) {
		ret = sceUsbdResume(0);
		if (ret < 0)
			DEBUG(6, 357, "usb0MacSelect", "# ERROR # sceUsbdResume() ret=%08x\n", ret);
	} else {
		ret = sceUsbdHostStart(0);
		if (ret < 0)
			DEBUG(6, 362, "usb0MacSelect", "# ERROR # sceUsbdHostStart() ret=%08x\n", ret);
	}
	return ret;
}
/*
 * Sony's compiler split usb1MacSelect and usb2MacSelect into a host (mode 0) and a
 * device (mode 1) half and inlined each half differently in each build: the halves
 * are written once and the build's out-of-line copies wrap them.
 */
static inline __attribute__((always_inline)) int usb1Host(int action)
{
	int ret = sceUdcdEnd(1);
	if (ret < 0 && ret != (int)0x80243002)
		DEBUG(6, 411, "usb1MacSelect", "# ERROR # sceUdcdEnd() ret=%08x\n", ret);
	ScePwmForDriver_13161CCC(1, 0x200, 0, 0);
	if (!usbEnabled(1, 0))
		return SERV_ERROR(FATAL);
	s_mode[1] = 0;
	return action == 2 ? sceUsbdResume(1) : sceUsbdHostStart(1);
}
static inline __attribute__((always_inline)) int usb1Device(void)
{
	int ret;
	if (!s_mode[1]) {
		ret = sceUsbdHostStop(1);
		if (ret < 0)
			DEBUG(6, 388, "usb1MacSelect", "# ERROR # sceUsbdHostStop() ret=%08x\n", ret);
	}
	ScePwmForDriver_13161CCC(1, 0, 0, 0);
	if (!usbEnabled(1, 1))
		return SERV_ERROR(FATAL);
	s_mode[1] = 1;
	ret = sceUdcdInit(1);
	if (ret < 0)
		DEBUG(6, 401, "usb1MacSelect", "# ERROR # sceUdcdInit() ret=%08x\n", ret);
	return ret;
}
#if FW_SINCE(0x04000000)
static int __attribute__((noinline)) usb1MacSelectDevice(void)
{
	return usb1Device();
}
#else
#define usb1MacSelectDevice usb1Device
#endif
static int __attribute__((noinline, noclone)) usb1MacSelect(int mode, int action)
{
	if (!mode)
		return usb1Host(action);
	if (mode != 1)
		return SERV_ERROR(INVALID_PARAM);
	return usb1MacSelectDevice();
}
static int port2Host(void)
{
	unsigned info;
	if (SceSblAIMgrForDriver_C6E83F34())
		return s_ether != 0;
	unsigned hardware = SceSysconForDriver_CBD6D8BC() & 0xff0000;
	if (hardware <= 0x600000) {
		sceSysconGetMultiCnInfo(&info);
		return !(info & 0xfb00);
	}
	if (hardware >= 0x800000) {
		SceSysconForDriver_D6F6D472(&info);
		return (info & 0xff00) == 0x200;
	}
	return 0;
}
static inline __attribute__((always_inline)) int usb2Host(int action)
{
	int ret;
	if (s_mode[2] == 1) {
		ret = sceUdcdEnd(2);
		if (ret < 0)
			DEBUG(6, 489, "usb2MacSelect", "# ERROR # sceUdcdEnd() ret=%08x\n", ret);
	}
	if (!usbEnabled(2, 0))
		return SERV_ERROR(FATAL);
	sceGpioPortSet(1, 3);
	s_mode[2] = 0;
	if ((SceSysconForDriver_CBD6D8BC() & 0xfd0000) == 0x800000)
		ScePervasiveForDriver_29E65AE1(2, 0, 0);
	return action == 2 ? sceUsbdResume(2) : sceUsbdHostStart(2);
}
static inline __attribute__((always_inline)) int usb2Device(void)
{
	int ret;
	/* Genuine Dolce is excluded from the physical connector policy here. */
	if (!SceSblAIMgrForDriver_C6E83F34()) {
		unsigned info;
		unsigned hardware = SceSysconForDriver_CBD6D8BC() & 0xff0000;
		int host = 0;
		if (hardware <= 0x600000) {
			sceSysconGetMultiCnInfo(&info);
			host = !(info & 0xfb00);
		} else if (hardware >= 0x800000) {
			SceSysconForDriver_D6F6D472(&info);
			host = (info & 0xff00) == 0x200;
		}
		if (host) {
			DEBUG(15, 452, "usb2MacSelect", "USB Port#2 is operating as Host mode.\n");
			return SERV_ERROR(NOT_SUPPORTED);
		}
	}
	if (!s_mode[2]) {
		ret = sceUsbdHostStop(2);
		if (ret < 0)
			DEBUG(6, 462, "usb2MacSelect", "# ERROR # sceUsbdHostStop() ret=%08x\n", ret);
	}
	if (!usbEnabled(2, 1))
		return SERV_ERROR(FATAL);
	sceGpioPortClear(1, 3);
	s_mode[2] = 1;
	if ((SceSysconForDriver_CBD6D8BC() & 0xfd0000) == 0x800000)
		ScePervasiveForDriver_29E65AE1(2, 0x80c47064, 0x18);
	ret = sceUdcdInit(2);
	if (ret < 0)
		DEBUG(6, 478, "usb2MacSelect", "# ERROR # sceUdcdInit() ret=%08x\n", ret);
	return ret;
}
static int __attribute__((noinline, noclone)) usb2MacSelectHost(int action)
{
	return usb2Host(action);
}
#if FW_SINCE(0x04000000)
#define usb2MacSelectDevice usb2Device
static int __attribute__((noinline, noclone)) usb2MacSelect(int mode, int action)
#else
static int __attribute__((noinline)) usb2MacSelectDevice(void)
{
	return usb2Device();
}
static inline __attribute__((always_inline)) int usb2MacSelect(int mode, int action)
#endif
{
	if (!mode)
		return usb2Host(action);
	if (mode != 1)
		return SERV_ERROR(INVALID_PARAM);
	return usb2Device();
}
static void accessoryChanged(int info, void *arg)
{
	sceKernelSetEventFlag(s_event, info ? 8 : 16);
}
static void microUsbChanged(int info, void *arg)
{
	sceKernelSetEventFlag(s_event, (info & 0xff00) == 0x200 ? 64 : 128);
}
static void multiCnChanged(int info, void *arg)
{
	sceKernelSetEventFlag(s_event, info & 0xfb00 ? 4 : 2);
}
static inline __attribute__((always_inline)) void registerConnectors(void)
{
	SceSysconForDriver_A26586B2(multiCnChanged, NULL);
	SceSysconForDriver_67A4CB9F(microUsbChanged, NULL);
	SceSysconForDriver_4A42712F(accessoryChanged, NULL);
}
static inline __attribute__((always_inline)) void devKitPower(unsigned enable)
{
	unsigned info;
	unsigned hardware = SceSysconForDriver_CBD6D8BC() & 0xff0000;
	sceSysconGetMultiCnInfo(&info);
	if ((info & 0xff00) != 0x200 && hardware <= 0x600000)
		SceSysconForDriver_B1F88B11(enable);
}
static int _sceUsbServSysEventHandler(int phase, int event, void *arg, void *opt)
{
	if (!phase) {
		if (event == SYSEVENT_SUSPEND_PREPARE(0)) {
			int ret = SceUdcdForDriver_9E7F7EFE();
			if (SceSblAIMgrForDriver_71608CA3() && !s_mode[0]) {
				ret = sceUsbdSuspendPhase2(0, 1);
				if (ret)
					SceDebugForDriver_02B04343(6, SCE_DBGINFO_FUNC(728, "_sceUsbServSysEventHandler"),
						"Fail: sceUsbdSuspendPhase2(%08x)\n", ret);
			}
			return ret;
		}
		if (event == SYSEVENT_SUSPEND_DEVICE(14)) {
			SceSysconForDriver_A26586B2(NULL, NULL);
			SceSysconForDriver_67A4CB9F(NULL, NULL);
			SceSysconForDriver_4A42712F(NULL, NULL);
			SceUdcdForDriver_D01CFD22();
			for (unsigned i = 0; i < 3; i++) {
				if (!s_mode[i])
					sceUsbdSuspend(i);
				ScePervasiveForDriver_4F64DEF6(i);
				s_mode[i] = 1;
			}
			s_hostActive = 0;
			sceSysconCtrlAccPower(0);
			devKitPower(0);
			if (SceSblAIMgrForDriver_C6E83F34() && s_etherSuspend)
				SceSysconForDriver_3274A925(0);
			sceGpioPortClear(1, 3);
		}
	} else if (phase == 1) {
		if (event == SYSEVENT_RESUME_START) {
			s_resumed = 0;
			return sceKernelSetEventFlag(s_event, 32);
		}
		if (event == SYSEVENT_RESUME_IDLE_QUERY)
			return s_resumed ? 0 : -1;
	}
	return 0;
}
static inline __attribute__((always_inline)) int cleanupProcess(int process)
{
	if (!used())
		return 0;
	if (sceKernelIsUserModeThread())
		for (int i = 1; i < OWNER_MAX; i++)
			if (s_owner[i].process != -1 && s_owner[i].process == process) {
				s_owner[i].process = -1;
				s_owner[i].count--;
			}
	if (!used())
		sceSysconCtrlAccPower(0);
	return 0;
}
static int processExit(SceUID pid, ProcEventParam *param, void *common)
{
	return cleanupProcess(pid);
}
static int processKill(SceUID pid, ProcEventParam *param, void *common)
{
	return cleanupProcess(pid);
}
static int processSuspend(SceUID pid, int phase, ProcEventParam *param, void *common)
{
	return 0;
}
static int processResume(SceUID pid, int phase, ProcEventParam *param, void *common)
{
	return 0;
}

static int UsbServThread(unsigned arglen, void *arg)
{
	for (int i = 0; i < OWNER_MAX; i++) {
		s_owner[i].count = 0;
		s_owner[i].process = -1;
	}
	sceSysconCtrlAccPower(0);
	if (SceSblAIMgrForDriver_C6E83F34()) {
		SceSysconForDriver_3274A925(1);
		s_ether = 1;
	}
	s_accessory = SceSysconForDriver_B9EA2FA8();
	if (s_accessory && used())
		usb0MacSelect(0, 1);
	if (SceSblAIMgrForDriver_71608CA3()) {
		s_accessory = 1;
		if (used())
			sceSysconCtrlAccPower(1);
	}
	if (SceSysconForDriver_CBD6D8BC() & 2)
		usb1MacSelect(0, 1);
	if (port2Host() || (SceSysconForDriver_CBD6D8BC() & 0xff0000) <= 0x100000) {
		usb2MacSelectHost(1);
		devKitPower(1);
		s_hostActive = 1;
	}
	registerConnectors();
	for (;;) {
		unsigned flags;
		int ret = sceKernelWaitEventFlag(s_event, 0xfe,
						  SCE_KERNEL_EVF_WAITMODE_OR |
						  SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &flags, NULL);
		if (ret < 0) {
			DEBUG(6, 877, "UsbServThread", "# ERROR # sceKernelWaitEventFlag 0x%08x\n", ret);
			continue;
		}
		if (flags & 0xc6) {
			if (port2Host()) {
				if (!s_hostActive) {
					s_hostActive = 1;
					usb2MacSelectHost(1);
					devKitPower(1);
				}
			} else if (s_hostActive) {
				s_hostActive = 0;
				devKitPower(0);
#if FW_SINCE(0x04000000)
				usb2MacSelect(1, 1);
#else
				usb2MacSelectDevice();
#endif
			}
		}
		if (flags & 0x18) {
			if (flags & 8) {
				s_accessory = 1;
				if (used())
					sceSysconCtrlAccPower(1);
			} else {
				s_accessory = 0;
				sceSysconCtrlAccPower(0);
			}
		}
		if (!(flags & 0x20))
			continue;
		SceGpioForDriver_372022A4(1, 3, 0);
		sceGpioPortClear(1, 3);
		SceUdcdForDriver_1A5A3851();
		if (SceSblAIMgrForDriver_C6E83F34()) {
			lock();
			SceSysconForDriver_3274A925(1);
			s_ether = 1;
			unlock();
		}
		ScePervasiveForDriver_BB67EF58(0);
		usb0MacSelect(0, 2);
		s_accessory = SceSysconForDriver_B9EA2FA8();
		if (SceSblAIMgrForDriver_71608CA3()) {
			s_accessory = 1;
			if (used())
				sceSysconCtrlAccPower(1);
		}
		if (s_accessory && used())
			sceSysconCtrlAccPower(1);
		else
			usb0MacSelect(1, 1);
		if (SceModulemgrForDriver_BBE1771C("SceBbmc") >= 0) {
			ScePervasiveForDriver_BB67EF58(1);
#if FW_SINCE(0x04000000)
			usb1MacSelect(0, 2);
#else
			usb1Host(2);
#endif
		} else {
			usb1MacSelectDevice();
		}
		usb2MacSelectHost(2);
		if (port2Host()) {
			devKitPower(1);
			s_hostActive = 1;
		} else {
			usb2MacSelectDevice();
		}
		if (SceSblAIMgrForDriver_C6E83F34())
			s_etherSuspend = 1;
		registerConnectors();
		s_resumed = 1;
	}
	__builtin_unreachable();
}
int _sceUsbServInit(unsigned arglen, const void *argp)
{
	static const SceKernelProcEventHandler events = {
		.size = sizeof(SceKernelProcEventHandler),
		.create = NULL,
		.exit = processExit,
		.kill = processKill,
		.suspend = processSuspend,
		.resume = processResume,
		.notify = NULL,
	};
	SceGpioForDriver_372022A4(1, 3, 0);
	sceGpioPortClear(1, 3);
	for (unsigned i = 0; i < 3; i++)
		ScePervasiveForDriver_4F64DEF6(i);
	for (unsigned i = 0; i < 3; i++)
		s_mode[i] = 1;
	ScePervasiveForDriver_BB67EF58(1);
	usb1MacSelectDevice();
	ScePervasiveForDriver_BB67EF58(2);
	usb2MacSelectDevice();
	SceKernelMutexOptParam option;
	option.size = sizeof(option);
	option.ceilingPriority = 32;
	int ret = sceKernelCreateMutex("SceUsbServ", 6, 0, &option);
	if (ret < 0) {
		DEBUG(6, 1056, "_sceUsbServInit", "# ERROR # sceKernelCreateMutex 0x%08x\n", ret);
		return 1;
	}
	s_mutex = ret;
	s_event = sceKernelCreateEventFlag("SceUsbserv", SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (s_event < 0) {
		DEBUG(6, 1064, "_sceUsbServInit", "# ERROR # sceKernelCreateEventFlag 0x%08x\n", s_event);
		goto deleteMutex;
	}
	s_thread = sceKernelCreateThread("SceUsbServ", UsbServThread, 60, 0x2000, 0, 0x80000, NULL);
	if (s_thread < 0) {
		DEBUG(6, 1073, "_sceUsbServInit", "# ERROR # sceKernelCreateThread 0x%08x\n", s_thread);
		goto deleteEvent;
	}
	sceKernelStartThread(s_thread, 0, NULL);
	s_systemHandler = sceKernelRegisterSysEventHandler("SceUsbServ", _sceUsbServSysEventHandler, NULL);
	if (s_systemHandler < 0) {
		DEBUG(6, 1083, "_sceUsbServInit", "# ERROR # sceKernelRegisterSysEventHandler 0x%08x\n", s_systemHandler);
		goto deleteThread;
	}
	s_processHandler = sceKernelRegisterProcEventHandler("SceUsbserv", &events, NULL);
	if (s_processHandler < 0) {
		DEBUG(6, 1090, "_sceUsbServInit", "# ERROR # sceKernelRegisterProcEventHandler 0x%08x\n", s_processHandler);
		sceKernelUnregisterSysEventHandler(s_systemHandler);
		goto deleteThread;
	}
	if (SceSblAIMgrForDriver_71608CA3())
		SceUsbdForDriver_1EC94F18(&s_driver);
	return 0;
deleteThread:
	sceKernelDeleteThread(s_thread);
deleteEvent:
	sceKernelDeleteEventFlag(s_event);
deleteMutex:
	sceKernelDeleteMutex(s_mutex);
	return 1;
}
int sceUsbServMacSelect(SceUInt port, SceBool mode)
{
	if ((unsigned)mode > 1)
		return SERV_ERROR(INVALID_PARAM);
	switch (port) {
	case 0:
		if (s_mode[0] == mode)
			return 0;
		return usb0MacSelect(mode, 1);
	case 1:
		if (s_mode[1] == mode)
			return 0;
		return usb1MacSelect(mode, 1);
	case 2:
		if (s_mode[2] == mode)
			return 0;
		return usb2MacSelect(mode, 1);
	}
	return SERV_ERROR(INVALID_PARAM);
}
SceBool SceUsbServForDriver_F0553A69(SceUInt port)
{
	return port < 3 ? s_mode[port] : SERV_ERROR(INVALID_PARAM);
}
int sceUsbServAccessoryActivate(void)
{
	int ret = 0;
	lock();
	if (s_accessory && !used()) {
		usb0MacSelect(0, 1);
		sceSysconCtrlAccPower(1);
	}
	if (!sceKernelIsUserModeThread()) {
		s_owner[0].count++;
		goto out;
	}
	int pid = sceKernelGetProcessId();
	if (pid >= 0) {
		for (int i = 1; i < OWNER_MAX; i++)
			if (s_owner[i].process == pid) {
				s_owner[i].count++;
				goto out;
			}
		for (int i = 1; i < OWNER_MAX; i++)
			if (s_owner[i].process == -1) {
				s_owner[i].count++;
				s_owner[i].process = pid;
				goto out;
			}
	}
	ret = SERV_ERROR(FATAL);
out:
	unlock();
	return ret;
}
int sceUsbServAccessoryDeactivate(void)
{
	lock();
	if (!sceKernelIsUserModeThread()) {
		if (s_owner[0].count > 1)
			s_owner[0].count--;
		else
			s_owner[0].count = 0;
	} else {
		int pid = sceKernelGetProcessId();
		unsigned i;
		if (pid < 0)
			goto fail;
		for (i = 1; i < OWNER_MAX; i++)
			if (s_owner[i].process == pid && --s_owner[i].count <= 0)
				break;
		if (i == OWNER_MAX)
			goto fail;
		s_owner[i].count = 0;
		s_owner[i].process = -1;
	}
	if (!used()) {
		usb0MacSelect(1, 1);
		sceSysconCtrlAccPower(0);
	}
	unlock();
	return 0;
fail:
	unlock();
	return SERV_ERROR(FATAL);
}
int sceUsbServAccessoryActivateForUser(void)
{
	unsigned domain = sceSyscallEnter();
	lock();
	int ret = sceUsbServAccessoryActivate();
	unlock();
	sceSyscallLeave(domain);
	return ret;
}
int sceUsbServAccessoryDeactivateForUser(void)
{
	unsigned domain = sceSyscallEnter();
	lock();
	int ret = sceUsbServAccessoryDeactivate();
	unlock();
	sceSyscallLeave(domain);
	return ret;
}
int sceUsbServEtherEnable(void)
{
	if (!SceSblAIMgrForDriver_C6E83F34()) {
		DEBUG(15, 1224, "sceUsbServEtherEnable", "Not supported\n");
		return SERV_ERROR(UNAVAILABLE);
	}
	lock();
	if (!s_ether) {
		SceSysconForDriver_3274A925(1);
		s_ether = 1;
		sceKernelSetEventFlag(s_event, 2);
		DEBUG(6, 1239, "sceUsbServEtherEnable", "USB Ether is just enabled.\n");
	} else {
		DEBUG(6, 1241, "sceUsbServEtherEnable", "USB Ether is already enabled.\n");
	}
	unlock();
	return 0;
}
int sceUsbServEtherDisable(void)
{
	if (!SceSblAIMgrForDriver_C6E83F34()) {
		DEBUG(15, 1253, "sceUsbServEtherDisable", "Not supported\n");
		return SERV_ERROR(UNAVAILABLE);
	}
	lock();
	if (s_ether) {
		SceSysconForDriver_3274A925(0);
		s_ether = 0;
		sceKernelSetEventFlag(s_event, 4);
		DEBUG(6, 1268, "sceUsbServEtherDisable", "USB Ether is just disabled.\n");
	} else {
		DEBUG(6, 1270, "sceUsbServEtherDisable", "USB Ether is already disabled.\n");
	}
	unlock();
	return 0;
}
int SceUsbServForDriver_6D738018(void)
{
	if (!SceSblAIMgrForDriver_C6E83F34())
		return SERV_ERROR(UNAVAILABLE);
	s_etherSuspend = 0;
	return 0;
}

static inline __attribute__((always_inline)) int is_ds(unsigned id)
{
	uint16_t *dev = SceUsbdForDriver_BC3EF82B(id, NULL, 1);
	if (!dev) { ERROR(1389, "is_ds"); return 0; }
	return dev[4] == 0x54c && (dev[5] == 0x42f || dev[5] == 0x268 || dev[5] == 0x5c4);
}
/* The 3.6x source declares it inline; 4.00, an older build, does not. */
#if FW_SINCE(0x04000000)
static unsigned is_supported_device(unsigned id)
#else
static inline unsigned is_supported_device(unsigned id)
#endif
{
	uint8_t *dev = SceUsbdForDriver_BC3EF82B(id, NULL, 1);
	if (!dev) { ERROR(1413, "is_supported_device"); return 0; }
	if (is_ds(id))
		return 0;
	uint16_t *words = (void *)dev;
	if (words[4] == 0x54c && (words[5] == 0x69b || words[5] == 0x4e4 || words[5] == 0x1cb))
		return 255;
	int audio = 0, hid = 0;
	uint8_t *configuration = dev;
	for (int i = 0; i < dev[17]; i++) {
		configuration = SceUsbdForDriver_BC3EF82B(id, configuration, 2);
		if (!configuration) { ERROR(1431, "is_supported_device"); return 0; }
		uint8_t *interface = configuration;
		while ((interface = SceUsbdForDriver_BC3EF82B(id, interface, 4))) {
			unsigned type = interface[5];
			if (type == 1) {
				audio = 1;
			} else if (type == 3) {
				hid = 1;
			} else if (type == 6) {
				return type;
			} else if (type == 8) {
				if (sceKernelSysrootIsSafeMode())
					return 0;
				return type;
			}
		}
	}
	return !audio && hid ? 3 : 0;
}
static SceUInt32 tickTimer(void *arg)
{
	s_timer = -1;
	if (s_attached > 0) {
		sceKernelPowerTick(1);
		s_timer = SceThreadmgrForDriver_C58DF384("SceUsbservTickTimer", 5000000, tickTimer, NULL);
	}
	return 0;
}
static void completeCharge(int result, int length, void *arg)
{
}
static int usb_probe(int id)
{
	if (is_supported_device(id)) {
		DEBUG(6, 1464, "usb_probe", "[%s] USB Probe (MTP/HID/Storage) <----- dev_id=%d\n", "usb_probe", id);
		return 0;
	}
	if (is_ds(id))
		DEBUG(6, 1469, "usb_probe", "[%s] USB Probe (DS3/Navicon/DS4) <----- dev_id=%d\n", "usb_probe", id);
	return -1;
}
static int usb_attach(int id)
{
	if (is_ds(id)) {
		DEBUG(6, 1483, "usb_attach", "[%s] USB Attach (DS3/Navicon) <----- dev_id=%d, attached_device[%d]\n", "usb_attach", id, s_attached);
		return -1;
	}
	unsigned type = is_supported_device(id);
	if (!type)
		return -1;
	if (s_timer < 1)
		s_timer = SceThreadmgrForDriver_C58DF384("SceUsbservTickTimer", 0, tickTimer, NULL);
	uint8_t *configuration = SceUsbdForDriver_BC3EF82B(id, NULL, 2);
	if (!configuration) { ERROR(1494, "usb_attach"); return -1; }
	int pipe = SceUsbdForDriver_1CDBFF9F(id, NULL);
	if (pipe < 0) { ERROR(1499, "usb_attach"); return -1; }
	SceUsbdDeviceRequest request = { 0, 9, configuration[5], 0, 0 };
	if (SceUsbdForDriver_2E05660F(pipe, &request, NULL, completeCharge, NULL)) {
		ERROR(1505, "usb_attach"); return -1;
	}
	s_attached++;
	DEBUG(6, 1513, "usb_attach", "[%s] USB Attach (MTP/HID/Storage) <----- dev_id=%d, type=%02x, attached_device[%d]\n", "usb_attach", id, type, s_attached);
	return 0;
}
static int usb_detach(int id)
{
	unsigned type = is_supported_device(id);
	if (type)
		s_attached--;
	DEBUG(6, 1528, "usb_detach", "[%s] USB Detach <----- dev_id=%d, type=%02x, attached_device[%d]\n", "usb_detach", id, type, s_attached);
	return 0;
}
static inline __attribute__((always_inline)) void lock(void)
{
	int ret = sceKernelLockMutex(s_mutex, 1, NULL);
	(void)ret;
#if FW_DEBUG
	if (ret < 0 && sceKernelGetAssertLevel() > 0)
		SceDebugForDriver_821A2D59(1, 0, SCE_DBGINFO_FUNC(1548, "lock"),
			__builtin_return_address(0), "ret=%08x\n", ret);
#endif
}
static inline __attribute__((always_inline)) void unlock(void)
{
	int ret = sceKernelUnlockMutex(s_mutex, 1);
	(void)ret;
#if FW_DEBUG
	if (ret < 0 && sceKernelGetAssertLevel() > 0)
		SceDebugForDriver_821A2D59(1, 0, SCE_DBGINFO_FUNC(1557, "unlock"),
			__builtin_return_address(0), "ret=%08x\n", ret);
#endif
}
