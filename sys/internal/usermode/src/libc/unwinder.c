// unwinder.c: the language-independent unwinder of the ARM exception handling ABI (EHABI):
// the virtual register set the personality routines read and pop, the lookup of a return
// address in the exception index table of the module that holds it, and the two phases of
// a throw (find the handler, then unwind to it). The entry points that capture the caller's
// registers, and the VFP save and restore they need, are in unwinder_asm.S.
#include <stdlib.h>
#include <libkernel/libkernel.h>
#include "unwinder.h"

#define R_SP	13
#define R_LR	14
#define R_PC	15

#define EXIDX_CANTUNWIND	1	/* an index entry for a function that cannot be unwound */

		/* THE VIRTUAL REGISTER SETS */
struct core_s
	{	/* the core registers */
	unsigned int r[16];
	};

struct vfp_s
	{	/* the VFP registers */
	unsigned long long d[32];
	};

typedef struct phase2_virtual_register_set_s
	{	/* the registers the entry stub captured: phase 2 unwinds them for real */
	unsigned char demand_save_vfp_low;	/* 1: save d0-d15 before the first change */
	unsigned char demand_save_vfp_high;	/* 1: save d16-d31 before the first change */
	struct core_s core;
	} phase2_virtual_register_set;

typedef struct phase1_virtual_register_set_s
	{	/* a copy that phase 1 unwinds virtually, with the VFP registers it saved */
	unsigned char demand_save_vfp_low;
	unsigned char demand_save_vfp_high;
	struct core_s core;
	struct vfp_s vfp;
	} phase1_virtual_register_set;

typedef _Unwind_Reason_Code (*personality_routine)(_Unwind_State state,
	_Unwind_Control_Block *ucbp, _Unwind_Context *context);

		/* THE EXCEPTION INDEX TABLE */
typedef struct __EIT_entry
	{	/* an entry of a module's exception index table (.ARM.exidx) */
	unsigned int fnoffset;	/* the function: a 31-bit place-relative offset */
	unsigned int content;	/* EXIDX_CANTUNWIND, the unwinding data, or an offset to it */
	} __EIT_entry;

static const __EIT_entry *EIT_limit;	/* the index table of the last module searched */
static const __EIT_entry *EIT_base;
static unsigned int text_base;	/* its executable segment */
static unsigned int text_limit;

static int EIT_comparator(const void *ck, const void *ce);

		/* THE ENTRY STUBS AND THE VFP REGISTERS (unwinder_asm.S) */
void __ARM_Unwind_VRS_VFPpreserve_low(void *vfpp);
void __ARM_Unwind_VRS_VFPpreserve_high(void *vfpp);
void __ARM_Unwind_VRS_VFPrestore_low(void *vfpp);
void __ARM_Unwind_VRS_VFPrestore_high(void *vfpp);
void __ARM_Unwind_VRS_corerestore(void *corep);

_Unwind_Reason_Code __aeabi_unwind_cpp_pr0(_Unwind_State state,
	_Unwind_Control_Block *ucbp, _Unwind_Context *context);
_Unwind_Reason_Code __aeabi_unwind_cpp_pr1(_Unwind_State state,
	_Unwind_Control_Block *ucbp, _Unwind_Context *context);
_Unwind_Reason_Code __aeabi_unwind_cpp_pr2(_Unwind_State state,
	_Unwind_Control_Block *ucbp, _Unwind_Context *context);

static __inline unsigned int __ARM_resolve_prel31(const void *p)
{	/* the address a 31-bit place-relative offset at p points to */
	return (unsigned int)((*(const int *)p << 1) >> 1) + (unsigned int)p;
}

_Unwind_VRS_Result _Unwind_VRS_Set(_Unwind_Context *context, _Unwind_VRS_RegClass regclass,
	unsigned int regno, _Unwind_VRS_DataRepresentation representation, void *valuep)
{	/* set a register of the virtual register set */
	phase1_virtual_register_set *vrsp = (phase1_virtual_register_set *)context;

	switch (regclass) {
	case _UVRSC_CORE:
		if (representation != _UVRSD_UINT32 || regno > 15)
			return _UVRSR_FAILED;
		vrsp->core.r[regno] = *(unsigned int *)valuep;
		return _UVRSR_OK;
	case _UVRSC_VFP:
	case _UVRSC_WMMXD:
	case _UVRSC_WMMXC:
		return _UVRSR_NOT_IMPLEMENTED;
	default:
		break;
	}
	return _UVRSR_FAILED;
}

_Unwind_VRS_Result _Unwind_VRS_Get(_Unwind_Context *context, _Unwind_VRS_RegClass regclass,
	unsigned int regno, _Unwind_VRS_DataRepresentation representation, void *valuep)
{	/* read a register of the virtual register set */
	phase1_virtual_register_set *vrsp = (phase1_virtual_register_set *)context;

	switch (regclass) {
	case _UVRSC_CORE:
		if (representation != _UVRSD_UINT32 || regno > 15)
			return _UVRSR_FAILED;
		*(unsigned int *)valuep = vrsp->core.r[regno];
		return _UVRSR_OK;
	case _UVRSC_VFP:
	case _UVRSC_WMMXD:
	case _UVRSC_WMMXC:
		return _UVRSR_NOT_IMPLEMENTED;
	default:
		break;
	}
	return _UVRSR_FAILED;
}

_Unwind_VRS_Result _Unwind_VRS_Pop(_Unwind_Context *context, _Unwind_VRS_RegClass regclass,
	unsigned int discriminator, _Unwind_VRS_DataRepresentation representation)
{	/* pop registers from the virtual stack into the virtual register set */
	phase1_virtual_register_set *vrsp = (phase1_virtual_register_set *)context;

	switch (regclass) {
	case _UVRSC_CORE: {
		unsigned int mask, *vsp, *rp, sp_loaded;

		if (representation != _UVRSD_UINT32)
			return _UVRSR_FAILED;
		mask = discriminator & 0xffff;
		vsp = (unsigned int *)vrsp->core.r[R_SP];
		rp = vrsp->core.r;
		sp_loaded = discriminator & (1 << R_SP);	/* the popped sp wins */
		for (; mask != 0; mask >>= 1, rp++)
			if (mask & 1)
				*rp = *vsp++;
		if (!sp_loaded)
			vrsp->core.r[R_SP] = (unsigned int)vsp;
		return _UVRSR_OK;
	}
	case _UVRSC_VFP: {
		unsigned int start = discriminator >> 16;
		unsigned int count = discriminator & 0xffff;
		int some_low = start < 16;
		int some_high = start + count > 16;
		struct vfp_s temp_vfp;
		unsigned int *vsp;

		if ((representation != _UVRSD_VFPX && representation != _UVRSD_DOUBLE)
			|| (representation == _UVRSD_VFPX && some_high))
			return _UVRSR_FAILED;
		if (representation == _UVRSD_DOUBLE && start + count > 32)
			return _UVRSR_FAILED;
		if (some_low && vrsp->demand_save_vfp_low == 1) {	/* keep the caller's */
			vrsp->demand_save_vfp_low = 0;
			__ARM_Unwind_VRS_VFPpreserve_low(&vrsp->vfp.d[0]);
		}
		if (some_high && vrsp->demand_save_vfp_high == 1) {
			vrsp->demand_save_vfp_high = 0;
			__ARM_Unwind_VRS_VFPpreserve_high(&vrsp->vfp.d[16]);
		}
		if (some_low)
			__ARM_Unwind_VRS_VFPpreserve_low(&temp_vfp.d[0]);
		if (some_high)
			__ARM_Unwind_VRS_VFPpreserve_high(&temp_vfp.d[16]);
		vsp = (unsigned int *)vrsp->core.r[R_SP];
		while (count--) {	/* word by word: the stack is only word aligned */
			unsigned int *dp = (unsigned int *)&temp_vfp.d[start++];
			const unsigned int *wp = vsp;

			vsp += 2;
			dp[0] = wp[0];
			dp[1] = wp[1];
		}
		vrsp->core.r[R_SP] = (unsigned int)(vsp + (representation == _UVRSD_VFPX));
		if (some_low)
			__ARM_Unwind_VRS_VFPrestore_low(&temp_vfp.d[0]);
		if (some_high)
			__ARM_Unwind_VRS_VFPrestore_high(&temp_vfp.d[16]);
		return _UVRSR_OK;
	}
	case _UVRSC_WMMXD:
	case _UVRSC_WMMXC:
		return _UVRSR_NOT_IMPLEMENTED;
	default:
		break;
	}
	return _UVRSR_FAILED;
}

void _Unwind_DebugHook(void)
{	/* an empty debugger hook nothing calls, kept in the library */
}

void _Unwind_Activity(_Unwind_Control_Block *ucbp, unsigned int reason, unsigned int arg)
{	/* a hook for debuggers: unwinding reached a point of interest */
}

static _Unwind_Reason_Code find_and_expand_eit_entry(_Unwind_Control_Block *ucbp,
	unsigned int return_address)
{	/* find the index entry for a return address, and cache its unwinding data */
	SceKernelModuleInfo info;
	const __EIT_entry *eitp;
	unsigned int *ehtp, additional;
	int i;

	return_address -= 2;	/* inside the call */
	if (return_address < text_base || text_limit < return_address) {	/* another module */
		if (sceKernelGetModuleInfoByAddr((const void *)return_address, &info) != 0)
			goto lookup_failed;
		for (i = 0; i < 4; i++)
			if (info.segments[i].perms & 1) {	/* the executable segment */
				EIT_base = (const __EIT_entry *)info.exidx_top;
				EIT_limit = (const __EIT_entry *)info.exidx_btm;
				text_base = (unsigned int)info.segments[i].vaddr;
				text_limit = text_base + info.segments[i].memsz;
				break;
			}
	}
	eitp = (const __EIT_entry *)bsearch(&return_address, EIT_base, EIT_limit - EIT_base,
		sizeof(__EIT_entry), EIT_comparator);
	if (eitp == NULL) {
lookup_failed:
		ucbp->unwinder_cache.reserved3 = 0;
		_Unwind_Activity(ucbp, _UAACT_ENDING, _UAARG_ENDING_UNWINDER_LOOKUPFAILED);
		return _URC_FAILURE;
	}
	ucbp->pr_cache.fnstart = __ARM_resolve_prel31(&eitp->fnoffset);
	if (eitp->content == EXIDX_CANTUNWIND) {
		ucbp->unwinder_cache.reserved3 = 0;
		_Unwind_Activity(ucbp, _UAACT_ENDING, _UAARG_ENDING_NOUNWIND);
		return _URC_FAILURE;
	}
	ehtp = (unsigned int *)&eitp->content;	/* the unwinding data is in the entry */
	additional = 1;
	if (!(eitp->content & 0x80000000)) {	/* in .ARM.extab */
		ehtp = (unsigned int *)__ARM_resolve_prel31(&eitp->content);
		additional = 0;
	}
	ucbp->pr_cache.additional = additional;
	ucbp->pr_cache.ehtp = ehtp;
	if (*ehtp & 0x80000000) {	/* the compact model: one of the three routines */
		unsigned int idx = (*ehtp >> 24) & 0xf;

		if (idx == 0)
			ucbp->unwinder_cache.reserved3 = (unsigned int)&__aeabi_unwind_cpp_pr0;
		else if (idx == 1)
			ucbp->unwinder_cache.reserved3 = (unsigned int)&__aeabi_unwind_cpp_pr1;
		else if (idx == 2)
			ucbp->unwinder_cache.reserved3 = (unsigned int)&__aeabi_unwind_cpp_pr2;
		else {
			ucbp->unwinder_cache.reserved3 = 0;
			_Unwind_Activity(ucbp, _UAACT_ENDING, _UAARG_ENDING_TABLECORRUPT);
			return _URC_FAILURE;
		}
	} else	/* the generic model: an offset to the routine */
		ucbp->unwinder_cache.reserved3 = __ARM_resolve_prel31(ehtp);
	return _URC_OK;
}

void unwind_phase2(_Unwind_Control_Block *ucbp, phase2_virtual_register_set *vrsp)
{	/* unwind frame by frame until a landing pad takes the exception */
	_Unwind_Reason_Code pr_result;

	for (;;) {
		if (find_and_expand_eit_entry(ucbp, vrsp->core.r[R_PC]) != _URC_OK)
			abort();
		ucbp->unwinder_cache.reserved2 = vrsp->core.r[R_PC];	/* the call site */
		pr_result = ((personality_routine)ucbp->unwinder_cache.reserved3)(
			_US_UNWIND_FRAME_STARTING, ucbp, (_Unwind_Context *)vrsp);
		if (pr_result == _URC_INSTALL_CONTEXT)
			__ARM_Unwind_VRS_corerestore(&vrsp->core);
		else if (pr_result != _URC_CONTINUE_UNWIND)
			abort();
	}
}

static void restore_non_core_regs(phase1_virtual_register_set *vrsp)
{	/* put back the VFP registers phase 1 changed */
	if (vrsp->demand_save_vfp_low == 0)
		__ARM_Unwind_VRS_VFPrestore_low(&vrsp->vfp.d[0]);
	if (vrsp->demand_save_vfp_high == 0)
		__ARM_Unwind_VRS_VFPrestore_high(&vrsp->vfp.d[16]);
}

_Unwind_Reason_Code __ARM_Unwind_RaiseException(_Unwind_Control_Block *ucbp,
	phase2_virtual_register_set *entry_vrsp)
{	/* throw: find a handler (phase 1), then unwind to it (phase 2) */
	phase1_virtual_register_set phase1_vrs;
	_Unwind_Reason_Code pr_result;

	if (ucbp->unwinder_cache.reserved1 != 0) {	/* rethrown while propagating: keep the state */
		_Unwind_Control_Block *saved = (_Unwind_Control_Block *)malloc(sizeof(*saved));

		if (saved == NULL)
			return _URC_FAILURE;
		saved->unwinder_cache = ucbp->unwinder_cache;
		saved->barrier_cache = ucbp->barrier_cache;
		saved->cleanup_cache = ucbp->cleanup_cache;
		ucbp->unwinder_cache.reserved1 = (unsigned int)saved;
	} else
		ucbp->unwinder_cache.reserved1 = 1;
	entry_vrsp->core.r[R_PC] = entry_vrsp->core.r[R_LR];
	phase1_vrs.core = entry_vrsp->core;
	phase1_vrs.demand_save_vfp_low = 1;
	phase1_vrs.demand_save_vfp_high = 1;
	for (;;) {
		if (find_and_expand_eit_entry(ucbp, phase1_vrs.core.r[R_PC]) != _URC_OK) {
			restore_non_core_regs(&phase1_vrs);
			return _URC_FAILURE;
		}
		pr_result = ((personality_routine)ucbp->unwinder_cache.reserved3)(
			_US_VIRTUAL_UNWIND_FRAME, ucbp, (_Unwind_Context *)&phase1_vrs);
		if (pr_result == _URC_HANDLER_FOUND)
			break;
		if (pr_result == _URC_CONTINUE_UNWIND)
			continue;
		restore_non_core_regs(&phase1_vrs);
		return _URC_FAILURE;
	}
	restore_non_core_regs(&phase1_vrs);
	unwind_phase2(ucbp, entry_vrsp);
}

void __ARM_Unwind_Resume(_Unwind_Control_Block *ucbp, phase2_virtual_register_set *entry_vrsp)
{	/* a cleanup is done: unwind on */
	entry_vrsp->demand_save_vfp_low = 0;
	entry_vrsp->demand_save_vfp_high = 0;
	entry_vrsp->core.r[R_PC] = entry_vrsp->core.r[R_LR];
	unwind_phase2(ucbp, entry_vrsp);
}

void __ARM_Unwind_Walk(void *unused, phase2_virtual_register_set *entry_vrsp)
{	/* unwind the caller's stack virtually, frame by frame, to its end */
	phase1_virtual_register_set phase1_vrs;
	_Unwind_Control_Block ucb;
	_Unwind_Reason_Code pr_result;

	entry_vrsp->core.r[R_PC] = entry_vrsp->core.r[R_LR];
	phase1_vrs.core = entry_vrsp->core;
	phase1_vrs.demand_save_vfp_low = 0;
	phase1_vrs.demand_save_vfp_high = 0;
	do {
		if (find_and_expand_eit_entry(&ucb, phase1_vrs.core.r[R_PC]) != _URC_OK)
			break;
		pr_result = ((personality_routine)ucb.unwinder_cache.reserved3)(
			_US_VIRTUAL_UNWIND_FRAME, &ucb, (_Unwind_Context *)&phase1_vrs);
		if ((int)phase1_vrs.core.r[R_PC] < 0)
			break;
	} while (pr_result == _URC_HANDLER_FOUND || pr_result == _URC_CONTINUE_UNWIND);
}

void _Unwind_Complete(_Unwind_Control_Block *ucbp)
{	/* the exception is caught: give back the state a rethrow kept */
	_Unwind_Control_Block *saved = (_Unwind_Control_Block *)ucbp->unwinder_cache.reserved1;

	if (saved == NULL)
		abort();
	if (saved == (_Unwind_Control_Block *)1) {
		ucbp->unwinder_cache.reserved1 = 0;
		return;
	}
	ucbp->unwinder_cache = saved->unwinder_cache;
	ucbp->barrier_cache = saved->barrier_cache;
	ucbp->cleanup_cache = saved->cleanup_cache;
	free(saved);
}

void _Unwind_DeleteException(_Unwind_Control_Block *ucbp)
{	/* destroy a caught foreign exception */
	if (ucbp->exception_cleanup != NULL)
		ucbp->exception_cleanup(_URC_FOREIGN_EXCEPTION_CAUGHT, ucbp);
}

static int EIT_comparator(const void *ck, const void *ce)
{	/* bsearch: is the return address before, in or after an entry's function */
	unsigned int return_address = *(const unsigned int *)ck;
	const __EIT_entry *eitp = (const __EIT_entry *)ce;
	const __EIT_entry *next_eitp = eitp + 1;
	unsigned int next_fn;

	if (next_eitp != EIT_limit)
		next_fn = __ARM_resolve_prel31(&next_eitp->fnoffset);
	else
		next_fn = 0xffffffff;
	if (return_address < __ARM_resolve_prel31(&eitp->fnoffset))
		return -1;
	if (return_address >= next_fn)
		return 1;
	return 0;
}
