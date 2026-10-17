/* ScePervasive driver interfaces not declared by vitasdk. */
#pragma once

#include <stdint.h>
#include <lowio/pervasive_regs.h>

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
