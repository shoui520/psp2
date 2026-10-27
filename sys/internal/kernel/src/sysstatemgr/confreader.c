/* Boot-script commands, path policy, and the two-pass configuration reader. */
#include <sysmem/sysmem.h>
#include <processmgr/processmgr.h>
#include "sysstatemgr.h"

/* Sony's tables are not const; the compiler finds them unwritten and puts them in rodata. */
static struct { const char *path; unsigned size; } s_devices[] = {
	{ "sd0:", 4 }, { "host0:", 6 }, { "os0:", 4 }, { "vs0:", 4 },
	{ "ur0:", 4 }, { "ud0:", 4 }, { "ux0:", 4 }, { NULL, 0 },
};

void ConfReaderInit(ConfReader *reader, int depth)
{
	int i;

	memset(reader, 0, sizeof(*reader));
	reader->depth = depth;
	reader->confBlock = -1;
	reader->cmdBlock = -1;
	reader->confModule = -1;
	reader->cwd = SceSysmemForKernel_85571907(0x400);
	if (!reader->cwd)
		SCE_DBG_ASSERT_FAIL(167);
	reader->cwd[0x3ff] = 0;
	reader->absPath = SceSysmemForKernel_85571907(0x400);
	if (!reader->absPath)
		SCE_DBG_ASSERT_FAIL(173);
	reader->absPath[0x3ff] = 0;
	reader->remotePath = SceSysmemForKernel_85571907(0x400);
	if (!reader->remotePath)
		SCE_DBG_ASSERT_FAIL(179);
	reader->remotePath[0x3ff] = 0;
	for (i = 0; i < 4; i++)
		reader->rpath[i] = NULL;
	reader->shellFilename = NULL;
	for (i = 0; i < 32; i++) {
		reader->env[i].name = NULL;
		reader->env[i].value = NULL;
	}
	reader->envCount = 0;
}

/* name unknown: release all reader resources after execution. */
static void ConfReaderClose(ConfReader *reader)
{
	int status, i;

	ConfReaderUtilsFreeCmdBuf(reader);
	if (reader->confModule == -1)
		ConfReaderUtilsFreeConfBuf(reader);
	else
		sceKernelStopUnloadModule(reader->confModule, 0, NULL, 0, NULL, &status);
	for (i = 0; i < 4; i++)
		if (reader->rpath[i])
			SceSysmemForKernel_4233C16D(reader->rpath[i]);
	ConfReaderUtilsFreeLoaded(reader);
	SceSysmemForKernel_4233C16D(reader->absPath);
	SceSysmemForKernel_4233C16D(reader->cwd);
	SceSysmemForKernel_4233C16D(reader->remotePath);
	if (reader->shellFilename)
		SceSysmemForKernel_4233C16D(reader->shellFilename);
}

static __attribute__((noclone)) char *ConfReaderRelToAbsPath(ConfReader *reader, char *path)
{
	const char *cwd;
	int external, i;
	unsigned size;

	if (!strnlen(path, 0x3ff))
		return NULL;
	if (path[0] == '/' || path[0] == '\\')
		return path;
	external = ConfReaderUtilsIsManufacturingMode();
	for (i = 0; s_devices[i].path; i++) {
		if (!strncmp(s_devices[i].path, path, s_devices[i].size)) {
			if (external || (strncmp("sd0:", path, 4) && strncmp("ux0:", path, 4)))
				return path;
			SCE_DBG_PRINTF(0, 1742, "psp2config : no permission to access sd0/ux0:(%s)\n", path);
			return NULL;
		}
	}
	cwd = reader->cwd;
	size = strnlen(cwd, 0x3ff);
	strncpy(reader->absPath, cwd, 0x3ff);
	strncpy(reader->absPath + size, path, 0x3ff - size);
	path = reader->absPath;
	if (external || (strncmp("sd0:", path, 4) && strncmp("ux0:", path, 4)))
		return path;
	SCE_DBG_PRINTF(0, 1761, "psp2config : no permission to access sd0/ux0:(%s)\n", path);
	return NULL;
}

static char *ConfReaderGetRemotePath(char **buffer, const char *path)
{
	int len = strnlen(path, 0x3ff), i;

	if (s_noRemoteModule || !SceQafMgrForDriver_082A4FC2() ||
	    !sceKernelCheckDipsw(DIPSW_HOST0_ACCESS) || !strncmp("host0:", path, 6))
		return NULL;
	strncpy(*buffer, "host0:module", 0x3ff);
	(*buffer)[12] = '/';
	(*buffer)[13] = 0;
	for (i = len - 1; i >= 0; i--) {
		if (path[i] == '/' || path[i] == '\\' || path[i] == ':') {
			strncpy(*buffer + 13, path + i + 1, 0x3f2);
			return *buffer;
		}
	}
	SCE_DBG_PRINTF(0, 1809, "ConfReaderGetRemotePath : wrong path(%s)\n", path);
	SCE_DBG_ASSERT_FAIL(1810);
	return *buffer;
}

static char *ConfReaderGetRemoteRpath(ConfReader *reader, const char *path, int index)
{
	int len = strnlen(path, 0x3ff), i;
	char *rpath = reader->rpath[index];
	unsigned size;

	if (!rpath || s_noRemoteRpath || !strncmp("host0:", path, 6))
		return NULL;
	strncpy(reader->remotePath, rpath, 0x3ff);
	size = strnlen(reader->rpath[index], 0x400);
	for (i = len - 1; i >= 0; i--) {
		if (path[i] == '/' || path[i] == '\\' || path[i] == ':') {
			strncpy(reader->remotePath + size, path + i + 1, 0x3ff - size);
			return reader->remotePath;
		}
	}
	SCE_DBG_PRINTF(0, 1849, "ConfReaderGetRemoteRpath : wrong path(%s)\n", path);
	SCE_DBG_ASSERT_FAIL(1850);
	return reader->remotePath;
}

static int ConfReaderCmdEcho(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	int i;
	for (i = 0; i < argc; i++)
		SCE_DBG_PRINTF(0, 1593, "%s ", argv[i]);
	SCE_DBG_PRINTF(0, 1595, "\n");
	return 0;
}

static int ConfReaderCmdSleep(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	SCE_DBG_PRINTF(0, 1581, "sleep command\n");
	return 0;
}

static int ConfReaderCmdEnd(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	SCE_DBG_PRINTF(0, 1571, "Finished configure commands.\n");
	return 0;
}

static int ConfReaderCmdKill(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	SCE_DBG_PRINTF(0, 1557, "kill command\n");
	return 0;
}

static int ConfReaderCmdUnknown(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	SCE_DBG_PRINTF(0, 993, "unknown command (ignored)\n");
	return 0;
}

static int ConfReaderCmdSetEnv(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	const char *name;
	int count;

	if (argc <= 1) {
		SCE_DBG_PRINTF(0, 1935, "syntax error (setenv)\n");
		return CONF_ERROR(22);
	}
	name = argv[0];
	if (!strnlen(name, 0x100)) {
		SCE_DBG_PRINTF(0, 1940, "syntax error (setenv) : too short name\n");
		return CONF_ERROR(22);
	}
	if (!strnlen(argv[1], 0x100)) {
		SCE_DBG_PRINTF(0, 1945, "syntax error (setenv) : too short value\n");
		return CONF_ERROR(22);
	}
	if (reader->envCount >= 32) {
		SCE_DBG_PRINTF(0, 1950, "syntax error (setenv) : no space\n");
		return CONF_ERROR(12);
	}
	count = reader->envCount;
	reader->env[count].name = name;
	reader->env[count].value = argv[1];
	reader->envCount = count + 1;
	return 0;
}

static int ConfReaderCmdCd(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	char *path, *arg;
	unsigned size;
	int i;

	if (argc < 1) {
		SCE_DBG_PRINTF(0, 1894, "syntax error (cd)\n");
		return CONF_ERROR(22);
	}
	arg = argv[0];
	if (!strnlen(arg, 0x100))
		return CONF_ERROR(22);
	path = ConfReaderRelToAbsPath(reader, arg);
	if (!path)
		return CONF_ERROR(13);
	size = strnlen(path, 0x3ff);
	strncpy(reader->cwd, path, 0x3fe);
	for (i = 0; s_devices[i].path; i++)
		if (!strncmp(s_devices[i].path, path, s_devices[i].size) && size == s_devices[i].size)
			goto changed;
	if (reader->cwd[size - 1] != '/') {
		reader->cwd[size] = '/';
		reader->cwd[size + 1] = 0;
	}
changed:
	SCE_DBG_PRINTF(0, 1924, "Changing current working directory to %s\n", reader->cwd);
	return 0;
}

static int ConfReaderCmdLoad(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	char *path, *remote;
	int ret, status;

	if (argc < 1)
		return CONF_ERROR(22);
	path = ConfReaderRelToAbsPath(reader, argv[0]);
	if (!path)
		return CONF_ERROR(13);
	remote = ConfReaderGetRemotePath(&reader->remotePath, path);
	if ((!remote || (ret = sceKernelLoadStartModule(remote, len, args, 0, NULL, &status)) < 0) &&
	    (ret = sceKernelLoadStartModule(path, len, args, 0x800, NULL, &status)) < 0)
		ret = sceKernelLoadStartModule(path, len, args, 0, NULL, &status);
	if (ret >= 0) {
		SCE_DBG_PRINTF(0, 1032, "\n");
		ret = ConfReaderUtilsRememberLoaded(reader, ret, path);
	}
	return ret;
}

/* name unknown: load without starting. */
static int ConfReaderCmdLoadOnly(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	char *path, *remote;
	int ret;

	if (argc < 1)
		return CONF_ERROR(22);
	path = ConfReaderRelToAbsPath(reader, argv[0]);
	if (!path)
		return CONF_ERROR(13);
	remote = ConfReaderGetRemotePath(&reader->remotePath, path);
	if ((!remote || (ret = sceKernelLoadModule(remote, 0, NULL)) < 0) &&
	    (ret = sceKernelLoadModule(path, 0x800, NULL)) < 0)
		ret = sceKernelLoadModule(path, 0, NULL);
	if (ret < 0)
		return ret;
	return ConfReaderUtilsRememberLoaded(reader, ret, path);
}

/* names unknown: start, stop, unload, and hardware-specific load. */
static int ConfReaderCmdStart(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	char *path;
	SceUID uid;
	int status;

	if (argc < 1)
		return CONF_ERROR(22);
	path = ConfReaderRelToAbsPath(reader, argv[0]);
	if (!path || (uid = ConfReaderUtilsFindLoaded(reader, path)) < 0)
		return CONF_ERROR(2);
	return sceKernelStartModule(uid, len, args, 0, NULL, &status);
}

static int ConfReaderCmdStop(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	char *path;
	SceUID uid;
	int status;

	if (argc < 1)
		return CONF_ERROR(22);
	path = ConfReaderRelToAbsPath(reader, argv[0]);
	if (!path || (uid = ConfReaderUtilsFindLoaded(reader, path)) < 0)
		return CONF_ERROR(2);
	return sceKernelStopModule(uid, len, args, 0, NULL, &status);
}

static int ConfReaderCmdUnload(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	char *path;
	SceUID uid;
	int ret, status;

	if (argc < 1)
		return CONF_ERROR(22);
	path = ConfReaderRelToAbsPath(reader, argv[0]);
	if (!path || (uid = ConfReaderUtilsFindLoaded(reader, path)) < 0)
		return CONF_ERROR(2);
	ret = sceKernelStopUnloadModule(uid, len, args, 0, NULL, &status);
	return ret < 0 ? ret : ConfReaderUtilsForgetLoaded(reader, uid);
}

static int ConfReaderCmdUnloadOnly(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	char *path;
	SceUID uid;
	int ret;

	if (argc < 1)
		return CONF_ERROR(22);
	path = ConfReaderRelToAbsPath(reader, argv[0]);
	if (!path || (uid = ConfReaderUtilsFindLoaded(reader, path)) < 0)
		return CONF_ERROR(2);
	ret = sceKernelUnloadModule(uid, 0, NULL);
	return ret < 0 ? ret : ConfReaderUtilsForgetLoaded(reader, uid);
}

static int ConfReaderCmdSLoad(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	if (SceSysrootForDriver_930B1342() & 0x40)
		return ConfReaderCmdLoad(reader, argc, argv, len, args);
	return 0;
}

static int ConfReaderCmdToolLoad(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	char *path, *remote;
	int ret, status;

	if (!sceKernelCheckDipsw(SCE_DIPSW_ENABLE_TOOL_PHYMEMPART))
		return 0;
	if (argc < 1)
		return CONF_ERROR(22);
	path = ConfReaderRelToAbsPath(reader, argv[0]);
	if (!path)
		return CONF_ERROR(13);
	remote = ConfReaderGetRemotePath(&reader->remotePath, path);
	if (!remote || (ret = sceKernelLoadStartModule(remote, len, args, 0x20, NULL, &status)) < 0)
		ret = sceKernelLoadStartModule(path, len, args, 0x20, NULL, &status);
	if (ret >= 0) {
		SCE_DBG_PRINTF(0, 1072, "\n");
		ret = ConfReaderUtilsRememberLoaded(reader, ret, path);
	}
	return ret;
}

static int ConfReaderCmdSpawn(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	char *path, *remote;
	int pid, i;

	if (argc < 1)
		return CONF_ERROR(22);
	path = ConfReaderRelToAbsPath(reader, argv[0]);
	if (!path)
		return CONF_ERROR(13);
	remote = ConfReaderGetRemotePath(&reader->remotePath, path);
	if (!remote || (pid = sceKernelCreateProcess("main", 0x1000000, remote, NULL)) < 0) {
		for (i = 0; i < 4; i++) {
			remote = ConfReaderGetRemoteRpath(reader, path, i);
			if (remote && (pid = sceKernelCreateProcess("main", 0x1000000, remote, NULL)) >= 0)
				goto created;
		}
		remote = path;
		pid = sceKernelCreateProcess("main", 0x1000000, path, NULL);
	}
created:
	SCE_DBG_PRINTF(0, 1288, "Creating a process from %s : ", remote);
	if (pid < 0) {
		SCE_DBG_PRINTF(0, 1290, "Failed (err=0x%08x)\n", pid);
		return pid;
	}
	SCE_DBG_PRINTF(0, 1293, "OK (pid=0x%08x)\n", pid);
	SCE_DBG_PRINTF(0, 1296, "Starting a process (pid=0x%08x)\n", pid);
	sceKernelStartProcess(pid, 0, 0, NULL);
	ConfReaderUtilsRememberLoaded(reader, pid, path);
	return 0;
}

static int ConfReaderCmdSpawnWait(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	char *path, *remote;
	int pid, ret;

	if (argc < 1)
		return CONF_ERROR(22);
	path = ConfReaderRelToAbsPath(reader, argv[0]);
	if (!path)
		return CONF_ERROR(13);
	remote = ConfReaderGetRemotePath(&reader->remotePath, path);
	if (!remote || (pid = sceKernelCreateProcess("main", 0x1000000, remote, NULL)) < 0) {
		remote = path;
		pid = sceKernelCreateProcess("main", 0x1000000, path, NULL);
	}
	SCE_DBG_PRINTF(0, 1495, "Creating a process from %s : ", remote);
	if (pid < 0) {
		SCE_DBG_PRINTF(0, 1497, "Failed (err=0x%08x)\n", pid);
		return pid;
	}
	SCE_DBG_PRINTF(0, 1500, "OK (pid=0x%08x)\n", pid);
	SCE_DBG_PRINTF(0, 1503, "Starting a process (pid=0x%08x)\n", pid);
	sceKernelStartProcess(pid, 0, 0, NULL);
	ret = sceKernelWaitProcessEnd(pid, NULL, NULL);
	SCE_DBG_PRINTF(0, 1507, "Waiting for a process termination (pid=0x%08x) : ", pid);
	if (ret < 0)
		SCE_DBG_PRINTF(0, 1509, "Failed (err=0x%08x)\n", ret);
	else
		SCE_DBG_PRINTF(0, 1511, "OK\n");
	return ret;
}

static int ConfReaderCmdWait(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	char *path;
	int pid, ret;

	if (argc < 1)
		return CONF_ERROR(22);
	path = ConfReaderRelToAbsPath(reader, argv[0]);
	if (!path || (pid = ConfReaderUtilsFindLoaded(reader, path)) < 0)
		return CONF_ERROR(2);
	ret = sceKernelWaitProcessEnd(pid, NULL, NULL);
	SCE_DBG_PRINTF(0, 1540, "Waiting for a process : ");
	if (ret < 0)
		SCE_DBG_PRINTF(0, 1542, "Failed (pid=0x%08x, err=0x%08x)\n", pid, ret);
	else
		SCE_DBG_PRINTF(0, 1544, "OK (pid=0x%08x)\n", pid);
	ConfReaderUtilsForgetLoaded(reader, pid);
	return ret;
}

static int ConfReaderCmdAppSpawn(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	SceKernelProcessOpt opt;
	unsigned out[0x205];			/* SceAppMgr's app state */
	unsigned type;
	char *path, *remote, *budget;
	int i, pid;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.flags = 0x4000;			/* budgetId */
	memset(out, 0, sizeof(out));
	out[0] = sizeof(out);
	if (argc < 1)
		return CONF_ERROR(22);
	path = ConfReaderRelToAbsPath(reader, argv[0]);
	if (!path)
		return CONF_ERROR(13);
	i = strnlen(path, 0x3ff);
	strncpy(reader->absPath, path, 0x3ff);
	while (--i >= 0) {
		char c = reader->absPath[i], *name, *shell;
		if (c != '/' && c != '\\' && c != ':')
			continue;
		name = reader->absPath + i + 1;
		if (strncmp(name, "shell.self", 0x40))
			continue;
		shell = reader->shellFilename;
		if (!shell)
			continue;
		if (!strncmp(shell, "no file", 0x40))
			return CONF_ERROR(13);
		/* Sony bounds this against 0x3f, not the path buffer size. */
		strncpy(name, shell, 0x3f - i);
		break;
	}
	path = reader->absPath;
	if (!path)
		return CONF_ERROR(13);
	remote = ConfReaderGetRemotePath(&reader->remotePath, path);
	opt.budgetId = 2;
	type = 0x1000000;
	if (argc != 1) {
		budget = argv[1];
		if (!strncmp(budget, "SHELL_BUDGET_ID", 15)) {
			type = 0x4000000;
			opt.budgetId = 3;
		} else if (strncmp(budget, "GAME_BUDGET_ID", 14)) {
			SCE_DBG_PRINTF(0, 1357, "appspawn : unknown budget ID (%s)\n", budget);
			return CONF_ERROR(22);
		}
	}
	if (!remote || (pid = SceSysrootForKernel_sceKernelSysrootAppMgrSpawnProcess(
		"main", type, remote, 0, NULL, &opt, out)) < 0) {
		for (i = 0; i < 4; i++) {
			remote = ConfReaderGetRemoteRpath(reader, path, i);
			if (remote && (pid = SceSysrootForKernel_sceKernelSysrootAppMgrSpawnProcess(
				"main", type, remote, 0, NULL, &opt, out)) >= 0)
				goto spawned;
		}
		remote = path;
		pid = SceSysrootForKernel_sceKernelSysrootAppMgrSpawnProcess(
			"main", type, path, 0, NULL, &opt, out);
	}
spawned:
	SCE_DBG_PRINTF(0, 1389, "Creating a process from %s : ", remote);
	s_appProcess = pid;
	if (pid < 0) {
		SCE_DBG_PRINTF(0, 1392, "Failed (err=0x%08x)\n", pid);
		return pid;
	}
	SCE_DBG_PRINTF(0, 1396, "OK (pid=0x%08x)\n", pid);
	ConfReaderUtilsRememberLoaded(reader, pid, path);
	return 0;
}

static inline int checkFreeKernelBudgetSize(void)
{
	SceKernelPhyMemPartInfoCore info;
	int ret;

	ret = sceKernelUmountBootimageFS();
	if (!ret) {
		ret = sceKernelGetPhyMemPartInfoCore(SceSysmemForKernel_0164D817(), &info);
		if (ret >= 0) {
			SCE_PRINTF_WITH_INFO(0, 1977,
				"SceProcessmgrKernelBudget::ScePhyMemPartKD : total=0x%08x, free=0x%08x\n", info.psize, info.freeSize);
			if (info.freeSize < 0x3c0000) {
				SCE_PRINTF_WITH_INFO(0, 1979, "\n### SYSTEM ALERT : kernel budget shortage\n");
				SCE_PRINTF_WITH_INFO(0, 1980, "###   expected free size : 0x%08x [B]\n", 0x3c0000);
				SCE_PRINTF_WITH_INFO(0, 1981, "###   free size          : 0x%08x [B]\n###\n", info.freeSize);
				SCE_PRINTF_WITH_INFO(0, 1982, "### Please report to bugzilla #47324\n");
			}
		}
	}
	return ret;
}

static int ConfReaderCmdUmountBootfs(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	if (s_bootimage) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(2, 0, SCE_DBGINFO_FUNC(1430, "ConfReaderCmdUmountBootfs"),
			"ConfReaderCmdUmountBootfs() cancel mount %s\n", s_bootimage);
#endif
		s_bootimage = NULL;
		return 0;
	}
	return checkFreeKernelBudgetSize();
}

void ConfReader_setDefaultBootimage(const char *path)
{
	s_bootimage = path;
}

void ConfReader_mountDefaultBootimage(void)
{
	if (s_bootimage) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 1414, "ConfReader_mountDefaultBootimage(): mount %s\n", s_bootimage);
		sceKernelMountBootimageFS(s_bootimage);
		s_bootimage = NULL;
	}
}

static int ConfReaderCmdLoadConfig(ConfReader *reader, int argc, char **argv, unsigned len, char *args)
{
	char *path, *remote;
	int fd, ret;

	if (argc < 1) {
		SCE_DBG_PRINTF(0, 1609, "syntax error (loadconfig)\n");
		return CONF_ERROR(22);
	}
	path = ConfReaderRelToAbsPath(reader, argv[0]);
	if (!path)
		return CONF_ERROR(13);
	remote = ConfReaderGetRemotePath(&reader->remotePath, path);
	if (!remote) {
		SCE_DBG_PRINTF(0, 1629, "Loading a configuration file (%s) ...\n", path);
	} else {
		fd = sceIoOpen(remote, 1, 0);
		/* Only descriptor zero is closed: reproduce Sony's descriptor leak. */
		if (!fd) {
			sceIoClose(fd);
			SCE_DBG_PRINTF(0, 1625, "Loading a configuration file (%s) : ", remote);
		} else if (fd < 0) {
			SCE_DBG_PRINTF(0, 1629, "Loading a configuration file (%s) ...\n", path);
		}
	}
	if (++reader->depth > 4) {
		SCE_DBG_PRINTF(0, 1639, "Skipped\n");
		ret = 0;
	} else if (!remote || (ret = ConfReaderExecCommand(remote, reader->depth)) < 0) {
		ret = ConfReaderExecCommand(path, reader->depth);
	}
	reader->depth--;
	return ret;
}


/* name unknown: the commands by keyword; the bytecode stores the ID. */
static struct {
	unsigned char id;
	const char *name;
	int (*execute)(ConfReader *, int, char **, unsigned, char *);
} s_commands[CONF_CMD_COUNT] = {
	{ CONF_CMD_UNKNOWN, "", ConfReaderCmdUnknown },
	{ CONF_CMD_LOAD, "load", ConfReaderCmdLoad },
	{ CONF_CMD_UNLOAD, "unload", ConfReaderCmdUnload },
	{ CONF_CMD_LOADONLY, "loadonly", ConfReaderCmdLoadOnly },
	{ CONF_CMD_START, "start", ConfReaderCmdStart },
	{ CONF_CMD_STOP, "stop", ConfReaderCmdStop },
	{ CONF_CMD_UNLOADONLY, "unloadonly", ConfReaderCmdUnloadOnly },
	{ CONF_CMD_SPAWN, "spawn", ConfReaderCmdSpawn },
	{ CONF_CMD_SPAWNWAIT, "spawnwait", ConfReaderCmdSpawnWait },
	{ CONF_CMD_WAIT, "wait", ConfReaderCmdWait },
	{ CONF_CMD_KILL, "kill", ConfReaderCmdKill },
	{ CONF_CMD_END, "end", ConfReaderCmdEnd },
	{ CONF_CMD_SLEEP, "sleep", ConfReaderCmdSleep },
	{ CONF_CMD_ECHO, "echo", ConfReaderCmdEcho },
	{ CONF_CMD_LOADCONFIG, "loadconfig", ConfReaderCmdLoadConfig },
	{ CONF_CMD_IFMODEL, "ifmodel", ConfReaderCmdUnknown },
	{ CONF_CMD_IF, "if", ConfReaderCmdUnknown },
	{ CONF_CMD_IFNMODEL, "ifnmodel", ConfReaderCmdUnknown },
	{ CONF_CMD_ELSE, "else", ConfReaderCmdUnknown },
	{ CONF_CMD_ENDIF, "endif", ConfReaderCmdUnknown },
	{ CONF_CMD_INCLUDE, "include", ConfReaderCmdLoadConfig },
	{ CONF_CMD_CD, "cd", ConfReaderCmdCd },
	{ CONF_CMD_SETENV, "setenv", ConfReaderCmdSetEnv },
	{ CONF_CMD_SETMODFILE, "setmodfile", ConfReaderCmdSetEnv },
	{ CONF_CMD_REPEAT, "repeat", NULL },
	{ CONF_CMD_ENDREPEAT, "endrepeat", NULL },
	{ CONF_CMD_APPSPAWN, "appspawn", ConfReaderCmdAppSpawn },
	{ CONF_CMD_TLOAD, "tload", ConfReaderCmdToolLoad },
	{ CONF_CMD_UMOUNT_BOOTFS, "umount_bootfs", ConfReaderCmdUmountBootfs },
	{ CONF_CMD_SLOAD, "sload", ConfReaderCmdSLoad },
};

/* name unknown: a keyword's command ID, CONF_CMD_UNKNOWN for none. */
static __attribute__((noinline)) int ConfReaderGetCommandId(const char *name)
{
	int i;

	for (i = 0; i < CONF_CMD_COUNT; i++)
		if (!strcmp(name, s_commands[i].name))
			return s_commands[i].id;
	return CONF_CMD_UNKNOWN;
}

/* name unknown: bytecode words use network byte order. */
static __attribute__((noinline)) void ConfReaderWriteWord(unsigned char *p, unsigned value)
{
	p[3] = value;
	p[0] = value >> 24;
	p[1] = value >> 16;
	p[2] = value >> 8;
}
static __attribute__((noinline)) unsigned ConfReaderReadWord(const unsigned char *p)
{
	unsigned value = p[3];

	value |= (unsigned)p[0] << 24;
	value |= (unsigned)p[1] << 16;
	value |= (unsigned)p[2] << 8;
	return value;
}

/* name unknown: strip CR, truncate to 511 bytes and discard the rest of a line. */
static int ConfReaderReadLine(ConfReader *reader, char *buf)
{
	unsigned n = 0;
	SceSize size;
	int c;

	while (reader->cursor < reader->confSize) {
		c = (signed char)reader->confBuf[reader->cursor];
		reader->cursor++;
		if (c == -1)
			return 0;
		if (c == '\n')
			goto finished;
		if (c != '\r')
			buf[n++] = c;
		if (n >= 511) {
			size = reader->confSize;
			while (reader->cursor < size) {
				c = reader->confBuf[reader->cursor];
				reader->cursor++;
				if (c == '\n')
					break;
			}
			goto finished;
		}
	}
	if (!n)
		return 0;
finished:
	buf[n] = 0;
	return n + 1;
}

static int ConfReaderReadTextFmt(ConfReader *reader)
{
	SceOff size;
	int ret;

	size = sceIoLseek(reader->fd, 0, 2);
	if (size < 1 || (int)sceIoLseek(reader->fd, 0, 0) != 0)
		return CONF_ERROR(2);
	ret = ConfReaderUtilsAllocConfBuf(reader, (unsigned)size);
	if (ret < 0) {
		SCE_DBG_PRINTF(0, 535, "ConfReaderReadTextFmt : alloc confbuf failed\n");
		return CONF_ERROR(12);
	}
	ret = sceIoRead(reader->fd, reader->confBuf, (unsigned)size);
	if (ret != (int)size)
		return CONF_ERROR(2);
	return ret;
}

static __attribute__((always_inline)) inline int ConfReaderReadPrxFmt(ConfReader *reader, const char *path)
{
	int ret, status;

	sceIoClose(reader->fd);
	reader->confModule = sceKernelLoadModule(path, 0x100, NULL);
	if (reader->confModule < 0) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO_FUNC(501, "ConfReaderReadPrxFmt"),
			"ConfReaderReadPrxFmt : failed (path=%s, ret=0x%08x)\n", path, reader->confModule);
#endif
		return reader->confModule;
	}
	ret = sceKernelStartModule(reader->confModule, 0, NULL, 0, NULL, &status);
	if (ret < 0) {
		sceKernelUnloadModule(reader->confModule, 0, NULL);
		return ret;
	}
	reader->confBuf = SceKernelPsp2Config_A6483A7C;
	reader->confSize = SceKernelPsp2Config_C1BA20DA - SceKernelPsp2Config_A6483A7C;
	return 0;
}

static __attribute__((always_inline)) inline int ConfReaderOpen(ConfReader *reader, const char *path, int *opened)
{
	int ret, retry = 7;
	unsigned magic;

	for (;;) {
		ret = sceIoOpen(path, 1, 0);
		if (ret >= 0)
			break;
		if (ret != (int)0x8009000aU) {
#if FW_DEBUG
			SceDebugForDriver_D9703808(1, 15, SCE_DBGINFO_FUNC(279, "ConfReaderOpen"),
				"ConfReaderOpen : not found %s (0x%08X)\n", path, ret);
#endif
			return CONF_ERROR(2);
		}
		if (!--retry) {
			s_noRemoteRpath = s_noRemoteModule = 1;
			return CONF_ERROR(2);
		}
		sceKernelDelayThread(200000);
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO_FUNC(294, "ConfReaderOpen"), ".");
#endif
	}
	*opened = 1;
	reader->fd = ret;
	reader->loaded = NULL;
	reader->execTop = -1;
	reader->repeatTop = -1;
	ret = sceIoRead(reader->fd, &magic, 4);
	if (ret != 4) {
		ret = CONF_ERROR(9);
		goto close;
	}
	if (magic == 0x00454353) {
		ret = ConfReaderReadPrxFmt(reader, path);
		if (ret >= 0)
			return ret;
		SceDebugForDriver_391B5B74(SCE_DBGINFO_FUNC(315, "ConfReaderOpen"), __builtin_return_address(0));
		goto failed;
	}
	if (!SceQafMgrForDriver_883E9465())
		goto failed;
	ret = ConfReaderReadTextFmt(reader);
	if (ret >= 0)
		goto close;
	SceDebugForDriver_391B5B74(SCE_DBGINFO_FUNC(323, "ConfReaderOpen"), __builtin_return_address(0));
	/* The panic returns here in Sony's build: the reader then reports not found. */
failed:
	ret = CONF_ERROR(2);
close:
	sceIoClose(reader->fd);
	return ret;
}

static __attribute__((always_inline)) inline void ConfReaderReadRpath(ConfReader *reader)
{
	ConfReader remote;
	char buf[512], *argv[8];
	int fd, ret, count = 0, argc, size;

	if (!SceQafMgrForDriver_B9770A13() && !SceQafMgrForDriver_082A4FC2())
		return;
	fd = sceIoOpen("host0:psp2config.rpath", 1, 0);
	if (fd < 0)
		return;
	ConfReaderInit(&remote, 0);
	remote.fd = fd;
	remote.loaded = NULL;
	remote.execTop = -1;
	remote.repeatTop = -1;
	ret = ConfReaderReadTextFmt(&remote);
	if (ret >= 0) {
		ret = ConfReaderUtilsAllocCmdBuf(&remote, 0x8000);
		if (ret < 0) {
#if FW_DEBUG
			SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO_FUNC(398, "ConfReaderReadRpath"),
				"ConfReaderReadRpath : alloc cmdbuf failed\n");
#endif
		} else {
			while ((size = ConfReaderReadLine(&remote, buf)) && count < 4) {
				argc = ConfReaderUtilsTokenize(argv, 8, buf, size);
				if (argc < 1 || argv[0][0] == '#')
					continue;
				if (!strncmp(argv[0], "set_shell_filename", 0xff) && !reader->shellFilename) {
					reader->shellFilename = SceSysmemForKernel_85571907(0x40);
#if FW_DEBUG
					SceDebugForDriver_FD753E7A(0, reader->shellFilename != NULL,
						SCE_DBGINFO_FUNC(417, "ConfReaderReadRpath"), __builtin_return_address(0));
#endif
					reader->shellFilename[63] = 0;
					strncpy(reader->shellFilename, argc == 1 ? "no file" : argv[1], 63);
				} else {
					size = strnlen(argv[0], 0x100);
					reader->rpath[count] = SceSysmemForKernel_85571907(size + 1);
					reader->rpath[count][size] = 0;
					strncpy(reader->rpath[count++], argv[0], size);
				}
			}
		}
	}
	sceIoClose(remote.fd);
	ConfReaderClose(&remote);
}

static __attribute__((always_inline)) inline int ConfReaderParseCmd(ConfReader *reader)
{
	char line[512], *argv[8];
	int size, argc, id, first, ret, flags, i, lineno = 0;
	int active = 1, condition, repeat;
	unsigned offset = 0, repeatOffset, payload, n;
	ConfCommand *cmd;

	ret = ConfReaderUtilsAllocCmdBuf(reader, 0x8000);
	if (ret < 0) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO_FUNC(564, "ConfReaderParseCmd"),
			"ConfReaderReadParseCmd :alloc cmdbuf failed\n");
#endif
		return CONF_ERROR(12);
	}
	while ((size = ConfReaderReadLine(reader, line))) {
		repeat = 0;
		repeatOffset = 0;
		argc = ConfReaderUtilsTokenize(argv, 8, line, size);
		lineno++;
		if (argc < 1)
			continue;
		n = strnlen(argv[0], 0x100);
		flags = 0;
		if (!n || argv[0][0] == '#') {
			flags = CONF_FLAG_VALID | CONF_FLAG_SKIP;
		} else {
			for (i = 0; i < (int)n; i++) {
				if (argv[0][i] == '&')
					flags |= CONF_FLAG_VALID | CONF_FLAG_ASYNC;
				else if (argv[0][i] == '-')
					flags |= CONF_FLAG_VALID | CONF_FLAG_IGNORE_ERROR;
				else {
					flags = CONF_FLAG_VALID;
					break;
				}
			}
		}
		if (flags & CONF_FLAG_SKIP)
			continue;
		first = flags == CONF_FLAG_VALID ? 1 : 2;
		if (first == 2 && argc == 1)
			continue;
		id = ConfReaderGetCommandId(argv[first - 1]);
		if (!id) {
#if FW_DEBUG
			SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO_FUNC(696, "ConfReaderParseCmd"),
				"unknown command (%d)\n", 0);
#endif
			ret = 0;
			goto syntax;
		}
		if (id == CONF_CMD_IFMODEL || id == CONF_CMD_IF || id == CONF_CMD_IFNMODEL) {
			if (flags != CONF_FLAG_VALID)
				goto invalid;
			ret = ConfReaderUtilsEvalCond(reader, argc, argv, 1, &condition);
			if (ret < 0)
				goto syntax;
			if (id == CONF_CMD_IFNMODEL)
				condition = !condition;
			ret = ConfReaderUtilsPushExecFlags(reader, active, condition);
			if (ret < 0)
				goto syntax;
			active = active && condition;
			continue;
		}
		if (id == CONF_CMD_ELSE || id == CONF_CMD_ENDIF) {
			if (flags != CONF_FLAG_VALID)
				goto invalid;
			ret = ConfReaderUtilsPopExecFlags(reader, &active, &condition);
			if (ret < 0)
				goto syntax;
			if (id == CONF_CMD_ELSE) {
				condition = !condition;
				ConfReaderUtilsPushExecFlags(reader, active, condition);
				active = active && condition;
			}
			continue;
		}
		if (id == CONF_CMD_REPEAT) {
			if (flags != CONF_FLAG_VALID && argc == 1)
				goto invalid;
			repeat = ConfReaderUtilsDecimal(argv[1]);
			if (repeat <= 0) {
				ret = repeat;
				goto syntax;
			}
			ret = ConfReaderUtilsPushRepeatInfo(reader, repeat - 1, offset);
			if (ret < 0)
				goto syntax;
			continue;
		}
		if (id == CONF_CMD_ENDREPEAT) {
			if (flags != CONF_FLAG_VALID)
				goto invalid;
			ret = ConfReaderUtilsPopRepeatInfo(reader, &repeat, &repeatOffset);
			if (ret < 0)
				goto syntax;
		}
		payload = 0;
		for (i = first; i < argc; i++) {
			payload += strnlen(argv[i], 0x100) + 1;
			if (payload >= 512) {
#if FW_DEBUG
				SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO_FUNC(723, "ConfReaderParseCmd"),
					"no space for args (%d)\n", id);
#endif
				return CONF_ERROR(11);
			}
		}
		if (offset + payload + sizeof(*cmd) > reader->cmdSize) {
#if FW_DEBUG
			SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO_FUNC(736, "ConfReaderParseCmd"),
				"ConfReaderReadParseCmd : no space\n");
#endif
			return CONF_ERROR(12);
		}
		cmd = (ConfCommand *)(reader->cmdBuf + offset);
		cmd->flags = flags;
		cmd->id = id;
		cmd->length = payload;
		ConfReaderWriteWord(cmd->active, active);
		ConfReaderWriteWord(cmd->repeat, repeat);
		ConfReaderWriteWord(cmd->repeatOffset, repeatOffset);
		offset += sizeof(*cmd);
		for (i = first; i < argc; i++) {
			n = strnlen(argv[i], 0x100);
			memcpy(reader->cmdBuf + offset, argv[i], n);
			reader->cmdBuf[offset + n] = ' ';
			offset += n + 1;
		}
	}
	reader->cmdSize = offset;
	return 0;
invalid:
	ret = CONF_ERROR(22);
syntax:
#if FW_DEBUG
	SceDebugForDriver_D9703808(1, 0, SCE_DBGINFO_FUNC(777, "ConfReaderParseCmd"),
		"syntax error at line %d\n", lineno);
#endif
	/* Unknown commands and nonpositive repeats report an error but return zero;
	 * the original then executes the still-full allocation, not the prefix. */
	return ret;
}

int ConfReaderExecCommand(const char *path, int depth)
{
	ConfReader reader;
	char *argv[8], *args;
	unsigned offset = 0, len, repeatOffset;
	ConfCommand *cmd;
	int ret, fd, i, argc, id, flags, active, repeat, spawned = 0, opened = 0;
	const char *name __attribute__((unused));

	ConfReaderInit(&reader, depth);
	ret = ConfReaderOpen(&reader, path, &opened);
	/* Failure to open returns before remote-path setup or cleanup. */
	if (!opened)
		return depth == 0 ? CONF_NOT_FOUND : ret;
	ConfReaderReadRpath(&reader);
	if (!s_noRemoteRpath) {
		fd = sceIoOpen("host0:module", 1, 0);
		if (fd == CONF_ERROR(21) || fd == CONF_ERROR(134))
			sceIoClose(fd);
		else
			s_noRemoteModule = fd < 1;
	}
	if (ret < 0)
		return depth == 0 ? CONF_NOT_FOUND : ret;
	ret = ConfReaderParseCmd(&reader);
	if (ret < 0)
		goto finished;
	i = strnlen(path, 0x3ff);
	strncpy(reader.cwd, path, 0x3ff);
	for (fd = i - 1; fd >= 0; fd--)
		if (reader.cwd[fd] == '/' || reader.cwd[fd] == '\\') {
			reader.cwd[fd + 1] = 0;
			goto execute;
		}
	while (--i >= 0)
		if (reader.cwd[i] == ':') {
			reader.cwd[i + 1] = 0;
			break;
		}
execute:
	ret = 0;
	while (offset < reader.cmdSize) {
		if (offset + sizeof(*cmd) > reader.cmdSize) {
			ret = CONF_ERROR(47);
			goto finished;
		}
		cmd = (ConfCommand *)(reader.cmdBuf + offset);
		flags = cmd->flags;
		id = cmd->id;
		len = cmd->length;
		active = ConfReaderReadWord(cmd->active);
		repeat = ConfReaderReadWord(cmd->repeat);
		repeatOffset = ConfReaderReadWord(cmd->repeatOffset);
		offset += sizeof(*cmd);
		if (offset + len > reader.cmdSize) {
			ret = CONF_ERROR(47);
			goto finished;
		}
		args = (char *)reader.cmdBuf + offset;
		argc = len ? ConfReaderUtilsTokenize(argv, 8, args, len) : 0;
		offset += len;
		if (id >= CONF_CMD_COUNT) {
			SCE_DBG_PRINTF(0, 848, " ConfReaderExecCommand : Unknown command (%d)\n", id);
			ret = CONF_ERROR(47);
			goto finished;
		}
		ret = 0;
		if (!ConfReaderUtilsIsActive(active))
			goto progress;
		if (flags & CONF_FLAG_ASYNC) {
			SCE_DBG_PRINTF(0, 857, "Not supported '&' option\n");
			goto progress;
		}
		if (id == CONF_CMD_END)
			goto finished;
		if (id == CONF_CMD_ENDREPEAT) {
			if (repeat > 0) {
				ConfReaderWriteWord(cmd->repeat, repeat - 1);
				offset = repeatOffset;
			}
			goto progress;
		}
		if ((id == CONF_CMD_SPAWN || id == CONF_CMD_SPAWNWAIT || id == CONF_CMD_APPSPAWN) &&
		    !spawned) {
			SceSysrootForDriver_7C2C10E2(7, 0);
			spawned = 1;
		}
		name = s_commands[id].name;
		SCE_DBG_PRINTF_LEVEL(2, 0, 879, "ConfReaderExecCommand : %s command start\n", name);
		if (id != CONF_CMD_UMOUNT_BOOTFS && id != CONF_CMD_SETENV && id != CONF_CMD_ECHO)
			ConfReader_mountDefaultBootimage();
		ret = s_commands[id].execute(&reader, argc, argv, len, args);
		if (ret < 0) {
			SCE_DBG_PRINTF(0, 896, "ConfReaderExecCommand : %s command failed, err=0x%08x", name, ret);
			if (argc > 0)
				SCE_DBG_PRINTF(0, 898, ", arg0=%s", argv[0]);
			SCE_DBG_PRINTF(0, 900, "\n");
			if (!(flags & CONF_FLAG_IGNORE_ERROR)) {
				if (!spawned)
					SceSysrootForDriver_7C2C10E2(7, 1);
				goto finished;
			}
		}
progress:
		SceSysrootForKernel_47724459(1);
	}
	if (!spawned)
		SceSysrootForDriver_7C2C10E2(7, 0);
finished:
	ConfReaderClose(&reader);
	return ret;
}
