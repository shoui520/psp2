/*
 * Sony's module layout (mkprx.py tables generates the SCE sections): text, the
 * import stubs and the SCE tables, read-only data and the unwind tables in one
 * segment; data and bss in the next. The SCE tables are kept when a module
 * links with --gc-sections (only what its exports and start reach).
 */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(module_info)

PHDRS
{
	text PT_LOAD FLAGS(5);
	data PT_LOAD FLAGS(6);
}

SECTIONS
{
	. = 0x81000000;
	.text : { *(.text .text.*) *(.glue_7) *(.glue_7t) *(.vfp11_veneer) *(.v4_bx) } :text
	.sceStub.text : {
		__sce_stub_top = .;
		*(.sceStub.text)
	}
	__sce_text_end = .;
	.sceModuleInfo.rodata : { KEEP(*(.sceModuleInfo.rodata)) }
	.sceLib.ent : {
		__sce_libent_top = .;
		KEEP(*(.sceLib.ent))
		__sce_libent_end = .;
	}
	.sceLib.stub : {
		__sce_libstub_top = .;
		KEEP(*(.sceLib.stub))
		__sce_libstub_end = .;
	}
	.sceExport.rodata : { KEEP(*(.sceExport.rodata)) }
	.sceImport.rodata : { KEEP(*(.sceImport.rodata)) }
	.sceFNID.rodata : { KEEP(*(.sceFNID.rodata)) }
	.sceVNID.rodata : { KEEP(*(.sceVNID.rodata)) }
	.sceFStub.rodata : { KEEP(*(.sceFStub.rodata)) }
	.sceVStub.rodata : {
		__sce_vstub_top = .;
		KEEP(*(.sceVStub.rodata))
	}
	__sce_tables_end = .;
	.rodata : { KEEP(*(.rodata.sceSdkVersion)) *(.rodata .rodata.* .sce_dbginfo) }
	.ARM.exidx : ALIGN(16) {
		__exidx_start = .;
		*(.ARM.exidx .ARM.exidx.*)
		__exidx_end = .;
	}
	.ARM.extab : ALIGN(16) {
		__extab_start = .;
		*(.ARM.extab .ARM.extab.*)
		__extab_end = .;
	}
	.sceRefs.rodata : { KEEP(*(.sceRefs.rodata)) }

	. = ALIGN(0x1000);
	.data : { *(.data .data.*) } :data
	.bss : { *(.bss .bss.*) *(COMMON) }

	.ARM.attributes 0 : { *(.ARM.attributes) }
	.comment 0 : { *(.comment) }
	.sce_dbgsrc 0 : { *(.sce_dbgsrc) }
	.sce_vstub 0 : { *(.sce_vstub) }
	/DISCARD/ : { *(.note.GNU-stack) }
}
