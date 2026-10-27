/*
 * NSKBL: the C runtime's abort, which the unwinder calls when it cannot go on. The boot
 * loader has nowhere to report to, so it stops where it is. (File name ours: no record or
 * string names it.)
 */

void abort(void) __attribute__((noreturn));

void abort(void)
{
	for (;;)
		;
}
