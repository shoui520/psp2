ZeroPC:
	mov r16, #0xAD002600
	stad.bpcache [sa1,+#14], r16
	idf drc0, bif
	wdf drc0
	ba WaitForReset
	nop

EventHandler:
	smlsi #0, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	mov r15, #0x00000000
	mov r22, #0x00000000
	mov r0, #0x00000020
	and r0, pa1, r0
	and r1, pa0, #0x0000000C
	or r0, r0, r1
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba EventHandler_3
	mov r1, #0x00000000
	and r0, pa0, #0x00000004
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba EventHandler_1
	or r1, r1, #0x00000001
EventHandler_1:
	and r0, pa0, #0x00000008
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba EventHandler_2
	or r1, r1, #0x00000002
EventHandler_2:
	and r0, pa1, #0x00000020
	and.testtanz.chan0 !c0, p0, r0, r0
	p0 or r1, r1, #0x00000008
	and.testtaz.chan0 !c0, p0, r1, r1
	p0 ba EventHandler_3
	or r16, r1, #0x00000000
	mov r23, pclink
	ba.savelink EmitLoopBack
	mov pclink, r23
EventHandler_3:
	and r0, pa0, #0x00000360
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba EventHandler_5
	mov r1, #0x00000000
	and r0, pa0, #0x00000300
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba EventHandler_4
	or r1, r1, #0x00000060
EventHandler_4:
	and r0, pa0, #0x00000020
	and.testtanz.chan0 !c0, p0, r0, r0
	p0 or r1, r1, #0x00000080
	or r16, r1, #0x00000000
	mov r23, pclink
	ba.savelink EmitLoopBack
	mov pclink, r23
EventHandler_5:
	and r0, pa0, #0x00000010
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba EventHandler_7
	mov r1, #0x00000000
	and r0, pa0, #0x00000010
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba EventHandler_6
	ldr r10, #457, drc0
	wdf drc0
	and r10, r10, #0xFFFF0000
	shr r10, r10, #0x00000010
	or r1, r1, #0x00000100
EventHandler_6:
	or r16, r1, #0x00000000
	mov r23, pclink
	ba.savelink EmitLoopBack
	mov pclink, r23
EventHandler_7:
	and r0, pa0, #0x00001000
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba EventHandler_8
	mov r16, #0x00020000
	mov r23, pclink
	ba.savelink EmitLoopBack
	mov pclink, r23
EventHandler_8:
	and r0, pa0, #0x00000002
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba EventHandler_9
	ldad.bpcache r2, [sa1,+#12], drc0
	mov r3, #0x00000001
	wdf drc0
	imae.u r2, r3.low, #1.low, r2, u32
	stad.bpcache [sa1,+#12], r2
	idf drc0, bif
	wdf drc0
	mov r15, #0x00000053
	ba Timer
EventHandler_9:
	and r0, pa1, #0x00000040
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba EventHandler_15
	ldad.bpcache r1, [sa2,+#1], drc0
	wdf drc0
	ldad.bpcache r0, [sa0,+#45], drc0
	wdf drc0
	imae.u r0, r1.low, #32.low, r0, u32
	ldad.fcfill r4, [r0,#1++], drc0
	ldad.fetch7 r5, [r0,#0++], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r5, #4
	!p0 ba EventHandler_10
	mov r16, #0x00000001
	mov r17, #0x00000010
	mov r23, pclink
	ba.savelink InvalidateSLC
	mov pclink, r23
EventHandler_10:
	and.testtanz.chan0 !c0, p0, r5, #2
	!p0 ba EventHandler_11
	mov r16, #0x00000001
	mov r23, pclink
	ba.savelink InvalidateBIFCache
	mov pclink, r23
	ba EventHandler_12
EventHandler_11:
	and.testtanz.chan0 !c0, p0, r5, #1
	!p0 ba EventHandler_12
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink InvalidateBIFCache
	mov pclink, r23
EventHandler_12:
	and.testtanz.chan0 !c0, p0, r5, #8
	!p0 ba EventHandler_14
	lock #7
	str #516, #1
EventHandler_13:
	ldr r2, #70, drc0
	wdf drc0
	shl.testpat.chan0 !c0, p0, r2, #24
	p0 ba EventHandler_13
	mov r2, #0x00000080
	str #69, r2
	ldr r0, #8927, drc0
	wdf drc0
	release #7
EventHandler_14:
	mov pclink, r4
	imae.u r0, r1.low, #1.low, #1, u32
	and r0, r0, #0x000000FF
	stad.bpcache [sa2,+#1], r0
	idf drc0, bif
	wdf drc0
	lapc
EventHandler_15:
	phas #0, #0, pixel, end, parallel
	or.end r0, r0, #0x00000000

HostKickFlushPDCache:
	phas #0, #0, pixel, end, parallel
	or.end r0, r0, #0x00000000

HostKickPower:
	xor.testtaz.chan0 !c0, p0, r7, #3
	!p0 ba HostKickPower_1
	ba.savelink StartTimer
	nop
	ba HostKickProcessed
HostKickPower_1:
	ldad.bpcache r0, [sa1,+#8], drc0
	ldad.bpcache r2, [sa1,+#9], drc0
	wdf drc0
	and r0, r0, ~#0x00000002
	stad.bpcache [sa1,+#8], r0
	and r2, r2, ~#0x00000002
	stad.bpcache [sa1,+#9], r2
	or r0, sa11, #0x00000000
	xor.testtaz.chan0 !c0, p0, r7, #1
	!p0 ba HostKickPower_2
	or r0, r0, #0x00000001
HostKickPower_2:
	xor.testtaz.chan0 !c0, p0, r7, #2
	!p0 ba HostKickPower_3
	p0 or r0, r0, #0x00000002
HostKickPower_3:
	or sa11, r0, #0x00000000
	ba.savelink CheckHostPowerRequest
	nop
	ba HostKickProcessed

HostKickContextSuspend:
	or r0, r6, #0x00000000
	ldad.bpcache r1, [r0,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r7, #2
	!p0 ba HostKickContextSuspend_1
	and r1, r1, ~#0x00000010
	stad.bpcache [r0,+#0], r1
	ba HostKickProcessed
HostKickContextSuspend_1:
	or r1, r1, #0x00000010
	stad.bpcache [r0,+#0], r1
	or r2, sa10, #0x00000000
	and.testtaz.chan0 !c0, p0, r2, #1
	p0 ba HostKickContextSuspend_2
	or r2, sa7, #0x00000000
	xor.testtaz.chan0 !c0, p0, r0, r2
	!p0 ba HostKickContextSuspend_2
	or r16, sa10, #0x00000000
	and.testtanz.chan0 !c0, p2, r16, #14
	p2 ba HostKickContextSuspend_2
	or r16, r16, #0x00000008
	or sa10, r16, #0x00000000
	idf drc0, bif
	wdf drc0
HostKickContextSuspend_2:
	or r2, sa9, #0x00000000
	and.testtaz.chan0 !c0, p0, r2, #1
	p0 ba HostKickContextSuspend_3
	ldad.bpcache r2, [sa0,+#52], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r0, r2
	!p0 ba HostKickContextSuspend_3
	nop
HostKickContextSuspend_3:
	ba HostKickProcessed

HostKick2D:
	or r1, r7, #0x00000000
	ldad.bpcache r2, [r1,+#5], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r2, r2
	imae.u r2, r2.low, #1.low, #1, u32
	stad.bpcache [r1,+#5], r2
	idf drc0, bif
	wdf drc0
	p0 ba HostKick2D_2
	ldad.bpcache r2, [r1,+#1], drc0
	wdf drc0
	imae.u r4, r2.low, #1.low, #35, u32
	or i.l, r4, #0x00000000
	or r5, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p0, r5, r5
	!p0 ba HostKick2D_1
	imae.u r3, r2.low, #1.low, #32, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r1, #0x00000000
HostKick2D_1:
	imae.u r4, r2.low, #1.low, #35, u32
	or i.l, r4, #0x00000000
	or sa[il + #0], r1, #0x00000000
	!p0 stad.bpcache [r1,+#7], r5
	!p0 stad.bpcache [r5,+#8], r1
	idf drc0, bif
	wdf drc0
HostKick2D_2:
	ba HostKickProcessed

HostKickTransfer:
	or r1, r7, #0x00000000
	ldad.bpcache r2, [r1,+#5], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r2, r2
	imae.u r2, r2.low, #1.low, #1, u32
	stad.bpcache [r1,+#5], r2
	idf drc0, bif
	p0 ba HostKickTransfer_2
	ldad.bpcache r2, [r1,+#1], drc0
	wdf drc0
	imae.u r4, r2.low, #1.low, #17, u32
	or i.l, r4, #0x00000000
	or r5, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p0, r5, r5
	!p0 ba HostKickTransfer_1
	imae.u r3, r2.low, #1.low, #14, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r1, #0x00000000
HostKickTransfer_1:
	imae.u r4, r2.low, #1.low, #17, u32
	or i.l, r4, #0x00000000
	or sa[il + #0], r1, #0x00000000
	!p0 stad.bpcache [r1,+#43], r5
	!p0 stad.bpcache [r5,+#44], r1
	idf drc0, bif
HostKickTransfer_2:
	wdf drc0
	ba HostKickProcessed

HostKickTA:
	or r1, r7, #0x00000000
	ldad.bpcache r2, [r1,+#5], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r2, r2
	imae.u r2, r2.low, #1.low, #1, u32
	stad.bpcache [r1,+#5], r2
	p0 ba HostKickTA_2
	ldad.bpcache r2, [r1,+#18], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r2, r2
	p0 ba HostKickTA_2
	ldad.bpcache r2, [r1,+#1], drc0
	wdf drc0
	imae.u r4, r2.low, #1.low, #23, u32
	or i.l, r4, #0x00000000
	or r5, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p0, r5, r5
	!p0 ba HostKickTA_1
	imae.u r3, r2.low, #1.low, #20, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r1, #0x00000000
HostKickTA_1:
	imae.u r4, r2.low, #1.low, #23, u32
	or i.l, r4, #0x00000000
	or sa[il + #0], r1, #0x00000000
	!p0 stad.bpcache [r1,+#22], r5
	!p0 stad.bpcache [r5,+#23], r1
HostKickTA_2:
	idf drc0, bif
	wdf drc0
	ba HostKickProcessed

HostKickCleanup:
	or r1, sa11, #0x00000000
	xor.testtaz.chan0 !c0, p0, r6, #1
	!p0 ba HostKickCleanup_1
	or r1, r1, #0x00000004
	ba HostKickCleanup_5
HostKickCleanup_1:
	xor.testtaz.chan0 !c0, p0, r6, #2
	!p0 ba HostKickCleanup_2
	or r1, r1, #0x00000008
	ba HostKickCleanup_5
HostKickCleanup_2:
	xor.testtaz.chan0 !c0, p0, r6, #3
	!p0 ba HostKickCleanup_3
	or r1, r1, #0x00000010
	ba HostKickCleanup_5
HostKickCleanup_3:
	xor.testtaz.chan0 !c0, p0, r6, #4
	!p0 ba HostKickCleanup_4
	or r1, r1, #0x00000020
	ba HostKickCleanup_5
HostKickCleanup_4:
	xor.testtaz.chan0 !c0, p0, r6, #5
	!p0 ba HostKickCleanup_5
	or r1, r1, #0x00000040
	ba HostKickCleanup_5
HostKickCleanup_5:
	stad.bpcache [sa0,+#57], r7
	or sa11, r1, #0x00000000
	idf drc0, bif
	wdf drc0
	ba.savelink CheckHostCleanupRequest
	nop
	ba HostKickProcessed
	nop

HostKickProcess_Queues:
	ba HostKickProcessed

HostKickSetHWPerfStatus:
	ldad.bpcache r0, [sa0,+#74], drc0
	mov r1, #0xFFFFFFFF
	wdf drc0
	stad.bpcache [r0,+#2], r1
	and.testtanz.chan0 !c0, p0, r6, #1
	!p0 ba HostKickSetHWPerfStatus_1
HostKickSetHWPerfStatus_1:
	and r0, r6, ~#0x00000001
	or sa39, r0, #0x00000000
	ba.savelink UpdateHWPerfRegs
	ba HostKickProcessed

HostKickProcessed:
	mov sa12, #0x00000000
	mov sa13, #0x00000007
	or r0, sa9, #0x00000000
	shl.testpat.chan0 !c0, p0, r0, #25
	p0 ba HostKickProcessed_1
	and r0, r0, ~#0x00000040
	or sa9, r0, #0x00000000
	str #423, #1
	idf drc0, bif
	wdf drc0
HostKickProcessed_1:
	mov r23, pclink
	ba.savelink Check2DQueue
	mov pclink, r23
	mov r23, pclink
	ba.savelink Check3DQueues
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink CheckTAQueue
	mov pclink, r23
	phas #0, #0, pixel, end, parallel
	or.end r0, r0, #0x00000000

HostKickGetMiscInfo:
	or r2, r7, #0x00000000
	ldr r1, #9, drc0
	ldr r3, #8, drc0
	wdf drc0
	stad.bpcache [r2,+#1], r1
	stad.bpcache [r2,+#2], r3
	mov r1, #0x00010000
	or r1, r1, #0x00000700
	or r1, r1, #0x00000011
	stad.bpcache [r2,+#3], r1
	mov r1, #0x000013A5
	stad.bpcache [r2,+#4], r1
	mov r1, #0x0000021F
	stad.bpcache [r2,+#5], r1
	mov r1, #0x00020000
	or r1, r1, #0x00000100
	or r1, r1, #0x00000006
	stad.bpcache [r2,+#6], r1
	mov r1, #0x30322518
	stad.bpcache [r2,+#7], r1
	ldad.bpcache r3, [r2,+#0], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r3, #2
	!p0 ba HostKickGetMiscInfo_1
	mov r1, #0x00000084
	stad.bpcache [r2,+#22], r1
	mov r1, #0x00000020
	stad.bpcache [r2,+#23], r1
	mov r1, #0x000000E8
	stad.bpcache [r2,+#9], r1
	mov r1, #0x00000070
	stad.bpcache [r2,+#10], r1
	mov r1, #0x000002A4
	stad.bpcache [r2,+#11], r1
	mov r1, #0x000001E0
	stad.bpcache [r2,+#12], r1
	mov r1, #0x000004C0
	stad.bpcache [r2,+#13], r1
	mov r1, #0x000000DC
	stad.bpcache [r2,+#14], r1
	mov r1, #0x00000104
	stad.bpcache [r2,+#15], r1
	mov r1, #0x0000003C
	stad.bpcache [r2,+#16], r1
	mov r1, #0x0000006C
	stad.bpcache [r2,+#17], r1
	mov r1, #0x00000220
	stad.bpcache [r2,+#18], r1
	mov r1, #0x00000084
	stad.bpcache [r2,+#19], r1
	mov r1, #0x0000009C
	stad.bpcache [r2,+#20], r1
	mov r1, #0x000000B4
	stad.bpcache [r2,+#21], r1
HostKickGetMiscInfo_1:
	idf drc0, bif
	wdf drc0
	or r3, r3, #0x00000001
	stad.bpcache [r2,+#0], r3
	idf drc0, bif
	wdf drc0
	ba HostKickProcessed

HostKickDataBreakpoint:
	mov r1, #0x000003D1
	imae.u r1, r6.low, #3.low, r1, u32
	str r1, r7
	ima32.s1.u r1, r1, #1, #1
	str r1, r8
	ima32.s1.u r1, r1, #1, #1
	str r1, r9
	mov r1, #0x000013BA
	imae.u r1, r6.low, #3.low, r1, u32
	str r1, r7
	ima32.s1.u r1, r1, #1, #1
	str r1, r8
	ima32.s1.u r1, r1, #1, #1
	and r2, r9, ~#0x00000007
	imae.u r2, r2.low, #1.low, r9, u32
	str r1, r2
	stad.bpcache [sa1,+#10], #1
	idf drc0, bif
	wdf drc0
	ba HostKickProcessed
