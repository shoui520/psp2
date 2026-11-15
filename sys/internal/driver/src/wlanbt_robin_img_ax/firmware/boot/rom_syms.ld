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
/* 32x32 -> 64-bit multiply (low word in r0, high in r1), reached through the veneer at 0x52d4c. */
PROVIDE(romMul32x32 = 0x03f3f3b4);
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
PROVIDE(osa_MsgQueuePeek     = 0x03f063c7);
PROVIDE(osa_MsgQueueSendFront = 0x03f06393);
PROVIDE(osa_BlockPoolCreate   = 0x03f0562b);
PROVIDE(osa_BlockPoolDelete   = 0x03f056b9);
PROVIDE(osa_BufferClone       = 0x03f0575d);
PROVIDE(osa_PoolQuery         = 0x03f055ef);
PROVIDE(osa_PoolCapacity      = 0x03f05783);
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
PROVIDE(btBdhRomConfigureState  = 0x03f06d67);
PROVIDE(btLcoRomComputeContext  = 0x03f1ee8d);
PROVIDE(btBdhRomBuildState      = 0x03f070b7);
PROVIDE(btLscRomScheduleState   = 0x03f31e11);
PROVIDE(btBdhRomCheckState      = 0x03f06d8b);
PROVIDE(btBdhRomValidateState   = 0x03f06dfd);
PROVIDE(btBdhRomParseState      = 0x03f0705d);
PROVIDE(btLdaRomNotifyState     = 0x03f23043);
PROVIDE(btBdhRomDispatchState   = 0x03f06ff3);
PROVIDE(btBdhRomProcessState    = 0x03f06af3);
PROVIDE(btBdhRomUpdateState     = 0x03f06bf1);

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
PROVIDE(romHsmStart             = 0x03f0662f);
PROVIDE(romHsmStateInit         = 0x03f06637);
PROVIDE(romHsmTransition        = 0x03f0664f);
PROVIDE(mdevRomProcessState     = 0x03f062b3);

PROVIDE(romStringCopy   = 0x03f066ed);
PROVIDE(romStringLength = 0x03f0672b);
PROVIDE(romRateIndex     = 0x03f3ab4f);
PROVIDE(romRateValue     = 0x03f3abc5);
PROVIDE(romRateBitrate   = 0x03f3a2ab);
PROVIDE(romLibNotifyEntry = 0x03f3adfb);
PROVIDE(romLibBuildEntry    = 0x03f3ad63);
PROVIDE(romLibComputeEntry  = 0x03f3ad9b);
PROVIDE(romLibApplyEntry    = 0x03f3add1);
PROVIDE(romLibQueryEntry    = 0x03f3ada9);
PROVIDE(romLibResolveEntry  = 0x03f3addf);
PROVIDE(romLibResetEntry    = 0x03f3adb5);
PROVIDE(romLibScheduleEntry = 0x03f3aded);
PROVIDE(romLibSyncEntry     = 0x03f3adc3);
PROVIDE(romEdcaSetCwMin = 0x03f3a62f);
PROVIDE(romEdcaSetCwMax = 0x03f3a643);
PROVIDE(romEdcaSetAifsn = 0x03f3a665);
PROVIDE(romEdcaSetTxop  = 0x03f3a64d);
PROVIDE(romLibApplyState      = 0x03f3a515);
PROVIDE(romLibAcquireState    = 0x03f3a60f);
PROVIDE(romLibDispatchState   = 0x03f3a3b5);
PROVIDE(romLibParseState      = 0x03f3a3ed);
PROVIDE(romLibResolveState    = 0x03f3a521);
PROVIDE(romLibManageState     = 0x03f3a599);
PROVIDE(romLibFormatState     = 0x03f3a55d);
PROVIDE(romLibConfigureContext = 0x03f3a865);
PROVIDE(romLibScheduleState   = 0x03f3a52d);
PROVIDE(romLibResolveContext  = 0x03f3a963);
PROVIDE(romLibAcquireContext  = 0x03f3aa11);
PROVIDE(romLibCheckContext    = 0x03f3a879);
PROVIDE(romLibDispatchContext = 0x03f3a8bb);
PROVIDE(romLibScheduleContext = 0x03f3a96b);
PROVIDE(romLibFormatContext   = 0x03f3a9c5);
PROVIDE(romLibInspectContext  = 0x03f3a9d5);
PROVIDE(romLibTrackContext    = 0x03f3a9e5);
PROVIDE(romLibUpdateContext   = 0x03f3a831);
PROVIDE(romMacStatValidate = 0x03f3acdb);
PROVIDE(romMacStatPrepare  = 0x03f3ace7);
PROVIDE(romMacStatCheck    = 0x03f3accf);
PROVIDE(romMacWakeRequest  = 0x03f3a917);
PROVIDE(romMacSleepNotify  = 0x03f3a907);
PROVIDE(__aeabi_uldivmod   = 0x03f3ec30);
PROVIDE(romMacEntryCheck   = 0x03f3ac3d);
PROVIDE(romMacEntryWrite   = 0x03f3ac8b);
PROVIDE(romMacConfigureEntry = 0x03f3a865);

/* Marvell's MAC timer read (msi_wl_GetMCUCoreTimerTxTSF, named in Marvell's esa sources) */
PROVIDE(msi_wl_GetMCUCoreTimerTxTSF = 0x03f3a6b9);

PROVIDE(romGetMicroseconds = 0x03f3a7cb);
PROVIDE(romLibRecordState  = 0x03f3a76f);
PROVIDE(romLibProcessContext = 0x03f3a7e3);
PROVIDE(romLibNotifyState  = 0x03f3a551);
PROVIDE(romMul64x32        = 0x03f3f3ac);
PROVIDE(romEdcaSetCwMin    = 0x03f3a62f);
PROVIDE(romEdcaSetCwMax    = 0x03f3a643);
PROVIDE(romEdcaSetAifsn    = 0x03f3a665);
PROVIDE(romEdcaSetTxop     = 0x03f3a64d);
PROVIDE(romLibDispatchEntry = 0x03f3acf3);
PROVIDE(romLibParseEntry   = 0x03f3ad03);
PROVIDE(romLibConfigureState = 0x03f3a2d1);
PROVIDE(romLibManageContext = 0x03f3aa0b);
PROVIDE(romMacEntryRead    = 0x03f3acbf);
PROVIDE(romLibHandleState  = 0x03f3a2a5);
PROVIDE(romLibClearContext = 0x03f3aadb);
PROVIDE(romLibUpdateState  = 0x03f3a2b9);
PROVIDE(romLibCheckState   = 0x03f3a347);
PROVIDE(romAesKeySet       = 0x03f3be3f);
PROVIDE(romAesDecryptBlock = 0x03f3be6f);
PROVIDE(romAesEncryptBlock = 0x03f3be7f);
PROVIDE(romLibPrepareState = 0x03f3a36f);
PROVIDE(romLibSelectContext = 0x03f3ab21);
PROVIDE(romLibConvertContext = 0x03f3aaf1);
PROVIDE(romLibVerifyState   = 0x03f3a6b9);
PROVIDE(romLibQueryState    = 0x03f3a4c9);
PROVIDE(romLibResetState    = 0x03f3a4db);
PROVIDE(romLibBuildState    = 0x03f3a417);
PROVIDE(romLibComputeContext = 0x03f3a8ff);
PROVIDE(romLibParseContext  = 0x03f3a8d7);
PROVIDE(romLibBuildContext  = 0x03f3a8e1);
/* 64-bit signed divide (ARM), reached through the veneer at 0x5311c. */
PROVIDE(__aeabi_ldivmod    = 0x03f3eb70);

/* MD5 (ARM entry points), reached through the veneers at 0x531d4, 0x531dc and 0x531e4. */
PROVIDE(wpa_MD5Init   = 0x03f3ae09);
PROVIDE(wpa_MD5Update = 0x03f3b511);
PROVIDE(wpa_MD5Final  = 0x03f3b58d);
/* Signed 32-bit divide, reached through the veneer at 0x52cf4. */
PROVIDE(__aeabi_idiv  = 0x03f3f240);

/* SHA-256 (Thumb), reached through the veneers at 0x531ec and 0x531f4. */
PROVIDE(sha256_vector       = 0x03f059bd);
PROVIDE(hmac_sha256_vector  = 0x03f05a1b);
PROVIDE(btHccRomPrepareState    = 0x03f0f401);
PROVIDE(mdevRomSyncState        = 0x03f06749);
PROVIDE(btBdhRomHandleState     = 0x03f06a25);
PROVIDE(btLtsRomPrepareContext  = 0x03f35513);
PROVIDE(btLdrRomUpdateState     = 0x03f259b9);
PROVIDE(btLcoRomResetContext    = 0x03f1f6e5);
PROVIDE(btLdaRomPrepareState    = 0x03f21d67);
PROVIDE(btLcoRomProcessState    = 0x03f14a65);
PROVIDE(btLdrRomReleaseState    = 0x03f2aa65);
PROVIDE(btLtsRomManageState     = 0x03f33561);
PROVIDE(btLtsRomScheduleState   = 0x03f33143);
PROVIDE(btLcoRomManageContext   = 0x03f2021b);
PROVIDE(btLtsRomQueryState      = 0x03f33001);
PROVIDE(btLtsRomApplyState      = 0x03f330f3);
PROVIDE(btLtsRomResetState      = 0x03f3302d);
PROVIDE(btLcoRomScheduleContext = 0x03f1fbcf);
PROVIDE(btLcoRomNotifyContext   = 0x03f1fcc1);
PROVIDE(btLcoRomSetupContext    = 0x03f20863);
PROVIDE(btLcoRomAcquireContext  = 0x03f2023f);
PROVIDE(btLtsRomUpdateContext   = 0x03f3530d);
PROVIDE(btLcoRomReleaseState    = 0x03f1afe9);
PROVIDE(romStateActiveTable     = 0x03f419e0);
/* The controller's variables in the ROM's DTCM (0x0400a700 - 0x0400fc00); the image only reads and hooks them. */
PROVIDE(btLinkContext                = 0x0400f954);
PROVIDE(btLinkRecord                 = 0x0400f95c);
PROVIDE(btLinkEvent                  = 0x0400f948);
PROVIDE(btLinkStatus                 = 0x0400f8f0);
PROVIDE(btLinkValues                 = 0x0400f960);
PROVIDE(btHostWindow                 = 0x0400c4d4);
PROVIDE(btDeviceProfile              = 0x0400d814);
PROVIDE(btDeviceRecord               = 0x0400dde8);
PROVIDE(btTargetConfig               = 0x0400de50);
PROVIDE(btSlotQueues                 = 0x0400dcdc);
PROVIDE(btSlotQueueIndex             = 0x0400f99c);
PROVIDE(btLinkPacketType             = 0x0400f710);
PROVIDE(btPacketTypes                = 0x0400f968);
PROVIDE(btTargetStatus               = 0x0400de53);
PROVIDE(btTargetNode                 = 0x0400de56);
PROVIDE(btTargetSlot                 = 0x0400de58);
PROVIDE(btDeviceWindow               = 0x0400cd28);
PROVIDE(btDeviceBitmap               = 0x0400cd2c);
PROVIDE(btNextPayload                = 0x0400ed84);
PROVIDE(btPeerPacket                 = 0x0400adaa);
PROVIDE(btPeerFrame                  = 0x0400ae0c);
PROVIDE(btPeerBuffer                 = 0x0400ada0);
PROVIDE(btDevicePolicy               = 0x0400d610);
PROVIDE(btDeviceRate                 = 0x0400d5a0);
PROVIDE(btThresholdPayload           = 0x0400f429);
PROVIDE(btThresholdRegister          = 0x0400f42b);
PROVIDE(btThresholdRecord            = 0x0400f430);
PROVIDE(btModeState                  = 0x0400f431);
PROVIDE(btModeContext                = 0x0400f432);
PROVIDE(btMaskPacket                 = 0x0400f8c0);
PROVIDE(btMaskFrame                  = 0x0400f8c1);
PROVIDE(btPowerHooks                 = 0x0400ebbc);
PROVIDE(btPowerThresholdBitmap       = 0x0400f364);
PROVIDE(btPowerThresholdRate         = 0x0400f368);
PROVIDE(btLinkRate                   = 0x0400f934);
PROVIDE(btLinkMaskContext            = 0x0400f7ff);
PROVIDE(btLinkMaskHeader             = 0x0400f8c8);
PROVIDE(btLinkIndexRecord            = 0x0400f7fd);
PROVIDE(btLinkMaskState              = 0x0400f7fe);
PROVIDE(btDeviceEntry                = 0x0400c7b8);
/* The controller's pools below the image's SQRAM (0xc0001100 - 0xc0006fff), outside its scatter regions. */
PROVIDE(btPoolHeaderMemory           = 0xc00022f4);
PROVIDE(btPoolFrameMemory            = 0xc0001154);
PROVIDE(btPoolDescriptorMemory       = 0xc0003c04);
PROVIDE(btTablePool                  = 0xc0001118);
PROVIDE(btTablePoolConfig            = 0xc0001124);
PROVIDE(btTablePoolControl           = 0xc0005f64);
PROVIDE(btPacketPool                 = 0xc000113c);
PROVIDE(btPacketPoolConfig           = 0xc0001108);
PROVIDE(btPacketPoolControl          = 0xc0005fe4);
PROVIDE(btEventPool                  = 0xc0001148);
PROVIDE(btEventPoolConfig            = 0xc0001110);
PROVIDE(btEventPoolControl           = 0xc0006004);
PROVIDE(btEventPoolFailures          = 0xc0001100);
PROVIDE(btPacketPoolFailures         = 0xc0001104);
PROVIDE(btRecordBuffers              = 0xc0006014);
PROVIDE(btLimitParam                 = 0x0400f154);
PROVIDE(btEntryHooks                = 0x0400eb88);
PROVIDE(btContextBuffers            = 0xc00068e4);
