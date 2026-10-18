/* SceSysmem's SceSblAIMgrForDriver checks that vitasdk's psp2kern/kernel/aimgr.h does not declare. */

#pragma once

#include <scetypes.h>
#include <stdint.h>

int SceSblAIMgrForDriver_6D5A3FC9(void);	/* vitasdk: ksceSblAimgrIsJapaneseFat */
int SceSblAIMgrForDriver_FF5784B9(void);	/* vitasdk: ksceSblAimgrIsPrototypeRev2 */
int SceSblAIMgrForDriver_05F79D4A(void);	/* vitasdk: ksceSblAimgrIsPrototypeRev7 */
int SceSblAIMgrForDriver_BB9D146B(void);	/* vitasdk: ksceSblAimgrIsToolRev3 */
int SceSblAIMgrForDriver_37A79140(void);	/* vitasdk: ksceSblAimgrIsToolRev4 */
int SceSblAIMgrForDriver_E5E47FF7(void);	/* vitasdk: ksceSblAimgrIsToolRev5 */

SceUInt16 SceSblAIMgrForDriver_14345161(void);	/* vitasdk: ksceSblAimgrGetProductCode */
int SceSblAIMgrForDriver_274663A0(void);	/* vitasdk: ksceSblAimgrIsTool */
int SceSblAIMgrForDriver_3B638885(void);	/* vitasdk: ksceSblAimgrIsTest */
int SceSblAIMgrForDriver_4273B97B(void);	/* vitasdk: ksceSblAimgrIsVITA */
int SceSblAIMgrForDriver_47D9CF13(SceUInt32 *smi);	/* vitasdk: ksceSblAimgrGetSMI */
int SceSblAIMgrForDriver_71608CA3(void);	/* vitasdk: ksceSblAimgrIsDolce */
int SceSblAIMgrForDriver_963CA644(void);	/* vitasdk: ksceSblAimgrIsGenuineVITA */
SceUInt16 SceSblAIMgrForDriver_B33CEC8F(void);	/* vitasdk: ksceSblAimgrGetProductSubCode */
int SceSblAIMgrForDriver_C6E83F34(void);	/* vitasdk: ksceSblAimgrIsGenuineDolce */
int SceSblAIMgrForDriver_D78B04A2(void);	/* vitasdk: ksceSblAimgrIsCEX */
int SceSblAIMgrForDriver_F4B98F66(void);	/* vitasdk: ksceSblAimgrIsDEX */
