/*
 * Marvell 88W8787 Mask ROM entry points and ROM-owned data used by the
 * converted firmware. Only symbols referenced by src/ are listed; entries
 * are added as code is converted. Device and ThreadX entries are Thumb;
 * ARM runtime entry points are listed separately below.
 *
 * ThreadX names are inferred from use (see include/robin/threadx.h).
 */

/* ThreadX */
PROVIDE(tx_kernel_enter       = 0x03f02f3d);
PROVIDE(tx_timer_activate     = 0x03f0420d);
PROVIDE(tx_timer_deactivate   = 0x03f04381);
PROVIDE(tx_timer_change       = 0x03f0450d);
PROVIDE(tx_timer_create       = 0x03f0452d);
PROVIDE(tx_time_get           = 0x03f045e5);
PROVIDE(_tx_timer_interrupt   = 0x03f05199);
PROVIDE(tx_interrupt_control  = 0x03f052fd);
/* Task API identities inferred from genuine RAM adapters and their callers. */
PROVIDE(tx_thread_suspend = 0x03f04ad1);
PROVIDE(tx_thread_resume = 0x03f04951);
PROVIDE(tx_thread_sleep = 0x03f04b59);
PROVIDE(tx_thread_priority_change = 0x03f04811);
PROVIDE(tx_thread_identify = 0x03f044b9);
PROVIDE(tx_thread_relinquish = 0x03f0499d);
/* Genuine RAM veneer supplies this external timestamp entry address. */
PROVIDE(romGetTimerTimestamp = 0x03f3a6c5);
PROVIDE(_tx_thread_system_state = 0x0400fc78);

/* mdev device framework */
PROVIDE(mdevInterrupt         = 0x03f06301);
PROVIDE(mdevForwardEvent      = 0x03f062db);
PROVIDE(mdevOpenDevice        = 0x03f06255);
PROVIDE(mdevIoctl             = 0x03f062f1);
PROVIDE(mdevReleaseDevice     = 0x03f062a3);

/* Buffer allocator, original ARM veneer resolves this Thumb entry. */
PROVIDE(osa_BufferFree        = 0x03f05525);

/* ARM run-time ABI helpers (ARM state) */
PROVIDE(__aeabi_memcpy        = 0x03f3f00c);
PROVIDE(__aeabi_memcpy4       = 0x03f3f0e0);
PROVIDE(__aeabi_memclr4       = 0x03f3f144);
