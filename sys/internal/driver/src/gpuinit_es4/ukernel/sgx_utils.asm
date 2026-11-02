	.align2
InvalidateBIFCache:
	and.testtanz.chan0 !c0, p0, r16, r16
	p0 str #781, #8
	!p0 str #781, #4
	lapc

CheckTAQueue:
	mov r16, #0x00000000
CheckTAQueue_1:
	imae.u r17, r16.low, #1.low, #20, u32
	or i.l, r17, #0x00000000
	or r18, sa[il + #0], #0x00000000
	and.testtanz.chan0 !c0, p2, r18, r18
	p2 mov r16, #0x00040000
	p2 ba EmitLoopBack
	imae.u r16, r16.low, #1.low, #1, u32
	xor.testtanz.chan0 !c0, p2, r16, #3
	p2 ba CheckTAQueue_1
	mov r16, #0x00000001
	ba ClearActivePowerFlags

Check3DQueues:
	mov r16, #0x00000000
Check3DQueues_1:
	imae.u r17, r16.low, #1.low, #14, u32
	or i.l, r17, #0x00000000
	or r19, sa[il + #0], #0x00000000
	imae.u r18, r16.low, #1.low, #26, u32
	or i.l, r18, #0x00000000
	or r20, sa[il + #0], #0x00000000
	or.testtanz.chan0 !c0, p2, r19, r20
	p2 mov r16, #0x00080000
	p2 ba EmitLoopBack
	imae.u r16, r16.low, #1.low, #1, u32
	xor.testtanz.chan0 !c0, p2, r16, #3
	p2 ba Check3DQueues_1
	mov r16, #0x00000002
	ba ClearActivePowerFlags

Check2DQueue:
	mov r16, #0x00000000
Check2DQueue_1:
	imae.u r17, r16.low, #1.low, #32, u32
	or i.l, r17, #0x00000000
	or r18, sa[il + #0], #0x00000000
	and.testtanz.chan0 !c0, p2, r18, r18
	p2 mov r16, #0x00400000
	p2 ba EmitLoopBack
	imae.u r16, r16.low, #1.low, #1, u32
	xor.testtanz.chan0 !c0, p2, r16, #3
	p2 ba Check2DQueue_1
	mov r16, #0x00000004
	ba ClearActivePowerFlags

Resume:
	ldad.bpcache r16, [sa0,+#85], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p3, r16, #0
	p3 ba Resume_1
	imae.s r16, r16.low, #1.low, #127, u32
	stad.bpcache [sa0,+#85], r16
	idf drc0, bif
	wdf drc0
	xor.testtanz.chan0 !c0, p3, r16, #0
	p3 ba Resume_2
Resume_1:
	mov r16, #0x00001200
	ldr r16, r16, drc0
	wdf drc0
	and r16, r16, ~#0x80000000
	mov r17, #0x00001200
	str r17, r16
	ldr r16, #669, drc0
	wdf drc0
	and r16, r16, ~#0x00000007
	str #669, r16
Resume_2:
	lapc

IdleCore:
	ldad.bpcache r17, [sa0,+#85], drc0
	wdf drc0
	imae.u r17, r17.low, #1.low, #1, u32
	stad.bpcache [sa0,+#85], r17
	idf drc0, bif
	wdf drc0
IdleCore_1:
	and r17, r16, #0x00000003
	xor.testtaz.chan0 !c0, p2, r17, #1
	p2 or r17, sa10, #0x00000000
	p2 ba IdleCore_2
	or r17, sa9, #0x00000000
	and.testtanz.chan0 !c0, p2, r17, #32
	p2 ba IdleCore_36
IdleCore_2:
	and.testtaz.chan0 !c0, p2, r17, #1
	p2 ba IdleCore_36
	ldr r17, #669, drc0
	wdf drc0
	or r17, r17, #0x00000007
	str #669, r17
	mov r17, #0x00001200
	ldr r17, r17, drc0
	wdf drc0
	or r17, r17, #0x80000000
	mov r18, #0x00001200
	str r18, r17
IdleCore_3:
	and r17, r16, #0x00000003
	xor.testtaz.chan0 !c0, p2, r17, #1
	p2 ba IdleCore_8
IdleCore_4:
	ldr r17, #577, drc0
	mov r18, #0x000003E8
	wdf drc0
IdleCore_5:
	isub16.testpat.chan0 r18, p2, r18, #1
	p2 ba IdleCore_5
	ldr r18, #577, drc0
	wdf drc0
	xor.testtanz.chan0 !c0, p2, r17, r18
	p2 ba IdleCore_4
IdleCore_6:
	ldr r17, #578, drc0
	ldr r18, #579, drc0
	wdf drc0
	xor r19, r17, r18
	mov r18, #0x000003E8
IdleCore_7:
	isub16.testpat.chan0 r18, p2, r18, #1
	p2 ba IdleCore_7
	ldr r17, #578, drc0
	ldr r18, #579, drc0
	wdf drc0
	xor r17, r17, r18
	xor.testtanz.chan0 !c0, p2, r19, r17
	p2 ba IdleCore_6
	ba IdleCore_12
IdleCore_8:
	ldr r17, #583, drc0
	ldr r18, #584, drc0
	wdf drc0
	xor r19, r17, r18
	mov r18, #0x000003E8
IdleCore_9:
	isub16.testpat.chan0 r18, p2, r18, #1
	p2 ba IdleCore_9
	ldr r17, #583, drc0
	ldr r18, #584, drc0
	wdf drc0
	xor r17, r17, r18
	xor.testtanz.chan0 !c0, p2, r19, r17
	p2 ba IdleCore_8
IdleCore_10:
	ldr r19, #585, drc0
	mov r18, #0x000003E8
IdleCore_11:
	isub16.testpat.chan0 r18, p2, r18, #1
	p2 ba IdleCore_11
	ldr r17, #585, drc0
	xor.testtanz.chan0 !c0, p2, r19, r17
	p2 ba IdleCore_10
IdleCore_12:
	or r19, sa38, #0x00000000
	ima16.s r19, r19, #1, #1.neg, u16, u16
IdleCore_13:
	mov r20, #0x00000000
	mov r21, #0x00000019
IdleCore_14:
	and r17, r16, #0x00000003
	xor.testtaz.chan0 !c0, p2, r17, #1
	p2 ba IdleCore_15
	imae.u r17, r19.low, #1.low, #2, u32
	shl r17, r17, #0x0000000C
	or r17, r17, #0x000002C3
	ldr r17, r17, drc0
	imae.u r18, r19.low, #1.low, #2, u32
	shl r18, r18, #0x0000000C
	or r18, r18, #0x000002C6
	ldr r18, r18, drc0
	wdf drc0
	and r17, r17, r18
	imae.u r18, r19.low, #1.low, #2, u32
	shl r18, r18, #0x0000000C
	or r18, r18, #0x000002E3
	ldr r18, r18, drc0
	wdf drc0
	and r17, r17, r18
	imae.u r18, r19.low, #1.low, #2, u32
	shl r18, r18, #0x0000000C
	or r18, r18, #0x000002E6
	ldr r18, r18, drc0
	wdf drc0
	and r17, r17, r18
	ba IdleCore_16
IdleCore_15:
	imae.u r17, r19.low, #1.low, #2, u32
	shl r17, r17, #0x0000000C
	or r17, r17, #0x000002C2
	ldr r17, r17, drc0
	imae.u r18, r19.low, #1.low, #2, u32
	shl r18, r18, #0x0000000C
	or r18, r18, #0x000002C5
	ldr r18, r18, drc0
	wdf drc0
	and r17, r17, r18
	imae.u r18, r19.low, #1.low, #2, u32
	shl r18, r18, #0x0000000C
	or r18, r18, #0x000002E2
	ldr r18, r18, drc0
	wdf drc0
	and r17, r17, r18
	imae.u r18, r19.low, #1.low, #2, u32
	shl r18, r18, #0x0000000C
	or r18, r18, #0x000002E5
	ldr r18, r18, drc0
	wdf drc0
	and r17, r17, r18
IdleCore_16:
	mov r18, #0x00010000
	and.testtanz.chan0 !c0, p2, r17, r18
	p2 ba IdleCore_18
	mov r20, #0x00000001
	isub16.testnat.chan0 r21, p2, r21, #1
	p2 ba IdleCore_37
	mov r18, #0x000001F4
IdleCore_17:
	isub16.testpat.chan0 r18, p2, r18, #1
	p2 ba IdleCore_17
	ba IdleCore_14
IdleCore_18:
	isub16.testpat.chan0 r19, p2, r19, #1
	p2 ba IdleCore_13
	or r19, sa38, #0x00000000
	ima16.s r19, r19, #1, #1.neg, u16, u16
IdleCore_19:
	mov r21, #0x00000019
IdleCore_20:
	mov r18, #0xAAAAAAAA
	imae.u r17, r19.low, #1.low, #2, u32
	shl r17, r17, #0x0000000C
	or r17, r17, #0x000002AA
	ldr r17, r17, drc0
	wdf drc0
	xor.testtanz.chan0 !c0, p2, r17, r18
	p2 ba IdleCore_21
	imae.u r17, r19.low, #1.low, #2, u32
	shl r17, r17, #0x0000000C
	or r17, r17, #0x000002E0
	ldr r17, r17, drc0
	wdf drc0
	xor.testtanz.chan0 !c0, p2, r17, r18
	p2 ba IdleCore_21
	imae.u r17, r19.low, #1.low, #2, u32
	shl r17, r17, #0x0000000C
	or r17, r17, #0x000002E1
	ldr r17, r17, drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p2, r17, r18
	p2 ba IdleCore_23
IdleCore_21:
	mov r20, #0x00000001
	isub16.testnat.chan0 r21, p2, r21, #1
	p2 ba IdleCore_37
	mov r18, #0x000001F4
IdleCore_22:
	isub16.testpat.chan0 r18, p2, r18, #1
	p2 ba IdleCore_22
	ba IdleCore_20
IdleCore_23:
	mov r21, #0x00000019
	mov r18, #0xAAAAAAAA
	isub16.testpat.chan0 !c0, p2, r19, #1
	p2 ba IdleCore_24
	shl r17, g22, #0x00000001
	shl r17, #3, r17
	or r17, r17, r18
IdleCore_24:
	imae.u r18, r19.low, #1.low, #2, u32
	shl r18, r18, #0x0000000C
	or r18, r18, #0x000002A9
	ldr r18, r18, drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p2, r17, r18
	p2 ba IdleCore_26
	mov r20, #0x00000001
	isub16.testnat.chan0 r21, p2, r21, #1
	p2 ba IdleCore_37
	mov r18, #0x000001F4
IdleCore_25:
	isub16.testpat.chan0 r18, p2, r18, #1
	p2 ba IdleCore_25
	nop
	ba IdleCore_24
IdleCore_26:
	isub16.testpat.chan0 r19, p2, r19, #1
	p2 ba IdleCore_19
	xor.testtaz.chan0 !c0, p2, r20, #1
	p2 ba IdleCore_3
IdleCore_27:
	or r19, sa38, #0x00000000
	mov r20, #0x00001209
	ima16.s r19, r19, #1, #1.neg, u16, u16
	mov r17, #0x0000FFFF
IdleCore_28:
	ldr r18, r20, drc0
	wdf drc0
	imae.u r20, r20.low, #1.low, #1, u32
	and r17, r17, r18
	isub16.testpat.chan0 r19, p2, r19, #1
	p2 ba IdleCore_28
	xor.testtaz.chan0 !c0, p2, r17, #0
	p2 ba IdleCore_30
	mov r17, #0x00000050
IdleCore_29:
	isub16.testpat.chan0 r17, p2, r17, #1
	p2 ba IdleCore_29
	ba IdleCore_27
IdleCore_30:
	ldr r18, #4565, drc0
	wdf drc0
	mov r17, #0x00000014
IdleCore_31:
	isubu16.testtanz.chan0 r17, p2, r17, #1
	p2 ba IdleCore_31
	ldr r19, #4565, drc0
	wdf drc0
	and r18, r18, r19
	mov r17, #0x00000007
	and r18, r18, r17
	xor.testtanz.chan0 !c0, p2, r18, r17
	p2 ba IdleCore_30
	and r17, r16, #0x00000003
	xor.testtaz.chan0 !c0, p2, r17, #1
	p2 ba IdleCore_34
IdleCore_32:
	ldr r18, #451, drc0
	wdf drc0
	mov r17, #0x00000028
IdleCore_33:
	isubu16.testtanz.chan0 r17, p2, r17, #1
	p2 ba IdleCore_33
	ldr r17, #451, drc0
	wdf drc0
	xor.testtanz.chan0 !c0, p2, r17, r18
	p2 ba IdleCore_32
	nop
	ba IdleCore_36
IdleCore_34:
	ldr r18, #448, drc0
	wdf drc0
	mov r17, #0x00000028
IdleCore_35:
	isubu16.testtanz.chan0 r17, p2, r17, #1
	p2 ba IdleCore_35
	ldr r17, #448, drc0
	wdf drc0
	xor.testtanz.chan0 !c0, p2, r17, r18
	p2 ba IdleCore_34
IdleCore_36:
	lapc
IdleCore_37:
	ldr r17, #669, drc0
	wdf drc0
	and r17, r17, ~#0x00000007
	str #669, r17
	mov r18, #0x000003E8
IdleCore_38:
	isub16.testpat.chan0 r18, p2, r18, #1
	p2 ba IdleCore_38
	ba IdleCore_1

DPMThresholdUpdate:
	xor.testtanz.chan0 !c0, p3, r17, #1
	ldad.bpcache r16, [sa0,+#77], drc0
	wdf drc0
	ldad.fetch4 r17, [r16,+#9], drc0
	wdf drc0
	str #392, r17
	ldad.bpcache r17, [r16,+#0], drc0
	and r19, r19, #0x0000FFFF
	or r19, r19, #0x00010000
	str #394, r19
	str #389, r20
	p3 ba DPMThresholdUpdate_1
	and.testtaz.chan0 !c0, p3, r20, r20
	or r20, sa9, #0x00000000
	mov r19, #0xDFFFFFFF
	and r20, r20, r19
	or sa9, r20, #0x00000000
	p3 ba DPMThresholdUpdate_1
	staw.bpcache [sa0,+#106], r18
	idf drc0, bif
	wdf drc0
	mov r19, #0x20000000
	or r20, r20, r19
	or sa9, r20, #0x00000000
	mov r19, #0x00000100
	ima16.s r18, r18, #1, r19.neg, u16, u16
DPMThresholdUpdate_1:
	str #393, r18
	wdf drc0
	and r17, r17, ~#0x00000003
	stad.bpcache [r16,+#0], r17
	idf drc0, bif
	wdf drc0
	ba DPMReloadFreeList

DPMReloadFreeList:
	ldr r17, #448, drc0
	ldr r18, #452, drc0
	ldr r19, #457, drc0
	wdf drc0
	and r19, r19, #0xFFFF0000
	shr r19, r19, #0x00000010
	ldr r16, #459, drc0
	wdf drc0
	and r16, r16, #0xFFFF0000
	shr r16, r16, #0x00000010
	str #391, r17
	shl r18, r18, #0x00000010
	str #402, r18
	shl r16, r16, #0x00000010
	or r19, r19, r16
	str #398, r19
	str #417, #1
DPMReloadFreeList_1:
	ldr r18, #70, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p2, r18, #1
	p2 ba DPMReloadFreeList_1
	str #69, #1
	lapc

StoreTAPB:
	ldad.bpcache r16, [sa0,+#77], drc0
	ldr r17, #448, drc0
	ldr r18, #452, drc1
	wdf drc0
	stad.bpcache [r16,+#6], r17
	wdf drc1
	shl r18, r18, #0x00000010
	stad.bpcache [r16,+#7], r18
	ldr r17, #457, drc0
	ldr r18, #459, drc1
	wdf drc0
	and r17, r17, #0xFFFF0000
	shr r17, r17, #0x00000010
	stad.bpcache [r16,+#8], r17
	wdf drc1
	and r18, r18, #0xFFFF0000
	shr r18, r18, #0x00000010
	stad.bpcache [r16,+#9], r18
	idf drc0, bif
	wdf drc0
	lapc

LoadTAPB:
	xor.testtanz.chan0 !c0, p3, r17, #1
	stad.bpcache [sa0,+#77], r16
	idf drc1, bif
	ldad.bpcache r17, [r16,+#4], drc0
	wdf drc0
	and r17, r17, ~#0x0000000F
	str #390, r17
	ldad.bpcache r17, [r16,+#6], drc0
	wdf drc0
	str #391, r17
	ldad.bpcache r17, [r16,+#7], drc0
	wdf drc0
	str #402, r17
	ldad.bpcache r17, [r16,+#8], drc0
	ldad.bpcache r18, [r16,+#9], drc0
	wdf drc0
	shl r18, r18, #0x00000010
	or r17, r17, r18
	str #398, r17
	ldad.bpcache r17, [r16,+#10], drc0
	wdf drc0
	str #392, r17
	ldad.bpcache r17, [r16,+#13], drc0
	wdf drc0
	str #389, r17
	p3 ba LoadTAPB_1
	and.testtaz.chan0 !c0, p3, r17, r17
	or r18, sa9, #0x00000000
	mov r19, #0xDFFFFFFF
	and r18, r18, r19
	or sa9, r18, #0x00000000
	p3 ba LoadTAPB_1
	mov r19, #0x20000000
	or r18, r18, r19
	or sa9, r18, #0x00000000
	ldad.bpcache r18, [r16,+#11], drc0
	wdf drc0
	staw.bpcache [sa0,+#106], r18
	idf drc0, bif
	wdf drc0
	mov r19, #0x00000100
	ima16.s r17, r18, #1, r19.neg, u16, u16
	ba LoadTAPB_2
LoadTAPB_1:
	ldad.bpcache r17, [r16,+#11], drc0
	wdf drc0
LoadTAPB_2:
	str #393, r17
	ldad.bpcache r17, [r16,+#12], drc0
	wdf drc0
	and r17, r17, #0x0000FFFF
	or r17, r17, #0x00010000
	str #394, r17
	wdf drc1
	mov r18, #0xFFFFFFFF
	mov r17, #0x00001203
	str r17, r18
	ldad.bpcache.fetch3 r17, [sa0,+#76], drc0
	mov r20, #0x00000000
	wdf drc0
	xor.testtanz.chan0 !c0, p0, r17, r19
	p0 or r20, r20, #0x00000001
	xor.testtanz.chan0 !c0, p0, r17, r18
	p0 or r20, r20, #0x00000002
	xor.testtanz.chan0 !c0, p0, r18, r19
	p0 or r20, r20, #0x00000004
	str #409, r20
	str #417, #1
LoadTAPB_3:
	ldr r17, #70, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p2, r17, #1
	p2 ba LoadTAPB_3
	str #69, #1
	lapc

Store3DPB:
	ldad.bpcache r16, [sa0,+#78], drc0
	ldr r17, #451, drc0
	ldr r18, #455, drc1
	wdf drc0
	stad.bpcache [r16,+#6], r17
	wdf drc1
	shl r18, r18, #0x00000010
	stad.bpcache [r16,+#7], r18
	ldr r17, #458, drc0
	ldr r18, #460, drc1
	wdf drc0
	and r17, r17, #0x0000FFFF
	shr r17, r17, #0x00000000
	stad.bpcache [r16,+#8], r17
	wdf drc1
	and r18, r18, #0x0000FFFF
	shr r18, r18, #0x00000000
	stad.bpcache [r16,+#9], r18
	idf drc0, bif
	wdf drc0
	lapc

Load3DPB:
	stad.bpcache [sa0,+#78], r16
	idf drc1, bif
	ldad.bpcache r17, [r16,+#4], drc0
	wdf drc0
	and r17, r17, ~#0x0000000F
	str #384, r17
	ldad.bpcache r17, [r16,+#6], drc0
	wdf drc0
	str #385, r17
	ldad.bpcache r17, [r16,+#7], drc0
	wdf drc0
	str #403, r17
	ldad.bpcache r18, [r16,+#9], drc0
	ldad.bpcache r17, [r16,+#8], drc1
	wdf drc0
	shl r18, r18, #0x00000010
	wdf drc1
	or r17, r17, r18
	str #408, r17
	ldad.bpcache.fetch3 r17, [sa0,+#76], drc0
	mov r20, #0x00000000
	wdf drc0
	xor.testtanz.chan0 !c0, p0, r17, r19
	p0 or r20, r20, #0x00000001
	xor.testtanz.chan0 !c0, p0, r17, r18
	p0 or r20, r20, #0x00000002
	xor.testtanz.chan0 !c0, p0, r18, r19
	p0 or r20, r20, #0x00000004
	str #409, r20
	str #416, #1
Load3DPB_1:
	ldr r17, #70, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p2, r17, #2
	p2 ba Load3DPB_1
	str #69, #2
	lapc

SetupDPMPagetable:
	ldad.bpcache r17, [r16,+#0], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p2, r17, #1
	p2 ba SetupDPMPagetable_3
	stad.bpcache [sa0,+#58], r16
	idf drc0, bif
SetupDPMPagetable_1:
	ldr r19, #705, drc1
	wdf drc1
	and.testtaz.chan0 !c0, p2, r19, #1
	p2 ba SetupDPMPagetable_1
	str #8913, #0
	or r17, r16, #0x00000000
	mov r20, #0x00200000
	mov r16, #0xDEADBEEF
	wdf drc0
	mov r18, #0xDEADBEEF
	emitpds.tasks.taske #0 /* incp */, r16, r18, r20, #0x00001043
	or r16, r17, #0x00000000
	setm #1
SetupDPMPagetable_2:
	ldr r17, #8913, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p2, r17, #1
	p2 ba.mon SetupDPMPagetable_2
	str #8913, #0
	ldad.bpcache r17, [r16,+#0], drc0
	wdf drc0
	and r17, r17, ~#0x00000001
	stad.bpcache [r16,+#0], r17
	idf drc0, bif
	wdf drc0
	ba SetupDPMPagetable_3
SetupDPMPagetable_3:
	lapc

InitPBLoopback:
	ldad.bpcache r0, [sa0,+#58], drc0
	wdf drc0
	mov r19, #0x00010001
	ldad.bpcache r1, [r0,+#0], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r1, #1
	p0 ba InitPBLoopback_1
	and r1, r1, ~#0x00000003
	stad.bpcache [r0,+#0], r1
	ldad.bpcache.fetch2 r1, [r0,+#1], drc0
	wdf drc0
	stad.bpcache.fetch2 [r0,+#5], r1
	stad.bpcache.fetch2 [r0,+#7], #0
	ldad.bpcache r1, [r0,+#4], drc1
	ldad.bpcache r21, [r0,+#14], drc0
	wdf drc0
	ldaw.bpcache r4, [r21,+#2], drc1
	ldad.bpcache r0, [r21,+#0], drc1
	wdf drc1
	ba InitPBLoopback_1
InitPBLoopback_1:
	mov r2, #0xFFFFFFF0
	imae.u r1, r4.low, #4.low, r1, u32
	mov r3, #0xFFFF0000
	imae.u r5, r4.low, #1.low, #1, u32
	or r3, r3, r5
	stad.bpcache [r1,#1++], r3
	isub16.testtaz.chan0 r0, p0, r0, #1
	p0 br InitPBLoopback_7
InitPBLoopback_2:
	and.testtaz.chan0 !c0, p0, r0, r2
	p0 ima16.s r5, r4, #1, #1.neg, u16, u16
	p0 shl r5, r5, #0x00000010
	p0 imae.u r4, r4.low, #1.low, #1, u32
	p0 or r3, r5, r4
	p0 br InitPBLoopback_4
	ima16.s r5, r4, #1, #16.neg, u16, u16
	shl r5, r5, #0x00000010
	ima16.s r4, r4, #1, #14.neg, u16, u16
	and r4, r4, #0x0000FFFF
	or r3, r4, r5
	and r0, r0, #0x0000FFFF
	shr r2, r0, #0x00000004
	smlsi #1, #0, #1, #0, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	mov r20, #0x00010001
	ima16.repeat8.u r4, r19, r3, r20, u16, u16
	ima16.repeat7.u r12, r19, r11, r20, u16, u16
	mov r20, #0x00100010
InitPBLoopback_3:
	isub16.testtaz.chan0 r2, p0, r2, #1
	smlsi #1, #0, #1, #0, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	ima16.repeat8.u r3, r19, r3, r20, u16, u16
	ima16.repeat8.u r11, r19, r11, r20, u16, u16
	smlsi #1, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	stad.bpcache.repeat16 [r1,#1++], r3
	!p0 br InitPBLoopback_3
	and.testtaz.chan0 !c0, p0, r0, #15
	or r3, r18, #0x00000000
	p0 br InitPBLoopback_6
InitPBLoopback_4:
	and r0, r0, #0x0000000F
	mov r20, #0x00010001
InitPBLoopback_5:
	isub16.testtaz.chan0 r0, p0, r0, #1
	ima16.u r3, r19, r3, r20, u16, u16
	stad.bpcache [r1,#1++], r3
	!p0 br InitPBLoopback_5
InitPBLoopback_6:
	ldad.bpcache r21, [r21,+#3], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r21, r21
	p0 br InitPBLoopback_7
	ldaw.bpcache r4, [r21,+#2], drc0
	wdf drc0
	and r3, r3, ~#0x0000FFFF
	or r3, r3, r4
	stad.bpcache [r1,-#1], r3
	ldad.bpcache r0, [sa0,+#58], drc0
	wdf drc0
	ldad.bpcache r1, [r0,+#4], drc0
	wdf drc0
	imae.u r1, r4.low, #4.low, r1, u32
	ldad.bpcache r0, [r21,+#0], drc0
	wdf drc0
	shr r3, r3, #0x00000010
	imae.u r3, r3.low, #1.low, #1, u32
	imae.u r5, r4.low, #1.low, #1, u32
	shl r3, r3, #0x00000010
	or r3, r3, r5
	stad.bpcache [r1,#1++], r3
	ima16.s r0, r0, #1, #1.neg, u16, u16
	mov r5, #0x0000FFFF
	and.testtaz.chan0 r0, p0, r0, r5
	!p0 mov r2, #0xFFFFFFF0
	!p0 br InitPBLoopback_2
InitPBLoopback_7:
	or r3, r3, #0x0000FFFF
	stad.bpcache [r1,-#1], r3
	idf drc0, bif
	wdf drc0
	str #8913, #1
	lapc

WaitForReset:
	lock #0
	nop
	lock #0

DPMStateClear:
	str #421, #2
DPMStateClear_1:
	ldr r16, #75, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p2, r16, #64
	p2 ba DPMStateClear_1
	str #77, #64
	lapc

DPMStateStore:
	str #421, #1
DPMStateStore_1:
	ldr r16, #75, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p2, r16, #16
	p2 ba DPMStateStore_1
	str #77, #16
	lapc

DPMStateLoad:
	str #421, #4
DPMStateLoad_1:
	ldr r16, #75, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p2, r16, #32
	p2 ba DPMStateLoad_1
	str #77, #32
	lapc

EmitLoopBack:
	and.testtaz.chan0 !c0, p2, r16, r16
	p2 ba EmitLoopBack_2
	ldad.bpcache r17, [sa0,+#56], drc0
	wdf drc0
	or r19, r17, r16
	xor.testtaz.chan0 !c0, p2, r19, r17
	p2 ba EmitLoopBack_2
	stad.bpcache [sa0,+#56], r19
	idf drc0, bif
EmitLoopBack_1:
	ldr r19, #705, drc1
	wdf drc1
	and.testtaz.chan0 !c0, p2, r19, #1
	p2 ba EmitLoopBack_1
	mov r20, #0xDEADBEEF
	wdf drc0
	nop
	mov r18, #0xDEADBEEF
	emitpds.tasks.taske #0 /* incp */, r20, r18, r16, #0x00000843
EmitLoopBack_2:
	lapc

DPMPauseAllocations:
	ldr r16, #669, drc0
	wdf drc0
	or r16, r16, #0x00000001
	str #669, r16
DPMPauseAllocations_1:
	ldr r16, #448, drc0
	wdf drc0
	mov r17, #0x00000190
DPMPauseAllocations_2:
	isubu16.testtanz.chan0 r17, p2, r17, #1
	p2 ba DPMPauseAllocations_2
	ldr r17, #448, drc0
	wdf drc0
	xor.testtanz.chan0 !c0, p2, r17, r16
	p2 ba DPMPauseAllocations_1
	lapc

DPMResumeAllocations:
	ldr r16, #669, drc0
	wdf drc0
	and r16, r16, ~#0x00000001
	str #669, r16
	lapc

StoreDPMContext:
	ldr r19, #797, drc0
	wdf drc0
	and r19, r19, ~#0x00000200
	or r19, r18, r19
	str #797, r19
	ldad.bpcache r18, [r16,+#32], drc1
	wdf drc1
	ldr r19, #396, drc0
	wdf drc0
	and r19, r19, ~#0x00000009
	and.testtaz.chan0 !c0, p2, r18, #32
	p2 or r19, r19, #0x00000008
	or r19, r17, r19
	str #396, r19
	ldad.bpcache r19, [r16,+#16], drc0
	wdf drc0
	str #395, r19
	str #421, #1
StoreDPMContext_1:
	ldr r17, #75, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p2, r17, #16
	p2 ba StoreDPMContext_1
	str #77, #16
	and.testtanz.chan0 !c0, p2, r18, #32
	p2 ba StoreDPMContext_10
	mov r19, #0x00000048
	imae.u r17, r19.low, #1.low, r16, u32
	mov r18, #0x0000218D
	or r19, sa38, #0x00000000
StoreDPMContext_2:
	ldad.bpcache r20, [r17,#1++], drc0
	wdf drc0
	str r18, r20
	isub16.testtaz.chan0 r19, p0, r19, #1
	p0 ba StoreDPMContext_3
	mov r20, #0x00001000
	imae.u r18, r20.low, #1.low, r18, u32
	ba StoreDPMContext_2
StoreDPMContext_3:
	or r19, sa38, #0x00000000
	mov r18, #0x00000004
	ima16.s r18, r18, #1, r19.neg, u16, u16
	imae.u r17, r18.low, #4.low, r17, u32
	str #422, #1
	mov r19, #0x00000058
	imae.u r17, r19.low, #1.low, r16, u32
	mov r18, #0x0000208C
	or r19, sa38, #0x00000000
StoreDPMContext_4:
	ldad.bpcache r20, [r17,#1++], drc0
	wdf drc0
	str r18, r20
	isub16.testtaz.chan0 r19, p0, r19, #1
	p0 ba StoreDPMContext_5
	mov r20, #0x00001000
	imae.u r18, r20.low, #1.low, r18, u32
	ba StoreDPMContext_4
StoreDPMContext_5:
	or r19, sa38, #0x00000000
	mov r18, #0x00000004
	ima16.s r18, r18, #1, r19.neg, u16, u16
	imae.u r17, r18.low, #4.low, r17, u32
	str #147, #4
	mov r19, #0x00000068
	imae.u r17, r19.low, #1.low, r16, u32
	mov r18, #0x00002088
	or r19, sa38, #0x00000000
StoreDPMContext_6:
	ldad.bpcache r20, [r17,#1++], drc0
	wdf drc0
	str r18, r20
	isub16.testtaz.chan0 r19, p0, r19, #1
	p0 ba StoreDPMContext_7
	mov r20, #0x00001000
	imae.u r18, r20.low, #1.low, r18, u32
	ba StoreDPMContext_6
StoreDPMContext_7:
	or r19, sa38, #0x00000000
	mov r18, #0x00000004
	ima16.s r18, r18, #1, r19.neg, u16, u16
	imae.u r17, r18.low, #4.low, r17, u32
	mov r17, #0x40000000
	str #137, r17
	mov r17, #0x00080480
StoreDPMContext_8:
	ldr r18, #75, drc0
	wdf drc0
	and r18, r18, r17
	xor.testtanz.chan0 !c0, p2, r18, r17
	p2 ba StoreDPMContext_8
	mov r17, #0x00080480
	str #77, r17
	ldad.bpcache r17, [r16,+#17], drc0
	wdf drc0
	mov r18, #0x00001201
	str r18, r17
	mov r17, #0x000010DA
	str r17, #4
StoreDPMContext_9:
	mov r17, #0x000010DB
	ldr r17, r17, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p2, r17, #4
	p2 ba StoreDPMContext_9
	mov r17, #0x000010DC
	str r17, #4
StoreDPMContext_10:
	lapc

LoadDPMContext:
	ldr r19, #797, drc0
	wdf drc0
	and r19, r19, ~#0x00000200
	or r19, r18, r19
	str #797, r19
	ldad.bpcache r18, [r16,+#32], drc1
	wdf drc1
	ldr r19, #396, drc0
	wdf drc0
	and r19, r19, ~#0x00000009
	and.testtaz.chan0 !c0, p2, r18, #32
	p2 or r19, r19, #0x00000008
	or r19, r17, r19
	str #396, r19
	ldad.bpcache r19, [r16,+#16], drc0
	wdf drc0
	str #395, r19
	str #421, #4
LoadDPMContext_1:
	ldr r17, #75, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p2, r17, #32
	p2 ba LoadDPMContext_1
	str #77, #32
	and.testtanz.chan0 !c0, p2, r18, #32
	p2 ba LoadDPMContext_8
	mov r19, #0x00000048
	imae.u r17, r19.low, #1.low, r16, u32
	mov r18, #0x0000218D
	or r19, sa38, #0x00000000
LoadDPMContext_2:
	ldad.bpcache r20, [r17,#1++], drc0
	wdf drc0
	str r18, r20
	isub16.testtaz.chan0 r19, p0, r19, #1
	p0 ba LoadDPMContext_3
	mov r20, #0x00001000
	imae.u r18, r20.low, #1.low, r18, u32
	ba LoadDPMContext_2
LoadDPMContext_3:
	or r19, sa38, #0x00000000
	mov r18, #0x00000004
	ima16.s r18, r18, #1, r19.neg, u16, u16
	imae.u r17, r18.low, #4.low, r17, u32
	str #422, #4
	mov r19, #0x00000058
	imae.u r17, r19.low, #1.low, r16, u32
	mov r18, #0x0000208D
	or r19, sa38, #0x00000000
LoadDPMContext_4:
	ldad.bpcache r20, [r17,#1++], drc0
	wdf drc0
	str r18, r20
	isub16.testtaz.chan0 r19, p0, r19, #1
	p0 ba LoadDPMContext_5
	mov r20, #0x00001000
	imae.u r18, r20.low, #1.low, r18, u32
	ba LoadDPMContext_4
LoadDPMContext_5:
	or r19, sa38, #0x00000000
	mov r18, #0x00000004
	ima16.s r18, r18, #1, r19.neg, u16, u16
	imae.u r17, r18.low, #4.low, r17, u32
	str #147, #2
	mov r17, #0x00200100
LoadDPMContext_6:
	ldr r18, #75, drc0
	wdf drc0
	and r18, r18, r17
	xor.testtanz.chan0 !c0, p2, r18, r17
	p2 ba LoadDPMContext_6
	mov r17, #0x00200100
	str #77, r17
	ldad.bpcache r17, [r16,+#17], drc0
	wdf drc0
	mov r18, #0x00001201
	str r18, r17
	mov r17, #0x000010DA
	str r17, #2
LoadDPMContext_7:
	mov r17, #0x000010DB
	ldr r17, r17, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p2, r17, #2
	p2 ba LoadDPMContext_7
	mov r17, #0x000010DC
	str r17, #2
	mov r3, #0x00001208
	ldr r3, r3, drc0
	wdf drc0
	str #468, r3
LoadDPMContext_8:
	lapc

SetupDirListBase:
	mov r20, #0x00000321
	ldr r21, r20, drc0
	ldad.bpcache r18, [r16,+#2], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p3, r17, #4
	and.testtanz.chan0 !c0, p1, r17, #3
	mov r19, #0x00000000
	and.testtaz.chan0 !c0, p0, r21, r21
	p0 ba SetupDirListBase_7
	xor.testtaz.chan0 !c0, p0, r18, r21
	!p0 ba SetupDirListBase_1
	nop
	ba SetupDirListBase_7
SetupDirListBase_1:
	mov r19, #0x00000001
SetupDirListBase_2:
	xor.testtaz.chan0 !c0, p0, r19, #7
	p0 ba SetupDirListBase_3
	mov r20, #0x0000030D
	imae.u r20, r19.low, #1.low, r20, u32
	ldr r21, r20, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r21, r21
	p0 ba SetupDirListBase_7
	xor.testtaz.chan0 !c0, p0, r18, r21
	p0 ba SetupDirListBase_7
	imae.u r19, r19.low, #1.low, #1, u32
	ba SetupDirListBase_2
SetupDirListBase_3:
	ldad.bpcache r19, [sa0,+#80], drc0
	ldr r20, #798, drc1
	wdf drc1
	p1 and r21, r20, #0x000000F0
	!p1 and r21, r20, #0x0000F000
	p1 shr r21, r21, #0x00000004
	!p1 shr r21, r21, #0x0000000C
	p3 and r20, r20, #0x000000F0
	p3 shr r20, r20, #0x00000004
	p3 ba SetupDirListBase_4
	and r20, r20, #0x000F0000
	shr r20, r20, #0x00000010
SetupDirListBase_4:
	wdf drc0
SetupDirListBase_5:
	imae.u r19, r19.low, #1.low, #1, u32
	xor.testtaz.chan0 !c0, p0, r19, #7
	p0 mov r19, #0x00000000
	xor.testtanz.chan0 !c0, p0, r21, r19
	p0 xor.testtanz.chan0 !c0, p0, r20, r19
	!p0 ba SetupDirListBase_5
	and.testtaz.chan0 !c0, p0, r19, r19
	p0 mov r20, #0x00000321
	p0 ba SetupDirListBase_6
	mov r20, #0x0000030D
	imae.u r20, r19.low, #1.low, r20, u32
SetupDirListBase_6:
	stad.bpcache [sa0,+#80], r19
	idf drc1, bif
	mov r17, #0x00000001
	and.testtanz.chan0 !c0, p0, r17, r17
	p0 str #781, #8
	!p0 str #781, #4
	ldad.bpcache r18, [r16,+#2], drc0
	wdf drc0
SetupDirListBase_7:
	ldad.bpcache r17, [r16,+#0], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r17, #1
	!p0 ba SetupDirListBase_8
	and r17, r17, ~#0x00000001
	stad.bpcache [r16,+#0], r17
	idf drc1, bif
	wdf drc1
	ba SetupDirListBase_6
	nop
SetupDirListBase_8:
	p3 ba SetupDirListBase_14
	nop
	p1 ba SetupDirListBase_9
	str #692, #1
	str #696, #1
SetupDirListBase_9:
	lock #7
	str #516, #1
	str #514, #1
	str #642, #1
	or r22, r22, #0x00000040
	p1 ba SetupDirListBase_11
SetupDirListBase_10:
	ldr r21, #78, drc0
	wdf drc0
	and r21, r21, #0x00000003
	xor.testtanz.chan0 !c0, p0, r21, #3
	p0 ba SetupDirListBase_10
	str #80, #3
SetupDirListBase_11:
	ldr r21, #70, drc0
	wdf drc0
	shl.testpat.chan0 !c0, p0, r21, #24
	p0 ba SetupDirListBase_11
	mov r21, #0x00000080
	str #69, r21
SetupDirListBase_12:
	ldr r21, #75, drc0
	wdf drc0
	shl.testpat.chan0 !c0, p0, r21, #5
	p0 ba SetupDirListBase_12
	mov r21, #0x04000000
	str #77, r21
	ldr r21, #8927, drc0
	wdf drc0
	release #7
	!p1 mov r16, #0x00000022
	p1 mov r16, #0x00000044
	nop
	nop
	nop.nosched
	str #4939, r16
	str #4938, r16
SetupDirListBase_13:
	ldr r17, #4932, drc0
	wdf drc0
	and r17, r17, #0x00000003
	xor.testtaz.chan0 !c0, p0, r17, #3
	!p0 ba SetupDirListBase_13
	str #4933, #3
SetupDirListBase_14:
	ldr r21, r20, drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r21, r18
	p0 ba SetupDirListBase_15
	mov r24, #0x00000001
	or r25, r20, #0x00000000
	or r26, r18, #0x00000000
	mov r28, pclink
	ba.savelink JumpToSupervisor
	mov pclink, r28
	nop
SetupDirListBase_15:
	lapc

SetupRequestor:
	ldr r20, #798, drc1
	wdf drc0
	p3 shl r21, r19, #0x00000010
	wdf drc1
	p3 and r20, r20, ~#0x000F0000
	p3 ba SetupRequestor_1
	!p1 shl r21, r19, #0x00000004
	p1 shl r21, r19, #0x0000000C
	!p1 and r20, r20, ~#0x000000F0
	p1 and r20, r20, ~#0x0000F000
SetupRequestor_1:
	or r20, r20, r21
	mov r24, #0x00000001
	mov r25, #0x0000031E
	or r26, r20, #0x00000000
	mov r28, pclink
	ba.savelink JumpToSupervisor
	mov pclink, r28
	ldr r20, #798, drc0
	wdf drc0
	lapc

ClearActivePowerFlags:
	or r17, sa13, #0x00000000
	and.testtaz.chan0 !c0, p0, r17, r16
	p0 ba ClearActivePowerFlags_1
	xor r16, r16, ~#0x00000000
	and r17, r17, r16
	or sa13, r17, #0x00000000
	and.testtaz.chan0 !c0, p0, r17, r17
	!p0 ba ClearActivePowerFlags_1
	ldad.bpcache r17, [sa1,+#7], drc0
	wdf drc0
	or sa12, r17, #0x00000000
ClearActivePowerFlags_1:
	lapc

SwitchEDMBIFBank:
	ldr r17, #798, drc0
	xor.testtaz.chan0 !c0, p2, r16, #2
	p2 mov r18, #0x00000007
	wdf drc0
	p2 ba SwitchEDMBIFBank_1
	xor.testtaz.chan0 !c0, p2, r16, #0
	p2 and r18, r17, #0x000000F0
	p2 shr r18, r18, #0x00000004
	p2 ba SwitchEDMBIFBank_1
	and r18, r17, #0x0000F000
	shr r18, r18, #0x0000000C
SwitchEDMBIFBank_1:
	and r17, r17, ~#0x0000000F
	or r17, r17, r18
	mov r24, #0x00000001
	mov r25, #0x0000031E
	or r26, r17, #0x00000000
	mov r28, pclink
	ba.savelink JumpToSupervisor
	mov pclink, r28
	ldr r17, #798, drc0
	wdf drc0
	xor.testtanz.chan0 !c0, p2, r16, #2
	p2 ba SwitchEDMBIFBank_2
	stad.bpcache [sa0,+#0], sa0
SwitchEDMBIFBank_2:
	lapc

InvalidateSLC:
	or r16, r16, r17
	nop
	nop.nosched
	str #4939, r16
	str #4938, r16
InvalidateSLC_1:
	ldr r20, #4932, drc0
	wdf drc0
	and r20, r20, #0x00000003
	xor.testtaz.chan0 !c0, p0, r20, #3
	!p0 ba InvalidateSLC_1
	str #4933, #3
	lapc

FreeContextMemory:
	ldad.bpcache r2, [sa0,+#50], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	!p0 xor.testtaz.chan0 !c0, p0, r2, r14
	p0 ba FreeContextMemory_1
	stad.bpcache [sa0,+#50], #0
	stad.bpcache [sa0,+#52], #0
	stad.bpcache [sa0,+#51], #0
	ldr r3, #396, drc0
	wdf drc0
	and r3, r3, #0x00000004
	shr r4, r3, #0x00000002
	and.testtaz.chan0 !c0, p0, r4, r4
	p0 stad.bpcache [sa0,+#43], #0
	!p0 stad.bpcache [sa0,+#44], #0
	idf drc0, bif
	ldad.bpcache r3, [r2,+#0], drc0
	wdf drc0
	or r16, r3, #0x00000000
	mov r17, #0x00000000
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	or r16, r2, #0x00000000
	or r17, r4, #0x00000000
	mov r18, #0x00000000
	mov r23, pclink
	ba.savelink StoreDPMContext
	mov pclink, r23
FreeContextMemory_1:
	ldad.bpcache r2, [sa0,+#43], drc0
	ldad.bpcache r3, [sa0,+#44], drc1
	mov r4, #0x00000000
	wdf drc0
	wdf drc1
	xor.testtaz.chan0 !c0, p0, r14, r2
	p0 ba FreeContextMemory_2
	xor.testtaz.chan0 !c0, p0, r14, r3
	p0 mov r4, #0x00000004
	p0 ba FreeContextMemory_2
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba FreeContextMemory_2
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 mov r4, #0x00000004
	p0 ba FreeContextMemory_2
	ldr r4, #396, drc0
	wdf drc0
	and r4, r4, #0x00000002
	shr r4, r4, #0x00000001
	xor r4, r4, #0x00000001
	shl r4, r4, #0x00000002
FreeContextMemory_2:
	ldad.bpcache r5, [sa0,+#50], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r5, r14
	p0 stad.bpcache [sa0,+#50], #0
	and.testtaz.chan0 !c0, p0, r4, r4
	p0 stad.bpcache [sa0,+#43], #0
	!p0 or r2, r3, #0x00000000
	!p0 stad.bpcache [sa0,+#44], #0
	ldr r5, #396, drc0
	wdf drc0
	and r5, r5, ~#0x00000004
	or r5, r5, #0x00000008
	or r5, r5, r4
	str #396, r5
	shr r4, r4, #0x00000002
	and.testtaz.chan0 !c0, p0, r2, r2
	!p0 xor.testtaz.chan0 !c0, p0, r2, r14
	p0 ba FreeContextMemory_3
	ldad.bpcache r3, [r2,+#0], drc0
	wdf drc0
	or r16, r3, #0x00000000
	mov r17, #0x00000000
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	or r16, r2, #0x00000000
	or r17, r4, #0x00000000
	mov r18, #0x00000000
	mov r23, pclink
	ba.savelink StoreDPMContext
	mov pclink, r23
FreeContextMemory_3:
	or r16, r0, #0x00000000
	mov r17, #0x00000000
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	xor.testtaz.chan0 !c0, p0, r2, r14
	p0 ba FreeContextMemory_4
	ldr r5, #396, drc0
	wdf drc0
	and r5, r5, ~#0x00000001
	or r5, r5, r4
	str #396, r5
	ldr r5, #797, drc0
	wdf drc0
	and r5, r5, ~#0x00000200
	str #797, r5
	ldad.bpcache r3, [r14,+#16], drc0
	wdf drc0
	str #395, r3
	mov r23, pclink
	ba.savelink DPMStateLoad
	mov pclink, r23
FreeContextMemory_4:
	ldad.bpcache r2, [sa0,+#77], drc0
	ldad.bpcache r3, [r0,+#6], drc0
	ldad.bpcache r4, [sa0,+#78], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r2, r4
	p0 xor.testtaz.chan0 !c0, p0, r2, r3
	p0 ba FreeContextMemory_9
	xor.testtaz.chan0 !c0, p0, r4, #0
	p0 ba FreeContextMemory_5
	mov r16, #0x00000001
	mov r23, pclink
	ba.savelink IdleCore
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink Store3DPB
	mov pclink, r23
FreeContextMemory_5:
	xor.testtaz.chan0 !c0, p0, r2, r3
	p0 ba FreeContextMemory_7
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba FreeContextMemory_6
	xor.testtaz.chan0 !c0, p0, r2, r4
	p0 ba FreeContextMemory_6
	mov r23, pclink
	ba.savelink StoreTAPB
	mov pclink, r23
FreeContextMemory_6:
	mov r5, #0x00000000
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	or r16, r3, #0x00000000
	mov r23, pclink
	ba.savelink SetupDPMPagetable
	mov pclink, r23
	or r16, r3, #0x00000000
	or r17, r5, #0x00000000
	mov r23, pclink
	ba.savelink LoadTAPB
	mov pclink, r23
	mov r16, #0x00000002
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	ba FreeContextMemory_8
FreeContextMemory_7:
	mov r23, pclink
	ba.savelink StoreTAPB
	mov pclink, r23
FreeContextMemory_8:
	xor.testtaz.chan0 !c0, p0, r3, r4
	p0 ba FreeContextMemory_9
	or r16, r3, #0x00000000
	mov r23, pclink
	ba.savelink Load3DPB
	mov pclink, r23
FreeContextMemory_9:
	ldr r5, #798, drc0
	wdf drc0
	or r16, r5, #0x00000000
	and r17, r5, #0x000000F0
	and r5, r5, ~#0x0000F000
	shl r17, r17, #0x00000008
	or r5, r5, r17
	mov r24, #0x00000001
	mov r25, #0x0000031E
	or r26, r5, #0x00000000
	mov r23, pclink
	ba.savelink JumpToSupervisor
	mov pclink, r23
	str #424, #1
	mov r3, #0x80000000
	str #137, r3
	mov r3, #0x01000800
FreeContextMemory_10:
	ldr r2, #75, drc0
	wdf drc0
	and r2, r2, r3
	xor.testtanz.chan0 !c0, p0, r2, r3
	p0 ba FreeContextMemory_10
	mov r3, #0x01000800
	str #77, r3
	mov r24, #0x00000001
	mov r25, #0x0000031E
	or r26, r16, #0x00000000
	mov r23, pclink
	ba.savelink JumpToSupervisor
	mov pclink, r23
	xor.testtaz.chan0 !c0, p0, r4, #0
	p0 ba FreeContextMemory_12
	ldad.bpcache r3, [r0,+#6], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r3, r4
	p0 ba FreeContextMemory_11
	or r16, r4, #0x00000000
	mov r23, pclink
	ba.savelink Load3DPB
	mov pclink, r23
FreeContextMemory_11:
	or r4, r16, #0x00000000
	mov r23, pclink
	ba.savelink Resume
	mov pclink, r23
	or r16, r4, #0x00000000
FreeContextMemory_12:
	lapc

MoveSceneToCompleteList:
	ldad.bpcache r18, [r17,+#85], drc0
	ldad.bpcache r19, [r17,+#86], drc1
	wdf drc0
	and.testtanz.chan0 !c0, p2, r18, r18
	wdf drc1
	p2 ba MoveSceneToCompleteList_1
	stad.bpcache [r16,+#18], r19
	ba MoveSceneToCompleteList_2
MoveSceneToCompleteList_1:
	stad.bpcache [r18,+#86], r19
MoveSceneToCompleteList_2:
	and.testtanz.chan0 !c0, p2, r19, r19
	p2 ba MoveSceneToCompleteList_3
	stad.bpcache [r16,+#19], r18
	ba MoveSceneToCompleteList_4
MoveSceneToCompleteList_3:
	stad.bpcache [r19,+#85], r18
MoveSceneToCompleteList_4:
	ldad.bpcache r18, [r16,+#20], drc0
	ldad.bpcache r19, [r16,+#21], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p2, r18, r18
	p2 ba MoveSceneToCompleteList_5
	stad.bpcache [r16,+#20], r17
	ba MoveSceneToCompleteList_6
MoveSceneToCompleteList_5:
	stad.bpcache [r19,+#86], r17
MoveSceneToCompleteList_6:
	stad.bpcache [r16,+#21], r17
	stad.bpcache [r17,+#85], r19
	stad.bpcache [r17,+#86], #0
	idf drc0, bif
	wdf drc0
	p2 ba MoveSceneToCompleteList_9
	ldad.bpcache r17, [r16,+#1], drc0
	wdf drc0
	imae.u r18, r17.low, #1.low, #26, u32
	or i.l, r18, #0x00000000
	or r20, sa[il + #0], #0x00000000
	imae.u r19, r17.low, #1.low, #29, u32
	or i.l, r19, #0x00000000
	or r21, sa[il + #0], #0x00000000
	and.testtanz.chan0 !c0, p2, r20, r20
	p2 ba MoveSceneToCompleteList_7
	imae.u r18, r17.low, #1.low, #26, u32
	or i.l, r18, #0x00000000
	or sa[il + #0], r16, #0x00000000
	ba MoveSceneToCompleteList_8
MoveSceneToCompleteList_7:
	stad.bpcache [r21,+#25], r16
MoveSceneToCompleteList_8:
	imae.u r19, r17.low, #1.low, #29, u32
	or i.l, r19, #0x00000000
	or sa[il + #0], r16, #0x00000000
	stad.bpcache [r16,+#24], r21
	idf drc0, bif
	wdf drc0
MoveSceneToCompleteList_9:
	lapc

CommonProgramEnd:
	and.testtaz.chan0 !c0, p0, r16, r16
	p0 ba CommonProgramEnd_2
	str #4939, #16
CommonProgramEnd_1:
	ldr r16, #4932, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r16, #2
	p0 ba CommonProgramEnd_1
	str #4933, #2
CommonProgramEnd_2:
	idf drc0, bif
	wdf drc0
	phas #0, #0, pixel, end, parallel
	nop.end

JumpToSupervisor:
	or sa40, r24, #0x00000000
	or sa41, r25, #0x00000000
	or sa42, r26, #0x00000000
	mov r29, pclink
	shl r29, r29, #0x00000004
	p0 or r29, r29, #0x00000001
	p1 or r29, r29, #0x00000002
	p2 or r29, r29, #0x00000004
	p3 or r29, r29, #0x00000008
	sprvv.end
	smlsi #0, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	and.testtanz.chan0 !c0, p0, r29, #1
	and.testtanz.chan0 !c0, p1, r29, #2
	and.testtanz.chan0 !c0, p2, r29, #4
	and.testtanz.chan0 !c0, p3, r29, #8
	shr r29, r29, #0x00000004
	mov pclink, r29
	nop
	lapc

/* Sony's */
USEClkGateOn:
	ldr r17, #1, drc0
	wdf drc0
	and r17, r17, ~#0x0000C0C0
	or r17, r17, #0x00004040
	str #1, r17
	lapc

/* Sony's */
USEClkGateAuto:
	ldr r17, #1, drc0
	wdf drc0
	and r17, r17, ~#0x0000C0C0
	or r17, r17, #0x00008080
	str #1, r17
	lapc
