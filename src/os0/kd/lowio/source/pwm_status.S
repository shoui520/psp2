/* PWM status read with explicit channel validation and one hardware read. */
.syntax unified
.cpu cortex-a9
.thumb
.text
.align 2
.global ScePwmForDriver_99911920
.type ScePwmForDriver_99911920, %function
.thumb_func
ScePwmForDriver_99911920:
	movw r3, #:lower16:s_pwmBase
	movt r3, #:upper16:s_pwmBase
	ldr r3, [r3]
	cmp r0, #1
	bhi .Linvalid
	lsl r0, r0, #5
	ldr r0, [r3, r0]
	ubfx r0, r0, #1, #1
	bx lr
.Linvalid:
	movw r0, #0x200
	movt r0, #0x803f
	bx lr
.size ScePwmForDriver_99911920, .-ScePwmForDriver_99911920
.section .note.GNU-stack,"",%progbits
