STM32F10x Firmware Library V2.0.3
=================================

STMicroelectronics' firmware library for the STM32F10x (V2.0.3, 09/22/2008), the
library barkley's motion MCU program (motionC) links: its RCC and FLASH functions are
V2.0.3's (FLASH_SetLatency's two read-modify-writes, FLASH_ProgramWord's
`*(vu16*)(Address + 2)` and `!= FLASH_BUSY` checks, the 0x0F/0xFFF timeouts, RCC_DeInit's
CIR reset to 0); V3.x differs in each. Sony's changes are in
barkley/src/STM32/Libraries/fwlib.patch.

ST no longer publishes V2.0.3. Two copies were compared:
* eLua's SVN, tags/v0.6, src/platform/stm32/FWLib/library (github.com/BackupTheBerlios/elua-svn)
* github.com/timmy00274672/STM32-Learning, FWLib/library
Their code is the same; comments differ (apostrophes lost in one, a misspelling in
stm32f10x_adc.h). These files are the eLua copy, unchanged.

The library carries no license file; each file's header holds ST's copyright and its
"for guidance only" notice.

Kept: library/inc/{stm32f10x_type.h, stm32f10x_map.h, stm32f10x_lib.h, cortexm3_macro.h}
and the inc/ and src/ files of each driver the program uses. The rest of the release
(examples, startup files, the other drivers) is left out.
