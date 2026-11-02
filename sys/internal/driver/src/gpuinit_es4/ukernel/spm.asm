/* Master: SPM, TA out of parameter memory (partial render or abort). */

/* SPM event task: TAOutOfMem or SPMAbortComplete. */
	.align2
SPMEventHandler:
	smlsi #0, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	mov r15, #0x00000000
	mov r22, #0x00000000
	and r0, pa0, #0x00000300
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba SPMEventHandler_1
	ldad.bpcache r1, [sa1,+#32], drc0
	mov r2, #0x00000001
	wdf drc0
	imae.u r1, r2.low, #1.low, r1, u32
	stad.bpcache [sa1,+#32], r1
	idf drc0, bif
	wdf drc0
	str #77, #6	/* EUR_CR_EVENT_HOST_CLEAR */
	or r1, sa9, #0x00000000
	mov r2, #0x00000100
	and.testtanz.chan0 !c0, p1, r0, r2
	p1 or r1, r1, #0x01000000
	!p1 and r1, r1, ~#0x01000000
	or sa9, r1, #0x00000000
	mov r15, #0x00001214
	ba TAOutOfMem
SPMEventHandler_1:
	and r0, pa0, #0x00000020
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba SPMEventHandler_2
	mov r0, #0x00001000
	str #77, r0	/* EUR_CR_EVENT_HOST_CLEAR */
	mov r15, #0x0000121B
	ba SPMAbortComplete
SPMEventHandler_2:
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink CommonProgramEnd
	mov pclink, r23
	nop

/* SPM loopback task: TAOutOfMem, SPMAbortComplete or SPMRenderFinished. */
SPMHandler:
	smlsi #0, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	mov r15, #0x00000000
	mov r22, #0x00000000
	ldad.bpcache r0, [sa0,+#56], drc0
	xor r1, pa0, ~#0x00000000
	wdf drc0
	and r0, r0, r1
	stad.bpcache [sa0,+#56], r0
	and r0, pa0, #0x00000060
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba SPMHandler_1
	or r1, sa9, #0x00000000
	and.testtanz.chan0 !c0, p1, r0, #32
	p1 or r1, r1, #0x01000000
	!p1 and r1, r1, ~#0x01000000
	or sa9, r1, #0x00000000
	str #77, #6	/* EUR_CR_EVENT_HOST_CLEAR */
	mov r15, #0x00001233
	ba TAOutOfMem
SPMHandler_1:
	and r0, pa0, #0x00000080
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba SPMHandler_2
	mov r0, #0x00001000
	str #77, r0	/* EUR_CR_EVENT_HOST_CLEAR */
	mov r15, #0x0000123A
	ba SPMAbortComplete
SPMHandler_2:
	and r0, pa0, #0x00100000
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba SPMHandler_3
	mov r15, #0x0000123F
	ba SPMRenderFinished
SPMHandler_3:
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink CommonProgramEnd
	mov pclink, r23

/* Whether the current scene cannot render yet. */
IsSceneBlocked:
	ldad.bpcache r2, [r1,+#69], drc1
	mov r4, #0x00000008
	wdf drc1
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba IsSceneBlocked_2
	imae.u r2, r4.low, #1.low, r2, u32
	ldad.bpcache.fetch2 r3, [r2,+#1], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, r4
	p0 ba IsSceneBlocked_1
	ldad.bpcache r5, [r4,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r5, r3
	p0 ba IsSceneBlocked_1
	nop
	ba IsSceneBlocked_7
IsSceneBlocked_1:
	ldad.bpcache.fetch2 r3, [r2,#0++], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, r4
	p0 ba IsSceneBlocked_2
	ldad.bpcache r5, [r4,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r5, r3
	p0 ba IsSceneBlocked_2
	nop
	ba IsSceneBlocked_7
IsSceneBlocked_2:
	ldad.bpcache r2, [r1,+#103], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba IsSceneBlocked_6
	mov r3, #0x000001A0
	imae.s r3, r3.low, #1.low, r1, u32
	ldad.bpcache.fetch4 r4, [r3,#0++], drc0
IsSceneBlocked_3:
	wdf drc0
	ldad.bpcache r8, [r5,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r8, r4
	p0 ba IsSceneBlocked_4
	nop
	ba IsSceneBlocked_7
IsSceneBlocked_4:
	ldad.bpcache r8, [r7,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r8, r6
	p0 ba IsSceneBlocked_5
	nop
	ba IsSceneBlocked_7
IsSceneBlocked_5:
	isub16.testtaz.chan0 r2, p0, r2, #1
	!p0 ldad.bpcache.fetch4 r4, [r3,#0++], drc0
	!p0 ba IsSceneBlocked_3
IsSceneBlocked_6:
	mov r2, #0x00000000
	lapc
IsSceneBlocked_7:
	mov r2, #0x00000001
	lapc

/* Whether a partial render is not allowed now. */
IsPartialRenderBlocked:
	ldad.bpcache r8, [sa0,+#52], drc0
	wdf drc0
	ldad.bpcache r8, [r8,+#20], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r8, r8
	!p1 ba IsPartialRenderBlocked_1
	mov r0, #0x00000000
	lapc
IsPartialRenderBlocked_1:
	mov r0, #0x00000001
	lapc

/* TA out of parameter memory: grow the free list, partial render, or abort. */
TAOutOfMem:
	or r0, sa9, #0x00000000
	or r0, r0, #0x00000800
	or sa9, r0, #0x00000000
	or r5, sa10, #0x00000000
	and.testtanz.chan0 !c0, p0, r5, #1
	p0 ba TAOutOfMem_1
	mov r5, pclink
	ba.savelink IsPartialRenderBlocked
	mov pclink, r5
	and.testtaz.chan0 !c0, p0, r0, r0
	p0 ba TAOutOfMem_1
	or r5, sa10, #0x00000000
	or r5, r5, #0x00000040
	or r5, r5, #0x00004000
	or sa10, r5, #0x00000000
	or r5, sa9, #0x00000000
	or r5, r5, #0x00000040
	or sa9, r5, #0x00000000
	mov r23, pclink
	ba.savelink Check3DQueues
	mov pclink, r23
	ba TAOutOfMem_14
TAOutOfMem_1:
	or r0, sa9, #0x00000000
	mov r2, #0x20000000
	and.testtaz.chan0 !c0, p0, r0, r2
	p0 ba TAOutOfMem_4
	mov r2, #0x40000000
	and.testtanz.chan0 !c0, p0, r0, r2
	p0 ba TAOutOfMem_4
	or r0, r0, #0x40000000
	or sa9, r0, #0x00000000
	ldaw.bpcache r2, [sa0,+#106], drc0
	wdf drc0
	str #393, r2	/* EUR_CR_DPM_ZLS_PAGE_THRESHOLD */
	ldad.bpcache r1, [sa0,+#77], drc0
	ldad.bpcache r2, [sa0,+#78], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r1, r2
	!p0 ba TAOutOfMem_2
	mov r16, #0x00000001
	mov r23, pclink
	ba.savelink IdleCore
	mov pclink, r23
	nop
TAOutOfMem_2:
	mov r23, pclink
	ba.savelink DPMPauseAllocations
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink DPMReloadFreeList
	mov pclink, r23
	ldr r3, #669, drc0	/* EUR_CR_DMS_CTRL */
	wdf drc0
	ldad.bpcache r5, [sa0,+#49], drc0
	wdf drc0
	ldad r4, [r5,+#7], drc1
	wdf drc1
	and r3, r3, ~#0x3C000000
	or r3, r3, r4
	str #669, r3	/* EUR_CR_DMS_CTRL */
	mov r23, pclink
	ba.savelink DPMResumeAllocations
	mov pclink, r23
	xor.testtaz.chan0 !c0, p0, r1, r2
	!p0 ba TAOutOfMem_3
	or r1, r16, #0x00000000
	mov r23, pclink
	ba.savelink Resume
	mov pclink, r23
	or r16, r1, #0x00000000
TAOutOfMem_3:
	str #423, #1	/* EUR_CR_DPM_OUTOFMEM */
	ba TAOutOfMem_14
TAOutOfMem_4:
	or r0, r0, #0x00000800
	or sa9, r0, #0x00000000
	and.testtanz.chan0 !c0, p0, r0, #32
	p0 ba TAOutOfMem_14
	stad.bpcache [sa0,+#87], #0
	ldad.bpcache r1, [sa0,+#51], drc0
	wdf drc0
	ldad.bpcache r2, [r1,+#67], drc0
	wdf drc0
	ldad.bpcache r3, [r2,+#3], drc0
	wdf drc0
	mov r4, #0x00000001
	imae.u r3, r4.low, #1.low, r3, u32
	stad.bpcache [r2,+#3], r3
	idf drc0, bif
	wdf drc0
	ldad.bpcache r14, [sa0,+#50], drc0
	wdf drc0
	ldad.bpcache r2, [r14,+#32], drc0
	wdf drc0
	or r2, r2, #0x00000080
	stad.bpcache [r14,+#32], r2
	idf drc0, bif
	wdf drc0
	or r2, sa10, #0x00000000
	and.testtaz.chan0 !c0, p0, r2, #1
	p0 ba TAOutOfMem_5
	or r2, r2, #0x00000040
	or sa10, r2, #0x00000000
	ba TAOutOfMem_14
TAOutOfMem_5:
	wdf drc0
	ba.savelink IsSceneBlocked
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba TAOutOfMem_6
	or r0, r0, #0x00000040
	or sa9, r0, #0x00000000
	mov r23, pclink
	ba.savelink Check2DQueue
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink Check3DQueues
	mov pclink, r23
	ba TAOutOfMem_14
TAOutOfMem_6:
	and r0, r0, ~#0x00000040
	shl.testpat.chan0 !c0, p0, r0, #24
	p0 ba TAOutOfMem_11
	shl.testnat.chan0 !c0, p0, r0, #22
	p0 ba TAOutOfMem_11
	shl.testnat.chan0 !c0, p0, r0, #23
	p0 ba TAOutOfMem_7
	ldad.bpcache r2, [sa0,+#60], drc0
	ldr r3, #457, drc0	/* EUR_CR_DPM_PAGE_STATUS */
	wdf drc0
	and r3, r3, #0xFFFF0000
	shr r3, r3, #0x00000010
	isub16.testpat.chan0 !c0, p0, r2, r3
	ba TAOutOfMem_8
TAOutOfMem_7:
	ldad.bpcache r2, [sa0,+#61], drc0
	ldr r3, #459, drc0	/* EUR_CR_DPM_GLOBAL_PAGE_STATUS */
	wdf drc0
	and r3, r3, #0xFFFF0000
	shr r3, r3, #0x00000010
	isub16.testpat.chan0 !c0, p0, r2, r3
TAOutOfMem_8:
	p0 ba TAOutOfMem_10
	or r0, r0, #0x00000200
	and.testtanz.chan0 !c0, p0, r0, #4
	p0 ba TAOutOfMem_9
	ldr r1, #134, drc0	/* EUR_CR_TE_PSG */
	wdf drc0
	or r1, r1, #0x00500000
	str #134, r1	/* EUR_CR_TE_PSG */
TAOutOfMem_9:
	mov r23, pclink
	ba.savelink DPMPauseAllocations
	mov pclink, r23
	ldr r1, #393, drc1	/* EUR_CR_DPM_ZLS_PAGE_THRESHOLD */
	mov r2, #0x00000020
	wdf drc1
	imae.u r1, r2.low, #1.low, r1, u32
	str #393, r1	/* EUR_CR_DPM_ZLS_PAGE_THRESHOLD */
	stad.bpcache [sa0,+#63], r2
	ldr r2, #394, drc0	/* EUR_CR_DPM_TA_GLOBAL_LIST */
	wdf drc0
	stad.bpcache [sa0,+#62], r2
	and r2, r2, #0x0000FFFF
	imae.u r2, r2.low, #1.low, #16, u32
	or r2, r2, #0x00010000
	str #394, r2	/* EUR_CR_DPM_TA_GLOBAL_LIST */
	idf drc0, bif
	wdf drc0
	mov r23, pclink
	ba.savelink DPMReloadFreeList
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink DPMResumeAllocations
	mov pclink, r23
	mov r2, #0x0000119C
	ldr r2, r2, drc0
	wdf drc0
	or r2, r2, #0x00000002
	mov r5, #0x0000119C
	str r5, r2
	mov r2, #0x00001200
	ldr r2, r2, drc0
	wdf drc0
	and r2, r2, ~#0x00001FFF
	or r2, r2, #0x0000000F
	mov r5, #0x00001200
	str r5, r2
	or sa9, r0, #0x00000000
	ldr r5, #456, drc0	/* EUR_CR_DPM_ABORT_STATUS_MTILE */
	wdf drc0
	and r5, r5, #0x000000E0
	shr r5, r5, #0x00000005
	imae.u r1, r5.low, #1.low, #2, u32
	shl r1, r1, #0x0000000C
	or r1, r1, #0x000001A7
	str r1, #1
	str #423, #1	/* EUR_CR_DPM_OUTOFMEM */
	ba TAOutOfMem_14
TAOutOfMem_10:
	and r0, r0, ~#0x00000180
TAOutOfMem_11:
	ldr r1, #456, drc0	/* EUR_CR_DPM_ABORT_STATUS_MTILE */
	wdf drc0
	and r1, r1, #0x000000E0
	shr r1, r1, #0x00000005
	imae.u r4, r1.low, #1.low, #2, u32
	shl r4, r4, #0x0000000C
	or r4, r4, #0x00000093
	str r4, #1
	mov r4, #0x00100000
	ldr r3, #456, drc0	/* EUR_CR_DPM_ABORT_STATUS_MTILE */
	wdf drc0
	and r3, r3, #0x000000E0
	shr r3, r3, #0x00000005
TAOutOfMem_12:
	imae.u r2, r3.low, #1.low, #2, u32
	shl r2, r2, #0x0000000C
	or r2, r2, #0x0000004B
	ldr r2, r2, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r4
	p0 ba TAOutOfMem_12
	mov r4, #0x00100000
	str #77, r4	/* EUR_CR_EVENT_HOST_CLEAR */
	or r0, r0, #0x00000020
	ldr r1, #456, drc0	/* EUR_CR_DPM_ABORT_STATUS_MTILE */
	wdf drc0
	shl.testnat.chan0 !c0, p0, r0, #7
	p0 and r1, r1, ~#0x0000000F
	p0 or r1, r1, #0x00000010
	p0 and r0, r0, ~#0x01000000
	stad.bpcache [sa0,+#65], r1
	idf drc0, bif
	wdf drc0
	and.testtanz.chan0 !c0, p0, r1, #16
	!p0 shl.testnat.chan0 !c0, p0, r0, #22
	or r0, r0, #0x00000002
	mov r2, #0x00000004
	mov r3, #0x00000004
	mov r4, #0x00000010
	ldr r5, #459, drc0	/* EUR_CR_DPM_GLOBAL_PAGE_STATUS */
	wdf drc0
	and r5, r5, #0xFFFF0000
	shr r5, r5, #0x00000010
	isub16.testnat.chan0 r5, p0, r5, #2
	!p0 and r5, r5, #0x0000FFFF
	p0 mov r5, #0x00000000
	stad.bpcache [sa0,+#61], r5
	and r1, r1, #0x000000E0
	shr r1, r1, #0x00000005
	imae.u r5, r1.low, #1.low, #2, u32
	shl r5, r5, #0x0000000C
	or r5, r5, #0x000001C8
	str r5, r4
	imae.u r5, r1.low, #1.low, #2, u32
	shl r5, r5, #0x0000000C
	or r5, r5, #0x000001A7
	str r5, r2
TAOutOfMem_13:
	imae.u r2, r1.low, #1.low, #2, u32
	shl r2, r2, #0x0000000C
	or r2, r2, #0x00000098
	ldr r2, r2, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, #1
	p0 ba TAOutOfMem_13
	imae.u r2, r1.low, #1.low, #2, u32
	shl r2, r2, #0x0000000C
	or r2, r2, #0x00000096
	ldr r2, r2, drc0
	imae.u r4, r1.low, #1.low, #2, u32
	shl r4, r4, #0x0000000C
	or r4, r4, #0x00000097
	ldr r4, r4, drc0
	wdf drc0
	mov r5, #0x00001096
	str r5, r2
	mov r5, #0x00001097
	str r5, r4
	mov r5, #0x000011A7
	str r5, r3
	ldr r3, #457, drc0	/* EUR_CR_DPM_PAGE_STATUS */
	wdf drc0
	and r3, r3, #0xFFFF0000
	shr r3, r3, #0x00000010
	ima16.s r3, r3, #1, #2.neg, u16, u16
	and r3, r3, #0x0000FFFF
	stad.bpcache [sa0,+#60], r3
	ldr r1, #669, drc0	/* EUR_CR_DMS_CTRL */
	wdf drc0
	or r1, r1, #0x00000001
	str #669, r1	/* EUR_CR_DMS_CTRL */
	or sa9, r0, #0x00000000
	idf drc1, bif
	wdf drc1
TAOutOfMem_14:
	mov pclink, r15
	nop
	lapc

/* TA abort done: render what was binned, restart the TA. */
SPMAbortComplete:
	or r0, sa9, #0x00000000
	shl.testpat.chan0 !c0, p0, r0, #19
	!p0 shl.testnat.chan0 !c0, p0, r0, #11
	p0 ba SPMAbortComplete_1
	nop
	ba SPMAbortComplete_29
SPMAbortComplete_1:
	or r0, r0, #0x00001000
	or sa9, r0, #0x00000000
	or r0, sa10, #0x00000000
	and.testtaz.chan0 !c0, p0, r0, #1
	p0 ba SPMAbortComplete_2
	or r0, sa9, #0x00000000
	or r0, r0, #0x00100000
	or sa9, r0, #0x00000000
	or r0, sa10, #0x00000000
	and r0, r0, ~#0x00000040
	or sa10, r0, #0x00000000
	ba SPMAbortComplete_29
SPMAbortComplete_2:
	ldad.bpcache r0, [sa0,+#51], drc0
	ldad.bpcache r14, [sa0,+#50], drc1
	wdf drc0
	wdf drc1
	ldad.bpcache r1, [r0,+#67], drc0
	wdf drc0
	ldad.bpcache r2, [r1,+#4], drc0
	wdf drc0
	mov r3, #0x00000001
	imae.u r2, r3.low, #1.low, r2, u32
	stad.bpcache [r1,+#4], r2
	idf drc0, bif
	wdf drc0
	ldad.bpcache r3, [r0,+#92], drc0
	wdf drc0
	stad.bpcache [sa0,+#67], r3
	ldad.bpcache r3, [r0,+#93], drc0
	wdf drc0
	stad.bpcache [sa0,+#69], r3
SPMAbortComplete_3:
	ldr r1, #577, drc0	/* EUR_CR_MTE_MEM_CHECKSUM */
	ldr r2, #578, drc0	/* EUR_CR_MTE_TE_CHECKSUM */
	ldr r3, #579, drc0	/* EUR_CR_TE_CHECKSUM */
	wdf drc0
	xor r4, r1, r2
	xor r4, r4, r3
	mov r2, #0x000009C4
SPMAbortComplete_4:
	isub16.testpat.chan0 r2, p0, r2, #1
	p0 ba SPMAbortComplete_4
	ldr r1, #577, drc0	/* EUR_CR_MTE_MEM_CHECKSUM */
	ldr r2, #578, drc0	/* EUR_CR_MTE_TE_CHECKSUM */
	ldr r3, #579, drc0	/* EUR_CR_TE_CHECKSUM */
	wdf drc0
	xor r1, r1, r2
	xor r1, r1, r3
	xor.testtanz.chan0 !c0, p0, r1, r4
	p0 ba SPMAbortComplete_3
	ldad.bpcache r1, [sa0,+#65], drc0
	wdf drc0
	shr r1, r1, #0x00000005
	mov r2, #0x00001000
	imae.u r3, r1.low, #1.low, #2, u32
	shl r3, r3, #0x0000000C
	or r3, r3, #0x0000004B
	str r3, r2
	mov r1, #0x00001000
	str #77, r1	/* EUR_CR_EVENT_HOST_CLEAR */
	mov r1, #0x000000FA
SPMAbortComplete_5:
	isub16.testpat.chan0 r1, p0, r1, #1
	p0 ba SPMAbortComplete_5
	mov r24, #0x00000001
	mov r25, #0x00000020
	mov r26, #0x00000040
	mov r23, pclink
	ba.savelink JumpToSupervisor
	mov pclink, r23
	mov r24, #0x00000001
	mov r25, #0x00000020
	mov r26, #0x00000000
	mov r23, pclink
	ba.savelink JumpToSupervisor
	mov pclink, r23
	ldr r1, #396, drc0	/* EUR_CR_DPM_STATE_CONTEXT_ID */
	wdf drc0
	and r1, r1, ~#0x00000003
	and r2, r1, #0x00000004
	shr r2, r2, #0x00000002
	xor r3, r2, #0x00000001
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ldad.bpcache r10, [sa0,+#43], drc0
	!p0 ldad.bpcache r10, [sa0,+#44], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r10, r10
	p1 ba SPMAbortComplete_6
	ldad.bpcache r4, [r10,+#0], drc0
	wdf drc0
	or r16, r4, #0x00000000
	mov r17, #0x00000001
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	or r16, r10, #0x00000000
	or r17, r3, #0x00000000
	mov r18, #0x00000200
	mov r23, pclink
	ba.savelink StoreDPMContext
	mov pclink, r23
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 stad.bpcache [sa0,+#43], #0
	!p0 stad.bpcache [sa0,+#44], #0
	idf drc1, bif
	wdf drc1
SPMAbortComplete_6:
	shl r3, r3, #0x00000001
	shl r2, r2, #0x00000000
	or r1, r1, r3
	or r1, r1, r2
	and r1, r1, ~#0x00000008
	str #396, r1	/* EUR_CR_DPM_STATE_CONTEXT_ID */
	ldad.bpcache r1, [r14,+#16], drc0
	wdf drc0
	str #395, r1	/* EUR_CR_DPM_STATE_TABLE_BASE */
	ldr r11, #797, drc0	/* EUR_CR_BIF_BANK_SET */
	wdf drc0
	and r11, r11, ~#0x00000200
	str #797, r11	/* EUR_CR_BIF_BANK_SET */
	or r11, sa9, #0x00000000
	and.testtanz.chan0 !c0, p1, r11, #2
	p1 ba SPMAbortComplete_7
	str #407, #1	/* EUR_CR_DPM_LSS_PARTIAL_CONTEXT */
SPMAbortComplete_7:
	mov r23, pclink
	ba.savelink DPMStateStore
	mov pclink, r23
	mov r23, pclink
	ba.savelink DPMStateClear
	mov pclink, r23
	ldr r1, #396, drc0	/* EUR_CR_DPM_STATE_CONTEXT_ID */
	wdf drc0
	xor r1, r1, #0x00000001
	str #396, r1	/* EUR_CR_DPM_STATE_CONTEXT_ID */
	mov r23, pclink
	ba.savelink DPMStateLoad
	mov pclink, r23
	ldr r1, #396, drc0	/* EUR_CR_DPM_STATE_CONTEXT_ID */
	wdf drc0
	xor r1, r1, #0x00000001
	str #396, r1	/* EUR_CR_DPM_STATE_CONTEXT_ID */
	str #407, #0	/* EUR_CR_DPM_LSS_PARTIAL_CONTEXT */
	ldad.bpcache r3, [r14,+#0], drc0
	wdf drc0
	ldad.bpcache r3, [r14,+#0], drc0
	wdf drc0
	or r16, r3, #0x00000000
	mov r17, #0x00000001
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	ldad.bpcache r2, [sa0,+#78], drc0
	ldad.bpcache r3, [r3,+#6], drc0
	ldad.bpcache r4, [sa0,+#77], drc0
	wdf drc0
	mov r16, #0x00001200
	ldr r16, r16, drc0
	wdf drc0
	or r16, r16, #0x80000000
	mov r17, #0x00001200
	str r17, r16
	xor.testtaz.chan0 !c0, p0, r2, r3
	p0 ba SPMAbortComplete_10
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba SPMAbortComplete_8
	xor.testtaz.chan0 !c0, p0, r2, r4
	p0 ba SPMAbortComplete_8
	mov r23, pclink
	ba.savelink Store3DPB
	mov pclink, r23
SPMAbortComplete_8:
	xor.testtanz.chan0 !c0, p0, r3, r4
	p0 ba SPMAbortComplete_9
	mov r23, pclink
	ba.savelink StoreTAPB
	mov pclink, r23
SPMAbortComplete_9:
	or r16, r3, #0x00000000
	mov r23, pclink
	ba.savelink Load3DPB
	mov pclink, r23
SPMAbortComplete_10:
	or r1, sa9, #0x00000000
	and.testtanz.chan0 !c0, p0, r1, #2
	mov r2, #0x00000002
	p0 or r2, r2, #0x00000001
	str #399, r2	/* EUR_CR_DPM_3D_DEALLOCATE */
	shl.testnat.chan0 !c0, p0, r1, #21
	p0 ba SPMAbortComplete_11
	str #406, #1	/* EUR_CR_DPM_PARTIAL_RENDER */
SPMAbortComplete_11:
	mov r1, #0x00000000
	imae.s r2, r1.low, #1.low, r0, u32
	mov r3, #0x00002130
	or r4, sa38, #0x00000000
SPMAbortComplete_12:
	ldad.bpcache r5, [r2,#1++], drc0
	wdf drc0
	str r3, r5
	isub16.testtaz.chan0 r4, p0, r4, #1
	p0 ba SPMAbortComplete_13
	mov r5, #0x00001000
	imae.u r3, r5.low, #1.low, r3, u32
	ba SPMAbortComplete_12
SPMAbortComplete_13:
	or r4, sa38, #0x00000000
	mov r3, #0x00000004
	ima16.s r3, r3, #1, r4.neg, u16, u16
	imae.u r2, r3.low, #4.low, r2, u32
	ldad.fetch6 r3, [r2,#0++], drc0
	str #259, #0	/* EUR_CR_ISP_RENDBOX1 */
	str #260, #0	/* EUR_CR_ISP_RENDBOX2 */
	str #257, #0	/* EUR_CR_ISP_RENDER */
	wdf drc0
	str #811, r3	/* EUR_CR_BIF_3D_REQ_BASE */
	str #812, r4	/* EUR_CR_BIF_ZLS_REQ_BASE */
	str #256, r5	/* EUR_CR_PIXELBE */
	str #261, r6	/* EUR_CR_ISP_IPFMISC */
	str #267, r7	/* EUR_CR_3D_AA_MODE */
	str #265, r8	/* EUR_CR_ISP_DBIAS */
	ldad r3, [r2,#1++], drc0
	wdf drc0
	str #275, r3	/* EUR_CR_4Kx4K_RENDER */
	ldad.bpcache r5, [r2,#1++], drc0
	wdf drc0
	str #258, r5	/* EUR_CR_ISP_RGN_BASE */
	mov r3, #0x00001144
	or r4, sa38, #0x00000000
SPMAbortComplete_14:
	isub16.testtaz.chan0 r4, p2, r4, #1
	p2 ba SPMAbortComplete_15
	ldad.bpcache r5, [r2,#1++], drc0
	wdf drc0
	str r3, r5
	mov r5, #0x00000001
	imae.u r3, r5.low, #1.low, r3, u32
	ba SPMAbortComplete_14
SPMAbortComplete_15:
	or r5, sa38, #0x00000000
	mov r3, #0x00000004
	ima16.s r3, r3, #1, r5.neg, u16, u16
	imae.u r2, r3.low, #4.low, r2, u32
	ldad r3, [r2,#1++], drc0
	wdf drc0
	mov r7, #0x00001147
	str r7, r3
	ldad.fetch3 r3, [r2,#0++], drc0
	wdf drc0
	str #330, r3	/* EUR_CR_ISP_MTILE */
	str #328, r4	/* EUR_CR_ISP_MTILE1 */
	str #329, r5	/* EUR_CR_ISP_MTILE2 */
	ldad r3, [r2,#1++], drc0
	wdf drc0
	str #82, r3	/* EUR_CR_DOUBLE_PIXEL_PARTITIONS */
	ldad.fetch3 r3, [r2,#0++], drc0
	wdf drc0
	str #262, r3	/* EUR_CR_ISP_OGL_MODE */
	str #263, r4	/* EUR_CR_ISP_PERPENDICULAR */
	str #264, r5	/* EUR_CR_ISP_CULLVALUE */
	mov r3, #0x00002143
	or r4, sa38, #0x00000000
SPMAbortComplete_16:
	ldad.bpcache r5, [r2,#1++], drc0
	wdf drc0
	str r3, r5
	isub16.testtaz.chan0 r4, p0, r4, #1
	p0 ba SPMAbortComplete_17
	mov r5, #0x00001000
	imae.u r3, r5.low, #1.low, r3, u32
	ba SPMAbortComplete_16
SPMAbortComplete_17:
	or r4, sa38, #0x00000000
	mov r3, #0x00000004
	ima16.s r3, r3, #1, r4.neg, u16, u16
	imae.u r2, r3.low, #4.low, r2, u32
	ldad.fetch8 r3, [r2,#0++], drc0
	ldad.bpcache r12, [r14,+#31], drc1
	wdf drc1
	ldad.bpcache r12, [r12,+#0], drc1
	wdf drc1
	and.testtaz.chan0 !c0, p0, r12, #2
	wdf drc0
	p0 ba SPMAbortComplete_18
	and r12, r3, #0x07000000
	and r13, r3, #0x000F0000
	shr r12, r12, #0x00000003
	shr r13, r13, #0x00000004
	and r3, r3, ~#0x00E0F000
	or r12, r12, r13
	or r3, r3, r12
	or r4, r5, #0x00000000
	or r6, r7, #0x00000000
SPMAbortComplete_18:
	str #288, r3	/* EUR_CR_ISP_ZLSCTL */
	str #289, r4	/* EUR_CR_ISP_ZLOAD_BASE */
	str #290, r5	/* EUR_CR_ISP_ZSTORE_BASE */
	str #291, r6	/* EUR_CR_ISP_STENCIL_LOAD_BASE */
	str #292, r7	/* EUR_CR_ISP_STENCIL_STORE_BASE */
	str #302, r8	/* EUR_CR_ISP_BGOBJDEPTH */
	str #303, r9	/* EUR_CR_ISP_BGOBJ */
	str #305, r10	/* EUR_CR_ISP_BGOBJTAG */
	ldad r3, [r2,#1++], drc0
	wdf drc0
	str #307, r3	/* EUR_CR_ISP_MULTISAMPLECTL2 */
	ldad.fetch8 r3, [r2,#0++], drc0
	wdf drc0
	str #306, r3	/* EUR_CR_ISP_MULTISAMPLECTL */
	str #311, r4	/* EUR_CR_ISP_TAGCTRL */
	str #512, r5	/* EUR_CR_TSP_CONFIG */
	str #663, r6	/* EUR_CR_EVENT_PIXEL_PDS_EXEC */
	str #664, r7	/* EUR_CR_EVENT_PIXEL_PDS_DATA */
	str #665, r8	/* EUR_CR_EVENT_PIXEL_PDS_INFO */
	str #673, r9	/* EUR_CR_USE_FILTER0_LEFT */
	str #674, r10	/* EUR_CR_USE_FILTER0_RIGHT */
	ldad.fetch8 r3, [r2,#0++], drc0
	wdf drc0
	str #675, r3	/* EUR_CR_USE_FILTER0_EXTRA */
	str #676, r4	/* EUR_CR_USE_FILTER1_LEFT */
	str #677, r5	/* EUR_CR_USE_FILTER1_RIGHT */
	str #678, r6	/* EUR_CR_USE_FILTER1_EXTRA */
	str #712, r7	/* EUR_CR_USE_FILTER2_LEFT */
	str #713, r8	/* EUR_CR_USE_FILTER2_RIGHT */
	str #714, r9	/* EUR_CR_USE_FILTER2_EXTRA */
	str #679, r10	/* EUR_CR_USE_FILTER_TABLE */
	ldad.fetch8 r3, [r2,#0++], drc0
	wdf drc0
	str #519, r3	/* EUR_CR_TPU_YUV00 */
	str #520, r4	/* EUR_CR_TPU_YUV01 */
	str #521, r5	/* EUR_CR_TPU_YUV02 */
	str #522, r6	/* EUR_CR_TPU_YUV03 */
	str #525, r7	/* EUR_CR_TPU_YUV10 */
	str #526, r8	/* EUR_CR_TPU_YUV11 */
	str #527, r9	/* EUR_CR_TPU_YUV12 */
	str #528, r10	/* EUR_CR_TPU_YUV13 */
	ldad.fetch2 r3, [r2,#0++], drc0
	wdf drc0
	str #523, r3	/* EUR_CR_TPU_YUV04 */
	str #529, r4	/* EUR_CR_TPU_YUV14 */
	ldad.fetch2 r3, [r2,#0++], drc0
	wdf drc0
	str #524, r3	/* EUR_CR_TPU_YUV05 */
	str #530, r4	/* EUR_CR_TPU_YUV15 */
	ldad r3, [r2,#1++], drc0
	wdf drc0
	or r4, sa38, #0x00000000
SPMAbortComplete_19:
	ima32.s1.s r4, r4, #1, #1.neg
	imae.u r5, r4.low, #1.low, #2, u32
	shl r5, r5, #0x0000000C
	or r5, r5, #0x000002BA
	str r5, r3
	and.testtanz.chan0 !c0, p0, r4, r4
	p0 ba SPMAbortComplete_19
	ldr r1, #288, drc0	/* EUR_CR_ISP_ZLSCTL */
	wdf drc0
	and r3, r1, #0x00E00000
	mov r2, #0x00600000
	xor.testtaz.chan0 !c0, p0, r3, r2
	!p0 mov r2, #0x00800000
	!p0 xor.testtaz.chan0 !c0, p0, r3, r2
	p0 mov r2, #0x00100000
	p0 and.testtaz.chan0 !c0, p0, r1, r2
	p0 ba SPMAbortComplete_22
	ldad.bpcache r3, [r0,+#66], drc0
	mov r4, #0x00000180
	wdf drc0
	and.testtanz.chan0 !c0, p0, r3, r4
	p0 ba SPMAbortComplete_20
	mov r4, #0xFFF99FF9
	and r3, r1, r4
	str #288, r3	/* EUR_CR_ISP_ZLSCTL */
	ba SPMAbortComplete_22
SPMAbortComplete_20:
	or r2, r1, #0x00000000
	and.testtaz.chan0 !c0, p0, r2, #2
	p0 ba SPMAbortComplete_21
	ldr r3, #261, drc0	/* EUR_CR_ISP_IPFMISC */
	wdf drc0
	or r3, r3, #0x00000100
	str #261, r3	/* EUR_CR_ISP_IPFMISC */
SPMAbortComplete_21:
	ba SPMAbortComplete_22
SPMAbortComplete_22:
	ldad.bpcache r4, [r0,+#66], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, #16
	p0 ba SPMAbortComplete_23
	ldr r3, #288, drc0	/* EUR_CR_ISP_ZLSCTL */
	wdf drc0
	or r3, r3, #0x00000001
	str #288, r3	/* EUR_CR_ISP_ZLSCTL */
SPMAbortComplete_23:
	ldr r3, #669, drc0	/* EUR_CR_DMS_CTRL */
	ldad.bpcache r4, [r0,+#91], drc1
	wdf drc0
	and r3, r3, ~#0x03C00000
	wdf drc1
	or r3, r3, r4
	str #669, r3	/* EUR_CR_DMS_CTRL */
	mov r16, #0x00000001
	mov r17, #0x00000010
	mov r23, pclink
	ba.savelink InvalidateSLC
	mov pclink, r23
	and.testtanz.chan0 !c0, p0, r22, #64
	p0 ba SPMAbortComplete_24
	str #642, #1	/* EUR_CR_USE_CACHE */
	or r22, r22, #0x00000040
SPMAbortComplete_24:
	str #693, #1	/* EUR_CR_PDS_INV1 */
	str #696, #1	/* EUR_CR_PDS_INV_CSC */
	str #271, #1	/* EUR_CR_TSP_PARAMETER_CACHE */
SPMAbortComplete_25:
	ldr r1, #78, drc0	/* EUR_CR_PDS_CACHE_STATUS */
	wdf drc0
	and r1, r1, #0x00000005
	xor.testtanz.chan0 !c0, p0, r1, #5
	p0 ba SPMAbortComplete_25
	str #80, #5	/* EUR_CR_PDS_CACHE_HOST_CLEAR */
	and.testtanz.chan0 !c0, p1, r11, #2
	p1 ba SPMAbortComplete_27
	ldad.bpcache r1, [sa0,+#65], drc0
	wdf drc0
	and.testtaz.chan0 r1, p0, r1, #15
	p0 ba SPMAbortComplete_28
	ldad.bpcache r2, [r14,+#4], drc1
	mov r3, #0x0000000C
	imae.u r1, r1.low, r3.low, #0, u32
	shl r1, r1, #0x00000004
	ldr r3, #258, drc1	/* EUR_CR_ISP_RGN_BASE */
	wdf drc1
	str #323, r3	/* EUR_CR_ISP_ZLS_EXTZ_RGN_BASE */
	mov r5, #0x00001102
	ldr r3, r5, drc1
	wdf drc1
	imae.u r3, r1.low, r2.low, r3, u32
	str r5, r3
	mov r5, #0x00001144
	or r4, sa38, #0x00000000
SPMAbortComplete_26:
	isub16.testtaz.chan0 r4, p0, r4, #1
	p0 ba SPMAbortComplete_28
	ldr r3, r5, drc1
	wdf drc1
	imae.u r3, r1.low, r2.low, r3, u32
	str r5, r3
	ima32.s1.u r5, r5, #1, #1
	ba SPMAbortComplete_26
SPMAbortComplete_27:
	ldr r3, #258, drc0	/* EUR_CR_ISP_RGN_BASE */
	wdf drc0
	str #323, r3	/* EUR_CR_ISP_ZLS_EXTZ_RGN_BASE */
SPMAbortComplete_28:
	mov r16, #0x00000001
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	ldad.bpcache r4, [r14,+#15], drc0
	ldad.bpcache.fetch3 r1, [r0,+#86], drc0
	wdf drc0
	stad.bpcache.fetch3 [r4,#0++], r1
	ldad.bpcache r1, [r0,+#90], drc0
	wdf drc0
	and r2, r1, #0xFFFF0000
	or r2, r2, #0x00004000
	stad.bpcache [r4,+#10], r2
	and r2, r1, #0x0000FFFF
	or r2, r2, #0x40000000
	stad.bpcache [r4,+#12], r2
	idf drc0, bif
	wdf drc0
	mov r16, #0x00000002
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	ldr r10, #458, drc0	/* EUR_CR_DPM_PAGE */
	wdf drc0
	and r10, r10, #0x0000FFFF
	shr r10, r10, #0x00000000
	ldr r1, #669, drc0	/* EUR_CR_DMS_CTRL */
	wdf drc0
	or r1, r1, #0x00000001
	str #669, r1	/* EUR_CR_DMS_CTRL */
	mov r23, pclink
	ba.savelink USEClkGateOn
	mov pclink, r23
	str #266, #1	/* EUR_CR_ISP_START_RENDER */
	ldad.bpcache r0, [sa0,+#67], drc0
	wdf drc0
	or r0, r0, #0x12000000
	mov r30, #0x08001008
	mov.nosched r31, #0x08002000
	stad [r30,+#0], r0
	stad [r31,+#0], #0
	idf drc0, bif
	wdf drc0
	or r0, sa10, #0x00000000
	or r1, sa9, #0x00000000
	shl.testnat.chan0 !c0, p0, r1, #21
	p0 or r0, r0, #0x00000100
	or r0, r0, #0x00000033
	or sa10, r0, #0x00000000
	stad.bpcache [sa0,+#54], r14
	idf drc0, bif
	wdf drc0
SPMAbortComplete_29:
	mov pclink, r15
	nop
	lapc

/* Partial render done: return its memory, let the TA continue. */
SPMRenderFinished:
	or r10, sa10, #0x00000000
	mov r11, #0xFFFFFFFD
	and r10, r10, r11
	or sa10, r10, #0x00000000
	mov r16, #0x00001200
	ldr r16, r16, drc0
	wdf drc0
	and r16, r16, ~#0x80000000
	mov r17, #0x00001200
	str r17, r16
	or r10, sa9, #0x00000000
	and r10, r10, ~#0x00001800
	or sa9, r10, #0x00000000
	or r1, sa10, #0x00000000
	shl.testnat.chan0 !c0, p1, r1, #23
	!p1 ba SPMRenderFinished_1
	and r1, r1, ~#0x00000100
	or sa10, r1, #0x00000000
	ba SPMRenderFinished_3
SPMRenderFinished_1:
	ldad.bpcache r2, [sa0,+#60], drc0
	ldr r3, #457, drc0	/* EUR_CR_DPM_PAGE_STATUS */
	wdf drc0
	and r3, r3, #0xFFFF0000
	shr r3, r3, #0x00000010
	isub16.testnat.chan0 !c0, p0, r2, r3
	p0 ba SPMRenderFinished_2
	and.testtaz.chan0 !c0, p0, r10, #2
	p0 ba SPMRenderFinished_3
	ldad.bpcache r2, [sa0,+#61], drc0
	ldr r3, #459, drc0	/* EUR_CR_DPM_GLOBAL_PAGE_STATUS */
	wdf drc0
	and r3, r3, #0xFFFF0000
	shr r3, r3, #0x00000010
	isub16.testnat.chan0 !c0, p0, r2, r3
	p0 or r10, r10, #0x00000100
SPMRenderFinished_2:
	p0 or r10, r10, #0x00000080
SPMRenderFinished_3:
	ldr r2, #669, drc1	/* EUR_CR_DMS_CTRL */
	wdf drc1
	and r2, r2, ~#0x00000001
	str #669, r2	/* EUR_CR_DMS_CTRL */
	and.testtanz.chan0 !c0, p0, r10, #2
	p0 ba SPMRenderFinished_4
	str #407, #1	/* EUR_CR_DPM_LSS_PARTIAL_CONTEXT */
SPMRenderFinished_4:
	mov r3, #0x00001208
	ldr r3, r3, drc0
	wdf drc0
	str #468, r3	/* EUR_CR_DPM_MTILE_ABORTED */
	ldad.bpcache r3, [sa0,+#65], drc0
	wdf drc0
	and r3, r3, #0x000000E0
	shr r3, r3, #0x00000005
	imae.u r1, r3.low, #1.low, #2, u32
	shl r1, r1, #0x0000000C
	or r1, r1, #0x000001A6
	str r1, #2
	str #406, #0	/* EUR_CR_DPM_PARTIAL_RENDER */
	mov r0, #0x00000200
SPMRenderFinished_5:
	imae.u r1, r3.low, #1.low, #2, u32
	shl r1, r1, #0x0000000C
	or r1, r1, #0x0000004B
	ldr r1, r1, drc0
	wdf drc0
	and r1, r1, r0
	xor.testtanz.chan0 !c0, p0, r1, r0
	p0 ba SPMRenderFinished_5
	mov r0, #0x00000200
	imae.u r2, r3.low, #1.low, #2, u32
	shl r2, r2, #0x0000000C
	or r2, r2, #0x0000004D
	str r2, r0
	str #407, #0	/* EUR_CR_DPM_LSS_PARTIAL_CONTEXT */
	ldad.bpcache r14, [sa0,+#54], drc0
	wdf drc0
	!p1 ba SPMRenderFinished_6
	mov r16, #0x00000100
	mov r23, pclink
	ba.savelink EmitLoopBack
	mov pclink, r23
	ba SPMRenderFinished_7
SPMRenderFinished_6:
	imae.u r2, r3.low, #1.low, #2, u32
	shl r2, r2, #0x0000000C
	or r2, r2, #0x000001A7
	str r2, #1
	str #423, #1	/* EUR_CR_DPM_OUTOFMEM */
SPMRenderFinished_7:
	ldad.bpcache r0, [r14,+#31], drc0
	wdf drc0
	ldad.bpcache r2, [r0,+#0], drc0
	wdf drc0
	or r2, r2, #0x00000002
	stad.bpcache [r0,+#0], r2
	stad.bpcache [sa0,+#54], #0
	idf drc0, bif
	wdf drc0
	mov r9, #0xFFEFFFDD
	and r10, r10, r9
	or sa9, r10, #0x00000000
	mov pclink, r15
	lapc
