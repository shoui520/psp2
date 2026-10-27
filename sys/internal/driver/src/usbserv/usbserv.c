/* SceUsbServ: USB port selection, accessory power and suspend/resume service. */
#include "usbserv.h"
#include <intrmgr/syscall.h>
#include <sysmem/sysevent_ids.h>

#define SERV_ERROR(name) ((int)SCE_USBSERV_ERROR_##name)
#if FW_DEBUG
#define DEBUG(flags, line, func, fmt, ...) \
	SceDebugForDriver_D9703808(1, flags, SCE_DBGINFO_FUNC(line, func), fmt, ##__VA_ARGS__)
#else
#define DEBUG(flags, line, func, fmt, ...) do { } while (0)
#endif
#define ERROR(line, func) \
	SceDebugForDriver_02B04343(15, SCE_DBGINFO_FUNC(line, func), "### error ###\n")

/* Private state/helper names unknown unless recovered from debug records. */
static SceUID s_timer = -1;
static struct {
	int etherSuspend, hostActive, accessory, ether;
	SceUID mutex;
	struct { int count, process; } owner[8];
	SceUID thread, systemHandler, event;
	int mode[3], attached;
	SceUID processHandler;
	int resumed;
} s_serv = { .etherSuspend = 1 };

static inline __attribute__((always_inline)) void lock(void)
{
	int ret = sceKernelLockMutex(s_serv.mutex, 1, NULL);
	(void)ret;
#if FW_DEBUG
	if (ret < 0 && sceKernelGetAssertLevel() > 0)
		SceDebugForDriver_821A2D59(1, 0, SCE_DBGINFO_FUNC(1548, "lock"),
			__builtin_return_address(0), "ret=%08x\n", ret);
#endif
}
static inline __attribute__((always_inline)) void unlock(void)
{
	int ret = sceKernelUnlockMutex(s_serv.mutex, 1);
	(void)ret;
#if FW_DEBUG
	if (ret < 0 && sceKernelGetAssertLevel() > 0)
		SceDebugForDriver_821A2D59(1, 0, SCE_DBGINFO_FUNC(1557, "unlock"),
			__builtin_return_address(0), "ret=%08x\n", ret);
#endif
}
static int used(void)
{
	if (s_serv.owner[0].count)
		return 1;
	for (unsigned i = 1; i < 8; i++)
		if (s_serv.owner[i].process != -1 && s_serv.owner[i].count)
			return 1;
	return 0;
}
static int usbEnabled(unsigned port, unsigned mode)
{
	for (unsigned i = 0; i < 2000; i++)
		if (!SceUdcdForDriver_1C684884(500, port))
			return 1;
	SceDebugForDriver_02B04343(6, SCE_DBGINFO_FUNC(207, "usbEnabled"),
		"timeout: USB PHY ready(ch=%d sel=%d)\n", port, mode);
	return 0;
}
static void configurePort(unsigned port, unsigned mode)
{
	ScePervasiveForDriver_4AF7A01E(port, 11);
	ScePervasiveForDriver_AD1E81EB(port, 11);
	ScePervasiveForDriver_6B2BE37E(port, mode);
	ScePervasiveForDriver_A2EFD7AF(port, mode ? 10 : 9);
	ScePervasiveForDriver_13CC07C9(port, mode ? 10 : 9);
}
static int usb0_mac_select(unsigned mode, unsigned action)
{
	int ret = 0;
	if (mode) {
		if (!s_serv.mode[0]) {
			ret = sceUsbdHostStop(0);
			if (ret < 0)
				DEBUG(6, 333, "usb0_mac_select", "# ERROR # sceUsbdHostStop() ret=%08x\n", ret);
		}
		ScePervasiveForDriver_4F64DEF6(0);
		return ret;
	}
	ScePervasiveForDriver_BB67EF58(0);
	configurePort(0, 0);
	if (!usbEnabled(0, 0))
		return SERV_ERROR(FATAL);
	s_serv.mode[0] = 0;
	if (action == 2) {
		ret = sceUsbdResume(0);
		if (ret < 0)
			DEBUG(6, 357, "usb0_mac_select", "# ERROR # sceUsbdResume() ret=%08x\n", ret);
	} else {
		ret = sceUsbdHostStart(0);
		if (ret < 0)
			DEBUG(6, 362, "usb0_mac_select", "# ERROR # sceUsbdHostStart() ret=%08x\n", ret);
	}
	return ret;
}
static int usb1_mac_select(unsigned mode, unsigned action)
{
	int ret;
	if (mode) {
		if (!s_serv.mode[1]) {
			ret = sceUsbdHostStop(1);
			if (ret < 0)
				DEBUG(6, 388, "usb1_mac_select", "# ERROR # sceUsbdHostStop() ret=%08x\n", ret);
		}
		ScePwmForDriver_13161CCC(1, 0, 0, 0);
		configurePort(1, 1);
		if (!usbEnabled(1, 1))
			return SERV_ERROR(FATAL);
		s_serv.mode[1] = 1;
		ret = sceUdcdInit(1);
		if (ret < 0)
			DEBUG(6, 401, "usb1_mac_select", "# ERROR # sceUdcdInit() ret=%08x\n", ret);
		return ret;
	}
	ret = sceUdcdEnd(1);
	if (ret < 0 && ret != (int)0x80243002)
		DEBUG(6, 411, "usb1_mac_select", "# ERROR # sceUdcdEnd() ret=%08x\n", ret);
	ScePwmForDriver_13161CCC(1, 0x200, 0, 0);
	configurePort(1, 0);
	if (!usbEnabled(1, 0))
		return SERV_ERROR(FATAL);
	s_serv.mode[1] = 0;
	return action == 2 ? sceUsbdResume(1) : sceUsbdHostStart(1);
}
static int port2Host(void)
{
	unsigned info;
	if (SceSblAIMgrForDriver_C6E83F34())
		return s_serv.ether != 0;
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
static int usb2_mac_select(unsigned mode, unsigned action)
{
	int ret;
	if (!mode) {
		if (s_serv.mode[2] == 1) {
			ret = sceUdcdEnd(2);
			if (ret < 0)
				DEBUG(6, 489, "usb2_mac_select", "# ERROR # sceUdcdEnd() ret=%08x\n", ret);
		}
		configurePort(2, 0);
		if (!usbEnabled(2, 0))
			return SERV_ERROR(FATAL);
		sceGpioPortSet(1, 3);
		s_serv.mode[2] = 0;
		if ((SceSysconForDriver_CBD6D8BC() & 0xfd0000) == 0x800000)
			ScePervasiveForDriver_29E65AE1(2, 0, 0);
		return action == 2 ? sceUsbdResume(2) : sceUsbdHostStart(2);
	}
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
			DEBUG(15, 452, "usb2_mac_select", "USB Port#2 is operating as Host mode.\n");
			return SERV_ERROR(NOT_SUPPORTED);
		}
	}
	if (!s_serv.mode[2]) {
		ret = sceUsbdHostStop(2);
		if (ret < 0)
			DEBUG(6, 462, "usb2_mac_select", "# ERROR # sceUsbdHostStop() ret=%08x\n", ret);
	}
	configurePort(2, 1);
	if (!usbEnabled(2, 1))
		return SERV_ERROR(FATAL);
	sceGpioPortClear(1, 3);
	s_serv.mode[2] = 1;
	if ((SceSysconForDriver_CBD6D8BC() & 0xfd0000) == 0x800000)
		ScePervasiveForDriver_29E65AE1(2, 0x80c47064, 0x18);
	ret = sceUdcdInit(2);
	if (ret < 0)
		DEBUG(6, 478, "usb2_mac_select", "# ERROR # sceUdcdInit() ret=%08x\n", ret);
	return ret;
}
int sceUsbServMacSelect(SceUInt port, SceBool mode)
{
	if ((unsigned)mode > 1 || port > 2)
		return SERV_ERROR(INVALID_PARAM);
	if (s_serv.mode[port] == mode)
		return 0;
	if (!port)
		return usb0_mac_select(mode, 1);
	return port == 1 ? usb1_mac_select(mode, 1) : usb2_mac_select(mode, 1);
}
SceBool SceUsbServForDriver_F0553A69(SceUInt port)
{
	return port < 3 ? s_serv.mode[port] : SERV_ERROR(INVALID_PARAM);
}
static inline __attribute__((always_inline)) int accessoryActivate(void)
{
	lock();
	if (s_serv.accessory && !used()) {
		usb0_mac_select(0, 1);
		sceSysconCtrlAccPower(1);
	}
	int ret = 0;
	if (!sceKernelIsUserModeThread()) {
		s_serv.owner[0].count++;
	} else {
		int pid = sceKernelGetProcessId(), slot = -1;
		if (pid >= 0) {
			for (unsigned i = 1; i < 8; i++)
				if (s_serv.owner[i].process == pid) { slot = i; break; }
			if (slot < 0)
				for (unsigned i = 1; i < 8; i++)
					if (s_serv.owner[i].process == -1) { slot = i; break; }
		}
		if (slot < 0) {
			ret = SERV_ERROR(FATAL);
		} else {
			s_serv.owner[slot].count++;
			s_serv.owner[slot].process = pid;
		}
	}
	unlock();
	return ret;
}
int sceUsbServAccessoryActivate(void)
{
	return accessoryActivate();
}
int sceUsbServAccessoryDeactivate(void)
{
	lock();
	int ret = 0;
	if (!sceKernelIsUserModeThread()) {
		if (--s_serv.owner[0].count < 0)
			s_serv.owner[0].count = 0;
	} else {
		int pid = sceKernelGetProcessId(), slot = -1;
		if (pid >= 0)
			for (unsigned i = 1; i < 8; i++)
				if (s_serv.owner[i].process == pid && --s_serv.owner[i].count <= 0) {
					slot = i; break;
				}
		if (slot < 0) { ret = SERV_ERROR(FATAL); goto out; }
		s_serv.owner[slot].count = 0;
		s_serv.owner[slot].process = -1;
	}
	if (!used()) {
		usb0_mac_select(1, 1);
		sceSysconCtrlAccPower(0);
	}
out:
	unlock();
	return ret;
}
int sceUsbServAccessoryActivateForUser(void)
{
	unsigned domain = sceSyscallEnter();
	lock();
	int ret = accessoryActivate();
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
	if (!s_serv.ether) {
		SceSysconForDriver_3274A925(1);
		s_serv.ether = 1;
		sceKernelSetEventFlag(s_serv.event, 2);
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
	if (s_serv.ether) {
		SceSysconForDriver_3274A925(0);
		s_serv.ether = 0;
		sceKernelSetEventFlag(s_serv.event, 4);
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
	s_serv.etherSuspend = 0;
	return 0;
}

static int is_ds(unsigned id)
{
	uint16_t *dev = SceUsbdForDriver_BC3EF82B(id, NULL, 1);
	if (!dev) { ERROR(1389, "is_ds"); return 0; }
	return dev[4] == 0x54c && (dev[5] == 0x42f || dev[5] == 0x268 || dev[5] == 0x5c4);
}
static unsigned is_supported_device(unsigned id)
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
	for (unsigned i = 0; i < dev[17]; i++) {
		configuration = SceUsbdForDriver_BC3EF82B(id, configuration, 2);
		if (!configuration) { ERROR(1431, "is_supported_device"); return 0; }
		uint8_t *interface = configuration;
		while ((interface = SceUsbdForDriver_BC3EF82B(id, interface, 4))) {
			unsigned type = interface[5];
			if (type == 1)
				audio = 1;
			else if (type == 3)
				hid = 1;
			else if (type == 6 || type == 8) {
				if (type == 8 && sceKernelSysrootIsSafeMode())
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
	if (s_serv.attached > 0) {
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
		DEBUG(6, 1483, "usb_attach", "[%s] USB Attach (DS3/Navicon) <----- dev_id=%d, attached_device[%d]\n", "usb_attach", id, s_serv.attached);
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
	s_serv.attached++;
	DEBUG(6, 1513, "usb_attach", "[%s] USB Attach (MTP/HID/Storage) <----- dev_id=%d, type=%02x, attached_device[%d]\n", "usb_attach", id, type, s_serv.attached);
	return 0;
}
static int usb_detach(int id)
{
	unsigned type = is_supported_device(id);
	if (type)
		s_serv.attached--;
	DEBUG(6, 1528, "usb_detach", "[%s] USB Detach <----- dev_id=%d, type=%02x, attached_device[%d]\n", "usb_detach", id, type, s_serv.attached);
	return 0;
}
static void accessoryChanged(int info, void *arg)
{
	sceKernelSetEventFlag(s_serv.event, info ? 8 : 16);
}
static void microUsbChanged(int info, void *arg)
{
	sceKernelSetEventFlag(s_serv.event, (info & 0xff00) == 0x200 ? 64 : 128);
}
static void multiCnChanged(int info, void *arg)
{
	sceKernelSetEventFlag(s_serv.event, info & 0xfb00 ? 4 : 2);
}
static void registerConnectors(void)
{
	SceSysconForDriver_A26586B2(multiCnChanged, NULL);
	SceSysconForDriver_67A4CB9F(microUsbChanged, NULL);
	SceSysconForDriver_4A42712F(accessoryChanged, NULL);
}
static int _sceUsbServSysEventHandler(int phase, int event, void *arg, void *opt)
{
	if (!phase) {
		if (event == SYSEVENT_SUSPEND_PREPARE(0)) {
			int ret = SceUdcdForDriver_9E7F7EFE();
			if (SceSblAIMgrForDriver_71608CA3() && !s_serv.mode[0]) {
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
				if (!s_serv.mode[i])
					sceUsbdSuspend(i);
				ScePervasiveForDriver_4F64DEF6(i);
				s_serv.mode[i] = 1;
			}
			s_serv.hostActive = 0;
			sceSysconCtrlAccPower(0);
			unsigned info;
			unsigned hardware = SceSysconForDriver_CBD6D8BC();
			sceSysconGetMultiCnInfo(&info);
			if ((info & 0xff00) != 0x200 && (hardware & 0xff0000) <= 0x600000)
				SceSysconForDriver_B1F88B11(0);
			if (SceSblAIMgrForDriver_C6E83F34() && s_serv.etherSuspend)
				SceSysconForDriver_3274A925(0);
			sceGpioPortClear(1, 3);
		}
	} else if (phase == 1) {
		if (event == SYSEVENT_RESUME_START) {
			s_serv.resumed = 0;
			return sceKernelSetEventFlag(s_serv.event, 32);
		}
		if (event == SYSEVENT_RESUME_IDLE_QUERY)
			return s_serv.resumed ? 0 : -1;
	}
	return 0;
}
static int cleanupProcess(int process)
{
	if (!used())
		return 0;
	if (sceKernelIsUserModeThread())
		for (unsigned i = 1; i < 8; i++)
			if (s_serv.owner[i].process != -1 && s_serv.owner[i].process == process) {
				s_serv.owner[i].process = -1;
				s_serv.owner[i].count--;
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

static void devKitPower(unsigned enable)
{
	unsigned info;
	unsigned hardware = SceSysconForDriver_CBD6D8BC();
	sceSysconGetMultiCnInfo(&info);
	if ((info & 0xff00) != 0x200 && (hardware & 0xff0000) <= 0x600000)
		SceSysconForDriver_B1F88B11(enable);
}
static int UsbServThread(unsigned arglen, void *arg)
{
	for (unsigned i = 0; i < 8; i++) {
		s_serv.owner[i].count = 0;
		s_serv.owner[i].process = -1;
	}
	sceSysconCtrlAccPower(0);
	if (SceSblAIMgrForDriver_C6E83F34()) {
		SceSysconForDriver_3274A925(1);
		s_serv.ether = 1;
	}
	s_serv.accessory = SceSysconForDriver_B9EA2FA8();
	if (s_serv.accessory && used())
		usb0_mac_select(0, 1);
	if (SceSblAIMgrForDriver_71608CA3()) {
		s_serv.accessory = 1;
		if (used())
			sceSysconCtrlAccPower(1);
	}
	if (SceSysconForDriver_CBD6D8BC() & 2)
		usb1_mac_select(0, 1);
	if (port2Host() || (SceSysconForDriver_CBD6D8BC() & 0xff0000) <= 0x100000) {
		usb2_mac_select(0, 1);
		devKitPower(1);
		s_serv.hostActive = 1;
	}
	registerConnectors();
	for (;;) {
		unsigned flags;
		int ret = sceKernelWaitEventFlag(s_serv.event, 0xfe,
						  SCE_KERNEL_EVF_WAITMODE_OR |
						  SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, &flags, NULL);
		if (ret < 0) {
			DEBUG(6, 877, "UsbServThread", "# ERROR # sceKernelWaitEventFlag 0x%08x\n", ret);
			continue;
		}
		if (flags & 0xc6) {
			int host = port2Host();
			if (host && !s_serv.hostActive) {
				s_serv.hostActive = 1;
				usb2_mac_select(0, 1);
				devKitPower(1);
			} else if (!host && s_serv.hostActive) {
				s_serv.hostActive = 0;
				devKitPower(0);
				usb2_mac_select(1, 1);
			}
		}
		if (flags & 0x18) {
			s_serv.accessory = !!(flags & 8);
			if (!s_serv.accessory)
				sceSysconCtrlAccPower(0);
			else if (used())
				sceSysconCtrlAccPower(1);
		}
		if (!(flags & 0x20))
			continue;
		SceGpioForDriver_372022A4(1, 3, 0);
		sceGpioPortClear(1, 3);
		SceUdcdForDriver_1A5A3851();
		if (SceSblAIMgrForDriver_C6E83F34()) {
			lock();
			SceSysconForDriver_3274A925(1);
			s_serv.ether = 1;
			unlock();
		}
		ScePervasiveForDriver_BB67EF58(0);
		usb0_mac_select(0, 2);
		s_serv.accessory = SceSysconForDriver_B9EA2FA8();
		if (SceSblAIMgrForDriver_71608CA3()) {
			s_serv.accessory = 1;
			if (used())
				sceSysconCtrlAccPower(1);
		}
		if (s_serv.accessory && used())
			sceSysconCtrlAccPower(1);
		else
			usb0_mac_select(1, 1);
		if (SceModulemgrForDriver_BBE1771C("SceBbmc") >= 0) {
			ScePervasiveForDriver_BB67EF58(1);
			usb1_mac_select(0, 2);
		} else {
			usb1_mac_select(1, 1);
		}
		usb2_mac_select(0, 2);
		if (port2Host()) {
			devKitPower(1);
			s_serv.hostActive = 1;
		} else {
			usb2_mac_select(1, 1);
		}
		if (SceSblAIMgrForDriver_C6E83F34())
			s_serv.etherSuspend = 1;
		registerConnectors();
		s_serv.resumed = 1;
	}
	__builtin_unreachable();
}
int _sceUsbServInit(unsigned arglen, const void *argp)
{
	static const SceUsbdDriver driver = { "usb_charge", usb_probe, usb_attach, usb_detach };
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
	for (unsigned i = 0; i < 3; i++) {
		ScePervasiveForDriver_4F64DEF6(i);
		s_serv.mode[i] = 1;
	}
	ScePervasiveForDriver_BB67EF58(1);
	usb1_mac_select(1, 1);
	ScePervasiveForDriver_BB67EF58(2);
	usb2_mac_select(1, 1);
	unsigned options[2] = { 8, 32 };
	int ret = sceKernelCreateMutex("SceUsbServ", 6, 0, (void *)options);
	if (ret < 0) {
		DEBUG(6, 1056, "_sceUsbServInit", "# ERROR # sceKernelCreateMutex 0x%08x\n", ret);
		return 1;
	}
	s_serv.mutex = ret;
	s_serv.event = sceKernelCreateEventFlag("SceUsbserv", SCE_KERNEL_EVF_ATTR_MULTI, 0, NULL);
	if (s_serv.event < 0) {
		DEBUG(6, 1064, "_sceUsbServInit", "# ERROR # sceKernelCreateEventFlag 0x%08x\n", s_serv.event);
		goto deleteMutex;
	}
	s_serv.thread = sceKernelCreateThread("SceUsbServ", UsbServThread, 60, 0x2000, 0, 0x80000, NULL);
	if (s_serv.thread < 0) {
		DEBUG(6, 1073, "_sceUsbServInit", "# ERROR # sceKernelCreateThread 0x%08x\n", s_serv.thread);
		goto deleteEvent;
	}
	sceKernelStartThread(s_serv.thread, 0, NULL);
	s_serv.systemHandler = sceKernelRegisterSysEventHandler("SceUsbServ", _sceUsbServSysEventHandler, NULL);
	if (s_serv.systemHandler < 0) {
		DEBUG(6, 1083, "_sceUsbServInit", "# ERROR # sceKernelRegisterSysEventHandler 0x%08x\n", s_serv.systemHandler);
		goto deleteThread;
	}
	s_serv.processHandler = sceKernelRegisterProcEventHandler("SceUsbserv", &events, NULL);
	if (s_serv.processHandler < 0) {
		DEBUG(6, 1090, "_sceUsbServInit", "# ERROR # sceKernelRegisterProcEventHandler 0x%08x\n", s_serv.processHandler);
		sceKernelUnregisterSysEventHandler(s_serv.systemHandler);
		goto deleteThread;
	}
	if (SceSblAIMgrForDriver_71608CA3())
		SceUsbdForDriver_1EC94F18(&driver);
	return 0;
deleteThread:
	sceKernelDeleteThread(s_serv.thread);
deleteEvent:
	sceKernelDeleteEventFlag(s_serv.event);
deleteMutex:
	sceKernelDeleteMutex(s_serv.mutex);
	return 1;
}
