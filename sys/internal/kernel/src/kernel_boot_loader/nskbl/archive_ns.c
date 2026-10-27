/*
 * NSKBL: the kernel it loads (archive_ns.c in Sony's debug records). Core 0 mounts
 * os0: and sd0: (sceSdStandaloneInit), reads the secure modules the kernel hands to
 * the secure processor into a cache the sysroot keeps (cacheSecurityModules), and
 * loads psp2bootconfig.skprx from os0:; every core then starts its part of it, and
 * psp2bootconfig loads the rest of the kernel.
 */

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <fw.h>
#include <kernel/iofilemgr.h>
#include <iofilemgr/iofilemgr.h>
#include <modulemgr/modulemgr.h>
#include <modulemgr/modulemgr_control.h>
#include <kernel_boot_loader/kernel_boot_param.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/memblock.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include "../../sysmem/sysmem_internal.h"

/* kbl_main.c */
void sceSdStandaloneInit(void);

/* the rest of the kernel */
int sceKernelLibraryDBSetPsp2bootcfg(SceUID modid);
int sceKernelGetModuleBootEntry(SceUID modid, SceModuleEntry *entry);

#define LOAD_MODULE_SHARED		0x20	/* sceKernelLoadModule's flag */
#define MODULE_BOOT_ARGS		4	/* argp is the boot param */

/* the secure modules' cache: each image 64-byte aligned */
#define SECURITY_MODULE_CACHE_SIZE	0x20000
#define SECURITY_MODULE_ALIGN		64
#define SECURITY_MODULES		3

static const char *const s_bootconfigModules[] = {
	"psp2bootconfig.skprx",
};

static const char *const s_securityModules[SECURITY_MODULES] = {
	"os0:sm/gcauthmgr_sm.self",
	"os0:sm/rmauth_sm.self",
	"os0:sm/encdec_w_portability_sm.self",
};

/* the cores take the sysroot's lock around each start; cleared, core 0 starts alone */
static int s_bootLock = 1;
static int s_bootStatus;
static SceUID s_bootconfigId;

/*
 * names[i] loaded into modid[i] (0 for a NULL name): from os0:kd/, or os0: itself
 * when fromRoot is set
 */
static int loadModules(const char *const *names, SceUID *modid, int count, int fromRoot,
		       int shared)
{
	char path[256];
	SceUID uid;
	int i;

	path[sizeof(path) - 1] = '\0';
	for (i = 0; i < count; i++) {
		if (names[i] == NULL) {
			modid[i] = 0;
			continue;
		}
		path[0] = '\0';
		if (fromRoot)
			snprintf(path, sizeof(path) - 1, "%s:%s", "os0", names[i]);
		else
			snprintf(path, sizeof(path) - 1, "%s:kd/%s", "os0", names[i]);
		uid = sceKernelLoadModule(path, shared ? LOAD_MODULE_SHARED : 0, NULL);
		if (uid < 0) {
			SCE_DBG_PRINTF(0xf, 142, "ERROR: loading %s failed (0x%08X)\n", path, uid);
			return uid;
		}
		SCE_DBG_PRINTF(0, 146, "\n");
		modid[i] = uid;
	}
	return 0;
}

/* name unknown: loadModules from os0:kd/ (no call reaches it) */
int KblLoadKernelModules(const char *const *names, SceUID *modid, int count, int shared)
{
	return loadModules(names, modid, count, 0, shared);
}

/* name unknown: the boot without the cores' lock (no call reaches it) */
int KblBootModulesUnlocked(void)
{
	s_bootLock = 0;
	return 0;
}

/* starts each module on core 0, then runs its boot entry on every core */
int sceKernelBootBootModules(const SceUID *modid, int count, SceSize args, void *argp)
{
	SceModuleEntry entry;
	SceUID uid;
	int i, res;

	for (i = 0; i < count; i++) {
		uid = modid[i];
		if (uid == 0)
			continue;
		if (s_bootLock)
			sceKernelSysrootCorelockLock(0);
		if (sceKernelCpuId() == 0) {
			res = sceKernelStartModule(uid, args, argp, 0, NULL, &s_bootStatus);
			if (res < 0) {
				SCE_DBG_PRINTF(0xf, 210,
					       "ERROR: sceKernelStartModule() failed (0x%08X)\n", res);
				return res;
			}
		}
		if (s_bootLock)
			sceKernelSysrootCorelockUnlock();
		if (s_bootStatus == SCE_KERNEL_START_NO_RESIDENT)
			continue;
		res = sceKernelGetModuleBootEntry(uid, &entry);
		if (res < 0) {
			/* Sony prints the module's UID here */
			SCE_DBG_PRINTF(0xf, 233,
				       "ERROR: sceKernelGetModuleBootEntry() failed (0x%08X)\n",
				       uid);
			return res;
		}
		if (entry != NULL) {
			res = entry(args, argp);
			if (res < 0) {
				SCE_DBG_PRINTF(0xf, 245, "ERROR: boot_entry() failed (0x%08X)\n", res);
				return res;
			}
		}
	}
	return 0;
}

/*
 * name unknown: a file read whole into buffer, its size in *length; 0, or an error. Sony
 * tests the seeks' offsets as ints, and returns a failed one as an int
 */
static inline int readFile(const char *path, void *buffer, SceSize max, SceSize *length)
{
	SceOff size;
	SceUID fd;
	int res;

	fd = sceIoOpen(path, SCE_O_RDONLY, 0);
	if (fd < 0)
		return fd;
	size = sceIoLseek(fd, 0, SCE_SEEK_END);
	if (size < 0) {
		sceIoClose(fd);
		return size;
	}
	if ((SceSize)size > max || (int)sceIoLseek(fd, 0, SCE_SEEK_SET) < 0 ||
	    sceIoRead(fd, buffer, size) < 0) {
		sceIoClose(fd);
		return -1;
	}
	res = sceIoClose(fd);
	if (res < 0)
		return res;
	*length = size;
	return 0;
}

/* the secure modules read into a block the sysroot keeps for the kernel */
static inline void cacheSecurityModules(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	struct SysrootSelfImage *images;
	SceSize left, length, aligned;
	SceUID uid;
	char *base;
	int i;

	images = SceSysmemForKernel_85571907(sizeof(*images) * SECURITY_MODULES);
	if (images == NULL) {
		SCE_DBG_ASSERT_CHECK(images != NULL, 329);
		return;
	}
	memset(images, 0, sizeof(*images) * SECURITY_MODULES);
	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = ALLOC_ATTR_NO_CLEAR;
	uid = sceKernelAllocMemBlockForKernel("SceKernelSecurityModuleCache",
					      SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW,
					      SECURITY_MODULE_CACHE_SIZE, &opt);
	if (uid < 0) {
		SCE_DBG_ASSERT_FAIL(344);
		SceSysmemForKernel_4233C16D(images);
		return;
	}
	sceKernelGetMemBlockBase(uid, (void **)&base);
	left = SECURITY_MODULE_CACHE_SIZE;
	length = 0;
	for (i = 0; i < SECURITY_MODULES; i++) {
		if (readFile(s_securityModules[i], base, left, &length) < 0)
			continue;
		aligned = (length + SECURITY_MODULE_ALIGN - 1) & ~(SECURITY_MODULE_ALIGN - 1);
		left -= aligned;
		images[i].data = base;
		base += aligned;
		images[i].size = length;
	}
	SysrootSetSelfImages(images);
}

int KblArchive(SceKernelBootParam *param)
{
	int res;

	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0) {
		sceSdStandaloneInit();
		cacheSecurityModules();
	}
	sceKernelSysrootCorelockUnlock();

	sceKernelSysrootCorelockLock(0);
	if (sceKernelCpuId() == 0) {
		res = loadModules(s_bootconfigModules, &s_bootconfigId, 1, 1, 0);
		if (res < 0) {
			SCE_DBG_PRINTF(0xf, 422, "ERROR: loadModules() failed (0x%08X)\n", res);
			return res;
		}
		sceKernelLibraryDBSetPsp2bootcfg(s_bootconfigId);
	}
	sceKernelSysrootCorelockUnlock();
	res = sceKernelBootBootModules(&s_bootconfigId, 1, MODULE_BOOT_ARGS, param);
	if (res < 0) {
		SCE_DBG_PRINTF(0xf, 438, "ERROR: sceKernelBootBootModules() failed (0x%08X)\n",
			       res);
		return res;
	}
	return 0;
}
