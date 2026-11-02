TAEventHandler:
	smlsi #0, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	mov r15, #0x00000000
	mov r22, #0x00000000
	and r0, pa0, #0x00000010
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 br TAEventHandler_1
	ldr r10, #457, drc0
	wdf drc0
	and r10, r10, #0xFFFF0000
	shr r10, r10, #0x00000010
	mov r0, #0x00002000
	str #77, r0
	mov r15, #0x000006BE
	ba TAFinished
TAEventHandler_1:
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink CommonProgramEnd
	mov pclink, r23

TAHandler:
	smlsi #0, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	mov r15, #0x00000000
	mov r22, #0x00000000
	ldad.bpcache r0, [sa0,+#56], drc0
	xor r1, pa0, ~#0x00000000
	wdf drc0
	and r0, r0, r1
	stad.bpcache [sa0,+#56], r0
	and r0, pa0, #0x00000100
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba TAHandler_1
	mov r0, #0x00002000
	str #77, r0
	mov r15, #0x000006D1
	ba TAFinished
TAHandler_1:
	and r0, pa0, #0x00040000
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba TAHandler_2
	ba.savelink FindTA
TAHandler_2:
	and r0, pa0, #0x00200000
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba TAHandler_3
	mov r23, pclink
	ba.savelink InitPBLoopback
	mov pclink, r23
TAHandler_3:
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink CommonProgramEnd
	mov pclink, r23

TAFinished:
	mov sa12, #0x00000000
	mov sa13, #0x00000007
	or r0, sa9, #0x00000000
	shl.testpat.chan0 !c0, p1, r0, #22
	p1 shl.testpat.chan0 !c0, p1, r0, #21
	p1 ba TAFinished_7
	shl.testnat.chan0 !c0, p1, r0, #21
	p1 ba TAFinished_1
	or r0, r0, #0x00000422
	or sa9, r0, #0x00000000
	stad.bpcache [sa0,+#87], #0
	ldad.bpcache r1, [sa0,+#86], drc0
	wdf drc0
	or r1, r1, #0x00000001
	stad.bpcache [sa0,+#86], r1
	mov r16, #0x00000080
	mov r23, pclink
	ba.savelink EmitLoopBack
	mov pclink, r23
	ba TAFinished_31
TAFinished_1:
	ldad.bpcache r1, [sa0,+#49], drc0
	wdf drc0
	ldad r1, [r1,+#1], drc0
	ldad.bpcache r14, [sa0,+#50], drc0
	wdf drc0
	ldad.bpcache r2, [r14,+#32], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r1, #2
	p1 or r2, r2, #0x00000200
	p1 ba TAFinished_2
	or r2, r2, #0x00000040
TAFinished_2:
	stad.bpcache [r14,+#32], r2
	idf drc0, bif
	wdf drc0
	str #224, #1
	str #421, #2
	str #422, #2
	str #147, #1
	mov r2, #0x40000000
	str #137, r2
	mov r3, #0x00100640
TAFinished_3:
	ldr r2, #75, drc0
	wdf drc0
	and r2, r2, r3
	xor.testtanz.chan0 !c0, p0, r2, r3
	p0 ba TAFinished_3
	mov r3, #0x00100640
	str #77, r3
TAFinished_4:
	ldr r2, #70, drc0
	wdf drc0
	shl.testpat.chan0 !c0, p0, r2, #26
	p0 ba TAFinished_4
	mov r3, #0x00000020
	str #69, r3
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	ldad.bpcache r2, [r14,+#30], drc1
	or r3, sa38, #0x00000000
	mov r4, #0x00000000
TAFinished_5:
	imae.u r5, r4.low, #1.low, #2, u32
	shl r5, r5, #0x0000000C
	or r5, r5, #0x00000088
	ldr r5, r5, drc1
	wdf drc1
	shr r6, r2, #0x00000006
TAFinished_6:
	stad.bpcache.fetch16 [r5,#0++], #0
	isub16.testtanz.chan0 r6, p0, r6, #1
	p0 ba TAFinished_6
	ima32.s1.u r4, r4, #1, #1
	xor.testtanz.chan0 !c0, p0, r4, r3
	p0 ba TAFinished_5
	idf drc1, bif
	wdf drc1
	mov r16, #0x00000002
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	mov r2, #0x0000119C
	ldr r2, r2, drc0
	wdf drc0
	and r2, r2, ~#0x00000002
	mov r1, #0x0000119C
	str r1, r2
	mov r2, #0x00001200
	ldr r2, r2, drc0
	wdf drc0
	and r2, r2, ~#0x00001FFF
	or r2, r2, #0x00001FFF
	mov r1, #0x00001200
	str r1, r2
	shl.testpat.chan0 !c0, p1, r0, #22
	p1 ba TAFinished_7
	ldad.bpcache r1, [sa0,+#63], drc0
	ldr r2, #393, drc0
	wdf drc0
	ima16.s r2, r2, #1, r1.neg, u16, u16
	str #393, r2
	ldad.bpcache r2, [sa0,+#62], drc0
	wdf drc0
	str #394, r2
	mov r23, pclink
	ba.savelink DPMReloadFreeList
	mov pclink, r23
TAFinished_7:
	mov r2, #0x40000000
	and.testtaz.chan0 !c0, p0, r0, r2
	p0 ba TAFinished_9
	ldaw r3, [sa0,+#106], drc0
	wdf drc0
	mov r2, #0x00000100
	ima16.s r3, r3, #1, r2.neg, u16, u16
	str #393, r3
	mov r2, #0xBFFFFFFF
	and r0, r0, r2
	or sa9, r0, #0x00000000
	ldad.bpcache r1, [sa0,+#77], drc0
	ldad.bpcache r2, [sa0,+#78], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r1, r2
	!p0 ba TAFinished_8
	mov r16, #0x00000001
	mov r23, pclink
	ba.savelink IdleCore
	mov pclink, r23
TAFinished_8:
	mov r23, pclink
	ba.savelink DPMReloadFreeList
	mov pclink, r23
	xor.testtaz.chan0 !c0, p0, r1, r2
	!p0 ba TAFinished_9
	or r1, r16, #0x00000000
	mov r23, pclink
	ba.savelink Resume
	mov pclink, r23
	or r16, r1, #0x00000000
TAFinished_9:
	and r0, r0, ~#0x01000000
	and r1, r0, ~#0x00000785
	and r1, r1, ~#0x00000018
	or sa9, r1, #0x00000000
	mov r2, #0x00001200
	ldr r2, r2, drc0
	wdf drc0
	and r2, r2, ~#0x00001FFF
	or r2, r2, #0x00001FFF
	mov r1, #0x00001200
	str r1, r2
	ldad.bpcache r0, [sa0,+#49], drc0
	wdf drc0
	ldad r2, [r0,+#22], drc0
	wdf drc0
	and r2, r2, ~#0x00000001
	stad [r0,+#22], r2
	ldad.fetch2 r2, [r0,+#26], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba TAFinished_10
	mov r4, #0x00000001
	imae.u r2, r4.low, #1.low, r2, u32
	stad.bpcache [r3,+#0], r2
	idf drc0, bif
TAFinished_10:
	ldad.bpcache r6, [sa0,+#51], drc0
	wdf drc0
	ldad r2, [r0,+#11], drc0
	ldr r4, #457, drc1
	wdf drc1
	and r4, r4, #0xFFFF0000
	shr r4, r4, #0x00000010
	wdf drc0
	stad.bpcache [r2,+#5], r4
	idf drc0, bif
	ldad.bpcache r2, [r0,+#1], drc0
	wdf drc0
	mov r3, #0x10000000
	and.testtaz.chan0 !c0, p1, r2, r3
	p1 ba TAFinished_12
	str #4939, #32
TAFinished_11:
	ldr r2, #4932, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r2, #2
	p1 ba TAFinished_11
	str #4933, #2
TAFinished_12:
	ldad r1, [r0,+#23], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r1, r1
	p0 ba TAFinished_14
	mov r2, #0x00000118
	imae.u r2, r2.low, #1.low, r0, u32
	imae.u r3, r1.low, #8.low, r2, u32
	ldad.fetch2 r4, [r2,#0++], drc0
TAFinished_13:
	wdf drc0
	xor.testtanz.chan0 !c0, p0, r2, r3
	stad.bpcache [r4,+#0], r5
	p0 ldad.fetch2 r4, [r2,#0++], drc0
	p0 ba TAFinished_13
TAFinished_14:
	mov r2, #0x00004000
	str #75, r2
	ldad.bpcache r14, [sa0,+#50], drc0
	ldad r1, [r0,+#1], drc1
	wdf drc0
	ldad.bpcache r2, [r14,+#32], drc0
	wdf drc1
	and.testtaz.chan0 !c0, p0, r1, #2
	!p0 mov r7, #0x07C00000
	!p0 and.testtanz.chan0 !c0, p0, r1, r7
	p0 ba TAFinished_15
	ldr r7, #4297, drc1
	wdf drc1
	or r7, r7, #0x40000000
	mov r30, #0x08001008
	mov.nosched r31, #0x08002000
	stad [r30,+#0], r7
	stad [r31,+#0], #0
	idf drc1, bif
	wdf drc1
	str #4296, #1
	str #4296, #0
	str #200, #1
	str #200, #0
TAFinished_15:
	ldad r7, [r0,+#8], drc1
	and.testtaz.chan0 !c0, p0, r1, #5
	wdf drc1
	and.testtaz.chan0 !c0, p1, r7, #8
	p1 and.testtanz.chan0 !c0, p1, r1, #2
	and.testtanz.chan0 !c0, p2, r1, #2
	wdf drc0
	p2 or r2, r2, #0x00000020
	p2 stad.bpcache [r14,+#32], r2
	!p1 ba TAFinished_17
	stad.bpcache [sa0,+#50], #0
	idf drc0, bif
	mov r2, #0x40000000
	str #137, r2
TAFinished_16:
	ldr r2, #75, drc1
	wdf drc1
	shl.testpat.chan0 !c0, p3, r2, #21
	p3 ba TAFinished_16
	mov r2, #0x00000400
	str #77, r2
	wdf drc0
TAFinished_17:
	ldad.bpcache r0, [sa0,+#52], drc0
	wdf drc0
	p0 ba TAFinished_18
	ldad.bpcache r3, [r0,+#18], drc0
	wdf drc0
	ldad.bpcache r4, [r0,+#19], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 stad.bpcache [r0,+#18], r6
	stad.bpcache [r0,+#19], r6
	!p0 stad.bpcache [r4,+#86], r6
	!p0 stad.bpcache [r6,+#85], r4
	idf drc0, bif
	wdf drc0
TAFinished_18:
	p1 ba TAFinished_21
	mov r3, #0x00000144
	imae.u r2, r3.low, #1.low, r6, u32
	mov r3, #0x0000209D
	or r4, sa38, #0x00000000
TAFinished_19:
	ldr r5, r3, drc0
	wdf drc0
	stad.bpcache [r2,#1++], r5
	isub16.testtaz.chan0 r4, p0, r4, #1
	p0 ba TAFinished_20
	mov r5, #0x00001000
	imae.u r3, r5.low, #1.low, r3, u32
	ba TAFinished_19
TAFinished_20:
	or r4, sa38, #0x00000000
	mov r3, #0x00000004
	ima16.s r3, r3, #1, r4.neg, u16, u16
	imae.u r2, r3.low, #4.low, r2, u32
	mov r3, #0x000010D8
	ldr r3, r3, drc0
	wdf drc0
	stad.bpcache [r6,+#80], r3
	idf drc0, bif
	wdf drc0
	p2 ba TAFinished_21
	ldad.bpcache r4, [r6,+#67], drc0
	wdf drc0
	ldad.bpcache r3, [r4,+#1], drc0
	wdf drc0
	ima32.s1.u r3, r3, #1, #1
	stad.bpcache [r4,+#1], r3
	idf drc0, bif
	wdf drc0
	ba TAFinished_27
TAFinished_21:
	and.testtaz.chan0 !c0, p0, r7, #1
	p0 ba TAFinished_22
	mov r7, pclink
	ba.savelink FreeContextMemory
	mov pclink, r7
TAFinished_22:
	ldad.bpcache r1, [sa0,+#49], drc0
	wdf drc0
	ldad r3, [r1,+#1], drc0
	mov r1, #0x00004000
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r1
	p0 ba TAFinished_23
	stad.bpcache [sa1,+#31], #0
	idf drc0, bif
	wdf drc0
TAFinished_23:
	or r16, r0, #0x00000000
	or r17, r6, #0x00000000
	mov r23, pclink
	ba.savelink MoveSceneToCompleteList
	mov pclink, r23
	or r3, sa4, #0x00000000
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba TAFinished_27
	ldad.bpcache r1, [sa0,+#49], drc0
	wdf drc0
	ldad r3, [r1,+#8], drc0
	mov r2, #0x00003001
	wdf drc0
	and.testtanz.chan0 !c0, p0, r3, r2
	p0 ba TAFinished_27
	ldad.bpcache r1, [r14,+#2], drc0
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	ldad.bpcache r5, [r14,+#6], drc0
	mov r2, #0x00000000
	wdf drc0
TAFinished_24:
	imae.u r4, r2.low, #4.low, r5, u32
	ldad r4, [r4,+#0], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, #1
	p0 ba TAFinished_26
	and r4, r4, ~#0x00000003
	mov r3, #0x0000001C
	imae.u r6, r3.low, #1.low, r14, u32
	mov r3, #0x00000000
TAFinished_25:
	ldad r7, [r6,#1++], drc0
	wdf drc0
	ima32.s1.s r8, r4, #1, r7
	ima32.s2.s r7, r4, #1, r8
	ldad.bpcache r8, [r7,+#0], drc0
	wdf drc0
	or r8, r8, #0x80000000
	stad.bpcache [r7,+#0], r8
	or r7, sa38, #0x00000000
	imae.u r3, r3.low, #1.low, #1, u32
	xor.testtanz.chan0 !c0, p0, r3, r7
	p0 ba TAFinished_25
TAFinished_26:
	xor.testtanz.chan0 !c0, p0, r2, r1
	p0 imae.u r2, r2.low, #1.low, #1, u32
	p0 ba TAFinished_24
	idf drc0, bif
	wdf drc0
	mov r16, #0x00000002
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	nop
TAFinished_27:
	mov r23, pclink
	ba.savelink Check2DQueue
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink Check3DQueues
	mov pclink, r23
	ldad.bpcache r1, [sa0,+#49], drc0
	wdf drc0
	ba.savelink AdvanceTACCBROff
	or r1, sa4, #0x00000000
	ldad.bpcache r4, [r0,+#23], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, r4
	imae.u r2, r1.low, #1.low, #20, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r4, #0x00000000
	!p0 ba TAFinished_28
	imae.u r3, r1.low, #1.low, #23, u32
	or i.l, r3, #0x00000000
	mov sa[il + #0], #0x00000000
TAFinished_28:
	!p0 stad.bpcache [r4,+#22], #0
	stad.bpcache [r0,+#23], #0
	idf drc0, bif
	wdf drc0
	ldad.bpcache r2, [r0,+#5], drc0
	ldad.bpcache r4, [r0,+#18], drc0
	wdf drc0
	or.testtaz.chan0 !c0, p1, r2, r4
	wdf drc0
	p1 ba TAFinished_30
	nop
	!p0 ba TAFinished_29
	imae.u r2, r1.low, #1.low, #20, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r0, #0x00000000
TAFinished_29:
	imae.u r3, r1.low, #1.low, #23, u32
	or i.l, r3, #0x00000000
	or r2, sa[il + #0], #0x00000000
	imae.u r3, r1.low, #1.low, #23, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r0, #0x00000000
	!p0 stad.bpcache [r2,+#23], r0
	stad.bpcache [r0,+#22], r2
	idf drc0, bif
	wdf drc0
TAFinished_30:
	mov r23, pclink
	ba.savelink CheckTAQueue
	mov pclink, r23
	or r1, sa10, #0x00000000
	mov r0, #0x00003000
	and r2, r1, r0
	mov r0, #0x00001000
	xor.testtanz.chan0 !c0, p1, r2, r0
	p1 ba TAFinished_31
	mov r0, #0x00002000
	or r1, r1, r0
	or sa10, r1, #0x00000000
TAFinished_31:
	or r4, sa10, #0x00000000
	and.testtanz.chan0 !c0, p0, r4, #1
	p0 ba TAFinished_32
	mov r23, pclink
	ba.savelink USEClkGateAuto
	mov pclink, r23
TAFinished_32:
	mov pclink, r15
	nop
	lapc

FindTA:
	or r1, sa9, #0x00000000
	and.testtanz.chan0 !c0, p0, r1, #4
	and.testtanz.chan0 !c0, p1, r1, #1
	shl.testnat.chan0 !c0, p2, r1, #10
	p2 ba FindTA_22
	p0 ba FindTA_22
	or r1, sa11, #0x00000000
	and.testtaz.chan0 !c0, p2, r1, #3
	p2 ba FindTA_1
	ba FindTA_22
FindTA_1:
	mov r8, #0x00000000
	or r0, sa20, #0x00000000
	!p1 ba FindTA_3
FindTA_2:
	or r1, sa4, #0x00000000
	isub16.testpat.chan0 !c0, p0, r8, r1
	p0 ba FindTA_22
FindTA_3:
	and.testtaz.chan0 !c0, p0, r0, r0
	!p0 ba FindTA_5
	imae.u r8, r8.low, #1.low, #1, u32
	or r0, sa5, #0x00000000
	isub16.testnat.chan0 !c0, p0, r0, r8
	p0 ba FindTA_4
	imae.u r1, r8.low, #1.low, #20, u32
	or i.l, r1, #0x00000000
	or r0, sa[il + #0], #0x00000000
	p1 ba FindTA_2
	nop
	ba FindTA_3
	nop
FindTA_4:
	p1 ba FindTA_22
	mov r16, #0x00000001
	mov r23, pclink
	ba.savelink ClearActivePowerFlags
	mov pclink, r23
	ba FindTA_22
FindTA_5:
	ldad.bpcache r1, [r0,+#5], drc0
	ldad.bpcache r2, [r0,+#0], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r1, r1
	p0 ba FindTA_13
	and.testtanz.chan0 !c0, p0, r2, #16
	!p0 ba FindTA_6
	nop
	ba FindTA_13
FindTA_6:
	ldad.bpcache r1, [r0,+#4], drc0
	wdf drc0
	ldad.bpcache r1, [r1,+#1], drc0
	ldad.bpcache r2, [r0,+#3], drc1
	wdf drc0
	wdf drc1
	imae.u r1, r1.low, #1.low, r2, u32
	ldad.bpcache r9, [r1,+#22], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r9, #1
	p0 br FindTA_13
	ldad r14, [r1,+#19], drc0
	wdf drc0
	ldad r9, [r1,+#1], drc0
	wdf drc0
	ldad.fetch4 r2, [r1,+#24], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba FindTA_7
	ldad.bpcache r6, [r3,+#0], drc0
	ldad.bpcache r7, [r5,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r6, r2
	p0 xor.testtaz.chan0 !c0, p0, r7, r4
	!p0 ba FindTA_13
FindTA_7:
	mov r2, #0x00000400
	wdf drc0
	and.testtaz.chan0 !c0, p0, r9, r2
	p0 ba FindTA_8
	ldad.fetch2 r4, [r1,+#67], drc0
	wdf drc0
	ldad.bpcache r6, [r5,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r6, r4
	!p0 ba FindTA_13
FindTA_8:
	mov r10, #0x00000000
	ldad.bpcache r2, [r0,+#0], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r2, #64
	p0 ba FindTA_12
	mov r4, #0x00000000
FindTA_9:
	imae.u r3, r4.low, #1.low, #20, u32
	or i.l, r3, #0x00000000
	or r2, sa[il + #0], #0x00000000
FindTA_10:
	or.testtaz.chan0 !c0, p0, r2, r2
	p0 ba FindTA_11
	xor.testtanz.chan0 !c0, p0, r2, r0
	p0 mov r10, #0x00000001
	p0 ba FindTA_12
	ldad.bpcache r2, [r2,+#23], drc0
	wdf drc0
	ba FindTA_10
FindTA_11:
	imae.u r4, r4.low, #1.low, #1, u32
	xor.testtaz.chan0 !c0, p0, r4, #3
	!p0 ba FindTA_9
FindTA_12:
	mov r2, #0x00000005
	and.testtaz.chan0 !c0, p0, r9, r2
	p0 ba FindTA_16
	ldad.bpcache r2, [r14,+#32], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r2, #16
	p0 ba FindTA_14
FindTA_13:
	ldad.bpcache r0, [r0,+#23], drc0
	wdf drc0
	ba FindTA_3
FindTA_14:
	xor.testtaz.chan0 !c0, p0, r10, #0
	p0 ba FindTA_20
	ldad.bpcache r2, [r1,+#17], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba FindTA_16
	ldad.bpcache.fetch4 r3, [r2,+#1], drc0
	wdf drc0
	ldad.bpcache r7, [r4,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r7, r3
	p0 ba FindTA_15
	ba FindTA_13
FindTA_15:
	ldad.bpcache r7, [r6,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r7, r5
	p0 ba FindTA_16
	nop
	ba FindTA_13
FindTA_16:
	xor.testtaz.chan0 !c0, p0, r10, #0
	p0 ba FindTA_20
	ldad.bpcache r2, [r1,+#33], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 br FindTA_20
	mov r3, #0x00000088
	imae.u r3, r3.low, #1.low, r1, u32
FindTA_17:
	ldad.bpcache.fetch2 r4, [r3,#0++], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r5, r5
	p0 ba FindTA_18
	ldad.bpcache r6, [r5,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r6, r4
	p0 ba FindTA_18
	nop
	ba FindTA_13
FindTA_18:
	ldad.bpcache.fetch2 r4, [r3,#0++], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r5, r5
	p0 ba FindTA_19
	ldad.bpcache r6, [r5,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r6, r4
	p0 ba FindTA_19
	nop
	ba FindTA_13
FindTA_19:
	isub16.testtanz.chan0 r2, p0, r2, #1
	p0 ba FindTA_17
FindTA_20:
	mov r2, #0x00010000
	and.testtaz.chan0 !c0, p0, r9, r2
	p0 br FindTA_21
	mov r2, #0x00010000
	and.testtaz.chan0 !c0, p0, r9, r2
	p0 ba FindTA_21
	or r2, sa10, #0x00000000
	and.testtanz.chan0 !c0, p0, r2, #1
	p0 ba FindTA_22
	ldad.bpcache r2, [r0,+#6], drc1
	ldad.fetch4 r3, [r1,+#11], drc1
	wdf drc1
	stad.bpcache.fetch4 [r2,+#9], r3
	ldad.bpcache r3, [r2,+#0], drc0
	wdf drc0
	or r3, r3, #0x00000002
	stad.bpcache [r2,+#0], r3
	idf drc0, bif
	wdf drc0
	br FindTA_21
	nop
FindTA_21:
	p1 ba FindTA_22
	ldad.bpcache r3, [r14,+#32], drc0
	ldad r4, [r1,+#8], drc1
	wdf drc0
	and r3, r3, #0x00000040
	wdf drc1
	and r4, r4, #0x00000002
	or.testtanz.chan0 !c0, p0, r3, r4
	p0 ba DummyProcessTA
	or r2, r8, #0x00000000
	ba KickTA
	nop
FindTA_22:
	lapc

RenderDetailsUpdate:
	ldad r1, [r0,+#16], drc0
	ldad r2, [r0,+#8], drc1
	wdf drc0
	wdf drc1
	stad.bpcache [r1,+#78], #0
	stad.bpcache [r1,+#66], r2
	mov r3, #0x00000200
	and.testtaz.chan0 !c0, p0, r2, r3
	p0 ba RenderDetailsUpdate_1
	ldad.fetch2 r2, [r0,+#67], drc0
	wdf drc0
	stad.bpcache.fetch2 [r1,+#75], r2
RenderDetailsUpdate_1:
	ldad r3, [r0,+#9], drc0
	wdf drc0
	stad.bpcache [sa0,+#66], r3
	stad.bpcache [r1,+#92], r3
	ldad r3, [r0,+#10], drc0
	wdf drc0
	stad.bpcache [sa0,+#68], r3
	stad.bpcache [r1,+#93], r3
	ldad.fetch4 r2, [r0,+#28], drc0
	wdf drc0
	stad.bpcache.fetch4 [r1,+#69], r2
	ldad r2, [r0,+#17], drc0
	wdf drc0
	stad.bpcache [r1,+#69], r2
	ldad r4, [r0,+#33], drc0
	wdf drc0
	stad.bpcache [r1,+#103], r4
	and.testtaz.chan0 !c0, p0, r4, r4
	p0 ba RenderDetailsUpdate_3
	mov r5, #0x00000088
	imae.u r2, r5.low, #1.low, r0, u32
	mov r5, #0x000001A0
	imae.u r3, r5.low, #1.low, r1, u32
	ldad.fetch4 r5, [r2,#0++], drc0
RenderDetailsUpdate_2:
	isub16.testtaz.chan0 r4, p0, r4, #1
	wdf drc0
	stad.bpcache.fetch4 [r3,#0++], r5
	!p0 ldad.fetch4 r5, [r2,#0++], drc0
	!p0 ba RenderDetailsUpdate_2
RenderDetailsUpdate_3:
	ldad r3, [r0,+#20], drc0
	wdf drc0
	stad.bpcache [r1,+#79], r3
	ldad.fetch3 r3, [r0,+#1], drc0
	wdf drc0
	stad.bpcache.fetch3 [r1,+#86], r3
	ldad r3, [r0,+#5], drc0
	wdf drc0
	stad.bpcache [r1,+#90], r3
	ldad r2, [r0,+#21], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba RenderDetailsUpdate_5
	mov r3, #0x0000029C
	imae.s r3, r3.low, #1.low, r0, u32
	imae.u r4, r2.low, #8.low, r3, u32
	ldad.fetch2 r5, [r3,#0++], drc0
RenderDetailsUpdate_4:
	wdf drc0
	xor.testtanz.chan0 !c0, p0, r3, r4
	stad.bpcache [r5,+#0], r6
	p0 ldad.fetch2 r5, [r3,#0++], drc0
	p0 ba RenderDetailsUpdate_4
RenderDetailsUpdate_5:
	ldad r2, [r0,+#24], drc0
	wdf drc0
	stad.bpcache [r1,+#94], r2
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba RenderDetailsUpdate_7
	mov r3, #0x00000218
	imae.s r3, r3.low, #1.low, r0, u32
	imae.u r4, r2.low, #8.low, r3, u32
	ldad.fetch2 r5, [r3,#0++], drc0
RenderDetailsUpdate_6:
	wdf drc0
	xor.testtanz.chan0 !c0, p0, r3, r4
	stad.bpcache.fetch2 [r1,#95++], r5
	p0 ldad.fetch2 r5, [r3,#0++], drc0
	p0 ba RenderDetailsUpdate_6
RenderDetailsUpdate_7:
	idf drc1, bif
	wdf drc1
	lapc

AdvanceTACCBROff:
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

DummyProcessTA:
	mov sa5, #0x00000002
	ldad.bpcache r2, [r14,+#32], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r2, #64
	p0 ba DummyProcessTA_1
	ldad r3, [r1,+#1], drc0
	mov r4, #0x00000009
	wdf drc0
	and.testtanz.chan0 !c0, p0, r3, r4
	p0 ba DummyProcessTA_1
	or r6, r1, #0x00000000
	mov r7, pclink
	ba.savelink FreeContextMemory
	mov pclink, r7
	or r1, r6, #0x00000000
DummyProcessTA_1:
	ldad.bpcache r4, [r14,+#0], drc0
	wdf drc0
	or r16, r4, #0x00000000
	mov r17, #0x00000000
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	or r7, sa38, #0x00000000
	mov r9, #0x00000068
	ima32.s1.s r10, r14, #1, r9
	ima32.s2.s r8, r14, #1, r10
	mov r9, #0x00000000
DummyProcessTA_2:
	ldad.bpcache.fetch1 r2, [r8,#0++], drc0
	ldad.bpcache r3, [r14,+#30], drc0
	wdf drc0
	shr r3, r3, #0x00000006
DummyProcessTA_3:
	stad.bpcache.fetch16 [r2,#0++], #0
	isub16.testtanz.chan0 r3, p0, r3, #1
	p0 ba DummyProcessTA_3
	ima32.s1.u r9, r9, #1, #1
	xor.testtanz.chan0 !c0, p0, r7, r9
	p0 ba DummyProcessTA_2
	idf drc1, bif
	wdf drc1
	mov r16, #0x00000002
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	ldad r2, [r1,+#1], drc0
	mov r3, #0x00010000
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r3
	p0 ba DummyProcessTA_4
	mov r17, #0x00000000
	mov r23, pclink
	ba.savelink DPMThresholdUpdate
	mov pclink, r23
	nop
DummyProcessTA_4:
	mov r9, pclink
	or r10, r0, #0x00000000
	or r0, r1, #0x00000000
	ba.savelink RenderDetailsUpdate
	or r1, r0, #0x00000000
	or r0, r10, #0x00000000
	mov pclink, r9
	ldad.fetch2 r2, [r1,+#26], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba DummyProcessTA_5
	mov r4, #0x00000001
	imae.u r2, r4.low, #1.low, r2, u32
	stad.bpcache [r3,+#0], r2
	idf drc0, bif
DummyProcessTA_5:
	ldad r2, [r1,+#16], drc0
	wdf drc0
	ldad r2, [r1,+#23], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba DummyProcessTA_7
	mov r3, #0x00000118
	imae.u r3, r3.low, #1.low, r1, u32
	imae.u r2, r2.low, #8.low, r3, u32
	ldad.fetch2 r4, [r3,#0++], drc0
DummyProcessTA_6:
	wdf drc0
	xor.testtanz.chan0 !c0, p0, r2, r3
	stad.bpcache [r4,+#0], r5
	p0 ldad.fetch2 r4, [r3,#0++], drc0
	p0 ba DummyProcessTA_6
DummyProcessTA_7:
	mov r2, #0x00004000
	str #75, r2
	ldad r2, [r1,+#1], drc1
	ldad r3, [r1,+#8], drc1
	ldad.bpcache r4, [r14,+#32], drc0
	wdf drc1
	and.testtanz.chan0 !c0, p0, r2, #5
	and.testtaz.chan0 !c0, p1, r3, #8
	p1 and.testtanz.chan0 !c0, p1, r2, #2
	and.testtanz.chan0 !c0, p2, r2, #2
	wdf drc0
	p0 and r4, r4, #0x00000040
	p1 or r4, r4, #0x00000020
	and r4, r4, ~#0x0000001F
	stad.bpcache [r14,+#32], r4
	idf drc0, bif
	wdf drc0
	ldad r6, [r1,+#16], drc0
	wdf drc0
	!p0 ba DummyProcessTA_8
	ldad.bpcache r3, [r0,+#18], drc0
	wdf drc0
	ldad.bpcache r4, [r0,+#19], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 stad.bpcache [r0,+#18], r6
	stad.bpcache [r0,+#19], r6
	!p0 stad.bpcache [r4,+#86], r6
	!p0 stad.bpcache [r6,+#85], r4
	idf drc0, bif
	wdf drc0
DummyProcessTA_8:
	p1 ba DummyProcessTA_9
	p2 ba DummyProcessTA_9
	ldad.bpcache r4, [r6,+#67], drc0
	wdf drc0
	ldad.bpcache r3, [r4,+#1], drc0
	wdf drc0
	ima32.s1.u r3, r3, #1, #1
	stad.bpcache [r4,+#1], r3
	idf drc0, bif
	wdf drc0
	ba DummyProcessTA_10
DummyProcessTA_9:
	or r16, r0, #0x00000000
	or r17, r6, #0x00000000
	mov r23, pclink
	ba.savelink MoveSceneToCompleteList
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink Check2DQueue
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink Check3DQueues
	mov pclink, r23
	nop
DummyProcessTA_10:
	mov r5, pclink
	ba.savelink AdvanceTACCBROff
	mov pclink, r5
	ldad.bpcache r1, [r0,+#1], drc0
	wdf drc0
	ldad.bpcache r4, [r0,+#22], drc0
	ldad.bpcache r5, [r0,+#23], drc1
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, r4
	wdf drc1
	!p0 ba DummyProcessTA_11
	imae.u r2, r1.low, #1.low, #20, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r5, #0x00000000
DummyProcessTA_11:
	!p0 stad.bpcache [r4,+#23], r5
	and.testtaz.chan0 !c0, p1, r5, r5
	!p1 ba DummyProcessTA_12
	imae.u r3, r1.low, #1.low, #23, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r4, #0x00000000
DummyProcessTA_12:
	!p1 stad.bpcache [r5,+#22], r4
	stad.bpcache [r0,+#22], #0
	stad.bpcache [r0,+#23], #0
	idf drc1, bif
	wdf drc1
	ldad.bpcache r2, [r0,+#5], drc0
	ldad.bpcache r4, [r0,+#18], drc1
	wdf drc0
	wdf drc1
	or.testtaz.chan0 !c0, p1, r2, r4
	wdf drc0
	p1 ba DummyProcessTA_14
	imae.u r3, r1.low, #1.low, #23, u32
	or i.l, r3, #0x00000000
	or r4, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p0, r4, r4
	!p0 stad.bpcache [r4,+#23], r0
	!p0 ba DummyProcessTA_13
	imae.u r2, r1.low, #1.low, #20, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r0, #0x00000000
DummyProcessTA_13:
	imae.u r3, r1.low, #1.low, #23, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r0, #0x00000000
	stad.bpcache [r0,+#22], r4
DummyProcessTA_14:
	idf drc0, bif
	wdf drc0
	mov r23, pclink
	ba.savelink CheckTAQueue
	mov pclink, r23
	lapc

KickTA:
	mov sa5, #0x00000002
	stad.bpcache [sa0,+#52], r0
	stad.bpcache [sa0,+#49], r1
	or sa4, r2, #0x00000000
	idf drc0, bif
	wdf drc0
	ldad.bpcache r4, [r0,+#26], drc0
	wdf drc0
	stad.bpcache [sa0,+#81], r4
	and r29, r4, #0x0000FFFF
	or r29, r29, #0x30000000
	mov r30, #0x08001008
	mov.nosched r31, #0x08002000
	stad [r30,+#0], r29
	stad [r31,+#0], #0
	idf drc0, bif
	wdf drc0
	shr r29, r4, #0x00000010
	or r29, r29, #0x32000000
	mov r30, #0x08001008
	mov.nosched r31, #0x08002000
	stad [r30,+#0], r29
	stad [r31,+#0], #0
	idf drc0, bif
	wdf drc0
	ldad r4, [r1,+#16], drc0
	wdf drc0
	stad.bpcache [sa0,+#51], r4
	stad.bpcache [r14,+#1], r4
	idf drc0, bif
	wdf drc0
	ldad.bpcache r2, [sa0,+#50], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba KickTA_2
	xor.testtaz.chan0 !c0, p0, r2, r14
	!p0 ba KickTA_1
	ba KickTA_2
KickTA_1:
	ldr r4, #396, drc0
	wdf drc0
	and r4, r4, #0x00000004
	shr r11, r4, #0x00000002
	and.testtaz.chan0 !c0, p0, r11, #1
	p0 stad.bpcache [sa0,+#43], #0
	!p0 stad.bpcache [sa0,+#44], #0
	idf drc0, bif
	wdf drc0
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
	or r17, r11, #0x00000000
	mov r18, #0x00000000
	mov r23, pclink
	ba.savelink StoreDPMContext
	mov pclink, r23
KickTA_2:
	ldad.bpcache r2, [sa0,+#77], drc0
	ldad.bpcache r3, [r0,+#6], drc0
	ldad.bpcache r4, [sa0,+#78], drc0
	wdf drc0
	ldad.bpcache r5, [r3,+#0], drc0
	xor.testtaz.chan0 !c0, p0, r2, r3
	wdf drc0
	p0 ba KickTA_6
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba KickTA_3
	xor.testtaz.chan0 !c0, p0, r2, r4
	p0 ba KickTA_3
	mov r23, pclink
	ba.savelink StoreTAPB
	mov pclink, r23
KickTA_3:
	xor.testtanz.chan0 !c0, p0, r3, r4
	p0 ba KickTA_4
	mov r16, #0x00000001
	mov r23, pclink
	ba.savelink IdleCore
	mov pclink, r23
	mov r23, pclink
	ba.savelink Store3DPB
	mov pclink, r23
KickTA_4:
	xor.testtaz.chan0 !c0, p0, r2, r3
	p0 ba KickTA_5
	xor.testtanz.chan0 !c0, p0, r2, r4
	p0 ba KickTA_5
	mov r16, #0x00000001
	mov r23, pclink
	ba.savelink IdleCore
	mov pclink, r23
KickTA_5:
	or r16, r0, #0x00000000
	mov r17, #0x00000000
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	ldad r6, [r1,+#6], drc1
	ldad r7, [r1,+#7], drc1
	wdf drc1
	xor.testtanz.chan0 !c0, p0, r6, r7
	p0 mov r6, #0x00000001
	!p0 mov r6, #0x00000000
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	or r16, r3, #0x00000000
	mov r23, pclink
	ba.savelink SetupDPMPagetable
	mov pclink, r23
	or r16, r3, #0x00000000
	or r17, r6, #0x00000000
	mov r23, pclink
	ba.savelink LoadTAPB
	mov pclink, r23
	mov r16, #0x00000002
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	or r4, r16, #0x00000000
	mov r23, pclink
	ba.savelink Resume
	mov pclink, r23
	or r16, r4, #0x00000000
	ba KickTA_7
KickTA_6:
	and.testtaz.chan0 !c0, p0, r5, #2
	p0 ba KickTA_7
	ldad r6, [r1,+#6], drc1
	ldad r7, [r1,+#7], drc1
	wdf drc1
	xor.testtanz.chan0 !c0, p0, r6, r7
	p0 mov r17, #0x00000001
	!p0 mov r17, #0x00000000
	mov r23, pclink
	ba.savelink DPMThresholdUpdate
	mov pclink, r23
KickTA_7:
	mov r2, #0x00000238
	imae.u r2, r2.low, #1.low, r1, u32
	ldad.fetch8 r3, [r2,#0++], drc0
	wdf drc0
	str #129, r3
	str #130, r4
	str #131, r5
	str #132, r6
	str #133, r7
	str #134, r8
	str #142, r9
	str #143, r10
	mov r3, #0x00002087
	or r4, sa38, #0x00000000
KickTA_8:
	ldad.bpcache r5, [r2,#1++], drc0
	wdf drc0
	str r3, r5
	isub16.testtaz.chan0 r4, p0, r4, #1
	p0 ba KickTA_9
	mov r5, #0x00001000
	imae.u r3, r5.low, #1.low, r3, u32
	ba KickTA_8
KickTA_9:
	or r4, sa38, #0x00000000
	mov r3, #0x00000004
	ima16.s r3, r3, #1, r4.neg, u16, u16
	imae.u r2, r3.low, #4.low, r2, u32
	mov r3, #0x00002088
	or r4, sa38, #0x00000000
KickTA_10:
	ldad.bpcache r5, [r2,#1++], drc0
	wdf drc0
	str r3, r5
	isub16.testtaz.chan0 r4, p0, r4, #1
	p0 ba KickTA_11
	mov r5, #0x00001000
	imae.u r3, r5.low, #1.low, r3, u32
	ba KickTA_10
KickTA_11:
	or r4, sa38, #0x00000000
	mov r3, #0x00000004
	ima16.s r3, r3, #1, r4.neg, u16, u16
	imae.u r2, r3.low, #4.low, r2, u32
	ldad.fetch7 r3, [r2,#0++], drc0
	wdf drc0
	str #144, r3
	str #145, r4
	str #146, r5
	str #641, r6
	str #680, r7
	str #804, r8
	str #226, r9
	ldad r3, [r2,#1++], drc0
	wdf drc0
	str #148, r3
	ldad r3, [r2,#1++], drc0
	wdf drc0
	str #4177, r3
	mov r23, pclink
	ba.savelink USEClkGateOn
	mov pclink, r23
	ldr r4, #669, drc0
	ldad r5, [r1,+#6], drc1
	wdf drc1
	ldad r6, [r1,+#7], drc1
	wdf drc1
	xor.testtaz.chan0 !c0, p3, r5, r6
	p3 ba KickTA_13
	or r2, sa9, #0x00000000
	wdf drc0
	mov r7, #0x20000000
	and.testtaz.chan0 !c0, p3, r2, r7
	p3 ba KickTA_12
	ldr r3, #457, drc0
	wdf drc0
	shr r3, r3, #0x00000010
	ldr r2, #393, drc0
	wdf drc0
	isub16.testnat.chan0 !c0, p3, r3, r2
	p3 ba KickTA_13
KickTA_12:
	or r5, r6, #0x00000000
KickTA_13:
	wdf drc0
	and r4, r4, ~#0x3C000000
	or r4, r4, r5
	str #669, r4
	or r16, r0, #0x00000000
	mov r17, #0x00000000
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	ldad r2, [r1,+#1], drc0
	mov r4, #0x00000040
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r4
	p0 ba KickTA_16
	str #516, #1
	mov r5, #0x00000080
KickTA_14:
	ldr r4, #70, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, r5
	p0 ba KickTA_14
	mov r5, #0x00000080
	str #69, r5
	str #514, #1
	mov r5, #0x04000000
KickTA_15:
	ldr r4, #75, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, r5
	p0 ba KickTA_15
	mov r5, #0x04000000
	str #77, r5
KickTA_16:
	mov r4, #0x00000020
	and.testtaz.chan0 !c0, p0, r2, r4
	p0 ba KickTA_18
	str #692, #1
	str #696, #1
KickTA_17:
	ldr r4, #78, drc0
	wdf drc0
	and r4, r4, #0x00000003
	xor.testtanz.chan0 !c0, p0, r4, #3
	p0 ba KickTA_17
	str #80, #3
KickTA_18:
	and.testtaz.chan0 !c0, p0, r2, #16
	p0 ba KickTA_19
	and.testtanz.chan0 !c0, p0, r22, #64
	p0 ba KickTA_19
	str #642, #1
	or r22, r22, #0x00000040
KickTA_19:
	mov r16, #0x00000001
	mov r17, #0x00000010
	mov r23, pclink
	ba.savelink InvalidateSLC
	mov pclink, r23
	ldad.bpcache r3, [sa0,+#43], drc0
	ldad.bpcache r4, [sa0,+#44], drc1
	mov r5, #0x00000000
	wdf drc0
	wdf drc1
	xor.testtaz.chan0 !c0, p1, r14, r3
	p1 ba KickTA_20
	xor.testtaz.chan0 !c0, p1, r14, r4
	p1 mov r5, #0x00000004
	p1 or r3, r4, #0x00000000
	p1 ba KickTA_20
	and.testtaz.chan0 !c0, p1, r3, r3
	p1 ba KickTA_20
	and.testtaz.chan0 !c0, p1, r4, r4
	p1 mov r5, #0x00000004
	p1 or r3, r4, #0x00000000
	p1 ba KickTA_20
	ldr r5, #396, drc0
	wdf drc0
	and r5, r5, #0x00000002
	shr r5, r5, #0x00000001
	xor r5, r5, #0x00000001
	and.testtanz.chan0 !c0, p1, r5, #1
	shl r5, r5, #0x00000002
	p1 or r3, r4, #0x00000000
KickTA_20:
	and.testtaz.chan0 !c0, p1, r5, #4
	p1 stad.bpcache [sa0,+#43], r14
	!p1 stad.bpcache [sa0,+#44], r14
	stad.bpcache [sa0,+#50], r14
	idf drc0, bif
	wdf drc0
	ldr r2, #396, drc0
	wdf drc0
	and r2, r2, ~#0x00000004
	or r2, r2, r5
	str #396, r2
	shr r5, r5, #0x00000002
	and.testtaz.chan0 !c0, p1, r3, r3
	!p1 xor.testtaz.chan0 !c0, p1, r3, r14
	p1 ba KickTA_21
	ldad.bpcache r4, [r3,+#0], drc0
	wdf drc0
	or r16, r4, #0x00000000
	mov r17, #0x00000000
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	or r16, r3, #0x00000000
	or r17, r5, #0x00000000
	mov r18, #0x00000000
	mov r23, pclink
	ba.savelink StoreDPMContext
	mov pclink, r23
	or r16, r0, #0x00000000
	mov r17, #0x00000000
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
KickTA_21:
	ldad r2, [r1,+#1], drc0
	wdf drc0
	mov r4, #0x00000009
	and.testtanz.chan0 !c0, p0, r2, r4
	p0 ba KickTA_22
	xor.testtaz.chan0 !c0, p0, r14, r3
	p0 ba KickTA_27
	or r16, r0, #0x00000000
	mov r17, #0x00000000
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	or r16, r14, #0x00000000
	or r17, r5, #0x00000000
	mov r18, #0x00000000
	mov r23, pclink
	ba.savelink LoadDPMContext
	mov pclink, r23
	ba KickTA_27
KickTA_22:
	mov r2, #0x000010D7
	str r2, #1
	mov r2, #0x000010DA
	str r2, #1
	str #224, #1
	ldr r2, #396, drc0
	wdf drc0
	and r2, r2, ~#0x00000001
	or r2, r2, #0x00000008
	or r2, r2, r5
	str #396, r2
	str #421, #2
	mov r3, #0x00000240
	str #422, #2
	str #147, #1
	or r3, r3, #0x00100000
KickTA_23:
	ldr r2, #75, drc0
	wdf drc0
	and r2, r2, r3
	xor.testtanz.chan0 !c0, p0, r2, r3
	p0 ba KickTA_23
	str #77, r3
	mov r3, #0x00000000
	mov r4, #0x00000000
	or r3, r3, #0x00000020
	or r4, r4, #0x00000020
KickTA_24:
	ldr r2, #70, drc0
	wdf drc0
	and r2, r2, r3
	xor.testtanz.chan0 !c0, p0, r2, r3
	p0 ba KickTA_24
	str #69, r4
	mov r4, #0x000010D8
KickTA_25:
	ldr r2, r4, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, #1
	p0 ba KickTA_25
	mov r4, #0x000010DB
KickTA_26:
	ldr r2, r4, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, #1
	p0 ba KickTA_26
	mov r2, #0x000010DC
	str r2, #1
	mov r3, #0x00001208
	ldr r3, r3, drc0
	wdf drc0
	str #468, r3
KickTA_27:
	ldad.bpcache r2, [sa0,+#51], drc0
	mov r4, #0x00000144
	wdf drc0
	imae.u r3, r4.low, #1.low, r2, u32
	mov r4, #0x0000209D
	or r5, sa38, #0x00000000
KickTA_28:
	ldad.bpcache r6, [r3,#1++], drc0
	wdf drc0
	str r4, r6
	isub16.testtaz.chan0 r5, p0, r5, #1
	p0 ba KickTA_29
	mov r6, #0x00001000
	imae.u r4, r6.low, #1.low, r4, u32
	ba KickTA_28
KickTA_29:
	or r5, sa38, #0x00000000
	mov r4, #0x00000004
	ima16.s r4, r4, #1, r5.neg, u16, u16
	imae.u r3, r4.low, #4.low, r3, u32
	ldad.bpcache r3, [r14,+#32], drc0
	ldr r4, #134, drc0
	wdf drc0
	or r4, r4, #0x01000000
	shl.testnat.chan0 !c0, p0, r3, #22
	p0 or r4, r4, #0x00200000
	str #134, r4
	stad.bpcache [sa0,+#87], #0
	ldad.bpcache r3, [sa0,+#86], drc0
	wdf drc0
	or r3, r3, #0x00000001
	stad.bpcache [sa0,+#86], r3
	ldad r3, [r1,+#1], drc0
	wdf drc0
	mov r4, #0x00000005
	and.testtaz.chan0 !c0, p0, r3, r4
	p0 ba KickTA_30
	stad.bpcache [r14,+#32], #0
	idf drc0, bif
	wdf drc0
KickTA_30:
	and.testtanz.chan0 !c0, p0, r3, #1
	p0 ba KickTA_32
	ldad.bpcache r3, [r2,+#80], drc0
	wdf drc0
	or r3, r3, #0x00000002
	mov r2, #0x000010D7
	str r2, r3
KickTA_31:
	mov r2, #0x000010D8
	ldr r2, r2, drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, #2
	p0 ba KickTA_31
KickTA_32:
	or r1, sa9, #0x00000000
	or r1, r1, #0x00000001
	or sa9, r1, #0x00000000
	or r5, sa4, #0x00000000
	ldad.bpcache r1, [r0,+#22], drc0
	wdf drc0
	ldad.bpcache r4, [r0,+#23], drc0
	and.testtanz.chan0 !c0, p0, r1, r1
	wdf drc0
	and.testtanz.chan0 !c0, p1, r4, r4
	p0 stad.bpcache [r1,+#23], r4
	p1 stad.bpcache [r4,+#22], r1
	p0 ba KickTA_33
	imae.u r2, r5.low, #1.low, #20, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r4, #0x00000000
KickTA_33:
	p1 ba KickTA_34
	imae.u r3, r5.low, #1.low, #23, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r1, #0x00000000
KickTA_34:
	idf drc0, bif
	wdf drc0
	imae.u r2, r5.low, #1.low, #20, u32
	or i.l, r2, #0x00000000
	or r1, sa[il + #0], #0x00000000
	and.testtanz.chan0 !c0, p0, r1, r1
	stad.bpcache [r0,+#22], #0
	stad.bpcache [r0,+#23], r1
	p0 stad.bpcache [r1,+#22], r0
	imae.u r2, r5.low, #1.low, #20, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r0, #0x00000000
	p0 ba KickTA_35
	imae.u r3, r5.low, #1.low, #23, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r0, #0x00000000
KickTA_35:
	idf drc0, bif
	wdf drc0
	ldad.bpcache r0, [sa0,+#49], drc0
	wdf drc0
	mov r9, pclink
	ba.savelink RenderDetailsUpdate
	mov pclink, r9
	ldr r10, #457, drc0
	wdf drc0
	and r10, r10, #0xFFFF0000
	shr r10, r10, #0x00000010
	ldad.bpcache r1, [sa0,+#49], drc0
	wdf drc0
	ldad r2, [r1,+#1], drc0
	mov r4, #0x00004000
	wdf drc0
	or r3, r2, #0x00000000
	and r3, r3, ~#0xF0000000
	or r3, r3, #0x20000000
	mov r30, #0x08001008
	mov.nosched r31, #0x08002000
	stad [r30,+#0], r3
	stad [r31,+#0], #0
	idf drc0, bif
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r4
	p0 ba KickTA_36
	mov r4, #0x000001F4
	stad.bpcache [sa1,+#31], r4
	idf drc0, bif
	wdf drc0
KickTA_36:
	str #128, #1
	ldad.bpcache r3, [sa0,+#66], drc0
	wdf drc0
	or r3, r3, #0x10000000
	mov r30, #0x08001008
	mov.nosched r31, #0x08002000
	stad [r30,+#0], r3
	stad [r31,+#0], #0
	idf drc0, bif
	wdf drc0
	lapc
