/* Buffers, loaded-object records, and parser stacks for the boot reader. */
#include "sysstatemgr.h"
#include <sysmem/uid_guid.h>

int ConfReaderUtilsAllocConfBuf(ConfReader *reader, SceSize size)
{
	reader->confBlock = sceKernelAllocPartitionMemBlock(SCE_KERNEL_PARTITION_ROOT_ID,
		"SceSSMConfBuf", CONF_MEMTYPE, (size + 0xfff) & ~0xfff, NULL);
	if (reader->confBlock < 0) {
		SCE_DBG_ASSERT_FAIL(168);
		return CONF_ERROR(12);
	}
	sceKernelGetMemBlockBase(reader->confBlock, (void **)&reader->confBuf);
	reader->confSize = size;
	reader->cursor = 0;
	return 0;
}

void ConfReaderUtilsFreeConfBuf(ConfReader *reader) /* name unknown */
{
	if (reader->confBlock >= 0) {
		sceKernelFreeMemBlock(reader->confBlock);
		reader->confBlock = -1;
		reader->confSize = 0;
		reader->confBuf = NULL;
		reader->cursor = 0;
	}
}

int ConfReaderUtilsAllocCmdBuf(ConfReader *reader, SceSize size)
{
	reader->cmdBlock = sceKernelAllocPartitionMemBlock(SCE_KERNEL_PARTITION_ROOT_ID,
		"SceSSMCommandBuffer", CONF_MEMTYPE, (size + 0xfff) & ~0xfff, NULL);
	if (reader->cmdBlock < 0) {
		SCE_DBG_ASSERT_FAIL(208);
		return CONF_ERROR(12);
	}
	sceKernelGetMemBlockBase(reader->cmdBlock, (void **)&reader->cmdBuf);
	reader->cmdSize = size;
	return 0;
}

void ConfReaderUtilsFreeCmdBuf(ConfReader *reader) /* name unknown */
{
	if (reader->cmdBlock >= 0) {
		sceKernelFreeMemBlock(reader->cmdBlock);
		reader->cmdBlock = -1;
		reader->cmdSize = 0;
		reader->cmdBuf = NULL;
	}
}

int ConfReaderUtilsRememberLoaded(ConfReader *reader, SceUID uid, const char *path) /* name unknown */
{
	ConfLoaded *loaded = sceKernelAllocHeapMemory(SCE_KERNEL_HEAP_ID, sizeof(*loaded));
	unsigned size;
	char *copy;

	if (loaded) {
		size = strnlen(path, 0x100) + 1;
		copy = sceKernelAllocHeapMemory(SCE_KERNEL_HEAP_ID, size);
		loaded->path = copy;
		if (copy) {
			copy[0] = 0;
			strncpy(copy, path, size);
			loaded->next = reader->loaded;
			loaded->uid = uid;
			reader->loaded = loaded;
			return 0;
		}
		sceKernelFreeHeapMemory(SCE_KERNEL_HEAP_ID, loaded);
	}
	return CONF_ERROR(12);
}

SceUID ConfReaderUtilsFindLoaded(ConfReader *reader, const char *path) /* name unknown */
{
	ConfLoaded *loaded;

	for (loaded = reader->loaded; loaded; loaded = loaded->next)
		if (!strncmp(loaded->path, path, 0x100))
			return loaded->uid;
	return -1;
}

int ConfReaderUtilsForgetLoaded(ConfReader *reader, SceUID uid) /* name unknown */
{
	ConfLoaded *loaded = reader->loaded, *previous = loaded, *next;
	char *path;

	while (loaded) {
		if (loaded->uid == uid) {
			next = loaded->next;
			path = loaded->path;
			if (loaded == reader->loaded)
				reader->loaded = next;
			else
				previous->next = next;
			sceKernelFreeHeapMemory(SCE_KERNEL_HEAP_ID, path);
			sceKernelFreeHeapMemory(SCE_KERNEL_HEAP_ID, loaded);
			return 0;
		}
		previous = loaded;
		loaded = loaded->next;
	}
	return CONF_ERROR(2);
}

void ConfReaderUtilsFreeLoaded(ConfReader *reader) /* name unknown */
{
	ConfLoaded *loaded = reader->loaded, *next;

	while (loaded) {
		next = loaded->next;
		sceKernelFreeHeapMemory(SCE_KERNEL_HEAP_ID, loaded->path);
		sceKernelFreeHeapMemory(SCE_KERNEL_HEAP_ID, loaded);
		loaded = next;
	}
}

/* name unknown: decimal repeat count, bounded to eight digits. */
int ConfReaderUtilsDecimal(const char *text)
{
	unsigned len = strnlen(text, 9), i;
	int result = 0;

	if (len >= 9)
		return 0;
	for (i = 0; i < len; i++) {
		if ((unsigned)((unsigned char)text[i] - '0') > 9)
			return 0;
		result = result * 10 + (signed char)text[i] - '0';
	}
	return result;
}

/* name unknown: whitespace is tab, form feed, or space (not C isspace). */
static inline int ConfReaderUtilsIsSpace(char c)
{
	switch (c) {
	case '\t':
	case '\f':
	case ' ':
		return 1;
	default:
		return 0;
	}
}

int ConfReaderUtilsTokenize(char **argv, int limit, char *buf, unsigned size)
{
	unsigned i = 0, j;
	int argc = 0;

	if (size < 2)
		return 0;
	for (;;) {
		if (ConfReaderUtilsIsSpace(buf[i]) && i < size - 1) {
			i++;
			continue;
		}
		if (i >= size - 1)
			return argc;
		argv[argc++] = buf + i;
		j = i + 1;
		if (argc >= limit || j >= size)
			break;
		while (!ConfReaderUtilsIsSpace(buf[j]) && j < size - 1)
			j++;
		if (j >= size)
			break;
		buf[j] = 0;
		i = j + 1;
		if (i >= size)
			break;
	}
	return argc;
}

/* The conditions of if/ifmodel/ifnmodel; Sony names them ConfReaderUtilsIs<Mode>
 * (0.931: ConfReaderUtilsIsSafeMode, ConfReaderUtilsIsUpdateMode). */
static int ConfAlways(void)
{
	return 1;
}

static int ConfNever(void)
{
	return 0;
}

int ConfReaderUtilsIsManufacturingMode(void)
{
	return SceSysrootForKernel_55392965();
}

static int ConfReaderUtilsIsExternalBootMode(void)
{
	return SceSysrootForKernel_89D19090();
}

static int ConfReaderUtilsIsUpdateMode(void)
{
	return SceSysrootForKernel_B0E1FC67();
}

static int ConfReaderUtilsIsUsbEnumWakeup(void)
{
	return SceSysrootForKernel_79C9AE10();
}

static int ConfReaderUtilsIsBsodReboot(void)
{
	return SceSysrootForKernel_4373AC96();
}

static int ConfReaderUtilsIsUd0Exist(void)
{
	SceIoStat stat;
	return sceIoGetstat("ud0:", &stat) == 0;
}

static int ConfReaderUtilsIsSafeMode(void)
{
	return SceSysrootForKernel_sceKernelSysrootIsSafeMode();
}

static int ConfReaderUtilsIsDevelopmentMode(void)
{
	return SceSblACMgrForKernel_BBA13D9C() != 0;
}

static int ConfReaderUtilsIsAuCodecIcConexant(void)
{
	unsigned char flags[16];
	SceSysrootForDriver_46E72428(flags);
	return (flags[0] >> 6) & 1;
}

static struct {
	int (*evaluate)(void);
	const char *name;
} s_conditions[] = {
	{ ConfAlways, "1" },
	{ ConfNever, "0" },
	{ ConfReaderUtilsIsManufacturingMode, "MANUFACTURING_MODE" },
	{ ConfReaderUtilsIsExternalBootMode, "EXTERNAL_BOOT_MODE" },
	{ ConfReaderUtilsIsUpdateMode, "UPDATE_MODE" },
	{ ConfReaderUtilsIsUsbEnumWakeup, "USB_ENUM_WAKEUP" },
	{ ConfReaderUtilsIsBsodReboot, "BSOD_REBOOT" },
	{ ConfReaderUtilsIsUd0Exist, "UD0_EXIST" },
	{ ConfReaderUtilsIsSafeMode, "SAFE_MODE" },
	{ ConfReaderUtilsIsDevelopmentMode, "DEVELOPMENT_MODE" },
	{ ConfReaderUtilsIsAuCodecIcConexant, "AU_CODEC_IC_CONEXANT" },
	{ NULL, NULL },
};

int ConfReaderUtilsEvalCond(ConfReader *reader, int argc, char **argv, int first, int *result)
{
	int i, k, found, value = 1;

	if (first < argc) {
		for (i = first; i < argc; i++) {
			found = 0;
			for (k = 0; s_conditions[k].name; k++) {
				if (!strcmp(argv[i], s_conditions[k].name)) {
					if (value)
						value = s_conditions[k].evaluate() != 0;
					found = 1;
				}
			}
			if (!found) {
				SCE_DBG_PRINTF(0, 334, "ConfReaderUtilsGetModelType : unknown model type\n");
				return CONF_ERROR(22);
			}
		}
		*result = value;
		return 0;
	}
	return CONF_ERROR(22);
}

int ConfReaderUtilsPopExecFlags(ConfReader *reader, int *parent, int *condition)
{
	if (reader->execTop < 0) {
		SCE_DBG_PRINTF(0, 348, "ConfReaderUtilsPopExecFlags : too nest\n");
		return CONF_ERROR(11);
	}
	*parent = reader->exec[reader->execTop].parent;
	*condition = reader->exec[reader->execTop].condition;
	reader->execTop--;
	return 0;
}

int ConfReaderUtilsPushExecFlags(ConfReader *reader, int parent, int condition)
{
	if (++reader->execTop > 31) {
		SCE_DBG_PRINTF(0, 365, "ConfReaderUtilsPushExecFlags : too nest\n");
		return CONF_ERROR(11);
	}
	reader->exec[reader->execTop].parent = parent;
	reader->exec[reader->execTop].condition = condition;
	return 0;
}

int ConfReaderUtilsPopRepeatInfo(ConfReader *reader, int *count, unsigned *offset)
{
	if (reader->repeatTop < 0) {
		SCE_DBG_PRINTF(0, 404, "ConfReaderUtilsPopRepeatInfo : too nest\n");
		return CONF_ERROR(11);
	}
	*count = reader->repeat[reader->repeatTop].count;
	*offset = reader->repeat[reader->repeatTop].offset;
	reader->repeatTop--;
	return 0;
}

int ConfReaderUtilsPushRepeatInfo(ConfReader *reader, int count, unsigned offset)
{
	if (++reader->repeatTop > 3) {
		SCE_DBG_PRINTF(0, 420, "ConfReaderUtilsPushRepeatInfo : too nest\n");
		return CONF_ERROR(11);
	}
	reader->repeat[reader->repeatTop].offset = offset;
	reader->repeat[reader->repeatTop].count = count;
	return 0;
}

/* name unknown: whether a command executes; Sony's build reduces it to its argument. */
int ConfReaderUtilsIsActive(int active)
{
	return active;
}
