/*
 * weak_calls.c: forwarding functions linked between SNC's runtime objects: each calls a
 * function that is not linked in (a weak reference, zero) with a trailing flag, and returns
 * 9 when it is absent.  Nothing calls them.  A module whose original has the second one too
 * (the flag 1 form) defines SNC_WEAK_CALLS_ON.  The original has no name or debug record for
 * the object or the callees; the names are ours.
 */

extern int sceUnlinkedForward(int a0, int a1, int a2, int flag) __attribute__((weak));

int screenShotForwardOff(int a0, int a1, int a2)
{
	if (sceUnlinkedForward == 0)
		return 9;
	return sceUnlinkedForward(a0, a1, a2, 0);
}

#ifdef SNC_WEAK_CALLS_ON
extern int sceUnlinkedForward2(int a0, int a1, int a2, int flag) __attribute__((weak));

int screenShotForwardOn(int a0, int a1, int a2)
{
	if (sceUnlinkedForward2 == 0)
		return 9;
	return sceUnlinkedForward2(a0, a1, a2, 1);
}
#endif
