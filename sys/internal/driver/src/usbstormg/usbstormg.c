/*
 * SceUsbstorMg: MagicGate security protocol over USB mass-storage commands.
 * The four supported firmwares carry identical code. Internal names are
 * unknown; the names below describe their roles, imports retain their NIDs.
 */
#include <stdint.h>
#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/threadmgr.h>
#include <kernel/error.h>
#include <sysmem/sysmem.h>
#include <usbstor/usbstor.h>
#include <usbstormg/usbstormg.h>

#define MG_ERROR_MEMORY	((int)0x8024410a)
#define MG_ERROR_INIT	((int)0x80244101)
#define MG_DISABLED	8
#define MG_AUTHENTICATED	2
#define USB_TIMEOUT	1000000

static struct {
	uint64_t info;
	uint8_t challenge[8];
	MagicGateFileInfo deviceInfo;
	uint64_t response;
	uint8_t hostResponse[8];
	uint8_t tag[8];
	int session;
	int device;
	unsigned flags;
	unsigned phase;
	unsigned reserved0;
	unsigned stopped;
	int memblock;
	unsigned reserved1;
	uint8_t *buffer;
	unsigned reserved2;
} s_mg;

_Static_assert(sizeof(s_mg) == 96, "MagicGate USB command context");

static int ProcessCommand(int unit, const uint8_t *cdb, struct UsbstorSense *sense);
static int StopTransfer(int event, int argument);
static int Initialize(void);
static int Finalize(void);

static struct UsbstorFunction s_handler = {
	.unit = USBSTOR_UNIT_ANY,
	.requestSense = 0,
	.busy = 0,
	.priority = 1,
	.command = ProcessCommand,
	.event = StopTransfer,
	.start = Initialize,
	.stop = Finalize,
};

/* name unknown: enter the privileged caller context used by MagicGate. */
static unsigned EnterContext(void)
{
	unsigned value;

	__asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(value));
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 3" :: "r"(value << 16));
	return value;
}

static void LeaveContext(unsigned value)
{
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 3" :: "r"(value));
}

/* name unknown: request cancellation when the protocol is available. */
static int StopTransfer(int event, int argument)
{
	if (s_mg.flags & MG_DISABLED)
		return -3;
	s_mg.stopped = 1;
	return 0;
}

/* name unknown: release the session and backing storage. */
static int Finalize(void)
{
	unsigned context = EnterContext();
	int permission, ret = 0;

	if (!SceSblACMgrForDriver_A27E47A7(0))
		goto out;
	permission = ksceKernelSetPermission(0x40);
	if (permission < 0) {
		ret = permission == SCE_ERROR_ERRNO_ENOSYS ? 0 : permission;
		goto out;
	}
	if (s_mg.session >= 0) {
		SceMagicGateForDriver_144BFA76(s_mg.session);
		s_mg.session = -1;
	}
	if (!(s_mg.flags & MG_DISABLED)) {
		SceMagicGateForDriver_8E2E577E();
		SceMagicGateForDriver_6D3B42E6();
	}
	ksceKernelFreeMemBlock(s_mg.memblock);
	ksceKernelSetPermission(permission);
out:
	LeaveContext(context);
	return ret;
}

/* name unknown: obtain a page-aligned temporary MagicGate workspace. */
static int AllocTemporary(void **base, unsigned *size)
{
	SceKernelAllocMemBlockKernelOpt opt;
	void *ptr;
	unsigned bytes;
	int uid, ret;

	ret = SceMagicGateForDriver_758D7158(NULL, 0);
	if (ret < 0)
		return ret;
	bytes = ((unsigned)ret + 0xfff) & ~0xfffU;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 0x200000;
	uid = ksceKernelAllocMemBlock("SceUsbstorMgTmp", 0x1020d006, bytes, &opt);
	if (uid < 0)
		return MG_ERROR_MEMORY;
	if (ksceKernelGetMemBlockBase(uid, &ptr) < 0) {
		ksceKernelFreeMemBlock(uid);
		return MG_ERROR_MEMORY;
	}
	ret = SceMagicGateForDriver_758D7158(ptr, bytes);
	if (ret < 0) {
		ksceKernelFreeMemBlock(uid);
		return ret;
	}
	*base = ptr;
	*size = bytes;
	return uid;
}

static unsigned ReadBe16(const uint8_t *p)
{
	return ((unsigned)p[0] << 8) | p[1];
}

static unsigned ReadBe32(const uint8_t *p)
{
	return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) |
	       ((unsigned)p[2] << 8) | p[3];
}

static void CopyBytes(void *dst, const void *src, unsigned size)
{
	uint8_t *d = dst;
	const uint8_t *s = src;

	while (size--)
		*d++ = *s++;
}

static int SameTag(const uint8_t *a, const uint8_t *b)
{
	unsigned diff = 0;
	int i;

	for (i = 0; i < 8; i++)
		diff |= a[i] ^ b[i];
	return diff == 0;
}

static int Sense(struct UsbstorSense *sense, unsigned key, unsigned asc, unsigned ascq)
{
	sense->key = key;
	sense->asc = asc;
	sense->ascq = ascq;
	return USBSTOR_COMMAND_FAILED;
}

/* name unknown: first host challenge (SEND KEY, format 0x30). */
static int ReceiveChallenge(unsigned size, struct UsbstorSense *sense)
{
	uint8_t challenge[8], hostResponse[8], zero[16];
	void *temp;
	unsigned tempSize;
	int uid = 0, ret = -1;
	uint8_t *buf = s_mg.buffer;

	if (size != 0x14) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
		goto out;
	}
	if (SceUsbstorForDriver_F1A690DC(buf, 0x14, USB_TIMEOUT) < 0)
		goto out;
	if (ReadBe16(buf) + 2 < 0x14) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
		goto out;
	}
	CopyBytes(challenge, buf + 4, 8);
	CopyBytes(hostResponse, buf + 12, 8);
	for (tempSize = 0; tempSize < 8; tempSize++)
		zero[tempSize] = 0;
	CopyBytes(zero + 8, challenge, 8);
	uid = AllocTemporary(&temp, &tempSize);
	if (uid < 0) {
		uid = 0;
		Sense(sense, SCSI_KEY_NOT_READY, 0xff, 0xff);
		goto out;
	}
	if (SceMagicGateForDriver_CA74A13D(buf, 0x410, temp, tempSize, zero) < 0) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_KEY_EXCHANGE_FAILURE, 0);
		goto out;
	}
	CopyBytes(s_mg.challenge, challenge, 8);
	CopyBytes(s_mg.hostResponse, hostResponse, 8);
	s_mg.phase = 1;
	ret = 0;
out:
	if (ret < 0)
		s_mg.phase = 0;
	s_mg.flags &= ~MG_AUTHENTICATED;
	if (uid)
		ksceKernelFreeMemBlock(uid);
	return ret;
}

/* name unknown: report the device challenge and authentication material. */
static int ReportChallenge(unsigned size, struct UsbstorSense *sense)
{
	uint8_t info[16], challenge[16];
	void *temp;
	unsigned tempSize, bytes;
	int uid = 0, ret = -1;
	uint8_t *buf = s_mg.buffer;

	if (size != 0x43c) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
		goto out;
	}
	if (s_mg.phase != 1) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_KEY_EXCHANGE_FAILURE, 0);
		goto out;
	}
	if (SceMagicGateForDriver_0201E54B(info) < 0) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
		goto out;
	}
	info[14] = 0;
	info[15] = 0;
	uid = AllocTemporary(&temp, &tempSize);
	if (uid < 0) {
		uid = 0;
		Sense(sense, SCSI_KEY_NOT_READY, 0xff, 0xff);
		goto out;
	}
	if (SceMagicGateForDriver_D819D5C6(s_mg.device, &s_mg.deviceInfo) < 0) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
		goto out;
	}
	if (SceMagicGateForDriver_CA74A13D(buf, 0x410, temp, tempSize, info) < 0) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_KEY_EXCHANGE_FAILURE, 0);
		goto out;
	}
	if (SceMagicGateForDriver_E0888D78(&s_mg.response, &s_mg.info) < 0) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
		goto out;
	}
	if (SceMagicGateForDriver_42A9D74E(-1, s_mg.tag, &s_mg.deviceInfo,
					s_mg.hostResponse, &s_mg.response, buf) < 0) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_KEY_EXCHANGE_FAILURE, 0);
		goto out;
	}
	for (bytes = 0; bytes < 8; bytes++)
		challenge[bytes] = 0;
	CopyBytes(challenge + 8, s_mg.challenge, 8);
	if (SceMagicGateForDriver_CA74A13D(buf, 0x410, temp, tempSize, challenge) < 0) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_KEY_EXCHANGE_FAILURE, 0);
		goto out;
	}
	bytes = (ReadBe32(buf) + 1) * 16;
	memmove(buf + 0x2c, buf, bytes);
	memset(buf + bytes + 0x2c, 0, 0x410 - bytes);
	buf[0] = 4;
	buf[1] = 0x3a;
	buf[2] = 0;
	buf[3] = 0;
	CopyBytes(buf + 4, &s_mg.deviceInfo, 16);
	CopyBytes(buf + 20, s_mg.tag, 8);
	CopyBytes(buf + 28, info + 8, 8);
	CopyBytes(buf + 36, &s_mg.response, 8);
	if (SceUsbstorForDriver_D3C7EE70(buf, 0x43c, USB_TIMEOUT) < 0)
		goto out;
	s_mg.phase = 2;
	ret = 0;
out:
	if (ret < 0)
		s_mg.phase = 0;
	s_mg.flags &= ~MG_AUTHENTICATED;
	if (uid)
		ksceKernelFreeMemBlock(uid);
	return ret;
}

/* name unknown: check the host response and enable the session. */
static int Authenticate(unsigned size, struct UsbstorSense *sense)
{
	uint8_t received[8], tag[8];
	uint8_t *buf = s_mg.buffer;
	int ret = -1;

	if (size != 0x41c) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
		goto out;
	}
	if (s_mg.phase != 2) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_KEY_EXCHANGE_FAILURE, 0);
		goto out;
	}
	if (SceUsbstorForDriver_F1A690DC(buf, 0x41c, USB_TIMEOUT) < 0)
		goto out;
	if (ReadBe16(buf) + 2 < 0x41c) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
		goto out;
	}
	CopyBytes(received, buf + 4, 8);
	memmove(buf, buf + 12, 0x410);
	if (SceMagicGateForDriver_42A9D74E(-1, tag, &s_mg.deviceInfo,
					&s_mg.response, s_mg.hostResponse, buf) < 0 ||
	    !SameTag(received, tag)) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_KEY_EXCHANGE_FAILURE, 0);
		goto out;
	}
	if (SceMagicGateForDriver_42A9D74E(s_mg.session, NULL, &s_mg.deviceInfo,
					s_mg.tag, received, buf) < 0) {
		Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
		goto out;
	}
	s_mg.phase = 3;
	s_mg.flags |= MG_AUTHENTICATED;
	return 0;
out:
	s_mg.phase = 0;
	s_mg.flags &= ~MG_AUTHENTICATED;
	return ret;
}

/* name unknown: read one MagicGate record (REPORT KEY, format 0x33). */
static int ReadRecord(unsigned size, unsigned index, struct UsbstorSense *sense)
{
	uint8_t tag[8], iv[8] = { 0 };
	uint8_t *buf = s_mg.buffer;
	unsigned flags, bytes;
	int i;

	if (size != 0x404 || index > 31)
		return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
	if (SceMagicGateForDriver_1BCE34AD(s_mg.device, index, buf, 0x400) < 0)
		return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
	flags = buf[1];
	buf[1] = flags | 0x10;
	bytes = ReadBe16(buf + 2);
	if (buf[0] != index || (bytes & 7) || bytes > 0x3f8 ||
	    ((flags & 0x20) && bytes > 0x3f0))
		return Sense(sense, SCSI_KEY_NOT_READY, 0xff, 0xff);
	if (s_mg.phase == 3) {
		if (flags & 0x20) {
			if (SceMagicGateForDriver_593016E1(tag, buf, 0, bytes + 8,
							 s_mg.session) < 0)
				return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
		} else {
			for (i = 0; i < 8; i++)
				tag[i] = 0;
		}
		if ((flags & 0x80) &&
		    SceMagicGateForDriver_B0827B4F(buf + 8, buf + 8,
				(flags & 0x40) ? iv : NULL, bytes, s_mg.session) < 0)
			return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
	} else {
		memset(buf + 8, 0, bytes);
		for (i = 0; i < 8; i++)
			tag[i] = 0;
	}
	if (flags & 0x20)
		CopyBytes(buf + bytes + 8, tag, 8);
	memmove(buf + 4, buf, 0x400);
	buf[0] = 4;
	buf[1] = 2;
	buf[2] = 0;
	buf[3] = 0;
	return SceUsbstorForDriver_D3C7EE70(buf, 0x404, USB_TIMEOUT) < 0 ?
	       USBSTOR_COMMAND_FAILED : USBSTOR_COMMAND_DONE;
}

/* name unknown: validate and write one MagicGate record (format 0x34). */
static int WriteRecord(unsigned size, unsigned index, struct UsbstorSense *sense)
{
	uint8_t tag[8], iv[8] = { 0 };
	uint8_t *buf = s_mg.buffer;
	unsigned flags, bytes;

	if (size != 0x404 || index > 31)
		return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
	if (SceUsbstorForDriver_F1A690DC(buf, 0x404, USB_TIMEOUT) < 0)
		return USBSTOR_COMMAND_FAILED;
	if (ReadBe16(buf) + 2 < 0x404)
		return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
	buf = memmove(buf, buf + 4, 0x400);
	flags = buf[1];
	bytes = ReadBe16(buf + 2);
	if (buf[0] != index || flags >> 6 == 1 || (bytes & 7) || bytes > 0x3f8 ||
	    ((flags & 0x20) && bytes > 0x3f0) || ((flags & 0xa0) && bytes < 8))
		return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
	if (s_mg.phase != 3 && (flags & 0xa0))
		return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_KEY_EXCHANGE_FAILURE, 2);
	if (flags & 0x10)
		return Sense(sense, SCSI_KEY_MEDIUM_ERROR, SCSI_ASC_WRITE_ERROR, 0);
	if ((flags & 0x80) &&
	    SceMagicGateForDriver_4F4B2DC9(buf + 8, buf + 8,
			(flags & 0x40) ? iv : NULL, bytes, s_mg.session) < 0)
		return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
	if (flags & 0x20) {
		if (SceMagicGateForDriver_593016E1(tag, buf, 0, bytes + 8,
						 s_mg.session) < 0)
			return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
		if (!SameTag(buf + bytes + 8, tag))
			return Sense(sense, SCSI_KEY_MEDIUM_ERROR, SCSI_ASC_WRITE_ERROR, 0);
	}
	if (SceMagicGateForDriver_75B445DA(s_mg.device, index, buf, 0x400) < 0)
		return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
	return 0;
}

/* name unknown: report the 16-byte device information (format 0x3f). */
static int ReportInfo(unsigned size, struct UsbstorSense *sense)
{
	uint8_t *buf = s_mg.buffer;

	if (size != 0x12)
		return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
	buf[0] = 0;
	buf[1] = 0x10;
	if (SceMagicGateForDriver_D819D5C6(s_mg.device, &s_mg.deviceInfo) < 0)
		return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
	CopyBytes(buf + 2, &s_mg.deviceInfo, 16);
	SceUsbstorForDriver_D3C7EE70(buf, 0x12, USB_TIMEOUT);
	return 0;
}

/* name unknown: registered USB command callback. */
static int ProcessCommand(int unit, const uint8_t *cdb, struct UsbstorSense *sense)
{
	unsigned size, format;
	int ret;

	if (s_mg.flags & MG_DISABLED)
		return USBSTOR_COMMAND_UNKNOWN;
	if (unit)
		return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_LUN_NOT_SUPPORTED, 0);
	if (cdb[0] != SCSI_SEND_KEY && cdb[0] != SCSI_REPORT_KEY)
		return USBSTOR_COMMAND_UNKNOWN;
	if (cdb[7] != 0xbc)
		return Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
	s_handler.busy = 1;
	size = ReadBe16(cdb + 8);
	format = cdb[10] & 0x3f;
	if (cdb[0] == SCSI_SEND_KEY) {
		switch (format) {
		case 0x30:
			ret = ReceiveChallenge(size, sense);
			break;
		case 0x32:
			ret = Authenticate(size, sense);
			break;
		case 0x34:
			ret = WriteRecord(size, ReadBe32(cdb + 2), sense);
			break;
		default:
			ret = Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
			break;
		}
	} else {
		switch (format) {
		case 0x31:
			ret = ReportChallenge(size, sense);
			break;
		case 0x33:
			ret = ReadRecord(size, ReadBe32(cdb + 2), sense);
			break;
		case 0x3f:
			ret = ReportInfo(size, sense);
			break;
		default:
			ret = Sense(sense, SCSI_KEY_ILLEGAL_REQUEST, SCSI_ASC_INVALID_FIELD_IN_CDB, 0);
			break;
		}
	}
	s_handler.busy = 0;
	return ret;
}

/* name unknown: registered USB initialization callback. */
static int Initialize(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	unsigned context = EnterContext();
	void *base;
	int permission, ret = 0;

	if (!SceSblACMgrForDriver_A27E47A7(0))
		goto disabled;
	permission = ksceKernelSetPermission(0x40);
	if (permission < 0) {
		if (permission == SCE_ERROR_ERRNO_ENOSYS)
			goto disabled;
		ret = permission;
		goto out;
	}
	s_handler.busy = 0;
	s_mg.stopped = 0;
	s_mg.flags = MG_DISABLED;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 0x200000;
	s_mg.memblock = ksceKernelAllocMemBlock("SceUsbstorMg", 0x10208006, 0x2000, &opt);
	if (s_mg.memblock < 0) {
		ret = MG_ERROR_MEMORY;
		goto permissionOut;
	}
	if (ksceKernelGetMemBlockBase(s_mg.memblock, &base) < 0) {
		ret = MG_ERROR_MEMORY;
		goto freeBlock;
	}
	memset(base, 0, 0x2000);
	s_mg.buffer = (uint8_t *)(((uintptr_t)base + 0x3f) & ~0x3fU);
	if (SceMagicGateForDriver_46A28FAA() < 0) {
		ret = MG_ERROR_INIT;
		s_mg.flags |= MG_DISABLED;
		goto freeBlock;
	}
	if (SceMagicGateForDriver_12CE769D(&s_mg.info) < 0)
		goto closeMagicGate;
	s_mg.session = -1;
	s_mg.device = -1;
	if (SceMagicGateForDriver_30DF2E82() < 0)
		goto closeMagicGate;
	ret = SceMagicGateForDriver_7B052A10();
	if (ret >= 0) {
		s_mg.device = ret;
		ret = SceMagicGateForDriver_FEF11D84(3);
		if (ret >= 0) {
			s_mg.session = ret;
			s_mg.flags = 0;
			ret = 0;
			goto permissionOut;
		}
	}
	ret = MG_ERROR_INIT;
	s_mg.flags |= MG_DISABLED;
	SceMagicGateForDriver_8E2E577E();
	SceMagicGateForDriver_6D3B42E6();
	goto freeBlock;
closeMagicGate:
	ret = MG_ERROR_INIT;
	s_mg.flags |= MG_DISABLED;
	SceMagicGateForDriver_6D3B42E6();
freeBlock:
	if (s_mg.memblock >= 0) {
		ksceKernelFreeMemBlock(s_mg.memblock);
		s_mg.memblock = -1;
	}
permissionOut:
	ksceKernelSetPermission(permission);
	goto out;
disabled:
	s_mg.flags |= MG_DISABLED;
out:
	LeaveContext(context);
	return ret;
}

int module_start(unsigned args, void *argp)
{
	memset(&s_mg, 0, sizeof(s_mg));
	return SceUsbstorForDriver_C0944A04(&s_handler) < 0 ? 2 : 0;
}

int module_stop(unsigned args, void *argp)
{
	return (unsigned)SceUsbstorForDriver_EAC08C24(&s_handler) >> 31;
}
