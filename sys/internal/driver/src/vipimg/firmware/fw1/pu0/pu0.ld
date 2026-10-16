/*
 * PU0's program: its DRAM part from 0x801000, its local RAM code and data, and
 * task 1's overlays, whose images follow it in DRAM.
 *
 * Local RAM: interrupt vectors to 0x90, the runtime's code from 0x90, task 1's
 * code at VIP_TASK_CODE (0x600); the task stacks from 0x2000, task 1's data
 * from 0x2400, the runtime's data from 0x2c50 (TASK_DEMUX_DATA_SIZE).
 */
MEMORY
{
	image (rwx) : ORIGIN = 0x801000, LENGTH = 0x1f000
	code (rwx) : ORIGIN = 0x200090, LENGTH = 0x600 - 0x90
	overlay (rwx) : ORIGIN = 0x200600, LENGTH = 0x1000 - 0x600
	data (rw) : ORIGIN = 0x202400, LENGTH = 0x850
	runtime (rw) : ORIGIN = 0x202c50, LENGTH = 0x3b0
}

ENTRY(vipStart)
pu0Start = vipStart;

SECTIONS
{
	.text : { *(.text .text.*) . = ALIGN(8); } > image
	.rodata : { *(.rodata .rodata.*) . = ALIGN(8); } > image
	.data : { *(.data .data.*) . = ALIGN(8); } > image

	.lram.text : { *(.lram.text) . = ALIGN(8); } > code AT> image
	.task1.text : { *(.task1.text) . = ALIGN(8); } > overlay AT> image
	.task1.data : { *(.task1.data) . = ALIGN(8); } > data AT> image
	.task1.bss (NOLOAD) : { *(.task1.bss) } > data
	.lram.data : { *(.lram.data) . = ALIGN(8); } > runtime AT> image
	__pu0_lram_image_end = LOADADDR(.lram.data) + SIZEOF(.lram.data);

	.bss (NOLOAD) : ALIGN(8) { *(.bss .bss.* COMMON) } > image
	__pu0_lram_image_gap = ADDR(.bss) - __pu0_lram_image_end;
	__pu0_bss_start = ADDR(.bss);
	__pu0_bss_size = SIZEOF(.bss);

	__lram_text_load = LOADADDR(.lram.text);
	__lram_text_start = ADDR(.lram.text);
	__lram_text_size = SIZEOF(.lram.text);
	__lram_data_load = LOADADDR(.lram.data);
	__lram_data_start = ADDR(.lram.data);
	__lram_data_size = SIZEOF(.lram.data);
	__task1_text_load = LOADADDR(.task1.text);
	__task1_text_size = SIZEOF(.task1.text);
	__task1_data_load = LOADADDR(.task1.data);
	__task1_data_start = ADDR(.task1.data);
	__task1_data_size = SIZEOF(.task1.data);

	/DISCARD/ : { *(.comment) }
}
