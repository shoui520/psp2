ENTRY(moduleInfo)
PHDRS { metadata PT_LOAD FLAGS(5); }
SECTIONS {
	. = 0x81000000;
	.rodata : { KEEP(*(.rodata.sceModuleData)) } :metadata
	/DISCARD/ : { *(.text*) *(.data*) *(.bss*) *(.ARM.attributes) *(.comment) }
}
