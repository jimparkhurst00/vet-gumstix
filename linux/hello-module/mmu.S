.section .text
	
	.global arm_set_ttbr0
arm_set_ttbr0:

	mcr	p15, 0, r0, c2, c0, 0
	mov	pc, lr
	
	.global arm_read_ttbr0
arm_read_ttbr0:

	mrc	p15, 0, r0, c2, c0, 0
	mov	pc, lr
	
	.global arm_set_ttbr1
arm_set_ttbr1:

	mcr	p15, 0, r0, c2, c0, 1
	mov	pc, lr
	
	.global arm_read_ttbr1
arm_read_ttbr1:

	mrc	p15, 0, r0, c2, c0, 1
	mov	pc, lr

	.global arm_set_ttbcr
arm_set_ttbcr:
	mcr	p15, 0, r0, c2, c0, 2
	mov	pc, lr

	.global arm_get_ttbcr
arm_get_ttbcr:
	mrc	p15, 0, r0, c2, c0, 2
	mov	pc, lr