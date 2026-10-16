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
PROVIDE(__aeabi_memclr        = 0x03f3f198);
PROVIDE(__aeabi_memset4       = 0x03f3effc);
PROVIDE(__aeabi_uread4        = 0x03f3f220);
PROVIDE(strlen                = 0x03f3f1d9);

/* ThreadX byte and block pools, queues, event flags, semaphores, threads. */
PROVIDE(tx_block_allocate       = 0x03f01401);
PROVIDE(tx_block_pool_create    = 0x03f01539);
PROVIDE(tx_block_pool_delete    = 0x03f015c1);
PROVIDE(tx_block_release        = 0x03f01715);
PROVIDE(tx_byte_allocate        = 0x03f01799);
PROVIDE(tx_byte_pool_create     = 0x03f018f5);
PROVIDE(tx_byte_pool_delete     = 0x03f01975);
PROVIDE(tx_byte_release         = 0x03f01ac9);
PROVIDE(tx_event_flags_create   = 0x03f021d1);
PROVIDE(tx_event_flags_get      = 0x03f022d5);
PROVIDE(tx_event_flags_info_get = 0x03f02395);
PROVIDE(tx_event_flags_set      = 0x03f023c9);
PROVIDE(tx_queue_create         = 0x03f035c5);
PROVIDE(tx_queue_receive        = 0x03f03ad5);
PROVIDE(tx_queue_send           = 0x03f03cc5);
PROVIDE(tx_semaphore_create     = 0x03f03f61);
PROVIDE(tx_semaphore_get        = 0x03f04065);
PROVIDE(tx_semaphore_put        = 0x03f0412d);
PROVIDE(tx_thread_create        = 0x03f04299);

/* Mask ROM message and buffer services (Thumb). */
PROVIDE(osa_MsgAlloc          = 0x03f06791);
PROVIDE(osa_MsgFree           = 0x03f06797);
PROVIDE(osa_MsgSend           = 0x03f067af);
PROVIDE(osa_MsgQueueSend      = 0x03f06357);
PROVIDE(osa_BufferAlloc       = 0x03f056c9);
PROVIDE(osa_MsgQueueRecv      = 0x03f06315);
PROVIDE(osa_MsgQueueCount     = 0x03f063c7);
PROVIDE(osa_BlockPoolCreate   = 0x03f0562b);
PROVIDE(mdevRegisterDriver    = 0x03f061d9);
PROVIDE(mdevReceiveMessage    = 0x03f06813);

/* Mask ROM Bluetooth controller entry points; roles are inferred. */
PROVIDE(btHccRomApplyState      = 0x03f13185);
PROVIDE(btLcoRomVerifyState     = 0x03f1cdab);
PROVIDE(btBdhRomResolveState    = 0x03f07bbf);
PROVIDE(btBdhRomProcessContext  = 0x03f087e3);
PROVIDE(btBdhRomScheduleState   = 0x03f07be9);
PROVIDE(btBdhRomUpdateContext   = 0x03f08991);
PROVIDE(btBdhRomHandleContext   = 0x03f08459);
PROVIDE(btBdhRomRecordState     = 0x03f0821f);
PROVIDE(btBdhRomNotifyState     = 0x03f07bf9);
PROVIDE(btBdhRomFilterState     = 0x03f0827b);
PROVIDE(btBdhRomTrackState      = 0x03f07ce9);
PROVIDE(btBdhRomApplyState      = 0x03f07b8d);
PROVIDE(btLscRomApplyState      = 0x03f31de9);
PROVIDE(btBdhRomSyncState       = 0x03f07b6b);
PROVIDE(btBdhRomFormatState     = 0x03f07c2b);
PROVIDE(btBdhRomSelectState     = 0x03f080ff);
PROVIDE(btLscRomNotifyState     = 0x03f31e59);
PROVIDE(btBdhRomAcquireState    = 0x03f07deb);
PROVIDE(btBdhRomVerifyState     = 0x03f08051);
PROVIDE(btLscRomResolveState    = 0x03f31e07);
PROVIDE(btBdhRomClearState      = 0x03f07f7b);
PROVIDE(btBdhRomSetupState      = 0x03f07f03);
PROVIDE(btBdhRomConvertState    = 0x03f07fdb);
PROVIDE(btBdhRomAdjustState     = 0x03f08169);
PROVIDE(btBdhRomInspectState    = 0x03f07c5d);
PROVIDE(btBdhRomManageState     = 0x03f07d5f);

/* ROM data: message queue handles, and the Bluetooth controller's RAM state. */
PROVIDE(romQueueHct             = 0x03f41740);
PROVIDE(romQueueBtCore          = 0x03f41744);
PROVIDE(romHctLinkInfo          = 0x0400b160);
PROVIDE(romHctPendingCommand    = 0x0400b1b8);
PROVIDE(romHctLinkQueue         = 0x0400b1c8);
PROVIDE(romHctHostSlot          = 0x0400c3cc);
PROVIDE(romHctReleaseBuffer     = 0x0400eb8c);
PROVIDE(romHctAcquireSession    = 0x0400eb94);
PROVIDE(romHctSourceCounter     = 0x0400eba4);
PROVIDE(romHctMode              = 0x0400f44c);
PROVIDE(romHctFlowEnabled       = 0x0400f44f);
PROVIDE(romHctDeferredCount     = 0x0400f450);
PROVIDE(romHctFrameFlag         = 0x0400f453);
PROVIDE(romHctHeaderSize        = 0x0400f454);
PROVIDE(romHctDescriptor        = 0x0400f455);
PROVIDE(romHctQueueMode         = 0x0400f457);
PROVIDE(romHctTimer             = 0x0400f45a);
PROVIDE(romHctChannel           = 0x0400f45e);
PROVIDE(romHctParam             = 0x0400f460);
PROVIDE(romHctControl           = 0x0400f474);
PROVIDE(romHctWindow            = 0x0400f4a0);
PROVIDE(romHctFilterAcl         = 0x0400f594);
PROVIDE(romHctFilterEvent       = 0x0400f598);
PROVIDE(romTargetTable          = 0x0400ddf6);
PROVIDE(romHctCoexMode          = 0x0400f448);
PROVIDE(romCoexPrimaryFlag      = 0xc00330c8);
PROVIDE(romHctRxHook             = 0x0400fa30);
PROVIDE(mdevRomQueryState       = 0x03f06611);
PROVIDE(mdevRomProcessState     = 0x03f062b3);

PROVIDE(romStringCopy   = 0x03f066ed);
PROVIDE(romStringLength = 0x03f0672b);
PROVIDE(romRateIndex     = 0x03f3ab4f);
PROVIDE(romEdcaSetCwMin = 0x03f3a62f);
PROVIDE(romEdcaSetCwMax = 0x03f3a643);
PROVIDE(romEdcaSetAifsn = 0x03f3a665);
PROVIDE(romEdcaSetTxop  = 0x03f3a64d);
PROVIDE(romMacStatValidate = 0x03f3acdb);
PROVIDE(romMacStatPrepare  = 0x03f3ace7);
PROVIDE(romMacStatCheck    = 0x03f3accf);
PROVIDE(romMacWakeRequest  = 0x03f3a917);
PROVIDE(romMacSleepNotify  = 0x03f3a907);
PROVIDE(__aeabi_uldivmod   = 0x03f3ec30);
PROVIDE(romMacEntryCheck   = 0x03f3ac3d);
PROVIDE(romMacEntryWrite   = 0x03f3ac8b);
PROVIDE(romMacConfigureEntry = 0x03f3a865);
