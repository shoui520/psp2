/* Debug ES3 bus-error console decoding. Private type/table names are unknown. */

#include <stddef.h>
#include <sysmem/cpu.h>
#include <scetypes.h>
#include <sysmem/sysclib.h>
#if FW_DEBUG
#define SCE_BUSERROR_TABLES_ONLY
#include "buserror_es3.h"
#include "buserror.h"
#undef SCE_BUSERROR_TABLES_ONLY
#include <sysmem/debug.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include "buserror_smc.h"

#include "buserror_text.h"

#define ARRAY_SIZE(a)	(sizeof(a) / sizeof((a)[0]))

static int __attribute__((noinline, noclone))
FormatOcpTargetInfo_ES3(char *buf, SceSize size, const char *name,
		      SceUInt32 address, SceUInt32 attribute)
{
	unsigned int master = (attribute >> 16) & 63;
	unsigned int command = (attribute >> 8) & 7;
	unsigned int burstError = attribute & 8;
	unsigned int permissionError = attribute & 4;
	unsigned int addressError = attribute & 1;

	if (buf != NULL)
		return snprintf(buf, size,
			"   target : %s\n  address : 0x%08x\nattribute : 0x%08x\n"
			"   master : %s\n      cmd : %s\n%s%s%s",
			name, address, attribute, ocpMasters[master], ocpCommands[command],
			burstError ? "   reason : Burst Access Error\n" : "",
			permissionError ? "   reason : Register Permission Error\n" : "",
			addressError ? "   reason : Address Hole Error\n" : "");
	SCE_DBG_PRINTF(0, 1054,
		"   target : %s\n  address : 0x%08x\nattribute : 0x%08x\n"
		"   master : %s\n      cmd : %s\n%s%s%s",
		name, address, attribute, ocpMasters[master], ocpCommands[command],
		burstError ? "   reason : Burst Access Error\n" : "",
		permissionError ? "   reason : Register Permission Error\n" : "",
		addressError ? "   reason : Address Hole Error\n" : "");
	return 0;
}

/* An OCP result may advance the initially null output pointer. */
#define FORMAT_OUT(line, ...) do { \
	if (buf + len != NULL) { \
		SceUInt32 written = snprintf(buf + len, size - len, __VA_ARGS__); \
		if (written >= size - len) \
			written = size - len; \
		len += written; \
	} else { \
		SCE_DBG_PRINTF(0, line, __VA_ARGS__); \
	} \
} while (0)

static inline __attribute__((always_inline)) void
FormatBusErrorInfo_ES3(char *buf, SceSize size, const BusErrorRecord *rec)
{
	const BusErrorOcpInfo *ocp = (const void *)((const char *)rec + rec->hdrSize);
	const BusErrorSiInfo *si = (const void *)((const char *)ocp + rec->ocpSize);
	const BusErrorGrabInfo *grab = (const void *)((const char *)si + rec->siSize);
	const BusErrorRegbusInfo *reg = (const void *)((const char *)grab + rec->grabSize);
	const BusErrorVipInfo *vip = rec->hdrSize > 27 ?
		(const void *)((const char *)reg + rec->regbusSize) : NULL;
	SceUInt32 status0 = ocp->status[0][0] | ocp->status[1][0];
	SceUInt32 status1 = ocp->status[0][1] | ocp->status[1][1];
	SceUInt32 busStatus = ocp->status[0][2] | ocp->status[1][2];
	const BusErrorSiBus *bus;
	const BusErrorSiAgent *agent;
	const BusErrorPort *port;
	const BusErrorSystemBus *sys;
	SceUInt32 i, j, written;
	int len = 0;

	FORMAT_OUT(1102,
		"[ Bus error occured on target device ]\nBET0:0x%08x, BET1:0x%08x, BEBT:0x%08x\n\n",
		status0, status1, busStatus);
	for (i = 0; i < ocp->count; i++) {
		const BusErrorOcpTarget *target = i < 15 ?
			&ocpTargets0[i] : &ocpTargets1[i - 15];
		SceUInt32 combined = i < 15 ? status0 : status1;
		SceUInt32 first = i < 15 ? ocp->status[0][0] : ocp->status[0][1];
		const BusErrorOcpError *error = &ocp->target[i].error[0];

		if (!(combined & target->mask))
			continue;
		if (!(first & target->mask))
			error++;
		written = FormatOcpTargetInfo_ES3(buf + len, size - len, target->name,
						error->address, error->attribute);
		len += written;
	}
	bus = (const BusErrorSiBus *)(si + 1);
	for (i = 0; i < si->count; i++) {
		for (sys = systemBuses; sys != systemBuses + ARRAY_SIZE(systemBuses); sys++)
			if (sys->address == bus->address)
				break;
		if (sys == systemBuses + ARRAY_SIZE(systemBuses))
			break;
		if (busStatus & sys->mask)
			FORMAT_OUT(1201,
				"%s\nSI_FLAG_STATUS0:    0x%016llx\n"
				"SI_FLAG_STATUS1:    0x%016llx\n"
				"SI_FLAG_STATUS2:    0x%016llx\n"
				"SI_FLAG_STATUS3:    0x%016llx\n"
				"SI_FLAG_STATUS4:    0x%016llx\n",
				sys->name, bus->flagStatus[0], bus->flagStatus[1],
				bus->flagStatus[2], bus->flagStatus[3], bus->flagStatus[4]);
		agent = (const BusErrorSiAgent *)(bus + 1);
		for (j = 0; j < bus->initiatorCount; j++) {
			for (port = sys->initiators; port->name; port++)
				if (port->address == agent->address)
					break;
			if (port->name && agent->status)
				FORMAT_OUT(1234,
					"%40s: STATUS=0x%08X, ERROR_LOG=0x%012llX, ADDR=0x%08llX\n",
					port->name, (SceUInt32)agent->status,
					agent->errorLog, agent->errorAddress);
			agent = (const void *)((const char *)agent + agent->size);
		}
		for (j = 0; j < bus->targetCount; j++) {
			SceUInt32 status;

			for (port = sys->targets; port->name; port++)
				if (port->address == agent->address)
					break;
			status = (SceUInt32)agent->status & 0xffff0fff;
			if (port->name && status)
				FORMAT_OUT(1272,
					"%40s: STATUS=0x%08X, ERROR_LOG=0x%012llX, ADDR=0x%08llX\n",
					port->name, status, agent->errorLog, agent->errorAddress);
			agent = (const void *)((const char *)agent + agent->size);
		}
		bus = (const BusErrorSiBus *)agent;
	}
	if (grab->status & 4)
		FORMAT_OUT(1304,
			"GRAB_WERRID:   0x%08X\nGRAB_WERRADDR: 0x%08X\n"
			"GRAB_WERRATTR: 0x%08X\nGRAB_WERRINFO: 0x%08X\n",
			grab->write[0], grab->write[1], grab->write[2], grab->write[3]);
	if (grab->status & 2)
		FORMAT_OUT(1327,
			"GRAB_RERRID:   0x%08X\nGRAB_RERRADDR: 0x%08X\n"
			"GRAB_RERRATTR: 0x%08X\nGRAB_RERRINFO: 0x%08X\n",
			grab->read[0], grab->read[1], grab->read[2], grab->read[3]);
	if (grab->secureStatus & 4)
		FORMAT_OUT(1351,
			"GRAB_SECWERRID:   0x%08X\nGRAB_SECWERRADDR: 0x%08X\n"
			"GRAB_SECWERRATTR: 0x%08X\nGRAB_SECWERRINFO: 0x%08X\n",
			grab->secureWrite[0], grab->secureWrite[1],
			grab->secureWrite[2], grab->secureWrite[3]);
	if (grab->secureStatus & 2)
		FORMAT_OUT(1374,
			"GRAB_SECRERRID:   0x%08X\nGRAB_SECRERRADDR: 0x%08X\n"
			"GRAB_SECRERRATTR: 0x%08X\nGRAB_SECRERRINFO: 0x%08X\n",
			grab->secureRead[0], grab->secureRead[1],
			grab->secureRead[2], grab->secureRead[3]);
	if (reg->status & 1)
		FORMAT_OUT(1400,
			"REGBUS_ERRID:   0x%08X\nREGBUS_ERRADDR: 0x%08X\n"
			"REGBUS_ERRATTR: 0x%08X\nREGBUS_ERRINFO: 0x%08X\n",
			reg->error[0], reg->error[1], reg->error[2], reg->error[3]);
	if (reg->secureStatus & 1)
		FORMAT_OUT(1423,
			"REGBUS_SECERRID:   0x%08X\nREGBUS_SECERRADDR: 0x%08X\n"
			"REGBUS_SECERRATTR: 0x%08X\nREGBUS_SECERRINFO: 0x%08X\n",
			reg->secureError[0], reg->secureError[1],
			reg->secureError[2], reg->secureError[3]);
	if (!vip)
		return;
	if (vip->bap[0])
		FORMAT_OUT(1446,
			"VIP_PROT_BAP_ERRV:  0x%08X\nVIP_PROT_BAP_ERRC:  0x%08X\nVIP_PROT_BAP_ERRA:  0x%08X\n",
			vip->bap[0], vip->bap[1], vip->bap[2]);
	if (vip->vdpd[0])
		FORMAT_OUT(1466,
			"VIP_PROT_VDPD_ERRV: 0x%08X\nVIP_PROT_VDPD_ERRC: 0x%08X\nVIP_PROT_VDPD_ERRA: 0x%08X\n",
			vip->vdpd[0], vip->vdpd[1], vip->vdpd[2]);
	if (vip->vdpm[0])
		FORMAT_OUT(1487,
			"VIP_PROT_VDPM_ERRV: 0x%08X\nVIP_PROT_VDPM_ERRC: 0x%08X\nVIP_PROT_VDPM_ERRA: 0x%08X\n",
			vip->vdpm[0], vip->vdpm[1], vip->vdpm[2]);
	if (vip->venezia[0])
		FORMAT_OUT(1507,
			"VENE_PROT_REG_ERRV: 0x%08X\nVENE_PROT_REG_ERRC: 0x%08X\nVENE_PROT_REG_ERRA: 0x%08X\n",
			vip->venezia[0], vip->venezia[1], vip->venezia[2]);
}

int busErrorIntrHandler(int code, void *arg)
{
	BusErrorRecord *rec;
	const BusErrorOcpInfo *fixedOcp;
	const BusErrorSiInfo *si;
	const BusErrorSiBus *bus;
	SceUInt32 ttbr0, vbar;
	void *sysroot;
	int dump;

	dump = SceDebugForKernel_A465A31A(0);
	busErrorSmc10D();
	if (dump)
		SceDebugForDriver_62466B0A(sysrootSharedMemory()->tty, 0x1000);
	rec = &sysrootSharedMemory()->busError;
	FormatBusErrorInfo_ES3(NULL, 0, rec);
	fixedOcp = (const BusErrorOcpInfo *)(rec + 1);
	if (rec->siSize) {
		si = (const void *)((const char *)rec + rec->hdrSize + rec->ocpSize);
		bus = (const BusErrorSiBus *)(si + 1);
		if (bus->address == BUSERROR_MAIN_XBAR && (bus->flagStatus[0] & 3))
			for (;;)
				;
	}
	if (!dump)
		for (;;)
			;
	sysroot = SceSysrootForKernel_3E455842();
	__asm__ volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(ttbr0));
	__asm__ volatile("mrc p15, 0, %0, c12, c0, 0" : "=r"(vbar));
	busErrorSmc122(2, ttbr0, vbar, sysroot);
	for (;;)
		;
	return 0;
}

#else
#include <buserror/buserror_types.h>
#include <sysmem/debug.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysroot.h>
#include "buserror_smc.h"

int busErrorIntrHandler(int code, void *arg)
{
	BusErrorRecord *rec;
	const BusErrorOcpInfo *fixedOcp;
	const BusErrorSiInfo *si;
	const BusErrorSiBus *bus;
	void *sysroot;
	SceUInt32 ttbr0, vbar;
	int dump;

	dump = SceDebugForKernel_A465A31A(0);
	busErrorSmc10D();
	if (dump)
		SceDebugForDriver_62466B0A(sysrootSharedMemory()->tty, 0x1000);
	rec = &sysrootSharedMemory()->busError;
	/* This callback consumes the fixed OCP status words, not hdrSize. */
	fixedOcp = (const BusErrorOcpInfo *)(rec + 1);
	if (rec->siSize != 0) {
		si = (const BusErrorSiInfo *)((char *)rec + rec->hdrSize + rec->ocpSize);
		bus = (const BusErrorSiBus *)(si + 1);
		if (bus->address == BUSERROR_MAIN_XBAR && (bus->flagStatus[0] & 3) != 0)
			goto hang;
	}
	if (!dump)
		goto hang;
	sysroot = SceSysrootForKernel_3E455842();
	__asm__ volatile("mrc p15, 0, %0, c2, c0, 0" : "=r"(ttbr0));
	__asm__ volatile("mrc p15, 0, %0, c12, c0, 0" : "=r"(vbar));
	busErrorSmc122(2, ttbr0, vbar, sysroot);
hang:
	for (;;)
		;
	return 0;	/* (not reached) */
}
#endif

int RegisterBusErrorHandler_ES3(void)
{
	int ret;

	ret = sceKernelRegisterIntrHandler(0x20, "SceBusError", 0, busErrorIntrHandler, NULL,
					    0x10, 0xf, NULL);
	if (ret < 0) {
#if FW_DEBUG
		SCE_DBG_PRINTF(6, 1739, "register default bus error handler failed. 0x%08x\n", ret);
		SCE_DBG_ASSERT_FAIL(1740);
#endif
	} else {
		sceKernelEnableIntr(0x20);
		ret = 0;
	}
	return ret;
}
