/* Program-class checks and the native configuration-flag predicates. */
#include <stdint.h>
#include "../common/halt_assert.h"

extern int sceAppMgrIsGameProgram(void);
extern int sceAppMgrIsNonGameProgram(void);
extern uint32_t ScePafMisc_AF4FC3F4(void);
extern int sceCtrlSetSamplingMode(unsigned int mode);
extern int sceCtrlSetSamplingModeExt(unsigned int mode);

uint32_t sce_paf_misc_controller_mode_d418;

int ScePafMisc_30EAFB53(void)
{
	int result;

	if (sce_paf_misc_controller_mode_d418 == 2)
		result = sceCtrlSetSamplingModeExt(0);
	else
		result = sceCtrlSetSamplingMode(0);
	if (!result)
		return 0;
	return result;
}

/* Native BSS byte queried by the program-class flag exports. */
uint8_t sce_paf_misc_program_flags_eefc;

static inline __attribute__((always_inline)) void CheckProgramClass(void)
{
	if (sceAppMgrIsGameProgram() || sceAppMgrIsNonGameProgram())
		on_error();
}

int ScePafMisc_2C227FFD(void)
{
	CheckProgramClass();
	return sce_paf_misc_program_flags_eefc & 1;
}

int ScePafMisc_CB582B61(void)
{
	CheckProgramClass();
	return (sce_paf_misc_program_flags_eefc & 2) != 0;
}

int ScePafMisc_B7CFFF5C(void)
{
	CheckProgramClass();
	return (sce_paf_misc_program_flags_eefc & 4) != 0;
}

int ScePafMisc_C74F989D(void)
{
	CheckProgramClass();
	return (sce_paf_misc_program_flags_eefc & 8) != 0;
}

int ScePafMisc_D8D642F5(void)
{
	CheckProgramClass();
	return (sce_paf_misc_program_flags_eefc & 0x10) != 0;
}

int ScePafMisc_9DE7DA6D(void)
{
	CheckProgramClass();
	return (sce_paf_misc_program_flags_eefc & 0x20) != 0;
}

int ScePafMisc_7ED8F03C(void)
{
	CheckProgramClass();
	return (sce_paf_misc_program_flags_eefc & 0x40) != 0;
}

int ScePafMisc_183E674B(void)
{
	CheckProgramClass();
	return ScePafMisc_AF4FC3F4() == 0;
}

int ScePafMisc_48DF4B35(void)
{
	CheckProgramClass();
	return ScePafMisc_AF4FC3F4() == 0;
}

int ScePafMisc_AFB1006F(void)
{
	CheckProgramClass();
	return ScePafMisc_AF4FC3F4() == 0;
}

int ScePafMisc_A6A33C15(void)
{
	CheckProgramClass();
	return ScePafMisc_AF4FC3F4() == 1;
}
