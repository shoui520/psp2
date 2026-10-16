/* PWM registers (ScePwmReg at 0xe20c0000): two channels of 0x20 bytes.
 * Sony names none of them; the names follow what lowio's ScePwmForDriver
 * exports do with each word.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

#define PWM_REG_BASE			0xe20c0000

#define SCE_PWM_CONTROL_ENABLE		0x001	/* the output runs */
#define SCE_PWM_CONTROL_UPDATE		0x002	/* load period and duty; clears when done */
#define SCE_PWM_CONTROL_INVERT		0x100	/* output level while the duty is 0 */
#define SCE_PWM_CONTROL_IDLE_LEVEL	0x200	/* output level while the period is 0 */
#define SCE_PWM_CONTROL_LEVELS		(SCE_PWM_CONTROL_INVERT | SCE_PWM_CONTROL_IDLE_LEVEL)

typedef struct ScePwmChannelRegisters {
	volatile uint32_t control;
	volatile uint32_t period;
	volatile uint32_t duty;
	volatile uint32_t reserved[5];
} ScePwmChannelRegisters;

typedef struct ScePwmRegisters {
	ScePwmChannelRegisters channel[2];
} ScePwmRegisters;

_Static_assert(offsetof(ScePwmChannelRegisters, duty) == 0x8, "PWM duty");
_Static_assert(sizeof(ScePwmChannelRegisters) == 0x20, "PWM channel size");
