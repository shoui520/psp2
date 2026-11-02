	.align2
TwoDEventHandler:
	smlsi #0, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	mov r15, #0x00000000
	mov r22, #0x00000000
	and r0, pa0, #0x00001000
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 br TwoDEventHandler_1
	mov r0, #0x08000000
	str #4173, r0
	mov r15, #0x00001DA8
	ba TwoDComplete
TwoDEventHandler_1:
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink CommonProgramEnd
	mov pclink, r23

TwoDHandler:
	smlsi #0, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	mov r15, #0x00000000
	mov r22, #0x00000000
	ldad.bpcache r0, [sa0,+#56], drc0
	xor r1, pa0, ~#0x00000000
	wdf drc0
	and r0, r0, r1
	stad.bpcache [sa0,+#56], r0
	and r0, pa0, #0x00020000
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba TwoDHandler_1
	mov r0, #0x08000000
	str #4173, r0
	mov r15, #0x00001DBB
	ba TwoDComplete
TwoDHandler_1:
	and r0, pa0, #0x00400000
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba TwoDHandler_2
	ba.savelink Find2D
TwoDHandler_2:
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink CommonProgramEnd
	mov pclink, r23

TwoDComplete:
	or r0, sa8, #0x00000000
	and r0, r0, ~#0x00000001
	or sa8, r0, #0x00000000
	ldr r3, #4104, drc0
	wdf drc0
	and r3, r3, ~#0x00000300
	or r3, r3, #0x00000200
	str #4104, r3
	ldad.bpcache r1, [sa0,+#47], drc0
	wdf drc0
	ba.savelink Update2DSurfaceOps
	mov r0, #0x00004000
	str #75, r0
	ldad.bpcache r0, [sa0,+#48], drc0
	wdf drc0
	ba.savelink Advance2DCCBROff
	and.testtanz.chan0 !c0, p1, r2, r2
	ldad.bpcache r1, [r0,+#1], drc0
	wdf drc0
	ldad.bpcache r4, [r0,+#8], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, r4
	imae.u r2, r1.low, #1.low, #32, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r4, #0x00000000
	!p0 ba TwoDComplete_1
	imae.u r3, r1.low, #1.low, #35, u32
	or i.l, r3, #0x00000000
	mov sa[il + #0], #0x00000000
TwoDComplete_1:
	!p0 stad.bpcache [r4,+#7], #0
	stad.bpcache [r0,+#8], #0
	idf drc0, bif
	wdf drc0
	!p1 ba TwoDComplete_3
	imae.u r3, r1.low, #1.low, #35, u32
	or i.l, r3, #0x00000000
	or r4, sa[il + #0], #0x00000000
	!p0 stad.bpcache [r4,+#8], r0
	!p0 ba TwoDComplete_2
	imae.u r2, r1.low, #1.low, #32, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r0, #0x00000000
TwoDComplete_2:
	imae.u r3, r1.low, #1.low, #35, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r0, #0x00000000
	stad.bpcache [r0,+#7], r4
	idf drc0, bif
	wdf drc0
TwoDComplete_3:
	mov r23, pclink
	ba.savelink CheckTAQueue
	mov pclink, r23
	mov r23, pclink
	ba.savelink Check3DQueues
	mov pclink, r23
	nop
	ba.savelink Find2D
	mov pclink, r15
	lapc

Find2D:
	or r1, sa11, #0x00000000
	or r0, sa8, #0x00000000
	and.testtanz.chan0 !c0, p1, r0, #1
	p1 ba Find2D_16
	and.testtaz.chan0 !c0, p2, r1, #3
	p2 ba Find2D_1
	nop
	ba Find2D_16
Find2D_1:
	mov r10, #0x00000000
	or r0, sa32, #0x00000000
Find2D_2:
	and.testtanz.chan0 !c0, p0, r0, r0
	p0 ba Find2D_4
	imae.u r10, r10.low, #1.low, #1, u32
	or r0, sa5, #0x00000000
	isub16.testnat.chan0 !c0, p0, r0, r10
	p0 ba Find2D_3
	imae.u r1, r10.low, #1.low, #32, u32
	or i.l, r1, #0x00000000
	or r0, sa[il + #0], #0x00000000
	ba Find2D_2
Find2D_3:
	mov r16, #0x00000004
	mov r23, pclink
	ba.savelink ClearActivePowerFlags
	mov pclink, r23
	ba Find2D_16
Find2D_4:
	ldad.bpcache r2, [r0,+#0], drc0
	ldad.bpcache r1, [r0,+#4], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r2, #16
	!p0 ba Find2D_5
	nop
	ba Find2D_14
Find2D_5:
	ldad.bpcache r1, [r1,+#1], drc0
	ldad.bpcache r2, [r0,+#3], drc1
	wdf drc0
	wdf drc1
	imae.u r1, r1.low, #1.low, r2, u32
	ldad.fetch4 r2, [r1,+#46], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba Find2D_6
	ldad.bpcache r6, [r3,+#0], drc0
	ldad.bpcache r7, [r5,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r6, r2
	p0 xor.testtaz.chan0 !c0, p0, r7, r4
	p0 ba Find2D_6
	ba Find2D_14
Find2D_6:
	ldad.fetch4 r2, [r1,+#50], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba Find2D_7
	ldad.bpcache r6, [r3,+#0], drc0
	ldad.bpcache r7, [r5,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r6, r2
	p0 xor.testtaz.chan0 !c0, p0, r7, r4
	p0 ba Find2D_7
	nop
	ba Find2D_14
Find2D_7:
	ldad r2, [r1,+#30], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba Find2D_11
	mov r3, #0x0000007C
	imae.s r3, r3.low, #1.low, r1, u32
	ldad.fetch4 r4, [r3,#0++], drc0
Find2D_8:
	isub16.testtaz.chan0 r2, p0, r2, #1
	wdf drc0
	ldad.bpcache r8, [r5,+#0], drc0
	ldad.bpcache r9, [r7,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p1, r8, r4
	p1 ba Find2D_9
	nop
	ba Find2D_14
Find2D_9:
	xor.testtaz.chan0 !c0, p1, r9, r6
	p1 ba Find2D_10
	nop
	ba Find2D_14
Find2D_10:
	!p0 ldad.fetch4 r4, [r3,#0++], drc0
	!p0 ba Find2D_8
Find2D_11:
	ldad.fetch4 r2, [r1,+#42], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba Find2D_13
	ldad.bpcache r6, [r3,+#0], drc0
	ldad.bpcache r7, [r5,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r6, r2
	p0 ba Find2D_12
	ba Find2D_14
Find2D_12:
	xor.testtaz.chan0 !c0, p0, r7, r4
	p0 ba Find2D_13
	nop
	ba Find2D_14
	nop
Find2D_13:
	ba Find2D_15
Find2D_14:
	ldad.bpcache r0, [r0,+#8], drc0
	wdf drc0
	ba Find2D_2
Find2D_15:
	ldad.bpcache r2, [r0,+#0], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, #4
	p0 or r2, r10, #0x00000000
	p0 ba Kick2D
	and r2, r2, ~#0x00000004
	stad.bpcache [r0,+#0], r2
	ba DummyProcess2D
Find2D_16:
	lapc

Kick2D:
	mov sa5, #0x00000002
	stad.bpcache [sa0,+#48], r0
	stad.bpcache [sa0,+#47], r1
	ldad.bpcache r2, [r0,+#6], drc0
	wdf drc0
	stad.bpcache [sa0,+#83], r2
	and r29, r2, #0x0000FFFF
	or r29, r29, #0x38000000
	mov r30, #0x08001008
	mov.nosched r31, #0x08002000
	stad [r30,+#0], r29
	stad [r31,+#0], #0
	idf drc0, bif
	wdf drc0
	shr r29, r2, #0x00000010
	or r29, r29, #0x3A000000
	mov r30, #0x08001008
	mov.nosched r31, #0x08002000
	stad [r30,+#0], r29
	stad [r31,+#0], #0
	idf drc0, bif
	wdf drc0
	or r16, r0, #0x00000000
	mov r17, #0x00000004
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	ldad r2, [r1,+#3], drc1
	ldr r3, #4104, drc0
	wdf drc0
	and r3, r3, ~#0x00000300
	or r3, r3, #0x00000100
	str #4104, r3
	mov r3, #0x00000010
	imae.u r3, r3.low, #1.low, r1, u32
	ldad r4, [r3,#1++], drc0
	wdf drc1
Kick2D_1:
	isub16.testtaz.chan0 r2, p0, r2, #1
	wdf drc0
	str #4192, r4
	!p0 ldad r4, [r3,#1++], drc0
	!p0 ba Kick2D_1
	stad.bpcache [sa0,+#91], #0
	ldad.bpcache r1, [sa0,+#86], drc0
	wdf drc0
	or r1, r1, #0x00000010
	stad.bpcache [sa0,+#86], r1
	idf drc0, bif
	wdf drc0
	or r2, sa8, #0x00000000
	or r2, r2, #0x00000001
	or sa8, r2, #0x00000000
	ldad.bpcache r1, [r0,+#1], drc0
	wdf drc0
	ldad.bpcache r4, [r0,+#7], drc0
	ldad.bpcache r5, [r0,+#8], drc1
	wdf drc0
	and.testtanz.chan0 !c0, p0, r4, r4
	wdf drc1
	and.testtanz.chan0 !c0, p1, r5, r5
	p0 stad.bpcache [r4,+#8], r5
	p1 stad.bpcache [r5,+#7], r4
	p0 ba Kick2D_2
	imae.u r2, r1.low, #1.low, #32, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r5, #0x00000000
Kick2D_2:
	p1 ba Kick2D_3
	imae.u r3, r1.low, #1.low, #35, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r4, #0x00000000
Kick2D_3:
	idf drc0, bif
	wdf drc0
	imae.u r2, r1.low, #1.low, #32, u32
	or i.l, r2, #0x00000000
	or r4, sa[il + #0], #0x00000000
	and.testtanz.chan0 !c0, p0, r4, r4
	stad.bpcache [r0,+#7], #0
	stad.bpcache [r0,+#8], r4
	p0 stad.bpcache [r4,+#7], r0
	imae.u r2, r1.low, #1.low, #32, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r0, #0x00000000
	p0 ba Kick2D_4
	imae.u r3, r1.low, #1.low, #35, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r0, #0x00000000
Kick2D_4:
	idf drc0, bif
	wdf drc0
	lapc

Advance2DCCBROff:
	ldad.bpcache r2, [r0,+#4], drc0
	wdf drc0
	ldad.bpcache r3, [r2,+#1], drc0
	ldad r4, [r1,+#0], drc1
	wdf drc0
	wdf drc1
	imae.u r3, r3.low, #1.low, r4, u32
	and r3, r3, #0x0000FFFF
	stad.bpcache [r2,+#1], r3
	ldad.bpcache r2, [r0,+#5], drc0
	wdf drc0
	imae.s r2, r2.low, #1.low, #127, u32
	stad.bpcache [r0,+#5], r2
	idf drc0, bif
	wdf drc0
	lapc

Update2DSurfaceOps:
	mov sa12, #0x00000000
	mov sa13, #0x00000007
	ldad.fetch2 r4, [r1,+#48], drc1
	ldad.fetch2 r2, [r1,+#52], drc0
	wdf drc1
	and.testtaz.chan0 !c0, p0, r5, r5
	p0 ba Update2DSurfaceOps_1
	mov r6, #0x00000001
	imae.u r4, r6.low, #1.low, r4, u32
	stad.bpcache [r5,+#0], r4
Update2DSurfaceOps_1:
	ldad.fetch2 r4, [r1,+#44], drc1
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba Update2DSurfaceOps_2
	mov r6, #0x00000001
	imae.u r2, r6.low, #1.low, r2, u32
	stad.bpcache [r3,+#0], r2
Update2DSurfaceOps_2:
	ldad r2, [r1,+#30], drc0
	wdf drc1
	and.testtaz.chan0 !c0, p0, r5, r5
	p0 ba Update2DSurfaceOps_3
	mov r6, #0x00000001
	imae.u r4, r6.low, #1.low, r4, u32
	stad.bpcache [r5,+#0], r4
Update2DSurfaceOps_3:
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba Update2DSurfaceOps_5
	mov r3, #0x0000007C
	imae.s r3, r3.low, #1.low, r1, u32
	ldad.fetch2 r4, [r3,#0++], drc0
Update2DSurfaceOps_4:
	isub16.testtaz.chan0 r2, p0, r2, #1
	mov r6, #0x00000008
	imae.u r3, r6.low, #1.low, r3, u32
	mov r6, #0x00000001
	wdf drc0
	imae.u r4, r6.low, #1.low, r4, u32
	stad.bpcache [r5,+#0], r4
	!p0 ldad.fetch2 r4, [r3,#0++], drc0
	!p0 ba Update2DSurfaceOps_4
Update2DSurfaceOps_5:
	ldad r2, [r1,+#55], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba Update2DSurfaceOps_7
	mov r3, #0x000000E0
	imae.s r3, r3.low, #1.low, r1, u32
	ldad.fetch2 r4, [r3,#0++], drc0
Update2DSurfaceOps_6:
	isub16.testtaz.chan0 r2, p0, r2, #1
	wdf drc0
	stad.bpcache [r4,+#0], r5
	!p0 ldad.fetch2 r4, [r3,#0++], drc0
	!p0 ba Update2DSurfaceOps_6
Update2DSurfaceOps_7:
	idf drc0, bif
	wdf drc0
	or r2, sa9, #0x00000000
	shl.testpat.chan0 !c0, p0, r2, #25
	p0 ba Update2DSurfaceOps_8
	and r2, r2, ~#0x00000040
	or sa9, r2, #0x00000000
	ldr r2, #456, drc0
	wdf drc0
	and r2, r2, #0x000000E0
	shr r2, r2, #0x00000005
	imae.u r3, r2.low, #1.low, #2, u32
	shl r3, r3, #0x0000000C
	or r3, r3, #0x000001A7
	str r3, #1
	str #423, #1
	wdf drc0
Update2DSurfaceOps_8:
	lapc

DummyProcess2D:
	mov sa5, #0x00000002
	mov r7, pclink
	ba.savelink Update2DSurfaceOps
	nop
	ba.savelink Advance2DCCBROff
	mov pclink, r7
	and.testtanz.chan0 !c0, p1, r2, r2
	mov r2, #0x00004000
	str #75, r2
	ldad.bpcache r1, [r0,+#1], drc0
	wdf drc0
	ldad.bpcache r4, [r0,+#7], drc0
	ldad.bpcache r5, [r0,+#8], drc1
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, r4
	wdf drc1
	!p0 ba DummyProcess2D_1
	imae.u r2, r1.low, #1.low, #32, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r5, #0x00000000
DummyProcess2D_1:
	!p0 stad.bpcache [r4,+#8], r5
	and.testtaz.chan0 !c0, p0, r5, r5
	!p0 ba DummyProcess2D_2
	imae.u r3, r1.low, #1.low, #35, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r4, #0x00000000
DummyProcess2D_2:
	!p0 stad.bpcache [r5,+#7], r4
	stad.bpcache [r0,+#7], #0
	stad.bpcache [r0,+#8], #0
	idf drc1, bif
	wdf drc1
	!p1 ba DummyProcess2D_4
	imae.u r3, r1.low, #1.low, #35, u32
	or i.l, r3, #0x00000000
	or r4, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p0, r4, r4
	!p0 stad.bpcache [r4,+#8], r0
	!p0 ba DummyProcess2D_3
	imae.u r2, r1.low, #1.low, #32, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r0, #0x00000000
DummyProcess2D_3:
	imae.u r3, r1.low, #1.low, #35, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r0, #0x00000000
	stad.bpcache [r0,+#7], r4
DummyProcess2D_4:
	idf drc0, bif
	wdf drc0
	ba Find2D
	nop
	.align2
	nop
