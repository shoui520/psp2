/*
 * callout.c: BSD-style timer callout stubs used by PDP/PTP and slpque in SceLibPspnetAdhoc.
 */

#include "pspnet_adhoc_internal.h"

__attribute__((noinline))
void adhocCalloutInit(AdhocCallout *c)
{
	sceClibMemset(c, 0, sizeof(*c));
}

__attribute__((noinline))
void adhocCalloutNoop(void)
{
}

__attribute__((noinline))
void adhocCalloutStop(AdhocCallout *c)
{
	int s;

	s = adhocSplNet();
	if (c->flags & PSPNET_ADHOC_CALLOUT_PENDING) {
		c->next = NULL;
		c->prev = NULL;
		c->func = NULL;
		c->flags &= ~PSPNET_ADHOC_CALLOUT_PENDING;
	}
	c->flags &= ~PSPNET_ADHOC_CALLOUT_ACTIVE;
	adhocSplX(s);
}

__attribute__((noinline))
void adhocCalloutReset(AdhocCallout *c, int ticks, void (*func)(void *arg), void *arg)
{
	int s;

	if (ticks == 0)
		ticks = 1;
	s = adhocSplNet();
	if (c->flags & PSPNET_ADHOC_CALLOUT_PENDING) {
		int nestedSpl = adhocSplNet();
		if (c->flags & PSPNET_ADHOC_CALLOUT_PENDING) {
			c->next = NULL;
			c->prev = NULL;
			c->func = NULL;
			c->flags &= ~PSPNET_ADHOC_CALLOUT_PENDING;
		}
		c->flags &= ~PSPNET_ADHOC_CALLOUT_ACTIVE;
		adhocSplX(nestedSpl);
	}
	c->func = func;
	c->arg = arg;
	c->time = ticks;
	c->flags = PSPNET_ADHOC_CALLOUT_ACTIVE | PSPNET_ADHOC_CALLOUT_PENDING;
	c->next = NULL;
	c->prev = NULL;
	adhocSplX(s);
}
