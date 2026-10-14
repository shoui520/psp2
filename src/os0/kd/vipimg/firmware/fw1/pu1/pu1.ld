/*
 * PU1's program: its DRAM part from 0x820000, its local RAM code and data, and
 * task 0's overlays, whose images follow it in DRAM.
 *
 * Local RAM: interrupt vectors to 0x90, the runtime's code from 0x90, task 0's
 * code at VIP_TASK_CODE (0x230), the macroblock loop and the motion saving after
 * it; the task stack from 0x2000, task 0's data from 0x2400 (the slice buffers
 * with g_parsed over them, VIP_PARSED_OVERLAP in), then the runtime's data, up to
 * 0x3000. Task 0's switch saves 0x2000 up to the runtime's data.
 */
MEMORY
{
	image (rwx) : ORIGIN = 0x820000, LENGTH = 0x38000
	code (rwx) : ORIGIN = 0x200090, LENGTH = 0x230 - 0x90
	overlay (rwx) : ORIGIN = 0x200230, LENGTH = 0x1000 - 0x230
	data (rw) : ORIGIN = 0x202400, LENGTH = 0xc00
}

ENTRY(vipStart)
pu1Start = vipStart;

SECTIONS
{
	.text : { *(.text .text.*) . = ALIGN(8); } > image
	.rodata : { *(.rodata .rodata.*) . = ALIGN(8); } > image
	.data : { *(.data .data.*) . = ALIGN(8); } > image

	.lram.text : { *(.lram.text) . = ALIGN(8); } > code AT> image
	.task0.text : { *(.task0.text) . = ALIGN(8); } > overlay AT> image
}

LOOPS_START = ADDR(.task0.text) + SIZEOF(.task0.text);
DMA_BASE = 0;
INCLUDE fw1/pu1/loops.ld
ASSERT(__loops_end <= 0x201000, "PU1's code overlays overrun local RAM")

SECTIONS
{

	.task0.data : { *(.task0.data) . = ALIGN(8); } > data AT> image
	.task0.bss (NOLOAD) : { *(.task0.bss) } > data
	.task0.slice (NOLOAD) : ALIGN(8)
	{
		*(.task0.bss.colocated)
		*(.task0.bss.motion)
	} > data
	.task0.parsed ADDR(.task0.slice) + 0x200 /* VIP_PARSED_OVERLAP */ (NOLOAD) :
	{
		*(.task0.bss.parsed)
	}
	ASSERT(ADDR(.task0.parsed) + SIZEOF(.task0.parsed) <=
		ADDR(.task0.slice) + SIZEOF(.task0.slice), "g_parsed past the slice buffers")
	.lram.data : ALIGN(8) { *(.lram.data) } > data AT> image
	__task0_save_size = ADDR(.lram.data) - 0x202000;
	ASSERT(__task0_save_size <= 0xf00, "task 0's data past TASK_DECODER_DATA_SIZE")
	__pu1_lram_image_end = LOADADDR(.lram.data) + SIZEOF(.lram.data);

	.bss (NOLOAD) : ALIGN(8) { *(.bss .bss.* COMMON) } > image
	__pu1_lram_image_gap = ADDR(.bss) - __pu1_lram_image_end;
	__pu1_bss_start = ADDR(.bss);
	__pu1_bss_size = SIZEOF(.bss);

	__lram_text_load = LOADADDR(.lram.text);
	__lram_text_start = ADDR(.lram.text);
	__lram_text_size = SIZEOF(.lram.text);
	__lram_data_load = LOADADDR(.lram.data);
	__lram_data_start = ADDR(.lram.data);
	__lram_data_size = SIZEOF(.lram.data);
	__task0_text_load = LOADADDR(.task0.text);
	__task0_text_size = SIZEOF(.task0.text);
	__task0_data_load = LOADADDR(.task0.data);
	__task0_data_start = ADDR(.task0.data);
	__task0_data_size = SIZEOF(.task0.data);

	/DISCARD/ : { *(.comment) }
}
