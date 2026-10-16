/* Shared exception context definitions. */
#pragma once

/* name unknown: return from an exception into the given context */
void excpReturnToContext(SceExcpmgrContext *context)
{
	const unsigned long long *vfp;

	writeDfsr(0);
	writeIfsr(0);
	writeAdfsr(0);
	writeAifsr(0);
	writeDfar(0);
	writeIfar(0);
	writeTpidruro(context->tpidruro);
	excpSetAddressSpace(context->ttbr1, context->contextId);
	writeDacr(context->dacr);
	writePar(context->par);
	excpLeave(context);
	if (context->excpCode != 0) {
		writeFpexc(context->fpexc);
		writeFpscr(context->fpscr);
		/*
		 * Reload d0-d31 of the interrupted code: a register list C cannot
		 * name. Deliberately no clobber list, the compiler keeps no state in
		 * VFP registers here and what follows must not save/restore them.
		 */
		vfp = context->vfp;
		__asm__ volatile(".fpu neon\n\t"
				 "vldmia %0!, {d0-d15}\n\t"
				 "vldmia %0, {d16-d31}"
				 : "+r" (vfp) :: "memory");
	}
	writeCpacr(context->cpacr);
	excpRestoreDebugRegisters(context);
	excpRearmTrace();
	excpReturnToAbortMode(context);
}
