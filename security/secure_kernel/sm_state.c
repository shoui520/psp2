/*
 * sm_state.c: Secure Module critical section, stop-pending, and suspend-pending
 * state flags in .sbss (0x805a6a..0x805aab).
 *
 * Internal function and variable names are ours.
 */

#include "secure_kernel.h"

/* ============================================================================
 * .sbss (0x808170..0x80817c)
 * ============================================================================ */

static uint32_t s_smSuspendPending;		/* 0x808170 */
static uint32_t s_smStopPending;		/* 0x808174 */
static uint32_t s_smCriticalSectionActive;	/* 0x808178 */

/* ============================================================================
 * Functions (0x805a6a..0x805aab)
 * ============================================================================ */

/* 0x805a6a (name ours) */
void clearSmCriticalSectionActive(void)
{
	s_smCriticalSectionActive = 0u;
}

/* 0x805a72 (name ours) */
void setSmCriticalSectionActive(void)
{
	s_smCriticalSectionActive = 1u;
}

/* 0x805a7a (name ours) */
uint32_t isSmCriticalSectionActive(void)
{
	return s_smCriticalSectionActive;
}

/* 0x805a80 (name ours) */
void setSmStopPending(void)
{
	s_smStopPending = 1u;
}

/* 0x805a88 (name ours) */
void clearSmStopPending(void)
{
	s_smStopPending = 0u;
}

/* 0x805a90 (name ours) */
uint32_t isSmStopPending(void)
{
	return s_smStopPending;
}

/* 0x805a96 (name ours) */
void setSmSuspendPending(void)
{
	s_smSuspendPending = 1u;
}

/* 0x805a9e (name ours) */
void clearSmSuspendPending(void)
{
	s_smSuspendPending = 0u;
}

/* 0x805aa6 (name ours) */
uint32_t isSmSuspendPending(void)
{
	return s_smSuspendPending;
}
