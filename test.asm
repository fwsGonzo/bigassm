.org 0x100000

.global _start
_start:             ;; Entry point label
	li sp, -16      ;; Stack at end of 128-bit

	li t1, 4096

	li t1, 4097

	li t1, -2049

	li t1, 1024

	li t1, 0x12345678

	li t0, 2        ;; Syscall 2 (print)
	sq t0, sp-0     ;; Store 128-bit value
	lq sp+0, a7     ;; Load 128-bit value

	la a0, hello_world ;; address of string
	scall           ;; Execute syscall

exit:
	li a7, 1        ;; Syscall 1 (exit)
	li a0, 0x666    ;; Exit code (1st arg)
	scall           ;; Execute syscall
	jmp exit

hello_world:        ;; String label
	.type hello_world, @object
	.string "Hello World!" ;; Zt-string
