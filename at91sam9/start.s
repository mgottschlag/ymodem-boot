.global start_asm
.extern main
.extern bss_start
.extern bss_end
.section .setup, "ax"

start_asm:
	ldr sp, =stack + 0x1000
	/* clear .bss */
	mov r1, #0
	ldr r4, =bss_start
	ldr r5, =bss_end
bss_loop:
	cmp r4, r5
	bge bss_done
	str r1, [r4]
	add r4, r4, #4
	b bss_loop
bss_done:
	blx main

.section .bss
.lcomm stack, 0x1000

