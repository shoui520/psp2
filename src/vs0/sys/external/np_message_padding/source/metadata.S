/* Inert module metadata. All addresses are linker-owned ABS32 references. */
.section .rodata.sceModuleData,"a",%progbits
.balign 4
.global moduleInfo
.type moduleInfo, %object
moduleInfo:
	.hword 0
	.byte 1, 1
	.asciz "SceNpMessagePadding"
	.zero 7
	.byte 6
	.word 0
	.word moduleExports - moduleInfo
	.word moduleExportsEnd - moduleInfo
	.word moduleExportsEnd - moduleInfo
	.word moduleExportsEnd - moduleInfo
	.word MODULE_NID
	.word 0, 0, 0
	.word 0xffffffff, 0xffffffff
	.word unwindTable - moduleInfo
	.word unwindEnd - moduleInfo
	.word unwindEnd - moduleInfo
	.word unwindEnd - moduleInfo
.size moduleInfo, . - moduleInfo
moduleExports:
	.hword 0x20, 0, 0x8000, 0, 2, 0
	.word 0, 0, 0
	.word variableNids, variableEntries
moduleExportsEnd:
variableNids:
	.word 0x936c8a78, 0x6c2224ba
variableEntries:
	.word moduleSdkVersion, moduleInfo
unwindTable:
	.word (moduleInfo - unwindTable) & 0x7fffffff
	.word 1
unwindEnd:
.global moduleSdkVersion
.type moduleSdkVersion, %object
moduleSdkVersion:
	.word MODULE_SDK_VERSION
.size moduleSdkVersion, . - moduleSdkVersion
