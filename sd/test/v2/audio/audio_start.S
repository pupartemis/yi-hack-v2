.syntax unified
.arm
.text
.global _start
.type _start, %function

_start:
	mov r4, sp
	ldr r0, [r4]
	add r1, r4, #4
	add r2, r1, r0, lsl #2
	add r2, r2, #4
	bl main
	mov r7, #1
	svc #0

.section .note.GNU-stack,"",%progbits
