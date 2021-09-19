.org 0x100000

.global _start
_start:             ;; Entry point label
	li t0, -16      ;; Stack at end of 128-bit
	xor sp, sp
	add sp, t0

	li  s0, 4
	xor s1, s1
repeat:
	add s0, -1
	bne s0, s1, repeat

	call my_function ;; A regular function call
	;; We return here after the function ends.

exit:
	li a7, 1        ;; Syscall 1 (exit)
	li a0, 0x666    ;; Exit code (1st arg)
	ecall           ;; Execute syscall
	jmp exit        ;; Loop exit to prevent problems

hello_world:        ;; String label
	.type hello_world, @object
	.string "Hello World!" ;; Zt-string

my_function:
	add sp, -32
	sq a0, sp+0

	li t0, 2        ;; Syscall 2 (print)
	sq t0, sp+16    ;; Store 128-bit value
	lq sp+16, a7    ;; Load 128-bit value

	la a0, hello_world ;; address of string
	ecall           ;; Execute syscall

	lq sp+0, a0

	ret
