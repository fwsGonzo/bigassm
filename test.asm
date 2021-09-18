.global _start
_start:             ;; Entry point label
	li sp, -16      ;; Stack at end of 128-bit

	li a7, 2        ;; Syscall 2 (print)
	sq a7, sp-0     ;; Store 128-bit value
	lq sp+0, a7     ;; Load 128-bit value

	la a0, hello_world ;; address of string
	scall           ;; Execute syscall

exit:
	li a7, 1        ;; Syscall 1 (exit)
	li a0, 0        ;; Code: 0
	scall           ;; Execute syscall
	jmp exit

hello_world:        ;; String label
	.type hello_world, @object
	.string "Hello World!" ;; Zt-string
