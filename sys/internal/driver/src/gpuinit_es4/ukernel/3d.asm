/* Master: 3D events, render and transfer run lists, kicks. */

/* 3D event task: ThreeDMemFree or PixelEndRender. */
	.align2
ThreeDEventHandler:
	smlsi #0, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	mov r15, #0x00000000
	mov r22, #0x00000000
	and r0, pa0, #0x00000004
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 br ThreeDEventHandler_1
	mov r0, #0x00000001
	str #77, r0	/* EUR_CR_EVENT_HOST_CLEAR */
	mov r15, #0x00001600
	ba ThreeDMemFree
ThreeDEventHandler_1:
	and r0, pa0, #0x00000008
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 br ThreeDEventHandler_4
	or r3, sa10, #0x00000000
	and.testtanz.chan0 !c0, p0, r3, #4
	p0 ba ThreeDEventHandler_3
	ldr r10, #458, drc0	/* EUR_CR_DPM_PAGE */
	wdf drc0
	and r10, r10, #0x0000FFFF
	shr r10, r10, #0x00000000
	and.testtanz.chan0 !c0, p0, r3, #2
	p0 ba ThreeDEventHandler_2
	ba ThreeDEventHandler_3
	nop
ThreeDEventHandler_2:
	ba ThreeDEventHandler_3
ThreeDEventHandler_3:
	mov r0, #0x00040000
	str #77, r0	/* EUR_CR_EVENT_HOST_CLEAR */
	mov r15, #0x00001613
	ba PixelEndRender
ThreeDEventHandler_4:
	and r0, pa1, #0x00000020
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 br ThreeDEventHandler_5
ThreeDEventHandler_5:
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink CommonProgramEnd
	mov pclink, r23

/* 3D loopback task: Find3D or finish a render. */
ThreeDHandler:
	smlsi #0, #0, #0, #1, incrementmode, incrementmode, incrementmode, incrementmode, #0, #0, #0
	mov r15, #0x00000000
	mov r22, #0x00000000
	ldad.bpcache r0, [sa0,+#56], drc0
	xor r1, pa0, ~#0x00000000
	wdf drc0
	and r0, r0, r1
	stad.bpcache [sa0,+#56], r0
	and r0, pa0, #0x00000001
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba ThreeDHandler_1
	mov r0, #0x00000001
	str #77, r0	/* EUR_CR_EVENT_HOST_CLEAR */
	mov r15, #0x00001629
	ba ThreeDMemFree
ThreeDHandler_1:
	and r0, pa0, #0x00000002
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba ThreeDHandler_2
	mov r0, #0x00040000
	str #77, r0	/* EUR_CR_EVENT_HOST_CLEAR */
	mov r15, #0x00001630
	ba PixelEndRender
ThreeDHandler_2:
	and r0, pa0, #0x00000008
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba ThreeDHandler_3
ThreeDHandler_3:
	and r0, pa0, #0x00080000
	and.testtanz.chan0 !c0, p0, r0, r0
	!p0 ba ThreeDHandler_4
	ba.savelink Find3D
ThreeDHandler_4:
	mov r16, #0x00000000
	mov r23, pclink
	ba.savelink CommonProgramEnd
	mov pclink, r23

/* DPM freed the render's memory: RenderFinished. */
ThreeDMemFree:
	or r0, sa10, #0x00000000
	and r0, r0, ~#0x00000020
	and.testtaz.chan0 !c0, p1, r0, #16
	p1 ba RenderFinished
	or sa10, r0, #0x00000000
	mov pclink, r15
	nop
	lapc

/* Pixel back end done: RenderFinished. */
PixelEndRender:
	or r0, sa10, #0x00000000
	and.testtanz.chan0 !c0, p0, r0, #4
	p0 ba KickTransferRender_15
	ldr r1, #0, drc0	/* EUR_CR_CLKGATECTL */
	wdf drc0
	and r1, r1, #0x00000003
	xor.testtanz.chan0 !c0, p0, r1, #2
	p0 ba PixelEndRender_3
PixelEndRender_1:
	or r2, sa38, #0x00000000
	mov r1, #0x00000000
PixelEndRender_2:
	ima32.s1.s r2, r2, #1, #1.neg
	imae.u r3, r2.low, #1.low, #2, u32
	shl r3, r3, #0x0000000C
	or r3, r3, #0x00000002
	ldr r3, r3, drc0
	wdf drc0
	or r1, r1, r3
	and.testtanz.chan0 !c0, p0, r2, r2
	p0 ba PixelEndRender_2
	and.testtanz.chan0 !c0, p0, r1, #1
	p0 ba PixelEndRender_1
PixelEndRender_3:
	and.testtaz.chan0 !c0, p1, r0, #32
	and r0, r0, ~#0x00000010
	p1 ba RenderFinished
	or sa10, r0, #0x00000000
	mov pclink, r15
	nop
	lapc

/* Adds the visibility query results to the host totals. */
VisibilityQueryAccumulator:
	ldad.bpcache r0, [r0,+#79], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r0, r0
	p1 ba VisibilityQueryAccumulator_4
	mov r2, #0x00000000
	imae.u r3, r2.low, #1.low, #2, u32
	shl r3, r3, #0x0000000C
	or r3, r3, #0x00000130
	ldr r3, r3, drc0
	wdf drc0
	or r2, sa38, #0x00000000
	isub16.testtaz.chan0 r2, p1, r2, #1
	p1 ba VisibilityQueryAccumulator_4
	or r4, r3, #0x00000000
	imae.u r5, r0.low, #4.low, r4, u32
	mov r16, #0x00000001
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
VisibilityQueryAccumulator_1:
	or r1, r0, #0x00000000
	or r4, r3, #0x00000000
VisibilityQueryAccumulator_2:
	ldad.bpcache r6, [r4,+#0], drc0
	ldad r7, [r5,#1++], drc0
	wdf drc0
	ima32.s1.s r8, r6, #1, r7
	ima32.s2.s r6, r6, #1, r8
	stad.bpcache [r4,#1++], r6
	isub16.testtanz.chan0 r1, p1, r1, #1
	p1 ba VisibilityQueryAccumulator_2
	isub16.testtanz.chan0 r2, p1, r2, #1
	p1 ba VisibilityQueryAccumulator_1
	idf drc0, bif
	wdf drc0
	str #4939, #16	/* EUR_CR_MASTER_SLC_CTRL_FLUSH */
VisibilityQueryAccumulator_3:
	ldr r1, #4932, drc0	/* EUR_CR_MASTER_SLC_EVENT_STATUS */
	wdf drc0
	and.testtaz.chan0 !c0, p1, r1, #2
	p1 ba VisibilityQueryAccumulator_3
	str #4933, #2	/* EUR_CR_MASTER_SLC_EVENT_CLEAR */
	mov r16, #0x00000002
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
VisibilityQueryAccumulator_4:
	lapc

/* Writes the render's sync status values. */
UpdateRenderStatusValues:
	ldad.bpcache r2, [r0,+#69], drc1
	wdf drc1
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba UpdateRenderStatusValues_1
	ldad.bpcache.fetch2 r2, [r2,+#3], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba UpdateRenderStatusValues_1
	mov r4, #0x00000001
	imae.u r2, r4.low, #1.low, r2, u32
	stad.bpcache [r3,+#0], r2
UpdateRenderStatusValues_1:
	ldad.bpcache r2, [r0,+#66], drc0
	mov r3, #0x00000200
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r3
	p0 ba UpdateRenderStatusValues_2
	ldad.bpcache.fetch2 r2, [r0,+#75], drc0
	wdf drc0
	mov r4, #0x00000001
	imae.u r2, r4.low, #1.low, r2, u32
	stad.bpcache [r3,+#0], r2
UpdateRenderStatusValues_2:
	ldad.bpcache.fetch2 r2, [r0,+#71], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba UpdateRenderStatusValues_3
	mov r4, #0x00000001
	imae.u r2, r4.low, #1.low, r2, u32
	stad.bpcache [r3,+#0], r2
UpdateRenderStatusValues_3:
	ldad.bpcache r2, [r0,+#103], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba UpdateRenderStatusValues_5
	mov r3, #0x000001A0
	imae.s r3, r3.low, #1.low, r0, u32
	ldad.bpcache.fetch2 r4, [r3,#0++], drc0
UpdateRenderStatusValues_4:
	isub16.testtaz.chan0 r2, p0, r2, #1
	mov r6, #0x00000008
	imae.u r3, r6.low, #1.low, r3, u32
	mov r6, #0x00000001
	wdf drc0
	imae.u r4, r6.low, #1.low, r4, u32
	stad.bpcache [r5,+#0], r4
	!p0 ldad.bpcache.fetch2 r4, [r3,#0++], drc0
	!p0 ba UpdateRenderStatusValues_4
UpdateRenderStatusValues_5:
	ldad.bpcache r2, [r0,+#94], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba UpdateRenderStatusValues_7
	mov r3, #0x0000017C
	imae.s r3, r3.low, #1.low, r0, u32
	imae.u r2, r2.low, #8.low, r3, u32
	ldad.bpcache.fetch2 r4, [r3,#0++], drc0
UpdateRenderStatusValues_6:
	xor.testtanz.chan0 !c0, p0, r2, r3
	wdf drc0
	stad.bpcache [r4,+#0], r5
	p0 ldad.bpcache.fetch2 r4, [r3,#0++], drc0
	p0 ba UpdateRenderStatusValues_6
UpdateRenderStatusValues_7:
	ldad.bpcache r4, [r1,+#31], drc0
	wdf drc0
	ldad.bpcache r3, [r4,+#0], drc0
	wdf drc0
	or r3, r3, #0x00000001
	stad.bpcache [r4,+#0], r3
	idf drc0, bif
	wdf drc0
	or r2, sa9, #0x00000000
	shl.testpat.chan0 !c0, p0, r2, #25
	p0 ba UpdateRenderStatusValues_8
	and r2, r2, ~#0x00000040
	or sa9, r2, #0x00000000
	mov r4, pclink
	ba.savelink TARestartWithoutRender
	mov pclink, r4
UpdateRenderStatusValues_8:
	lapc

/* Releases what a finished render held. */
ReleaseRenderResources:
	ldad.bpcache r2, [r0,+#66], drc0
	wdf drc0
	ldad.bpcache r4, [r1,+#32], drc0
	wdf drc0
	and r4, r4, ~#0x00000060
	or r3, r4, #0x00000010
	stad.bpcache [r1,+#32], r3
	mov sa12, #0x00000000
	mov sa13, #0x00000007
	mov r3, #0x00000400
	and.testtanz.chan0 !c0, p0, r2, r3
	!p0 br ReleaseRenderResources_1
	ldad.bpcache r4, [r0,+#67], drc0
	wdf drc0
	ldad.bpcache r3, [r4,+#1], drc0
	wdf drc0
	ima32.s1.u r3, r3, #1, #1
	stad.bpcache [r4,+#1], r3
ReleaseRenderResources_1:
	stad.bpcache [r1,+#1], #0
	and.testtanz.chan0 !c0, p0, r2, #12
	ldad.bpcache r2, [r0,+#69], drc0
	p0 ba ReleaseRenderResources_2
	ldad.bpcache r3, [r0,+#65], drc1
	wdf drc1
	stad.bpcache [r3,+#0], #0
	ldad.bpcache r3, [sa0,+#43], drc1
	ldad.bpcache r4, [sa0,+#44], drc0
	wdf drc1
	xor.testtaz.chan0 !c0, p0, r1, r3
	p0 stad.bpcache [sa0,+#43], #0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r1, r4
	p0 stad.bpcache [sa0,+#44], #0
ReleaseRenderResources_2:
	wdf drc0
	and.testtanz.chan0 !c0, p0, r2, r2
	!p0 ba ReleaseRenderResources_3
	ldad.bpcache r2, [r2,+#0], drc0
	wdf drc0
	stad.bpcache [r2,+#0], #0
ReleaseRenderResources_3:
	idf drc0, bif
	wdf drc0
	mov r3, #0x00004000
	str #75, r3	/* EUR_CR_EVENT_STATUS */
	lapc

/* Render done: free its memory, flush SLC, queries, notify host, next work. */
RenderFinished:
	and.testtanz.chan0 !c0, p0, r0, #2
	shl.testnat.chan0 !c0, p2, r0, #25
	and r1, r0, ~#0x00003041
	or sa10, r1, #0x00000000
	or r2, sa9, #0x00000000
	and.testtanz.chan0 !c0, p3, r2, #1
	p3 ba RenderFinished_1
	mov r23, pclink
	ba.savelink USEClkGateAuto
	mov pclink, r23
	nop
RenderFinished_1:
	p2 ba.savelink TARestartWithoutRender
	nop
	p0 ba RenderFinished_2
	ldad.bpcache r0, [sa0,+#55], drc0
	wdf drc0
	ldad.bpcache r1, [r0,+#66], drc0
	wdf drc0
	mov r2, #0x00000040
	and.testtanz.chan0 !c0, p1, r1, r2
	mov r1, #0x00000001
	p1 ba.savelink VisibilityQueryAccumulator
RenderFinished_2:
	ldr r2, #288, drc0	/* EUR_CR_ISP_ZLSCTL */
	wdf drc0
	and r3, r2, #0x00E00000
	mov r4, #0x00600000
	xor.testtaz.chan0 !c0, p1, r3, r4
	!p1 mov r4, #0x00800000
	!p1 xor.testtaz.chan0 !c0, p1, r3, r4
	p1 mov r4, #0x00100000
	p1 and.testtaz.chan0 !c0, p1, r2, r4
	!p1 ba RenderFinished_3
	ldad.bpcache r2, [sa0,+#64], drc0
	wdf drc0
	mov r3, #0x00001053
	str r3, r2
RenderFinished_3:
	!p0 ba RenderFinished_4
	mov r16, #0x00100000
	mov r23, pclink
	ba.savelink EmitLoopBack
	mov pclink, r23
	ba RenderFinished_20
RenderFinished_4:
	or r3, sa6, #0x00000000
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 br RenderFinished_12
	ldad.bpcache r2, [sa0,+#55], drc1
	wdf drc1
	ldad.bpcache r3, [r2,+#66], drc0
	mov r4, #0x00003000
	wdf drc0
	and.testtanz.chan0 !c0, p0, r3, r4
	p0 ba RenderFinished_12
	ldad.bpcache r14, [sa0,+#54], drc0
	wdf drc0
	ldad.bpcache r1, [r14,+#2], drc0
	wdf drc1
	ldad.bpcache r3, [r2,+#78], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p1, r1, r3
	p1 ba RenderFinished_12
	ldad.bpcache r0, [r14,+#6], drc0
	wdf drc0
	imae.u r3, r3.low, #1.low, #1, u32
RenderFinished_5:
	imae.u r4, r3.low, #4.low, r0, u32
	ldad r4, [r4,+#0], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r4, #1
	p0 ba RenderFinished_6
	xor.testtaz.chan0 !c0, p0, r1, r3
	p0 ba RenderFinished_12
	imae.u r3, r3.low, #1.low, #1, u32
	ba RenderFinished_5
RenderFinished_6:
	stad.bpcache [r2,+#78], r3
	or r0, sa10, #0x00000000
	wdf drc0
	and.testtaz.chan0 !c0, p0, r0, #8
	p0 ba RenderFinished_7
	and r0, r0, ~#0x00000408
	or sa10, r0, #0x00000000
	ldad.bpcache r1, [r14,+#31], drc0
	wdf drc0
	ldad.bpcache r2, [r1,+#0], drc0
	wdf drc0
	or r2, r2, #0x00000004
	stad.bpcache [r1,+#0], r2
	mov r23, pclink
	ba.savelink CheckTAQueue
	mov pclink, r23
	nop
	ba.savelink Find3D
	nop
	ba RenderFinished_20
RenderFinished_7:
	ldad.bpcache r6, [r14,+#4], drc1
	mov r0, #0x0000000C
	imae.u r4, r0.low, r3.low, #0, u32
	shl r4, r4, #0x00000004
	mov r5, #0x00000000
RenderFinished_8:
	imae.u r7, r5.low, #4.low, #44, u32
	imae.u r7, r7.low, #1.low, r2, u32
	ldad.bpcache r7, [r7,+#0], drc1
	wdf drc1
	imae.u r0, r4.low, r6.low, r7, u32
	xor.testtaz.chan0 !c0, p0, r5, #0
	p0 str #258, r0	/* EUR_CR_ISP_RGN_BASE */
	p0 ba RenderFinished_9
	mov r7, #0x00001143
	imae.u r7, r5.low, #1.low, r7, u32
	str r7, r0
RenderFinished_9:
	or r7, sa38, #0x00000000
	imae.u r5, r5.low, #1.low, #1, u32
	xor.testtanz.chan0 !c0, p0, r5, r7
	p0 ba RenderFinished_8
	ldad.bpcache r5, [r2,+#66], drc0
	or r4, sa10, #0x00000000
	wdf drc0
	and.testtanz.chan0 !c0, p0, r5, #8
	p0 ba RenderFinished_10
	xor.testtaz.chan0 !c0, p1, r1, r3
	or r4, r4, #0x00000031
	!p1 mov r3, #0x00000002
	p1 mov r3, #0x00000003
	ba RenderFinished_11
RenderFinished_10:
	mov r3, #0x00000000
	or r4, r4, #0x00000011
RenderFinished_11:
	str #399, r3	/* EUR_CR_DPM_3D_DEALLOCATE */
	or sa10, r4, #0x00000000
	stad.bpcache [sa0,+#89], #0
	ldad.bpcache r2, [sa0,+#86], drc0
	wdf drc0
	or r2, r2, #0x00000002
	stad.bpcache [sa0,+#86], r2
	idf drc0, bif
	wdf drc0
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
	mov r23, pclink
	ba.savelink USEClkGateOn
	mov pclink, r23
	ldad.bpcache r2, [r14,+#5], drc0
	wdf drc0
	mov r10, #0x00001147
	str r10, r2
	ldr r10, #458, drc0	/* EUR_CR_DPM_PAGE */
	wdf drc0
	and r10, r10, #0x0000FFFF
	shr r10, r10, #0x00000000
	str #266, #1	/* EUR_CR_ISP_START_RENDER */
	ba RenderFinished_20
RenderFinished_12:
	or r0, sa10, #0x00000000
	and r0, r0, ~#0x00000008
	or sa10, r0, #0x00000000
	ldad.bpcache r0, [sa0,+#55], drc0
	ldad.bpcache r1, [sa0,+#54], drc1
	wdf drc0
	wdf drc1
	ldad.bpcache r2, [r0,+#66], drc0
	wdf drc0
	mov r3, #0x00000800
	and.testtaz.chan0 !c0, p1, r2, r3
	p1 ba RenderFinished_14
	str #4939, #64	/* EUR_CR_MASTER_SLC_CTRL_FLUSH */
RenderFinished_13:
	ldr r2, #4932, drc0	/* EUR_CR_MASTER_SLC_EVENT_STATUS */
	wdf drc0
	and.testtaz.chan0 !c0, p1, r2, #2
	p1 ba RenderFinished_13
	str #4933, #2	/* EUR_CR_MASTER_SLC_EVENT_CLEAR */
RenderFinished_14:
	ba.savelink UpdateRenderStatusValues
	or r1, sa6, #0x00000000
	or r0, sa7, #0x00000000
	ldad.bpcache r4, [r0,+#25], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, r4
	imae.u r2, r1.low, #1.low, #26, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r4, #0x00000000
	!p0 ba RenderFinished_15
	imae.u r3, r1.low, #1.low, #29, u32
	or i.l, r3, #0x00000000
	mov sa[il + #0], #0x00000000
RenderFinished_15:
	!p0 stad.bpcache [r4,+#24], #0
	stad.bpcache [r0,+#25], #0
	idf drc0, bif
	wdf drc0
	ldad.bpcache r4, [r0,+#20], drc0
	wdf drc0
	ldad.bpcache r5, [r4,+#86], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r5, r5
	stad.bpcache [r0,+#20], r5
	p1 stad.bpcache [r0,+#21], #0
	!p1 stad.bpcache [r5,+#85], #0
	stad.bpcache [r4,+#86], #0
	idf drc0, bif
	wdf drc0
	p1 ba RenderFinished_17
	imae.u r3, r1.low, #1.low, #29, u32
	or i.l, r3, #0x00000000
	or r4, sa[il + #0], #0x00000000
	!p0 stad.bpcache [r4,+#25], r0
	!p0 ba RenderFinished_16
	imae.u r2, r1.low, #1.low, #26, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r0, #0x00000000
RenderFinished_16:
	imae.u r3, r1.low, #1.low, #29, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r0, #0x00000000
	stad.bpcache [r0,+#24], r4
	idf drc0, bif
	wdf drc0
RenderFinished_17:
	ldad.bpcache r0, [sa0,+#55], drc0
	ldad.bpcache r1, [sa0,+#54], drc0
	wdf drc0
	ba.savelink ReleaseRenderResources
	or r1, sa9, #0x00000000
	mov r0, #0x00000018
	and r2, r1, r0
	mov r0, #0x00000008
	xor.testtanz.chan0 !c0, p1, r2, r0
	p1 ba RenderFinished_18
	mov r0, #0x00000010
	or r1, r1, r0
	or sa9, r1, #0x00000000
RenderFinished_18:
	or r0, sa9, #0x00000000
	shl.testpat.chan0 !c0, p0, r0, #11
	p0 ba RenderFinished_19
	mov r16, #0x00000080
	mov r23, pclink
	ba.savelink EmitLoopBack
	mov pclink, r23
	ba RenderFinished_20
RenderFinished_19:
	mov r23, pclink
	ba.savelink CheckTAQueue
	mov pclink, r23
	mov r23, pclink
	ba.savelink Check2DQueue
	mov pclink, r23
	nop
	ba.savelink Find3D
RenderFinished_20:
	mov pclink, r15
	lapc

/* Picks the next 3D work: transfer or render. */
Find3D:
	or r0, sa11, #0x00000000
	or r1, sa10, #0x00000000
	and.testtaz.chan0 !c0, p0, r0, #3
	p0 ba Find3D_1
	nop
	ba Find3D_6
Find3D_1:
	and.testtanz.chan0 !c0, p0, r1, #10
	p0 ba Find3D_6
	and.testtanz.chan0 !c0, p0, r1, #1
	!p0 ba Find3D_2
	and.testtanz.chan0 !c0, p1, r1, #4
	p1 ba Find3D_6
Find3D_2:
	mov r1, #0x00000000
Find3D_3:
	!p0 ba Find3D_4
	or r2, sa6, #0x00000000
	isub16.testpat.chan0 !c0, p1, r1, r2
	p1 ba Find3D_6
Find3D_4:
	mov r0, pclink
	ba.savelink CheckTransferRunList
	ba.savelink CheckRenderRunlist
	mov pclink, r0
	or r2, sa5, #0x00000000
	imae.u r1, r1.low, #1.low, #1, u32
	isub16.testnat.chan0 !c0, p1, r2, r1
	!p1 ba Find3D_3
	or r3, sa10, #0x00000000
	and.testtaz.chan0 !c0, p1, r3, #64
	p1 ba Find3D_5
	mov r4, #0x00004000
	and.testtaz.chan0 !c0, p1, r3, r4
	p1 ba Find3D_5
	mov r2, #0x00000002
	isub16.testnat.chan0 !c0, p1, r2, r1
	!p1 ba Find3D_3
Find3D_5:
	mov r16, #0x00000002
	mov r23, pclink
	ba.savelink ClearActivePowerFlags
	mov pclink, r23
	nop
Find3D_6:
	lapc

/* Finds a renderable scene: KickRender. */
CheckRenderRunlist:
	imae.u r2, r1.low, #1.low, #26, u32
	or i.l, r2, #0x00000000
	or r3, sa[il + #0], #0x00000000
CheckRenderRunlist_1:
	and.testtaz.chan0 !c0, p1, r3, r3
	!p1 ba CheckRenderRunlist_2
	lapc
CheckRenderRunlist_2:
	ldad.bpcache r2, [r3,+#0], drc0
	ldad.bpcache r4, [r3,+#20], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p2, r2, #16
	p2 ba CheckRenderRunlist_4
	ldad.bpcache r5, [sa0,+#52], drc0
	wdf drc0
	xor.testtanz.chan0 !c0, p2, r5, r3
	p2 ba CheckRenderRunlist_3
	or r5, sa9, #0x00000000
	mov r6, #0x00000800
	and.testtaz.chan0 !c0, p2, r5, r6
	p2 ba CheckRenderRunlist_3
	ba CheckRenderRunlist_4
	nop
CheckRenderRunlist_3:
	ba CheckRenderRunlist_10
CheckRenderRunlist_4:
	ldad.bpcache r14, [r4,+#68], drc0
	wdf drc0
	ldad.bpcache r2, [r14,+#31], drc0
	wdf drc0
	ldad.bpcache r2, [r2,+#0], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p1, r2, #4
	p1 ba CheckRenderRunlist_11
	ldad.bpcache r5, [r4,+#69], drc0
	mov r6, #0x00000008
	wdf drc0
	and.testtaz.chan0 !c0, p1, r5, r5
	p1 ba CheckRenderRunlist_6
	imae.u r5, r6.low, #1.low, r5, u32
	ldad.bpcache.fetch2 r6, [r5,+#1], drc0
	ldad.bpcache.fetch2 r8, [r5,#0++], drc1
	wdf drc0
	and.testtaz.chan0 !c0, p1, r7, r7
	p1 ba CheckRenderRunlist_5
	ldad.bpcache r10, [r7,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p1, r10, r6
	p1 ba CheckRenderRunlist_5
	wdf drc1
	ba CheckRenderRunlist_10
CheckRenderRunlist_5:
	wdf drc1
	and.testtaz.chan0 !c0, p1, r9, r9
	p1 ba CheckRenderRunlist_6
	ldad.bpcache r10, [r9,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p1, r10, r8
	p1 ba CheckRenderRunlist_6
	nop
	ba CheckRenderRunlist_10
CheckRenderRunlist_6:
	ldad.bpcache.fetch4 r5, [r4,+#69], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r6, r6
	p1 ba CheckRenderRunlist_7
	ldad.bpcache r9, [r6,+#0], drc0
	ldad.bpcache r10, [r8,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p1, r9, r5
	p1 xor.testtaz.chan0 !c0, p1, r10, r7
	!p1 ba CheckRenderRunlist_10
CheckRenderRunlist_7:
	ldad.bpcache r2, [r4,+#103], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r2, r2
	p1 ba CheckRenderRunlist_9
	mov r5, #0x000001A0
	imae.s r5, r5.low, #1.low, r4, u32
	ldad.bpcache.fetch4 r6, [r5,#0++], drc0
CheckRenderRunlist_8:
	wdf drc0
	ldad.bpcache r10, [r7,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p1, r10, r6
	!p1 ba CheckRenderRunlist_10
	ldad.bpcache r10, [r9,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p1, r10, r8
	!p1 ba CheckRenderRunlist_10
	isub16.testtaz.chan0 r2, p1, r2, #1
	!p1 ldad.bpcache.fetch4 r6, [r5,#0++], drc0
	!p1 ba CheckRenderRunlist_8
CheckRenderRunlist_9:
	ba CheckRenderRunlist_11
CheckRenderRunlist_10:
	ldad.bpcache r3, [r3,+#25], drc0
	wdf drc0
	ba CheckRenderRunlist_1
CheckRenderRunlist_11:
	mov pclink, r0
	or r0, r3, #0x00000000
	or r2, r1, #0x00000000
	or r1, r4, #0x00000000
	p0 ba CheckRenderRunlist_18
	ldad.bpcache r3, [r14,+#32], drc0
	ldad.bpcache r4, [r1,+#66], drc1
	wdf drc0
	and r3, r3, #0x00000040
	wdf drc1
	and r4, r4, #0x00000003
	or.testtanz.chan0 !c0, p1, r4, r3
	or r4, sa10, #0x00000000
	and.testtaz.chan0 !c0, p2, r4, #64
	p2 ba CheckRenderRunlist_12
	mov r5, #0x00004000
	and.testtaz.chan0 !c0, p2, r4, r5
	p2 ba CheckRenderRunlist_12
	or r3, sa4, #0x00000000
	wdf drc0
	isub16.testnanz.chan0 !c0, p2, r2, r3
	p2 ba CheckRenderRunlist_12
	ldad.bpcache r0, [sa0,+#52], drc0
	wdf drc0
	ldad.bpcache r1, [r0,+#20], drc0
	wdf drc0
	or r2, sa6, #0x00000000
	ldad.bpcache r14, [r1,+#68], drc0
	wdf drc0
	!p1 ba KickRender
	ba CheckRenderRunlist_13
	nop
CheckRenderRunlist_12:
	!p1 ba KickRender
CheckRenderRunlist_13:
	mov sa5, #0x00000002
	ldad.bpcache r5, [r0,+#24], drc0
	ldad.bpcache r6, [r0,+#25], drc1
	wdf drc0
	and.testtaz.chan0 !c0, p0, r5, r5
	wdf drc1
	!p0 ba CheckRenderRunlist_14
	imae.u r3, r2.low, #1.low, #26, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r6, #0x00000000
CheckRenderRunlist_14:
	!p0 stad.bpcache [r5,+#25], r6
	and.testtaz.chan0 !c0, p1, r6, r6
	!p1 ba CheckRenderRunlist_15
	imae.u r4, r2.low, #1.low, #29, u32
	or i.l, r4, #0x00000000
	or sa[il + #0], r5, #0x00000000
CheckRenderRunlist_15:
	!p1 stad.bpcache [r6,+#24], r5
	stad.bpcache [r0,+#24], #0
	stad.bpcache [r0,+#25], #0
	idf drc1, bif
	ldad.bpcache r5, [r1,+#86], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r5, r5
	stad.bpcache [r0,+#20], r5
	p0 stad.bpcache [r0,+#21], #0
	!p0 stad.bpcache [r5,+#85], #0
	stad.bpcache [r1,+#86], #0
	wdf drc1
	p0 ba CheckRenderRunlist_17
	imae.u r4, r2.low, #1.low, #29, u32
	or i.l, r4, #0x00000000
	or r5, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p1, r5, r5
	!p1 stad.bpcache [r5,+#25], r0
	!p1 ba CheckRenderRunlist_16
	imae.u r3, r2.low, #1.low, #26, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r0, #0x00000000
CheckRenderRunlist_16:
	imae.u r3, r2.low, #1.low, #29, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r0, #0x00000000
	stad.bpcache [r0,+#24], r5
	idf drc0, bif
	wdf drc0
CheckRenderRunlist_17:
	or r0, r1, #0x00000000
	ldad.bpcache r1, [r1,+#68], drc0
	mov r7, pclink
	wdf drc0
	ba.savelink UpdateRenderStatusValues
	ba.savelink ReleaseRenderResources
	mov pclink, r7
	mov r23, pclink
	ba.savelink CheckTAQueue
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink Check2DQueue
	mov pclink, r23
	ba Find3D
CheckRenderRunlist_18:
	or r16, sa10, #0x00000000
	and.testtanz.chan0 !c0, p2, r16, #14
	p2 ba CheckRenderRunlist_19
	or r16, r16, #0x00000008
	or sa10, r16, #0x00000000
	idf drc0, bif
	wdf drc0
CheckRenderRunlist_19:
	lapc

/* Starts a render: DPM state, ISP/TSP/PBE and ZLS registers, page directory. */
KickRender:
	mov sa5, #0x00000002
	ldad.bpcache r4, [r0,+#26], drc0
	wdf drc0
	stad.bpcache [sa0,+#82], r4
	and r29, r4, #0x0000FFFF
	or r29, r29, #0x34000000
	mov r30, #0x08001008
	mov.nosched r31, #0x08002000
	stad [r30,+#0], r29
	stad [r31,+#0], #0
	idf drc0, bif
	wdf drc0
	shr r29, r4, #0x00000010
	or r29, r29, #0x36000000
	mov r30, #0x08001008
	mov.nosched r31, #0x08002000
	stad [r30,+#0], r29
	stad [r31,+#0], #0
	idf drc0, bif
	wdf drc0
	or sa7, r0, #0x00000000
	stad.bpcache [sa0,+#55], r1
	or sa6, r2, #0x00000000
	idf drc0, bif
	wdf drc0
	ldad.bpcache r3, [r1,+#92], drc0
	wdf drc0
	stad.bpcache [sa0,+#67], r3
	ldad.bpcache r3, [r1,+#93], drc0
	wdf drc0
	stad.bpcache [sa0,+#69], r3
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
	ldad.bpcache r2, [sa0,+#43], drc0
	ldad.bpcache r3, [sa0,+#44], drc1
	mov r4, #0x00000000
	wdf drc0
	wdf drc1
	xor.testtaz.chan0 !c0, p0, r14, r2
	p0 ba KickRender_1
	xor.testtaz.chan0 !c0, p0, r14, r3
	p0 mov r4, #0x00000002
	p0 or r2, r3, #0x00000000
	p0 ba KickRender_1
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba KickRender_1
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 mov r4, #0x00000002
	p0 or r2, r3, #0x00000000
	p0 ba KickRender_1
	ldr r4, #396, drc0	/* EUR_CR_DPM_STATE_CONTEXT_ID */
	wdf drc0
	and r4, r4, #0x00000004
	shr r4, r4, #0x00000002
	xor r4, r4, #0x00000001
	and.testtanz.chan0 !c0, p0, r4, #1
	shl r4, r4, #0x00000001
	p0 or r2, r3, #0x00000000
KickRender_1:
	and.testtaz.chan0 !c0, p0, r4, #2
	p0 stad.bpcache [sa0,+#43], r14
	!p0 stad.bpcache [sa0,+#44], r14
	idf drc0, bif
	wdf drc0
	ldr r3, #396, drc0	/* EUR_CR_DPM_STATE_CONTEXT_ID */
	wdf drc0
	and r3, r3, ~#0x00000002
	or r3, r3, r4
	str #396, r3	/* EUR_CR_DPM_STATE_CONTEXT_ID */
	shr r4, r4, #0x00000001
	xor.testtaz.chan0 !c0, p0, r14, r2
	p0 ba KickRender_3
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba KickRender_2
	ldad.bpcache r3, [r2,+#32], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, #32
	p0 ba KickRender_2
	ldad.bpcache r3, [r2,+#0], drc0
	wdf drc0
	or r16, r3, #0x00000000
	mov r17, #0x00000001
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	or r16, r2, #0x00000000
	or r17, r4, #0x00000000
	mov r18, #0x00000200
	mov r23, pclink
	ba.savelink StoreDPMContext
	mov pclink, r23
KickRender_2:
	or r16, r0, #0x00000000
	mov r17, #0x00000001
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	or r16, r14, #0x00000000
	or r17, r4, #0x00000000
	mov r18, #0x00000200
	mov r23, pclink
	ba.savelink LoadDPMContext
	mov pclink, r23
	ba KickRender_4
KickRender_3:
	or r16, r0, #0x00000000
	mov r17, #0x00000001
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
KickRender_4:
	ldad.bpcache r3, [r14,+#0], drc0
	wdf drc0
	ldad.bpcache r2, [sa0,+#78], drc0
	ldad.bpcache r3, [r3,+#6], drc0
	ldad.bpcache r4, [sa0,+#77], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p0, r2, r3
	p0 ba KickRender_7
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba KickRender_5
	xor.testtaz.chan0 !c0, p0, r2, r4
	p0 ba KickRender_5
	mov r23, pclink
	ba.savelink Store3DPB
	mov pclink, r23
KickRender_5:
	xor.testtanz.chan0 !c0, p0, r3, r4
	p0 ba KickRender_6
	mov r16, #0x00000002
	mov r23, pclink
	ba.savelink IdleCore
	mov pclink, r23
	mov r23, pclink
	ba.savelink StoreTAPB
	mov pclink, r23
KickRender_6:
	or r16, r3, #0x00000000
	mov r23, pclink
	ba.savelink Load3DPB
	mov pclink, r23
	or r4, r16, #0x00000000
	mov r23, pclink
	ba.savelink Resume
	mov pclink, r23
	or r16, r4, #0x00000000
KickRender_7:
	ldad.bpcache r5, [r1,+#66], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r5, #8
	p0 ba KickRender_8
	or r5, sa10, #0x00000000
	or r5, r5, #0x00000030
	or sa10, r5, #0x00000000
	str #399, #3	/* EUR_CR_DPM_3D_DEALLOCATE */
	ba KickRender_9
KickRender_8:
	str #399, #0	/* EUR_CR_DPM_3D_DEALLOCATE */
	or r5, sa10, #0x00000000
	or r5, r5, #0x00000010
	or sa10, r5, #0x00000000
KickRender_9:
	mov r23, pclink
	ba.savelink USEClkGateOn
	mov pclink, r23
	mov r2, #0x00000000
	imae.s r2, r2.low, #1.low, r1, u32
	mov r3, #0x00002130
	or r4, sa38, #0x00000000
KickRender_10:
	ldad.bpcache r5, [r2,#1++], drc0
	wdf drc0
	str r3, r5
	isub16.testtaz.chan0 r4, p0, r4, #1
	p0 ba KickRender_11
	mov r5, #0x00001000
	imae.u r3, r5.low, #1.low, r3, u32
	ba KickRender_10
KickRender_11:
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
KickRender_12:
	isub16.testtaz.chan0 r4, p2, r4, #1
	p2 ba KickRender_13
	ldad.bpcache r5, [r2,#1++], drc0
	wdf drc0
	str r3, r5
	mov r5, #0x00000001
	imae.u r3, r5.low, #1.low, r3, u32
	ba KickRender_12
KickRender_13:
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
KickRender_14:
	ldad.bpcache r5, [r2,#1++], drc0
	wdf drc0
	str r3, r5
	isub16.testtaz.chan0 r4, p0, r4, #1
	p0 ba KickRender_15
	mov r5, #0x00001000
	imae.u r3, r5.low, #1.low, r3, u32
	ba KickRender_14
KickRender_15:
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
	p0 ba KickRender_16
	and r12, r3, #0x07000000
	and r13, r3, #0x000F0000
	shr r12, r12, #0x00000003
	shr r13, r13, #0x00000004
	and r3, r3, ~#0x00E0F000
	or r12, r12, r13
	or r3, r3, r12
	or r4, r5, #0x00000000
	or r6, r7, #0x00000000
KickRender_16:
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
KickRender_17:
	ima32.s1.s r4, r4, #1, #1.neg
	imae.u r5, r4.low, #1.low, #2, u32
	shl r5, r5, #0x0000000C
	or r5, r5, #0x000002BA
	str r5, r3
	and.testtanz.chan0 !c0, p0, r4, r4
	p0 ba KickRender_17
	ldad.bpcache r5, [r1,+#66], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r5, #16
	p0 ba KickRender_18
	ldr r4, #288, drc0	/* EUR_CR_ISP_ZLSCTL */
	wdf drc0
	or r4, r4, #0x00000001
	str #288, r4	/* EUR_CR_ISP_ZLSCTL */
KickRender_18:
	ldr r4, #669, drc0	/* EUR_CR_DMS_CTRL */
	ldad.bpcache r5, [r1,+#91], drc1
	wdf drc0
	and r4, r4, ~#0x03C00000
	wdf drc1
	or r4, r4, r5
	str #669, r4	/* EUR_CR_DMS_CTRL */
	str #406, #0	/* EUR_CR_DPM_PARTIAL_RENDER */
	mov r16, #0x00000004
	mov r17, #0x00000040
	mov r23, pclink
	ba.savelink InvalidateSLC
	mov pclink, r23
	lock #7
	str #516, #1	/* EUR_CR_DCU_ICTRL */
KickRender_19:
	ldr r3, #70, drc0	/* EUR_CR_EVENT_STATUS2 */
	wdf drc0
	shl.testpat.chan0 !c0, p0, r3, #24
	p0 ba KickRender_19
	mov r3, #0x00000080
	str #69, r3	/* EUR_CR_EVENT_HOST_CLEAR2 */
	str #514, #1	/* EUR_CR_TCU_ICTRL */
	mov r4, #0x04000000
KickRender_20:
	ldr r3, #75, drc0	/* EUR_CR_EVENT_STATUS */
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r4
	p0 ba KickRender_20
	mov r3, #0x04000000
	str #77, r3	/* EUR_CR_EVENT_HOST_CLEAR */
	ldr r3, #8927, drc0	/* register 0x8B7C */
	wdf drc0
	release #7
	str #271, #1	/* EUR_CR_TSP_PARAMETER_CACHE */
	and.testtanz.chan0 !c0, p0, r22, #64
	p0 ba KickRender_21
	str #642, #1	/* EUR_CR_USE_CACHE */
	or r22, r22, #0x00000040
KickRender_21:
	str #693, #1	/* EUR_CR_PDS_INV1 */
	str #696, #1	/* EUR_CR_PDS_INV_CSC */
KickRender_22:
	ldr r2, #78, drc0	/* EUR_CR_PDS_CACHE_STATUS */
	wdf drc0
	and r2, r2, #0x00000005
	xor.testtanz.chan0 !c0, p0, r2, #5
	p0 ba KickRender_22
	str #80, #5	/* EUR_CR_PDS_CACHE_HOST_CLEAR */
	or r3, sa6, #0x00000000
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 br KickRender_25
	ldad.bpcache r3, [r1,+#66], drc0
	mov r6, #0x00003000
	wdf drc0
	and.testtanz.chan0 !c0, p0, r3, r6
	p0 ba KickRender_25
	ldad.bpcache r3, [r14,+#5], drc0
	wdf drc0
	mov r6, #0x00001147
	str r6, r3
	ldad.bpcache r3, [r1,+#78], drc1
	ldad.bpcache r6, [r14,+#2], drc1
	wdf drc1
	and.testtaz.chan0 !c0, p0, r6, r6
	p0 ba KickRender_25
	ldab.bpcache r4, [r14,+#12], drc1
	wdf drc1
	or r4, r4, #0x14000000
	mov r30, #0x08001008
	mov.nosched r31, #0x08002000
	stad [r30,+#0], r4
	stad [r31,+#0], #0
	idf drc0, bif
	wdf drc0
	ldad.bpcache r2, [r14,+#4], drc0
	mov r4, #0x0000000C
	imae.u r4, r4.low, r3.low, #0, u32
	shl r4, r4, #0x00000004
	mov r5, #0x00000000
KickRender_23:
	imae.u r7, r5.low, #4.low, #44, u32
	imae.u r7, r7.low, #1.low, r1, u32
	ldad.bpcache r7, [r7,+#0], drc0
	wdf drc0
	imae.u r8, r4.low, r2.low, r7, u32
	xor.testtaz.chan0 !c0, p0, r5, #0
	p0 str #258, r8	/* EUR_CR_ISP_RGN_BASE */
	p0 ba KickRender_24
	mov r7, #0x00001143
	imae.u r7, r5.low, #1.low, r7, u32
	str r7, r8
KickRender_24:
	or r7, sa38, #0x00000000
	imae.u r5, r5.low, #1.low, #1, u32
	xor.testtanz.chan0 !c0, p0, r5, r7
	p0 ba KickRender_23
	xor.testtaz.chan0 !c0, p0, r3, r6
	p0 ba KickRender_25
	ldr r3, #399, drc0	/* EUR_CR_DPM_3D_DEALLOCATE */
	wdf drc0
	and r3, r3, ~#0x00000001
	str #399, r3	/* EUR_CR_DPM_3D_DEALLOCATE */
KickRender_25:
	ldad.bpcache r5, [r1,+#66], drc0
	mov r4, #0x00000180
	wdf drc0
	and.testtanz.chan0 !c0, p1, r5, r4
	or r2, r5, #0x50000000
	mov r30, #0x08001008
	mov.nosched r31, #0x08002000
	stad [r30,+#0], r2
	stad [r31,+#0], #0
	idf drc0, bif
	wdf drc0
	ldad.bpcache r2, [r14,+#31], drc0
	wdf drc0
	ldad.bpcache r2, [r2,+#0], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, #2
	!p0 ba KickRender_26
	ldr r3, #288, drc0	/* EUR_CR_ISP_ZLSCTL */
	wdf drc0
	ba KickRender_27
KickRender_26:
	ldr r2, #288, drc0	/* EUR_CR_ISP_ZLSCTL */
	wdf drc0
	str #288, r2	/* EUR_CR_ISP_ZLSCTL */
KickRender_27:
	mov r16, #0x00000001
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	ldad.bpcache r2, [r14,+#15], drc0
	ldad.bpcache.fetch3 r3, [r1,+#86], drc0
	wdf drc0
	stad.bpcache.fetch3 [r2,#0++], r3
	ldad.bpcache r3, [r1,+#90], drc0
	wdf drc0
	and r4, r3, #0xFFFF0000
	or r4, r4, #0x00004000
	stad.bpcache [r2,+#10], r4
	and r4, r3, #0x0000FFFF
	or r4, r4, #0x40000000
	stad.bpcache [r2,+#12], r4
	idf drc0, bif
	wdf drc0
	mov r16, #0x00000002
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	stad.bpcache [sa0,+#89], #0
	ldad.bpcache r2, [sa0,+#86], drc0
	wdf drc0
	or r2, r2, #0x00000002
	stad.bpcache [sa0,+#86], r2
	stad.bpcache [sa0,+#54], r14
	or r2, sa10, #0x00000000
	or r2, r2, #0x00000001
	or sa10, r2, #0x00000000
	or r1, sa6, #0x00000000
	ldad.bpcache r4, [r0,+#24], drc0
	ldad.bpcache r5, [r0,+#25], drc1
	wdf drc0
	and.testtanz.chan0 !c0, p0, r4, r4
	wdf drc1
	and.testtanz.chan0 !c0, p1, r5, r5
	p0 stad.bpcache [r4,+#25], r5
	p1 stad.bpcache [r5,+#24], r4
	p0 ba KickRender_28
	imae.u r2, r1.low, #1.low, #26, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r5, #0x00000000
KickRender_28:
	p1 ba KickRender_29
	imae.u r3, r1.low, #1.low, #29, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r4, #0x00000000
KickRender_29:
	idf drc0, bif
	wdf drc0
	imae.u r2, r1.low, #1.low, #26, u32
	or i.l, r2, #0x00000000
	or r4, sa[il + #0], #0x00000000
	and.testtanz.chan0 !c0, p0, r4, r4
	stad.bpcache [r0,+#24], #0
	stad.bpcache [r0,+#25], r4
	p0 stad.bpcache [r4,+#24], r0
	imae.u r2, r1.low, #1.low, #26, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r0, #0x00000000
	p0 ba KickRender_30
	imae.u r3, r1.low, #1.low, #29, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r0, #0x00000000
KickRender_30:
	idf drc0, bif
	wdf drc0
	ldr r10, #458, drc0	/* EUR_CR_DPM_PAGE */
	wdf drc0
	and r10, r10, #0x0000FFFF
	shr r10, r10, #0x00000000
	str #266, #1	/* EUR_CR_ISP_START_RENDER */
	ldad.bpcache r3, [sa0,+#67], drc0
	wdf drc0
	or r3, r3, #0x11000000
	mov r30, #0x08001008
	mov.nosched r31, #0x08002000
	stad [r30,+#0], r3
	stad [r31,+#0], #0
	idf drc0, bif
	wdf drc0
	lapc

/* Finds a runnable transfer: KickTransferRender or DummyProcessTransfer. */
CheckTransferRunList:
	imae.u r2, r1.low, #1.low, #14, u32
	or i.l, r2, #0x00000000
	or r3, sa[il + #0], #0x00000000
CheckTransferRunList_1:
	and.testtanz.chan0 !c0, p1, r3, r3
	p1 ba CheckTransferRunList_2
	lapc
CheckTransferRunList_2:
	ldad.bpcache r2, [r3,+#0], drc0
	ldad.bpcache r4, [r3,+#4], drc0
	ldad.bpcache r5, [r3,+#3], drc1
	wdf drc0
	and.testtaz.chan0 !c0, p2, r2, #16
	p2 ba CheckTransferRunList_3
	nop
	ba CheckTransferRunList_16
CheckTransferRunList_3:
	ldad.bpcache r4, [r4,+#1], drc1
	wdf drc1
	imae.u r4, r4.low, #1.low, r5, u32
	ldad r2, [r4,+#41], drc0
	wdf drc0
	ldad.bpcache r8, [r4,+#42], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r8, r8
	p1 ba CheckTransferRunList_8
	mov r9, #0x000000AC
	imae.u r9, r9.low, #1.low, r4, u32
	mov r10, #0x00000018
	imae.u r10, r10.low, #1.low, r3, u32
CheckTransferRunList_4:
	ldad.bpcache.fetch2 r5, [r9,#0++], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r6, r6
	p1 ba CheckTransferRunList_5
	ldad.bpcache r7, [r6,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p1, r7, r5
	p1 ba CheckTransferRunList_5
	ba CheckTransferRunList_16
CheckTransferRunList_5:
	and.testtanz.chan0 !c0, p1, r2, #1
	p1 and.testtaz.chan0 !c0, p1, r2, #2
	!p1 ba CheckTransferRunList_6
	stad.bpcache.fetch2 [r10,#0++], r5
	idf drc0, bif
	wdf drc0
	mov r5, #0x00000008
	imae.u r10, r5.low, #1.low, r10, u32
CheckTransferRunList_6:
	ldad.bpcache.fetch2 r5, [r9,#0++], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r6, r6
	p1 ba CheckTransferRunList_7
	ldad.bpcache r7, [r6,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p1, r7, r5
	p1 ba CheckTransferRunList_7
	nop
	ba CheckTransferRunList_16
CheckTransferRunList_7:
	isub16.testtanz.chan0 r8, p1, r8, #1
	p1 ba CheckTransferRunList_4
CheckTransferRunList_8:
	ldad.bpcache r8, [r4,+#75], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r8, r8
	p1 ba CheckTransferRunList_13
	mov r9, #0x00000130
	imae.u r9, r9.low, #1.low, r4, u32
	mov r10, #0x00000098
	imae.u r10, r10.low, #1.low, r3, u32
CheckTransferRunList_9:
	ldad.bpcache.fetch2 r5, [r9,#0++], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r6, r6
	p1 ba CheckTransferRunList_10
	ldad.bpcache r7, [r6,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p1, r7, r5
	p1 ba CheckTransferRunList_10
	nop
	ba CheckTransferRunList_16
CheckTransferRunList_10:
	ldad.bpcache.fetch2 r5, [r9,#0++], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r6, r6
	p1 ba CheckTransferRunList_11
	ldad.bpcache r7, [r6,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p1, r7, r5
	p1 ba CheckTransferRunList_11
	nop
	ba CheckTransferRunList_16
CheckTransferRunList_11:
	and.testtanz.chan0 !c0, p1, r2, #1
	p1 and.testtaz.chan0 !c0, p1, r2, #2
	!p1 ba CheckTransferRunList_12
	mov r11, #0x00000008
	imae.u r10, r11.low, #1.low, r10, u32
	stad.bpcache.fetch2 [r10,#0++], r5
	idf drc0, bif
	wdf drc0
CheckTransferRunList_12:
	isub16.testtanz.chan0 r8, p1, r8, #1
	p1 ba CheckTransferRunList_9
CheckTransferRunList_13:
	ldad.fetch4 r5, [r4,+#79], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r6, r6
	p1 ba CheckTransferRunList_14
	ldad.bpcache r9, [r6,+#0], drc0
	ldad.bpcache r10, [r8,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p1, r9, r5
	p1 xor.testtaz.chan0 !c0, p1, r10, r7
	!p1 ba CheckTransferRunList_16
CheckTransferRunList_14:
	ldad.fetch4 r5, [r4,+#83], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r6, r6
	p1 ba CheckTransferRunList_15
	ldad.bpcache r9, [r6,+#0], drc0
	ldad.bpcache r10, [r8,+#0], drc0
	wdf drc0
	xor.testtaz.chan0 !c0, p1, r9, r5
	p1 xor.testtaz.chan0 !c0, p1, r10, r7
	!p1 ba CheckTransferRunList_16
	nop
CheckTransferRunList_15:
	ba CheckTransferRunList_17
CheckTransferRunList_16:
	ldad.bpcache r3, [r3,+#44], drc0
	wdf drc0
	ba CheckTransferRunList_1
CheckTransferRunList_17:
	mov r6, pclink
	mov pclink, r0
	or r0, r3, #0x00000000
	or r2, r1, #0x00000000
	or r1, r4, #0x00000000
	p0 ba CheckTransferRunList_18
	ldad.bpcache r3, [r0,+#0], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, #2
	p0 ba KickTransferRender
	and r3, r3, ~#0x00000002
	stad.bpcache [r0,+#0], r3
	ba DummyProcessTransfer
CheckTransferRunList_18:
	or r16, sa10, #0x00000000
	and.testtanz.chan0 !c0, p2, r16, #14
	p2 ba CheckTransferRunList_19
	or r16, r16, #0x00000008
	or sa10, r16, #0x00000000
	idf drc0, bif
	wdf drc0
CheckTransferRunList_19:
	lapc

/* Starts a transfer (3D blit) through the ISP. */
KickTransferRender:
	mov sa5, #0x00000002
	or sa7, r0, #0x00000000
	or sa3, r1, #0x00000000
	or sa6, r2, #0x00000000
	or r16, r0, #0x00000000
	mov r17, #0x00000002
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
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
	ldad.bpcache r4, [r0,+#42], drc0
	wdf drc0
	stad.bpcache [sa0,+#82], r4
	ldad r6, [r1,+#41], drc0
	wdf drc0
	and.testtanz.chan0 !c0, p0, r6, #2
	p0 and.testtaz.chan0 !c0, p0, r6, #1
	!p0 ba KickTransferRender_3
	mov r9, #0x000000AC
	imae.u r9, r9.low, #1.low, r1, u32
	mov r10, #0x00000018
	imae.u r10, r10.low, #1.low, r0, u32
	ldad r8, [r1,+#42], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r8, r8
	p0 ba KickTransferRender_2
KickTransferRender_1:
	ldad.bpcache.fetch2 r2, [r10,#0++], drc0
	wdf drc0
	mov r11, #0x00000008
	imae.u r10, r11.low, #1.low, r10, u32
	stad.bpcache.fetch2 [r9,#0++], r2
	idf drc0, bif
	wdf drc0
	mov r11, #0x00000008
	imae.u r9, r11.low, #1.low, r9, u32
	isub16.testtanz.chan0 r8, p0, r8, #1
	p0 ba KickTransferRender_1
KickTransferRender_2:
	mov r9, #0x00000130
	imae.u r9, r9.low, #1.low, r1, u32
	mov r10, #0x00000098
	imae.u r10, r10.low, #1.low, r0, u32
	ldad r8, [r1,+#75], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r8, r8
	p0 ba KickTransferRender_3
	mov r11, #0x00000008
	imae.u r10, r11.low, #1.low, r10, u32
	ldad.bpcache.fetch2 r4, [r10,#0++], drc1
	wdf drc1
	imae.u r9, r11.low, #1.low, r9, u32
	stad.bpcache.fetch2 [r9,#0++], r4
	idf drc1, bif
	wdf drc1
	isub16.testtanz.chan0 r8, p0, r8, #1
	p0 ba KickTransferRender_1
KickTransferRender_3:
	and.testtaz.chan0 !c0, p0, r6, #32
	p0 ba KickTransferRender_4
	ba DummyProcessTransfer
	nop
	lapc
KickTransferRender_4:
	ldad r5, [r1,+#99], drc1
	ldr r4, #669, drc0	/* EUR_CR_DMS_CTRL */
	wdf drc0
	ldad r2, [r1,+#101], drc0
	and r4, r4, ~#0x03C00000
	wdf drc1
	or r4, r4, r5
	str #669, r4	/* EUR_CR_DMS_CTRL */
	wdf drc0
	and.testtaz.chan0 !c0, p1, r2, r2
	p1 ba KickTransferRender_6
	mov r16, #0x00000001
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	mov r3, #0x00000198
	imae.s r3, r3.low, #1.low, r1, u32
	mov r4, #0x00000008
	imae.u r2, r2.low, r4.low, r3, u32
	ldad.fetch2 r4, [r3,#0++], drc0
KickTransferRender_5:
	xor.testtanz.chan0 !c0, p0, r2, r3
	wdf drc0
	stad.bpcache [r4,+#0], r5
	p0 ldad.fetch2 r4, [r3,#0++], drc0
	p0 ba KickTransferRender_5
	idf drc1, bif
	wdf drc1
	mov r16, #0x00000002
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
KickTransferRender_6:
	mov r2, #0x00000000
	imae.u r2, r2.low, #1.low, r1, u32
	ldad.fetch6 r3, [r2,#0++], drc0
	ldad.fetch6 r9, [r2,#0++], drc1
	str #399, #0	/* EUR_CR_DPM_3D_DEALLOCATE */
	str #288, #0	/* EUR_CR_ISP_ZLSCTL */
	wdf drc0
	str #305, r3	/* EUR_CR_ISP_BGOBJTAG */
	str #303, r4	/* EUR_CR_ISP_BGOBJ */
	str #302, r5	/* EUR_CR_ISP_BGOBJDEPTH */
	str #257, r6	/* EUR_CR_ISP_RENDER */
	str #258, r7	/* EUR_CR_ISP_RGN_BASE */
	str #261, r8	/* EUR_CR_ISP_IPFMISC */
	ldad.fetch4 r3, [r2,#0++], drc0
	wdf drc1
	str #811, r9	/* EUR_CR_BIF_3D_REQ_BASE */
	str #267, r10	/* EUR_CR_3D_AA_MODE */
	str #306, r11	/* EUR_CR_ISP_MULTISAMPLECTL */
	str #663, r12	/* EUR_CR_EVENT_PIXEL_PDS_EXEC */
	str #664, r13	/* EUR_CR_EVENT_PIXEL_PDS_DATA */
	str #665, r14	/* EUR_CR_EVENT_PIXEL_PDS_INFO */
	ldad r10, [r2,#1++], drc1
	ldad r13, [r1,+#97], drc1
	wdf drc0
	str #259, r3	/* EUR_CR_ISP_RENDBOX1 */
	str #260, r4	/* EUR_CR_ISP_RENDBOX2 */
	str #256, r5	/* EUR_CR_PIXELBE */
	str #275, r6	/* EUR_CR_4Kx4K_RENDER */
	wdf drc1
	str #82, r10	/* EUR_CR_DOUBLE_PIXEL_PARTITIONS */
	and.testtaz.chan0 !c0, p0, r13, r13
	p0 ba KickTransferRender_7
	ldad.fetch6 r3, [r2,#0++], drc0
	ldad.fetch6 r9, [r2,#0++], drc1
	wdf drc0
	str #519, r3	/* EUR_CR_TPU_YUV00 */
	str #520, r4	/* EUR_CR_TPU_YUV01 */
	str #521, r5	/* EUR_CR_TPU_YUV02 */
	str #522, r6	/* EUR_CR_TPU_YUV03 */
	str #523, r7	/* EUR_CR_TPU_YUV04 */
	str #524, r8	/* EUR_CR_TPU_YUV05 */
	wdf drc1
	str #525, r9	/* EUR_CR_TPU_YUV10 */
	str #526, r10	/* EUR_CR_TPU_YUV11 */
	str #527, r11	/* EUR_CR_TPU_YUV12 */
	str #528, r12	/* EUR_CR_TPU_YUV13 */
	str #529, r13	/* EUR_CR_TPU_YUV14 */
	str #530, r14	/* EUR_CR_TPU_YUV15 */
KickTransferRender_7:
	ldad r14, [r1,+#98], drc1
	wdf drc1
	and.testtaz.chan0 !c0, p0, r14, r14
	p0 ba KickTransferRender_8
	ldad.fetch8 r3, [r2,#0++], drc0
	ldad.fetch2 r11, [r2,#0++], drc1
	wdf drc0
	str #679, r3	/* EUR_CR_USE_FILTER_TABLE */
	str #673, r4	/* EUR_CR_USE_FILTER0_LEFT */
	str #674, r5	/* EUR_CR_USE_FILTER0_RIGHT */
	str #675, r6	/* EUR_CR_USE_FILTER0_EXTRA */
	str #676, r7	/* EUR_CR_USE_FILTER1_LEFT */
	str #677, r8	/* EUR_CR_USE_FILTER1_RIGHT */
	str #678, r9	/* EUR_CR_USE_FILTER1_EXTRA */
	str #712, r10	/* EUR_CR_USE_FILTER2_LEFT */
	wdf drc1
	str #713, r11	/* EUR_CR_USE_FILTER2_RIGHT */
	str #714, r12	/* EUR_CR_USE_FILTER2_EXTRA */
KickTransferRender_8:
	mov r16, #0x00000004
	mov r17, #0x00000040
	mov r23, pclink
	ba.savelink InvalidateSLC
	mov pclink, r23
	str #693, #1	/* EUR_CR_PDS_INV1 */
	str #696, #1	/* EUR_CR_PDS_INV_CSC */
	str #271, #1	/* EUR_CR_TSP_PARAMETER_CACHE */
	mov r23, pclink
	ba.savelink USEClkGateOn
	mov pclink, r23
	lock #7
	str #516, #1	/* EUR_CR_DCU_ICTRL */
	mov r3, #0x00000080
KickTransferRender_9:
	ldr r2, #70, drc0	/* EUR_CR_EVENT_STATUS2 */
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r3
	p0 ba KickTransferRender_9
	mov r2, #0x00000080
	str #69, r2	/* EUR_CR_EVENT_HOST_CLEAR2 */
	str #514, #1	/* EUR_CR_TCU_ICTRL */
KickTransferRender_10:
	ldr r2, #75, drc0	/* EUR_CR_EVENT_STATUS */
	wdf drc0
	shl.testpat.chan0 !c0, p0, r2, #5
	p0 ba KickTransferRender_10
	mov r2, #0x04000000
	str #77, r2	/* EUR_CR_EVENT_HOST_CLEAR */
	ldr r2, #8927, drc0	/* register 0x8B7C */
	wdf drc0
	release #7
	and.testtanz.chan0 !c0, p0, r22, #64
	p0 ba KickTransferRender_11
	str #642, #1	/* EUR_CR_USE_CACHE */
	or r22, r22, #0x00000040
KickTransferRender_11:
	ldr r2, #78, drc0	/* EUR_CR_PDS_CACHE_STATUS */
	wdf drc0
	and r2, r2, #0x00000005
	xor.testtanz.chan0 !c0, p0, r2, #5
	p0 ba KickTransferRender_11
	str #80, #5	/* EUR_CR_PDS_CACHE_HOST_CLEAR */
	ldad r2, [r1,+#100], drc0
	wdf drc0
	stad.bpcache [sa0,+#89], #0
	ldad.bpcache r2, [sa0,+#86], drc0
	wdf drc0
	or r2, r2, #0x00000002
	stad.bpcache [sa0,+#86], r2
	or r2, sa10, #0x00000000
	or r2, r2, #0x00000005
	or sa10, r2, #0x00000000
	or r1, sa6, #0x00000000
	ldad.bpcache r3, [r0,+#43], drc0
	wdf drc0
	ldad.bpcache r4, [r0,+#44], drc0
	and.testtanz.chan0 !c0, p0, r3, r3
	wdf drc0
	and.testtanz.chan0 !c0, p1, r4, r4
	p0 stad.bpcache [r3,+#44], r4
	p1 stad.bpcache [r4,+#43], r3
	p0 ba KickTransferRender_12
	imae.u r2, r1.low, #1.low, #14, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r4, #0x00000000
KickTransferRender_12:
	p1 ba KickTransferRender_13
	imae.u r2, r1.low, #1.low, #17, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r3, #0x00000000
KickTransferRender_13:
	idf drc0, bif
	wdf drc0
	imae.u r2, r1.low, #1.low, #14, u32
	or i.l, r2, #0x00000000
	or r3, sa[il + #0], #0x00000000
	and.testtanz.chan0 !c0, p0, r3, r3
	stad.bpcache [r0,+#43], #0
	stad.bpcache [r0,+#44], r3
	p0 stad.bpcache [r3,+#43], r0
	imae.u r2, r1.low, #1.low, #14, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r0, #0x00000000
	p0 ba KickTransferRender_14
	imae.u r2, r1.low, #1.low, #17, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r0, #0x00000000
KickTransferRender_14:
	idf drc0, bif
	wdf drc0
	str #266, #1	/* EUR_CR_ISP_START_RENDER */
	lapc
KickTransferRender_15:
	shl.testnat.chan0 !c0, p2, r0, #25
	and r1, r0, ~#0x000030CF
	or sa10, r1, #0x00000000
	p2 ba.savelink TARestartWithoutRender
	or r1, sa3, #0x00000000
	ldad.fetch2 r2, [r1,+#79], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba KickTransferRender_16
	mov r4, #0x00000001
	imae.u r2, r4.low, #1.low, r2, u32
	stad.bpcache [r3,+#0], r2
	idf drc0, bif
KickTransferRender_16:
	ldad.fetch2 r2, [r1,+#83], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba KickTransferRender_17
	mov r4, #0x00000001
	imae.u r2, r4.low, #1.low, r2, u32
	stad.bpcache [r3,+#0], r2
	idf drc0, bif
KickTransferRender_17:
	ldad r0, [r1,+#88], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r0, r0
	p0 ba KickTransferRender_19
	mov r2, #0x00000164
	imae.s r3, r2.low, #1.low, r1, u32
	imae.u r2, r0.low, #8.low, r3, u32
	ldad.fetch2 r4, [r3,#0++], drc0
KickTransferRender_18:
	xor.testtanz.chan0 !c0, p0, r2, r3
	wdf drc0
	stad.bpcache [r4,+#0], r5
	p0 ldad.fetch2 r4, [r3,#0++], drc0
	p0 ba KickTransferRender_18
	nop
KickTransferRender_19:
	ba.savelink KickTransferRender_25
	or r0, sa7, #0x00000000
	ba.savelink KickTransferRender_24
	and.testtanz.chan0 !c0, p1, r2, r2
	or r1, sa6, #0x00000000
	ldad.bpcache r4, [r0,+#44], drc0
	wdf drc0
	imae.u r2, r1.low, #1.low, #14, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r4, #0x00000000
	and.testtaz.chan0 !c0, p0, r4, r4
	!p0 ba KickTransferRender_20
	imae.u r3, r1.low, #1.low, #17, u32
	or i.l, r3, #0x00000000
	mov sa[il + #0], #0x00000000
KickTransferRender_20:
	!p0 stad.bpcache [r4,+#43], #0
	stad.bpcache [r0,+#44], #0
	idf drc0, bif
	wdf drc0
	!p1 ba KickTransferRender_22
	imae.u r3, r1.low, #1.low, #17, u32
	or i.l, r3, #0x00000000
	or r4, sa[il + #0], #0x00000000
	!p0 stad.bpcache [r4,+#44], r0
	!p0 ba KickTransferRender_21
	imae.u r2, r1.low, #1.low, #14, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r0, #0x00000000
KickTransferRender_21:
	imae.u r3, r1.low, #1.low, #17, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r0, #0x00000000
	stad.bpcache [r0,+#43], r4
	idf drc0, bif
	wdf drc0
KickTransferRender_22:
	or r0, sa9, #0x00000000
	shl.testpat.chan0 !c0, p0, r0, #11
	p0 ba KickTransferRender_23
	mov r16, #0x00000080
	mov r23, pclink
	ba.savelink EmitLoopBack
	mov pclink, r23
	nop
KickTransferRender_23:
	mov r23, pclink
	ba.savelink CheckTAQueue
	mov pclink, r23
	nop
	mov r23, pclink
	ba.savelink Check2DQueue
	mov pclink, r23
	nop
	ba.savelink Find3D
	mov pclink, r15
	lapc
KickTransferRender_24:
	ldad.bpcache r2, [r0,+#4], drc0
	wdf drc0
	ldad.bpcache r3, [r2,+#1], drc0
	ldad r4, [r1,+#40], drc1
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
KickTransferRender_25:
	ldad r2, [r1,+#41], drc1
	mov sa12, #0x00000000
	mov sa13, #0x00000007
	wdf drc1
	and.testtanz.chan0 !c0, p0, r2, #1
	p0 ba KickTransferRender_31
	ldad.bpcache r7, [r1,+#42], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r7, r7
	p1 ba KickTransferRender_28
	mov r8, #0x000000AC
	imae.u r8, r8.low, #1.low, r1, u32
KickTransferRender_26:
	ldad.bpcache.fetch2 r2, [r8,#0++], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba KickTransferRender_27
	mov r6, #0x00000001
	imae.u r2, r6.low, #1.low, r2, u32
	stad.bpcache [r3,+#0], r2
KickTransferRender_27:
	isub16.testtanz.chan0 r7, p0, r7, #1
	p0 mov r9, #0x00000008
	p0 imae.u r8, r9.low, #1.low, r8, u32
	p0 ba KickTransferRender_26
KickTransferRender_28:
	ldad.bpcache r7, [r1,+#75], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p1, r7, r7
	p1 ba KickTransferRender_31
	mov r8, #0x00000130
	imae.u r8, r8.low, #1.low, r1, u32
KickTransferRender_29:
	ldad.bpcache.fetch2 r4, [r8,+#1], drc1
	wdf drc1
	and.testtaz.chan0 !c0, p0, r5, r5
	p0 ba KickTransferRender_30
	mov r6, #0x00000001
	imae.u r4, r6.low, #1.low, r4, u32
	stad.bpcache [r5,+#0], r4
KickTransferRender_30:
	isub16.testtanz.chan0 r7, p0, r7, #1
	p0 mov r9, #0x00000010
	p0 imae.u r8, r9.low, #1.low, r8, u32
	p0 ba KickTransferRender_29
KickTransferRender_31:
	or r2, sa9, #0x00000000
	shl.testpat.chan0 !c0, p0, r2, #25
	p0 ba KickTransferRender_32
	and r2, r2, ~#0x00000040
	or sa9, r2, #0x00000000
	mov r4, pclink
	ba.savelink TARestartWithoutRender
	mov pclink, r4
	idf drc0, bif
	wdf drc0
KickTransferRender_32:
	mov r3, #0x00004000
	str #75, r3	/* EUR_CR_EVENT_STATUS */
	lapc

/* Completes a transfer without running it. */
DummyProcessTransfer:
	mov sa5, #0x00000002
	or r16, r0, #0x00000000
	mov r17, #0x00000002
	mov r23, pclink
	ba.savelink SetupDirListBase
	mov pclink, r23
	mov r23, pclink
	ba.savelink SetupRequestor
	mov pclink, r23
	ldad r2, [r1,+#101], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba DummyProcessTransfer_2
	mov r16, #0x00000001
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
	mov r3, #0x00000198
	imae.s r3, r3.low, #1.low, r1, u32
	mov r4, #0x00000008
	imae.u r2, r2.low, r4.low, r3, u32
	ldad.fetch2 r4, [r3,#0++], drc0
DummyProcessTransfer_1:
	xor.testtanz.chan0 !c0, p0, r2, r3
	wdf drc0
	stad.bpcache [r4,+#0], r5
	p0 ldad.fetch2 r4, [r3,#0++], drc0
	p0 ba DummyProcessTransfer_1
	idf drc0, bif
	wdf drc0
	mov r16, #0x00000002
	mov r23, pclink
	ba.savelink SwitchEDMBIFBank
	mov pclink, r23
DummyProcessTransfer_2:
	ldad.fetch2 r2, [r1,+#79], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba DummyProcessTransfer_3
	mov r4, #0x00000001
	imae.u r2, r4.low, #1.low, r2, u32
	stad.bpcache [r3,+#0], r2
	idf drc0, bif
DummyProcessTransfer_3:
	ldad.fetch2 r2, [r1,+#83], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r3, r3
	p0 ba DummyProcessTransfer_4
	mov r4, #0x00000001
	imae.u r2, r4.low, #1.low, r2, u32
	stad.bpcache [r3,+#0], r2
	idf drc0, bif
DummyProcessTransfer_4:
	ldad r2, [r1,+#88], drc0
	wdf drc0
	and.testtaz.chan0 !c0, p0, r2, r2
	p0 ba DummyProcessTransfer_6
	mov r3, #0x00000164
	imae.s r3, r3.low, #1.low, r1, u32
	imae.u r4, r2.low, #8.low, r3, u32
	ldad.fetch2 r5, [r3,#0++], drc0
DummyProcessTransfer_5:
	xor.testtanz.chan0 !c0, p0, r3, r4
	wdf drc0
	stad.bpcache [r5,+#0], r6
	p0 ldad.fetch2 r5, [r3,#0++], drc0
	p0 ba DummyProcessTransfer_5
DummyProcessTransfer_6:
	mov r12, pclink
	ba.savelink KickTransferRender_25
	ba.savelink KickTransferRender_24
	mov pclink, r12
	and.testtanz.chan0 !c0, p1, r2, r2
	ldad.bpcache r1, [r0,+#1], drc0
	wdf drc0
	ldad.bpcache r4, [r0,+#43], drc0
	ldad.bpcache r5, [r0,+#44], drc1
	wdf drc0
	and.testtaz.chan0 !c0, p0, r4, r4
	wdf drc1
	!p0 ba DummyProcessTransfer_7
	imae.u r2, r1.low, #1.low, #14, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r5, #0x00000000
DummyProcessTransfer_7:
	!p0 stad.bpcache [r4,+#44], r5
	and.testtaz.chan0 !c0, p0, r5, r5
	!p0 ba DummyProcessTransfer_8
	imae.u r3, r1.low, #1.low, #17, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r4, #0x00000000
DummyProcessTransfer_8:
	!p0 stad.bpcache [r5,+#43], r4
	stad.bpcache [r0,+#43], #0
	stad.bpcache [r0,+#44], #0
	idf drc1, bif
	wdf drc1
	!p1 ba DummyProcessTransfer_10
	imae.u r3, r1.low, #1.low, #17, u32
	or i.l, r3, #0x00000000
	or r4, sa[il + #0], #0x00000000
	and.testtaz.chan0 !c0, p0, r4, r4
	!p0 stad.bpcache [r4,+#44], r0
	!p0 ba DummyProcessTransfer_9
	imae.u r2, r1.low, #1.low, #14, u32
	or i.l, r2, #0x00000000
	or sa[il + #0], r0, #0x00000000
DummyProcessTransfer_9:
	imae.u r3, r1.low, #1.low, #17, u32
	or i.l, r3, #0x00000000
	or sa[il + #0], r0, #0x00000000
	stad.bpcache [r0,+#43], r4
DummyProcessTransfer_10:
	idf drc0, bif
	wdf drc0
	ba Find3D

/* Restarts the TA after out-of-memory without a render. */
TARestartWithoutRender:
	ldr r2, #456, drc0	/* EUR_CR_DPM_ABORT_STATUS_MTILE */
	wdf drc0
	and r2, r2, #0x000000E0
	shr r2, r2, #0x00000005
	imae.u r3, r2.low, #1.low, #2, u32
	shl r3, r3, #0x0000000C
	or r3, r3, #0x000001A7
	str r3, #1
	str #423, #1	/* EUR_CR_DPM_OUTOFMEM */
	or r5, sa10, #0x00000000
	and r5, r5, ~#0x00004000
	or sa10, r5, #0x00000000
	lapc
