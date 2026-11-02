InituKernelPrimary1:
	mov r0, #0xDEADBEEF
	nop
	mov r2, #0xDEADBEEF
	mov r4, #0xDEADBEEF
	ldad.bpcache r4, [r4,+#1], drc0
	wdf drc0
	ldad.bpcache r4, [r4,+#9], drc0
	wdf drc0
	and r4, r4, #0x00000001
	emitpds.tasks.taske #0 /* incp */, r0, r2, r4, #0x00000C43
	emitpds.tasks.taske #0 /* incp */, r0, r2, r4, #0x00001443
	mov r0, #0xDEADBEEF
	mov r2, #0xDEADBEEF
	mov r4, #0x00000000
	emitpds.tasks.taske #0 /* incp */, r0, r2, r4, #0x00000843
	phas #0, #0, pixel, end, parallel
	nop.end

InituKernelSecondary:
	nop
	phas #0, #0, pixel, end, parallel
	nop.nosched
	and.testtaz.chan0 !c0, p0, #0x00000001, #1
	p0 ldad.fetch4 pa124, [pa0,#1++], drc0
	wdf drc0
	nop.end

InituKernelPrimary2:
	smlsi #0, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	mov r15, #0x00000000
	mov r22, #0x00000000
	nop
	mov r0, #0xDEADBEEF
	str #8858, r0
	mov r0, #0xDEADBEEF
	str #8859, r0
	mov r0, #0x00001000
	ldr r0, r0, drc0
	wdf drc0
	ima32.s1.u r0, r0, #1, #1
	or sa38, r0, #0x00000000
	ba.savelink InitConfigRegisters
	xor.testtaz.chan0 !c0, p0, r0, r0
	ba.savelink UpdateHWPerfRegs
	ldad.bpcache r0, [sa1,+#9], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r0, #1
	p0 ba.savelink HWRecovery
	or r9, sa38, #0x00000000
	ldr r0, #798, drc0
	wdf drc0
	or r4, r0, #0x00000000
	and r5, r4, #0x0000000F
	shl r5, r5, #0x00000004
	and r4, r4, ~#0x000000F0
	or r4, r4, r5
	mov r24, #0x00000001
	mov r25, #0x0000031E
	or r26, r4, #0x00000000
	mov r23, pclink
	ba.savelink JumpToSupervisor
	mov pclink, r23
	ldr r1, #4177, drc0
	wdf drc0
	mov r4, #0x00000001
	mov r5, #0x00010000
	or r4, r4, r5
	str #4177, r4
	ldr r2, #8849, drc0
	wdf drc0
	mov r4, #0x00000000
	shl r4, r4, #0x0000001A
	mov r5, #0xDEADBEEF
	shr r5, r5, #0x00000006
	or r4, r4, r5
	mov r6, #0x00002291
	or r5, r9, #0x00000000
	mov r7, #0x00001000
InituKernelPrimary2_1:
	mov r24, #0x00000001
	or r25, r6, #0x00000000
	or r26, r4, #0x00000000
	mov r23, pclink
	ba.savelink JumpToSupervisor
	mov pclink, r23
	imae.u r6, r7.low, #1.low, r6, u32
	isub16.testtanz.chan0 r5, p0, r5, #1
	p0 ba InituKernelPrimary2_1
	mov r4, #0x00000000
	str #129, r4
	mov r4, #0x10010010
	str #130, r4
	str #131, r4
	mov r4, #0x0001F01F
	str #146, r4
	mov r4, #0x00001001
	str #132, r4
	mov r4, #0x00000100
	str #133, r4
	mov r4, #0x00400000
	str #134, r4
	ldr r4, #669, drc0
	mov r5, #0x11000000
	wdf drc0
	or r4, r4, r5
	str #669, r4
	ldr r4, #143, drc0
	wdf drc0
	or r4, r4, #0x00000002
	str #143, r4
	mov r4, #0xDEADBEEF
	or r5, r9, #0x00000000
	mov r6, #0x00002087
InituKernelPrimary2_2:
	str r6, r4
	mov r7, #0x00001000
	imae.u r6, r7.low, #1.low, r6, u32
	mov r7, #0x00003000
	imae.u r4, r7.low, #1.low, r4, u32
	isub16.testtanz.chan0 r5, p0, r5, #1
	p0 ba InituKernelPrimary2_2
	mov r4, #0x00000001
	str #224, r4
InituKernelPrimary2_3:
	ldr r4, #70, drc0
	wdf drc0
	shl.testpat.chan0 !c0, p0, r4, #26
	p0 ba InituKernelPrimary2_3
	mov r4, #0x00000020
	str #69, r4
	mov r24, #0x00000001
	mov r25, #0x0000031E
	or r26, r0, #0x00000000
	mov r23, pclink
	ba.savelink JumpToSupervisor
	mov pclink, r23
	str #4177, r1
	mov r6, #0x00002291
	or r5, r9, #0x00000000
	mov r7, #0x00001000
InituKernelPrimary2_4:
	mov r24, #0x00000001
	or r25, r6, #0x00000000
	or r26, r2, #0x00000000
	mov r23, pclink
	ba.savelink JumpToSupervisor
	mov pclink, r23
	imae.u r6, r7.low, #1.low, r6, u32
	isub16.testtanz.chan0 r5, p0, r5, #1
	p0 ba InituKernelPrimary2_4
	stad.bpcache [sa1,+#0], #1
	idf drc0, bif
	wdf drc0
	ba.savelink StartTimer
	ldad.bpcache r0, [sa1,+#13], drc0
	wdf drc0
	phas #0, #0, pixel, end, parallel
	nop.end

InitConfigRegisters:
	mov r0, #0x000020C4
	mov r1, #0x00000000
	mov r2, #0x00001000
	or r3, sa38, #0x00000000
InitConfigRegisters_1:
	str r0, #1
	imae.u r0, r2.low, #1.low, r0, u32
	imae.u r1, r1.low, #1.low, #1, u32
	xor.testtaz.chan0 !c0, p0, r1, r3
	!p0 ba InitConfigRegisters_1
	nop
	lapc
	nop

UpdateHWPerfRegs:
	!p0 ba UpdateHWPerfRegs_1
	mov r1, #0x000000FF
	str #16, r1
	str #928, #1
	str #928, #0
UpdateHWPerfRegs_1:
	or r0, sa39, #0x00000000
	and.testtaz.chan0 !c0, p0, r0, r0
	p0 mov r1, #0x00000000
	!p0 mov r1, #0x00000001
	str #30, r1
	p0 ba UpdateHWPerfRegs_2
	ldad.bpcache.fetch2 r0, [sa1,+#14], drc0
	ldad.bpcache.fetch2 r2, [sa1,+#22], drc1
	wdf drc0
	shl r0, r0, #0x00000000
	shl r1, r1, #0x00000010
	or r0, r0, r1
	wdf drc1
	shl r2, r2, #0x00000008
	shl r3, r3, #0x00000018
	or r0, r0, r2
	or r0, r0, r3
	str #36, r0
	ldad.bpcache.fetch2 r0, [sa1,+#16], drc0
	ldad.bpcache.fetch2 r2, [sa1,+#24], drc1
	wdf drc0
	shl r0, r0, #0x00000000
	shl r1, r1, #0x00000010
	or r0, r0, r1
	wdf drc1
	shl r2, r2, #0x00000008
	shl r3, r3, #0x00000018
	or r0, r0, r2
	or r0, r0, r3
	str #37, r0
	ldad.bpcache.fetch2 r0, [sa1,+#18], drc0
	ldad.bpcache.fetch2 r2, [sa1,+#26], drc1
	wdf drc0
	shl r0, r0, #0x00000000
	shl r1, r1, #0x00000010
	or r0, r0, r1
	wdf drc1
	shl r2, r2, #0x00000008
	shl r3, r3, #0x00000018
	or r0, r0, r2
	or r0, r0, r3
	str #38, r0
	ldad.bpcache.fetch2 r0, [sa1,+#20], drc0
	ldad.bpcache.fetch2 r2, [sa1,+#28], drc1
	wdf drc0
	shl r0, r0, #0x00000000
	shl r1, r1, #0x00000010
	or r0, r0, r1
	wdf drc1
	shl r2, r2, #0x00000008
	shl r3, r3, #0x00000018
	or r0, r0, r2
	or r0, r0, r3
	str #39, r0
	str #16, #0
UpdateHWPerfRegs_2:
	lapc

StartTimer:
	ldad.bpcache r0, [sa1,+#6], drc0
	wdf drc0
	or r0, r0, #0x01000000
	str #8883, r0
	lapc

HWRecovery:
	ldad.bpcache r0, [sa2,+#0], drc0
	ldad.bpcache r1, [sa2,+#1], drc1
	wdf drc0
	ldad.bpcache r2, [sa0,+#45], drc0
	wdf drc1
	wdf drc0
HWRecovery_1:
	xor.testtaz.chan0 !c0, p0, r0, r1
	p0 ba HWRecovery_10
	imae.u r4, r1.low, #32.low, r2, u32
	ldad r5, [r4,+#0], drc0
	ldad r4, [r4,+#3], drc0
	wdf drc0
	mov r3, #0x000000C1
	xor.testtaz.chan0 !c0, p0, r5, r3
	!p0 ba HWRecovery_3
	ldad.bpcache r5, [r4,+#5], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r5, r5
	imae.u r5, r5.low, #1.low, #1, u32
	stad.bpcache [r4,+#5], r5
	p0 ba HWRecovery_7
	ldad.bpcache r5, [r4,+#1], drc0
	wdf drc0
	imae.u r7, r5.low, #1.low, #35, u32
	or i.l, r7, #0x00000000
	or r8, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p0, r8, r8
	imae.u r7, r5.low, #1.low, #35, u32
	or i.l, r7, #0x00000000
	or sa[il + #0], r4, #0x00000000
	!p0 ba HWRecovery_2
	imae.u r6, r5.low, #1.low, #32, u32
	or i.l, r6, #0x00000000
	or sa[il + #0], r4, #0x00000000
HWRecovery_2:
	!p0 stad.bpcache [r8,+#8], r4
	!p0 stad.bpcache [r4,+#7], r8
	ba HWRecovery_7
HWRecovery_3:
	mov r3, #0x000000DC
	xor.testtaz.chan0 !c0, p0, r5, r3
	!p0 ba HWRecovery_5
	ldad.bpcache r5, [r4,+#5], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r5, r5
	imae.u r5, r5.low, #1.low, #1, u32
	stad.bpcache [r4,+#5], r5
	p0 ba HWRecovery_7
	ldad.bpcache r5, [r4,+#1], drc0
	wdf drc0
	imae.u r7, r5.low, #1.low, #17, u32
	or i.l, r7, #0x00000000
	or r8, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p0, r8, r8
	imae.u r7, r5.low, #1.low, #17, u32
	or i.l, r7, #0x00000000
	or sa[il + #0], r4, #0x00000000
	!p0 ba HWRecovery_4
	imae.u r6, r5.low, #1.low, #14, u32
	or i.l, r6, #0x00000000
	or sa[il + #0], r4, #0x00000000
HWRecovery_4:
	!p0 stad.bpcache [r8,+#44], r4
	!p0 stad.bpcache [r4,+#43], r8
	ba HWRecovery_7
HWRecovery_5:
	mov r3, #0x000000F6
	xor.testtaz.chan0 !c0, p0, r5, r3
	!p0 ba HWRecovery_8
	ldad.bpcache r5, [r4,+#5], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r5, r5
	imae.u r5, r5.low, #1.low, #1, u32
	stad.bpcache [r4,+#5], r5
	p0 ba HWRecovery_7
	ldad.bpcache r5, [r4,+#18], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r5, r5
	p0 ba HWRecovery_7
	ldad.bpcache r5, [r4,+#1], drc0
	wdf drc0
	imae.u r7, r5.low, #1.low, #23, u32
	or i.l, r7, #0x00000000
	or r8, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p0, r8, r8
	imae.u r7, r5.low, #1.low, #23, u32
	or i.l, r7, #0x00000000
	or sa[il + #0], r4, #0x00000000
	!p0 ba HWRecovery_6
	imae.u r6, r5.low, #1.low, #20, u32
	or i.l, r6, #0x00000000
	or sa[il + #0], r4, #0x00000000
HWRecovery_6:
	!p0 stad.bpcache [r8,+#23], r4
	!p0 stad.bpcache [r4,+#22], r8
HWRecovery_7:
	idf drc0, bif
	wdf drc0
HWRecovery_8:
	mov r3, #0x00000113
	xor.testtaz.chan0 !c0, p0, r5, r3
	!p0 ba HWRecovery_9
	ldad.bpcache r3, [sa1,+#2], drc0
	wdf drc0
	or r3, r3, #0x00000003
	stad.bpcache [sa1,+#2], r3
	idf drc0, bif
	wdf drc0
HWRecovery_9:
	imae.u r1, r1.low, #1.low, #1, u32
	and r1, r1, #0x000000FF
	ba HWRecovery_1
HWRecovery_10:
	or r1, sa8, #0x00000000
	and.testtaz.chan0 !c0, p0, r1, #1
	p0 ba HWRecovery_11
	ldad.bpcache r0, [sa0,+#48], drc0
	wdf drc0
	ldad.bpcache r2, [r0,+#0], drc0
	wdf drc0
	or r2, r2, #0x00000004
	stad.bpcache [r0,+#0], r2
HWRecovery_11:
	or r0, sa9, #0x00000000
	and.testtaz.chan0 !c0, p0, r0, #1
	p0 ba HWRecovery_12
	ldad.bpcache r0, [sa0,+#49], drc0
	wdf drc0
	ldad r1, [r0,+#1], drc0
	mov r2, #0x00000009
	wdf drc0
	and.testtaz.chan0 !c0, p0, r1, r2
	p0 ba HWRecovery_12
	ldad.bpcache r0, [sa0,+#50], drc0
	wdf drc0
	stad.bpcache [r0,+#32], #80
HWRecovery_12:
	mov r3, #0x00000000
	or r0, sa20, #0x00000000
	and.testtaz.chan0 !c0, p0, r0, r0
	p0 ba HWRecovery_16
HWRecovery_13:
	ldad.bpcache r1, [r0,+#6], drc0
	wdf drc0
	ldad.bpcache r2, [r1,+#0], drc0
	wdf drc0
	or r2, r2, #0x00000001
	stad.bpcache [r1,+#0], r2
	idf drc0, bif
	wdf drc0
	ldad.bpcache r1, [r0,+#18], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r1, r1
	p0 ba HWRecovery_15
HWRecovery_14:
	ldad.bpcache r2, [r1,+#68], drc0
	wdf drc0
	ldad.bpcache r4, [r2,+#32], drc0
	ldad.bpcache r1, [r1,+#86], drc1
	wdf drc0
	or r4, r4, #0x00000040
	stad.bpcache [r2,+#32], r4
	wdf drc1
	and.testtanz.chan0 !c0, p0, r1, r1
	p0 ba HWRecovery_14
HWRecovery_15:
	ldad.bpcache r0, [r0,+#23], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r0, r0
	p0 ba HWRecovery_13
HWRecovery_16:
	imae.u r3, r3.low, #1.low, #1, u32
	xor.testtaz.chan0 !c0, p0, r3, #3
	p0 ba HWRecovery_17
	imae.u r1, r3.low, #1.low, #20, u32
	or i.l, r1, #0x00000000
	or r0, sa[il + #0], #0x00000000
	and.testtanz.chan0 !c0, p0, r0, r0
	p0 ba HWRecovery_13
	ba HWRecovery_16
HWRecovery_17:
	mov r3, #0x00000000
	or r0, sa26, #0x00000000
	and.testtaz.chan0 !c0, p0, r0, r0
	p0 ba HWRecovery_20
HWRecovery_18:
	ldad.bpcache r1, [r0,+#6], drc0
	wdf drc0
	ldad.bpcache r2, [r1,+#0], drc0
	wdf drc0
	or r2, r2, #0x00000001
	stad.bpcache [r1,+#0], r2
	idf drc0, bif
	wdf drc0
	ldad.bpcache r1, [r0,+#20], drc0
	wdf drc0
HWRecovery_19:
	ldad.bpcache r2, [r1,+#68], drc0
	wdf drc0
	ldad.bpcache r4, [r2,+#32], drc0
	ldad.bpcache r1, [r1,+#86], drc1
	wdf drc0
	or r4, r4, #0x00000040
	stad.bpcache [r2,+#32], r4
	wdf drc1
	and.testtanz.chan0 !c0, p0, r1, r1
	p0 ba HWRecovery_19
	ldad.bpcache r0, [r0,+#25], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r0, r0
	p0 ba HWRecovery_18
HWRecovery_20:
	imae.u r3, r3.low, #1.low, #1, u32
	xor.testtaz.chan0 !c0, p0, r3, #3
	p0 ba HWRecovery_21
	imae.u r1, r3.low, #1.low, #26, u32
	or i.l, r1, #0x00000000
	or r0, sa[il + #0], #0x00000000
	and.testtanz.chan0 !c0, p0, r0, r0
	p0 ba HWRecovery_18
	nop
	ba HWRecovery_20
HWRecovery_21:
	or r1, sa10, #0x00000000
	and.testtaz.chan0 !c0, p0, r1, #4
	p0 ba HWRecovery_22
	or r0, sa7, #0x00000000
	ldad.bpcache r2, [r0,+#0], drc0
	wdf drc0
	or r2, r2, #0x00000002
	stad.bpcache [r0,+#0], r2
HWRecovery_22:
	stad.bpcache [sa0,+#77], #0
	stad.bpcache [sa0,+#78], #0
	stad.bpcache [sa0,+#43], #0
	stad.bpcache [sa0,+#44], #0
	mov sa9, #0x00000000
	stad.bpcache [sa0,+#49], #0
	stad.bpcache [sa0,+#50], #0
	stad.bpcache [sa0,+#51], #0
	stad.bpcache [sa0,+#52], #0
	mov sa5, #0x00000002
	mov sa10, #0x00000000
	stad.bpcache [sa0,+#54], #0
	stad.bpcache [sa0,+#55], #0
	stad.bpcache [sa0,+#56], #0
	mov sa3, #0x00000000
	mov sa8, #0x00000000
	stad.bpcache [sa0,+#47], #0
	stad.bpcache [sa0,+#48], #0
	stad.bpcache [sa0,+#85], #0
	ldad.bpcache r0, [sa2,+#0], drc0
	wdf drc0
	stad.bpcache [sa2,+#1], r0
	ldad.bpcache r0, [sa1,+#8], drc0
	wdf drc0
	and r0, r0, ~#0x00000001
	stad.bpcache [sa1,+#8], r0
	ldad.bpcache r0, [sa1,+#9], drc0
	wdf drc0
	and r0, r0, ~#0x00000001
	stad.bpcache [sa1,+#9], r0
	idf drc0, bif
	wdf drc0
	lapc
