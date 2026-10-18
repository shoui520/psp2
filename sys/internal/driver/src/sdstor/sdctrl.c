/* SceSdstor interrupt and control handling (Sony source: sdctrl.c). */
#include <sysmem/sysroot.h>
#include "sdstor_internal.h"
/* VFS carries SDK opaque mutex declarations; keep our complete owner ABI. */
#define sceKernelInitializeFastMutex ksceKernelInitializeFastMutex_vitasdk
#define sceKernelFinalizeFastMutex ksceKernelFinalizeFastMutex_vitasdk
#define lockFastMutex ksceKernelLockFastMutex_vitasdk
#define sceKernelUnlockFastMutex ksceKernelUnlockFastMutex_vitasdk
#include <iofilemgr/vfs.h>
#undef sceKernelInitializeFastMutex
#undef sceKernelFinalizeFastMutex
#undef lockFastMutex
#undef sceKernelUnlockFastMutex
#include <intrmgr/intrmgr.h>
#include <sysmem/debug.h>
#include <sdstor/mbr.h>
#include <sdif/sdif.h>
#include <sysmem/sysevent.h>
#include <psp2kern/kernel/threadmgr/thread.h>
#include <psp2kern/kernel/threadmgr/event_flags.h>
#include <psp2kern/kernel/ssmgr.h>
#include <sysmem/cpu.h>
#include <stdio.h>
#include <string.h>
#include <syscon/syscon.h>
#include <iofilemgr/iofilemgr.h>

typedef struct SceSdstorIntrHeader {
	SceKernelSubIntrHandler insertHandler;
	SceKernelSubIntrHandler removeHandler;
	uint8_t port;
	uint8_t insertSubIntr;
	uint8_t removeSubIntr;
	uint8_t reserved;
} SceSdstorIntrHeader;

typedef struct SceSdstorIntrThread {
	const char *name;
	SceKernelThreadEntry entry;
	int priority;
	SceSize stackSize;
	uint32_t attr;
	int affinity;
	SceUID threadId;
} SceSdstorIntrThread;

typedef struct SceSdstorIntr {
	uint32_t reserved;
	SceUID requestEvent;
	SceUID syncEvent;
	SceSdstorIntrHeader *header;
	SceSdstorIntrThread *thread;
	uint32_t cid[4];
	uint8_t port;
	uint8_t stat;
	uint8_t mountDisabled;
	uint8_t reservedTail;
} SceSdstorIntr;

typedef struct SceSdstorControl {
	SceUID handlerId;
	uint8_t reserved;
	uint8_t resumeHandled;
	uint8_t reservedTail[2];
} SceSdstorControl;

_Static_assert(sizeof(SceSdstorIntrHeader) == 12, "SceSdstor interrupt header");
_Static_assert(sizeof(SceSdstorIntrThread) == 28, "SceSdstor interrupt thread");
_Static_assert(sizeof(SceSdstorIntr) == 40, "SceSdstor interrupt context");
_Static_assert(sizeof(SceSdstorControl) == 8, "SceSdstor control context");

typedef union SceSdstorMediaSector {
	SceMbr mbr;
	struct {
		uint32_t cid[4];
		uint8_t reserved[496];
	} identity;
} SceSdstorMediaSector;

_Static_assert(sizeof(SceSdstorMediaSector) == 512, "SceSdstor media sector");

#if FW_DEBUG
static SceSdstorIntrThread sdintrThreads[5];
static SceSdstorMediaSector memoryCardMediaSector;
static SceSdstorIntrHeader sdintrHeaders[5];
static SceSdstorControl sdctrlState;
static SceSdstorIntr sdintrObjects[5];
static SceSdstorMediaSector gameCardMediaSector;
static SceMbr connectionProbeSector;
#else
static SceMbr connectionProbeSector;
static SceSdstorIntrThread sdintrThreads[5];
static SceSdstorIntrHeader sdintrHeaders[5];
static SceSdstorControl sdctrlState;
static SceSdstorIntr sdintrObjects[5];
static SceSdstorMediaSector memoryCardMediaSector;
static SceSdstorMediaSector gameCardMediaSector;
#endif

static const int gameCardMountIds[3] = {0x900, 0xa00, 0x100};

#if FW_DEBUG
static const uint8_t sdintrHeaderPorts[5] = {0, 1, 2, 3, 4};
static const uint8_t sdintrIrqNumbers[5] = {220, 221, 60, 15, 223};
#else
static const uint8_t sdintrIrqNumbers[5] = {220, 221, 60, 15, 223};
static const uint8_t sdintrHeaderPorts[5] = {0, 1, 2, 3, 4};
#endif
static const uint32_t sdintrStateRequests[3] = {0x110, 0x103, 0x1001};

int sdpart_valid_internal_mbr(const SceMbr *mbr);
int sddev_is_read_write(SceSdstorSddev *device);
unsigned int sdpart_get_mc_port(void);
unsigned int sdpart_get_xmc_port(void);
unsigned int sdpart_grab_mc(unsigned int port);
unsigned int sdpart_grab_xmc(unsigned int port);
SceSdstorPartitionEntry *sdpart_get_object(const char *name, int length);
int sdpart_remove(unsigned int port);
int sdpart_release_mc(unsigned int port);
int sdpart_release_xmc(unsigned int port);
int sddev_suspend(SceSdstorSddev *device);
int ksceSdifGetCID(SdifMmcContext *part, void *cid);
int sddev_remove(SceSdstorSddev *device);
int sddev_power_off(SceSdstorSddev *device);
int sdpart_resume_stat(unsigned int port);
int sdpart_insert(unsigned int port);
int sdpart_suspend(unsigned int port);
int sdpart_close(unsigned int port);
int sddev_insert(SceSdstorSddev *device);
int sddev_check_removal(SceSdstorSddev *device);
int sddev_resume(SceSdstorSddev *device);

int SceUsbMassForDriver_3C821E99(int intrCode, unsigned int subintrCode)
	__attribute__((weak));
int SceUsbMassForDriver_7833D935(int intrCode, unsigned int subintrCode)
	__attribute__((weak));
int SceUsbMassForDriver_F2BAB182(int intrCode);

int sdintrEnableUsbMassSubIntr(int intrCode, int subintrCode);
int sdintrDisableUsbMassSubIntr(int intrCode, int subintrCode);
int sdintrUsbMassHandler(int intrCode, void *userCtx);

static const SceKernelIntrCallbacks usbMassIntrCallbacks = {
	.size = sizeof(SceKernelIntrCallbacks),
	.enable = sdintrEnableUsbMassSubIntr,
	.disable = sdintrDisableUsbMassSubIntr
};

int sdintrEnableUsbMassSubIntr(int intrCode, int subintrCode)
{
	if (SceUsbMassForDriver_3C821E99)
		return SceUsbMassForDriver_3C821E99(intrCode, subintrCode);
	return 0;
}

int sdintrDisableUsbMassSubIntr(int intrCode, int subintrCode)
{
	if (SceUsbMassForDriver_7833D935)
		return SceUsbMassForDriver_7833D935(intrCode, subintrCode);
	return 0;
}

int sdintrUsbMassHandler(int intrCode, void *userCtx)
{
	return SceUsbMassForDriver_F2BAB182(intrCode);
}

int sdstorRegisterUsbMassInterrupt(void)
{
	SceKernelIntrRegisterOpt opt;

	opt.size = sizeof(opt);
	opt.num = 2;
	opt.handlers = (void *)&usbMassIntrCallbacks;
	opt.attr = 0;
	opt.config = 0;
	return sceKernelRegisterIntrHandler(15, "SceUsbMass", 0,
					     sdintrUsbMassHandler, NULL, 208, 15, &opt);
}

int sdintr_hdr_remove_handler(void *subintrArg, void *registerArg,
			      unsigned char intrPriority)
{
	SceSdstorIntr *interrupt = registerArg;
	int result;

	result = sceKernelSetEventFlag(interrupt->requestEvent, 1);
	if (result)
		SCE_DBG_PRINTF_LEVEL(1, 15, 2481, "0x%08x\n", result);
	return result;
}

int sdintr_hdr_insert_handler(void *subintrArg, void *registerArg,
			      unsigned char intrPriority)
{
	SceSdstorIntr *interrupt = registerArg;
	int result;

	result = sceKernelSetEventFlag(interrupt->requestEvent, 0x10);
	if (result)
		SCE_DBG_PRINTF_LEVEL(1, 15, 2463, "0x%08x\n", result);
	return result;
}

int sdintr_read_mid(SceSdstorIntr *interrupt)
{
	SceSdstorSddev *device;
	SceSdstorMediaSector *sector;
	unsigned int port, mediaIdSector = 0;
	int result;

	SCE_DBG_ASSERT(interrupt != NULL, 1207);
	port = interrupt->port;
	device = sddev_get_object(port);
	if (port == 1)
		sector = &gameCardMediaSector;
	else if (port == 2)
		sector = &memoryCardMediaSector;
	else {
		SCE_DBG_ASSERT(0, 1226);
		return 0x80010013;
	}
	result = sddev_power_on(device);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 15, 1232, "0x%08x\n", result);
		return result;
	}
	result = sddev_read(device, sector, 0, 1);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 15, 1237, "0x%08x\n", result);
		return result;
	}
	result = sdpart_valid_internal_mbr(&sector->mbr);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 15, 1242, "0x%08x\n", result);
		return result;
	}
	if (sector->mbr.partitions[0].code == 13)
		mediaIdSector = sector->mbr.partitions[0].offset;
	if (sector->mbr.partitions[1].code == 13)
		mediaIdSector = sector->mbr.partitions[1].offset;
	if (sector->mbr.partitions[2].code == 13)
		mediaIdSector = sector->mbr.partitions[2].offset;
	if (sector->mbr.partitions[3].code == 13)
		mediaIdSector = sector->mbr.partitions[3].offset;
	if (sector->mbr.partitions[4].code == 13)
		mediaIdSector = sector->mbr.partitions[4].offset;
	if (sector->mbr.partitions[5].code == 13)
		mediaIdSector = sector->mbr.partitions[5].offset;
	if (sector->mbr.partitions[6].code == 13)
		mediaIdSector = sector->mbr.partitions[6].offset;
	if (sector->mbr.partitions[7].code == 13)
		mediaIdSector = sector->mbr.partitions[7].offset;
	if (sector->mbr.partitions[8].code == 13)
		mediaIdSector = sector->mbr.partitions[8].offset;
	if (sector->mbr.partitions[9].code == 13)
		mediaIdSector = sector->mbr.partitions[9].offset;
	if (sector->mbr.partitions[10].code == 13)
		mediaIdSector = sector->mbr.partitions[10].offset;
	if (sector->mbr.partitions[11].code == 13)
		mediaIdSector = sector->mbr.partitions[11].offset;
	if (sector->mbr.partitions[12].code == 13)
		mediaIdSector = sector->mbr.partitions[12].offset;
	if (sector->mbr.partitions[13].code == 13)
		mediaIdSector = sector->mbr.partitions[13].offset;
	if (sector->mbr.partitions[14].code == 13)
		mediaIdSector = sector->mbr.partitions[14].offset;
	if (sector->mbr.partitions[15].code == 13)
		mediaIdSector = sector->mbr.partitions[15].offset;
	if (!mediaIdSector)
		return 0x80010002;
	result = sddev_read(device, sector, mediaIdSector, 1);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 15, 1262, "0x%08x\n", result);
		return result;
	}
	return mediaIdSector;
}

#if !FW_DEBUG
static inline
#endif
int sdintr_mount_gc(SceSdstorIntr *interrupt)
{
	SceSdstorSddev *device;
	int i, id, result, permission, mounted = 0;

	SCE_DBG_ASSERT(interrupt != NULL, 1506);
	device = sddev_get_object(interrupt->port);
	for (i = 0; i != 3; ++i) {
		id = gameCardMountIds[i];
		if (mounted == 1 && id == 0x100)
			continue;
		result = sceIoSetMountEvent(id, 0x100, 0, 0);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(2, 15, 1523,
					     "ret = 0x%08x, i = %d\n", result, i);
		permission = id == 0xa00 || !sddev_is_read_write(device) ? 0 : 0x1000;
		result = SceIofilemgrForDriver_D070BC48(id, NULL, permission, NULL, 0);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(2, 15, 1531, "0x%08x, i = %d\n", result, i);
		else if (!result && (id == 0x900 || id == 0xa00))
			mounted = 1;
	}
	return 0;
}

#if !FW_DEBUG
static inline
#endif
int sdintr_notify_rm(SceSdstorIntr *interrupt)
{
	unsigned int port, mcPort;
	int result;

	SCE_DBG_ASSERT(interrupt != NULL, 1862);
	port = interrupt->port;
	mcPort = sdpart_get_mc_port();
	if (!mcPort && port == 2) {
		result = vfsNotifyDeviceStatus(0x800);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(2, 15, 1872, "ret = 0x%08x\n", result);
		result = vfsNotifyDeviceStatus(0xd00);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(2, 15, 1876, "ret = 0x%08x\n", result);
	} else if (mcPort == port) {
		result = vfsNotifyDeviceStatus(0x800);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(2, 15, 1881, "ret = 0x%08x\n", result);
	}
	if (sdpart_get_xmc_port() == port) {
		result = vfsNotifyDeviceStatus(0xe00);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(2, 15, 1887, "ret = 0x%08x\n", result);
	}
	return 0;
}

int sdintr_mount_rm(SceSdstorIntr *interrupt)
{
	unsigned int port;
	int mountId, result;

	SCE_DBG_ASSERT(interrupt != NULL, 1814);
	port = interrupt->port;
	if (SceSysrootForDriver_55392965() || !SceSysrootForDriver_50FE3B4D()) {
		if (sdpart_grab_mc(port) != port)
			return 0;
		mountId = 0x800;
	} else {
		if (sdpart_grab_xmc(port) != port)
			return 0;
		mountId = 0xe00;
	}
	result = sceIoSetMountEvent(mountId, 0x100, 0, 0);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 15, 1836, "ret = 0x%08x\n", result);
	if ((SceSysrootForDriver_55392965() || !SceSysrootForDriver_50FE3B4D()) &&
	    !(interrupt->mountDisabled & 1)) {
		result = SceIofilemgrForDriver_D070BC48(mountId, NULL, 0, NULL, 0);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(2, 15, 1852, "0x%08x\n", result);
	}
	return 0;
}

#if !FW_DEBUG
static inline
#endif
int sdintr_rmdev(SceSdstorIntr *interrupt)
{
	IofilemgrRmdevParam param;
	SceSdstorPartitionEntry *partition;
	char name[12];
	unsigned int port;
	int i, result;

	SCE_DBG_ASSERT(interrupt != NULL, 1144);
	port = interrupt->port;
	SCE_DBG_ASSERT(port - 1 <= 3, 1153);
	for (i = 0; i != 17; ++i) {
		__builtin_memcpy(name, "sdstor0:999", sizeof(name));
		snprintf(name, 4, "%01d%02d", port, i);
		partition = sdpart_get_object(name, 3);
		if (partition && partition->sdpart) {
			snprintf(name, sizeof(name), "sdstor0:%01d%02d", port, i);
			param.dev = name;
			param.flags = 0;
			result = vfsRmdev(&param);
			if (result < 0)
				SCE_DBG_PRINTF_LEVEL(2, 15, 1173,
					"vfsRmdev(%s) returns 0x%08x\n", param.dev, result);
		}
	}
	return 0;
}

int sdintr_umount_gc(SceSdstorIntr *interrupt)
{
	unsigned int port;
	int i, result;

	SCE_DBG_ASSERT(interrupt != NULL, 1471);
	port = interrupt->port;
	for (i = 0; i != 3; ++i) {
		result = SceIofilemgrForDriver_20574100(gameCardMountIds[i], 1, 0);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(2, 15, 1481, "0x%08x, i = %d\n", result, i);
	}
	result = sdintr_rmdev(interrupt);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 15, 1486,
				     "sdintr_rmdev() returns 0x%08x\n", result);
	result = sdpart_remove(port);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(1, 15, 1490,
				     "sdpart_remove() returns 0x%08x\n", result);
#if FW_DEBUG
	for (i = 0; i != 3; ++i) {
		result = sceIoSetMountEvent(gameCardMountIds[i], 0x200, 0, 0);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(2, 15, 1496,
					     "ret = 0x%08x, i = %d\n", result, i);
	}
#else
	sceIoSetMountEvent(0x900, 0x200, 0, 0);
	sceIoSetMountEvent(0xa00, 0x200, 0, 0);
	sceIoSetMountEvent(0x100, 0x200, 0, 0);
#endif
	return 0;
}

int sdintr_umount_rm(SceSdstorIntr *interrupt)
{
	unsigned int port, selectedPort;
	int mountId = 0x800, isMc = 1, result;

	SCE_DBG_ASSERT(interrupt != NULL, 1764);
	port = interrupt->port;
	selectedPort = sdpart_get_mc_port();
	if (!selectedPort) {
		isMc = 0;
		mountId = 0xe00;
		selectedPort = sdpart_get_xmc_port();
	}
	if (selectedPort == port) {
		result = SceIofilemgrForDriver_20574100(mountId, 1, 0);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(2, 15, 1779, "0x%08x\n", result);
	}
	result = sdintr_rmdev(interrupt);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(1, 15, 1784,
				     "sdintr_rmdev() returns 0x%08x\n", result);
	result = sdpart_remove(port);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(1, 15, 1788,
				     "sdpart_remove() returns 0x%08x\n", result);
	if (selectedPort == port) {
		result = sceIoSetMountEvent(mountId, 0x200, 0, 0);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(2, 15, 1793, "ret = 0x%08x\n", result);
		if (isMc) {
			result = sdpart_release_mc(selectedPort);
			if (result < 0)
				SCE_DBG_PRINTF_LEVEL(2, 15, 1799, "ret = 0x%08x\n", result);
			return 0;
		}
	}
	result = sdpart_release_xmc(port);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 15, 1804, "ret = 0x%08x\n", result);
	return 0;
}

int sdintr_suspend_usd(SceSdstorIntr *interrupt)
{
	SceSdstorSddev *device;
	SceSdstorIntrHeader *header;
	unsigned int port, headerPort, irqPort;
	uint32_t cid[4], detectState;
	int result;

	SCE_DBG_ASSERT(interrupt != NULL, 2018);
	port = interrupt->port;
	device = sddev_get_object(port);
	SCE_DBG_ASSERT_FUNC(port <= 4, 763, "sdintr_get_irq");
	headerPort = sdintrHeaderPorts[port];
	SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2234, "sdintr_hdr_get_object");
	header = &sdintrHeaders[headerPort];
	SCE_DBG_ASSERT_FUNC(1, 2437, "sdintr_hdr_disable_insert_interrupt");
	irqPort = header->port;
	SCE_DBG_ASSERT_FUNC(irqPort <= 4, 2242, "sdintr_hdr_get_irq");
	result = sceKernelDisableSubIntr(sdintrIrqNumbers[irqPort], header->insertSubIntr);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2445,
			"sdintr_hdr_disable_insert_interrupt", "0x%08x\n", result);
		SCE_DBG_PRINTF_LEVEL(1, 15, 2028, "0x%08x\n", result);
	}
	if (interrupt->stat == 1) {
		cid[0] = 0;
		cid[1] = 0;
		cid[2] = 0;
		cid[3] = 0;
		SCE_DBG_ASSERT_FUNC(1, 2220, "sdintr_get_sd_cid");
		ksceSdifGetCID(device->sdif_ctx, cid);
		if (!cid[0] && !cid[1] && !cid[2] && !cid[3])
			memset(cid, 250, sizeof(cid));
		interrupt->cid[0] = cid[0];
		interrupt->cid[1] = cid[1];
		interrupt->cid[2] = cid[2];
		interrupt->cid[3] = cid[3];
	}
	result = sddev_suspend(device);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(1, 15, 2038,
				     "sddev_suspend() returns 0x%08x\n", result);
	if (port == 4)
		SceSysconForDriver_F99BC858(&detectState);
	return 0;
}

int sdintr_remove(SceSdstorIntr *interrupt)
{
	SceSdstorSddev *device;
	unsigned int port;
	int result;

	SCE_DBG_ASSERT(interrupt != NULL, 966);
	if (!interrupt->stat) {
		SCE_DBG_PRINTF_LEVEL(1, 15, 973, "stat = %d\n", interrupt->stat);
		return 0;
	}
	switch (interrupt->port) {
	case 1:
		if (SceSysrootForDriver_F804F761())
			goto removeUsd;
		SCE_DBG_ASSERT_FUNC(interrupt != NULL, 1272, "sdintr_remove_gc");
		device = sddev_get_object(interrupt->port);
		result = sdintr_umount_gc(interrupt);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1280, "sdintr_remove_gc",
				"sdintr_umount_gc() returns 0x%08x\n", result);
		result = sddev_remove(device);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1284, "sdintr_remove_gc",
				"sddev_remove() returns 0x%08x\n", result);
		break;
	case 2:
		SCE_DBG_ASSERT_FUNC(interrupt != NULL, 1565, "sdintr_remove_rm");
		device = sddev_get_object(interrupt->port);
		result = sdintr_umount_rm(interrupt);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1573, "sdintr_remove_rm",
				"0x%08x\n", result);
		result = sddev_remove(device);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1577, "sdintr_remove_rm",
				"sddev_remove() returns 0x%08x\n", result);
		break;
	case 3:
		SCE_DBG_ASSERT_FUNC(interrupt != NULL, 1897, "sdintr_remove_usb");
		device = sddev_get_object(interrupt->port);
		SCE_DBG_ASSERT_FUNC(interrupt != NULL, 1941, "sdintr_umount_usb");
		port = interrupt->port;
		result = SceIofilemgrForDriver_20574100(0xf00, 1, 0);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1949, "sdintr_umount_usb",
				"0x%08x\n", result);
		result = sdintr_rmdev(interrupt);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1953, "sdintr_umount_usb",
				"sdintr_rmdev() returns 0x%08x\n", result);
		result = sdpart_remove(port);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1957, "sdintr_umount_usb",
				"sdpart_remove() returns 0x%08x\n", result);
		result = sceIoSetMountEvent(0xf00, 0x200, 0, 0);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1961, "sdintr_umount_usb",
				"ret = 0x%08x\n", result);
		result = sddev_remove(device);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1909, "sdintr_remove_usb",
				"sddev_remove() returns 0x%08x\n", result);
		break;
	case 4:
removeUsd:
		SCE_DBG_ASSERT_FUNC(interrupt != NULL, 1970, "sdintr_remove_usd");
		device = sddev_get_object(interrupt->port);
		result = sdintr_umount_rm(interrupt);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1978, "sdintr_remove_usd",
				"0x%08x\n", result);
		result = sddev_remove(device);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1982, "sdintr_remove_usd",
				"sddev_remove() returns 0x%08x\n", result);
		break;
	default:
		break;
	}
	memset(interrupt->cid, 0, sizeof(interrupt->cid));
	if (interrupt->stat != 2)
		interrupt->stat = 0;
	return 0;
}

int sdctrl_system_event_handler(int resume, int eventId, void *eventArgs, void *opt)
{
	SceSdstorControl *control = opt;
	SceSdstorSddev *gameCard, *memoryCard;
	unsigned int port;
	int result;

	if (resume != 0) {
		if (resume != 1)
			return 0;
		goto resumeEvents;
	}
	if (eventId == 0x200) {
		SCE_DBG_ASSERT_FUNC(1, 755, "sdintr_get_object");
		SCE_DBG_ASSERT_FUNC(1, 755, "sdintr_get_object");
		SCE_DBG_ASSERT_FUNC(1, 755, "sdintr_get_object");
		result = sceKernelSetEventFlag(sdintrObjects[1].requestEvent, 0x100);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 585, "sdctrl_suspend",
				"sceKernelSetEventFlag() returns 0x%08x\n", result);
		result = sceKernelSetEventFlag(sdintrObjects[2].requestEvent, 0x100);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 590, "sdctrl_suspend",
				"sceKernelSetEventFlag() returns 0x%08x\n", result);
		if (SceSysrootForDriver_8AA268D6(11)) {
			result = sceKernelSetEventFlag(sdintrObjects[4].requestEvent, 0x100);
			if (result < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 596, "sdctrl_suspend",
					"sceKernelSetEventFlag() returns 0x%08x\n", result);
		}
		result = sceKernelWaitEventFlag(sdintrObjects[1].syncEvent, 0x100,
						5, NULL, NULL);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 607, "sdctrl_suspend",
				"sceKernelWaitEventFlag() returns 0x%08x\n", result);
		result = sceKernelWaitEventFlag(sdintrObjects[2].syncEvent, 0x100,
						5, NULL, NULL);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 617, "sdctrl_suspend",
				"sceKernelWaitEventFlag() returns 0x%08x\n", result);
		if (SceSysrootForDriver_8AA268D6(11)) {
			result = sceKernelWaitEventFlag(sdintrObjects[4].syncEvent,
							0x100, 5, NULL, NULL);
			if (result < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 628, "sdctrl_suspend",
					"sceKernelWaitEventFlag() returns 0x%08x\n", result);
		}
		return 0;
	}
	if (eventId == 0x400d) {
		SCE_DBG_ASSERT_FUNC(1, 763, "sdintr_get_irq");
		SCE_DBG_ASSERT_FUNC(1, 2234, "sdintr_hdr_get_object");
		SCE_DBG_ASSERT_FUNC(1, 763, "sdintr_get_irq");
		SCE_DBG_ASSERT_FUNC(1, 2234, "sdintr_hdr_get_object");
		gameCard = sddev_get_object(1);
		memoryCard = sddev_get_object(2);
		SCE_DBG_ASSERT_FUNC(1, 2419, "sdintr_hdr_disable_remove_interrupt");
		port = sdintrHeaders[1].port;
		SCE_DBG_ASSERT_FUNC(port <= 4, 2242, "sdintr_hdr_get_irq");
		result = sceKernelDisableSubIntr(sdintrIrqNumbers[port],
						 sdintrHeaders[1].removeSubIntr);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2427,
				"sdintr_hdr_disable_remove_interrupt", "0x%08x\n", result);
			SCE_DBG_PRINTF_LEVEL(1, 15, 431, "0x%08x\n", result);
		}
		result = sddev_power_off(gameCard);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(1, 15, 435, "0x%08x\n", result);
		SCE_DBG_ASSERT_FUNC(1, 2419, "sdintr_hdr_disable_remove_interrupt");
		port = sdintrHeaders[2].port;
		SCE_DBG_ASSERT_FUNC(port <= 4, 2242, "sdintr_hdr_get_irq");
		result = sceKernelDisableSubIntr(sdintrIrqNumbers[port],
						 sdintrHeaders[2].removeSubIntr);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2427,
				"sdintr_hdr_disable_remove_interrupt", "0x%08x\n", result);
			SCE_DBG_PRINTF_LEVEL(1, 15, 439, "0x%08x\n", result);
		}
		result = sddev_power_off(memoryCard);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(1, 15, 443, "0x%08x\n", result);
		return result;
	}
resumeEvents:
	if (eventId == 0x100000) {
		if (control->resumeHandled)
			return 0;
		control->resumeHandled = 1;
		SCE_DBG_ASSERT_FUNC(1, 755, "sdintr_get_object");
		SCE_DBG_ASSERT_FUNC(1, 755, "sdintr_get_object");
		SCE_DBG_ASSERT_FUNC(1, 755, "sdintr_get_object");
		result = sceKernelSetEventFlag(sdintrObjects[1].requestEvent, 0x1000);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 649, "sdctrl_resume",
				"sceKernelSetEventFlag() returns 0x%08x\n", result);
		result = sceKernelSetEventFlag(sdintrObjects[2].requestEvent, 0x1000);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 654, "sdctrl_resume",
				"sceKernelSetEventFlag() returns 0x%08x\n", result);
		if (SceSysrootForDriver_8AA268D6(11)) {
			result = sceKernelSetEventFlag(sdintrObjects[4].requestEvent, 0x1000);
			if (result < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 660, "sdctrl_resume",
					"sceKernelSetEventFlag() returns 0x%08x\n", result);
		}
		result = sceKernelWaitEventFlag(sdintrObjects[1].syncEvent, 0x1000,
						5, NULL, NULL);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 671, "sdctrl_resume",
				"sceKernelWaitEventFlag() returns 0x%08x\n", result);
		result = sceKernelWaitEventFlag(sdintrObjects[2].syncEvent, 0x1000,
						5, NULL, NULL);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 681, "sdctrl_resume",
				"sceKernelWaitEventFlag() returns 0x%08x\n", result);
		if (SceSysrootForDriver_8AA268D6(11)) {
			result = sceKernelWaitEventFlag(sdintrObjects[4].syncEvent,
							0x1000, 5, NULL, NULL);
			if (result < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 692, "sdctrl_resume",
					"sceKernelWaitEventFlag() returns 0x%08x\n", result);
		}
	} else if (eventId == 0x400000) {
		control->resumeHandled = 0;
	}
	return 0;
}

int sdintr_resume_usd(SceSdstorIntr *interrupt)
{
	SceSdstorSddev *device;
	SceSdstorIntrHeader *header;
	uint32_t cid[4] = {0};
	uint32_t cardCid[4] = {0};
	uint32_t swapped = 0;
	unsigned int port, headerPort;
	int result, status, irq;

	SCE_DBG_ASSERT_FUNC(interrupt != NULL, 2053, "sdintr_resume_usd");
	port = interrupt->port;
	device = sddev_get_object(port);
	SCE_DBG_ASSERT_FUNC(port <= 4, 763, "sdintr_get_irq");
	headerPort = sdintrHeaderPorts[port];
	SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2234, "sdintr_hdr_get_object");
	header = &sdintrHeaders[headerPort];
	result = sdpart_resume_stat(port);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 2065, "sdintr_resume_usd",
			"sdpart_resume() returns 0x%08x\n", result);
		goto failed;
	}
	result = sddev_resume(device);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 2070, "sdintr_resume_usd",
			"sddev_resume() returns 0x%08x\n", result);
		goto failed;
	}
	if (port == 4) {
		result = SceSysconForDriver_F99BC858(&swapped);
		if (result < 0 || (swapped & 1)) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 2078, "sdintr_resume_usd",
				"microSD swapped %d 0x%08x, \n", swapped, result);
			goto failed;
		}
	}
	SCE_DBG_ASSERT_FUNC(1, 2220, "sdintr_get_sd_cid");
	result = ksceSdifGetCID(device->sdif_ctx, cardCid);
	if (!cardCid[0] && !cardCid[1] && !cardCid[2] && !cardCid[3])
		memset(cardCid, 0xfa, sizeof(cardCid));
	__builtin_memcpy(cid, cardCid, sizeof(cid));
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 2086, "sdintr_resume_usd",
			"sddev_get_sd_cid() returns 0x%08x\n", result);
		goto failed;
	}
	if (memcmp(cid, interrupt->cid, sizeof(cid))) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 2094, "sdintr_resume_usd",
			"cid mismatch\n");
		if (interrupt->cid[0] || interrupt->cid[1] ||
		    interrupt->cid[2] || interrupt->cid[3]) {
			status = sdintr_umount_rm(interrupt);
			if (status < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2100,
					"sdintr_resume_usd", "0x%08x\n", status);
		}
		status = sdpart_insert(port);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2106, "sdintr_resume_usd",
				"sdpart_insert() returns 0x%08x\n", status);
		status = sdintr_mount_rm(interrupt);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2111, "sdintr_resume_usd",
				"0x%08x\n", status);
	} else {
		result = 0;
	}
	goto notify;
failed:
	if (interrupt->cid[0] || interrupt->cid[1] ||
	    interrupt->cid[2] || interrupt->cid[3]) {
		status = sdintr_umount_rm(interrupt);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2119, "sdintr_resume_usd",
				"0x%08x\n", status);
		status = sdpart_insert(port);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2123, "sdintr_resume_usd",
				"sdpart_insert() returns 0x%08x\n", status);
	}
notify:
	memset(interrupt->cid, 0, sizeof(interrupt->cid));
	status = sdintr_notify_rm(interrupt);
	if (status < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2131, "sdintr_resume_usd",
			"0x%08x\n", status);
	SCE_DBG_ASSERT_FUNC(1, 2373, "sdintr_hdr_enable_interrupt");
	port = header->port;
	SCE_DBG_ASSERT_FUNC(port <= 4, 2242, "sdintr_hdr_get_irq");
	irq = sdintrIrqNumbers[port];
	status = sceKernelEnableSubIntr(irq, header->insertSubIntr);
	if (status < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2381,
			"sdintr_hdr_enable_interrupt", "0x%08x\n", status);
	} else {
		status = sceKernelEnableSubIntr(irq, header->removeSubIntr);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2387,
				"sdintr_hdr_enable_interrupt", "0x%08x\n", status);
	}
	if (status < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2135, "sdintr_resume_usd",
			"0x%08x\n", status);
	return result;
}

int sdintr_thr_entry(SceSize args, void *argp)
{
	SceSdstorIntr *interrupt;
	SceSdstorIntrHeader *header;
	SceSdstorSddev *device;
	SceSdstorPartitionEntry *partition;
	SceSdstorMediaInfo info;
	uint32_t returned, waitPattern, requested, processed;
	uint32_t savedCid[4];
	unsigned int port, headerPort, state, operationPort;
	int result, status, irq, retries, sector, index;

	SCE_DBG_ASSERT_FUNC(args == 1 && argp != NULL, 2580, "sdintr_thr_entry");
	port = *(uint8_t *)argp;
	SCE_DBG_ASSERT_FUNC(port <= 4, 755, "sdintr_get_object");
	interrupt = &sdintrObjects[port];
	if (SceSysrootForDriver_55392965()) {
		device = sddev_get_object(interrupt->port);
		if (sddev_power_on(device) == (int)0x80010013)
			sddev_remove(device);
		else
			interrupt->stat = 1;
	}
	header = interrupt->header;
	SCE_DBG_ASSERT_FUNC(header != NULL, 2373, "sdintr_hdr_enable_interrupt");
	port = header->port;
	SCE_DBG_ASSERT_FUNC(port <= 4, 2242, "sdintr_hdr_get_irq");
	irq = sdintrIrqNumbers[port];
	result = sceKernelEnableSubIntr(irq, header->insertSubIntr);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2381,
			"sdintr_hdr_enable_interrupt", "0x%08x\n", result);
	} else {
		result = sceKernelEnableSubIntr(irq, header->removeSubIntr);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2387,
				"sdintr_hdr_enable_interrupt", "0x%08x\n", result);
	}
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2599, "sdintr_thr_entry",
			"sdintr_hdr_enable_interrupt() returns 0x%08x\n", result);
	for (;;) {
		SCE_DBG_ASSERT_FUNC(1, 826, "sdintr_entry");
		state = interrupt->stat;
		waitPattern = sdintrStateRequests[state];
		returned = 0;
		result = sceKernelWaitEventFlag(interrupt->requestEvent, 0x1113,
						5, &returned, NULL);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 849, "sdintr_entry",
				"sceKernelWaitEventFlag() returns 0x%08x\n", result);
		for (retries = 5; returned && retries; retries--) {
			if ((returned & 1) && state == 1) {
				device = sddev_get_object(interrupt->port);
				if (!sddev_check_removal(device)) {
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 861,
						"sdintr_entry", "Ignore removal interrupt\n");
					returned &= ~1u;
				}
			}
			if ((returned & 2) && state == 1)
				returned = (returned & ~2u) | 1;
			if ((returned & 0x10) && state != 2) {
				returned &= ~1u;
				if (state == 1) {
					SCE_DBG_ASSERT_FUNC(1, 955, "sdintr_check_connection");
					device = sddev_get_object(interrupt->port);
					if (!sddev_read(device, &connectionProbeSector, 0, 1)) {
						returned &= ~0x10u;
					} else {
						state = 0;
						interrupt->stat = 0;
					}
				}
				waitPattern = sdintrStateRequests[state];
			}
			requested = returned & waitPattern;
			if (requested & 0x100)
				processed = 0x100;
			else if (requested & 0x1000)
				processed = 0x1000;
			else if (requested & 0x10)
				processed = 0x10;
			else
				processed = requested & 1;
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 903, "sdintr_entry",
				"port = %d, ret_pat = 0x%08x, wait_pat = 0x%08x, req_pat = 0x%08x, do_pat = 0x%08x\n",
				interrupt->port, returned, waitPattern, requested, processed);
			if (processed == 0x100) {
				SCE_DBG_ASSERT_FUNC(1, 1062, "sdintr_suspend");
				port = interrupt->port;
				if (interrupt->stat > 1)
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1069,
						"sdintr_suspend", "stat = %d\n", interrupt->stat);
				switch (port) {
				case 1:
					if (SceSysrootForDriver_F804F761())
						goto suspendUsd;
					SCE_DBG_ASSERT_FUNC(1, 1340, "sdintr_suspend_gc");
					port = interrupt->port;
					device = sddev_get_object(port);
					SCE_DBG_ASSERT_FUNC(port <= 4, 763, "sdintr_get_irq");
					headerPort = sdintrHeaderPorts[port];
					SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2234,
						"sdintr_hdr_get_object");
					header = &sdintrHeaders[headerPort];
					SCE_DBG_ASSERT_FUNC(1, 2437,
						"sdintr_hdr_disable_insert_interrupt");
					headerPort = header->port;
					SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2242, "sdintr_hdr_get_irq");
					result = sceKernelDisableSubIntr(sdintrIrqNumbers[headerPort],
									 header->insertSubIntr);
					if (result < 0) {
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2445,
							"sdintr_hdr_disable_insert_interrupt", "0x%08x\n", result);
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1351,
							"sdintr_suspend_gc", "0x%08x\n", result);
					}
					partition = sdpart_get_object("gcd-lp-act-mediaid", 18);
					if (!partition)
						goto suspendGcWithoutIdentity;
					memset(&gameCardMediaSector, 0, sizeof(gameCardMediaSector));
					do {
						if (ksceSblRngPseudoRandomNumber(interrupt->cid, 16))
							goto suspendGcWithoutIdentity;
					} while (!interrupt->cid[0] && !interrupt->cid[1] &&
						 !interrupt->cid[2] && !interrupt->cid[3]);
					__builtin_memcpy(gameCardMediaSector.identity.cid,
							 interrupt->cid, sizeof(interrupt->cid));
					result = sddev_write(device, &gameCardMediaSector,
							     partition->start_lba, 1);
					if (result < 0)
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1363,
							"sdintr_suspend_gc", "0x%08x\n", result);
					result = sdpart_suspend(port);
					if (result < 0)
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1367, "sdintr_suspend_gc",
							"sdpart_suspend() returns 0x%08x\n", result);
					break;
suspendGcWithoutIdentity:
					if (interrupt->stat == 1) {
						result = sdintr_umount_gc(interrupt);
						if (result < 0)
							SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1376,
								"sdintr_suspend_gc", "0x%08x\n", result);
					}
					result = sddev_suspend(device);
					if (result < 0)
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1381, "sdintr_suspend_gc",
							"sddev_suspend() returns 0x%08x\n", result);
					break;
				case 2:
					SCE_DBG_ASSERT_FUNC(1, 1633, "sdintr_suspend_rm");
					port = interrupt->port;
					device = sddev_get_object(port);
					SCE_DBG_ASSERT_FUNC(port <= 4, 763, "sdintr_get_irq");
					headerPort = sdintrHeaderPorts[port];
					SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2234, "sdintr_hdr_get_object");
					header = &sdintrHeaders[headerPort];
					SCE_DBG_ASSERT_FUNC(1, 2437, "sdintr_hdr_disable_insert_interrupt");
					headerPort = header->port;
					SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2242, "sdintr_hdr_get_irq");
					result = sceKernelDisableSubIntr(sdintrIrqNumbers[headerPort],
									 header->insertSubIntr);
					if (result < 0) {
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2445,
							"sdintr_hdr_disable_insert_interrupt", "0x%08x\n", result);
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1644,
							"sdintr_suspend_rm", "0x%08x\n", result);
					}
					partition = sdpart_get_object("mcd-lp-act-mediaid", 18);
					if (!partition)
						goto suspendRmWithoutIdentity;
					memset(&memoryCardMediaSector, 0, sizeof(memoryCardMediaSector));
					do {
						if (ksceSblRngPseudoRandomNumber(interrupt->cid, 16))
							goto suspendRmWithoutIdentity;
					} while (!interrupt->cid[0] && !interrupt->cid[1] &&
						 !interrupt->cid[2] && !interrupt->cid[3]);
					__builtin_memcpy(memoryCardMediaSector.identity.cid,
							 interrupt->cid, sizeof(interrupt->cid));
					result = sddev_write(device, &memoryCardMediaSector,
							     partition->start_lba, 1);
					if (result < 0)
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1656,
							"sdintr_suspend_rm", "0x%08x\n", result);
					result = sdpart_suspend(port);
					if (result < 0)
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1660, "sdintr_suspend_rm",
							"sdpart_suspend() returns 0x%08x\n", result);
					break;
suspendRmWithoutIdentity:
					if (interrupt->stat == 1) {
						result = sdintr_umount_rm(interrupt);
						if (result < 0)
							SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1669,
								"sdintr_suspend_rm", "0x%08x\n", result);
					}
					result = sddev_suspend(device);
					if (result < 0)
						SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1674, "sdintr_suspend_rm",
							"sddev_suspend() returns 0x%08x\n", result);
					break;
				case 4:
suspendUsd:
					result = sdintr_suspend_usd(interrupt);
					interrupt->stat = 2;
					if (result < 0)
						SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 908, "sdintr_entry",
							"sdintr_suspend() returns 0x%08x\n", result);
					goto acknowledge;
				default:
					break;
				}
				interrupt->stat = 2;
			} else if (processed == 0x1000) {
				goto resumeMedia;
			} else if (processed == 0x10) {
				goto insertMedia;
			} else if (processed) {
				result = sdintr_remove(interrupt);
				if (result < 0)
					SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 929, "sdintr_entry",
						"sdintr_remove() returns 0x%08x\n", result);
			}
acknowledge:
			if (!processed)
				processed = returned;
			returned &= ~processed;
			result = sceKernelSetEventFlag(interrupt->syncEvent, processed);
			if (result)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 941, "sdintr_entry",
					"0x%08x\n", result);
		}
	}

resumeMedia:
	SCE_DBG_ASSERT_FUNC(1, 1098, "sdintr_resume");
	port = interrupt->port;
	operationPort = port;
	if (interrupt->stat != 2)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1106, "sdintr_resume",
			"stat = %d\n", interrupt->stat);
	if (port == 1 && !SceSysrootForDriver_F804F761())
		goto resumeGc;
	if (port == 2)
		goto resumeRm;
	if (port == 4 || port == 1) {
		result = sdintr_resume_usd(interrupt);
		goto resumeInfo;
	}
	goto acknowledge;
resumeInfo:
	if (result)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1128, "sdintr_resume", "0x%08x\n", result);
	device = sddev_get_object(operationPort);
	interrupt->stat = sddev_get_info(device, &info) == 0;
	goto acknowledge;

resumeGc:
	SCE_DBG_ASSERT_FUNC(1, 1390, "sdintr_resume_gc");
	port = interrupt->port;
	device = sddev_get_object(port);
	SCE_DBG_ASSERT_FUNC(port <= 4, 763, "sdintr_get_irq");
	headerPort = sdintrHeaderPorts[port];
	SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2234, "sdintr_hdr_get_object");
	header = &sdintrHeaders[headerPort];
	result = sdpart_resume_stat(port);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1402, "sdintr_resume_gc",
			"sdpart_resume() returns 0x%08x\n", result);
		goto resumeGcFailed;
	}
	result = sddev_resume(device);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1407, "sdintr_resume_gc",
			"sddev_resume() returns 0x%08x\n", result);
		goto resumeGcFailed;
	}
	sector = sdintr_read_mid(interrupt);
	if (sector > 0) {
		__builtin_memcpy(savedCid, gameCardMediaSector.identity.cid, sizeof(savedCid));
		memset(&gameCardMediaSector, 0, sizeof(gameCardMediaSector));
		status = sddev_write(device, &gameCardMediaSector, sector, 1);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1418, "sdintr_resume_gc", "0x%08x\n", status);
		if (savedCid[0] == interrupt->cid[0] && savedCid[1] == interrupt->cid[1] &&
		    savedCid[2] == interrupt->cid[2] && savedCid[3] == interrupt->cid[3] &&
		    (savedCid[0] || savedCid[1] || savedCid[2] || savedCid[3])) {
			result = 0;
			goto resumeGcNotify;
		}
	}
	if (interrupt->cid[0] || interrupt->cid[1] || interrupt->cid[2] || interrupt->cid[3]) {
		status = sdintr_umount_gc(interrupt);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1429, "sdintr_resume_gc", "0x%08x\n", status);
	}
	status = sdpart_insert(port);
	if (status < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1434, "sdintr_resume_gc",
			"sdpart_insert() returns 0x%08x\n", result);
	status = sdintr_mount_gc(interrupt);
	if (status < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1438, "sdintr_resume_gc", "0x%08x\n", status);
	goto resumeGcNotify;
resumeGcFailed:
	if (interrupt->cid[0] || interrupt->cid[1] || interrupt->cid[2] || interrupt->cid[3]) {
		status = sdintr_umount_gc(interrupt);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1446, "sdintr_resume_gc", "0x%08x\n", status);
		if (sdpart_insert(port) < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1450, "sdintr_resume_gc",
				"sdpart_insert() returns 0x%08x\n", result);
	}
resumeGcNotify:
	memset(interrupt->cid, 0, sizeof(interrupt->cid));
	SCE_DBG_ASSERT_FUNC(1, 1547, "sdintr_notify_gc");
	for (index = 0; index != 3; index++) {
		status = vfsNotifyDeviceStatus(gameCardMountIds[index]);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1555, "sdintr_notify_gc",
				"ret = 0x%08x, i = %d\n", status, index);
	}
	SCE_DBG_ASSERT_FUNC(1, 2373, "sdintr_hdr_enable_interrupt");
	headerPort = header->port;
	SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2242, "sdintr_hdr_get_irq");
	irq = sdintrIrqNumbers[headerPort];
	status = sceKernelEnableSubIntr(irq, header->insertSubIntr);
	if (status < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2381, "sdintr_hdr_enable_interrupt", "0x%08x\n", status);
	} else {
		status = sceKernelEnableSubIntr(irq, header->removeSubIntr);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2387, "sdintr_hdr_enable_interrupt", "0x%08x\n", status);
	}
	if (status < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1462, "sdintr_resume_gc", "0x%08x\n", status);
	goto resumeInfo;

resumeRm:
	SCE_DBG_ASSERT_FUNC(1, 1683, "sdintr_resume_rm");
	port = interrupt->port;
	device = sddev_get_object(port);
	SCE_DBG_ASSERT_FUNC(port <= 4, 763, "sdintr_get_irq");
	headerPort = sdintrHeaderPorts[port];
	SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2234, "sdintr_hdr_get_object");
	header = &sdintrHeaders[headerPort];
	result = sdpart_resume_stat(port);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1695, "sdintr_resume_rm",
			"sdpart_resume() returns 0x%08x\n", result);
		goto resumeRmFailed;
	}
	result = sddev_resume(device);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1700, "sdintr_resume_rm",
			"sddev_resume() returns 0x%08x\n", result);
		goto resumeRmFailed;
	}
	sector = sdintr_read_mid(interrupt);
	if (sector > 0) {
		__builtin_memcpy(savedCid, memoryCardMediaSector.identity.cid, sizeof(savedCid));
		memset(&memoryCardMediaSector, 0, sizeof(memoryCardMediaSector));
		status = sddev_write(device, &memoryCardMediaSector, sector, 1);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1711, "sdintr_resume_rm", "0x%08x\n", status);
		if (savedCid[0] == interrupt->cid[0] && savedCid[1] == interrupt->cid[1] &&
		    savedCid[2] == interrupt->cid[2] && savedCid[3] == interrupt->cid[3] &&
		    (savedCid[0] || savedCid[1] || savedCid[2] || savedCid[3])) {
			result = 0;
			goto resumeRmNotify;
		}
	}
	if (interrupt->cid[0] || interrupt->cid[1] || interrupt->cid[2] || interrupt->cid[3]) {
		status = sdintr_umount_rm(interrupt);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1722, "sdintr_resume_rm", "0x%08x\n", status);
	}
	status = sdpart_insert(port);
	if (status < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1727, "sdintr_resume_rm",
			"sdpart_insert() returns 0x%08x\n", status);
	status = sdintr_mount_rm(interrupt);
	if (status < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1731, "sdintr_resume_rm", "0x%08x\n", status);
	goto resumeRmNotify;
resumeRmFailed:
	if (interrupt->cid[0] || interrupt->cid[1] || interrupt->cid[2] || interrupt->cid[3]) {
		status = sdintr_umount_rm(interrupt);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1739, "sdintr_resume_rm", "0x%08x\n", status);
		status = sdpart_insert(port);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1743, "sdintr_resume_rm",
				"sdpart_insert() returns 0x%08x\n", status);
	}
resumeRmNotify:
	memset(interrupt->cid, 0, sizeof(interrupt->cid));
	status = sdintr_notify_rm(interrupt);
	if (status < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1751, "sdintr_resume_rm", "0x%08x\n", status);
	SCE_DBG_ASSERT_FUNC(1, 2373, "sdintr_hdr_enable_interrupt");
	headerPort = header->port;
	SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2242, "sdintr_hdr_get_irq");
	irq = sdintrIrqNumbers[headerPort];
	status = sceKernelEnableSubIntr(irq, header->insertSubIntr);
	if (status < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2381, "sdintr_hdr_enable_interrupt", "0x%08x\n", status);
	} else {
		status = sceKernelEnableSubIntr(irq, header->removeSubIntr);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2387, "sdintr_hdr_enable_interrupt", "0x%08x\n", status);
	}
	if (status < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1755, "sdintr_resume_rm", "0x%08x\n", status);
	goto resumeInfo;

insertMedia:
	SCE_DBG_ASSERT_FUNC(1, 1009, "sdintr_insert");
	port = interrupt->port;
	operationPort = port;
	if (interrupt->stat) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1017, "sdintr_insert",
			"stat = %d\n", interrupt->stat);
		goto acknowledge;
	}
	switch (port) {
	case 1:
		if (SceSysrootForDriver_F804F761())
			goto insertUsd;
		SCE_DBG_ASSERT_FUNC(1, 1293, "sdintr_insert_gc");
		port = interrupt->port;
		device = sddev_get_object(port);
		result = sddev_insert(device);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1302, "sdintr_insert_gc",
				"sddev_insert() returns 0x%08x\n", result);
			break;
		}
		result = sdpart_insert(port);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1307, "sdintr_insert_gc",
				"sdpart_insert() returns 0x%08x\n", result);
			break;
		}
		partition = sdpart_get_object("gcd-lp-act-mediaid", 18);
		if (partition) {
			memset(&gameCardMediaSector, 0, sizeof(gameCardMediaSector));
			status = sddev_read(device, &gameCardMediaSector, partition->start_lba, 1);
			if (status < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1318, "sdintr_insert_gc", "0x%08x\n", status);
			if (gameCardMediaSector.identity.cid[0] || gameCardMediaSector.identity.cid[1] ||
			    gameCardMediaSector.identity.cid[2] || gameCardMediaSector.identity.cid[3]) {
				memset(&gameCardMediaSector, 0, sizeof(gameCardMediaSector));
				status = sddev_write(device, &gameCardMediaSector, partition->start_lba, 1);
				if (status < 0)
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1325, "sdintr_insert_gc", "0x%08x\n", status);
			}
		}
		status = sdintr_mount_gc(interrupt);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1331, "sdintr_insert_gc",
				"sdintr_mount_gc() returns 0x%08x\n", status);
		break;
	case 2:
		SCE_DBG_ASSERT_FUNC(1, 1586, "sdintr_insert_rm");
		port = interrupt->port;
		device = sddev_get_object(port);
		result = sddev_insert(device);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1595, "sdintr_insert_rm",
				"sddev_insert() returns 0x%08x\n", result);
			break;
		}
		result = sdpart_insert(port);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1600, "sdintr_insert_rm",
				"sdpart_insert() returns 0x%08x\n", result);
			break;
		}
		partition = sdpart_get_object("mcd-lp-act-mediaid", 18);
		if (partition) {
			memset(&memoryCardMediaSector, 0, sizeof(memoryCardMediaSector));
			status = sddev_read(device, &memoryCardMediaSector, partition->start_lba, 1);
			if (status < 0)
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1611, "sdintr_insert_rm", "0x%08x\n", status);
			if (memoryCardMediaSector.identity.cid[0] || memoryCardMediaSector.identity.cid[1] ||
			    memoryCardMediaSector.identity.cid[2] || memoryCardMediaSector.identity.cid[3]) {
				memset(&memoryCardMediaSector, 0, sizeof(memoryCardMediaSector));
				status = sddev_write(device, &memoryCardMediaSector, partition->start_lba, 1);
				if (status < 0)
					SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 1618, "sdintr_insert_rm", "0x%08x\n", status);
			}
		}
		status = sdintr_mount_rm(interrupt);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1624, "sdintr_insert_rm",
				"sdintr_mount_rm() returns 0x%08x\n", status);
		break;
	case 3:
		SCE_DBG_ASSERT_FUNC(1, 1918, "sdintr_insert_usb");
		port = interrupt->port;
		device = sddev_get_object(port);
		result = sddev_insert(device);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1926, "sdintr_insert_usb",
				"sddev_insert() returns 0x%08x\n", result);
			break;
		}
		result = sdpart_insert(port);
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1931, "sdintr_insert_usb",
				"sdpart_insert() returns 0x%08x\n", result);
		break;
	case 4:
insertUsd:
		SCE_DBG_ASSERT_FUNC(1, 1991, "sdintr_insert_usd");
		port = interrupt->port;
		device = sddev_get_object(port);
		result = sddev_insert(device);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1999, "sdintr_insert_usd",
				"sddev_insert() returns 0x%08x\n", result);
			break;
		}
		result = sdpart_insert(port);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 2004, "sdintr_insert_usd",
				"sdpart_insert() returns 0x%08x\n", result);
			break;
		}
		status = sdintr_mount_rm(interrupt);
		if (status < 0)
			SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 2009, "sdintr_insert_usd",
				"sdintr_mount_rm() returns 0x%08x\n", status);
		break;
	default:
		goto acknowledge;
	}
	if (result)
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 1043, "sdintr_insert", "0x%08x\n", result);
	device = sddev_get_object(operationPort);
	if (!sddev_get_info(device, &info))
		interrupt->stat = 1;
	goto acknowledge;
}

int sdctrl_initialize(void)
{
	SceSdstorIntrHeader *header;
	SceSdstorIntrThread *thread;
	SceSdstorIntr *interrupt;
	uint8_t port, nextPort, threadArg;
	unsigned int headerPort;
	int result, requestEvent, syncEvent, irq;

	SCE_DBG_ASSERT_FUNC(1, 479, "sdctrl_init");
	sdctrlState.handlerId = 0;
	sdctrlState.reserved = 0;
	sdctrlState.resumeHandled = 0;
	result = sceKernelRegisterSysEventHandler("SceSdstor",
						 sdctrl_system_event_handler, &sdctrlState);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 389, "sdctrl_initialize", "0x%08x\n", result);
		return result;
	}
	sdctrlState.handlerId = result;
	memset(sdintrObjects, 0, sizeof(sdintrObjects));
	memset(&gameCardMediaSector, 0, sizeof(gameCardMediaSector));
	memset(&memoryCardMediaSector, 0, sizeof(memoryCardMediaSector));
	memset(sdintrHeaders, 0, sizeof(sdintrHeaders));
	for (port = 0, nextPort = 1; ; port++, nextPort++) {
		if (!port)
			continue;
		if (port == 4 && !SceSysrootForDriver_8AA268D6(11))
			break;
		SCE_DBG_ASSERT_FUNC(1, 2234, "sdintr_hdr_get_object");
		header = &sdintrHeaders[port];
		SCE_DBG_ASSERT_FUNC(1, 2281, "sdintr_hdr_init");
		header->insertHandler = sdintr_hdr_insert_handler;
		header->removeHandler = sdintr_hdr_remove_handler;
		header->port = port;
		header->insertSubIntr = 0;
		header->removeSubIntr = 1;
		if (nextPort == 5)
			break;
	}
	memset(sdintrThreads, 0, sizeof(sdintrThreads));
	for (port = 0, nextPort = 1; ; port++, nextPort++) {
		if (!port)
			continue;
		if (port == 4 && !SceSysrootForDriver_8AA268D6(11))
			break;
		SCE_DBG_ASSERT_FUNC(1, 2492, "sdintr_thr_get_object");
		thread = &sdintrThreads[port];
		SCE_DBG_ASSERT_FUNC(1, 2531, "sdintr_thr_init");
		thread->name = "SceSdstorIntr";
		thread->entry = sdintr_thr_entry;
		thread->priority = 28;
		thread->stackSize = 0x2000;
		thread->attr = 0;
		thread->affinity = 0x80000;
		result = sceKernelCreateThread("SceSdstorIntr", sdintr_thr_entry,
						28, 0x2000, 0, 0x80000, NULL);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2551, "sdintr_thr_init", "0x%08x\n", result);
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2520, "sdintr_thr_initialize", "0x%08x\n", result);
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 718, "sdintr_initialize", "0x%08x\n", result);
			goto failed;
		}
		thread->threadId = result;
		if (nextPort == 5)
			break;
	}
	for (port = 0, nextPort = 1; ; port++, nextPort++) {
		if (!port)
			continue;
		if (port == 4 && !SceSysrootForDriver_8AA268D6(11))
			break;
		SCE_DBG_ASSERT_FUNC(1, 763, "sdintr_get_irq");
		headerPort = sdintrHeaderPorts[port];
		SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2234, "sdintr_hdr_get_object");
		header = &sdintrHeaders[headerPort];
		SCE_DBG_ASSERT_FUNC(1, 2492, "sdintr_thr_get_object");
		thread = &sdintrThreads[port];
		SCE_DBG_ASSERT_FUNC(1, 755, "sdintr_get_object");
		interrupt = &sdintrObjects[port];
		threadArg = port;
		SCE_DBG_ASSERT_FUNC(1, 773, "sdintr_init");
		requestEvent = sceKernelCreateEventFlag("SceSdstorRequest", 0, 0, NULL);
		if (requestEvent < 0) {
			result = requestEvent;
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 784, "sdintr_init", "0x%08x\n", result);
			goto interruptFailed;
		}
		syncEvent = sceKernelCreateEventFlag("SceSdstorOpSync", 0, 0, NULL);
		if (syncEvent < 0) {
			result = syncEvent;
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 794, "sdintr_init", "0x%08x\n", result);
			goto interruptFailed;
		}
		interrupt->port = threadArg;
		interrupt->stat = 0;
		interrupt->mountDisabled = 0;
		__builtin_memset(interrupt->cid, 0, sizeof(interrupt->cid));
		interrupt->header = header;
		interrupt->requestEvent = requestEvent;
		interrupt->syncEvent = syncEvent;
		interrupt->thread = thread;
		SCE_DBG_ASSERT_FUNC(header != NULL, 2311, "sdintr_hdr_register_interrupt");
		SCE_DBG_ASSERT_FUNC(header != NULL, 2397, "sdintr_hdr_disable_interrupt");
		SCE_DBG_ASSERT_FUNC(header != NULL, 2419, "sdintr_hdr_disable_remove_interrupt");
		headerPort = header->port;
		SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2242, "sdintr_hdr_get_irq");
		result = sceKernelDisableSubIntr(sdintrIrqNumbers[headerPort], header->removeSubIntr);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2427,
				"sdintr_hdr_disable_remove_interrupt", "0x%08x\n", result);
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2403,
				"sdintr_hdr_disable_interrupt", "0x%08x\n", result);
		} else {
			SCE_DBG_ASSERT_FUNC(header != NULL, 2437, "sdintr_hdr_disable_insert_interrupt");
			headerPort = header->port;
			SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2242, "sdintr_hdr_get_irq");
			result = sceKernelDisableSubIntr(sdintrIrqNumbers[headerPort], header->insertSubIntr);
			if (result < 0) {
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2445,
					"sdintr_hdr_disable_insert_interrupt", "0x%08x\n", result);
				SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2409,
					"sdintr_hdr_disable_interrupt", "0x%08x\n", result);
			}
		}
		headerPort = header->port;
		SCE_DBG_ASSERT_FUNC(headerPort <= 4, 2242, "sdintr_hdr_get_irq");
		irq = sdintrIrqNumbers[headerPort];
		result = sceKernelRegisterSubIntrHandler(irq, header->insertSubIntr,
							"SceSdstorCardInsert", header->insertHandler, interrupt);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2326, "sdintr_hdr_register_interrupt",
				"irq = %d, ret = 0x%08x\n", irq, result);
			goto registerFailed;
		}
		result = sceKernelRegisterSubIntrHandler(irq, header->removeSubIntr,
							"SceSdstorCardRemove", header->removeHandler, interrupt);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2337,
				"sdintr_hdr_register_interrupt", "0x%08x\n", result);
			goto registerFailed;
		}
		SCE_DBG_ASSERT_FUNC(1, 2562, "sdintr_thr_start");
		result = sceKernelStartThread(thread->threadId, 1, &threadArg);
		if (result < 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 2569, "sdintr_thr_start", "0x%08x\n", result);
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 816, "sdintr_init", "0x%08x\n", result);
			goto interruptFailed;
		}
		if (nextPort == 5)
			break;
	}
	return 0;
registerFailed:
	SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 810, "sdintr_init", "0x%08x\n", result);
interruptFailed:
	SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 744, "sdintr_initialize", "0x%08x\n", result);
failed:
	SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 396, "sdctrl_initialize", "0x%08x\n", result);
	return result;
}

int sdctrl_remove(unsigned int port)
{
	SceSdstorIntr *interrupt;
	int result;

	SCE_DBG_ASSERT_FUNC(port <= 4, 491, "sdctrl_remove");
	SCE_DBG_ASSERT_FUNC(port <= 4, 755, "sdintr_get_object");
	interrupt = &sdintrObjects[port];
	if (port == 4 && !SceSysrootForDriver_8AA268D6(11))
		return 0;
	result = sceKernelSetEventFlag(interrupt->requestEvent, 2);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 502, "sdctrl_remove",
			"sceKernelSetEventFlag() returns 0x%08x\n", result);
	return 0;
}

int sdctrl_insert(unsigned int port)
{
	SceSdstorIntr *interrupt;
	int result;

	SCE_DBG_ASSERT_FUNC(port <= 4, 512, "sdctrl_insert");
	SCE_DBG_ASSERT_FUNC(port <= 4, 755, "sdintr_get_object");
	interrupt = &sdintrObjects[port];
	if (port == 4 && !SceSysrootForDriver_8AA268D6(11))
		return 0;
	result = sceKernelSetEventFlag(interrupt->requestEvent, 0x10);
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 15, 523, "sdctrl_insert",
			"sceKernelSetEventFlag() returns 0x%08x\n", result);
	return 0;
}

int sdctrl_disable_mount(unsigned int port)
{
	SCE_DBG_ASSERT_FUNC(port <= 4, 533, "sdctrl_disable_mount");
	if (port != 4 && port - 1 > 1)
		return 0x80010016;
	SCE_DBG_ASSERT_FUNC(port <= 4, 755, "sdintr_get_object");
	SceCpuForDriver_BDF6F8E4((SceInt8 *)&sdintrObjects[port].mountDisabled, 1);
	return 0;
}

int sdctrl_invalidate_partition(unsigned int port)
{
	SceSdstorIntr *interrupt;

	SCE_DBG_ASSERT_FUNC(port <= 4, 553, "sdctrl_invalidate_partition");
	if (port != 4 && port - 1 > 1)
		return 0x80010016;
	SCE_DBG_ASSERT_FUNC(port <= 4, 755, "sdintr_get_object");
	interrupt = &sdintrObjects[port];
	SCE_DBG_ASSERT_FUNC(1, 1183, "sdintr_invalidate_partition");
	port = interrupt->port;
	SCE_DBG_ASSERT_FUNC(port - 1 <= 3, 1190, "sdintr_invalidate_partition");
	if (port == 1 || port == 4) {
		sdintr_rmdev(interrupt);
		sdpart_close(port);
		return 0;
	}
	return 0x80010016;
}
