.syntax unified
.arm
.text
.global _start
.type _start, %function

_start:
	mov r6, sp
	ldr r0, =main
	ldr r1, [r6]
	add r2, r6, #4
	mov r3, #0
	mov r4, #0
	mov r5, #0
	mov r6, sp
	bl __libc_start_main
	mov r7, #1
	svc #0

.global probe_exit
.type probe_exit, %function
probe_exit:
	mov r0, #0
	mov r7, #1
	svc #0

.section .note.GNU-stack,"",%progbits
