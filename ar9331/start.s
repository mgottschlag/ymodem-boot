.global start_asm
.extern main
.extern bss_start
.extern bss_end
.set reorder
.section .setup, "ax"

/* kernel entry */
start_asm:
	/* disable interrupts */
	mtc0 $zero, $12 /* CP0_STATUS */
	/* setup an initial kernel stack */
	la $sp, stack
	addi $sp, 0x1000
	/* clear .bss */
	la $t0, bss_start
	la $t1, bss_end
	li $t2, 0
1:
	sw $t2, ($t0)
	addi $t0, 4
	bgt $t1, $t0, 1b
	/* call into C code */
	j main

.section .bss
.lcomm stack, 0x1000
