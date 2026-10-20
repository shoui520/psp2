/* Secure monitor calls: the service number in ip, the caller's r0-r3 passed through. */
.syntax unified
.thumb
.arch_extension sec
.text

.macro MonitorCall name, service
.global \name
.type \name, %function
.thumb_func
\name:
	movw ip, #\service
	smc #0
	bx lr
.size \name, . - \name
.endm

.balign 4
MonitorCall sceSysconSetPowerModeMonitorCall, 0x11a
MonitorCall sceCrashDumpMonitorCall, 0x122
