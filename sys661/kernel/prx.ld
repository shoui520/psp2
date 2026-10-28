OUTPUT_FORMAT("elf32-littlemips", "elf32-bigmips", "elf32-littlemips")
OUTPUT_ARCH(mips:allegrex)
ENTRY(module_start)

SECTIONS
{
	PROVIDE (__executable_start = 0x0); . = 0x0;
	.interp         : { *(.interp) }
	.dynamic        : { *(.dynamic) }
	.hash           : { *(.hash) }
	.dynsym         : { *(.dynsym) }
	.dynstr         : { *(.dynstr) }
	.gnu.version    : { *(.gnu.version) }
	.gnu.version_d  : { *(.gnu.version_d) }
	.gnu.version_r  : { *(.gnu.version_r) }
	.rel.text       : { *(.rel.text .rel.text.* .rel.gnu.linkonce.t.*) }
	.rela.text      : { *(.rela.text .rela.text.* .rela.gnu.linkonce.t.*) }
	.rel.init       : { *(.rel.init) }
	.rela.init      : { *(.rela.init) }
	.rel.fini       : { *(.rel.fini) }
	.rela.fini      : { *(.rela.fini) }
	.rel.sceStub.text         : { *(.rel.sceStub.text) *(SORT(.rel.sceStub.text.*)) }
	.rel.lib.ent.top          : { *(.rel.lib.ent.top) }
	.rel.lib.ent              : { *(.rel.lib.ent) }
	.rel.lib.ent.btm          : { *(.rel.lib.ent.btm) }
	.rel.lib.stub.top         : { *(.rel.lib.stub.top) }
	.rel.lib.stub             : { *(.rel.lib.stub) }
	.rel.lib.stub.btm         : { *(.rel.lib.stub.btm) }
	.rel.rodata.sceModuleInfo : { *(.rel.rodata.sceModuleInfo) }
	.rel.rodata.sceResident   : { *(.rel.rodata.sceResident) }
	.rel.rodata.sceNid        : { *(.rel.rodata.sceNid) }
	.rel.rodata.sceVstub      : { *(.rel.rodata.sceVstub) *(SORT(.rel.rodata.sceVstub.*)) }
	.rel.rodata               : { *(.rel.rodata .rel.rodata.* .rel.gnu.linkonce.r.*) }
	.rela.rodata              : { *(.rela.rodata .rela.rodata.* .rela.gnu.linkonce.r.*) }
	.rel.rodata.sceSdkVersion : { *(.rel.rodata.sceSdkVersion) }
	.rel.data.rel.ro          : { *(.rel.data.rel.ro*) }
	.rela.data.rel.ro         : { *(.rela.data.rel.ro*) }
	.rel.data                 : { *(.rel.data .rel.data.* .rel.gnu.linkonce.d.*) }
	.rela.data                : { *(.rela.data .rela.data.* .rel.gnu.linkonce.d.*) }
	.rel.tdata                : { *(.rel.tdata .rel.tdata.* .rel.gnu.linkonce.td.*) }
	.rela.tdata               : { *(.rela.tdata .rela.tdata.* .rela.gnu.linkonce.td.*) }
	.rel.tbss                 : { *(.rel.tbss .rel.tbss.* .rel.gnu.linkonce.tb.*) }
	.rela.tbss                : { *(.rela.tbss .rela.tbss.* .rel.gnu.linkonce.tb.*) }
	.rel.ctors                : { *(.rel.ctors) }
	.rela.ctors               : { *(.rela.ctors) }
	.rel.dtors                : { *(.rel.dtors) }
	.rela.dtors               : { *(.rela.dtors) }
	.rel.got                  : { *(.rel.got) }
	.rela.got                 : { *(.rela.got) }
	.rel.sdata                : { *(.rel.sdata .rel.sdata.* .rel.gnu.linkonce.s.*) }
	.rela.sdata               : { *(.rela.sdata .rela.sdata.* .rela.gnu.linkonce.s.*) }
	.rel.sbss                 : { *(.rel.sbss .rel.sbss.* .rel.gnu.linkonce.sb.*) }
	.rela.sbss                : { *(.rela.sbss .rela.sbss.* .rela.gnu.linkonce.sb.*) }
	.rel.sdata2               : { *(.rel.sdata2 .rel.sdata2.* .rel.gnu.linkonce.s2.*) }
	.rela.sdata2              : { *(.rela.sdata2 .rela.sdata2.* .rela.gnu.linkonce.s2.*) }
	.rel.sbss2                : { *(.rel.sbss2 .rel.sbss2.* .rel.gnu.linkonce.sb2.*) }
	.rela.sbss2               : { *(.rela.sbss2 .rela.sbss2.* .rela.gnu.linkonce.sb2.*) }
	.rel.bss                  : { *(.rel.bss .rel.bss.* .rel.gnu.linkonce.b.*) }
	.rela.bss                 : { *(.rela.bss .rela.bss.* .rela.gnu.linkonce.b.*) }
	.rel.plt                  : { *(.rel.plt) }
	.rela.plt                 : { *(.rela.plt) }

	.text :
	{
		_ftext = . ;
		*(.text .stub .text.* .gnu.linkonce.t.*)
		KEEP (*(.text.*personality*))
		*(.gnu.warning)
		*(.mips16.fn.*) *(.mips16.call.*)
	} =0
	.init           : { KEEP (*(.init)) } =0
	.plt            : { *(.plt) }
	.fini           : { KEEP (*(.fini)) } =0
	.sceStub.text   : { KEEP(*(.sceStub.text)) KEEP(*(SORT(.sceStub.text.*))) }
	PROVIDE (__etext = .);
	PROVIDE (_etext = .);
	PROVIDE (etext = .);
	.lib.ent.top    : { KEEP(*(.lib.ent.top)) }
	.lib.ent        : { KEEP(*(.lib.ent)) }
	.lib.ent.btm    : { KEEP(*(.lib.ent.btm)) }
	.lib.stub.top   : { KEEP(*(.lib.stub.top)) }
	.lib.stub       : { KEEP(*(.lib.stub)) }
	.lib.stub.btm   : { KEEP(*(.lib.stub.btm)) }
	.rodata.sceModuleInfo : { KEEP (*(.rodata.sceModuleInfo)) }
	.rodata.sceResident   : { KEEP (*(.rodata.sceResident)) }
	.rodata.sceNid        : { KEEP (*(.rodata.sceNid)) }
	.rodata.sceVstub      : { KEEP (*(.rodata.sceVstub)) KEEP (*(SORT(.rodata.sceVstub.*))) }
	.rodata               : { *(.rodata .rodata.* .gnu.linkonce.r.*) }
	.rodata1              : { *(.rodata1) }
	.rodata.sceSdkVersion : { KEEP (*(.rodata.sceSdkVersion)) }
	.sdata2               : { *(.sdata2 .sdata2.* .gnu.linkonce.s2.*) }
	.sbss2                : { *(.sbss2 .sbss2.* .gnu.linkonce.sb2.*) }
	.eh_frame_hdr         : { *(.eh_frame_hdr) }
	.eh_frame             : ONLY_IF_RO { KEEP (*(.eh_frame)) }
	.gcc_except_table     : ONLY_IF_RO { KEEP (*(.gcc_except_table)) *(.gcc_except_table.*) }

	. = ALIGN(16);
	.eh_frame             : ONLY_IF_RW { KEEP (*(.eh_frame)) }
	.gcc_except_table     : ONLY_IF_RW { KEEP (*(.gcc_except_table)) *(.gcc_except_table.*) }
	.tdata                : { *(.tdata .tdata.* .gnu.linkonce.td.*) }
	.tbss                 : { *(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon) }
	. = ALIGN(4);
	.data.rel.ro          : { *(.data.rel.ro.local) *(.data.rel.ro*) }
	.data :
	{
		_fdata = . ;
		*(.data .data.* .gnu.linkonce.d.*)
		KEEP (*(.gnu.linkonce.d.*personality*))
		SORT(CONSTRUCTORS)
	}
	.data1          : { *(.data1) }
	. = .;
	_gp = ALIGN(16) + 0x7ff0;
	.got            : { *(.got.plt) *(.got) }
	.sdata          : { *(.sdata .sdata.* .gnu.linkonce.s.*) }
	.lit8           : { *(.lit8) }
	.lit4           : { *(.lit4) }
	_edata = .;
	PROVIDE (edata = .);
	__bss_start = .;
	_fbss = .;
	.sbss :
	{
		PROVIDE (__sbss_start = .);
		PROVIDE (___sbss_start = .);
		*(.dynsbss)
		*(.sbss .sbss.* .gnu.linkonce.sb.*)
		*(.scommon)
		PROVIDE (__sbss_end = .);
		PROVIDE (___sbss_end = .);
	}
	.bss :
	{
		*(.dynbss)
		*(.bss .bss.* .gnu.linkonce.b.*)
		*(COMMON)
		. = ALIGN(4);
	}
	. = ALIGN(4);
	_end = .;
	PROVIDE (end = .);
	/DISCARD/ : { *(.comment) *(.pdr) *(.note.GNU-stack) *(.MIPS.abiflags) }
}
