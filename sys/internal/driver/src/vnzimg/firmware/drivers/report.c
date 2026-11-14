/*
 * The crash reporter, the driver at +0xe000 of the image: it hooks the NMI,
 * reserved instruction and zero divide exceptions (drivers/exception.c) and
 * records where each core took one in its report slot, for the ARM side to
 * read. The debug exception records only the debug register.
 */
#include "exception.h"

/* a core's report slot (0x20 bytes, at VNZ_KERNEL_REPORTS + core * 0x20) */
typedef struct VnzKernelReport {
	uint32_t kind;
	uint32_t pc;
	uint32_t lp;
	uint32_t sp;
	uint32_t debug;
	uint32_t reserved14[3];
} VnzKernelReport;

#define VNZ_KERNEL_REPORTS		0xf1840100
#define VNZ_KERNEL_REPORT_STACK		0x4000		/* the stack written back below sp */

static void reportException(uint32_t kind)
{
	volatile VnzKernelReport *report;
	VnzKernelExceptionState *state;
	uint32_t core;

	core = vnzCurrentCoreId() - VNZ_FIRST_CORE_ID;
	kind = vnzKernelExceptionKind();
	state = vnzKernelExceptionState();
	report = (volatile VnzKernelReport *)VNZ_KERNEL_REPORTS + core;

	if (kind == VNZ_KERNEL_EXCEPTION_DEBUG) {
		report->debug = state->control[VNZ_CR_DBG];
		return;
	}
	report->kind = kind;
	if (kind == VNZ_KERNEL_EXCEPTION_NMI)
		report->pc = state->control[VNZ_CR_NPC];
	else
		report->pc = state->control[VNZ_CR_EPC];
	report->lp = state->control[VNZ_CR_LP];
	report->sp = state->registers[15];
	vnzCacheFlush(report->sp, VNZ_KERNEL_REPORT_STACK);
	mep_syncm();
	vnzL2Writeback(report->sp, VNZ_KERNEL_REPORT_STACK);
}

static void moduleResume(uint32_t primary, uint32_t resumed)
{
	vnzKernelExceptionHook(VNZ_KERNEL_EXCEPTION_NMI, reportException, 0);
	vnzKernelExceptionHook(VNZ_KERNEL_EXCEPTION_RESERVED, reportException, 0);
	vnzKernelExceptionHook(VNZ_KERNEL_EXCEPTION_ZERO_DIVIDE, reportException, 0);
}

static void moduleSuspend(void)
{
	vnzKernelExceptionHook(VNZ_KERNEL_EXCEPTION_ZERO_DIVIDE, 0, 0);
	vnzKernelExceptionHook(VNZ_KERNEL_EXCEPTION_RESERVED, 0, 0);
	vnzKernelExceptionHook(VNZ_KERNEL_EXCEPTION_NMI, 0, 0);
}

extern const uint8_t vnzKernelReportSize[];	/* the link's (image.ld) */

const VnzKernelModule g_vnzKernelReportModule VNZ_KERNEL_SECTION(".report.module") = {
	.magic = VNZ_KERNEL_MODULE_DRIVER,
	.self = VNZ_PHYSICAL_OF(0x80e000),
	.size = (uint32_t)vnzKernelReportSize,
	.attributes = VNZ_KERNEL_MODULE_ATTRIBUTES,
	.version = VNZ_KERNEL_MODULE_VERSION,
	.resume = moduleResume,
	.suspend = moduleSuspend,
};
