/*
 * grab.c: the GRAB and SonyRegbus bridges, which the compat (Allegrex) and
 * GPU masters reach memory and registers through. SceLowio's
 * SceGrabForDriver exports call monitor calls 0x11b-0x121: stop and restart
 * GRAB ports, map the compat banks (sceGrabSetCmpMap), and power the compat
 * core or change the GPU clock with their ports stopped.
 *
 * Sony's names: sceGrabSetCmpMap, the file name (debug records); the
 * monitor calls follow the SceGrabForDriver names on the henkaku wiki.
 */
#include "driver_tzs.h"

#define GRAB_ERROR		((int)0x803f0b00)

#define GRAB_COMPAT_STOP	0	/* sceGrabCompatStop */
#define GRAB_COMPAT_START	1	/* sceGrabCompatStartEx */
#define GRAB_COMPAT_LCDDMAC	2	/* sceGrabCompatLcdDmacInit */

#define GPU_CLOCK_GATES		0xc000c	/* the GPU core gates its clock change stops */

/* the lowest DRAM address a compat bank may map, by model */
#define CMP_MAP_DRAM_MIN	0x42600000
#define CMP_MAP_DRAM_MIN_TV	0x42200000

SceKernelSpinlock s_grabLock;

/* the mappings; Sony's code reaches them through one base */
static struct {
	SceUID uid[4];
	SceSonyRegbusRegisters *regbus;
	SceSonyRegbusRegisters *regbusNs;
	SceGrabRegisters *grab;
	SceGrabRegisters *grabNs;
} s_grab;

static const struct {
	const char *name;
	uint32_t paddr;
	uint32_t size;
	void **base;
} s_grabDevices[4] = {
	{ "SceSonyRegbus", SONY_REGBUS_BASE, GRAB_PAGE_SIZE, (void **)&s_grab.regbus },
	{ "SceSonyRegbus", SONY_REGBUS_NS_BASE, GRAB_PAGE_SIZE, (void **)&s_grab.regbusNs },
	{ "SceGrab", GRAB_BASE, GRAB_PAGE_SIZE, (void **)&s_grab.grab },
	{ "SceGrab", GRAB_NS_BASE, GRAB_PAGE_SIZE, (void **)&s_grab.grabNs },
};

/* the port configuration words the Non-secure kernel may write, by port */
static const uint32_t s_grabPortConfigAllowed[GRAB_PORT_CONFIG_COUNT] = {
	0xf40, 0xf02, 0x13f, 0x23f, 0x43f, 0x83f,
};


/* 0x11f: wait for one of the Non-secure regbus status bits */
static int GrabRegbusWaitMonitorCall(unsigned int mask)
{
	if ((mask & ~3) != 0 || mask == 0)
		return GRAB_ERROR;
	while ((s_grab.regbusNs->status & mask) == 0)
		;
	return 0;
}

/* 0x11c: write one port configuration word, id = port << 8 | word */
static int GrabPortConfigMonitorCall(unsigned int id, uint32_t value)
{
	unsigned int word = id & 0xff;
	unsigned int port = (id >> 8) & 0xff;

	if (word > 0x1f || port > 5)
		return GRAB_ERROR;
	if (((s_grabPortConfigAllowed[port] >> word) & 1) == 0)
		return GRAB_ERROR;
	s_grab.grabNs->portConfig[port][word] = value;
	(void)s_grab.grabNs->portConfig[port][word];
	PervasiveDsb();
	return 0;
}

/* until no transfer is in flight on the ports of mask */
static inline void GrabWaitPortsIdle(unsigned int mask)
{
	uint32_t readBusy, writeBusy;

	do {
		do
			readBusy = s_grab.grabNs->readBusy;
		while (readBusy != s_grab.grabNs->readBusy);
		PervasiveDmb();
		do
			writeBusy = s_grab.grabNs->writeBusy;
		while (writeBusy != s_grab.grabNs->writeBusy);
		PervasiveDmb();
	} while (((readBusy | writeBusy) & mask) != 0);
}

/* 0x11b: stop the ports of mask and wait for them, or restart them */
static int GrabStopMonitorCall(unsigned int mask, int restart)
{
	int state;

	if ((mask >> 12) != 0)
		return GRAB_ERROR;
	state = SceCpuForDriver_D32ACE9E(&s_grabLock);
	if (restart) {
		s_grab.grabNs->writeStop &= ~mask;
		s_grab.grabNs->readStop &= ~mask;
		PervasiveDsb();
	} else {
		s_grab.grabNs->writeStop |= mask;
		s_grab.grabNs->readStop |= mask;
		PervasiveDmb();
		GrabWaitPortsIdle(mask);
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_grabLock, state);
	return 0;
}

/* 0x11d: map compat bank n to paddr, the CP RAM or DRAM above the secure part */
int sceGrabSetCmpMap(unsigned int n, uint32_t paddr)
{
	unsigned int model;

	if (n > 7)
		return GRAB_ERROR;
	if (paddr - 0x20000000 >= 0x10000000) {
		if (paddr - 0x40000000 >= 0x40000000)
			return GRAB_ERROR;
		model = sceKernelSysrootGetModelInfoForTZS();
		switch (model) {	/* 0x1xxxx handhelds, 0x2xxxx PS TV */
		default:
			SCE_DBG_PRINTF(11, 383, "Unknown model 0x%08x\n", model);
			/* fall through: treated as a handheld */
		case 0x10100:
		case 0x10150:
		case 0x10200:
		case 0x10220:
		case 0x11100:
			if (paddr < CMP_MAP_DRAM_MIN)
				return GRAB_ERROR;
			break;
		case 0x20100:
		case 0x20101:
		case 0x20110:
			if (paddr < CMP_MAP_DRAM_MIN_TV)
				return GRAB_ERROR;
			break;
		}
	}
	s_grab.grab->cmpMap[n] = paddr;
	PervasiveDsb();
	return 0;
}

/* 0x120: compat bank n's mapping; the monitor call returns the result in
 * r0 and the address in r1 */
static uint64_t GrabGetCmpMapMonitorCall(unsigned int n)
{
	union {
		uint64_t value;
		uint32_t word[2];
	} ret;

	if (n > 7) {
		ret.word[0] = GRAB_ERROR;
		ret.word[1] = 0xffffffff;
	} else {
		ret.word[0] = 0;
		ret.word[1] = s_grab.grab->cmpMap[n];
	}
	return ret.value;
}

static inline void GrabStopPorts(unsigned int mask)
{
	int state;

	state = SceCpuForDriver_D32ACE9E(&s_grabLock);
	s_grab.grabNs->writeStop |= mask;
	s_grab.grabNs->readStop |= mask;
	PervasiveDmb();
	GrabWaitPortsIdle(mask);
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_grabLock, state);
}

static inline void GrabRestartPorts(unsigned int mask)
{
	int state;

	state = SceCpuForDriver_D32ACE9E(&s_grabLock);
	s_grab.grabNs->writeStop &= ~mask;
	s_grab.grabNs->readStop &= ~mask;
	PervasiveDsb();
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_grabLock, state);
}

/* 0x121: change the GPU clock with its ports stopped and the other cores parked */
static int GrabGpuClockMonitorCall(unsigned int core, unsigned int mp)
{
	int ret;

	ParkOtherCpus(0);
	GrabStopPorts(GRAB_PORT_GPU);
	sceKernelUsleep(4);
	GpuGateDisable(GPU_CLOCK_GATES);
	ret = GpuClockSet(core, mp);
	GpuGateEnable(GPU_CLOCK_GATES);
	GrabRestartPorts(GRAB_PORT_GPU);
	ReleaseOtherCpus();
	return ret;
}

/* 0x11e: start, stop or set up the compat core with the ports stopped */
static int GrabCompatMonitorCall(int command, int param)
{
	unsigned int n;
	int ret;

	if (command == GRAB_COMPAT_START) {
		ParkOtherCpus(0);
		GrabStopPorts(GRAB_PORT_ALL);
		ret = AllegrexPowerOn(param);
		GrabRestartPorts(GRAB_PORT_ALL);
	} else if (command == GRAB_COMPAT_LCDDMAC) {
		ParkOtherCpus(0);
		GrabStopPorts(GRAB_PORT_ALL);
		ret = AllegrexStart();
		GrabRestartPorts(GRAB_PORT_ALL);
	} else {
		ParkOtherCpus(0);
		GrabStopPorts(GRAB_PORT_COMPAT);
		for (n = 0; n < GRAB_CMP_MAP_COUNT; n++)
			s_grab.grab->cmpMap[n] = 0;
		s_grab.regbus->accessMode[1] = 0;
		ret = AllegrexPowerOff();
		GrabRestartPorts(GRAB_PORT_COMPAT);
	}
	ReleaseOtherCpus();
	return ret;
}

static __attribute__((noinline)) int GrabInit(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	unsigned int i;
	void *base;
	int ret;

	for (i = 0; i < 4; i++) {
		memset(&opt, 0, sizeof(opt));
		opt.size = 0x58;
		opt.attr = 2;
		opt.paddr = s_grabDevices[i].paddr;
		ret = sceKernelAllocMemBlockForKernel(s_grabDevices[i].name, 0x20100806,
			s_grabDevices[i].size, &opt);
		if (ret < 0)
			goto error;
		s_grab.uid[i] = ret;
		sceKernelGetMemBlockBase(ret, &base);
		*s_grabDevices[i].base = base;
	}
	sceKernelRegisterMonitorCallHandler(GRAB_MONCALL_STOP,
		(SceKernelMonitorCallHandler)GrabStopMonitorCall);
	sceKernelRegisterMonitorCallHandler(GRAB_MONCALL_PORT_CONFIG,
		(SceKernelMonitorCallHandler)GrabPortConfigMonitorCall);
	sceKernelRegisterMonitorCallHandler(GRAB_MONCALL_GET_CMP_MAP,
		(SceKernelMonitorCallHandler)GrabGetCmpMapMonitorCall);
	sceKernelRegisterMonitorCallHandler(GRAB_MONCALL_SET_CMP_MAP,
		(SceKernelMonitorCallHandler)sceGrabSetCmpMap);
	sceKernelRegisterMonitorCallHandler(GRAB_MONCALL_COMPAT,
		(SceKernelMonitorCallHandler)GrabCompatMonitorCall);
	sceKernelRegisterMonitorCallHandler(GRAB_MONCALL_REGBUS_WAIT,
		(SceKernelMonitorCallHandler)GrabRegbusWaitMonitorCall);
	sceKernelRegisterMonitorCallHandler(GRAB_MONCALL_GPU_CLOCK,
		(SceKernelMonitorCallHandler)GrabGpuClockMonitorCall);
	return 0;

error:
	for (i = 0; i < 4; i++)
		SceSysmemForDriver_009E1C61(s_grab.uid[i]);
	return ret;
}

int GrabModuleStart(SceSize arglen, const void *argp)
{
	SceGrabRegisters *grabNs;

	(void)arglen;
	(void)argp;
	if (GrabInit() < 0)
		return 2;
	grabNs = s_grab.grabNs;
	grabNs->portConfig[2][0x20] = 0x030a2a;
	grabNs->portConfig[3][0x20] = 0x0c0a2a;
	grabNs->portConfig[4][0x20] = 0x300a2a;
	grabNs->portConfig[5][0x20] = 0xc00a2a;
	grabNs->portConfig[2][3] = 0x2b0140;
	grabNs->portConfig[3][3] = 0x2b0140;
	grabNs->portConfig[4][3] = 0x2b0140;
	grabNs->portConfig[5][3] = 0x2b0140;
	PervasiveDsb();
	return 0;
}
