/* Allegrex IPL startup and boot-file services. */
#include "ipl.h"
#include "ipl_data.h"
#include "../psp_ipl.h"

extern int boot_main(void *memBase, SceSize memSize, u32 bootArg,
		     u32 bootMode, u32 bootFlags, u32 resumeArg, u32 model);

/* These registers belong to the Allegrex memory controller and sysreg block. */
#define DDR_FLUSH_PATTERN	0xBC000000
#define DDR_FLUSH_REQUEST	0xBC000030
#define DDR_FLUSH_CONTROL	0xBC000044
#define SYSREG_MEMORY_CONFIG	0xBC100040
#define SYSREG_BUS_CLOCK		0xBC100050
#define IPL_RESUME_ARGUMENT	0xA83FFF00

int main(u32 arg0, u32 arg1, u32 arg2, u32 bootMode, u32 bootFlags)
{
	u32 previousGp;
	u32 resumeArg = 0;
#if FW_VERSION >= 0x03650000
	u32 bootGp = 0x8860D440;

	resumeArg = HW(IPL_RESUME_ARGUMENT);
	HW(IPL_RESUME_ARGUMENT) = 0;
#else
	u32 bootGp = 0x8860D420;
#endif

	bootFlags |= 0x01000000;
	/* GP is fixed by the EABI compiler; -G0 keeps C accesses independent
	 * of it. Kernel scratch registers and CP0 also have no C representation.
	 */
	__asm__ __volatile__(
		"move %0, $gp\n\t"
		"move $gp, %1\n\t"
		"mtc0 %2, $12\n\t"
		"move $k0, $zero\n\t"
		"move $k1, $zero\n\t"
		"mtc0 $zero, $13"
		: "=&r"(previousGp)
		: "r"(bootGp), "r"(0x60000000)
		: "$26", "$27", "memory");

	HW(DDR_FLUSH_PATTERN) = 0xCCCCCCCC;
	HW(DDR_FLUSH_PATTERN + 4) = 0xCCCCCCCC;
	HW(DDR_FLUSH_PATTERN + 8) = 0xCCCCCCCC;
	HW(DDR_FLUSH_PATTERN + 12) = 0xCCCCCCCC;
	HW(DDR_FLUSH_REQUEST) = 0;
	HW(DDR_FLUSH_REQUEST + 4) = 0;
	HW(DDR_FLUSH_REQUEST + 8) = 0;
	HW(DDR_FLUSH_REQUEST + 12) = 0;
	HW(DDR_FLUSH_REQUEST + 16) = 0;
	HW(DDR_FLUSH_CONTROL) &= ~0x60;
	HW(SYSREG_MEMORY_CONFIG) |= 2;
	sync();
	memset(&g_kirkBuffer, 0, (u32)g_bssEnd - (u32)&g_kirkBuffer);
	bootStartupHook();
	HW(SYSREG_BUS_CLOCK) |= 0x4080;
	boot_main((void *)0x88000000, 0x02000000, 0, bootMode,
		  bootFlags, resumeArg, 0);
	__asm__ __volatile__("move $gp, %0" :: "r"(previousGp)
			     : "memory");
	return 0;
}

SceCompatBootFile *findBootFile(const char *name)
{
	SceCompatBootFile *file = (SceCompatBootFile *)0x8B000000;

	while (file->buffer != NULL) {
		if (strcmp(file->name, name) == 0)
			return file;
		file++;
	}
	return NULL;
}

int openBootFile(SceCompatBootFile *file, int optional)
{
	SceCompatBootFile *entry;
	int result = SCE_ERROR_ERROR;

	if (file->name != NULL) {
		entry = findBootFile(file->name);
		result = optional == 1 ? 0 : SCE_ERROR_ERROR;
		if (entry != NULL) {
			SceSize size = entry->size;
			void *buffer = entry->buffer;

			file->size = size;
			file->buffer = buffer;
			result = 0;
		}
	}
	return result;
}
