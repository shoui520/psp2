/*
 * weak_calls.c: two forwarding functions linked between SNC's i64mod and i64div objects:
 * each calls a function that is not linked in (a weak reference, zero) with a trailing
 * flag, and returns 9 when it is absent.  Nothing calls them.  The original has no name or
 * debug record for the object or the callee; the names are ours.
 */

extern int sceUnlinkedForward(int a0, int a1, int a2, int flag) __attribute__((weak));
extern int sceUnlinkedForward2(int a0, int a1, int a2, int flag) __attribute__((weak));

int photoExportForwardOff(int a0, int a1, int a2)
{
	if (sceUnlinkedForward == 0)
		return 9;
	return sceUnlinkedForward(a0, a1, a2, 0);
}

int photoExportForwardOn(int a0, int a1, int a2)
{
	if (sceUnlinkedForward2 == 0)
		return 9;
	return sceUnlinkedForward2(a0, a1, a2, 1);
}
