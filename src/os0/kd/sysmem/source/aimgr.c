/*
 * SceSysmem: SceSblAIMgr's queries of the console's model, from the PS code
 * and the hardware info in the KBL parameters (cached at boot). The PS code
 * fields are big-endian.
 */

#include <psp2kern/kernel/kbl/kbl.h>
#include <psp2kern/kernel/ssmgr.h>

/* name unknown */
#define AIMGR_ERROR_INVALID_ARGUMENT	0x800f0c16

#define PRODUCT_CODE_TEST		0x100
#define PRODUCT_CODE_TOOL		0x101
#define PRODUCT_CODE_DEX		0x102
#define PRODUCT_CODE_CEX_FIRST		0x103	/* CEX: 0x103..0x111 */
#define PRODUCT_CODE_CEX_COUNT		0xf

/* the hardware info's model byte of a PS TV (Dolce) */
#define HARDWARE_MODEL_MASK		0xff0000

SceKblParam *ksceKernelSysrootGetKblParam(void);
int ksceKernelCheckDipsw(unsigned int bit);

/* a big-endian halfword */
#define SWAP16(x)			((SceUInt16)((x) >> 8 | (x) << 8))

/* the KBL's PS code, copied as two words */
static union {
	SceUInt32 words[2];
	ScePsCode code;
} s_psCode;
static SceUInt32 s_hardwareInfo;

static inline SceUInt16 AimgrProductCode(void)
{
	return SWAP16(s_psCode.code.product_code);
}

static inline SceUInt16 AimgrProductSubCode(void)
{
	return SWAP16(s_psCode.code.product_sub_code);
}

static inline SceUInt16 AimgrFactoryCode(void)
{
	return SWAP16(s_psCode.code.factory_code);
}

static inline int AimgrIsDolceHardware(void)
{
	SceUInt32 model = s_hardwareInfo & HARDWARE_MODEL_MASK;

	return model == 0x700000 || model == 0x720000 || model == 0x510000;
}

/* name unknown */
int AimgrInit(void)
{
	SceKblParam *kbl = ksceKernelSysrootGetKblParam();
	const SceUInt32 *psCode;

	if (kbl == NULL) {
		s_hardwareInfo = 0;
		s_psCode.words[0] = 0;
		s_psCode.words[1] = 0;
		return 0;
	}
	/* the KBL parameters are word aligned */
	psCode = (const SceUInt32 *)(const void *)&kbl->pscode;
	s_hardwareInfo = kbl->hardware_info;
	s_psCode.words[0] = psCode[0];
	s_psCode.words[1] = psCode[1];
	return 0;
}

int ksceSblAimgrIsTest(void)
{
	return AimgrProductCode() == PRODUCT_CODE_TEST;
}

int ksceSblAimgrIsTool(void)
{
	return AimgrProductCode() == PRODUCT_CODE_TOOL;
}

int ksceSblAimgrIsDEX(void)
{
	return AimgrProductCode() == PRODUCT_CODE_DEX;
}

__attribute__((noinline)) int ksceSblAimgrIsJapaneseFat(void)
{
	return AimgrProductCode() == PRODUCT_CODE_CEX_FIRST && AimgrProductSubCode() == 0x10 &&
	       AimgrFactoryCode() == 0x24;
}

int ksceSblAimgrIsCEX(void)
{
	if ((SceUInt16)(AimgrProductCode() - PRODUCT_CODE_CEX_FIRST) < PRODUCT_CODE_CEX_COUNT)
		return !ksceSblAimgrIsJapaneseFat();
	return 0;
}

__attribute__((noinline)) int ksceSblAimgrIsGenuineDolce(void)
{
	if ((AimgrProductCode() == PRODUCT_CODE_TOOL && AimgrProductSubCode() == 0x408) ||
	    (AimgrProductSubCode() & 0x200))
		return 1;
	if (!ksceSblAimgrIsJapaneseFat())
		return 0;
	return AimgrIsDolceHardware();
}

int ksceSblAimgrIsGenuineVITA(void)
{
	if ((SceUInt16)(AimgrProductCode() - PRODUCT_CODE_TEST) > 0x11 ||
	    ksceSblAimgrIsGenuineDolce()) {
		if (!ksceSblAimgrIsJapaneseFat())
			return 0;
		if (AimgrIsDolceHardware())
			return 0;
	}
	return 1;
}

int ksceSblAimgrIsVITA(void)
{
	return ksceSblAimgrIsGenuineVITA();
}

int ksceSblAimgrIsDolce(void)
{
	if (ksceSblAimgrIsGenuineDolce())
		return 1;
	return ksceKernelCheckDipsw(0x98) != 0;
}

int ksceSblAimgrIsToolRev4(void)
{
	return AimgrProductCode() == PRODUCT_CODE_TOOL && AimgrProductSubCode() == 4;
}

int ksceSblAimgrIsPrototypeRev2(void)
{
	return AimgrProductCode() == PRODUCT_CODE_CEX_FIRST && AimgrProductSubCode() == 2;
}

int ksceSblAimgrIsToolRev3(void)
{
	return AimgrProductCode() == PRODUCT_CODE_TOOL && AimgrProductSubCode() == 3;
}

int ksceSblAimgrIsToolRev5(void)
{
	return AimgrProductCode() == PRODUCT_CODE_TOOL && AimgrProductSubCode() == 5;
}

int ksceSblAimgrIsPrototypeRev7(void)
{
	return AimgrProductCode() == PRODUCT_CODE_CEX_FIRST && AimgrProductSubCode() == 7;
}

SceUInt16 ksceSblAimgrGetProductCode(void)
{
	return AimgrProductCode();
}

SceUInt16 ksceSblAimgrGetProductSubCode(void)
{
	return AimgrProductSubCode();
}

/* the system software version the console left the factory with */
int ksceSblAimgrGetSMI(SceUInt32 *smi)
{
	SceKblParam *kbl = ksceKernelSysrootGetKblParam();

	if (smi == NULL || kbl == NULL)
		return AIMGR_ERROR_INVALID_ARGUMENT;
	*smi = kbl->factory_fw_version;
	return 0;
}
