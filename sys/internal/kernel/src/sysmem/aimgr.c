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

SceKblParam *SceSysrootForKernel_9DB56D1F(void);
int SceDipswForDriver_A98FC2FD(unsigned int bit);

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
	SceKblParam *kbl = SceSysrootForKernel_9DB56D1F();
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

int SceSblAIMgrForDriver_3B638885(void)
{
	return AimgrProductCode() == PRODUCT_CODE_TEST;
}

int SceSblAIMgrForDriver_274663A0(void)
{
	return AimgrProductCode() == PRODUCT_CODE_TOOL;
}

int SceSblAIMgrForDriver_F4B98F66(void)
{
	return AimgrProductCode() == PRODUCT_CODE_DEX;
}

__attribute__((noinline)) int SceSblAIMgrForDriver_6D5A3FC9(void)
{
	return AimgrProductCode() == PRODUCT_CODE_CEX_FIRST && AimgrProductSubCode() == 0x10 &&
	       AimgrFactoryCode() == 0x24;
}

int SceSblAIMgrForDriver_D78B04A2(void)
{
	if ((SceUInt16)(AimgrProductCode() - PRODUCT_CODE_CEX_FIRST) < PRODUCT_CODE_CEX_COUNT)
		return !SceSblAIMgrForDriver_6D5A3FC9();
	return 0;
}

__attribute__((noinline)) int SceSblAIMgrForDriver_C6E83F34(void)
{
	if ((AimgrProductCode() == PRODUCT_CODE_TOOL && AimgrProductSubCode() == 0x408) ||
	    (AimgrProductSubCode() & 0x200))
		return 1;
	if (!SceSblAIMgrForDriver_6D5A3FC9())
		return 0;
	return AimgrIsDolceHardware();
}

int SceSblAIMgrForDriver_963CA644(void)
{
	if ((SceUInt16)(AimgrProductCode() - PRODUCT_CODE_TEST) > 0x11 ||
	    SceSblAIMgrForDriver_C6E83F34()) {
		if (!SceSblAIMgrForDriver_6D5A3FC9())
			return 0;
		if (AimgrIsDolceHardware())
			return 0;
	}
	return 1;
}

int SceSblAIMgrForDriver_4273B97B(void)
{
	return SceSblAIMgrForDriver_963CA644();
}

int SceSblAIMgrForDriver_71608CA3(void)
{
	if (SceSblAIMgrForDriver_C6E83F34())
		return 1;
	return SceDipswForDriver_A98FC2FD(0x98) != 0;
}

int SceSblAIMgrForDriver_37A79140(void)
{
	return AimgrProductCode() == PRODUCT_CODE_TOOL && AimgrProductSubCode() == 4;
}

int SceSblAIMgrForDriver_FF5784B9(void)
{
	return AimgrProductCode() == PRODUCT_CODE_CEX_FIRST && AimgrProductSubCode() == 2;
}

int SceSblAIMgrForDriver_BB9D146B(void)
{
	return AimgrProductCode() == PRODUCT_CODE_TOOL && AimgrProductSubCode() == 3;
}

int SceSblAIMgrForDriver_E5E47FF7(void)
{
	return AimgrProductCode() == PRODUCT_CODE_TOOL && AimgrProductSubCode() == 5;
}

int SceSblAIMgrForDriver_05F79D4A(void)
{
	return AimgrProductCode() == PRODUCT_CODE_CEX_FIRST && AimgrProductSubCode() == 7;
}

SceUInt16 SceSblAIMgrForDriver_14345161(void)
{
	return AimgrProductCode();
}

SceUInt16 SceSblAIMgrForDriver_B33CEC8F(void)
{
	return AimgrProductSubCode();
}

/* the system software version the console left the factory with */
int SceSblAIMgrForDriver_47D9CF13(SceUInt32 *smi)
{
	SceKblParam *kbl = SceSysrootForKernel_9DB56D1F();

	if (smi == NULL || kbl == NULL)
		return AIMGR_ERROR_INVALID_ARGUMENT;
	*smi = kbl->factory_fw_version;
	return 0;
}
