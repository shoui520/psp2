	.align2
ReadSignatureRegisters:
	ldr r7, r3, drc0
	wdf drc0
	and r7, r7, r4
	and.testtaz.chan0 !c0, p0, r2, #1
	p0 shl r7, r7, r6
	xor r0, r0, r7
	isub16.testtaz.chan0 r2, p0, r2, #1
	!p0 imae.u r3, r5.low, #1.low, r3, u32
	!p0 ba ReadSignatureRegisters
	nop
	lapc

CheckTALockup:
	mov r8, pclink
	mov r0, #0x00000000
	or r1, sa38, #0x00000000
	imae.u r1, r1.low, #1.low, #1, u32
	mov r4, #0x0000FFFF
	mov r5, #0x00000003
CheckTALockup_1:
	mov r2, #0x00000002
	mov r3, #0x000002C3
	mov r6, #0x00001000
	imae.u r3, r1.low, r6.low, r3, u32
	mov r6, #0x00000010
	ba.savelink ReadSignatureRegisters
	mov r2, #0x00000002
	mov r3, #0x000002E3
	mov r6, #0x00001000
	imae.u r3, r1.low, r6.low, r3, u32
	mov r6, #0x00000010
	ba.savelink ReadSignatureRegisters
	ima16.s r1, r1, #1, #1.neg, u16, u16
	isub16.testtanz.chan0 !c0, p0, r1, #1
	p0 ba CheckTALockup_1
	mov r2, #0x00000003
	mov r3, #0x00000241
	mov r4, #0xFFFFFFFF
	mov r5, #0x00000001
	mov r6, #0x00000000
	ba.savelink ReadSignatureRegisters
	mov r2, #0x00000001
	mov r3, #0x00000240
	mov r4, #0xFFFFFFFF
	mov r5, #0x00000001
	mov r6, #0x00000000
	ba.savelink ReadSignatureRegisters
	mov r2, #0x00000001
	mov r3, #0x000001C9
	mov r4, #0xFFFF0000
	mov r5, #0x00000001
	mov r6, #0x00000000
	ba.savelink ReadSignatureRegisters
	mov r2, #0x00000001
	mov r3, #0x000010CD
	mov r4, #0xFFFFFFFF
	mov r5, #0x00000001
	mov r6, #0x00000000
	ba.savelink ReadSignatureRegisters
	or r1, sa38, #0x00000000
CheckTALockup_2:
	isub16.testtanz.chan0 r1, p0, r1, #1
	imae.u r2, r1.low, #1.low, #2, u32
	shl r2, r2, #0x0000000C
	or r2, r2, #0x00000A80
	ldr r2, r2, drc0
	wdf drc0
	xor r0, r0, r2
	p0 ba CheckTALockup_2
	ldad.bpcache r2, [sa0,+#88], drc0
	wdf drc0
	stad.bpcache [sa0,+#88], r0
	ldad.bpcache r1, [sa0,+#86], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r1, #1
	p0 ba CheckTALockup_3
	xor r1, r1, #0x00000001
	stad.bpcache [sa0,+#86], r1
	ba CheckTALockup_4
CheckTALockup_3:
	xor.testtanz.chan0 !c0, p0, r0, r2
	p0 ba CheckTALockup_4
	mov r0, #0x00000000
	ba.savelink Lockup
CheckTALockup_4:
	mov pclink, r8
	idf drc1, bif
	wdf drc1
	lapc

Check3DLockup:
	mov r8, pclink
	mov r0, #0x00000000
	or r1, sa38, #0x00000000
	imae.u r1, r1.low, #1.low, #1, u32
	mov r4, #0x0000FFFF
	mov r5, #0x00000003
Check3DLockup_1:
	mov r2, #0x00000002
	mov r3, #0x000002C2
	mov r6, #0x00001000
	imae.u r3, r1.low, r6.low, r3, u32
	mov r6, #0x00000010
	ba.savelink ReadSignatureRegisters
	mov r2, #0x00000002
	mov r3, #0x000002E2
	mov r6, #0x00001000
	imae.u r3, r1.low, r6.low, r3, u32
	mov r6, #0x00000010
	ba.savelink ReadSignatureRegisters
	ima16.s r1, r1, #1, #1.neg, u16, u16
	isub16.testtanz.chan0 !c0, p0, r1, #1
	p0 ba Check3DLockup_1
	or r2, sa38, #0x00000000
	mov r5, #0x00001000
	mov r3, #0x00002139
	mov r4, #0xFFFFFFFF
	mov r6, #0x00000000
	ba.savelink ReadSignatureRegisters
	mov r2, #0x00000004
	mov r3, #0x00000244
	mov r4, #0xFFFFFFFF
	mov r5, #0x00000001
	mov r6, #0x00000000
	ba.savelink ReadSignatureRegisters
	mov r2, #0x00000001
	mov r3, #0x00000249
	mov r4, #0xFFFFFFFF
	mov r5, #0x00000001
	mov r6, #0x00000000
	ba.savelink ReadSignatureRegisters
	ldad.bpcache r2, [sa0,+#90], drc0
	wdf drc0
	stad.bpcache [sa0,+#90], r0
	ldad.bpcache r1, [sa0,+#86], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r1, #2
	p0 ba Check3DLockup_2
	xor r1, r1, #0x00000002
	stad.bpcache [sa0,+#86], r1
	ba Check3DLockup_3
Check3DLockup_2:
	xor.testtanz.chan0 !c0, p0, r0, r2
	p0 ba Check3DLockup_3
	mov r0, #0x00000001
	ba.savelink Lockup
Check3DLockup_3:
	mov pclink, r8
	idf drc1, bif
	wdf drc1
	lapc

Check2DLockup:
	mov r8, pclink
	mov r0, #0x00000000
	mov r2, #0x00000001
	mov r3, #0x0000105F
	mov r4, #0xFFFFFFFF
	mov r4, #0xFFFFFFFF
	ba.savelink ReadSignatureRegisters
	ldad.bpcache r2, [sa0,+#92], drc0
	stad.bpcache [sa0,+#92], r0
	wdf drc0
	ldad.bpcache r1, [sa0,+#86], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r1, #16
	p0 ba Check2DLockup_1
	xor r1, r1, #0x00000010
	stad.bpcache [sa0,+#86], r1
	ba Check2DLockup_2
Check2DLockup_1:
	xor.testtanz.chan0 !c0, p0, r0, r2
	p0 ba Check2DLockup_2
	mov r0, #0x00000002
	ba.savelink Lockup
Check2DLockup_2:
	ldr r3, #769, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, #16
	p0 ba Check2DLockup_3
	mov r0, #0x00000002
	ba.savelink Lockup
Check2DLockup_3:
	mov pclink, r8
	idf drc1, bif
	wdf drc1
	lapc

Lockup:
	and.testtanz.chan0 !c0, p0, r0, #2
	p0 ba Lockup_3
	and.testtanz.chan0 !c0, p0, r0, #1
	!p0 or r1, sa10, #0x00000000
	p0 or r1, sa9, #0x00000000
	!p0 mov r4, #0x00000001
	p0 mov r4, #0x00000001
	and.testtaz.chan0 !c0, p1, r1, r4
	or r1, sa8, #0x00000000
	mov r4, #0x00000001
	and.testtanz.chan0 !c0, p2, r1, r4
	p2 ba Lockup_8
	nop
	p1 ba Lockup_2
	or r1, sa9, #0x00000000
	or r2, sa10, #0x00000000
	p0 ba Lockup_1
	mov r4, #0x00000018
	and r3, r1, r4
	xor.testtaz.chan0 !c0, p1, r3, r4
	p1 ba Lockup_2
	mov r4, #0x00003000
	and.testtanz.chan0 !c0, p1, r2, r4
	p1 ba Lockup_2
	and.testtanz.chan0 !c0, p0, r3, r3
	p0 ba Lockup_8
	mov r4, #0x00000008
	or r1, r1, r4
	or sa9, r1, #0x00000000
	ba Lockup_7
Lockup_1:
	mov r4, #0x00003000
	and r3, r2, #0x00003000
	xor.testtaz.chan0 !c0, p1, r3, r4
	p1 ba Lockup_2
	mov r4, #0x00000018
	and.testtanz.chan0 !c0, p1, r1, r4
	p1 ba Lockup_2
	shl.testnat.chan0 !c0, p1, r2, #25
	!p1 shl.testnat.chan0 !c0, p1, r1, #25
	!p1 and.testtanz.chan0 !c0, p1, r1, #32
	p1 ba Lockup_2
	and.testtanz.chan0 !c0, p1, r3, r3
	p1 ba Lockup_8
	mov r4, #0x00001000
	or r2, r2, r4
	or sa10, r2, #0x00000000
	ba Lockup_7
Lockup_2:
	and.testtanz.chan0 !c0, p0, r0, #1
	p0 or r1, sa10, #0x00000000
	and.testtanz.chan0 !c0, p1, #0x00000000, #0
	p0 and.testtanz.chan0 !c0, p1, r1, #4
	p1 ba Lockup_3
	!p0 ldad.bpcache r1, [sa0,+#50], drc0
	p0 ldad.bpcache r1, [sa0,+#54], drc0
	wdf drc0
	ldad.bpcache r2, [r1,+#32], drc0
	!p0 mov r3, #0x00000C00
	p0 mov r3, #0x00001000
	wdf drc0
	or r2, r2, r3
	stad.bpcache [r1,+#32], r2
Lockup_3:
	xor.testtanz.chan0 !c0, p0, r0, #2
	p0 ba Lockup_4
Lockup_4:
	xor.testtanz.chan0 !c0, p0, r0, #0
	p0 ba Lockup_5
Lockup_5:
	xor.testtanz.chan0 !c0, p0, r0, #1
	p0 ba Lockup_6
Lockup_6:
	ldad.bpcache r0, [sa1,+#3], drc0
	ldad.bpcache r2, [sa1,+#8], drc1
	wdf drc0
	mov r1, #0x00000001
	imae.u r0, r1.low, #1.low, r0, u32
	stad.bpcache [sa1,+#3], r0
	wdf drc1
	or r2, r2, #0x00000001
	stad.bpcache [sa1,+#8], r2
	idf drc0, bif
	wdf drc0
	mov r0, #0x00004000
	str #75, r0
	ba WaitForReset
Lockup_7:
	idf drc0, bif
	wdf drc0
Lockup_8:
	lapc

RenderContextCleanup:
	ldad.bpcache r1, [r0,+#18], drc0
	ldad.bpcache r2, [r0,+#20], drc0
	ldad.bpcache r3, [r0,+#5], drc0
	wdf drc0
	or r2, r1, r2
	or.testtaz.chan0 !c0, p0, r2, r3
	!p0 ba RenderContextCleanup_3
	ldad.bpcache r2, [r0,+#6], drc0
	ldad.bpcache r3, [sa0,+#77], drc0
	ldad.bpcache r4, [sa0,+#78], drc1
	wdf drc0
	ldad.bpcache r5, [r0,+#0], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r5, #32
	p0 ba RenderContextCleanup_1
	xor.testtaz.chan0 !c0, p0, r2, r3
	p0 stad.bpcache [sa0,+#77], #0
RenderContextCleanup_1:
	wdf drc1
	and.testtaz.chan0 !c0, p0, r5, #32
	p0 ba RenderContextCleanup_2
	xor.testtaz.chan0 !c0, p0, r2, r4
	p0 stad.bpcache [sa0,+#78], #0
RenderContextCleanup_2:
	or r3, sa9, #0x00000000
	and r3, r3, ~#0x00200000
	or sa9, r3, #0x00000000
	mov r16, #0x00000001
	mov r17, #0x00000010
	mov r23, pclink
	ba.savelink InvalidateSLC
	mov pclink, r23
	ldad.bpcache r3, [sa1,+#2], drc0
	wdf drc0
	or r3, r3, #0x00000001
	stad.bpcache [sa1,+#2], r3
	or r3, sa11, #0x00000000
	and r3, r3, ~#0x00000008
	or sa11, r3, #0x00000000
	idf drc0, bif
	wdf drc0
	lapc
RenderContextCleanup_3:
	and.testtanz.chan0 !c0, p0, r1, r1
	p0 and.testtaz.chan0 !c0, p0, r3, r3
	!p0 ba RenderContextCleanup_7
	or r2, sa9, #0x00000000
	and.testtanz.chan0 !c0, p0, r2, #1
	p0 or r2, r2, #0x00200000
	!p0 and r2, r2, ~#0x00200000
	or sa9, r2, #0x00000000
	p0 ba RenderContextCleanup_7
	or r6, r1, #0x00000000
RenderContextCleanup_4:
	ldad.bpcache r14, [r6,+#68], drc0
	wdf drc0
	ldad.bpcache r2, [r14,+#32], drc0
	wdf drc0
	or r2, r2, #0x00000060
	stad.bpcache [r14,+#32], r2
	idf drc0, bif
	mov r1, pclink
	ba.savelink FreeContextMemory
	mov pclink, r1
	or r16, r0, #0x00000000
	or r17, r6, #0x00000000
	mov r23, pclink
	ba.savelink MoveSceneToCompleteList
	mov pclink, r23
	ldad.bpcache r6, [r0,+#18], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r6, r6
	p0 ba RenderContextCleanup_4
	ldad.bpcache r1, [r0,+#1], drc0
	wdf drc0
	ldad.bpcache r4, [r0,+#22], drc0
	ldad.bpcache r5, [r0,+#23], drc1
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, r4
	wdf drc1
	!p0 ba RenderContextCleanup_5
	imae.u r2, r1.low, #1.low, #20, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r5, #0x00000000
RenderContextCleanup_5:
	!p0 stad.bpcache [r4,+#23], r5
	and.testtaz.chan0 !c0, p1, r5, r5
	!p1 ba RenderContextCleanup_6
	imae.u r3, r1.low, #1.low, #23, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r4, #0x00000000
RenderContextCleanup_6:
	!p1 stad.bpcache [r5,+#22], r4
	stad.bpcache [r0,+#22], #0
	stad.bpcache [r0,+#23], #0
	idf drc1, bif
	wdf drc1
	mov r23, pclink
	ba.savelink Check3DQueues
	mov pclink, r23
	nop
RenderContextCleanup_7:
	lapc

TransferContextCleanup:
	ldad.bpcache r1, [r0,+#5], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r1, r1
	!p0 ba TransferContextCleanup_1
	mov r16, #0x00000001
	mov r17, #0x00000010
	mov r23, pclink
	ba.savelink InvalidateSLC
	mov pclink, r23
	ldad.bpcache r3, [sa1,+#2], drc0
	wdf drc0
	or r3, r3, #0x00000001
	stad.bpcache [sa1,+#2], r3
	or r3, sa11, #0x00000000
	and r3, r3, ~#0x00000010
	or sa11, r3, #0x00000000
	idf drc0, bif
	wdf drc0
	lapc
TransferContextCleanup_1:
	lapc

TwoDContextCleanup:
	ldad.bpcache r1, [r0,+#5], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r1, r1
	!p0 ba TwoDContextCleanup_1
	mov r16, #0x00000001
	mov r17, #0x00000010
	mov r23, pclink
	ba.savelink InvalidateSLC
	mov pclink, r23
	ldad.bpcache r3, [sa1,+#2], drc0
	wdf drc0
	or r3, r3, #0x00000001
	stad.bpcache [sa1,+#2], r3
	or r3, sa11, #0x00000000
	and r3, r3, ~#0x00000020
	or sa11, r3, #0x00000000
	idf drc0, bif
	wdf drc0
	lapc
TwoDContextCleanup_1:
	lapc

RenderTargetCleanup:
	mov r1, #0x00000018
	imae.u r14, r1.low, #1.low, r0, u32
	ldad.bpcache r8, [r0,+#5], drc0
	mov r7, #0x00000001
	ldad.bpcache r9, [r14,+#0], drc0
	ldad.bpcache r3, [r0,+#0], drc1
	ldad.bpcache r4, [r0,+#1], drc0
	wdf drc1
	ldad.bpcache r3, [r3,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r3, r4
	p0 ba RenderTargetCleanup_1
	nop
	ba RenderTargetCleanup_9
RenderTargetCleanup_1:
	ldad.bpcache r2, [r14,+#32], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r2, #16
	p0 ba RenderTargetCleanup_3
	mov r7, #0x00000000
	and.testtanz.chan0 !c0, p0, r2, #32
	p0 ba RenderTargetCleanup_3
	or r3, sa9, #0x00000000
	ldad.bpcache r6, [sa0,+#50], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r3, #1
	!p0 and r3, r3, ~#0x00200000
	!p0 ba RenderTargetCleanup_2
	xor.testtanz.chan0 !c0, p1, r14, r6
	p1 or r3, r3, #0x00200000
	!p1 and r3, r3, ~#0x00200000
RenderTargetCleanup_2:
	or sa9, r3, #0x00000000
	p0 ba RenderTargetCleanup_9
	or r2, r2, #0x00000060
	stad.bpcache [r14,+#32], r2
	idf drc0, bif
	ldad.bpcache r0, [r14,+#0], drc0
	wdf drc0
	mov r1, pclink
	ba.savelink FreeContextMemory
	mov pclink, r1
	ldad.bpcache r6, [r14,+#1], drc0
	wdf drc0
	or r16, r9, #0x00000000
	or r17, r6, #0x00000000
	mov r23, pclink
	ba.savelink MoveSceneToCompleteList
	mov pclink, r23
RenderTargetCleanup_3:
	mov r1, #0x00000084
	imae.u r14, r1.low, #1.low, r14, u32
	isub16.testtanz.chan0 r8, p0, r8, #1
	p0 ba RenderTargetCleanup_1
	xor.testtaz.chan0 !c0, p0, r7, #1
	!p0 ba RenderTargetCleanup_4
	mov r16, #0x00000001
	mov r17, #0x00000010
	mov r23, pclink
	ba.savelink InvalidateSLC
	mov pclink, r23
	or r3, sa9, #0x00000000
	and r3, r3, ~#0x00200000
	or sa9, r3, #0x00000000
	ldad.bpcache r3, [sa1,+#2], drc0
	wdf drc0
	or r3, r3, #0x00000001
	stad.bpcache [sa1,+#2], r3
	or r3, sa11, #0x00000000
	and r3, r3, ~#0x00000004
	or sa11, r3, #0x00000000
	idf drc0, bif
	wdf drc0
	ba RenderTargetCleanup_9
RenderTargetCleanup_4:
	ldad.bpcache r1, [r9,+#22], drc0
	ldad.bpcache r2, [r9,+#23], drc0
	wdf drc0
	or.testtanz.chan0 !c0, p0, r1, r2
	p0 ba RenderTargetCleanup_5
	ldad.bpcache r1, [r9,+#1], drc0
	wdf drc0
	imae.u r2, r1.low, #1.low, #20, u32
	or i.l, r2, #0x00000000
	or r3, sa[il + #0], #0x00000000
	xor.testtanz.chan0 !c0, p0, r9, r3
	p0 ba RenderTargetCleanup_8
RenderTargetCleanup_5:
	ldad.bpcache r1, [r9,+#18], drc0
	ldad.bpcache r2, [r9,+#5], drc0
	wdf drc0
	or.testtanz.chan0 !c0, p0, r1, r2
	p0 ba RenderTargetCleanup_8
	ldad.bpcache r1, [r9,+#1], drc0
	wdf drc0
	ldad.bpcache r4, [r9,+#22], drc0
	ldad.bpcache r5, [r9,+#23], drc1
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, r4
	wdf drc1
	!p0 ba RenderTargetCleanup_6
	imae.u r2, r1.low, #1.low, #20, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r5, #0x00000000
RenderTargetCleanup_6:
	!p0 stad.bpcache [r4,+#23], r5
	and.testtaz.chan0 !c0, p1, r5, r5
	!p1 ba RenderTargetCleanup_7
	imae.u r2, r1.low, #1.low, #23, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r4, #0x00000000
RenderTargetCleanup_7:
	!p1 stad.bpcache [r5,+#22], r4
	stad.bpcache [r9,+#22], #0
	stad.bpcache [r9,+#23], #0
	idf drc1, bif
	wdf drc1
RenderTargetCleanup_8:
	mov r23, pclink
	ba.savelink Check3DQueues
	mov pclink, r23
RenderTargetCleanup_9:
	lapc

SharedPBDescCleanup:
	ldad.bpcache r1, [sa0,+#77], drc0
	ldad.bpcache r2, [sa0,+#78], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r0, r1
	p0 stad.bpcache [sa0,+#77], #0
	xor.testtaz.chan0 !c0, p0, r0, r2
	p0 stad.bpcache [sa0,+#78], #0
	ldad.bpcache r3, [sa1,+#2], drc0
	wdf drc0
	or r3, r3, #0x00000001
	stad.bpcache [sa1,+#2], r3
	or r3, sa11, #0x00000000
	and r3, r3, ~#0x00000040
	or sa11, r3, #0x00000000
	idf drc0, bif
	wdf drc0
	lapc

Timer:
	or r0, sa9, #0x00000000
	and.testtanz.chan0 !c0, p0, r0, #1
	!p0 ba Timer_2
	and.testtanz.chan0 !c0, p0, r0, #32
	or r1, sa10, #0x00000000
	p0 and.testtanz.chan0 !c0, p0, r1, #1
	p0 ba Timer_2
	shl.testnat.chan0 !c0, p0, r0, #25
	p0 ba Timer_2
	or r0, sa10, #0x00000000
	shl.testnat.chan0 !c0, p0, r0, #25
	p0 ba Timer_2
	ldad.bpcache r0, [sa0,+#87], drc0
	ldad.bpcache r1, [sa1,+#5], drc0
	wdf drc0
	imae.u r0, r0.low, #1.low, #1, u32
	xor.testtaz.chan0 !c0, p0, r0, r1
	p0 mov r0, #0x00000000
	stad.bpcache [sa0,+#87], r0
	idf drc0, bif
	wdf drc0
	!p0 ba Timer_2
	ldad.bpcache r0, [sa0,+#49], drc0
	wdf drc0
	ldad r0, [r0,+#1], drc0
	wdf drc0
	mov r1, #0x00004000
	and.testtaz.chan0 !c0, p0, r0, r1
	p0 ba Timer_1
	ldad.bpcache r0, [sa1,+#31], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r0, r0
	p0 imae.u r0, r0.low, #1.low, #127, u32
	stad.bpcache [sa1,+#31], r0
	idf drc0, bif
	wdf drc0
	p0 ba Timer_2
	nop
Timer_1:
	ba.savelink CheckTALockup
Timer_2:
	or r0, sa10, #0x00000000
	and.testtanz.chan0 !c0, p0, r0, #1
	!p0 ba Timer_3
	ldad.bpcache r0, [sa0,+#89], drc0
	ldad.bpcache r1, [sa1,+#5], drc0
	wdf drc0
	imae.u r0, r0.low, #1.low, #1, u32
	xor.testtaz.chan0 !c0, p0, r0, r1
	p0 mov r0, #0x00000000
	stad.bpcache [sa0,+#89], r0
	idf drc0, bif
	wdf drc0
	p0 ba.savelink Check3DLockup
Timer_3:
	or r0, sa8, #0x00000000
	and.testtanz.chan0 !c0, p0, r0, #1
	!p0 ba Timer_4
	ldad.bpcache r0, [sa0,+#91], drc0
	ldad.bpcache r1, [sa1,+#5], drc0
	wdf drc0
	imae.u r0, r0.low, #1.low, #1, u32
	xor.testtaz.chan0 !c0, p0, r0, r1
	p0 mov r0, #0x00000000
	stad.bpcache [sa0,+#91], r0
	idf drc0, bif
	wdf drc0
	p0 ba.savelink Check2DLockup
	nop
Timer_4:
	ba.savelink CheckHostCleanupRequest
	or r0, sa12, #0x00000000
	and.testtaz.chan0 !c0, p0, r0, r0
	p0 ba Timer_5
	isub16.testtaz.chan0 r0, p0, r0, #1
	and r0, r0, #0x0000FFFF
	or sa12, r0, #0x00000000
	!p0 ba Timer_5
	ldad.bpcache r1, [sa1,+#8], drc0
	wdf drc0
	or r1, r1, #0x00000002
	stad.bpcache [sa1,+#8], r1
	idf drc0, bif
	wdf drc0
	mov r0, #0x00004000
	str #75, r0
Timer_5:
	ba.savelink CheckHostPowerRequest
	mov pclink, r15
	lapc

CheckHostCleanupRequest:
	or r3, sa11, #0x00000000
	and.testtaz.chan0 !c0, p0, r3, #92
	p0 and.testtaz.chan0 !c0, p0, r3, #32
	p0 ba CheckHostCleanupRequest_1
	ldad.bpcache r0, [sa0,+#57], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r3, #8
	p0 ba RenderContextCleanup
	and.testtanz.chan0 !c0, p0, r3, #4
	p0 ba RenderTargetCleanup
	and.testtanz.chan0 !c0, p0, r3, #16
	p0 ba TransferContextCleanup
	and.testtanz.chan0 !c0, p0, r3, #32
	p0 ba TwoDContextCleanup
	and.testtanz.chan0 !c0, p0, r3, #64
	p0 ba SharedPBDescCleanup
	nop
CheckHostCleanupRequest_1:
	lapc

CheckHostPowerRequest:
	or r6, sa11, #0x00000000
	and.testtanz.chan0 !c0, p0, r6, #3
	!p0 ba CheckHostPowerRequest_20
	mov r1, #0x00000000
	ldad.bpcache r0, [sa1,+#1], drc0
	wdf drc0
	or r2, sa10, #0x00000000
	and.testtaz.chan0 !c0, p0, r2, #1
	p0 ba CheckHostPowerRequest_2
	or r16, sa10, #0x00000000
	and.testtanz.chan0 !c0, p2, r16, #14
	p2 ba CheckHostPowerRequest_1
	or r16, r16, #0x00000008
	or sa10, r16, #0x00000000
	idf drc0, bif
	wdf drc0
CheckHostPowerRequest_1:
	or r1, r1, #0x00000001
CheckHostPowerRequest_2:
	or r2, sa9, #0x00000000
	and.testtaz.chan0 !c0, p0, r2, #1
	p0 ba CheckHostPowerRequest_3
	or r1, r1, #0x00000001
CheckHostPowerRequest_3:
	or r2, sa8, #0x00000000
	and.testtaz.chan0 !c0, p0, r2, #1
	p0 ba CheckHostPowerRequest_4
	or r1, r1, #0x00000001
CheckHostPowerRequest_4:
	xor.testtaz.chan0 !c0, p0, r1, #1
	p0 ba CheckHostPowerRequest_20
	ldr r1, #708, drc0
	mov r2, #0x00010000
	wdf drc0
	and.testtanz.chan0 !c0, p0, r1, r2
	p0 ba CheckHostPowerRequest_5
	nop
	ba CheckHostPowerRequest_20
CheckHostPowerRequest_5:
	str #8883, #0
	or r0, r0, #0x00000004
	and.testtanz.chan0 !c0, p0, r6, #1
	!p0 ba CheckHostPowerRequest_18
	ldad.bpcache r1, [sa0,+#77], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r1, r1
	p0 ba CheckHostPowerRequest_6
	mov r23, pclink
	ba.savelink StoreTAPB
	mov pclink, r23
CheckHostPowerRequest_6:
	ldad.bpcache r2, [sa0,+#78], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r1, r2
	p0 ba CheckHostPowerRequest_7
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba CheckHostPowerRequest_7
	mov r23, pclink
	ba.savelink Store3DPB
	mov pclink, r23
CheckHostPowerRequest_7:
	stad.bpcache [sa0,+#77], #0
	stad.bpcache [sa0,+#78], #0
	mov r3, #0x00000000
CheckHostPowerRequest_8:
	imae.u r4, r3.low, #1.low, #43, u32
	imae.u r4, r4.low, #4.low, sa0, u32
	ldad.bpcache r5, [r4,+#0], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r5, r5
	p0 ba CheckHostPowerRequest_10
	stad.bpcache [r4,+#0], #0
	idf drc0, bif
	wdf drc0
	ldad.bpcache r1, [r5,+#0], drc0
	wdf drc0
	ldad.bpcache r2, [r5,+#32], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r2, #32
	p0 ba CheckHostPowerRequest_9
	or r16, r1, #0x00000000
	mov r17, #0x00000000
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	or r16, r5, #0x00000000
	or r17, r3, #0x00000000
	mov r18, #0x00000000
	mov r23, pclink
	ba.savelink StoreDPMContext
	mov pclink, r23
	ba CheckHostPowerRequest_10
CheckHostPowerRequest_9:
	or r16, r1, #0x00000000
	mov r17, #0x00000001
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	or r16, r5, #0x00000000
	or r17, r3, #0x00000000
	mov r18, #0x00000200
	mov r23, pclink
	ba.savelink StoreDPMContext
	mov pclink, r23
CheckHostPowerRequest_10:
	imae.u r3, r3.low, #1.low, #1, u32
	xor.testtanz.chan0 !c0, p0, r3, #2
	p0 ba CheckHostPowerRequest_8
	stad.bpcache [sa0,+#43], #0
	stad.bpcache [sa0,+#44], #0
	stad.bpcache [sa0,+#50], #0
	stad.bpcache [sa0,+#54], #0
	ldad.bpcache r2, [sa2,+#0], drc0
	ldad.bpcache r3, [sa2,+#1], drc0
	wdf drc0
	xor.testtanz.chan0 !c0, p0, r2, r3
	!p0 ba CheckHostPowerRequest_11
	mov r16, #0xAD001A07
	stad.bpcache [sa1,+#14], r16
	idf drc0, bif
	wdf drc0
	ba WaitForReset
CheckHostPowerRequest_11:
	or r2, sa13, #0x00000000
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba CheckHostPowerRequest_12
	or r0, r0, #0x00000010
	ba CheckHostPowerRequest_17
CheckHostPowerRequest_12:
	mov r2, #0x00000000
	mov r3, #0x00000000
CheckHostPowerRequest_13:
	imae.u r4, r3.low, #1.low, #20, u32
	or i.l, r4, #0x00000000
	or r2, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 imae.u r3, r3.low, #1.low, #1, u32
	p0 xor.testtanz.chan0 !c0, p0, r3, #3
	p0 ba CheckHostPowerRequest_13
	mov r3, #0x00000000
CheckHostPowerRequest_14:
	imae.u r5, r3.low, #1.low, #26, u32
	or i.l, r5, #0x00000000
	or r4, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p0, r4, r4
	p0 imae.u r3, r3.low, #1.low, #1, u32
	p0 xor.testtanz.chan0 !c0, p0, r3, #3
	p0 ba CheckHostPowerRequest_14
	or r2, r2, r4
	mov r3, #0x00000000
CheckHostPowerRequest_15:
	imae.u r5, r3.low, #1.low, #14, u32
	or i.l, r5, #0x00000000
	or r4, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p0, r4, r4
	p0 imae.u r3, r3.low, #1.low, #1, u32
	p0 xor.testtanz.chan0 !c0, p0, r3, #3
	p0 ba CheckHostPowerRequest_15
	or r2, r2, r4
	mov r3, #0x00000000
CheckHostPowerRequest_16:
	imae.u r5, r3.low, #1.low, #32, u32
	or i.l, r5, #0x00000000
	or r4, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p0, r4, r4
	p0 imae.u r3, r3.low, #1.low, #1, u32
	p0 xor.testtanz.chan0 !c0, p0, r3, #3
	p0 ba CheckHostPowerRequest_16
	or r2, r2, r4
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 or r0, r0, #0x00000020
CheckHostPowerRequest_17:
	or r0, r0, #0x00000008
CheckHostPowerRequest_18:
	and r1, r6, ~#0x00000003
	or sa11, r1, #0x00000000
	stad.bpcache [sa0,+#56], #0
	and.testtanz.chan0 !c0, p0, r6, #1
	!p0 ba CheckHostPowerRequest_19
	nop
	ba.savelink StoreSecondaries
CheckHostPowerRequest_19:
	stad.bpcache [sa1,+#1], r0
	mov r16, #0x00000001
	mov r23, pclink
	ba.savelink CommonProgramEnd
	mov pclink, r23
CheckHostPowerRequest_20:
	lapc

StoreSecondaries:
	mov r3, #0x0000002B
	mov r1, #0x00000000
StoreSecondaries_1:
	or r2, r1, #0x00040000
	or i.l, r1, #0x00000000
	stad.bpcache [sa0,+r2], sa[il + #0]
	imae.u r1, r1.low, #1.low, #1, u32
	xor.testtaz.chan0 !c0, p0, r1, r3
	!p0 ba StoreSecondaries_1
	idf drc0, bif
	wdf drc0
	lapc
