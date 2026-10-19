/*
 * SceLowio (os0:kd/lowio.skprx): ScePervasiveForDriver, the pervasive block's exports, each
 * declared once with its definition as the prototype.
 */
#pragma once

#include <stdint.h>
#include <lowio/pervasive_regs.h>

/*
 * The monitor's bus and clock services: SMC 0x110 sets a bus's state, SMC 0x114 a clock's level.
 * The monitor also gets r2 and r3 as the caller left them.
 */
uint32_t ScePervasiveForDriver_84EAB5BC(void);		/* bus 0 (the GPU) on */
uint32_t ScePervasiveForDriver_E8656ADF(void);		/* bus 0 off */
uint32_t ScePervasiveForDriver_D58E3A30(void);		/* bus 1 (Venezia) on */
uint32_t ScePervasiveForDriver_E0711D42(void);		/* bus 1 off */
uint32_t ScePervasiveForDriver_B2F232E5(uint32_t mode);	/* bus 2: state 1, or 2 */
uint32_t ScePervasiveForDriver_A7855D3A(void);		/* bus 2: state 3 */
uint32_t ScePervasiveForDriver_4712740D(void);		/* bus 2 off */
uint32_t ScePervasiveForDriver_C0F7CC39(uint32_t level);	/* clock 0 */
uint32_t ScePervasiveForDriver_09C35980(void);		/* clock 1: level 0 */
uint32_t ScePervasiveForDriver_7D02215D(uint32_t level);	/* clock 2 */
uint32_t ScePervasiveForDriver_ECA675E0(void);		/* clock 3: level 0 */

/* Names unknown: set and clear the lowio miscellaneous control bit. */
uint32_t ScePervasiveForDriver_FDD445C3(void);
uint32_t ScePervasiveForDriver_132E514A(void);

int ScePervasiveForDriver_03E1FAA6(void);
uint32_t ScePervasiveForDriver_043B33F5(unsigned port, unsigned mode);
uint32_t ScePervasiveForDriver_04D145C7(void);
uint32_t ScePervasiveForDriver_12D17D47(unsigned value);
int ScePervasiveForDriver_13CC07C9(int port, unsigned int mask);
int ScePervasiveForDriver_146108D4(int port);
int ScePervasiveForDriver_15237AF8(int port);
uint32_t ScePervasiveForDriver_26B51075(void);
int ScePervasiveForDriver_28731EC5(void);
uint32_t ScePervasiveForDriver_29E65AE1(unsigned port, uint32_t address, uint32_t size);
int ScePervasiveForDriver_2EEBE9AE(void);
uint32_t ScePervasiveForDriver_2FB5F88F(unsigned value);
int ScePervasiveForDriver_31C0A98B(void);
uint32_t ScePervasiveForDriver_34537B12(void);
int ScePervasiveForDriver_3CF7A012(int port);
int ScePervasiveForDriver_3D8A8712(int port);
int ScePervasiveForDriver_4235EB7D(int port);
uint32_t ScePervasiveForDriver_453BD2EC(unsigned port, unsigned enable);
ScePervasiveVidRegisters *ScePervasiveForDriver_47708C03(void);
int ScePervasiveForDriver_47F2C35C(int port);
int ScePervasiveForDriver_4AF7A01E(int port, unsigned int mask);
uint32_t ScePervasiveForDriver_4B99558F(void);
int ScePervasiveForDriver_4E43299D(int port);
int ScePervasiveForDriver_4F64DEF6(unsigned head);
int ScePervasiveForDriver_51981FEE(void);
uint32_t ScePervasiveForDriver_589D0291(unsigned value);
int ScePervasiveForDriver_5BAFA0DC(int port);
uint32_t ScePervasiveForDriver_5E20D7D5(void);
int ScePervasiveForDriver_6A2FB697(void);
uint32_t ScePervasiveForDriver_6AF0196F(unsigned first, unsigned second);
uint32_t ScePervasiveForDriver_6B2BE37E(unsigned port, uint32_t value);
int ScePervasiveForDriver_6B5DEDA2(void);
uint32_t ScePervasiveForDriver_725B4F59(unsigned value);
ScePervasiveMiscRegisters *ScePervasiveForDriver_81A155F1(void);
int ScePervasiveForDriver_821AE6C7(int port);
int ScePervasiveForDriver_8CF567AD(void);	/* callers pass no arguments; lowio forwards r2 to the SMC */
uint32_t ScePervasiveForDriver_925D9D24(unsigned value);
uint32_t ScePervasiveForDriver_98D8914A(unsigned value);
uint32_t ScePervasiveForDriver_9F8E589C(unsigned port, uint32_t value);
uint32_t ScePervasiveForDriver_A1D2E440(uint32_t value);
int ScePervasiveForDriver_A2EFD7AF(int port, unsigned int mask);
int ScePervasiveForDriver_A3CF9844(int port);
uint32_t ScePervasiveForDriver_A6832B33(unsigned value);
int ScePervasiveForDriver_A7E64C6F(void);
int ScePervasiveForDriver_AD1E81EB(int port, unsigned int mask);
int ScePervasiveForDriver_B1CFA18F(void);
int ScePervasiveForDriver_B8AF1249(void);
int ScePervasiveForDriver_BB67EF58(unsigned head);
int ScePervasiveForDriver_CA0ACFC5(unsigned int mask);
uint32_t ScePervasiveForDriver_D185D235(void);
int ScePervasiveForDriver_D326E15F(int port);
int ScePervasiveForDriver_D34E74C5(int port);
int ScePervasiveForDriver_E2D8F6C3(void);
uint32_t ScePervasiveForDriver_E6728E30(void);
int ScePervasiveForDriver_E9D95643(unsigned mode, unsigned speed);
int ScePervasiveForDriver_EBE9C84E(void);
int ScePervasiveForDriver_EDCA3961(int port);
int ScePervasiveForDriver_EEE340ED(int port);
int ScePervasiveForDriver_F345FB53(int port);
int ScePervasiveForDriver_F861FEE4(int port);
uint32_t ScePervasiveForDriver_F9E7B023(void);
int ScePervasiveForDriver_FB01A2DD(void);

uint32_t ScePervasiveForDriver_03EA9418(void);
int ScePervasiveForDriver_07F2A738(void);
uint32_t ScePervasiveForDriver_0C1A7FFA(void);
int ScePervasiveForDriver_0EBBE8DE(void);
int ScePervasiveForDriver_1068C067(int port);
int ScePervasiveForDriver_17109C28(void);
int ScePervasiveForDriver_18DD8043(int port);	/* vitasdk: kscePervasiveUartClockDisable */
int ScePervasiveForDriver_243D0E78(uint32_t clock, double *frequency, double *pixelFrequency);
/* vitasdk: kscePervasiveDsiClockDisable */
int ScePervasiveForDriver_25AE181E(int port, unsigned int mask);
uint32_t ScePervasiveForDriver_270BE98A(unsigned index);
uint32_t ScePervasiveForDriver_2A563E35(void);
int ScePervasiveForDriver_2A9778CD(void);	/* vitasdk: kscePervasiveMsifClockDisable */
int ScePervasiveForDriver_2F195C97(void);	/* vitasdk: kscePervasiveGpioClockEnable */
uint32_t ScePervasiveForDriver_350BF570(unsigned head);
int ScePervasiveForDriver_377126CD(void);
int ScePervasiveForDriver_39979C55(void);
int ScePervasiveForDriver_39E51AE2(unsigned int mask);
int ScePervasiveForDriver_3BF2A9B5(void);
int ScePervasiveForDriver_3E79D3D3(void);
int ScePervasiveForDriver_3F3E1AEA(int port);
uint32_t ScePervasiveForDriver_41C3C299(void);
int ScePervasiveForDriver_41F9E1C3(int port);
uint32_t ScePervasiveForDriver_443E2A3F(void);
int ScePervasiveForDriver_4B223AF5(int port);
int ScePervasiveForDriver_4CCD40E6(void);
/* vitasdk: kscePervasiveRemovableMemoryGetCardInsertState */
uint32_t ScePervasiveForDriver_551EEE82(void);
uint32_t ScePervasiveForDriver_59B7CA34(unsigned source, unsigned divider);
int ScePervasiveForDriver_5AFE0AF0(void);
uint32_t ScePervasiveForDriver_5B4EF352(void);
/* vitasdk: kscePervasiveMsifSetClock */
uint32_t ScePervasiveForDriver_64ABE589(unsigned value);
int ScePervasiveForDriver_66BF2885(int port);
int ScePervasiveForDriver_6E11EB97(void);
int ScePervasiveForDriver_701D5355(int port);
uint32_t ScePervasiveForDriver_714EEFB7(void);	/* vitasdk: kscePervasiveGetSoCRevision */
int ScePervasiveForDriver_731A097D(void);	/* vitasdk: kscePervasiveGpioResetEnable */
int ScePervasiveForDriver_7704C013(void);	/* vitasdk: kscePervasiveMsifClockEnable */
int ScePervasiveForDriver_788B6C61(int port);	/* vitasdk: kscePervasiveUartResetEnable */
int ScePervasiveForDriver_78C34032(void);	/* vitasdk: kscePervasiveGpioResetDisable */
int ScePervasiveForDriver_7AE2F8E8(void);
int ScePervasiveForDriver_7B0F388B(void);
int ScePervasiveForDriver_7B16F900(int port);	/* vitasdk: kscePervasiveSpiResetEnable */
int ScePervasiveForDriver_7C285361(void);
int ScePervasiveForDriver_7E813FC5(unsigned head, uint32_t *clock);
int ScePervasiveForDriver_7F4AB4AA(void);
int ScePervasiveForDriver_86BEA9A4(int port);
uint32_t ScePervasiveForDriver_8847825F(void);
int ScePervasiveForDriver_8A85E36B(void);
/* vitasdk: kscePervasiveUartSetBaudrate */
uint32_t ScePervasiveForDriver_8BAB45F8(unsigned port, unsigned baud);
int ScePervasiveForDriver_8EE3AEDF(void);
/* vitasdk: kscePervasiveDsiSetPixelClock */
int ScePervasiveForDriver_91C80C41(unsigned head, uint32_t clock);
int ScePervasiveForDriver_9BB7B932(void);
uint32_t ScePervasiveForDriver_9E81E057(void);
int ScePervasiveForDriver_A10583FF(int port);
int ScePervasiveForDriver_A3569FF1(void);	/* vitasdk: kscePervasiveMsifResetEnable */
int ScePervasiveForDriver_A7CE7DCC(int port);	/* vitasdk: kscePervasiveUartResetDisable */
int ScePervasiveForDriver_A85BF98A(int port);	/* vitasdk: kscePervasiveSpiClockDisable */
uint32_t ScePervasiveForDriver_A96642E3(unsigned value);
int ScePervasiveForDriver_AC475961(int port);
int ScePervasiveForDriver_B2EE45C9(void);
int ScePervasiveForDriver_B68254AD(void);
int ScePervasiveForDriver_B9904983(void);
/* vitasdk: kscePervasiveDsiClockEnable */
int ScePervasiveForDriver_BC42C72F(int port, unsigned int mask);
uint32_t ScePervasiveForDriver_BD868E6E(void);
int ScePervasiveForDriver_C0C842FE(void);
int ScePervasiveForDriver_C37CFDFC(int port);
int ScePervasiveForDriver_CB0F15CD(void);	/* vitasdk: kscePervasiveMsifResetDisable */
int ScePervasiveForDriver_D3B59C33(int port);
int ScePervasiveForDriver_DFD96BFC(int port);	/* vitasdk: kscePervasiveSpiClockEnable */
uint32_t ScePervasiveForDriver_E3A735B9(unsigned port);
/* vitasdk: kscePervasiveDsiResetEnable */
int ScePervasiveForDriver_E3FC1C8D(int port, unsigned int mask);
int ScePervasiveForDriver_E40BED0F(void);
int ScePervasiveForDriver_E4B145AE(int port);	/* vitasdk: kscePervasiveSpiResetDisable */
int ScePervasiveForDriver_E92E28FF(void);
int ScePervasiveForDriver_EB176898(void);	/* vitasdk: kscePervasiveGpioClockDisable */
int ScePervasiveForDriver_EFD084D8(int port);	/* vitasdk: kscePervasiveUartClockEnable */
uint32_t ScePervasiveForDriver_F1908D3C(unsigned head);
uint32_t ScePervasiveForDriver_F58EF3A5(void);
uint32_t ScePervasiveForDriver_FB0EF8C7(void);
int ScePervasiveForDriver_FBE5AA2F(void);
int ScePervasiveForDriver_FF4B3CA6(int port);
/* vitasdk: kscePervasiveDsiResetDisable */
int ScePervasiveForDriver_FFB43AC2(int port, unsigned int mask);
