/*
 * SceCrashDump: after a crash reboot (syscon state 0x17), turn the crash
 * record the kernel left in a reserved physical window into an ARM ELF core
 * file. Tool units with the debugger attached (dipsw 0xc1 / 0xc2) also get the
 * kernel log and a core file on the host (host0:); every unit with a record
 * hands one to SceCoredump. The module never stays resident.
 *
 * Core file layout: ELF header, program headers (one PT_NOTE, then one PT_LOAD
 * per saved memory range and one for the page tables), the notes ("PSP2"
 * owner), then each range, page aligned.
 */

#include <elf.h>
#include <iofilemgr/iofilemgr.h>
#include <sysmem/sblaimgr.h>
#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include <modulemgr/modulemgr.h>
#include <rtc/rtc.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <lowio/pervasive.h>
#include <coredump/coredump.h>
#include <sysmem/debug.h>
#include <display/display.h>
#include <sysmem/qafmgr.h>
#include <syscon/syscon.h>

#define CRASHDUMP_BUF_SIZE	0x100000	/* notes, then one copy window */
#define CRASHDUMP_WINDOW	0x100000
#define CRASHDUMP_RECORD_SIZE	0x20000
#define CRASHDUMP_RECORD_VER	200
#define CRASHDUMP_MAX_RANGES	8
#define CRASHDUMP_SYSCON_CRASH	0x17

/* the crash record (CRASHDUMP_RECORD_SIZE, in the reserved window) */
typedef struct CrashdumpRange {
	SceUInt32 addr;				/* p_paddr of the segment */
	SceSize size;
	SceUInt32 offset;			/* in the physical window */
} CrashdumpRange;

typedef struct CrashdumpRecord {
	SceUInt8 magic[8];
	SceUInt32 version;			/* CRASHDUMP_RECORD_VER */
	SceUInt32 thread;			/* index of the crashed thread's context */
	SceUInt32 kind;				/* 0 / 1: layout of the thread context */
	SceUInt32 info[15];			/* +0x14: copied into the 0x1000 note */
	SceUInt32 nranges;			/* +0x50 */
	CrashdumpRange range[CRASHDUMP_MAX_RANGES];
	SceUInt32 nwords;			/* +0xb4: words of the 0x1001 note */
	SceSize ttbSize;			/* +0xb8: page tables */
	SceUInt32 ttbOffset;
	SceSize logSize;			/* +0xc0: kernel log, including the terminator */
	SceUInt32 logOffset;			/* from the record */
} CrashdumpRecord;

#define RECORD_NOTE_1100	0x8000		/* 0x1000 bytes */
#define RECORD_THREADS		0x9000		/* 0x1000 per thread */
#define RECORD_EXTRA		0xd000		/* { ?, size }, data from the start */
#define RECORD_WORDS		0x10000

/* the 0x1000 note */
typedef struct CrashdumpNoteInfo {
	SceSize size;				/* sizeof(CrashdumpNoteInfo) */
	SceUInt32 fwVersion;
	SceUInt32 reserved;			/* written 0 */
	SceUInt32 thread;
	SceUInt32 kind;
	SceUInt32 info[15];
} CrashdumpNoteInfo;

/* where the core file goes */
typedef struct CrashdumpOps {
	int (*open)(const char *name, SceSize size);
	int (*write)(const void *buf, SceSize size);
	int (*close)(void);
	int (*remove)(const char *name);
} CrashdumpOps;

static const SceUInt8 s_magic[8] = { 0x74, 0x31, 0x4a, 0x8b, 0xfe, 0x49, 0x00, 0x41 };
static const char s_noteName[] = "PSP2";

static SceUID s_dump;
static char *s_buf;
static SceUID s_fd;
static SceSize s_noteOff;

/* -- backends (names unknown) ------------------------------------------- */

static int coredumpRemove(const char *name)
{
	return 0;
}

static int coredumpClose(void)
{
	int ret = SceCoredumpForDriver_10863B61(s_dump);

	if (ret < 0)
		return ret;
	s_dump = -1;
	return 0;
}

static int coredumpWrite(const void *buf, SceSize size)
{
	SceCoredumpForDriver_12392973(s_dump, buf, size);
	return size;
}

static int coredumpOpen(const char *name, SceSize size)
{
	SceUID ret = SceCoredumpForDriver_13EF8516(size);

	if (ret < 0)
		return ret;
	s_dump = ret;
	return 0;
}

static int fileRemove(const char *name)
{
	return sceIoRemove(name);
}

static int fileClose(void)
{
	int ret = sceIoClose(s_fd);

	if (ret < 0)
		return ret;
	s_fd = -1;
	return 0;
}

static int fileWrite(const void *buf, SceSize size)
{
	return sceIoWrite(s_fd, buf, size);
}

static int fileOpen(const char *name, SceSize size)
{
	SceUID ret = sceIoOpen(name, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);

	if (ret < 0)
		return ret;
	s_fd = ret;
	return 0;
}

static const CrashdumpOps s_coredumpOps = { coredumpOpen, coredumpWrite, coredumpClose, coredumpRemove };
static const CrashdumpOps s_fileOps = { fileOpen, fileWrite, fileClose, fileRemove };

/* -- the core file ------------------------------------------------------ */

/* name unknown: append a note; returns its descriptor in the buffer (data copied if given) */
static void *crashdumpAddNote(SceUInt32 type, const void *data, SceSize size)
{
	SceSize len = strnlen(s_noteName, 0x20);
	SceSize namesz = len + 1;
	SceUInt32 *hdr;
	char *p, *desc;

	if (((size + 3) & ~3) + s_noteOff + 12 + ((len + 4) & ~3) >= CRASHDUMP_BUF_SIZE)
		return NULL;
	hdr = (SceUInt32 *)(s_buf + s_noteOff);
	hdr[0] = namesz;
	hdr[1] = size;
	hdr[2] = type;
	p = (char *)memcpy(&hdr[3], s_noteName, namesz) + namesz;
	s_noteOff += 12 + namesz;
	while (s_noteOff & 3) {
		*p++ = 0;
		s_noteOff++;
	}
	desc = p;
	if (data != NULL)
		desc = memcpy(p, data, size);
	s_noteOff += size;
	p = desc + size;
	while (s_noteOff & 3) {
		*p++ = 0;
		s_noteOff++;
	}
	return desc;
}

/* name unknown: physical base of the reserved window, by SoC revision */
static inline SceUInt32 crashdumpPhysBase(void)
{
	SceUInt32 rev = ScePervasiveForDriver_714EEFB7();

	if ((rev & 0x1ff00) == 0)
		return (int)rev < 0 ? 0x58000000 : 0x60000000;
	return (rev & 0x30000000) == 0 ? 0x58000000 : 0x60000000;
}

/* name unknown: map the 1 MiB around phys, at (*base + (phys & 0xfffff)) */
static inline SceUID crashdumpMap(SceUInt32 phys)
{
	SceKernelAllocMemBlockKernelOpt opt;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = phys & 0xfff00000;
	return SceSysmemForDriver_C94850C9("SceCrashDump", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_NC_RW, 2 * CRASHDUMP_WINDOW, &opt);
}

/* name unknown */
static int crashdumpWriteCore(const CrashdumpOps *ops, CrashdumpRecord *rec, const char *name, int full)
{
	CrashdumpNoteInfo info;
	Elf32_Ehdr ehdr;
	Elf32_Phdr phdr[10];
	SceUInt32 *ctx;
	char *buf, *map;
	SceSize hdrSize, phSize, notes, total, n, off, size, i;
	SceUInt32 count, cur, phys;
	SceUInt16 phnum;
	SceUID uid, block;
	int ret, ret2;
	void *p;

	uid = SceSysmemForDriver_C94850C9("SceCrashDumpBuf", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW, CRASHDUMP_BUF_SIZE, NULL);
	if (uid < 0)
		return uid;
	ret = SceSysmemForDriver_A841EDDA(uid, (void **)&buf);
	if (ret < 0)
		goto free;

	ehdr = (Elf32_Ehdr){ 0 };
	memset(phdr, 0, sizeof(phdr));
	if (!full) {
		hdrSize = sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr);
		phnum = 1;
		phSize = sizeof(Elf32_Phdr);
		count = 0;
	} else {
		count = rec->nranges;
		if (count > CRASHDUMP_MAX_RANGES) {
			SceSysmemForDriver_009E1C61(uid);
			return -1;
		}
		n = count + (rec->ttbSize != 0);
		phSize = (n + 1) * sizeof(Elf32_Phdr);
		phnum = n + 1;
		hdrSize = phSize + sizeof(Elf32_Ehdr);
	}

	/* ARM ELF32 little endian core, version 5 EABI */
	s_noteOff = 0;
	s_buf = buf;
	ehdr.e_ident[EI_MAG0] = ELFMAG0;
	ehdr.e_ident[EI_MAG1] = ELFMAG1;
	ehdr.e_ident[EI_MAG2] = ELFMAG2;
	ehdr.e_ident[EI_MAG3] = ELFMAG3;
	ehdr.e_ident[EI_CLASS] = ELFCLASS32;
	ehdr.e_ident[EI_DATA] = ELFDATA2LSB;
	ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	ehdr.e_type = ET_CORE;
	ehdr.e_machine = EM_ARM;
	ehdr.e_version = EV_CURRENT;
	ehdr.e_phoff = sizeof(Elf32_Ehdr);
	ehdr.e_flags = EF_ARM_EABI_VER5;
	ehdr.e_ehsize = sizeof(Elf32_Ehdr);
	ehdr.e_phentsize = sizeof(Elf32_Phdr);
	ehdr.e_phnum = phnum;

	info.size = sizeof(info);
	info.fwVersion = CRASHDUMP_FW_VERSION;
	info.reserved = 0;
	info.thread = rec->thread;
	info.kind = rec->kind;
	for (i = 0; i < 15; i++)
		info.info[i] = rec->info[i];
	crashdumpAddNote(0x1000, &info, sizeof(info));
	if (full)
		crashdumpAddNote(0x1001, (char *)rec + RECORD_WORDS, rec->nwords * 4);
	crashdumpAddNote(0x1100, (char *)rec + RECORD_NOTE_1100, 0x1000);
	ctx = (SceUInt32 *)((char *)rec + RECORD_THREADS + rec->thread * 0x1000);
	if (rec->kind == 0) {
		size = (ctx[13] + ctx[12] + 14 + (ctx[9] + ctx[10]) * 4) * 4;
		p = crashdumpAddNote(0x1200, NULL, size);
		if (p != NULL)
			memcpy(p, ctx, size);
	} else if (rec->kind == 1) {
		size = (ctx[14] + ctx[15] + (ctx[12] + ctx[11] + 4) * 4) * 4;
		p = crashdumpAddNote(0x1201, NULL, size);
		if (p != NULL)
			memcpy(p, ctx, size);
	}
	if (((SceSize *)((char *)rec + RECORD_EXTRA))[1] != 0) {
		p = crashdumpAddNote(0x1202, NULL, ((SceSize *)((char *)rec + RECORD_EXTRA))[1]);
		if (p != NULL)
			memcpy(p, (char *)rec + RECORD_EXTRA, ((SceSize *)((char *)rec + RECORD_EXTRA))[1]);
	}

	notes = s_noteOff;
	phdr[0].p_type = PT_NOTE;
	phdr[0].p_offset = hdrSize;
	phdr[0].p_filesz = notes;
	cur = (notes + 0xfff + hdrSize) & ~0xfff;
	if (full) {
		for (i = 0; i < count; i++) {
			phdr[i + 1].p_type = PT_LOAD;
			phdr[i + 1].p_offset = cur;
			phdr[i + 1].p_paddr = rec->range[i].addr;
			phdr[i + 1].p_filesz = rec->range[i].size;
			phdr[i + 1].p_memsz = rec->range[i].size;
			phdr[i + 1].p_flags = PF_R;
			phdr[i + 1].p_align = 0x1000;
			cur += rec->range[i].size;
		}
		if (rec->ttbSize != 0) {
			phdr[count + 1].p_type = PT_LOAD;
			phdr[count + 1].p_offset = cur;
			phdr[count + 1].p_filesz = rec->ttbSize;
			phdr[count + 1].p_memsz = rec->ttbSize;
			phdr[count + 1].p_flags = PF_R;
			phdr[count + 1].p_align = 0x1000;
			cur += rec->ttbSize;
		}
	}

	ret = ops->open(name, cur);
	if (ret < 0)
		goto free;
	ret = ops->write(&ehdr, sizeof(ehdr));
	if (ret < 0)
		goto fail;
	ret2 = ops->write(phdr, phSize);
	if (ret2 < 0) {
		ret = ret2;
		goto fail;
	}
	total = ret + ret2;
	if (notes != 0) {
		ret = ops->write(buf, notes);
		if (ret < 0)
			goto fail;
		total += ret;
	}
	if (!full)
		goto done;

	for (i = 0; i < count; i++) {
		memset(buf, 0, 0x1000);
		if (phdr[i + 1].p_offset != total) {
			ret = ops->write(buf, phdr[i + 1].p_offset - total);
			if (ret < 0)
				goto fail;
			total += ret;
		}
		for (off = 0; off < rec->range[i].size; off += CRASHDUMP_WINDOW) {
			phys = crashdumpPhysBase() + rec->range[i].offset + off;
			block = crashdumpMap(phys);
			if (block < 0) {
				ret = block;
				goto fail;
			}
			ret = SceSysmemForDriver_A841EDDA(block, (void **)&map);
			if (ret < 0) {
				SceSysmemForDriver_009E1C61(block);
				goto fail;
			}
			n = rec->range[i].size - off;
			if (n >= CRASHDUMP_WINDOW)
				n = CRASHDUMP_WINDOW;
			memcpy(buf, map + (phys & 0xfffff), n);
			ret = ops->write(buf, n);
			if (ret < 0)
				goto fail;	/* (the window stays mapped) */
			SceSysmemForDriver_009E1C61(block);
			total += ret;
		}
	}
	if (rec->ttbSize != 0) {
		memset(buf, 0, 0x1000);
		if (phdr[count + 1].p_offset != total) {
			ret = ops->write(buf, phdr[count + 1].p_offset - total);
			if (ret < 0)
				goto fail;
		}
		for (off = 0; off < rec->ttbSize; off += CRASHDUMP_WINDOW) {
			phys = crashdumpPhysBase() + rec->ttbOffset + off;
			block = crashdumpMap(phys);
			if (block < 0)
				return block;	/* (nothing closed or freed) */
			ret = SceSysmemForDriver_A841EDDA(block, (void **)&map);
			if (ret < 0) {
				SceSysmemForDriver_009E1C61(block);
				goto fail;
			}
			n = rec->ttbSize - off;
			if (n >= CRASHDUMP_WINDOW)
				n = CRASHDUMP_WINDOW;
			memcpy(buf, map + (phys & 0xfffff), n);
			ret = ops->write(buf, n);
			if (ret < 0) {
				SceSysmemForDriver_009E1C61(block);
				goto fail;
			}
			SceSysmemForDriver_009E1C61(block);
		}
	}
done:
	ops->close();
	SceSysmemForDriver_009E1C61(uid);
	return 0;

fail:
	ops->close();
	ops->remove(name);
free:
	SceSysmemForDriver_009E1C61(uid);
	return ret;
}

/* name unknown: dim the screen (lit pixels to 0xc0) to show a dump was taken */
static inline void crashdumpMarkScreen(void)
{
	SceDisplayFrameBuf fb;
	SceUInt32 x, y, *px;

	fb.size = sizeof(fb);
	if (SceDisplayForDriver_19F94C63(SceDisplayForDriver_C8E554C5(), 0, &fb, 1) < 0)
		return;
	for (y = 0; y < fb.height; y++) {
		px = (SceUInt32 *)fb.base + y * fb.pitch;
		for (x = 0; x < fb.width; x++, px++)
			*px = *px != 0 ? 0xc0 : 0;
	}
	SceCpuForDriver_364E68A4(fb.base, fb.width * fb.height * 4);
}

int module_start(SceSize args, void *argp)
{
	SceKernelAllocMemBlockKernelOpt opt;
	CrashdumpRecord *rec;
	SceDateTime t;
	SceUInt32 state;
	char logName[64], coreName[64];
	SceUID uid, fd;
	int full, ret;

	if (SceSysconForDriver_CF5B2F2F(&state) < 0 || (state & 0x7f) != CRASHDUMP_SYSCON_CRASH)
		return SCE_KERNEL_START_NO_RESIDENT;
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_PADDR;
	opt.paddr = crashdumpPhysBase();
	uid = SceSysmemForDriver_C94850C9("SceCrashDump", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_NC_RW, CRASHDUMP_RECORD_SIZE, &opt);
	if (uid < 0)
		return SCE_KERNEL_START_NO_RESIDENT;

	if (SceSysmemForDriver_A841EDDA(uid, (void **)&rec) < 0 ||
	    memcmp(rec, s_magic, sizeof(s_magic)) != 0 || rec->version != CRASHDUMP_RECORD_VER)
		goto out;

	if (sceRtcGetCurrentClockLocalTime(&t) < 0)
		memset(&t, 0, sizeof(t));
	if (!SceSblAIMgrForDriver_274663A0()) {
		if (!SceSblAIMgrForDriver_F4B98F66() && !SceQafMgrForDriver_382C71E8()) {
			full = 0;
			goto coredump;
		}
		ret = SceQafMgrForDriver_382C71E8();
		goto mark;
	}
	if (SceQafMgrForDriver_382C71E8()) {
		if (!SceDipswForDriver_A98FC2FD(0xc1) && !SceDipswForDriver_A98FC2FD(0xc2)) {
			ret = SceQafMgrForDriver_382C71E8();
			goto mark;
		}
		snprintf(logName, sizeof(logName), "host0:./psp2crashdump_%04d%02d%02d_%02d%02d%02d.log",
			 t.year, t.month, t.day, t.hour, t.minute, t.second);
		if (rec->logSize > 1) {
			fd = sceIoOpen(logName, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0666);
			if (fd >= 0) {
				if (sceIoWrite(fd, (char *)rec + rec->logOffset, rec->logSize - 1) < 0) {
					sceIoClose(fd);
					sceIoRemove(logName);
				} else {
					sceIoClose(fd);
				}
			}
		}
		snprintf(coreName, sizeof(coreName), "host0:./psp2crashdump_%04d%02d%02d_%02d%02d%02d.elf",
			 t.year, t.month, t.day, t.hour, t.minute, t.second);
		SceDebugForDriver_391B74B7("[CrashDump] Generating %s start\n", coreName);
		ret = crashdumpWriteCore(&s_fileOps, rec, coreName, 1);
		if (ret < 0)
			SceDebugForDriver_391B74B7("[CrashDump] Generating %s failed 0x%08X\n", coreName, ret);
		else
			SceDebugForDriver_391B74B7("[CrashDump] Generating %s done\n", coreName);
		ret = SceQafMgrForDriver_382C71E8();
mark:
		if (ret != 0)
			crashdumpMarkScreen();
	}
	full = 1;
coredump:
	SceDebugForDriver_391B74B7("[CrashDump] Generating coredump for crashdump start\n");
	ret = crashdumpWriteCore(&s_coredumpOps, rec, coreName, full);
	if (ret < 0)
		SceDebugForDriver_391B74B7("[CrashDump] Generating coredump for crashdump failed 0x%08X\n", ret);
	else
		SceDebugForDriver_391B74B7("[CrashDump] Generating coredump for crashdump done\n");
	memset(rec, 0xff, CRASHDUMP_RECORD_VER);
out:
	SceSysmemForDriver_009E1C61(uid);
	return SCE_KERNEL_START_NO_RESIDENT;
}
