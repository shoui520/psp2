/*
 * cui_setupper.self: the devkit's command-line system update setup ('psp2ctrl update').
 * It checks the PUP given on the command line, copies it to ud0:/PSP2UPDATE, extracts the
 * updater (psp2swu.self) from it, sets the update mode and reboots, for psp2swu to write
 * the firmware.
 *
 * No build of it names its functions or this file: the names are ours.
 */
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <kernel/processmgr.h>
#include <libkernel/libkernel.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/stdio.h>
#include <updater_core/updater_core.h>
#include "cui_view.h"

#define CUI_SETUPPER_ERROR_NO_MEMORY		0x80182001
#define CUI_SETUPPER_ERROR_PATH_TOO_LONG	0x80182006
#define CUI_SETUPPER_ERROR_INVALID_DST		0x80182007
#define CUI_SETUPPER_ERROR_ALREADY_UPDATE_MODE	0x80182801
#define CUI_SETUPPER_ERROR_PUP_NOT_FOUND	0x80182803
#define CUI_SETUPPER_ERROR_PUP_VERSION		0x80182804
#define CUI_SETUPPER_ERROR_PUP_AUTH		0x80182805
#define CUI_SETUPPER_ERROR_PUP_BROKEN		0x80182806
#define CUI_SETUPPER_ERROR_COPY			0x80182809

/* SceSblSsUpdateMgr: the PUP's version is not one this system takes */
#define SCE_SBL_US_ERROR_PUP_VERSION		0x800f0237

#define CUI_SETUPPER_COPY_BUFFER_SIZE	0x40000
#define CUI_SETUPPER_PROGRESS_INTERVAL	20

/* the PAF heap this program runs on */
SceUInt32 sce_paf_global_heap_size = 0x1c00000;
SceInt32 sce_paf_global_heap_reserved0 = 60000;
SceInt32 sce_paf_global_heap_reserved1 = 0x40000;

/* the process and its main thread (crt0's sceProcessParam): no preloaded module but libdbg */
extern "C" const unsigned int sce_process_preload_disabled = 0x01fb0000;
extern "C" const char sceUserMainThreadName[] = "SceCuiSetUpper";
extern "C" const char sceProcessName[] = "SceCuiSetUpper";
extern "C" const int sceUserMainThreadPriority = 160;

static int s_progressCount;

static void *CuiAlloc(SceSize size);
static void CuiFree(void *ptr);
static void *CuiMemalign(SceSize align, SceSize size);

extern "C" void __cxa_pure_virtual(void)
{
	__builtin_breakpoint(0);
}

class CuiSetUpper {
public:
	CuiSetUpper();
	~CuiSetUpper();

	int Setup(int argc, char *argv[]);
	int Install(void);

private:
	int Initialize(const char *pupPath);
	int CheckPup(void);
	int CheckWorkingDirectory(void);
	int CopyPup(void);
	void ReportProgress(void);
	int ExtractUpdater(void);
	static void CopyCallback(void *arg, SceInt32 status, SceUInt64 offset, SceUInt64 copied);

	char *m_pupPath;
	void *m_buffer;
	const char *m_workDir;
	SceSize m_workDirLen;
	const char *m_pupDst;
	SceSize m_pupDstLen;
	char *m_updaterPath;
	SceUInt64 m_updaterSize;
	SceUInt64 m_total;
	SceUInt64 m_done;
};

int CuiSetUpper::Setup(int argc, char *argv[])
{
	const char *pupPath;
	int ret;

	CuiInformUpdateStarted(0x80000001, 0, 30, "system update setup start.", 26);

	pupPath = "host0:PSP2UPDAT.PUP";
	if (argc > 1)
		pupPath = argv[1];
	sceClibPrintf("[INFO]%s\n", pupPath);

	sceClibPrintf("[INFO]%s\n", "initialize setup start...");
	ret = Initialize(pupPath);
	if (ret != 0) {
		CuiInformUpdateFinished(ret, "initialize setup failed.", 24);
		return ret;
	}
	CuiInformUpdateOngoing(1, 0, 30, "initialize setup done.", -1);

	sceClibPrintf("[INFO]%s\n", "check working directory...");
	ret = CheckWorkingDirectory();
	if (ret != 0) {
		CuiInformUpdateFinished(ret, "check working directory failed.", 31);
		return ret;
	}
	CuiInformUpdateOngoing(1, 0, 60, "check working directory done.", -1);

	sceClibPrintf("[INFO]%s\n", "check pup...");
	ret = CheckPup();
	if (ret != 0) {
		CuiInformUpdateFinished(ret, "check pup failed.", 17);
		return ret;
	}
	CuiInformUpdateOngoing(1, 0, 100, "check pup done.", -1);
	return 0;
}

int CuiSetUpper::Install(void)
{
	static char message[64];
	int ret;

	CuiInformUpdateStarted(0x80000002, 0, 300, "copy pup...", 11);
	ret = CopyPup();
	if (ret != 0) {
		CuiInformUpdateFinished(ret, "copy pup failed.", 16);
		return ret;
	}
	sceClibPrintf("[INFO]%s\n", "copy pup done.");

	sceClibPrintf("[INFO]%s\n", "extract updater...");
	ret = ExtractUpdater();
	if (ret != 0) {
		CuiInformUpdateFinished(ret, "extract updater failed.", 23);
		return ret;
	}
	sceClibPrintf("[INFO]%s\n", "extract updater done.");

	sce_paf_snprintf(message, sizeof(message), "set update mode (0x%02x)...",
			 SCE_UPDATER_UPDATE_MODE_UPDATE);
	sceClibPrintf("[INFO]%s\n", message);
	ret = sceUpdaterSetUpdateMode(SCE_UPDATER_UPDATE_MODE_UPDATE);
	if (ret != 0) {
		CuiInformUpdateFinished(ret, "set update mode failed.", 23);
		return ret;
	}
	CuiInformUpdateOngoing(2, 0, 100, "set update mode done.", -1);
	return 0;
}

int main(int argc, char *argv[])
{
	SceUpdaterMemoryInfo memoryInfo;
	SceUInt8 mode;
	int ret;

	sceKernelPowerLock(SCE_KERNEL_POWER_TICK_DEFAULT);
	CuiInformUpdateStarted(0x80000000, 0, 30, "cui_setupper.self: Build: May 21 2012 16:15:54", 46);

	memoryInfo.updaterCoreAlloc = CuiAlloc;
	memoryInfo.updaterCoreFree = CuiFree;
	memoryInfo.updaterCoreMemalign = CuiMemalign;
	ret = sceUpdaterInitialize(&memoryInfo);
	if (ret != 0) {
		CuiInformUpdateFinished(ret, "initialize failed.", 18);
		goto out;
	}
	CuiInformUpdateOngoing(0, 0, 50, "initialize done.", -1);

	mode = 0x30;
	ret = sceUpdaterGetUpdateMode(&mode);
	if (ret != 0) {
		CuiInformUpdateFinished(ret, "get update mode failed.", 23);
		goto out;
	}
	if (mode != SCE_UPDATER_UPDATE_MODE_NONE) {
		CuiInformUpdateFinished(CUI_SETUPPER_ERROR_ALREADY_UPDATE_MODE,
					"already in update mode.", 23);
		sceClibPrintf("[INFO]%s\n", "\tdisable update mode.");
		ret = sceUpdaterSetUpdateMode(SCE_UPDATER_UPDATE_MODE_NONE);
		if (ret < 0)
			CuiInformUpdateFinished(ret, "set update mode failed.", 23);
		sceClibPrintf("[INFO]%s\n", "\tdelete self & PUP in ud0:PSP2UPDATE directory.");
		ret = sceUpdaterCleanUp();
		if (ret < 0)
			CuiInformUpdateFinished(ret, "clean up ud0:PSP2UPDATE failed.", 31);
		if (ret >= 0)
			sceClibPrintf("[INFO]%s\n", "\tplease run 'psp2ctrl update' again.\n\n\n");
		ret = -1;
		sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
		return ret;
	}
	CuiInformUpdateOngoing(0, 0, 100, "check update mode done.", -1);
	goto run;
out:
	if (ret != 0) {
		sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
		return ret;
	}
run:
	{
		CuiSetUpper setupper;

		ret = setupper.Setup(argc, argv);
		if (ret != 0) {
			sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
			return ret;
		}
		ret = setupper.Install();
		if (ret != 0) {
			sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
			return ret;
		}
		sceClibPrintf("[INFO]%s\n", "reboot to UPDATE MODE and overwrite firmware.");
		sceClibPrintf("[INFO]%s\n", "this will take a few more minutes.");
		sceClibPrintf("[INFO]%s\n", "the progress is shown on the VITA display.\n\n");
		sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
		ret = sceUpdaterFinalize(0);
		if (ret != 0) {
			CuiInformUpdateFinished(ret, "reboot failed.", 14);
			sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
			return ret;
		}
		return 0;
	}
}

static void *CuiAlloc(SceSize size)
{
	return sce_paf_malloc(size);
}

static void CuiFree(void *ptr)
{
	sce_paf_free(ptr);
}

static void *CuiMemalign(SceSize align, SceSize size)
{
	return sce_paf_memalign(align, size);
}

CuiSetUpper::CuiSetUpper()
	: m_pupPath(NULL), m_buffer(NULL), m_workDir(NULL), m_workDirLen(0), m_pupDst(NULL),
	  m_pupDstLen(0), m_updaterPath(NULL), m_updaterSize(0), m_total(0), m_done(0)
{
}

CuiSetUpper::~CuiSetUpper()
{
	if (m_pupPath != NULL)
		sce_paf_free(m_pupPath);
	if (m_buffer != NULL)
		sce_paf_free(m_buffer);
	if (m_updaterPath != NULL)
		sce_paf_free(m_updaterPath);
}

int CuiSetUpper::Initialize(const char *pupPath)
{
	SceSize len;

	len = sce_paf_strlen(pupPath);
	if (len > 0x1000)
		return CUI_SETUPPER_ERROR_PATH_TOO_LONG;
	m_buffer = sce_paf_memalign(512, CUI_SETUPPER_COPY_BUFFER_SIZE);
	if (m_buffer == NULL)
		return CUI_SETUPPER_ERROR_NO_MEMORY;
	m_pupPath = (char *)sce_paf_malloc(len + 1);
	if (m_pupPath == NULL)
		return CUI_SETUPPER_ERROR_NO_MEMORY;
	sce_paf_memcpy(m_pupPath, pupPath, len + 1);
	return 0;
}

int CuiSetUpper::CheckPup(void)
{
	int ret;

	ret = sceUpdaterIsApplicablePup(m_pupPath);
	if (ret != 0) {
		switch (ret) {
		case SCE_UPDATER_ERROR_PUP_NOT_FOUND:
		case SCE_UPDATER_ERROR_PUP_NO_FILE:
			CuiInformUpdateFinished(CUI_SETUPPER_ERROR_PUP_NOT_FOUND, "pup not found.", 14);
			break;
		case SCE_SBL_US_ERROR_PUP_VERSION:
			CuiInformUpdateFinished(CUI_SETUPPER_ERROR_PUP_VERSION, "pup version is invalid.", 23);
			break;
		case SCE_UPDATER_ERROR_PUP_AUTH:
			CuiInformUpdateFinished(CUI_SETUPPER_ERROR_PUP_AUTH, "pup auth failure.", 17);
			break;
		default:
			CuiInformUpdateFinished(ret, "pup is invalid.", 15);
			break;
		}
		return ret;
	}
	ret = sceUpdaterGetSegmentSize(m_pupPath, SCE_UPDATER_SEGMENT_ID_UPDATER, &m_updaterSize);
	if (ret != 0) {
		CuiInformUpdateFinished(CUI_SETUPPER_ERROR_PUP_BROKEN, "pup is broken.", 14);
		CuiInformUpdateFinished(ret, "get updater size failed.", 24);
		return ret;
	}
	return 0;
}

int CuiSetUpper::CheckWorkingDirectory(void)
{
	int ret;

	m_workDir = sceUpdaterGetWorkingDirectory();
	m_pupDst = sceUpdaterGetPupPath();
	if (m_workDir == NULL || m_pupDst == NULL) {
		CuiInformUpdateFinished(CUI_SETUPPER_ERROR_INVALID_DST, "invalid dst path.", 17);
		return CUI_SETUPPER_ERROR_INVALID_DST;
	}
	m_workDirLen = sce_paf_strlen(m_workDir);
	m_pupDstLen = sce_paf_strlen(m_pupDst);
	m_updaterPath = (char *)sce_paf_malloc(m_workDirLen + 14);
	if (m_updaterPath == NULL)
		return CUI_SETUPPER_ERROR_NO_MEMORY;
	sce_paf_memcpy(m_updaterPath, m_workDir, m_workDirLen);
	m_updaterPath[m_workDirLen] = '/';
	sce_paf_memcpy(m_updaterPath + m_workDirLen + 1, "psp2swu.self", 13);
	ret = sceUpdaterPartitionSetUp();
	if (ret < 0) {
		CuiInformUpdateFinished(ret, "check dst directory failed", 26);
		return ret;
	}
	return 0;
}

int CuiSetUpper::CopyPup(void)
{
	SceUpdaterCopyFileOption option;
	SceUpdaterCopyFileTarget src;
	SceUpdaterCopyFileTarget dst;
	int ret;

	if (sce_paf_strncmp(m_pupPath, m_pupDst, m_pupDstLen) == 0)
		return 0;
	sce_paf_memset(&src, 0, sizeof(src));
	sce_paf_memset(&dst, 0, sizeof(dst));
	sce_paf_memset(&option, 0, sizeof(option));
	src.path = m_pupPath;
	dst.path = m_pupDst;
	option.arg = this;
	option.bufferSize = CUI_SETUPPER_COPY_BUFFER_SIZE;
	option.callback = CopyCallback;
	ret = sceUpdaterCopyFile(&src, &dst, &option);
	if (ret < 0)
		return ret;
	return 0;
}

void CuiSetUpper::ReportProgress(void)
{
	if (++s_progressCount % CUI_SETUPPER_PROGRESS_INTERVAL == 0)
		CuiInformUpdateOngoing(2, 0, (int)(m_done * 100 / m_total), NULL, -1);
}

int CuiSetUpper::ExtractUpdater(void)
{
	SceUInt64 size;
	SceUInt64 offset;
	SceUInt64 chunk;
	SceUID fd;
	int written;
	int ret;
	int n;

	size = m_updaterSize;
	fd = sceIoOpen(m_updaterPath, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 6);
	if (fd < 0) {
		CuiInformUpdateFinished(CUI_SETUPPER_ERROR_COPY, "extract self failed.", 20);
		CuiInformUpdateFinished(fd, "open dst failed.", 16);
		return fd;
	}
	offset = 0;
	ret = 0;
	chunk = CUI_SETUPPER_COPY_BUFFER_SIZE;
	while (offset < size && ret == 0) {
		if (size - offset < CUI_SETUPPER_COPY_BUFFER_SIZE)
			chunk = size - offset;
		ret = sceUpdaterParse(m_pupDst, SCE_UPDATER_SEGMENT_ID_UPDATER, m_buffer, chunk, offset);
		if (ret != 0) {
			CuiInformUpdateFinished(CUI_SETUPPER_ERROR_PUP_BROKEN, "extract self failed.", 20);
			CuiInformUpdateFinished(ret, "read src failed.", 16);
			break;
		}
		for (written = 0; written < chunk; written += n) {
			n = sceIoWrite(fd, (char *)m_buffer + written, chunk - written);
			if (n < 0) {
				CuiInformUpdateFinished(CUI_SETUPPER_ERROR_COPY, "copy dst failed.", 16);
				CuiInformUpdateFinished(n, "write dst failed.", 17);
				ret = n;
				break;
			}
			m_done += n;
		}
		offset += chunk;
		if (++s_progressCount % CUI_SETUPPER_PROGRESS_INTERVAL == 0) {
			CuiInformUpdateOngoing(2, 0, (int)(m_done * 100 / m_total), NULL, -1);
			if (offset >= size)
				break;
		}
	}
	sceIoClose(fd);
	return ret;
}

void CuiSetUpper::CopyCallback(void *arg, SceInt32 status, SceUInt64 offset, SceUInt64 copied)
{
	CuiSetUpper *setupper = (CuiSetUpper *)arg;
	SceUInt64 total;

	total = setupper->m_total;
	setupper->m_done = copied;
	if (total == 0) {
		total = offset + setupper->m_updaterSize;
		setupper->m_total = total;
	}
	if (++s_progressCount % CUI_SETUPPER_PROGRESS_INTERVAL == 0)
		CuiInformUpdateOngoing(2, 0, (int)(copied * 100 / total), NULL, -1);
}
