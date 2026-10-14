/* BSOD privately maps IFTU, GPIO and SPI during startup. */
#include "bsod_internal.h"

uint8_t *s_bsodIftu[3];
uint8_t *s_bsodSyscon[2];

#if FW_DEBUG
static const SceKernelDebugInfo s_iftuAssert = {
	0x6ec74984, 0x62f7350e, 0xe6d8021d,
	"iftu.c", 94, "_sceKernelBsodIftuInit",
};
static const SceKernelDebugInfo s_sysconAssert = {
	0x0f48c989, 0x80b93d37, 0x91de3a7a,
	"syscon.c", 92, "_sceKernelBsodSysconInit",
};
#endif

int _sceKernelBsodIftuInit(int dolce)
{
	static const char *const names[2][3] = {
		{ "SceBsodIftu0RegA", "SceBsodIftu0RegB", "SceBsodIftuc0Reg" },
		{ "SceBsodIftu1RegA", "SceBsodIftu1RegB", "SceBsodIftuc1Reg" },
	};
	SceKernelAllocMemBlockKernelOpt options;
	void *base;
	unsigned i;
	int uid;

	for (i = 0; i < 3; i++) {
		memset(&options, 0, sizeof(options));
		options.size = sizeof(options);
		options.attr = 2;
		options.paddr = 0xe5020000 + dolce * 0x10000 + i * 0x1000;
		uid = ksceKernelAllocMemBlock(names[dolce][i], SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
					    0x1000, &options);
		if (uid < 0) {
#if FW_DEBUG
			if (ksceKernelGetAssertLevel() > 0)
				ksceKernelAssertLevel(1, 0, &s_iftuAssert,
						     __builtin_return_address(0));
#endif
			return uid;
		}
		ksceKernelGetMemBlockBase(uid, &base);
		s_bsodIftu[i] = base;
	}
	return 0;
}

int _sceKernelBsodSysconInit(void)
{
	static const char *const names[2] = { "SceBsodGpio0", "SceBsodSpi0" };
	static const uint32_t physical[2] = { 0xe20a0000, 0xe0a00000 };
	SceKernelAllocMemBlockKernelOpt options;
	void *base;
	unsigned i;
	int uid;

	for (i = 0; i < 2; i++) {
		memset(&options, 0, sizeof(options));
		options.size = sizeof(options);
		options.attr = 2;
		options.paddr = physical[i];
		uid = ksceKernelAllocMemBlock(names[i], SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_SO_RW, 0x1000, &options);
		if (uid < 0) {
#if FW_DEBUG
			if (ksceKernelGetAssertLevel() > 0)
				ksceKernelAssertLevel(1, 0, &s_sysconAssert,
						     __builtin_return_address(0));
#endif
			return uid;
		}
		ksceKernelGetMemBlockBase(uid, &base);
		s_bsodSyscon[i] = base;
	}
	return 0;
}
