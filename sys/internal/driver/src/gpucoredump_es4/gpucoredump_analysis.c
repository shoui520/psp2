/*
 * SceGpuEs4CoreDump: the analysis of the captured registers, printed to
 * the console and into the dump (Sony's file name unknown).
 */

#include <scetypes.h>
#include <kernel/types.h>
#include "gpucoredump.h"
#include <gpucoredump_es4/gpucoredump_es4_imports.h>

/* name unknown: the area of the GPU address space an address is in */
const char *gpuCoreDumpRangeName(SceUInt32 address)
{
	const GpuCoreDumpRange *range;

	for (range = gpuCoreDumpRanges; range->name != NULL; range++)
		if (address >= range->base && address < (SceUInt64)range->base + range->size)
			return range->name;
	return "Unknown Memory Range";
}

void gpuCoreDumpPrintString(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value)
{
	if (analysis->log != NULL)
		gpuCoreDumpLogPrintf(analysis->log, "%s: %s\n", analysis->prefix, name);
}

/* the depth buffer width: in samples, and pixels by the MSAA mode */
void gpuCoreDumpPrintSamples(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value)
{
	SceUInt32 samples = (value + 1) * 32;
	SceUInt32 pixels = samples / ((analysis->coreFlags & 0x40) ? 2 : 1);

	if (analysis->log != NULL)
		gpuCoreDumpLogPrintf(analysis->log, "%s: %s: %d samples (%d pixels)\n", analysis->prefix, name,
				     samples, pixels);
}

void gpuCoreDumpPrintValue(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value)
{
	if (analysis->log != NULL)
		gpuCoreDumpLogPrintf(analysis->log, "%s: %s: %d\n", analysis->prefix, name, value);
}

/* a tile: x in bits 8-15, y in 0-7, 16 pixels each */
void gpuCoreDumpPrintTile(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value)
{
	SceUInt32 x = (value >> 8) & 0xff;
	SceUInt32 y = value & 0xff;

	if (analysis->log != NULL)
		gpuCoreDumpLogPrintf(analysis->log, "%s: %s: %d,%d (%d,%d px)\n", analysis->prefix, name, x, y,
				     x << 4, y << 4);
}

void gpuCoreDumpPrintTileSwapped(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value)
{
	gpuCoreDumpPrintTile(analysis, name, ((value >> 8) & 0xff) | (value & 0xff) << 8);
}

void gpuCoreDumpPrintUnsigned(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value)
{
	if (analysis->log != NULL)
		gpuCoreDumpLogPrintf(analysis->log, "%s: %s: %u\n", analysis->prefix, name, value);
}

/* the data masters a USSE pipe works for: V(ertex), P(ixel), S(ervice) per slot */
static void gpuCoreDumpPrintUssePipe(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 slots,
				     SceUInt32 pixel, SceUInt32 vertex)
{
	char usage[0x11];
	int active = 0;
	int i;

	usage[0x10] = '\0';
	for (i = 0; i != 0x10; i++, slots >>= 2) {
		switch (slots & 3) {
		case 1:
			usage[i] = 'P';
			analysis->coreFlags |= pixel;
			active = 1;
			break;
		case 0:
			usage[i] = 'V';
			active = 1;
			analysis->coreFlags |= vertex;
			break;
		case 3:
			usage[i] = 'S';
			active = 1;
			break;
		default:
			usage[i] = '.';
			break;
		}
	}
	if (active && analysis->log != NULL)
		gpuCoreDumpLogPrintf(analysis->log, "%s: %s Data Master usage %s\n", analysis->prefix, name,
				     usage);
}

void gpuCoreDumpPrintUssePipe3(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value)
{
	gpuCoreDumpPrintUssePipe(analysis, name, value, 0x400, 0x4000);
}

void gpuCoreDumpPrintUssePipe2(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value)
{
	gpuCoreDumpPrintUssePipe(analysis, name, value, 0x200, 0x2000);
}

void gpuCoreDumpPrintUssePipe1(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value)
{
	gpuCoreDumpPrintUssePipe(analysis, name, value, 0x100, 0x1000);
}

void gpuCoreDumpPrintUssePipe0(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value)
{
	gpuCoreDumpPrintUssePipe(analysis, name, value, 0x80, 0x800);
}

/* the unit that page-faulted (a bit of requesters) and its sub-unit */
static void gpuCoreDumpPrintRequesters(GpuCoreDumpAnalysis *analysis, const GpuCoreDumpRequester *req,
				       SceUInt32 requesters, SceUInt32 subunit)
{
	const GpuCoreDumpSubunit *sub;
	SceUInt32 bit;

	for (bit = 1; req->name != NULL; bit <<= 1, req++) {
		if (!(bit & requesters))
			continue;
		analysis->flags |= req->flags;
		if (analysis->log != NULL)
			gpuCoreDumpLogPrintf(analysis->log, "%s:   - Requester: %s\n", analysis->prefix, req->name);
		sub = req->subunits;
		if (sub == NULL)
			continue;
		for (; sub->name != NULL; sub++) {
			if (sub->id == subunit) {
				if (analysis->log != NULL)
					gpuCoreDumpLogPrintf(analysis->log, "%s:   - Sub-unit: %s\n", analysis->prefix,
							     sub->name);
				break;
			}
		}
	}
}

/* a draw number: 0 or count + 1 */
void gpuCoreDumpPrintDrawNumber(GpuCoreDumpAnalysis *analysis, const char *name, SceUInt32 value)
{
	if (value != 0) {
		if (analysis->log != NULL)
			gpuCoreDumpLogPrintf(analysis->log, "%s: %s: %u\n", analysis->prefix, name, value - 1);
	} else if (analysis->log != NULL) {
		gpuCoreDumpLogPrintf(analysis->log, "%s: %s: 0\n", analysis->prefix, name);
	}
}

/* the process a data master's memory context belongs to (core -1: the master unit) */
static SceUID gpuCoreDumpDmProcess(SceUInt32 dm, SceUInt32 core)
{
	SceUInt32 context;
	unsigned pid;
	int ok;

	if (dm >= GPU_COREDUMP_DATA_MASTERS)
		return -1;
	context = 0;
	if (core == (SceUInt32)-1) {
		ok = gpuCoreDumpMasterValue(gpuCoreDumpMasterDmRegs[dm], &context);
	} else {
		if (core >= GPU_COREDUMP_CORES)
			return -1;
		ok = gpuCoreDumpCoreValue(core, gpuCoreDumpCoreDmRegs[dm], &context);
	}
	if (!ok)
		return -1;
	pid = -1;
	if (SceGpuEs4ForDriver_7AA79658(context, &pid) != 0)
		return -1;
	return pid;
}

/* the process of the vertex or fragment work on a core */
static void gpuCoreDumpPrintActive(int core, int vertex, GpuCoreDumpAnalysis *analysis)
{
	SceUInt32 value, mask, reg;
	const char *phase, *name;
	SceUID pid;
	int shift, low;

	if (!gpuCoreDumpCoreValue(core, 0xc74, &value))
		return;
	if (vertex) {
		low = -7;
		shift = 4;
		mask = 0xf0;
		phase = "Vertex";
	} else {
		low = -3;
		shift = 12;
		mask = 0xf000;
		phase = "Fragment";
	}
	reg = (value << (low + 31)) >> 30 == 3 ? 0xc7c : 0xc78;
	if (!gpuCoreDumpCoreValue(core, reg, &value))
		return;
	pid = gpuCoreDumpDmProcess((mask & value) >> shift, core);
	if (pid < 0) {
		if (analysis->log != NULL)
			gpuCoreDumpLogPrintf(analysis->log, "%s: - %s: Active Process is unknown\n", analysis->prefix,
					     phase);
	} else {
		name = SceSysmemForDriver_E655852F(pid);
		if (analysis->log != NULL)
			gpuCoreDumpLogPrintf(analysis->log, "%s: - %s: Active Process is : %s (0x%08x)\n",
					     analysis->prefix, phase, name != NULL ? name : "unknown", pid);
	}
}

/* a rule's condition on the value it reads */
static inline __attribute__((always_inline)) int gpuCoreDumpRuleHolds(const GpuCoreDumpRule *rule, SceUInt32 value, int *holds)
{
	if (rule->cond == 1)
		*holds = (rule->mask & value) == rule->mask;
	else if (rule->cond == 2)
		*holds = (rule->mask & value) != rule->mask;
	else
		return 0;
	return 1;
}

void gpuCoreDumpAnalyse(PVRSRV_SGXDEV_INFO *devInfo, int cores, SceUInt32 enable,
			       GpuCoreDumpAnalysis *analysis)
{
	const GpuCoreDumpRule *rule;
	const GpuCoreDumpField *field;
	const GpuCoreDumpMessage *msg;
	SceUInt32 status, fault, value, context, address, type, *render;
	SceUID pid;
	const char *name;
	int core, ok, holds, i;

	analysis->coreFlags = GPU_COREDUMP_SUMMARY_BANNER;
	analysis->flags = GPU_COREDUMP_SUMMARY_BANNER | GPU_COREDUMP_SUMMARY_UNKNOWN;
	analysis->pid = -1;
	if (analysis->log != NULL)
		gpuCoreDumpLogPrintf(analysis->log, "%s: GPU Master Analysis:\n", analysis->prefix);
	for (rule = gpuCoreDumpMasterRules; rule->cond != 0; rule++) {
		if (!(enable & rule->enable) || !gpuCoreDumpMasterValue(rule->offset, &value))
			continue;
		if (!gpuCoreDumpRuleHolds(rule, value, &holds) || !holds)
			continue;
		if (analysis->log != NULL)
			gpuCoreDumpLogPrintf(analysis->log, "%s: %s\n", analysis->prefix, rule->message);
		analysis->coreFlags |= rule->flags;
	}
	if (gpuCoreDumpMasterValue(0x4c04, &status)) {
		type = (status >> 16) & 3;
		if (type != 0) {
			analysis->flags = (analysis->flags & ~GPU_COREDUMP_SUMMARY_UNKNOWN) |
					  GPU_COREDUMP_SUMMARY_PAGE_FAULT;
			if (analysis->log != NULL)
				gpuCoreDumpLogPrintf(analysis->log, "%s: - Page fault: %s\n", analysis->prefix,
						     gpuCoreDumpPageFaultTypes[type - 1]);
			if (analysis->log != NULL)
				gpuCoreDumpLogPrintf(analysis->log, "%s:   - Page access type: %s\n", analysis->prefix,
						     (status & 0x40000) ? "Directory access" : "Entry access");
			if (gpuCoreDumpMasterValue(0x4c08, &fault)) {
				context = fault & 0xf;
				address = fault & 0xfffff000;
				gpuCoreDumpPrintRequesters(analysis, gpuCoreDumpMasterRequesters, status & 0x3fff,
							   (fault >> 4) & 0x1f);
				name = gpuCoreDumpRangeName(address);
				if (analysis->log != NULL)
					gpuCoreDumpLogPrintf(analysis->log,
							     "%s:   - Address range: 0x%08x - 0x%08x = %s\n",
							     analysis->prefix, address, address + 0xfff, name);
				pid = gpuCoreDumpDmProcess(context, -1);
				if (pid < 0) {
					if (analysis->log != NULL)
						gpuCoreDumpLogPrintf(analysis->log, "%s:   - Context ID: %d\n",
								     analysis->prefix, context);
				} else {
					name = SceSysmemForDriver_E655852F(pid);
					if (analysis->log != NULL)
						gpuCoreDumpLogPrintf(analysis->log, "%s:   - Process : %s (0x%08x)\n",
								     analysis->prefix,
								     name != NULL ? name : "unknown", pid);
					analysis->pid = pid;
				}
			}
		}
	}
	if (analysis->log != NULL)
		gpuCoreDumpLogPrintf(analysis->log, "%s: \n", analysis->prefix);

	for (core = 0; core != cores; core++) {
		analysis->coreFlags &= GPU_COREDUMP_CORE_KEEP;
		if (analysis->log != NULL)
			gpuCoreDumpLogPrintf(analysis->log, "%s: GPU Core %d Analysis:\n", analysis->prefix, core);
		for (rule = gpuCoreDumpCoreRules; rule->cond != 0; rule++) {
			if (!(enable & rule->enable))
				continue;
			if (rule->source == 0)
				ok = gpuCoreDumpCoreValue(core, rule->offset, &value);
			else if (rule->source == 1)
				ok = gpuCoreDumpIndexedValue(core, rule->offset, &value);
			else
				continue;
			if (!ok || !gpuCoreDumpRuleHolds(rule, value, &holds) || !holds)
				continue;
			if (rule->message != NULL && analysis->log != NULL)
				gpuCoreDumpLogPrintf(analysis->log, "%s: %s\n", analysis->prefix, rule->message);
			analysis->coreFlags |= rule->flags;
		}
		if (gpuCoreDumpCoreValue(core, 0xc04, &status)) {
			type = (status >> 16) & 3;
			if (type != 0) {
				analysis->flags = (analysis->flags & ~GPU_COREDUMP_SUMMARY_UNKNOWN) |
						  GPU_COREDUMP_SUMMARY_PAGE_FAULT;
				if (analysis->log != NULL)
					gpuCoreDumpLogPrintf(analysis->log, "%s: - Page fault: %s\n", analysis->prefix,
							     gpuCoreDumpPageFaultTypes[type - 1]);
				if (analysis->log != NULL)
					gpuCoreDumpLogPrintf(analysis->log, "%s:   - Page access type: %s\n",
							     analysis->prefix,
							     (status & 0x40000) ? "Directory access" : "Entry access");
				if (gpuCoreDumpCoreValue(core, 0xc08, &fault)) {
					context = fault & 0xf;
					address = fault & 0xfffff000;
					gpuCoreDumpPrintRequesters(analysis, gpuCoreDumpCoreRequesters,
								   status & 0x3fff, (fault >> 4) & 0x1f);
					name = gpuCoreDumpRangeName(address);
					if (analysis->log != NULL)
						gpuCoreDumpLogPrintf(analysis->log,
								     "%s:   - Address range: 0x%08x - 0x%08x = %s\n",
								     analysis->prefix, address, address + 0xfff, name);
					pid = gpuCoreDumpDmProcess(context, core);
					if (pid < 0) {
						if (analysis->log != NULL)
							gpuCoreDumpLogPrintf(analysis->log, "%s:   - Context ID: %d\n",
									     analysis->prefix, context);
					} else {
						name = SceSysmemForDriver_E655852F(pid);
						if (analysis->log != NULL)
							gpuCoreDumpLogPrintf(analysis->log,
									     "%s:   - Process : %s (0x%08x)\n",
									     analysis->prefix,
									     name != NULL ? name : "unknown", pid);
						analysis->pid = pid;
					}
				}
			}
		}
		if (analysis->coreFlags & GPU_COREDUMP_CORE_VERTEX)
			gpuCoreDumpPrintActive(core, 1, analysis);
		if (analysis->coreFlags & GPU_COREDUMP_CORE_FRAGMENT)
			gpuCoreDumpPrintActive(core, 0, analysis);
		render = gpuCoreDumpGetRenderState();
		if (render != NULL) {
			if ((analysis->coreFlags & GPU_COREDUMP_CORE_VERTEX) && analysis->log != NULL)
				gpuCoreDumpLogPrintf(analysis->log, "%s: - Vertex: Frame Number: %d, Scene Index: %d\n",
						     analysis->prefix, render[0], render[2]);
			if ((analysis->coreFlags & GPU_COREDUMP_CORE_FRAGMENT) && analysis->log != NULL)
				gpuCoreDumpLogPrintf(analysis->log,
						     "%s: - Fragment: Frame Number: %d, Scene Index: %d\n",
						     analysis->prefix, render[1], render[3]);
		}
		for (field = gpuCoreDumpFields; field->name != NULL; field++) {
			if ((analysis->coreFlags & field->require) != field->require)
				continue;
			if (field->source == 0)
				ok = gpuCoreDumpCoreValue(core, field->offset, &value);
			else if (field->source == 1)
				ok = gpuCoreDumpIndexedValue(core, field->offset, &value);
			else
				continue;
			if (ok && field->print != NULL)
				field->print(analysis, field->name, (field->mask & value) >> field->shift);
		}
		if (analysis->log != NULL)
			gpuCoreDumpLogPrintf(analysis->log, "%s: \n", analysis->prefix);
	}

	if (devInfo->psSGXHostCtl->partialRenders != 0) {
		analysis->flags |= GPU_COREDUMP_SUMMARY_PARTIAL;
		if (analysis->log != NULL)
			gpuCoreDumpLogPrintf(analysis->log, "%s: Partial renders since boot: %d\n", analysis->prefix,
					     devInfo->psSGXHostCtl->partialRenders);
	}
	for (i = 0; i != GPU_COREDUMP_DATA_MASTERS; i++) {
		pid = gpuCoreDumpDmProcess(i, 0);
		analysis->dm[i].name = NULL;
		analysis->dm[i].pid = pid;
		if (pid >= 0)
			analysis->dm[i].name = SceSysmemForDriver_E655852F(pid);
	}
	for (msg = gpuCoreDumpMessages; msg->flag != 0; msg++)
		if ((msg->flag & analysis->flags) && analysis->log != NULL)
			gpuCoreDumpLogPrintf(analysis->log, "%s: %s\n", analysis->prefix, msg->message);
}
