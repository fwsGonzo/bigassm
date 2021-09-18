.org 0x100000

.global _start
_start:             ;; Entry point label
	li t0, -16      ;; Stack at end of 128-bit
	xor sp, sp
	add sp, t0

	call my_function

exit:
	li a7, 1        ;; Syscall 1 (exit)
	li a0, 0x666    ;; Exit code (1st arg)
	scall           ;; Execute syscall
	jmp exit

hello_world:        ;; String label
	.type hello_world, @object
	.string "Hello World!" ;; Zt-string

.align 4 ;; BUG work-around
my_function:
	add sp, -32
	sq a0, sp+0

	li t0, 2        ;; Syscall 2 (print)
	sq t0, sp+16    ;; Store 128-bit value
	lq sp+16, a7    ;; Load 128-bit value

	la a0, hello_world ;; address of string
	scall           ;; Execute syscall

	lq sp+0, a0

	ret
