/*
 * Sony's module layout (mkprx.py tables generates the SCE sections): text, the
 * import stubs and the SCE tables, read-only data and the unwind tables in one
 * segment; data and bss in the next.
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
	.text : {
		*(.ssMgrCore.SsMgrSysEventHandler)
		*(.ssMgrCore.module_start)
		*(.ssMgrCore.ssCheckProcessAuthInfo)
		*(.ssMgrCore.sceSblSsInfraAllocatePARangeVector)
		*(.ssMgrCore.SceSblSsMgrForDriver_C38D0CEA)
		*(.ssMgrCore.ksceSblSsGetNvsData)
		*(.ssMgrCore.ksceSblSsSetNvsData)
		*(.ssMgrCore.ksceSblSsEncryptWithPortability)
		*(.ssMgrCore.ksceSblSsDecryptWithPortability)
		*(.ssMgrCore.ksceSblSsMemset)
		*(.ssMgrQaf.ksceSblQafManagerGetQAFlags)
		*(.ssMgrQaf.ksceSblQafManagerGetQafName)
		*(.ssMgrQaf.qafInit)
		*(.text .text.*) *(.glue_7) *(.glue_7t) *(.vfp11_veneer) *(.v4_bx)
	} :text
	.sceStub.text : {
		__sce_stub_top = .;
		*(.sceStub.text)
	}
	__sce_text_end = .;
	.sceModuleInfo.rodata : { *(.sceModuleInfo.rodata) }
	.sceLib.ent : {
		__sce_libent_top = .;
		*(.sceLib.ent)
		__sce_libent_end = .;
	}
	.sceLib.stub : {
		__sce_libstub_top = .;
		*(.sceLib.stub)
		__sce_libstub_end = .;
	}
	.sceExport.rodata : { *(.sceExport.rodata) }
	.sceImport.rodata : { *(.sceImport.rodata) }
	.sceFNID.rodata : { *(.sceFNID.rodata) }
	.sceVNID.rodata : { *(.sceVNID.rodata) }
	.sceFStub.rodata : { *(.sceFStub.rodata) }
	.sceVStub.rodata : {
		__sce_vstub_top = .;
		*(.sceVStub.rodata)
	}
	__sce_tables_end = .;
	.rodata : { *(.rodata.sceSdkVersion) *(.rodata .rodata.* .sce_dbginfo) }
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
	.sceRefs.rodata : { *(.sceRefs.rodata) }

	. = @SS_MGR_DATA_BASE@;
	.data : { *(.data .data.*) } :data
	.bss : {
		*(.bss.ssMgr.dmacKeyringRegisters)
		*(.bss.ssMgr.dmacKeyringMutex)
		*(.bss.ssMgr.dmacKeyringStorage)
		*(.bss.ssMgr.qafInitialized)
		*(.bss.ssMgr.qaFlags)
@SS_MGR_QAF_BSS_INPUTS@
		*(.bss.ssMgr.qafSmId)
		*(.bss.ssMgr.aimgrPscode)
		*(.bss.ssMgr.aimgrOpenPsId)
		*(.bss.ssMgr.aimgrMutex)
		*(.bss.ssMgr.aimgrSmId)
		*(.bss.ssMgr.rngSeed)
		*(.bss.ssMgr.rngMutex)
		*(.bss.ssMgr.rngKey)
		*(.bss .bss.*) *(COMMON)
	}

	.ARM.attributes 0 : { *(.ARM.attributes) }
	.comment 0 : { *(.comment) }
	.sce_dbgsrc 0 : { *(.sce_dbgsrc) }
	.sce_vstub 0 : { *(.sce_vstub) }
	/DISCARD/ : { *(.note.GNU-stack) }
}
