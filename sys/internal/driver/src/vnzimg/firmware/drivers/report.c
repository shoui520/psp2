/*
 * The crash reporter, the driver at +0xe000 of the image: it hooks the NMI,
 * reserved instruction and zero divide exceptions (drivers/exception.c) and
 * records where each core took one in its report slot, for the ARM side to
 * read. The debug exception records only the debug register.
 */
#include "exception.h"

/* a core's report slot (0x20 bytes, at VK_REPORTS + core * 0x20) */
typedef struct VkReport {
	uint32_t kind;
	uint32_t pc;
	uint32_t lp;
	uint32_t sp;
	uint32_t debug;
	uint32_t reserved14[3];
} VkReport;

#define VK_REPORTS		0xf1840100
#define VK_REPORT_STACK		0x4000		/* the stack written back below sp */

static void reportException(uint32_t kind)
{
	volatile VkReport *report;
	VkExceptionState *state;
	uint32_t core;

	core = vnzCurrentCoreId() - VNZ_FIRST_CORE_ID;
	kind = vkExceptionKind();
	state = vkExceptionState();
	report = (volatile VkReport *)VK_REPORTS + core;

	if (kind == VK_EXCEPTION_DEBUG) {
		report->debug = state->control[VNZ_CR_DBG];
		return;
	}
	report->kind = kind;
	if (kind == VK_EXCEPTION_NMI)
		report->pc = state->control[VNZ_CR_NPC];
	else
		report->pc = state->control[VNZ_CR_EPC];
	report->lp = state->control[VNZ_CR_LP];
	report->sp = state->registers[15];
	vnzCacheFlush(report->sp, VK_REPORT_STACK);
	mep_syncm();
	vnzL2Writeback(report->sp, VK_REPORT_STACK);
}

static void moduleResume(uint32_t primary, uint32_t resumed)
{
	vkExceptionHook(VK_EXCEPTION_NMI, reportException, 0);
	vkExceptionHook(VK_EXCEPTION_RESERVED, reportException, 0);
	vkExceptionHook(VK_EXCEPTION_ZERO_DIVIDE, reportException, 0);
}

static void moduleSuspend(void)
{
	vkExceptionHook(VK_EXCEPTION_ZERO_DIVIDE, 0, 0);
	vkExceptionHook(VK_EXCEPTION_RESERVED, 0, 0);
	vkExceptionHook(VK_EXCEPTION_NMI, 0, 0);
}

const VkModule g_vkReportModule VK_SECTION(".report.module") = {
	.magic = VK_MODULE_DRIVER,
	.self = VNZ_PHYSICAL_OF(0x80e000),
	.size = 0x24c,
	.attributes = VK_MODULE_ATTRIBUTES,
	.version = VK_MODULE_VERSION,
	.resume = moduleResume,
	.suspend = moduleSuspend,
};
